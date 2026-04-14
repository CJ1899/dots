#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <Imlib2.h>
#include <stdio.h>
#include <stdlib.h>
#include "wall.h"

/* EXACT setRootAtoms FROM HSETROOT SOURCE */
static void set_root_atoms(Display *display, int screen, Pixmap pixmap) {
    Atom atom_root, atom_eroot, type;
    unsigned char *data_root = NULL, *data_eroot = NULL;
    int format;
    unsigned long length, after;

    atom_root = XInternAtom(display, "_XROOTPMAP_ID", True);
    atom_eroot = XInternAtom(display, "ESETROOT_PMAP_ID", True);

    if (atom_root != None && atom_eroot != None) {
        XGetWindowProperty(display, RootWindow(display, screen),
                           atom_root, 0L, 1L, False, AnyPropertyType,
                           &type, &format, &length, &after, &data_root);

        if (type == XA_PIXMAP && data_root) {
            XGetWindowProperty(display, RootWindow(display, screen),
                               atom_eroot, 0L, 1L, False, AnyPropertyType,
                               &type, &format, &length, &after, &data_eroot);

            if (data_eroot && type == XA_PIXMAP &&
                *((Pixmap *)data_root) == *((Pixmap *)data_eroot)) {
                XKillClient(display, *((Pixmap *)data_root));
            }

            if (data_eroot) XFree(data_eroot);
        }

        if (data_root) XFree(data_root);
    }

    atom_root = XInternAtom(display, "_XROOTPMAP_ID", False);
    atom_eroot = XInternAtom(display, "ESETROOT_PMAP_ID", False);

    XChangeProperty(display, RootWindow(display, screen),
                    atom_root, XA_PIXMAP, 32, PropModeReplace,
                    (unsigned char *)&pixmap, 1);

    XChangeProperty(display, RootWindow(display, screen),
                    atom_eroot, XA_PIXMAP, 32, PropModeReplace,
                    (unsigned char *)&pixmap, 1);
}

void render_wallpaper(Display *display, const char *path) {
    if (!display || !path) return;

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);

    imlib_set_cache_size(0);
    imlib_context_set_display(display);
    imlib_context_set_visual(DefaultVisual(display, screen));
    imlib_context_set_colormap(DefaultColormap(display, screen));

    Imlib_Image buffer = imlib_load_image(path);
    if (!buffer) return;

    Imlib_Image rootimg = imlib_create_image(width, height);

    Pixmap pixmap = XCreatePixmap(display, root, width, height,
                                  DefaultDepth(display, screen));

    imlib_context_set_image(rootimg);
    imlib_context_set_drawable(pixmap);

    imlib_context_set_image(buffer);

    int imgW = imlib_image_get_width();
    int imgH = imlib_image_get_height();

    /* ---- SIMPLE CENTER CROP COVER ---- */
    double aspect = (double)width / imgW;
    if ((int)(imgH * aspect) < height)
        aspect = (double)height / imgH;

    int newW = (int)(imgW * aspect);
    int newH = (int)(imgH * aspect);

    int x = (width - newW) / 2;
    int y = (height - newH) / 2;

    imlib_blend_image_onto_image(buffer,
                                 0,
                                 0, 0, imgW, imgH,
                                 x, y, newW, newH);

    /* push */
    imlib_render_image_on_drawable(0, 0);
    set_root_atoms(display, screen, pixmap);

    XSetWindowBackgroundPixmap(display, root, pixmap);
    XClearWindow(display, root);

    imlib_free_image();
    imlib_context_set_image(rootimg);
    imlib_free_image();

    XFlush(display);
}

