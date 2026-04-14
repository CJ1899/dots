#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
//#include <X11/Xlib.h>
#include "wall.h"
#include "ipc.h"

char sock_path[104];
static int serv_fd = -1;
//static Display *dpy = NULL;

typedef struct { int i; } FakeArg;

void run_command(char cmd) {
    FakeArg fwd = { .i = 1 }, bwd = { .i = -1 };
    switch (cmd) {
        case 'n': wall_cycle(&fwd); break;
        case 'p': wall_cycle(&bwd); break;
        case 'r': wall_random(NULL); break;
        case 's': wall_save(NULL); break;
        case 'm': wall_select(NULL); break;
        case 'u': wall_folder_select(&fwd); break;
        case 'd': wall_folder_select(&bwd); break;
        case 'R': wall_reload(NULL); break;
    }
}

void cleanup(int sig) {
    unlink(sock_path);
    if (serv_fd >= 0) close(serv_fd);
//        if (dpy)
//        XCloseDisplay(dpy);
    exit(0);
}

int main(int argc, char *argv[]) {
  const char *display = getenv("DISPLAY");  /* default to own screen */
    int argi = 1;                             /* index into argv after flags */

    /* Check if -S <display> is the first argument */
    if (argc > 2 && !strcmp(argv[1], "-S")) {
        display = argv[2];
        argi = 3;   /* real command starts after -S :1 */
    }

    get_sock_path(sock_path, sizeof(sock_path), display);

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_path);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    /* CLIENT SIDE */
    if (argc > argi) {
        if (!strcmp(argv[argi], "-h") || !strcmp(argv[argi], "--help")) {
            printf("Usage: wl [-S :display] <command>\n"
                   "  -S :N   target display :N's daemon (e.g. from SSH)\n"
                   "Commands: next prev rand fnext fprev menu save reload\n");
            return 0;
        }

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) { perror("socket"); return 1; }

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            fprintf(stderr, "wl: daemon not running on display %s (%s)\n",
                    display, sock_path);
            close(fd);
            return 1;
        }

        char cmd = 0;
        const char *arg = argv[argi];
        if      (!strcmp(arg, "-n") || !strcmp(arg, "next"))   cmd = 'n';
        else if (!strcmp(arg, "-p") || !strcmp(arg, "prev"))   cmd = 'p';
        else if (!strcmp(arg, "-r") || !strcmp(arg, "rand"))   cmd = 'r';
        else if (!strcmp(arg, "-u") || !strcmp(arg, "fnext"))  cmd = 'u';
        else if (!strcmp(arg, "-d") || !strcmp(arg, "fprev"))  cmd = 'd';
        else if (!strcmp(arg, "-m") || !strcmp(arg, "menu"))   cmd = 'm';
        else if (!strcmp(arg, "-s") || !strcmp(arg, "save"))   cmd = 's';
        else if (!strcmp(arg, "-R") || !strcmp(arg, "reload")) cmd = 'R';
        else { fprintf(stderr, "wl: unknown command: %s\n", arg); close(fd); return 1; }

        if (write(fd, &cmd, 1) < 0) perror("wl: write");
        close(fd);
        return 0;
    }

    /* DAEMON SIDE */
    serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serv_fd < 0) { perror("wl: socket"); return 1; }
    fcntl(serv_fd, F_SETFD, FD_CLOEXEC);
    srand(time(NULL));
    signal(SIGCHLD, SIG_IGN);
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    umask(0077);
    unlink(sock_path);

//    dpy = XOpenDisplay(NULL);
//  if (!dpy) {
//    fprintf(stderr, "Cannot open display\n");
//    return 1;
//  }

//  wall_set_display(dpy);

    if (bind(serv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
	close(serv_fd);
        return 1;
    }
    listen(serv_fd, 5);
    wall_restore();

    while (1) {
        int client_fd = accept(serv_fd, NULL, NULL);
        if (client_fd < 0) continue;

        char cmd;
        if (read(client_fd, &cmd, 1) > 0) {
            run_command(cmd);
        }
        close(client_fd);
    }

    return 0;
}

