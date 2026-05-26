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
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <Imlib2.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wall.h"

/* ── state (mirrors wall.c) ──────────────────────────────────────────────── */
int  count = 0;
int  cur   = 0;
char current_folder[256] = "";
char master_dir[1024]    = "";

static char **files    = NULL;
static int    capacity = 0;

/* ── per-output slot ─────────────────────────────────────────────────────── */
typedef struct Output {
    struct wl_output             *wl_output;
    struct wl_surface            *surface;
    struct zwlr_layer_surface_v1 *ls;
    struct wl_buffer             *buf;
    int32_t                       x, y, w, h;
    int                           done;
    struct Output                *next;
} Output;

/* ── wayland globals ─────────────────────────────────────────────────────── */
static struct wl_display          *wl_dpy      = NULL;
static struct wl_registry         *registry    = NULL;
static struct wl_compositor       *compositor  = NULL;
static struct zwlr_layer_shell_v1 *layer_shell = NULL;
static struct wl_shm              *shm         = NULL;
static Output                     *outputs     = NULL;

/* ══════════════════════════════════════════════════════════════════════════
   Shared helpers (identical to wall.c)
   ══════════════════════════════════════════════════════════════════════════ */

static size_t path_join(char *dest, size_t max,
                         const char *p1, const char *p2, const char *p3)
{
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

const char *get_save_path(void)
{
    static char path[PATH_MAX];
    if (path[0]) return path;
    const char *home = getenv("HOME");
    if (!home) return NULL;
    size_t hlen = strlen(home);
    const char *suffix = "/etc/wallman";
    size_t slen = strlen(suffix);
    if (hlen + slen >= PATH_MAX) return NULL;
    memcpy(path, home, hlen);
    memcpy(path + hlen, suffix, slen + 1);
    return path;
}

static void free_files(void)
{
    if (!files) return;
    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
    files    = NULL;
    count    = 0;
    capacity = 0;
}

static int natural_sort(const void *a, const void *b)
{
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

static void scan_and_fill(void)
{
    char scan_path[PATH_MAX];
    if (!path_join(scan_path, sizeof(scan_path), master_dir, current_folder, NULL))
        return;

    struct dirent **namelist = NULL;
    int n = scandir(scan_path, &namelist, NULL, NULL);
    if (n < 0) return;

    for (int i = 0; i < n; i++) {
        if (count >= 10000) {
            for (int j = i; j < n; j++) free(namelist[j]);
            break;
        }
        if (namelist[i]->d_name[0] != '.') {
            const char *ext = strrchr(namelist[i]->d_name, '.');
            if (ext && (strcasecmp(ext, ".jpg")  == 0 ||
                        strcasecmp(ext, ".png")  == 0 ||
                        strcasecmp(ext, ".jpeg") == 0 ||
                        strcasecmp(ext, ".webp") == 0)) {
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
                                files    = tmp;
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
    if (count > 0)
        qsort(files, (size_t)count, sizeof(char *), natural_sort);
}

static void wall_init(void)
{
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
                char *fdir  = dirname(d_tmp);
                char *fname = basename(b_tmp);
                size_t flen = strlen(fdir);
                if (flen < sizeof(current_folder))
                    memcpy(current_folder, fdir, flen + 1);
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

/* ══════════════════════════════════════════════════════════════════════════
   Wayland / renderer
   ══════════════════════════════════════════════════════════════════════════ */

static int shm_create(size_t size)
{
    int fd = -1;
#ifdef __linux__
    fd = memfd_create("wallman-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd >= 0) goto truncate;
#endif
    {
        char tmp[] = "/tmp/wallman-XXXXXX";
        fd = mkostemp(tmp, O_CLOEXEC);
        if (fd < 0) return -1;
        unlink(tmp);
    }
truncate:
    if (ftruncate(fd, (off_t)size) < 0) { close(fd); return -1; }
    return fd;
}

/* wl_output listener */
static void output_geometry(void *data, struct wl_output *o,
    int32_t x, int32_t y, int32_t pw, int32_t ph,
    int32_t sub, const char *make, const char *model, int32_t transform)
{
    (void)o;(void)pw;(void)ph;(void)sub;(void)make;(void)model;(void)transform;
    Output *op = data; op->x = x; op->y = y;
}
static void output_mode(void *data, struct wl_output *o,
    uint32_t flags, int32_t w, int32_t h, int32_t refresh)
{
    (void)o;(void)refresh;
    Output *op = data;
    if (flags & WL_OUTPUT_MODE_CURRENT) { op->w = w; op->h = h; }
}
static void output_done(void *data, struct wl_output *o)
{
    (void)o; ((Output *)data)->done = 1;
}
static void output_scale(void *data, struct wl_output *o, int32_t f)
{
    (void)data;(void)o;(void)f;
}
static void output_name(void *data, struct wl_output *o, const char *n)
{
    (void)data;(void)o;(void)n;
}
static void output_description(void *data, struct wl_output *o, const char *d)
{
    (void)data;(void)o;(void)d;
}
static const struct wl_output_listener output_listener = {
    .geometry    = output_geometry,
    .mode        = output_mode,
    .done        = output_done,
    .scale       = output_scale,
    .name        = output_name,
    .description = output_description,
};

/* zwlr_layer_surface listener — ack configure */
static void ls_configure(void *data, struct zwlr_layer_surface_v1 *ls,
                          uint32_t serial, uint32_t w, uint32_t h)
{
    (void)data;(void)w;(void)h;
    zwlr_layer_surface_v1_ack_configure(ls, serial);
}
static void ls_closed(void *data, struct zwlr_layer_surface_v1 *ls)
{
    (void)data;(void)ls;
}
static const struct zwlr_layer_surface_v1_listener ls_listener = {
    .configure = ls_configure,
    .closed    = ls_closed,
};

/* registry listener */
static void registry_global(void *data, struct wl_registry *reg,
    uint32_t name, const char *iface, uint32_t version)
{
    (void)data;
    if (!strcmp(iface, wl_compositor_interface.name)) {
        compositor = wl_registry_bind(reg, name, &wl_compositor_interface,
                         version < 4 ? version : 4);
    } else if (!strcmp(iface, wl_shm_interface.name)) {
        shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    } else if (!strcmp(iface, zwlr_layer_shell_v1_interface.name)) {
        layer_shell = wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface,
                          version < 4 ? version : 4);
    } else if (!strcmp(iface, wl_output_interface.name)) {
        Output *op = calloc(1, sizeof *op);
        if (!op) return;
        op->wl_output = wl_registry_bind(reg, name, &wl_output_interface,
                             version < 4 ? version : 4);
        wl_output_add_listener(op->wl_output, &output_listener, op);
        op->next = outputs;
        outputs  = op;
    }
}
static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
    (void)data;(void)reg;(void)name;
}
static const struct wl_registry_listener registry_listener = {
    .global        = registry_global,
    .global_remove = registry_global_remove,
};

void wall_setup_renderer(void)
{
    if (wl_dpy) return;
    wl_dpy = wl_display_connect(NULL);
    if (!wl_dpy) { fprintf(stderr, "wallman: cannot connect to wayland\n"); exit(1); }
    registry = wl_display_get_registry(wl_dpy);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(wl_dpy); /* collect globals */
    wl_display_roundtrip(wl_dpy); /* drain output geometry */
    if (!compositor || !shm || !layer_shell) {
        fprintf(stderr, "wallman: missing compositor/shm/layer-shell\n");
        exit(1);
    }
    imlib_set_cache_size(0);
}

static void set_on_output(Output *op, Imlib_Image src, int imgW, int imgH)
{
    int ow = op->w, oh = op->h;
    if (ow <= 0 || oh <= 0) return;

    /* tear down previous surface for this output */
    if (op->buf)     { wl_buffer_destroy(op->buf);             op->buf     = NULL; }
    if (op->ls)      { zwlr_layer_surface_v1_destroy(op->ls);  op->ls      = NULL; }
    if (op->surface) { wl_surface_destroy(op->surface);        op->surface = NULL; }

    /* scale-to-cover */
    double ax = (double)ow / imgW;
    double ay = (double)oh / imgH;
    double a  = ax > ay ? ax : ay;
    int sw = (int)(imgW * a);
    int sh = (int)(imgH * a);
    int ox = (ow - sw) / 2;
    int oy = (oh - sh) / 2;

    /* blit into canvas */
    Imlib_Image canvas = imlib_create_image(ow, oh);
    if (!canvas) return;
    imlib_context_set_image(canvas);
    imlib_context_set_color(0, 0, 0, 255);
    imlib_image_fill_rectangle(0, 0, ow, oh);
    imlib_context_set_blend(1);
    imlib_blend_image_onto_image(src, 0, 0, 0, imgW, imgH, ox, oy, sw, sh);

    /* shm */
    size_t stride = (size_t)ow * 4;
    size_t sz     = stride * (size_t)oh;
    int fd        = shm_create(sz);
    if (fd < 0) { imlib_context_set_image(canvas); imlib_free_image_and_decache(); return; }

    void *mem = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        close(fd);
        imlib_context_set_image(canvas);
        imlib_free_image_and_decache();
        return;
    }

    DATA32 *pixels = imlib_image_get_data_for_reading_only();
    memcpy(mem, pixels, sz);
    munmap(mem, sz);

    imlib_context_set_image(canvas);
    imlib_free_image_and_decache();

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)sz);
    close(fd);
    op->buf = wl_shm_pool_create_buffer(pool, 0, ow, oh,
                  (int32_t)stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);

    /* layer surface */
    op->surface = wl_compositor_create_surface(compositor);
    op->ls = zwlr_layer_shell_v1_get_layer_surface(
                 layer_shell, op->surface, op->wl_output,
                 ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaper");

    zwlr_layer_surface_v1_add_listener(op->ls, &ls_listener, op);
    zwlr_layer_surface_v1_set_size(op->ls, (uint32_t)ow, (uint32_t)oh);
    zwlr_layer_surface_v1_set_anchor(op->ls,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP    |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT   |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(op->ls, -1);

    wl_surface_commit(op->surface);
    wl_display_roundtrip(wl_dpy); /* compositor configure → ls_configure acks */

    wl_surface_attach(op->surface, op->buf, 0, 0);
    wl_surface_damage_buffer(op->surface, 0, 0, ow, oh);
    wl_surface_commit(op->surface);
    wl_display_roundtrip(wl_dpy);
}

static void run_setter(const char *filename)
{
    if (!wl_dpy) { fprintf(stderr, "wallman: renderer not initialized\n"); return; }

    char fullpath[PATH_MAX];
    if (!path_join(fullpath, sizeof(fullpath), master_dir, current_folder, filename))
        return;

    Imlib_Image img = imlib_load_image(fullpath);
    if (!img) { fprintf(stderr, "wallman: failed to load %s\n", fullpath); return; }

    imlib_context_set_image(img);
    int imgW = imlib_image_get_width();
    int imgH = imlib_image_get_height();

    for (Output *op = outputs; op; op = op->next)
        if (op->done)
            set_on_output(op, img, imgW, imgH);

    imlib_context_set_image(img);
    imlib_free_image_and_decache();
}

/* ══════════════════════════════════════════════════════════════════════════
   Public API — all functions from wall.c
   ══════════════════════════════════════════════════════════════════════════ */

void wall_restore(void)
{
    wall_init();
    if (count > 0) run_setter(files[cur]);
}

void wall_cycle(const void *arg)
{
    wall_init();
    if (count == 0) return;
    if (arg) { const Arg *a = arg; cur = (cur + a->i + count) % count; }
    run_setter(files[cur]);
}

void wall_reload(const void *arg)
{
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

void wall_save(const void *arg)
{
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

void wall_random(const void *arg)
{
    (void)arg;
    wall_init();
    if (count <= 1) return;
    cur = rand() % count;
    run_setter(files[cur]);
}

void wall_folder_select(const void *arg)
{
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
            if (path_join(full, sizeof(full), master_dir, namelist[i]->d_name, NULL))
                if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) is_dir = 1;
        }
        if (is_dir && namelist[i]->d_name[0] != '.') {
            char *s = strdup(namelist[i]->d_name);
            if (s) {
                if (f_count >= f_cap) {
                    f_cap = f_cap ? f_cap * 2 : 8;
                    char **tmp = realloc(folders, sizeof(char *) * f_cap);
                    if (!tmp) { free(s); free(namelist[i]); break; }
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
    if (flen < sizeof(current_folder))
        memcpy(current_folder, folders[f_cur], flen + 1);

    free_files();
    cur = 0;
    wall_init();
    if (count > 0) run_setter(files[cur]);

    for (int j = 0; j < f_count; j++) free(folders[j]);
    free(folders);
}

