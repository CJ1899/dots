#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "wall.h"

int count = 0;
int cur = 0;
char current_folder[256] = "";
char master_dir[1024] = "";
static char **files = NULL;
static int capacity = 0;

static size_t path_join(char *dest, size_t max, const char *p1, const char *p2, const char *p3) {
    size_t l1 = p1 ? strlen(p1) : 0;
    size_t l2 = p2 ? strlen(p2) : 0;
    size_t l3 = p3 ? strlen(p3) : 0;
    size_t total = l1 + (p2 ? 1 : 0) + l2 + (p3 ? 1 : 0) + l3;

    if (total >= max) return 0;

    char *ptr = dest;
    if (l1) { memcpy(ptr, p1, l1); ptr += l1; }
    if (p2) { *ptr++ = '/'; memcpy(ptr, p2, l2); ptr += l2; }
    if (p3) { *ptr++ = '/'; memcpy(ptr, p3, l3); ptr += l3; }
    *ptr = '\0';

    return total;
}

const char *get_save_path(void) {
    static char path[PATH_MAX];
    if (path[0]) return path;
    const char *home = getenv("HOME");
    if (!home) return NULL;

    size_t hlen = strlen(home);
    const char *suffix = "/etc/wallman";
    size_t slen = strlen(suffix);
    if (hlen + slen >= PATH_MAX) return NULL;

    memcpy(path, home, hlen);
    memcpy(path + hlen, suffix, slen + 1); /* +1 copies \0 */
    return path;
}

static void free_files(void) {
    if (!files) return;
    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
    files = NULL;
    count = 0;
    capacity = 0;
}

static int natural_sort(const void *a, const void *b) {
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    while (*s1 && *s2) {
        if (isdigit((unsigned char)*s1) && isdigit((unsigned char)*s2)) {
            char *p1, *p2;
            long n1 = strtol(s1, &p1, 10);
            long n2 = strtol(s2, &p2, 10);
            if (n1 != n2) return (n1 < n2) ? -1 : 1;
            s1 = p1; s2 = p2;
        } else {
            if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2))
                return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
            s1++; s2++;
        }
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

static void scan_and_fill(void) {
    char scan_path[PATH_MAX];
    if (!path_join(scan_path, sizeof(scan_path), master_dir, current_folder, NULL)) return;

    struct dirent **namelist;
    int n = scandir(scan_path, &namelist, NULL, NULL);
    if (n < 0) return;

    for (int i = 0; i < n; i++) {
        if (namelist[i]->d_name[0] != '.') {
            const char *ext = strrchr(namelist[i]->d_name, '.');
            if (ext && (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".png") == 0 ||
                        strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".webp") == 0)) {

                struct stat st;
                char fpath[PATH_MAX];
                if (path_join(fpath, sizeof(fpath), scan_path, namelist[i]->d_name, NULL)) {
                    if (stat(fpath, &st) == 0 && S_ISREG(st.st_mode)) {
                        char *s = strdup(namelist[i]->d_name);
                        if (s) {
                            /* Optimization: Exponential Realloc */
                            if (count >= capacity) {
                                capacity = capacity ? capacity * 2 : 16;
                                char **tmp = realloc(files, sizeof(char *) * capacity);
                                if (!tmp) { free(s); break; }
                                files = tmp;
                            }
                            files[count++] = s;
                        }
                    }
                }
            }
        }
        free(namelist[i]);
    }
    free(namelist);
    if (count > 0) qsort(files, count, sizeof(char *), natural_sort);
}

static void wall_init(void) {
    if (files) return;
    const char *save = get_save_path();
    if (master_dir[0] == '\0' && save) {
        FILE *f = fopen(save, "r");
        if (f) {
            char rel_path[PATH_MAX] = "";
            if (fgets(master_dir, sizeof(master_dir), f))
                master_dir[strcspn(master_dir, "\n")] = 0;
            if (fgets(rel_path, sizeof(rel_path), f))
                rel_path[strcspn(rel_path, "\n")] = 0;
            fclose(f);

            char d_tmp[PATH_MAX], b_tmp[PATH_MAX];
            size_t rlen = strlen(rel_path);
            if (rlen < PATH_MAX) {
                memcpy(d_tmp, rel_path, rlen + 1);
                memcpy(b_tmp, rel_path, rlen + 1);
                char *fdir = dirname(d_tmp);
                char *fname = basename(b_tmp);

                size_t flen = strlen(fdir);
                if (flen < sizeof(current_folder)) memcpy(current_folder, fdir, flen + 1);

                scan_and_fill();
                for (int i = 0; i < count; i++) {
                    if (strcmp(files[i], fname) == 0) { cur = i; break; }
                }
                return;
            }
        }
    }
    scan_and_fill();
}

