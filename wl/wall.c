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
#include "wall.h"

#define SAVE_PATH "/home/pc/.config/X11/wall"
static char master_dir[1024] = "";
static char current_folder[256] = "";

typedef struct { int i; } FakeArg;
static char **files = NULL;
static int count = 0;
static int cur = 0;


static int natural_sort(const void *a, const void *b) {
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;

    while (*s1 && *s2) {
        if (isdigit(*s1) && isdigit(*s2)) {
            char *ptr1, *ptr2;
            long n1 = strtol(s1, &ptr1, 10);
            long n2 = strtol(s2, &ptr2, 10);
            if (n1 != n2) return n1 - n2;
            s1 = ptr1;
            s2 = ptr2;
        } else {
            if (tolower(*s1) != tolower(*s2))
                return tolower(*s1) - tolower(*s2);
            s1++; s2++;
        }
    }
    return *s1 - *s2;
}

static void scan_and_fill(void) {
    char scan_path[2048];
    snprintf(scan_path, sizeof(scan_path), "%s/%s", master_dir, current_folder);

    struct dirent **namelist;
    int n = scandir(scan_path, &namelist, NULL, NULL);
    if (n < 0) return;

    while (n--) {
        if (namelist[n]->d_name[0] != '.') {
            const char *ext = strrchr(namelist[n]->d_name, '.');
            if (ext && (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".png") == 0 ||
                        strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".webp") == 0)) {
                files = realloc(files, sizeof(char *) * (count + 1));
                files[count++] = strdup(namelist[n]->d_name);
            }
        }
        free(namelist[n]);
    }
    free(namelist);
    if (count > 0) qsort(files, count, sizeof(char *), natural_sort);
}

static void wall_init(void) {
    if (files) return;

    if (master_dir[0] == '\0') {
        FILE *f = fopen(SAVE_PATH, "r");
        char rel_path[512] = "";

        if (f) {
            if (fgets(master_dir, sizeof(master_dir), f))
                master_dir[strcspn(master_dir, "\n")] = 0;
            if (fgets(rel_path, sizeof(rel_path), f))
                rel_path[strcspn(rel_path, "\n")] = 0;
            fclose(f);

            /* Extraction */
            char temp[512];
            strcpy(temp, rel_path);
            char *fname = basename(temp);
            char *fdir = dirname(temp);

            strncpy(current_folder, fdir, sizeof(current_folder)-1);
            current_folder[sizeof(current_folder)-1] = '\0';

            /* Perform the scan */
            scan_and_fill();

            /* Set index to saved file */
            for (int i = 0; i < count; i++) {
                if (strcmp(files[i], fname) == 0) {
                    cur = i;
                    break;
                }
            }
            return;
    }
}
    scan_and_fill();
}

void wall_reload(const void *arg) {
    if (!files || count == 0) {
        wall_init();
        return;
    }

    char *current_name = strdup(files[cur]);

    /* 2. Free the old list */
    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
    files = NULL;
    count = 0;

    /* 3. Re-scan the folder */
    wall_init();

    /* 4. Find where our wallpaper moved to in the new list */
    if (current_name) {
        for (int i = 0; i < count; i++) {
            if (strcmp(files[i], current_name) == 0) {
                cur = i;
                break;
            }
        }
        free(current_name);
    }

    /* If the file was deleted, cur stays at 0 by default from wall_init */
}

static void run_setter(const char *filename) {
    char fullpath[2048];
    snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", master_dir, current_folder, filename);

// Write current status to a temp file for the status bar
    FILE *f = fopen("/tmp/wall_info", "w");
    if (f) {
        // Formats as: "folder | 5/100"
        fprintf(f, "%s | %d/%d", current_folder, cur + 1, count);
        fclose(f);
    }

    if (fork() == 0) {
        execlp("hsetroot", "hsetroot", "-cover", fullpath, (char *)NULL);
        exit(1);
    }
}

void wall_cycle(const void *arg) {
    wall_init();
    if (count == 0) return;

    if (arg) {
        const FakeArg *a = (const FakeArg *)arg;
        cur = (cur + a->i + count) % count;
    }

    run_setter(files[cur]);
}

void wall_restore(void) {
    wall_init();
    if (count > 0) run_setter(files[cur]);
}

void wall_save(const void *arg) {
    if (count == 0 || master_dir[0] == '\0') return;
    FILE *f = fopen(SAVE_PATH, "w");
    if (f) {
        fprintf(f, "%s\n%s/%s\n", master_dir, current_folder, files[cur]);
        fclose(f);
    }
}

void wall_random(const void *arg) {
    wall_init();
    if (count <= 1) return;

    /* Seed the randomizer using the current time */
    srand(time(NULL));
    cur = rand() % count;

    /* Re-use your existing logic to apply the change */
    wall_cycle(NULL);
}

void wall_select(const void *arg) {
    wall_init();
    if (count == 0) return;

    char cmd[256];
    char result[32];
    //snprintf(cmd, sizeof(cmd), "seq 1 %d | dmenu -p 'Go to wallpaper:'", count);
    snprintf(cmd, sizeof(cmd), "seq 1 %d | rofi -dmenu -i -p 'Wallpaper:'", count);

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) return;

    if (fgets(result, sizeof(result), fp) != NULL) {
        int selection = atoi(result);
        if (selection > 0 && selection <= count) {
            cur = selection - 1; // Convert to 0-indexed
            wall_cycle(NULL);    // This applies the change via hsetroot
        }
    }
    pclose(fp);
}

void wall_folder_select(const void *arg) {
    if (master_dir[0] == '\0') wall_init();

    struct dirent **namelist;
    int n = scandir(master_dir, &namelist, NULL, alphasort);
    if (n < 0) return;

    char **folders = NULL;
    int f_count = 0, f_cur = 0;

    for (int i = 0; i < n; i++) {
        if (namelist[i]->d_type == DT_DIR && namelist[i]->d_name[0] != '.') {
            folders = realloc(folders, sizeof(char *) * (f_count + 1));
            folders[f_count] = strdup(namelist[i]->d_name);
            if (strcmp(folders[f_count], current_folder) == 0) f_cur = f_count;
            f_count++;
        }
        free(namelist[i]);
    }
    free(namelist);

    /* Shift logic */
    int dir = ((FakeArg *)arg)->i;
    f_cur = (f_cur + dir + f_count) % f_count;
    strcpy(current_folder, folders[f_cur]);

    /* Cleanup RAM for new folder */
    for (int j = 0; j < count; j++) free(files[j]);
    free(files);
    files = NULL;
    count = 0;
    cur = 0; /* Jumps to first image in NEW folder */

    /* Re-scan using current_folder in RAM */
    wall_init();
    if (count > 0) run_setter(files[cur]);

    for (int j = 0; j < f_count; j++) free(folders[j]);
    free(folders);
}

