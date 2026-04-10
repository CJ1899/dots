#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include "proc.h"

#define MAX_WATCHDOGS 32

static pid_t watchdogs[MAX_WATCHDOGS];
static int w_count = 0;

void start_watchdog(const AppConfig *app) {
    pid_t wpid = fork();

    if (wpid < 0) {
        perror("dwm:watchdog failed");
        return;
    }

    if (wpid == 0) {
        /* Create a new session so we can kill the whole group later */
        setsid();

        while (1) {
            pid_t app_pid = fork();

            if (app_pid == 0) {
                execvp(app->command[0], app->command);
                _exit(1);
            } else if (app_pid > 0) {
                waitpid(app_pid, NULL, 0);

                /* If delay is -1, we only run once */
                if (app->restart_delay < 0) {
                    _exit(0);
                } else {
                    struct timespec ts = {app->restart_delay, 0};
                    if (nanosleep(&ts, NULL) < 0 && errno != EINTR) {
                        _exit(0);
                    }
                }
            } else {
                _exit(1);
            }
        }
    } else {
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
        kill(-watchdogs[i], SIGTERM);
        waitpid(watchdogs[i], NULL, 0);
    }
}

