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
#include "wall.h"

int count = 0;
int cur = 0;
char current_folder[256] = "";

static char master_dir[1024] = "";
static char **files = NULL;

static const char *get_save_path(void) {
    static char path[PATH_MAX];
    if (path[0]) return path;
    const char *home = getenv("HOME");
    if (!home) return NULL;
    /* Using snprintf instead of strncpy to ensure termination */
    if (snprintf(path, sizeof(path), "%s/etc/wallman", home) >= (int)sizeof(path))
        return NULL;
    return path;
}

static void free_files(void) {
    if (!files) return;
    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
    files = NULL;
    count = 0;
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
    int r = snprintf(scan_path, sizeof(scan_path), "%s/%s", master_dir, current_folder);
    if (r < 0 || r >= (int)sizeof(scan_path)) return;

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
                if (snprintf(fpath, sizeof(fpath), "%s/%s", scan_path, namelist[i]->d_name) < (int)sizeof(fpath)) {
                    if (stat(fpath, &st) == 0 && S_ISREG(st.st_mode)) {
                        /* strdup first so we don't realloc if it fails */
                        char *s = strdup(namelist[i]->d_name);
                        if (s) {
                            char **tmp = realloc(files, sizeof(char *) * (count + 1));
                            if (tmp) {
                                files = tmp;
                                files[count++] = s;
                            } else {
                                free(s);
                            }
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
    static int seeded = 0;
    if (!seeded) { srand(time(NULL)); seeded = 1; }
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
            snprintf(d_tmp, sizeof(d_tmp), "%s", rel_path);
            snprintf(b_tmp, sizeof(b_tmp), "%s", rel_path);

            char *fdir = dirname(d_tmp);
            char *fname = basename(b_tmp);
            snprintf(current_folder, sizeof(current_folder), "%s", fdir);

            scan_and_fill();
            for (int i = 0; i < count; i++) {
                if (strcmp(files[i], fname) == 0) { cur = i; break; }
            }
            return;
        }
    }
    scan_and_fill();
}

static void run_setter(const char *filename) {
    if (!filename) return;
    char fullpath[PATH_MAX];
    int r = snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", master_dir, current_folder, filename);
    if (r < 0 || r >= (int)sizeof(fullpath)) return;

    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/hsetroot", "hsetroot", "-cover", fullpath, (char *)NULL);
        _exit(1);
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
    FILE *f = fopen(save, "w");
    if (f) {
        fprintf(f, "%s\n%s/%s\n", master_dir, current_folder, files[cur]);
        fclose(f);
    }
}

void wall_random(const void *arg) {
    wall_init();
    if (count <= 1) return;
    cur = rand() % count;
    run_setter(files[cur]);
}

void wall_select(const void *arg) {
    wall_init();
    if (count == 0) return;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "seq 1 %d | rofi -dmenu -i -p 'Wallpaper:'", count);
    FILE *fp = popen(cmd, "r");
    if (!fp) return;
    char result[32];
    if (fgets(result, sizeof(result), fp)) {
        char *end;
        long selection = strtol(result, &end, 10);
        if (selection > 0 && selection <= count) {
            cur = (int)selection - 1;
            run_setter(files[cur]);
        }
    }
    pclose(fp);
}

void wall_folder_select(const void *arg) {
    if (!arg || master_dir[0] == '\0') return;

    struct dirent **namelist;
    int n = scandir(master_dir, &namelist, NULL, alphasort);
    if (n < 0) return;

    char **folders = NULL;
    int f_count = 0, f_cur = 0;

    for (int i = 0; i < n; i++) {
        /* Speed: Use d_type first. If DT_UNKNOWN, fall back to stat */
        int is_dir = (namelist[i]->d_type == DT_DIR);

        if (namelist[i]->d_type == DT_UNKNOWN) {
            struct stat st;
            char full[PATH_MAX];
            if (snprintf(full, sizeof(full), "%s/%s", master_dir, namelist[i]->d_name) < (int)sizeof(full)) {
                if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
                    is_dir = 1;
            }
        }

        if (is_dir && namelist[i]->d_name[0] != '.') {
            char *s = strdup(namelist[i]->d_name);
            if (s) {
                char **tmp = realloc(folders, sizeof(char *) * (f_count + 1));
                if (tmp) {
                    folders = tmp;
                    folders[f_count] = s;
                    if (strcmp(folders[f_count], current_folder) == 0) f_cur = f_count;
                    f_count++;
                } else {
                    free(s);
                }
            }
        }
        free(namelist[i]);
    }
    free(namelist);

    if (f_count == 0) { free(folders); return; }

    int dir = ((Arg *)arg)->i;
    f_cur = (f_cur + dir + f_count) % f_count;

    /* Update current_folder and reset image index */
    snprintf(current_folder, sizeof(current_folder), "%s", folders[f_cur]);

    free_files();
    cur = 0; /* Reset to first image in new folder */

    wall_init();
    if (count > 0) run_setter(files[cur]);

    for (int j = 0; j < f_count; j++) free(folders[j]);
    free(folders);
}

