#ifndef IPC_H
#define IPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


#define MAX_MSG 16

static void get_sock_path(char *dest, size_t len, const char *display) {
    char *runtime = getenv("XDG_RUNTIME_DIR");
    const char *base = runtime ? runtime : "/tmp";
    snprintf(dest, len, "%s/wl.%s.sock", base, display ? display : ":0");
}

#endif