static void run_setter(const char *filename) {
    if (!filename) return;
    char fullpath[PATH_MAX];
    if (!path_join(fullpath, sizeof(fullpath), master_dir, current_folder, filename)) return;

    pid_t pid = fork();
    if (pid == 0) {
	//execl("/usr/bin/awww", "awww", "img", "--transition-type", "fade", "--transition-duration", "0.1", fullpath, (char *)NULL);
	execl("/usr/bin/wpaperctl", "set", fullpath, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

void wall_reload(const void *arg) {
    char *current_name = (files && count > 0) ? strdup(files[cur]) : NULL;
    free_files();
    wall_init();
    if (current_name) {
        for (int i = 0; i < count; i++) {
            if (strcmp(files[i], current_name) == 0) { cur = i; break; }
        }
        free(current_name);
    }
}

void wall_cycle(const void *arg) {
    wall_init();
    if (count == 0) return;
    if (arg) {
        const Arg *a = (const Arg *)arg;
        cur = (cur + a->i + count) % count;
    }
    run_setter(files[cur]);
}

void wall_restore(void) {
    wall_init();
    if (count > 0) run_setter(files[cur]);
}

void wall_save(const void *arg) {
    const char *save = get_save_path();
    if (count == 0 || master_dir[0] == '\0' || !save) return;

    char temp_path[PATH_MAX];
    size_t slen = strlen(save);
    if (slen + 5 >= PATH_MAX) return;
    memcpy(temp_path, save, slen);
    memcpy(temp_path + slen, ".tmp", 5);

    FILE *f = fopen(temp_path, "w");
    if (!f) return;

    if (fprintf(f, "%s\n%s/%s\n", master_dir, current_folder, files[cur]) < 0) goto cleanup;
    if (fflush(f) != 0) goto cleanup;

    int fd = fileno(f);
    if (fd == -1 || fsync(fd) != 0) goto cleanup;
    if (fclose(f) != 0) { unlink(temp_path); return; }
    f = NULL;

    if (rename(temp_path, save) != 0) { unlink(temp_path); return; }

    char dirbuf[PATH_MAX];
    memcpy(dirbuf, save, slen + 1);
    char *dir = dirname(dirbuf);
    int dfd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd >= 0) { fsync(dfd); close(dfd); }
    return;

cleanup:
    if (f) fclose(f);
    unlink(temp_path);
}

void wall_random(const void *arg) {
    wall_init();
    if (count <= 1) return;
    cur = rand() % count;
    run_setter(files[cur]);
}

void wall_folder_select(const void *arg) {
    if (!arg || master_dir[0] == '\0') return;

    struct dirent **namelist;
    int n = scandir(master_dir, &namelist, NULL, alphasort);
    if (n < 0) return;

    char **folders = NULL;
    int f_count = 0, f_cur = 0, f_cap = 0;

    for (int i = 0; i < n; i++) {
        int is_dir = (namelist[i]->d_type == DT_DIR);
        if (namelist[i]->d_type == DT_UNKNOWN) {
            struct stat st;
            char full[PATH_MAX];
            if (path_join(full, sizeof(full), master_dir, namelist[i]->d_name, NULL)) {
                if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) is_dir = 1;
            }
        }

        if (is_dir && namelist[i]->d_name[0] != '.') {
            char *s = strdup(namelist[i]->d_name);
            if (s) {
                if (f_count >= f_cap) {
                    f_cap = f_cap ? f_cap * 2 : 8;
                    char **tmp = realloc(folders, sizeof(char *) * f_cap);
                    if (!tmp) { free(s); break; }
                    folders = tmp;
                }
                folders[f_count] = s;
                if (strcmp(folders[f_count], current_folder) == 0) f_cur = f_count;
                f_count++;
            }
        }
        free(namelist[i]);
    }
    free(namelist);

    if (f_count == 0) { free(folders); return; }

    int dir = ((Arg *)arg)->i;
    f_cur = (f_cur + dir + f_count) % f_count;

    size_t flen = strlen(folders[f_cur]);
    if (flen < sizeof(current_folder)) memcpy(current_folder, folders[f_cur], flen + 1);

    free_files();
    cur = 0;
    wall_init();
    if (count > 0) run_setter(files[cur]);

    for (int j = 0; j < f_count; j++) free(folders[j]);
    free(folders);
}

