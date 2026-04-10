#ifndef WALL_H
#define WALL_H

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>


static inline int is_wayland(void) {
    /* 1. If the env var is set, trust it */
    if (getenv("WAYLAND_DISPLAY")) {
        return 1;
    }

    /* 2. Only check the socket if we aren't sure */
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/run/user/%d/wayland-0", getuid());
    struct stat st;
    if (stat(path, &st) == 0) {
        /* Double check we aren't actually on X11 right now */
        if (getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
            return 0;
        }
        return 1;
    }
    return 0;
}

#ifdef WAYLAND_SUPPORT
void set_wallpaper_native(const void *arg);
#endif

void wall_cycle(const void *arg);
void wall_save(const void *arg);
void wall_restore(void);
void wall_random(const void *arg);
void wall_select(const void *arg);
void wall_reload(const void *arg);
void wall_folder_select(const void *arg);
//void render_wallpaper(const char *path);

#endif

