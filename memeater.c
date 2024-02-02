/*
    MemEater 2.0 - (c) 2024 "Brendon Caligari" <caligari.m0ptp@gmail.com>
    License: GPLv3

    This is a tool written in my free time for my own use and enjoyment watching
    systems break as memory is eaten up.
*/


#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <getopt.h>
#include <assert.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/mman.h>


#define BI_NAME         "memeater"
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
#define CHILL_MIN       0
#define CHILL_MAX       60*60*24
#define CHILL_DEF       0
#define PNAME_LEN       15
#define SHORT_LEN       24
#define SBUF_LEN        32


void parsefail(const char *prg);

int parseuint(const char *str, int upperlimit, int lowerlimit);

char *readableint(char *ha, int hardnum, size_t strmax);

int meat(const char *whoami, int bs, int interval, int iterations, int chill, int lockmem, int nofill);


int main(int argc, char *argv[])
{
    int _errno;
    int opt;
    pid_t forkpid;
    char bsr[SBUF_LEN];
    char progname[SBUF_LEN];
    char pidstr[SBUF_LEN];
    int child_status;
    int forks = FORKS_DEF;
    int iterations = ITER_DEF;
    int interval =  INTERVAL_DEF;
    int bs = BS_DEF;
    int chill = CHILL_DEF;
    int lockmem = 0;
    int nofill = 0;

    bzero(progname, (size_t) SBUF_LEN);
    if (prctl(PR_GET_NAME, progname, 0, 0, 0) == -1) {
        _errno = errno;
        fprintf(stderr, "Unable to get programe name with error %d (%s)\n", _errno, strerror(-1));
        memcpy(progname, BI_NAME, strlen(BI_NAME));
    }
    opterr = 0;
    while ((opt = getopt(argc, argv, "i:f:b:c:w:ln")) != -1) {
        switch (opt) {
        case 'n':
            nofill = 1;
            break;
        case 'l':
            lockmem = 1;
            break;
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
        case 'w':
            if ((chill = parseuint(optarg, CHILL_MAX, CHILL_MIN)) == -1)
                parsefail(progname);
            break;
        default:
            parsefail(progname);
        }
    }
    if (optind < argc)
        parsefail(progname);

    printf("%s(%d); forks: %d; block size: %s; count: %d; sleep: %d; mlockall: %d, nofill: %d\n",
        progname, (int) getpid(), forks, readableint(bsr, bs, SHORT_LEN), iterations, interval, lockmem, nofill);

    if (forks) {
        for (int i = 0; i < forks; i++) {
            forkpid = fork();
            _errno = errno;
            switch (forkpid) {
            case -1:
                fprintf(stderr, "fork() for iteration %d failed with %d (%s)\n", i,  _errno, strerror(_errno));
                break;
            case 0:
                snprintf(pidstr, SBUF_LEN, "-%d", (int) getpid());
                if ((strlen(pidstr) + strlen(progname)) < PNAME_LEN)
                    strcat(progname, pidstr);
                else
                    memcpy(progname + (PNAME_LEN - strlen(pidstr)), pidstr, strlen(pidstr));
                if (prctl(PR_SET_NAME, progname, 0, 0, 0) == -1) {
                    _errno = errno;
                    fprintf(stderr, "Unable to change fork name for child %d to \"%s\"\n", (int) getpid(), progname);
                }
                meat(progname, bs, interval, iterations, chill, lockmem, nofill);
                exit(EXIT_SUCCESS);
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
        meat(progname, bs, interval, iterations, chill, lockmem, nofill);
    
    return EXIT_SUCCESS;
}


void parsefail(const char *prg)
{
    fprintf(stderr, "Usage: %s [-i secs] [-b bytes] [-c count] [-f forks] [-l]\n", prg);
    fprintf(stderr, "    -i secs       : interval sleep between malloc()s\n");
    fprintf(stderr, "    -b bytes      : bytes per malloc()\n");
    fprintf(stderr, "    -c count      : number of malloc()s, 0 for infinity\n");
    fprintf(stderr, "    -f forks      : number of program forks\n");
    fprintf(stderr, "    -w chillout   : seconds to sleep after last iteration\n");
    fprintf(stderr, "    -l            : mark faulted in pages as locked\n");
    fprintf(stderr, "    -n            : do not fill memory allocations\n");
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


char *readableint(char *ha, int hardnum, size_t strmax)
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
    return ha;
}


int meat(const char *whoami, int bs, int interval, int iterations, int chill, int lockmem, int nofill)
{
    char *ptr;
    int _errno;
    if (lockmem) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT) == -1) {
            _errno = errno;
            printf("%s: mlockall() failed with %d (%s)\n", whoami, _errno, strerror(_errno));
            printf("%s: proceeding without locking pages.\n", whoami);
        } else
            printf("%s: faulted pages are marked locked.\n", whoami);
    }

    srandom((unsigned) iterations);
    for (int i = 0; iterations == 0 || i < iterations; i++) {
        if (iterations != 1)
            printf("%s: malloc() iteration %d\n", whoami, i);
        else
            printf("%s: malloc()\n", whoami);
        if ((ptr = malloc((size_t) bs)) == NULL) {
            _errno = errno;
            printf("%s: malloc() failed with %d (%s)\n", whoami, _errno, strerror(_errno));
        } else if (!nofill)
            memset(ptr, (char) (random() & 0xFF), (size_t) bs);
        if ((iterations != 1) && (interval > 0))
            sleep(interval);
    }
    if (chill) {
        printf("%s: sleeping for %d seconds ...\n", whoami, chill);
        sleep(chill);
    }
    return 0;
}
