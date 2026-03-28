#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include "proc.h"

static pid_t watchdogs[20];
static int w_count = 0;

void start_watchdog(char *const command[]) {
    pid_t wpid = fork();

    if (wpid == 0) {
        setsid();
        while (1) {
            pid_t app_pid = fork();
            if (app_pid == 0) {
                execvp(command[0], command);
                fprintf(stderr, "dwm: execvp failed for %s\n", command[0]);
                _exit(1);
            } else if (app_pid > 0) {
                waitpid(app_pid, NULL, 0);
                struct timespec ts = {3, 0};
                nanosleep(&ts, NULL);
            } else {
                _exit(1);
            }
        }
    } else if (wpid > 0) {
        watchdogs[w_count++] = wpid;
    }
}

void spawn_apps(void) {
    char *cmds[][10] = {
        {"picom", NULL},
//      {"sudo", "xinput", "set-prop", "9", "311", "0", NULL},
        {"bar", NULL},
        {"xrdb","/home/pc/etc/X11/.Xresources", NULL},
        {"dunst", NULL},
        {"xclip", NULL},
        {"dbus-update-activation-environment", "--all", NULL},
        {"/usr/bin/xset", "b", "off", NULL},
        {"xset", "r", "rate", "200", "50", NULL},
        {"unclutter", "--timeout", "5", NULL},
        {"udiskie", NULL},
        {"bat-notify", NULL},
        {"wl", NULL},
        {NULL}
    };

    for (int i = 0; cmds[i][0] != NULL; i++) {
        start_watchdog(cmds[i]);
    }
}

void cleanup_apps(void) {
    for (int i = 0; i < w_count; i++) {
        kill(-watchdogs[i], SIGTERM);
        waitpid(watchdogs[i], NULL, 0);
    }
}

