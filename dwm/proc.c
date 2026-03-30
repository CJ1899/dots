#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <sys/prctl.h>
#include "proc.h"

static pid_t watchdogs[20];
static int w_count = 0;

void start_watchdog(const AppConfig *app) {
    pid_t wpid = fork();

    if (wpid == 0) { // Watchdog Process
        /* Access command via the app pointer */
        char proc_name[16];
        snprintf(proc_name, 16, "w-%s", app->command[0]);
        prctl(PR_SET_NAME, proc_name);

        setsid();
        while (1) {
            pid_t app_pid = fork();
            if (app_pid == 0) { // The actual App
                execvp(app->command[0], app->command);
                _exit(1);
            } else if (app_pid > 0) {
                waitpid(app_pid, NULL, 0);

                if (app->restart_delay < 0) {
                    _exit(0);
                } else {
                    struct timespec ts = {app->restart_delay, 0};
                    nanosleep(&ts, NULL);
                }
            } else {
                _exit(1);
            }
        }
    } else if (wpid > 0) {
        watchdogs[w_count++] = wpid;
    }
}

void spawn_apps(void) {
    for (int i = 0; apps[i].command[0] != NULL; i++) {
        start_watchdog(&apps[i]);
    }
}

void cleanup_apps(void) {
    for (int i = 0; i < w_count; i++) {
        kill(-watchdogs[i], SIGTERM);
        waitpid(watchdogs[i], NULL, 0);
    }
}

