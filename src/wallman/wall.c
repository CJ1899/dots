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
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>
#include <Imlib2.h>
#include "wall.h"

int count = 0;
int cur = 0;
char current_folder[256] = "";
char master_dir[1024] = "";
static char **files = NULL;
static int capacity = 0;

/* Internal Renderer State */
static Display *display = NULL;
static int screen;
//static Imlib_Image current_imlib_image = NULL;
static Pixmap last_pixmap = None;

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

/* Initialize X11 and Imlib once */
void wall_setup_renderer(void) {
    if (display) return;
    display = XOpenDisplay(NULL);
    if (!display) exit(1);
    screen = DefaultScreen(display);

    imlib_context_set_display(display);
    imlib_context_set_visual(DefaultVisual(display, screen));
    imlib_context_set_colormap(DefaultColormap(display, screen));
//    imlib_set_cache_size(64 * 1024 * 1024);
    imlib_set_cache_size(0);
}

/*static void prefetch_neighbors(void) {
    if (count < 2) return;
    int neighbors[2] = { (cur - 1 + count) % count, (cur + 1) % count };

    for (int i = 0; i < 2; i++) {
        char path[PATH_MAX];
        if (path_join(path, sizeof(path), master_dir, current_folder, files[neighbors[i]])) {
            Imlib_Image img = imlib_load_image(path);
            if (img) {
                imlib_context_set_image(img);
                imlib_free_image(); // Remains in Imlib2's 32MB cache
            }
        }
    }
}*/



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
    if (!path_join(scan_path, sizeof(scan_path), master_dir, current_folder, NULL))
        return;

    struct dirent **namelist = NULL;
    int n = scandir(scan_path, &namelist, NULL, NULL);
    if (n < 0)
        return;

    for (int i = 0; i < n; i++) {
        if (count >= 10000) {
            for (int j = i; j < n; j++) free(namelist[j]);
            break;
        }

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
                            if (count >= capacity) {
                                int new_cap = capacity ? capacity * 2 : 16;
                                char **tmp = realloc(files, sizeof(char *) * (size_t)new_cap);
                                if (!tmp) {
                                    free(s);
                                    for (int j = i; j < n; j++) free(namelist[j]);
                                    break;
                                }
                                files = tmp;
                                capacity = new_cap;
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

    if (count > 0) {
        qsort(files, (size_t)count, sizeof(char *), natural_sort);
    }
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
    if (!display) {
        fprintf(stderr, "Error: X11 Display not initialized\n");
        return;
    }

    char fullpath[PATH_MAX];
    if (!path_join(fullpath, sizeof(fullpath), master_dir, current_folder, filename))
        return;

    Imlib_Image buffer = imlib_load_image(fullpath);
    if (!buffer) {
        fprintf(stderr, "Error: Imlib2 failed to load %s\n", fullpath);
        return;
    }

    // Get Image and Screen dimensions
    imlib_context_set_image(buffer);
    int imgW = imlib_image_get_width();
    int imgH = imlib_image_get_height();
    int sw   = DisplayWidth(display, screen);
    int sh   = DisplayHeight(display, screen);
    Window root = RootWindow(display, screen);

    // Create the canvas for the root window
    Imlib_Image rootimg = imlib_create_image(sw, sh);
    if (!rootimg) {
        imlib_context_set_image(buffer);
        imlib_free_image_and_decache();
        return;
    }

    imlib_context_set_image(rootimg);
    imlib_context_set_color(0, 0, 0, 255);
    imlib_image_fill_rectangle(0, 0, sw, sh);
    imlib_context_set_dither(1);
    imlib_context_set_blend(1);

    // Handle Multi-Monitor via Xinerama
    int noutputs;
    XineramaScreenInfo *outputs = XineramaQueryScreens(display, &noutputs);
    XineramaScreenInfo fake = { .x_org = 0, .y_org = 0, .width = sw, .height = sh };
    if (!outputs) { outputs = &fake; noutputs = 1; }

    for (int i = 0; i < noutputs; i++) {
        double aspect = (double)outputs[i].width / imgW;
        if ((int)(imgH * aspect) < outputs[i].height)
            aspect = (double)outputs[i].height / imgH;

        int scaledW = (int)(imgW * aspect);
        int scaledH = (int)(imgH * aspect);
        int left = (outputs[i].width  - scaledW) / 2;
        int top  = (outputs[i].height - scaledH) / 2;

        imlib_context_set_image(rootimg);
        imlib_blend_image_onto_image(buffer, 0, 0, 0, imgW, imgH,
                                     outputs[i].x_org + left,
                                     outputs[i].y_org + top,
                                     scaledW, scaledH);
    }

    // Create the X11 Pixmap
    Pixmap new_pixmap = XCreatePixmap(display, root, sw, sh, DefaultDepth(display, screen));
    imlib_context_set_image(rootimg);
    imlib_context_set_drawable(new_pixmap);
    imlib_render_image_on_drawable(0, 0);

    XGrabServer(display);

    // Set the background
    XSetWindowBackgroundPixmap(display, root, new_pixmap);
    XClearWindow(display, root);

    // Set properties so compositors/terminals see the background
    Atom a_root  = XInternAtom(display, "_XROOTPMAP_ID",    False);
    Atom a_eroot = XInternAtom(display, "ESETROOT_PMAP_ID", False);
    XChangeProperty(display, root, a_root,  XA_PIXMAP, 32, PropModeReplace,
                    (unsigned char *)&new_pixmap, 1);
    XChangeProperty(display, root, a_eroot, XA_PIXMAP, 32, PropModeReplace,
                    (unsigned char *)&new_pixmap, 1);

    if (last_pixmap != None) {
        XFreePixmap(display, last_pixmap);
    }
    last_pixmap = new_pixmap;

    //XSetCloseDownMode(display, RetainTemporary);

    XUngrabServer(display);
    XFlush(display);

    // Cleanup Imlib2 Memory (Client-side)
    imlib_context_set_image(rootimg);
    imlib_free_image_and_decache();
    imlib_context_set_image(buffer);
    imlib_free_image_and_decache();

    if (outputs != &fake)
        XFree(outputs);
}

void wall_reload(const void *arg) {
    (void)arg;

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
    (void)arg;

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
    (void)arg;

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

