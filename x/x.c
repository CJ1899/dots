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
    snprintf(disp, sizeof(disp), ":%d", tty_num - 1);
    snprintf(vt, sizeof(vt), "vt%d", tty_num);
    snprintf(lock, sizeof(lock), "/tmp/.X%d-lock", tty_num - 1);

    // 2. Start Xlibre
    pid_t x_pid = fork();
    if (x_pid == 0) {
        // Removed -noreset if Xlibre doesn't like it; keep -keeptty
        execl("/usr/bin/X", "X", disp, vt, "-keeptty", NULL);
        _exit(1);
    }

    // 3. Wait for Xlibre to be ready
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

    if (!d) return 1;

    // 4. Start DWM
    setenv("DISPLAY", disp, 1);
    pid_t dwm_pid = fork();
    if (dwm_pid == 0) {
        execlp("dwm", "dwm", NULL);
        _exit(1);
    }

    // 5. Wait for DWM to exit
    waitpid(dwm_pid, NULL, 0);

    // 6. Xlibre Cleanup - GENTLE VERSION
    // We send the signal, but we don't unlink manually
    // unless Xlibre fails to clean up after itself.
    kill(x_pid, SIGTERM);

    // Wait for X to exit naturally
    waitpid(x_pid, NULL, 0);

    // Only unlink if the file STILL exists after X is dead
    if (access(lock, F_OK) == 0) {
        unlink(lock);
    }

    return 0;
}

