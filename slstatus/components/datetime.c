#include <stdio.h>
#include <time.h>
#include <string.h>
#include "../slstatus.h"
#include "../util.h"

const char *
datetime(const char *fmt)
{
    time_t t;
    struct tm *tm;
    char tmp[256];

    /* 12-hour clock icons */
    static const char *icons[] = {
        "🕛", "🕐", "🕑", "🕒", "🕓", "🕔",
        "🕕", "🕖", "🕗", "🕘", "🕙", "🕚"
    };

    t = time(NULL);
    tm = localtime(&t);
    if (!tm) return NULL;

    /* 1. Process the strftime flags from config.h first */
    if (!strftime(tmp, sizeof(tmp), fmt, tm)) {
        return NULL;
    }

    const char *icon = icons[tm->tm_hour % 12];

    /* 2. Find our custom placeholder 'CHR' and replace with emoji */
    /* We use 'CHR' because it's unlikely to be in your date format */
    char *pos = strstr(tmp, "CHR");
    if (pos) {
        size_t len_before = pos - tmp;
        snprintf(buf, sizeof(buf), "%.*s%s%s",
                 (int)len_before, tmp, icon, pos + 3);
    } else {
        strncpy(buf, tmp, sizeof(buf));
    }

    return buf;
}

