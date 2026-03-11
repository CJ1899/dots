#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "wall.h"

#define PIPE_BASE "/tmp/wl_pipe_"
typedef struct { int i; } FakeArg;

void print_help(char *name) {
    printf("Usage: %s [option/command]\n", name);
    printf("Options:           Commands:\n");
    printf("  -n               next           Next wallpaper\n");
    printf("  -p               prev           Previous wallpaper\n");
    printf("  -r               rand           Random wallpaper\n");
    printf("  -s               save           Save current state\n");
    printf("  -m               menu           Select via menu\n");
    printf("  -u               fnext          Folder forward\n");
    printf("  -d               fprev          Folder backward\n");
    printf("  -v               reload         Reload list\n");
}

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

int main(int argc, char *argv[]) {
    char pipe_path[256], *disp = getenv("DISPLAY");
    snprintf(pipe_path, sizeof(pipe_path), "%s%s", PIPE_BASE, disp ? disp : ":0");

    /* CLIENT MODE */
    if (argc > 1) {
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            print_help(argv[0]);
            return 0;
        }

        int fd = open(pipe_path, O_WRONLY | O_NONBLOCK);
        if (fd < 0) {
            fprintf(stderr, "Daemon not running on %s\n", disp ? disp : ":0");
            return 1;
        }

        char cmd = 0;
        /* Wallpaper Navigation */
        if      (!strcmp(argv[1], "-n") || !strcmp(argv[1], "next"))  cmd = 'n';
        else if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "prev"))  cmd = 'p';
        else if (!strcmp(argv[1], "-r") || !strcmp(argv[1], "rand"))  cmd = 'r';
        /* Folder Navigation */
        else if (!strcmp(argv[1], "-u") || !strcmp(argv[1], "fnext")) cmd = 'u';
        else if (!strcmp(argv[1], "-d") || !strcmp(argv[1], "fprev")) cmd = 'd';
        /* Utilities */
        else if (!strcmp(argv[1], "-m") || !strcmp(argv[1], "menu"))  cmd = 'm';
        else if (!strcmp(argv[1], "-s") || !strcmp(argv[1], "save"))  cmd = 's';
        else if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "reload")) cmd = 'R';

        if (cmd) write(fd, &cmd, 1);
        close(fd);
        return 0;
    }

    /* DAEMON MODE */
    unlink(pipe_path);
    if (mkfifo(pipe_path, 0666) == -1) return 1;

    wall_restore();

    while (1) {
        int fd = open(pipe_path, O_RDWR);
        char cmd;
        if (fd >= 0) {
            while (read(fd, &cmd, 1) > 0) run_command(cmd);
            close(fd);
        }
    }
    return 0;
}

