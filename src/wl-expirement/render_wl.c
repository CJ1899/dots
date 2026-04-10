#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <wayland-client.h>
#include <Imlib2.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"


static struct {
    struct wl_display *display;
    struct wl_shm *shm;
    struct wl_compositor *compositor;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
} wl;

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t w, uint32_t h) {
    zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface) {
}

static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t id) {
    // Object removed from registry
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};


static void registry_handle_global(void *data, struct wl_registry *reg, uint32_t name, const char *intf, uint32_t ver) {
    if (!strcmp(intf, wl_compositor_interface.name))
        wl.compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 3);
    else if (!strcmp(intf, wl_shm_interface.name))
        wl.shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    else if (!strcmp(intf, zwlr_layer_shell_v1_interface.name))
        wl.layer_shell = wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, 1);
}

static const struct wl_registry_listener reg_listener = {
    .global = registry_handle_global,
    .global_remove = registry_global_remove,
};

static int create_shm_file(off_t size) {
    int fd = memfd_create("wl_shm", MFD_CLOEXEC);
    if (fd >= 0) ftruncate(fd, size);
    return fd;
}


void set_wallpaper_native(const char *path) {
    if (!wl.display) {
        wl.display = wl_display_connect(NULL);
        if (!wl.display) return;
        struct wl_registry *reg = wl_display_get_registry(wl.display);
        wl_registry_add_listener(reg, &reg_listener, NULL);
        wl_display_roundtrip(wl.display);
    }

    Imlib_Image img = imlib_load_image(path);
    if (!img) return;
    imlib_context_set_image(img);
    int w = imlib_image_get_width(), h = imlib_image_get_height();
    uint32_t *data = imlib_image_get_data_for_reading_only();

    size_t size = w * h * 4;
    int fd = create_shm_file(size);
    uint32_t *pool_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memcpy(pool_data, data, size);

    struct wl_shm_pool *pool = wl_shm_create_pool(wl.shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, w, h, w * 4, WL_SHM_FORMAT_ARGB8888);

    if (!wl.surface) {
        wl.surface = wl_compositor_create_surface(wl.compositor);
        wl.layer_surface = zwlr_layer_shell_v1_get_layer_surface(wl.layer_shell, wl.surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaper");
        zwlr_layer_surface_v1_set_anchor(wl.layer_surface, 15);
        zwlr_layer_surface_v1_set_size(wl.layer_surface, 0, 0);
        zwlr_layer_surface_v1_add_listener(wl.layer_surface, &layer_listener, NULL);
    }

    wl_surface_attach(wl.surface, buffer, 0, 0);
    wl_surface_commit(wl.surface);
    wl_display_roundtrip(wl.display);

    munmap(pool_data, size);
    close(fd);
    wl_shm_pool_destroy(pool);
    imlib_free_image();
}

