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
        perror("dwm: watchdog failed");
        return;
    }

    if (wpid == 0) {
        setsid();

        while (1) {
            pid_t app_pid = fork();

            if (app_pid == 0) {
                execvp(app->command[0], app->command);
                _exit(1);

            } else if (app_pid > 0) {
                int status;
                waitpid(app_pid, &status, 0);

                if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                    fprintf(stderr, "dwm: %s exited with code %d\n",
                            app->command[0], WEXITSTATUS(status));
                else if (WIFSIGNALED(status))
                    fprintf(stderr, "dwm: %s killed by signal %d\n",
                            app->command[0], WTERMSIG(status));

                if (app->restart_delay < 0) {
                    _exit(0);
                } else {
                    struct timespec ts = {app->restart_delay, 0}, rem;
                    while (nanosleep(&ts, &rem) < 0) {
                        if (errno == EINTR) ts = rem;
                        else _exit(0);
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
    /* First pass: SIGTERM all watchdog groups */
    for (int i = 0; i < w_count; i++)
        kill(-watchdogs[i], SIGTERM);

    /* Give them 200ms to exit cleanly */
    struct timespec ts = {0, 200000000L};
    nanosleep(&ts, NULL);

    /* Second pass: SIGKILL anything still alive, then reap */
    for (int i = 0; i < w_count; i++) {
        if (waitpid(watchdogs[i], NULL, WNOHANG) == 0) {
            kill(-watchdogs[i], SIGKILL);
            waitpid(watchdogs[i], NULL, 0);
        }
    }
}

