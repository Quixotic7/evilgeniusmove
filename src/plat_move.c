/* SPDX-License-Identifier: MPL-2.0 */
/* The Move's platform: EvilGenius as a standalone Schwung tool. Move's own software is stopped while this runs
 * (schwung's launch-standalone.sh does that, and starts it again when this exits), so this process owns the SPI link
 * to the surface: the audio out, the pads' and buttons' MIDI, the LEDs and the screen.
 *
 * The SPI protocol and the launch contract are Schwung's (https://github.com/charlesvestal/schwung, MIT: the layout
 * below is src/lib/schwung_spi_lib.h's) and the shape of this host follows movy-host
 * (https://github.com/DimaDake/schwung-movy, MIT): an exclusive open, one transfer a 128-frame block, the screen
 * answered a slice at a time, a real-time audio thread on core 3 with a setuid helper to make it so.
 *
 *   the audio thread   one WAIT_AND_SEND a block: LED packets out, the transfer, the screen's slice, MIDI in, a block
 *   the main thread    every 2 ms: the app's input, clock and stores; every 33 ms: the screen */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "egm.h"

/* ---- the SPI page (schwung_spi_lib.h) */
#define SPI_DEVICE "/dev/ablspi0.0"
#define SPI_PAGE 4096
#define SPI_FREQ 20000000
#define SPI_IOC(nr) _IOC(_IOC_NONE, 0, (nr), 0)
#define SPI_IOC_WAIT_SEND_SIZE 10
#define SPI_IOC_SET_SPEED 11
#define OFF_OUT_MIDI 0
#define OFF_OUT_DISP_STAT 80
#define OFF_OUT_DISP_DATA 84
#define OFF_OUT_AUDIO 256
#define OUT_DISP_CHUNK 172
#define OFF_IN_MIDI 2048
#define OFF_IN_DISP_STAT (2048 + 248)
#define MIDI_IN_MAX 31
#define MIDI_OUT_MAX 20
#define FRAMES 128
#define DISPLAY_SIZE 1024
#define FADE_BLOCKS 172                /* half a second from silence: the link may still carry the last owner's block */

extern char **environ;

static int g_fd = -1;
static uint8_t *g_map;
static volatile sig_atomic_t g_stop;
static volatile int g_audio_stop;
static int g_audio_tid;
static volatile int g_late;
static volatile int g_test_vol = 100;                   /* percent of the output: 100 unless the test hook is on (below) */
static volatile int g_peak;                             /* the loudest sample before that, since the state was last written */
static char g_dir[400];

/* the screen: the main thread publishes a frame, the audio thread serves it. A seqlock: never a torn frame out */
static struct { uint32_t seq; uint8_t px[DISPLAY_SIZE]; } g_frame;
static uint8_t g_serving[DISPLAY_SIZE];
static uint8_t *g_live;                /* /dev/shm/schwung-display-live: schwung's display-server streams it */

static uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void logf_(const char *fmt, ...)
{
    va_list ap;
    __builtin_va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    __builtin_va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

/* ------------------------------------------------------------------ SPI --- */
static void spi_pump(void) { ioctl(g_fd, SPI_IOC(SPI_IOC_WAIT_SEND_SIZE), 0x300); }

static int spi_open(void)
{
    g_fd = open(SPI_DEVICE, O_RDWR | O_CLOEXEC);        /* exclusive: EBUSY while Move still holds it */
    if (g_fd < 0)
        return -errno;
    g_map = mmap(NULL, SPI_PAGE, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
    if (g_map == MAP_FAILED) {
        const int e = errno;
        close(g_fd);
        g_fd = -1;
        return -e;
    }
    memset(g_map, 0, SPI_PAGE);
    ioctl(g_fd, SPI_IOC(SPI_IOC_SET_SPEED), SPI_FREQ);
    g_map[0] = 0x0F;                                    /* a MIDI system reset: ends the surface's power-on LED show */
    g_map[1] = 0xFF;
    spi_pump();
    memset(g_map, 0, 4);
    return 0;
}

static void serve_display(const uint8_t *packed)       /* the panel pulls: it names the slice it wants (1..6) */
{
    uint32_t idx;
    memcpy(&idx, g_map + OFF_IN_DISP_STAT, 4);
    if (idx < 1 || idx > 6)
        return;
    {
        const int off = (int)(idx - 1) * OUT_DISP_CHUNK;
        const int len = idx == 6 ? DISPLAY_SIZE - off : OUT_DISP_CHUNK;
        memcpy(g_map + OFF_OUT_DISP_STAT, &idx, 4);
        memcpy(g_map + OFF_OUT_DISP_DATA, packed + off, (size_t)len);
    }
}

static void spi_close(void)
{
    static const uint8_t blank[DISPLAY_SIZE];
    int i;
    if (g_fd < 0)
        return;
    memset(g_map + OFF_OUT_AUDIO, 0, FRAMES * 4);
    memset(g_map, 0, MIDI_OUT_MAX * 4);
    for (i = 0; i < 16; i++) {                          /* two rounds of blank slices: Move does not inherit our pixels */
        spi_pump();
        serve_display(blank);
    }
    memset(g_map + OFF_OUT_DISP_STAT, 0, 4);
    munmap(g_map, SPI_PAGE);
    close(g_fd);
    g_fd = -1;
}

/* ------------------------------------------------------------------ the screen --- */
static void frame_publish(const uint8_t *px)
{
    __atomic_add_fetch(&g_frame.seq, 1, __ATOMIC_RELEASE);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    memcpy(g_frame.px, px, DISPLAY_SIZE);
    __atomic_add_fetch(&g_frame.seq, 1, __ATOMIC_RELEASE);
    if (g_live)
        memcpy(g_live, px, DISPLAY_SIZE);
}

static void frame_take(void)                            /* a torn read: keep what was being served */
{
    uint8_t tmp[DISPLAY_SIZE];
    const uint32_t s = __atomic_load_n(&g_frame.seq, __ATOMIC_ACQUIRE);
    if (s & 1u)
        return;
    memcpy(tmp, g_frame.px, DISPLAY_SIZE);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    if (__atomic_load_n(&g_frame.seq, __ATOMIC_RELAXED) == s)
        memcpy(g_serving, tmp, DISPLAY_SIZE);
}

static void live_open(void)
{
    const int fd = shm_open("/schwung-display-live", O_RDWR | O_CREAT, 0666);
    void *p;
    if (fd < 0)
        return;
    fchmod(fd, 0666);                                   /* whichever uid made it, the other must open it next */
    if (ftruncate(fd, DISPLAY_SIZE) != 0) {
        close(fd);
        return;
    }
    p = mmap(NULL, DISPLAY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    g_live = p == MAP_FAILED ? NULL : p;
}

/* ------------------------------------------------------------------ the audio thread --- */
static void *audio_main(void *arg)
{
    int16_t buf[FRAMES * 2];
    uint8_t out[MIDI_OUT_MAX][4];
    cpu_set_t set;
    struct sched_param sp = { .sched_priority = 70 };
    int fade = 0, late = 0, rc;
    uint64_t last = 0, report = 0;
    (void)arg;
    g_audio_tid = (int)syscall(SYS_gettid);
    pthread_setname_np(pthread_self(), "eg-audio");
    CPU_ZERO(&set);
    CPU_SET(3, &set);                                   /* the SPI core: Move's own audio thread lives there */
    sched_setaffinity(0, sizeof set, &set);
    rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    logf_("audio: tid %d, SCHED_FIFO 70 %s", g_audio_tid, rc == 0 ? "ok" : "refused (the helper will try)");
    while (!g_audio_stop) {
        const int nout = egm_leds_take(out, MIDI_OUT_MAX);
        uint8_t *in = g_map + OFF_IN_MIDI;
        int16_t *dst = (int16_t *)(void *)(g_map + OFF_OUT_AUDIO);
        uint64_t t;
        int i;
        memcpy(g_map + OFF_OUT_MIDI, out, (size_t)nout * 4);
        spi_pump();
        memset(g_map + OFF_OUT_MIDI, 0, MIDI_OUT_MAX * 4);
        t = now_us();
        if (last && t - last > 2902u + 400u)            /* a block is 2902 us */
            late++;
        last = t;
        if (t - report > 10000000u) {
            if (late)
                logf_("audio: %d late transfer(s) in the last 10 s", late);
            g_late += late;
            late = 0, report = t;
        }
        {
            uint32_t idx;
            memcpy(&idx, g_map + OFF_IN_DISP_STAT, 4);
            if (idx == 1)
                frame_take();
            serve_display(g_serving);
        }
        for (i = 0; i < MIDI_IN_MAX; i++) {
            const uint8_t *e = in + i * 8;
            if (e[0] | e[1] | e[2] | e[3])
                egm_midi_in(e);
        }
        memset(in, 0, MIDI_IN_MAX * 8);                 /* the mailbox keeps its events until overwritten: its only reader clears it */
        egm_audio(buf, FRAMES);
        {
            int vol = g_test_vol, pk = g_peak;
            for (i = 0; i < FRAMES * 2; i++) {
                int v = buf[i] < 0 ? -buf[i] : buf[i];
                if (v > pk)
                    pk = v;
                if (vol < 100)
                    buf[i] = (int16_t)(buf[i] * vol / 100);
            }
            g_peak = pk;
        }
        if (fade < FADE_BLOCKS) {
            for (i = 0; i < FRAMES * 2; i++)
                dst[i] = (int16_t)(buf[i] * fade / FADE_BLOCKS);
            fade++;
        } else
            memcpy(dst, buf, sizeof buf);
    }
    return NULL;
}

/* ------------------------------------------------------------------ real time --- */
/* ableton cannot take SCHED_FIFO. bin/heal (src/heal.c), installed root 04755 by schwung-heal from the stage the
 * launcher leaves, does it for this process's audio thread and nothing else */
static void rt_help(void)
{
    char helper[480], pid[16], tid[16];
    struct stat st;
    pid_t child;
    int status = -1;
    if (sched_getscheduler(g_audio_tid) == SCHED_FIFO)
        return;
    snprintf(helper, sizeof helper, "%s/bin/heal", g_dir);
    if (stat(helper, &st) != 0 || st.st_uid != 0 || !(st.st_mode & S_ISUID)) {
        logf_("rt: the audio thread is not real-time and %s is not root 04755: running DEGRADED (clicks are likely)", helper);
        return;
    }
    snprintf(pid, sizeof pid, "%d", (int)getpid());
    snprintf(tid, sizeof tid, "%d", g_audio_tid);
    {
        char *argv[] = { helper, "rt", pid, tid, NULL };
        if (posix_spawn(&child, helper, NULL, NULL, argv, environ) == 0)
            waitpid(child, &status, 0);
    }
    logf_("rt: helper exit %d, the audio thread is %s", WIFEXITED(status) ? WEXITSTATUS(status) : -1,
          sched_getscheduler(g_audio_tid) == SCHED_FIFO ? "SCHED_FIFO" : "NOT real-time (DEGRADED)");
}

static void on_term(int sig) { (void)sig; g_stop = 1; }

/* ------------------------------------------------------------------ the test hook --- */
/* Only when /dev/shm/egm-test exists at the start (a reboot clears it): surface events can be written into the FIFO
 * /dev/shm/egm-cmd, a line each ("note N VEL", "cc N VALUE", as the simulator's script), and the app's state is
 * written to /dev/shm/egm-state four times a second, so the unit can be checked over ssh with nobody at it.
 * A TEST IS QUIET: the speaker is loud and nobody may be there to turn it down, so with the hook on the output is
 * SILENT, or at the percent written in /dev/shm/egm-test (scripts/deploy.sh --test [PERCENT]). The state line's
 * peak= is the level before that, so a silent run still shows that sound was made */
static int g_test_fd = -1;

static void test_open(void)
{
    FILE *f = fopen("/dev/shm/egm-test", "r");
    int vol = 0;
    if (!f)
        return;
    if (fscanf(f, "%d", &vol) != 1)
        vol = 0;
    fclose(f);
    g_test_vol = vol < 0 ? 0 : vol > 100 ? 100 : vol;
    unlink("/dev/shm/egm-cmd");
    if (mkfifo("/dev/shm/egm-cmd", 0666) != 0)
        return;
    chmod("/dev/shm/egm-cmd", 0666);
    g_test_fd = open("/dev/shm/egm-cmd", O_RDWR | O_NONBLOCK | O_CLOEXEC);   /* (RDWR: never an end of file) */
    logf_("test hook: on (/dev/shm/egm-cmd, /dev/shm/egm-state), output at %d%%", g_test_vol);
}

static void test_poll(uint32_t ms, int late)
{
    static char buf[512];
    static int n;
    static uint32_t last;
    ssize_t r;
    if (g_test_fd < 0)
        return;
    while ((r = read(g_test_fd, buf + n, sizeof buf - 1 - (size_t)n)) > 0) {
        char *nl;
        n += (int)r;
        buf[n] = 0;
        while ((nl = strchr(buf, '\n'))) {
            char cmd[8] = "";
            int a = 0, b = 0;
            *nl = 0;
            if (sscanf(buf, "%7s %d %d", cmd, &a, &b) == 3) {
                uint8_t p[4] = { 0, 0, (uint8_t)a, (uint8_t)b };
                if (!strcmp(cmd, "note"))
                    p[0] = b ? 0x09 : 0x08, p[1] = b ? 0x90 : 0x80;
                else if (!strcmp(cmd, "cc"))
                    p[0] = 0x0B, p[1] = 0xB0;
                if (p[1])
                    egm_midi_in(p);
            }
            n -= (int)(nl + 1 - buf);
            memmove(buf, nl + 1, (size_t)n + 1);
        }
        if (n >= (int)sizeof buf - 1)
            n = 0;
    }
    if (ms - last >= 250u) {
        char line[1100];
        FILE *f;
        last = ms;
        egm_describe(line, sizeof line);
        if ((f = fopen("/dev/shm/egm-state.tmp", "w"))) {
            fprintf(f, "ms=%u late=%d vol=%d peak=%d %s\n", (unsigned)ms, late, g_test_vol, g_peak, line);
            g_peak = 0;
            fclose(f);
            rename("/dev/shm/egm-state.tmp", "/dev/shm/egm-state");
        }
    }
}

/* the power off: Move's own power service over D-Bus, which ableton may call (movy-host's power.c). The call answers
 * at once and the power goes seconds later, so this process then HOLDS, deaf to TERM: were it to exit, schwung's
 * launcher would start Move again and the power cut would catch it mid-boot (a failed boot, to schwung's counter) */
static void power_off(void)
{
    char *argv[] = { "/usr/bin/dbus-send", "--system", "--print-reply", "--dest=com.ableton.system",
                     "/com/ableton/System/Power", "com.ableton.system.shutDown", NULL };
    pid_t child;
    int i;
    logf_("power: off, by com.ableton.system");
    signal(SIGTERM, SIG_IGN);
    if (posix_spawn(&child, argv[0], NULL, NULL, argv, environ) != 0) {
        logf_("power: cannot run %s; leaving to Move instead", argv[0]);
        return;
    }
    for (i = 0; i < 120; i++)
        sleep(1);
}

int main(int argc, char **argv)
{
    char flash[480], *slash;
    pthread_t audio;
    struct sigaction sa;
    sigset_t ss;
    uint64_t t0, next;
    uint32_t last_frame = 0;
    uint8_t packed[DISPLAY_SIZE];
    int rc, how = 0;
    (void)argc;
    /* the module's own directory: the store's image lives beside the program */
    if (!realpath(argv[0], g_dir))
        snprintf(g_dir, sizeof g_dir, "%s", argv[0]);
    if ((slash = strrchr(g_dir, '/')))
        *slash = 0;
    snprintf(flash, sizeof flash, "%s/evilgenius-store.bin", g_dir);

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_term;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    sigemptyset(&ss);                                   /* MoveLauncher blocks these in what it starts */
    sigaddset(&ss, SIGTERM);
    sigaddset(&ss, SIGINT);
    sigprocmask(SIG_UNBLOCK, &ss, NULL);

    if ((rc = spi_open()) != 0) {
        logf_("spi: cannot open %s: %s%s", SPI_DEVICE, strerror(-rc), rc == -EBUSY ? " (Move still holds it)" : "");
        return 1;
    }
    logf_("evilgenius-host: SPI open, store %s", flash);
    live_open();
    egm_init(flash);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        logf_("rt: memory not locked (%s)", strerror(errno));
    if (pthread_create(&audio, NULL, audio_main, NULL) != 0) {
        logf_("audio: no thread");
        spi_close();
        return 1;
    }
    while (!g_audio_tid)
        usleep(1000);
    rt_help();
    /* again now the helper has lifted RLIMIT_MEMLOCK: as ableton the first try has no room */
    logf_("rt: memory %s", mlockall(MCL_CURRENT | MCL_FUTURE) == 0 ? "locked" : "NOT locked");

    test_open();
    t0 = now_us();
    next = t0;
    while (!g_stop) {
        const uint64_t t = now_us();
        const uint32_t ms = (uint32_t)((t - t0) / 1000u);
        if ((how = egm_ui_step(ms)))
            break;
        test_poll(ms, g_late);
        if (ms - last_frame >= 33u) {
            last_frame = ms;
            if (egm_frame(packed, ms))
                frame_publish(packed);
        }
        next += 2000u;
        if (next > now_us())
            usleep((useconds_t)(next - now_us()));
        else
            next = now_us();                            /* behind (a frame took long): no burst to catch up */
    }
    logf_("evilgenius-host: closing (%s)", g_stop ? "stopped" : how == 2 ? "power off" : "the player left");
    g_audio_stop = 1;
    pthread_join(audio, NULL);
    spi_close();
    egm_shutdown();
    if (how == 2 && !g_stop)
        power_off();
    return 0;
}
