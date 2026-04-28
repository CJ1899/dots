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
#include <libgen.h>
#include <ctype.h>

#include "wall.h"

char sock_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
static int serv_fd = -1;
static int slideshow_interval = 0;
static char current_mode = 'n';
extern char current_folder[256];
extern int count, cur;

void get_sock_path(char *dest, size_t len, const char *display)
{
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (!runtime || runtime[0] != '/') {
        fprintf(stderr, "Error: XDG_RUNTIME_DIR not set\n");
        exit(1);
    }

    if (!display) display = getenv("WAYLAND_DISPLAY");
    if (!display) display = "wayland-0";

    int ret = snprintf(dest, len, "%s/waywall-%d.%s.sock",
                       runtime, getuid(), display);

    if (ret < 0 || (size_t)ret >= len) {
        fprintf(stderr, "Error: Socket path too long\n");
        exit(1);
    }
}

#ifdef __linux__
#include <linux/landlock.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <stdint.h>

static inline int landlock_create_ruleset(const struct landlock_ruleset_attr *attr, size_t size, uint32_t flags) {
    return syscall(SYS_landlock_create_ruleset, attr, size, flags);
}

static inline int landlock_add_rule(int ruleset_fd, enum landlock_rule_type rule_type, const void *rule_attr, uint32_t flags) {
    return syscall(SYS_landlock_add_rule, ruleset_fd, rule_type, rule_attr, flags);
}

static inline int landlock_restrict_self(int ruleset_fd, uint32_t flags) {
    return syscall(SYS_landlock_restrict_self, ruleset_fd, flags);
}

static void landlock_apply(const char *master_dir, const char *save_path, const char *awww_path) {
    struct landlock_ruleset_attr rs_attr = {
        .handled_access_fs =
            LANDLOCK_ACCESS_FS_READ_FILE  | LANDLOCK_ACCESS_FS_READ_DIR  |
            LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_MAKE_REG   |
            LANDLOCK_ACCESS_FS_REMOVE_FILE| LANDLOCK_ACCESS_FS_EXECUTE,
    };

    int rs_fd = landlock_create_ruleset(&rs_attr, sizeof(rs_attr), 0);
    if (rs_fd < 0) return;

    #define LL_ALLOW(path, access) do { \
        int _fd = open((path), O_PATH | O_CLOEXEC); \
        if (_fd >= 0) { \
            struct landlock_path_beneath_attr pb = { .allowed_access = (access), .parent_fd = _fd }; \
            landlock_add_rule(rs_fd, LANDLOCK_RULE_PATH_BENEATH, &pb, 0); \
            close(_fd); \
        } \
    } while(0)

    uint64_t exec_bits = LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_EXECUTE;
    LL_ALLOW(awww_path, exec_bits);
    LL_ALLOW("/usr/bin/sh", exec_bits);
    LL_ALLOW("/etc/ld.so.cache", LANDLOCK_ACCESS_FS_READ_FILE);
    LL_ALLOW("/usr/lib", LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_EXECUTE);

    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (runtime) LL_ALLOW(runtime, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_WRITE_FILE);

    LL_ALLOW(master_dir, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR);

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0) {
        landlock_restrict_self(rs_fd, 0);
    }
    close(rs_fd);
    #undef LL_ALLOW
}
#endif

typedef struct {
    char cmd;
    int interval;
    int target;
} WallMsg;

static volatile sig_atomic_t running = 1;
void handle_sig(int sig) { running = 0; }

