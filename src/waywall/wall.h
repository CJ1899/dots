#ifndef WALL_H
#define WALL_H


typedef union {
    int i;
    unsigned int ui;
    float f;
    const void *v;
} Arg;

extern char master_dir[1024];

const char *get_save_path(void);
extern char current_folder[256];
extern int count, cur;

void wall_cycle(const void *arg);
void wall_save(const void *arg);
void wall_restore(void);
void wall_random(const void *arg);
void wall_select(const void *arg);
void wall_reload(const void *arg);
void wall_folder_select(const void *arg);
void wall_apply(void);

#endif

