#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <X11/Xlib.h>

int main() {
    // 1. Map TTY to Display
    char *tty = ttyname(STDIN_FILENO);
    int tty_num = 1;
    if (tty) sscanf(tty, "/dev/tty%d", &tty_num);

    char disp[8], vt[8], lock[64];
    snprintf(disp, 8, ":%d", tty_num - 1);
    snprintf(vt, 8, "vt%d", tty_num);
    snprintf(lock, sizeof(lock), "/tmp/.X%d-lock", tty_num - 1);

    // 2. Start Xorg and SAVE the PID
    pid_t x_pid = fork();
    if (x_pid == 0) {
        execl("/usr/bin/X", "X", disp, vt, "-keeptty", "-noreset", NULL);
        _exit(1);
    }

    // 3. Wait for Xorg to be ready
    int retry = 0;
    Display *d = NULL;
    while (retry++ < 50) {
        d = XOpenDisplay(disp);
        if (d) {
            XCloseDisplay(d);
            break;
        }
        usleep(100000);
    }

    // 4. Start DWM
    setenv("DISPLAY", disp, 1);
    pid_t dwm_pid = fork();
    if (dwm_pid == 0) {
        execlp("dwm", "dwm", NULL);
        _exit(1);
    }

    // 5. Wait for DWM to exit
    waitid(P_PID, dwm_pid, NULL, WEXITED);

    // 6. Cleanup Sequence
    fprintf(stderr, "dwm exited, shutting down X...\n");
    kill(x_pid, SIGTERM);
    waitpid(x_pid, NULL, 0);
    unlink(lock);

    return 0;
}

