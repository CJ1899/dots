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

#include "wall.h"
#include "ipc.h"

char sock_path[104];
static int serv_fd = -1;
static int slideshow_interval = 0;
static char current_mode = 'n';
extern char current_folder[256];
extern int count, cur;

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

static void landlock_apply(const char *master_dir, const char *save_path, const char *hsetroot_path) {
    int abi = landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
    if (abi < 0) {
        if (errno == ENOSYS) fprintf(stderr, "Landlock not supported by kernel.\n");
        return;
    }

    struct landlock_ruleset_attr rs_attr = {
        .handled_access_fs =
            LANDLOCK_ACCESS_FS_READ_FILE  | LANDLOCK_ACCESS_FS_READ_DIR   |
            LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_MAKE_REG   |
            LANDLOCK_ACCESS_FS_REMOVE_FILE| LANDLOCK_ACCESS_FS_EXECUTE,
    };

    int rs_fd = landlock_create_ruleset(&rs_attr, sizeof(rs_attr), 0);
    if (rs_fd < 0) {
        perror("Failed to create Landlock ruleset");
        return;
    }

    #define LL_ALLOW(path, access) do { \
        int _fd = open((path), O_PATH | O_CLOEXEC); \
        if (_fd < 0) { \
            fprintf(stderr, "Warning: Landlock could not find path: %s (%s)\n", path, strerror(errno)); \
        } else { \
            struct landlock_path_beneath_attr pb = { \
                .allowed_access = (access), \
                .parent_fd = _fd, \
            }; \
            if (landlock_add_rule(rs_fd, LANDLOCK_RULE_PATH_BENEATH, &pb, 0) < 0) { \
                fprintf(stderr, "Warning: Landlock failed to allow %s: %s\n", path, strerror(errno)); \
            } \
            close(_fd); \
        } \
    } while(0)

    /* 1. Execution & Dynamic Linker */
    uint64_t exec_bits = LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_EXECUTE;
    LL_ALLOW(hsetroot_path, exec_bits);
    LL_ALLOW("/lib64/ld-linux-x86-64.so.2", exec_bits);
    LL_ALLOW("/usr/bin/sh", exec_bits);
    LL_ALLOW("/etc/ld.so.cache", LANDLOCK_ACCESS_FS_READ_FILE);

    /* 2. System Libraries */
    uint64_t lib_bits = LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_EXECUTE;
    LL_ALLOW("/lib", lib_bits);
    LL_ALLOW("/usr/lib", lib_bits);
    LL_ALLOW("/lib64", lib_bits);

    /* 3. X11 & Auth */
    LL_ALLOW("/tmp/.X11-unix", LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_WRITE_FILE);
    char *xauth = getenv("XAUTHORITY");
    if (xauth) LL_ALLOW(xauth, LANDLOCK_ACCESS_FS_READ_FILE);

    /* 4. Data Paths */
    LL_ALLOW(master_dir, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR);
    LL_ALLOW(sock_path, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_WRITE_FILE);

    if (save_path) {
        char buf[PATH_MAX], *dname;
        snprintf(buf, sizeof(buf), "%s", save_path);
        dname = dirname(buf);
        LL_ALLOW(dname, LANDLOCK_ACCESS_FS_READ_FILE  | LANDLOCK_ACCESS_FS_WRITE_FILE |
                        LANDLOCK_ACCESS_FS_MAKE_REG   | LANDLOCK_ACCESS_FS_REMOVE_FILE);
    }

    /* 5. Enforce */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        perror("Failed to set NO_NEW_PRIVS");
    }
    if (landlock_restrict_self(rs_fd, 0) < 0) {
        perror("Failed to enforce Landlock sandbox");
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
        else if (!strcmp(argv[argi], "-h") || !strcmp(argv[argi], "--help")) {
            printf("Usage: wl [-S :display] [-y dur] [-j idx] [-i] <cmd>\n");
            printf("       wl -D | daemon    (Start background process)\n\n");
            printf("Commands: next, prev, rand, menu, save, reload, fnext, fprev\n");
            return 0;
        } else break;
        argi++;
    }

    if (argi < argc && !strcmp(argv[argi], "daemon")) {
        start_daemon = 1;
        argi++;
    }

    if (argc == 1) {
        printf("Usage: wl [-S :display] [-y dur] [-j idx] [-i] <cmd>\n");
        printf("       wl -D | daemon    (Start background process)\n\n");
        printf("Commands: next, prev, rand, menu, save, reload, fnext, fprev\n");
        return 0;
    }

    if (jump_target == -1) {
        fprintf(stderr, "Error: -j requires a positive integer index.\n");
        return 1;
    }

    get_sock_path(sock_path, sizeof(sock_path), display);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    size_t slen = strlen(sock_path);
    if (slen >= sizeof(addr.sun_path)) return 1;
    memcpy(addr.sun_path, sock_path, slen + 1);

    /* CLIENT SIDE */
    if (argi < argc || jump_target > 0 || query_mode) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0 || connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            fprintf(stderr, "Error: Could not connect to daemon. Is it running?\n");
            if (fd >= 0) close(fd);
            return 1;
        }

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
        close(fd);
        return 0;
    }

    /* DAEMON SIDE */
    if (!start_daemon) return 0;

    int test_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (test_fd >= 0) {
        if (connect(test_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            fprintf(stderr, "Error: daemon already running on display %s\n", display ? display : ":0");
            close(test_fd);
            return 1;
        }
        close(test_fd);
    }

    unlink(sock_path);
    serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serv_fd < 0) return 1;
    fcntl(serv_fd, F_SETFD, FD_CLOEXEC);
    srand(time(NULL));

    struct sigaction sa = { .sa_handler = handle_sig };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct sigaction sa_chld = { .sa_handler = SIG_IGN };
    sigaction(SIGCHLD, &sa_chld, NULL);

    umask(0077);
    if (bind(serv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(serv_fd, 5) < 0) {
        close(serv_fd); return 1;
    }

    wall_restore();

#ifdef __linux__
    landlock_apply(master_dir, get_save_path(), "/usr/bin/hsetroot");
#endif

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(serv_fd, &fds);
        struct timeval tv, *timeout = NULL;

        if (slideshow_interval > 0) {
            tv.tv_sec = slideshow_interval;
            tv.tv_usec = 0;
            timeout = &tv;
        }

        int activity = select(serv_fd + 1, &fds, NULL, NULL, timeout);
        if (activity < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (activity == 0 && slideshow_interval > 0) {
            run_command(current_mode, 0);
            continue;
        }

        if (FD_ISSET(serv_fd, &fds)) {
            int client_fd = accept4(serv_fd, NULL, NULL, SOCK_CLOEXEC);
            if (client_fd < 0) continue;

            struct ucred cred;
            socklen_t len = sizeof(struct ucred);
            if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0 || cred.uid != getuid()) {
                close(client_fd);
                continue;
            }

            WallMsg msg = {0};
            if (recv(client_fd, &msg, sizeof(WallMsg), MSG_WAITALL) == sizeof(WallMsg)) {
                if (strchr("nprsmudRji", msg.cmd)) {
                    if (msg.cmd == 'i') {
                        char resp[512];
                        int rlen = snprintf(resp, sizeof(resp), "%s | %d/%d", current_folder, cur + 1, count);
                        if (rlen >= (int)sizeof(resp)) rlen = sizeof(resp) - 1;
                        (void)write(client_fd, resp, (rlen < 0) ? 0 : rlen);
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

