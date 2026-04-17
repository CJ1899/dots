#ifndef WALL_H
#define WALL_H


typedef union {
    int i;
    unsigned int ui;
    float f;
    const void *v;
} Arg;

void wall_cycle(const void *arg);
void wall_save(const void *arg);
void wall_restore(void);
void wall_random(const void *arg);
void wall_select(const void *arg);
void wall_reload(const void *arg);
void wall_folder_select(const void *arg);

#endif

