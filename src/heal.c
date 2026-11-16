/* SPDX-License-Identifier: MIT */
/* bin/heal: evilgenius-host's root helper. After movy-host's (https://github.com/DimaDake/schwung-movy, host/heal/heal.c,
 * MIT, Copyright (c) 2026 megadake), cut down to the one thread this host has.
 *
 * The Move's `ableton` user cannot take SCHED_FIFO, and without it about one audio block in a thousand runs more than
 * a millisecond late (movy's measurements). So the host runs as ableton and asks this one closed question:
 *
 *     heal rt <pid> <audio tid>
 *
 * schwung-heal installs it from bin/heal.new as root 04755 (schwung docs/MODULES.md, "a privileged helper of their
 * own"). It touches ONLY a process whose executable is the evilgenius-host beside it, and of that process only the
 * named thread (SCHED_FIFO 70, Move's own audio priority) and its RLIMIT_MEMLOCK. Nothing is read from the
 * environment, nothing is executed, and it exits at once. */
#define _GNU_SOURCE
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

static int exe_of(const char *proc, char *out, size_t cap)
{
    const ssize_t n = readlink(proc, out, cap - 1);
    if (n <= 0)
        return -1;
    out[n] = '\0';
    return 0;
}

int main(int argc, char **argv)
{
    char self[PATH_MAX], want[PATH_MAX + 32], target[PATH_MAX], path[96], *slash, *endp;
    struct rlimit inf = { RLIM_INFINITY, RLIM_INFINITY };
    struct sched_param sp = { .sched_priority = 70 };
    struct stat st;
    long pid, tid;
    int bad;
    if (argc != 4 || strcmp(argv[1], "rt") != 0) {
        fprintf(stderr, "usage: heal rt <pid> <audio tid>\n");
        return 2;
    }
    pid = strtol(argv[2], &endp, 10);
    if (*endp || pid <= 1)
        return 2;
    tid = strtol(argv[3], &endp, 10);
    if (*endp || tid <= 1)
        return 2;
    if (exe_of("/proc/self/exe", self, sizeof self) < 0)
        return 3;
    if (!(slash = strrchr(self, '/')))                  /* <dir>/bin/heal */
        return 3;
    *slash = '\0';
    if (!(slash = strrchr(self, '/')) || strcmp(slash, "/bin") != 0)
        return 3;
    *slash = '\0';
    snprintf(want, sizeof want, "%s/evilgenius-host", self);
    snprintf(path, sizeof path, "/proc/%ld/exe", pid);
    if (exe_of(path, target, sizeof target) < 0 || strcmp(target, want) != 0) {
        fprintf(stderr, "heal: pid %ld is not %s\n", pid, want);
        return 4;
    }
    snprintf(path, sizeof path, "/proc/%ld/task/%ld", pid, tid);
    if (stat(path, &st) != 0) {                         /* the thread must be that process's own */
        fprintf(stderr, "heal: %ld is not a thread of pid %ld\n", tid, pid);
        return 5;
    }
    bad = prlimit((pid_t)pid, RLIMIT_MEMLOCK, &inf, NULL) != 0;
    bad |= sched_setscheduler((pid_t)tid, SCHED_FIFO, &sp) != 0;
    printf("heal: pid %ld thread %ld %s\n", pid, tid, bad ? "NOT promoted" : "SCHED_FIFO 70");
    return bad ? 6 : 0;
}
