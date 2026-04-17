#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../util.h"

const char *
get_wall_info(void)
{
    static char buf[256];
    FILE *fp;

    /* * We force the env here because slstatus runs in a clean environment.
     * We use $(id -u) so it works regardless of who is logged in.
     */
    if (!(fp = popen("XDG_RUNTIME_DIR=/run/user/$(id -u) DISPLAY=:0 wallman -i 2>/dev/null", "r"))) {
        return "---";
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        pclose(fp);
        return "---";
    }
    pclose(fp);

    /* Clean up the newline */
    buf[strcspn(buf, "\n")] = '\0';


    return buf;
}

