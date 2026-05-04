#ifndef PROC_H
#define PROC_H

typedef struct {
    char *const command[10];
    int restart_delay;
} AppConfig;

extern const AppConfig apps[];

void spawn_apps(void);
void cleanup_apps(void);

#endif

