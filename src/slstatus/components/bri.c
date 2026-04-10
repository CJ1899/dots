#include <stdio.h>
#include <stdint.h>
#include "../slstatus.h"
#include "../util.h"

const char *
bri_perc(const char *unused)
{
    uintmax_t cur, max;
    int percent;

    /* T440p standard intel backlight paths */
    if (pscanf("/sys/class/backlight/intel_backlight/brightness", "%ju", &cur) != 1 ||
        pscanf("/sys/class/backlight/intel_backlight/max_brightness", "%ju", &max) != 1) {
        return "";
    }

    if (max == 0) return "0%";

    percent = (int)((float)cur / (float)max * 100.0f + 0.5f);

    /* Clean brightness icon */
    return bprintf("☀️ %d%%", percent);
}

