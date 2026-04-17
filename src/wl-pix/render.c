#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>
#include <Imlib2.h>
#include <stdio.h>
#include <stdlib.h>
#include "wall.h"

static Pixmap last_pixmap = None;

static void
set_root_atoms(Display *dpy, int scr, Pixmap pixmap)
{
    Atom atom_root  = XInternAtom(dpy, "_XROOTPMAP_ID",    False);
    Atom atom_eroot = XInternAtom(dpy, "ESETROOT_PMAP_ID", False);

    XChangeProperty(dpy, RootWindow(dpy, scr),
                    atom_root, XA_PIXMAP, 32, PropModeReplace,
                    (unsigned char *)&pixmap, 1);
    XChangeProperty(dpy, RootWindow(dpy, scr),
                    atom_eroot, XA_PIXMAP, 32, PropModeReplace,
                    (unsigned char *)&pixmap, 1);
}

void
render_wallpaper(const char *path)
{
    if (!path) return;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "wl: render: cannot open display\n");
        return;
    }

    int scr     = DefaultScreen(dpy);
    int width   = DisplayWidth(dpy, scr);
    int height  = DisplayHeight(dpy, scr);
    int depth   = DefaultDepth(dpy, scr);
    Window root = RootWindow(dpy, scr);

    /* kill previous pixmap before creating new one */
    if (last_pixmap != None) {
        XKillClient(dpy, last_pixmap);
        XSync(dpy, False);
        last_pixmap = None;
    }

    /* xinerama */
    int noutputs = 0;
    XineramaScreenInfo *outputs = XineramaQueryScreens(dpy, &noutputs);
    if (!outputs || noutputs == 0) {
        outputs  = NULL;
        noutputs = 0;
    }

    /* fresh imlib context */
    Imlib_Context *ctx = imlib_context_new();
    imlib_context_push(ctx);

    imlib_context_set_display(dpy);
    imlib_context_set_visual(DefaultVisual(dpy, scr));
    imlib_context_set_colormap(DefaultColormap(dpy, scr));
    imlib_context_set_dither(1);
    imlib_context_set_blend(1);

    /* create pixmap and canvas */
    Pixmap pixmap = XCreatePixmap(dpy, root, width, height, depth);
    imlib_context_set_drawable(pixmap);

    Imlib_Image canvas = imlib_create_image(width, height);
    imlib_context_set_image(canvas);

    /* fill black */
    imlib_context_set_color(0, 0, 0, 255);
    imlib_image_fill_rectangle(0, 0, width, height);

    /* load source image */
    Imlib_Image buffer = imlib_load_image(path);
    if (!buffer) {
        fprintf(stderr, "wl: render: failed to load image: %s\n", path);
        imlib_free_image();
        imlib_context_pop();
        imlib_context_free(ctx);
        if (outputs) XFree(outputs);
        XFreePixmap(dpy, pixmap);
        XCloseDisplay(dpy);
        return;
    }

    imlib_context_set_image(buffer);
    int imgW = imlib_image_get_width();
    int imgH = imlib_image_get_height();

    /* blend onto canvas */
    imlib_context_set_image(canvas);

    if (outputs && noutputs > 0) {
        for (int i = 0; i < noutputs; i++) {
            int ox = outputs[i].x_org;
            int oy = outputs[i].y_org;
            int ow = outputs[i].width;
            int oh = outputs[i].height;

            imlib_context_set_cliprect(ox, oy, ow, oh);

            double aspect = (double)ow / imgW;
            if ((int)(imgH * aspect) < oh)
                aspect = (double)oh / imgH;

            int top  = (oh - (int)(imgH * aspect)) / 2;
            int left = (ow - (int)(imgW * aspect)) / 2;

            imlib_blend_image_onto_image(buffer, 0,
                                         0, 0, imgW, imgH,
                                         ox + left, oy + top,
                                         (int)(imgW * aspect),
                                         (int)(imgH * aspect));
        }
    } else {
        /* single screen fallback */
        imlib_context_set_cliprect(0, 0, width, height);

        double aspect = (double)width / imgW;
        if ((int)(imgH * aspect) < height)
            aspect = (double)height / imgH;

        int top  = (height - (int)(imgH * aspect)) / 2;
        int left = (width  - (int)(imgW * aspect)) / 2;

        imlib_blend_image_onto_image(buffer, 0,
                                     0, 0, imgW, imgH,
                                     left, top,
                                     (int)(imgW * aspect),
                                     (int)(imgH * aspect));
    }

    /* render canvas to pixmap */
    imlib_render_image_on_drawable(0, 0);

    /* push to X */
    set_root_atoms(dpy, scr, pixmap);
    XSetCloseDownMode(dpy, RetainTemporary);
    XSetWindowBackgroundPixmap(dpy, root, pixmap);
    XClearWindow(dpy, root);
    XFlush(dpy);
    XSync(dpy, False);

    /* save for next call to clean up */
    last_pixmap = pixmap;

    /* cleanup imlib */
    imlib_context_set_image(buffer);
    imlib_free_image();
    imlib_context_set_image(canvas);
    imlib_free_image();
    imlib_flush_loaders();
    imlib_context_pop();
    imlib_context_free(ctx);

    if (outputs) XFree(outputs);

    /* close throwaway connection — pixmap survives via RetainTemporary */
    XCloseDisplay(dpy);
}

