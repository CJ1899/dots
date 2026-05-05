#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../util.h"

const char *
get_wall_info(void)
{
    static char buf[256];
    char raw[128];
    FILE *fp;
    char *folder, *index;

    memset(buf, 0, sizeof(buf));
    memset(raw, 0, sizeof(raw));

    fp = popen("XDG_RUNTIME_DIR=/run/user/$(id -u) wallman -i 2>/dev/null", "r");

    if (!fp) return "^c#4C566A^---^d^";

    if (fgets(raw, sizeof(raw), fp) == NULL) {
        pclose(fp);
        return "^c#4C566A^---^d^";
    }
    pclose(fp);

    raw[strcspn(raw, "\n")] = '\0';

    folder = raw;
    index = strstr(raw, " | ");

    if (index) {
        *index = '\0';
        index += 3;
        snprintf(buf, sizeof(buf), "^c#88C0D0^🖼 %s ^c#CCCCCC^[%s]^d^", folder, index);
    } else if (strlen(raw) > 0) {
        snprintf(buf, sizeof(buf), "^c#88C0D0^🖼 ^c#FFFFFF^%s^d^", raw);
    } else {
        return "^c#4C566A^---^d^";
    }

    return buf;
}

