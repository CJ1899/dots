#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include "wall.h"
#include "ipc.h"

typedef struct {
    char cmd;
    int interval;
    int target;
} WallMsg;

char sock_path[104];
static int serv_fd = -1;
static int slideshow_interval = 0;
static char current_mode = 'n';
extern char current_folder[256];
extern int count, cur;

static volatile sig_atomic_t running = 1;
void handle_sig(int sig) { running = 0; }

void run_command(char cmd, int target) {
    Arg fwd = { .i = 1 }, bwd = { .i = -1 };
    switch (cmd) {
        case 'n': wall_cycle(&fwd); break;
        case 'p': wall_cycle(&bwd); break;
        case 'r': wall_random(NULL); break;
        case 's': wall_save(NULL); break;
        case 'm': wall_select(NULL); break;
        case 'u': wall_folder_select(&fwd); break;
        case 'd': wall_folder_select(&bwd); break;
        case 'R': wall_reload(NULL); break;
        case 'j':
            if (target > 0 && target <= count) {
                cur = target - 1;
                wall_cycle(NULL);
            }
            break;
    }
}

static int parse_duration(const char *str) {
    char *endptr;
    long val = strtol(str, &endptr, 10);
    if (endptr == str || val < 0) return 0;
    if (*endptr == 'm') return (val > INT_MAX / 60) ? INT_MAX : (int)(val * 60);
    return (val > INT_MAX) ? INT_MAX : (int)val;
}

int main(int argc, char *argv[]) {
    const char *display = getenv("DISPLAY");
    int target_interval = 0, jump_target = 0, query_mode = 0, argi = 1;

    while (argi < argc && argv[argi][0] == '-') {
        if (!strcmp(argv[argi], "-S") && argi + 1 < argc) display = argv[++argi];
        else if (!strcmp(argv[argi], "-y") && argi + 1 < argc) target_interval = parse_duration(argv[++argi]);
        else if (!strcmp(argv[argi], "-j") && argi + 1 < argc) {
            char *end;
            long val = strtol(argv[++argi], &end, 10);
            jump_target = (*end != '\0' || val < 0) ? 0 : (val > 1000000 ? 1000000 : (int)val);
        }
        else if (!strcmp(argv[argi], "-i")) query_mode = 1;
        else if (!strcmp(argv[argi], "-h") || !strcmp(argv[argi], "--help")) {
            printf("Usage: wl [-S :display] [-y dur] [-j idx] [-i] <cmd>\n");
            return 0;
        } else break;
        argi++;
    }

    get_sock_path(sock_path, sizeof(sock_path), display);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    size_t slen = strlen(sock_path);
    if (slen >= sizeof(addr.sun_path)) return 1;
    memcpy(addr.sun_path, sock_path, slen + 1);

    if (argc > argi || jump_target > 0 || query_mode) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0 || connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

        WallMsg msg = { .interval = target_interval, .target = jump_target };
        if (query_mode) msg.cmd = 'i';
        else if (jump_target > 0) msg.cmd = 'j';
        else {
            const char *arg = argv[argi];
            if      (!strcmp(arg, "-n") || !strcmp(arg, "next"))   msg.cmd = 'n';
            else if (!strcmp(arg, "-p") || !strcmp(arg, "prev"))   msg.cmd = 'p';
            else if (!strcmp(arg, "-r") || !strcmp(arg, "rand"))   msg.cmd = 'r';
            else if (!strcmp(arg, "-u") || !strcmp(arg, "fnext"))  msg.cmd = 'u';
            else if (!strcmp(arg, "-d") || !strcmp(arg, "fprev"))  msg.cmd = 'd';
            else if (!strcmp(arg, "-m") || !strcmp(arg, "menu"))   msg.cmd = 'm';
            else if (!strcmp(arg, "-s") || !strcmp(arg, "save"))   msg.cmd = 's';
            else if (!strcmp(arg, "-R") || !strcmp(arg, "reload")) msg.cmd = 'R';
            else msg.cmd = arg[0];
        }

        if (write(fd, &msg, sizeof(WallMsg)) != sizeof(WallMsg)) { close(fd); return 1; }
        if (query_mode) {
            char resp[64];
            int n = read(fd, resp, sizeof(resp) - 1);
            if (n > 0) { resp[n] = '\0'; printf("%s\n", resp); }
        }
        close(fd); return 0;
    }

    /* DAEMON SIDE */
    serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serv_fd < 0) return 1;
    if (fcntl(serv_fd, F_SETFD, FD_CLOEXEC) < 0)
    return 1;
    srand(time(NULL));

    struct sigaction sa = {0};
    sa.sa_handler = handle_sig;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    struct sigaction sa_chld = {0};
    sa_chld.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa_chld, NULL);
    sa.sa_handler = handle_sig;

    umask(0077);
    unlink(sock_path);
    if (bind(serv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(serv_fd, 5) < 0) {
        close(serv_fd); return 1;
    }
    wall_restore();

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(serv_fd, &fds);
        struct timeval tv, *timeout = NULL;
        if (slideshow_interval > 0) {
            tv.tv_sec = slideshow_interval; tv.tv_usec = 0; timeout = &tv;
        }

        int activity = select(serv_fd + 1, &fds, NULL, NULL, timeout);
        if (activity < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (activity == 0 && slideshow_interval > 0) {
            run_command(current_mode, 0); continue;
        }

        if (activity > 0 && FD_ISSET(serv_fd, &fds)) {
            int client_fd = accept4(serv_fd, NULL, NULL, SOCK_CLOEXEC);
            if (client_fd < 0) continue;

            struct ucred cred;
            socklen_t len = sizeof(struct ucred);
            if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0 || cred.uid != getuid()) {
                close(client_fd); continue;
            }

            WallMsg msg = {0};
            if (recv(client_fd, &msg, sizeof(WallMsg), MSG_WAITALL) == sizeof(WallMsg)) {
                if (strchr("nprsmudRji", msg.cmd)) {
                    if (msg.cmd == 'i') {
                        char resp[512];
                        int rlen = snprintf(resp, sizeof(resp), "%s | %d/%d", current_folder, cur + 1, count);
                        if (rlen >= (int)sizeof(resp)) rlen = sizeof(resp) - 1;
                        if (rlen < 0) rlen = 0;
                        (void)write(client_fd, resp, rlen);
                    } else {
                        if (msg.interval >= 0) slideshow_interval = (msg.interval > 86400) ? 86400 : msg.interval;
                        if (strchr("npr", msg.cmd)) current_mode = msg.cmd;
                        run_command(msg.cmd, msg.target);
                    }
                }
            }
            close(client_fd);
        }
    }
    unlink(sock_path);
    close(serv_fd);
    return 0;
}

