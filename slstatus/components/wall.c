#include <stdio.h>
#include <string.h>

const char *
get_wall_info(void)
{
    static char buf[256];
    FILE *f = fopen("/tmp/wall_info", "r");

    if (!f) return "---";

    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return "---";
    }

    fclose(f);
    buf[strcspn(buf, "\n")] = '\0'; // Strip newline
    return buf;
}

