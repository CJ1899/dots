/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>

#include "arg.h"
#include "slstatus.h"
#include "util.h"

struct arg {
    const char *(*func)(const char *);
    const char *fmt;
    const char *args;
    const int interval;    /* Interval in seconds */
    char cache[64];        /* Stores the last successful result */
    time_t next_update;    /* Timestamp for the next permitted run */
};

char buf[1024];
static volatile sig_atomic_t done;
static Display *dpy;

#include "config.h"

static void
terminate(const int signo)
{
    if (signo != SIGUSR1)
        done = 1;
}

static void
difftimespec(struct timespec *res, struct timespec *a, struct timespec *b)
{
    res->tv_sec = a->tv_sec - b->tv_sec - (a->tv_nsec < b->tv_nsec);
    res->tv_nsec = a->tv_nsec - b->tv_nsec +
                   (a->tv_nsec < b->tv_nsec) * 1E9;
}

static void
usage(void)
{
    die("usage: %s [-v] [-s] [-1]", argv0);
}

int
main(int argc, char *argv[])
{
    struct sigaction act;
    struct timespec start, current, diff, intspec, wait;
    size_t i, len;
    int sflag, ret;
    char status[MAXLEN];
    const char *res;

    sflag = 0;
    ARGBEGIN {
    case 'v':
        die("slstatus-"VERSION);
    case '1':
        done = 1;
        /* FALLTHROUGH */
    case 's':
        sflag = 1;
        break;
    default:
        usage();
    } ARGEND

    if (argc)
        usage();

    memset(&act, 0, sizeof(act));
    act.sa_handler = terminate;
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    act.sa_flags |= SA_RESTART;
    sigaction(SIGUSR1, &act, NULL);

    if (!sflag && !(dpy = XOpenDisplay(NULL)))
        die("XOpenDisplay: Failed to open display");

    /* Initialize next_update for all args to 0 so they run immediately on start */
    for (i = 0; i < LEN(args); i++) {
        args[i].next_update = 0;
        memset(args[i].cache, 0, sizeof(args[i].cache));
    }

    do {
        if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
            die("clock_gettime:");

        time_t now = time(NULL);
        status[0] = '\0';
        len = 0;

        for (i = 0; i < LEN(args); i++) {
            /* Only call the function if the interval has passed */
            if (now >= args[i].next_update) {
                res = args[i].func(args[i].args);
                if (!res) {
                    res = unknown_str;
                }

                /* Update the cache and set the next runtime */
                strncpy(args[i].cache, res, sizeof(args[i].cache) - 1);
                args[i].cache[sizeof(args[i].cache) - 1] = '\0';
                args[i].next_update = now + args[i].interval;
            }

            /* Build the status string using the cached value */
            if ((ret = esnprintf(status + len, sizeof(status) - len,
                                 args[i].fmt, args[i].cache)) < 0)
                break;

            len += ret;
        }

        if (sflag) {
            puts(status);
            fflush(stdout);
            if (ferror(stdout))
                die("puts:");
        } else {
            if (XStoreName(dpy, DefaultRootWindow(dpy), status) < 0)
                die("XStoreName: Allocation failed");
            XFlush(dpy);
        }

        if (!done) {
            if (clock_gettime(CLOCK_MONOTONIC, &current) < 0)
                die("clock_gettime:");
            difftimespec(&diff, &current, &start);

            intspec.tv_sec = interval / 1000;
            intspec.tv_nsec = (interval % 1000) * 1E6;
            difftimespec(&wait, &intspec, &diff);

            if (wait.tv_sec >= 0 &&
                nanosleep(&wait, NULL) < 0 &&
                errno != EINTR)
                    die("nanosleep:");
        }
    } while (!done);

    if (!sflag) {
        XStoreName(dpy, DefaultRootWindow(dpy), NULL);
        if (XCloseDisplay(dpy) < 0)
            die("XCloseDisplay: Failed to close display");
    }

    return 0;
}

