#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include "proc.h"

/* Match this to your expected number of background apps */
#define MAX_WATCHDOGS 32

static pid_t watchdogs[MAX_WATCHDOGS];
static int w_count = 0;

void start_watchdog(const AppConfig *app) {
    pid_t wpid = fork();

    if (wpid < 0) {
        perror("dwm: fork watchdog failed");
        return;
    }

    if (wpid == 0) { /* Watchdog Process */
        /* Create a new session so we can kill the whole group later */
        setsid();

        while (1) {
            pid_t app_pid = fork();

            if (app_pid == 0) { /* The actual App */
                execvp(app->command[0], app->command);
                /* If execvp returns, it failed */
                _exit(1);
            } else if (app_pid > 0) {
                /* Wait for the app to exit or crash */
                waitpid(app_pid, NULL, 0);

                /* If delay is -1, we only run once */
                if (app->restart_delay < 0) {
                    _exit(0);
                } else {
                    struct timespec ts = {app->restart_delay, 0};
                    /* Sleep before restarting; handles interrupts gracefully */
                    if (nanosleep(&ts, NULL) < 0 && errno != EINTR) {
                        _exit(0);
                    }
                }
            } else {
                /* Fork for the app failed */
                _exit(1);
            }
        }
    } else { /* Parent (dwm) */
        watchdogs[w_count++] = wpid;
    }
}

void spawn_apps(void) {
    for (int i = 0; apps[i].command[0] != NULL; i++) {
        if (w_count >= MAX_WATCHDOGS) {
            fprintf(stderr, "dwm: reached MAX_WATCHDOGS (%d), skipping: %s\n",
                    MAX_WATCHDOGS, apps[i].command[0]);
            break;
        }
        start_watchdog(&apps[i]);
    }
}

void cleanup_apps(void) {
    for (int i = 0; i < w_count; i++) {
        /* Kill the watchdog and its child (the app) using the group ID */
        kill(-watchdogs[i], SIGTERM);
        /* Clean up the watchdog zombie */
        waitpid(watchdogs[i], NULL, 0);
    }
}

