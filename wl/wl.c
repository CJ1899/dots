#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include "wall.h"
#include "ipc.h"

char sock_path[104];

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
    exit(0);
}

int main(int argc, char *argv[]) {
    get_sock_path(sock_path, sizeof(sock_path));

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_path);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    /* CLIENT MODE */
    if (argc > 1) {
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            return 0; // Help text here
        }

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            fprintf(stderr, "Daemon not running on %s\n", sock_path);
            close(fd);
            return 1;
        }

        char cmd = 0;
        if      (!strcmp(argv[1], "-n") || !strcmp(argv[1], "next"))   cmd = 'n';
        else if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "prev"))   cmd = 'p';
        else if (!strcmp(argv[1], "-r") || !strcmp(argv[1], "rand"))   cmd = 'r';
        else if (!strcmp(argv[1], "-u") || !strcmp(argv[1], "fnext"))  cmd = 'u';
        else if (!strcmp(argv[1], "-d") || !strcmp(argv[1], "fprev"))  cmd = 'd';
        else if (!strcmp(argv[1], "-m") || !strcmp(argv[1], "menu"))   cmd = 'm';
        else if (!strcmp(argv[1], "-s") || !strcmp(argv[1], "save"))   cmd = 's';
        else if (!strcmp(argv[1], "-R") || !strcmp(argv[1], "reload")) cmd = 'R';

        if (cmd) write(fd, &cmd, 1);
        close(fd);
        return 0;
    }

    /* DAEMON MODE */
    int serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    umask(0077);
    unlink(sock_path);

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

