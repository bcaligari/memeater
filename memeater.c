/*
    MemEater 2.0 - (c) 2024 "Brendon Caligari" <caligari.m0ptp@gmail.com>
    License: GPLv3

    This is a tool written for personal consumption with the idea of eating up
    all memory and seeing what happens.
*/


#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <sys/prctl.h>
#include <sys/wait.h>


#define BS_MIN          1 << 12             // 4KiB
#define BS_MAX          1 << 30             // 1GiB
#define BS_DEF          1 << 25             // 32MiB
#define FORKS_MIN       0
#define FORKS_MAX       3600
#define FORKS_DEF       0
#define INTERVAL_MIN    0
#define INTERVAL_MAX    3600
#define INTERVAL_DEF    5
#define ITER_MIN        0
#define ITER_MAX        2600
#define ITER_DEF        1


void parsefail(const char *prg);

int parseuint(const char *str, int upperlimit, int lowerlimit);

void readableint(char *ha, int hardnum, size_t strmax);

int meat(const char *whoami, int bs, int interval, int iterations);


int main(int argc, char *argv[])
{
    int _errno;
    int opt;
    pid_t forkpid;
    int bs, interval, iterations, forks;
    char bsr[24];
    char progname[24];
    char newprogname[24];
    int child_status;

    memset(progname, 0, (size_t) 24);
    if (prctl(PR_GET_NAME, progname, 0, 0, 0) == -1) {
        _errno = errno;
        fprintf(stderr, "Unable to get programe name with error %d (%s)", _errno, strerror(-1));
        strncpy(progname, argv[0], (size_t) 16);
    }
    forks = FORKS_DEF;
    iterations = ITER_DEF;
    interval =  INTERVAL_DEF;
    bs = BS_DEF;
    opterr = 0;
    while ((opt = getopt(argc, argv, "i:f:b:c:")) != -1) {
        switch (opt) {
        case 'i':
            if ((interval = parseuint(optarg, INTERVAL_MAX, INTERVAL_MIN)) == -1)
                parsefail(progname);
            break;
        case 'f':
            if ((forks = parseuint(optarg, FORKS_MAX, FORKS_MIN)) == -1)
                parsefail(progname);
            break;
        case 'b':
            if ((bs = parseuint(optarg, BS_MAX, BS_MIN)) == -1)
                parsefail(progname);
            break;
        case 'c':
            if ((iterations = parseuint(optarg, ITER_MAX, ITER_MIN)) == -1)
                parsefail(progname);
            break;
        default:
            parsefail(progname);
        }
    }
    if (optind < argc)
        parsefail(progname);

    readableint(bsr, bs, 16);
    printf("%s: forks: %d; block size: %s; count: %d; sleep: %d\n",
        progname, forks, bsr, iterations, interval);

    if (forks) {
        for (int i = 0; i < forks; i++) {
            forkpid = fork();
            _errno = errno;
            switch (forkpid) {
            case -1:
                fprintf(stderr, "fork() for iteration %d failed with %d (%s)\n", _errno, strerror(_errno));
                break;
            case 0:
                snprintf(newprogname, 16, "%s-%d", progname, (int) getpid());
                if (prctl(PR_SET_NAME, newprogname, 0, 0, 0) == -1) {
                    _errno = errno;
                    fprintf(stderr, "Unable to change fork name for child %d to \"%s\"\n", (int) getpid(), newprogname);
                }
                meat(newprogname, bs, interval, iterations);
                exit(EXIT_SUCCESS);
                break;
            default:
                printf("%s: forked %d ...\n", progname, forkpid);
            }
        }
        for (;;) {
            if ((forkpid = wait(&child_status)) == -1) {
                _errno = errno;
                if (_errno == ECHILD) {
                    printf("%s: no more children to wait()\n", progname);
                    break;
                }
                fprintf(stderr, "wait() failed with %d (%s) ... huh???\n", _errno, strerror(_errno));
            } else {
                if (WIFEXITED(child_status))
                    printf("%s: reaped %d which exited with %d\n",
                        progname, forkpid, (WEXITSTATUS(child_status)));
                if (WIFSIGNALED(child_status))
                    printf("%s: reaped %d terminated by a signal %d (%s)\n",
                        progname, forkpid, WTERMSIG(child_status), strsignal(WTERMSIG(child_status)));
            }
        }
    } else
        meat(progname, bs, interval, iterations);
    
    return 0;
}


void parsefail(const char *prg)
{
    fprintf(stderr, "Usage: %s [-i secs] [-b bytes] [-c count] [-f forks]\n", prg);
    fprintf(stderr, "    -i secs       : interval sleep between malloc()s\n");
    fprintf(stderr, "    -b bytes      : bytes per malloc()\n");
    fprintf(stderr, "    -c count      : number of malloc()s, 0 for infinity\n");
    fprintf(stderr, "    -f forks      : number of program forks\n");
    exit(EXIT_FAILURE);
}


int parseuint(const char *str, int upperlimit, int lowerlimit)
{
    long value;
    value = (int) strtol(str, NULL, 10);
    if ((errno == ERANGE) || (value < lowerlimit) || (value > upperlimit))
        return -1;
    return value;
}


void readableint(char *ha, int hardnum, size_t strmax)
{
    char prefix[4][8] = {"B", "KiB", "MiB", "GiB"};
    int m;
    ha[0] = 0;
    if (hardnum < (1 << 10)) {
        snprintf(ha, strmax, "%d %s", hardnum, prefix[0]);
    }
    else
        for (int i = 3; i >= 1; i--) {
            if (( m = (hardnum / (1 << (i * 10))) ) > 0) {
                if ((hardnum % (1 << (i * 10))) > 0)
                    snprintf(ha, strmax, "%.2f %s", (double) hardnum / (double) (1 << (i * 10)) , prefix[i]);
                else
                    snprintf(ha, strmax, "%d %s", m, prefix[i]);
                break;
            }
        }
    return;
}


int meat(const char *whoami, int bs, int interval, int iterations)
{
    char *ptr;
    int _errno;
    srandom((unsigned) iterations);
    for (int i = 0; iterations == 0 || i < iterations; i++) {
        if (iterations != 1)
            printf("%s: malloc() iteration %d\n", whoami, i);
        else
            printf("%s: malloc()\n", whoami);
        if ((ptr = malloc((size_t) bs)) == NULL) {
            _errno = errno;
            // deliberately to stdout
            printf("%s: malloc() failed with %d (%s)\n", whoami, _errno, strerror(_errno));
        } else
            memset(ptr, (char) random() || 0xFF, (size_t) bs);  // will it blend?
        if ((iterations != 1) && (interval > 0))
            sleep(interval);
    }
    return 0;
}