void run_command(char cmd, int target) {
    Arg fwd = { .i = 1 }, bwd = { .i = -1 };
    switch (cmd) {
        case 'n': wall_cycle(&fwd); break;
        case 'p': wall_cycle(&bwd); break;
        case 'r': wall_random(NULL); break;
        case 's': wall_save(NULL); break;
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
    const char *display = getenv("WAYLAND_DISPLAY");
    int target_interval = 0, jump_target = 0, query_mode = 0, start_daemon = 0;
    int argi = 1;

    while (argi < argc && argv[argi][0] == '-') {
        if (!strcmp(argv[argi], "-S") && argi + 1 < argc) display = argv[++argi];
        else if (!strcmp(argv[argi], "-y") && argi + 1 < argc) target_interval = parse_duration(argv[++argi]);
        else if (!strcmp(argv[argi], "-j") && argi + 1 < argc) {
            char *end;
            long val = strtol(argv[++argi], &end, 10);
            jump_target = (*end != '\0' || val <= 0) ? -1 : (int)val;
        }
        else if (!strcmp(argv[argi], "-i")) query_mode = 1;
        else if (!strcmp(argv[argi], "-D")) start_daemon = 1;
        argi++;
    }

    get_sock_path(sock_path, sizeof(sock_path), display);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };

    /* Manual memcpy instead of strncpy */
    size_t plen = strlen(sock_path);
    if (plen >= sizeof(addr.sun_path)) return 1;
    memcpy(addr.sun_path, sock_path, plen + 1);

    /* CLIENT */
    if (argi < argc || jump_target > 0 || query_mode) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0 || connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            if (fd >= 0) close(fd);
            return 1;
        }
        WallMsg msg = { .interval = target_interval, .target = jump_target };
        if (query_mode) msg.cmd = 'i';
        else if (jump_target > 0) msg.cmd = 'j';
        else {
            const char *arg = argv[argi];
            if      (!strcmp(arg, "next"))   msg.cmd = 'n';
            else if (!strcmp(arg, "prev"))   msg.cmd = 'p';
            else if (!strcmp(arg, "rand"))   msg.cmd = 'r';
            else if (!strcmp(arg, "save"))   msg.cmd = 's';
            else if (!strcmp(arg, "reload"))  msg.cmd = 'R';
            else msg.cmd = arg[0];
        }
        write(fd, &msg, sizeof(WallMsg));
        if (query_mode) {
            char resp[512];
            int n = read(fd, resp, sizeof(resp) - 1);
            if (n > 0) { resp[n] = '\0'; printf("%s\n", resp); }
        }
        close(fd);
        return 0;
    }

    /* DAEMON */
    if (!start_daemon) return 0;
    unlink(sock_path);
    serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serv_fd < 0) return 1;

    struct sigaction sa = { .sa_handler = handle_sig };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (bind(serv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(serv_fd, 5) < 0) {
        close(serv_fd); return 1;
    }

    wall_restore();

#ifdef __linux__
    landlock_apply(master_dir, get_save_path(), "/usr/bin/awww-daemon");
#endif

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(serv_fd, &fds);
        struct timeval tv, *timeout = NULL;
        if (slideshow_interval > 0) { tv.tv_sec = slideshow_interval; tv.tv_usec = 0; timeout = &tv; }

        int activity = select(serv_fd + 1, &fds, NULL, NULL, timeout);
        if (activity == 0 && slideshow_interval > 0) { run_command(current_mode, 0); continue; }
        if (activity <= 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (FD_ISSET(serv_fd, &fds)) {
            int client_fd = accept(serv_fd, NULL, NULL);
            if (client_fd < 0) continue;
            WallMsg msg = {0};
            if (recv(client_fd, &msg, sizeof(WallMsg), MSG_WAITALL) == sizeof(WallMsg)) {
                if (msg.cmd == 'i') {
                    char resp[512];
                    int rlen = snprintf(resp, sizeof(resp), "%s | %d/%d", current_folder, cur + 1, count);
                    write(client_fd, resp, (rlen < (int)sizeof(resp)) ? rlen : sizeof(resp)-1);
                } else {
                    if (msg.interval >= 0) slideshow_interval = msg.interval;
                    if (strchr("npr", msg.cmd)) current_mode = msg.cmd;
                    run_command(msg.cmd, msg.target);
                }
            }
            close(client_fd);
        }
    }
    unlink(sock_path);
    close(serv_fd);
    return 0;
}

