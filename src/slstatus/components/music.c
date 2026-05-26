#include <stdio.h>
#include <string.h>
#include <mpd/client.h>
#include "../util.h"

#define MAX_VISIBLE 30

static int
utf8_char_len(unsigned char c)
{
	if      (c < 0x80)           return 1;
	else if ((c & 0xe0) == 0xc0) return 2;
	else if ((c & 0xf0) == 0xe0) return 3;
	else if ((c & 0xf8) == 0xf0) return 4;
	else                         return 1;
}

static void
utf8_window_copy(char *dst, size_t dst_sz, const char *src, int offset)
{
	snprintf(dst, dst_sz, "%s", src + offset);
	size_t len = strlen(dst);
	if (len == 0)
		return;
	int i = (int)len - 1;
	while (i > 0 && ((unsigned char)dst[i] & 0xc0) == 0x80)
		i--;
	int clen = utf8_char_len((unsigned char)dst[i]);
	if (i + clen > (int)len)
		dst[i] = '\0';
}

const char *
music_status(const char *fmt)
{
	static char buf[256];
	static char window[MAX_VISIBLE + 1];
	static char last_track[256] = "";
	static int  offset          = 0;
	static struct mpd_connection *conn = NULL;

	/* 1. Persistent connection management */
	if (!conn)
		conn = mpd_connection_new(NULL, 0, 3000);

	/* 2. Error recovery */
	if (!conn || mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
		if (conn) { mpd_connection_free(conn); conn = NULL; }
		return "^c#BF616A^  ^d^";
	}

	struct mpd_status *status = mpd_run_status(conn);
	struct mpd_song   *song   = mpd_run_current_song(conn);

	/* 3. Cleanup and exit if stopped or no song */
	if (!status || !song || mpd_status_get_state(status) == MPD_STATE_STOP) {
		if (status) mpd_status_free(status);
		if (song)   mpd_song_free(song);
		if (!status && conn) { mpd_connection_free(conn); conn = NULL; }
		return "^c#BF616A^  ^d^";
	}

	/* 4. Metadata extraction and extension removal */
	const char *raw_title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
	if (!raw_title || raw_title[0] == '\0')
		raw_title = mpd_song_get_uri(song);

	char title[256];
	snprintf(title, sizeof(title), "%s", raw_title ? raw_title : "Unknown");
	char *dot = strrchr(title, '.');
	if (dot && dot != title) *dot = '\0';

	size_t len = strlen(title);

	/* 5. Track change detection; reset scroll offset */
	if (strncmp(last_track, title, sizeof(last_track) - 1) != 0) {
		offset = 0;
		snprintf(last_track, sizeof(last_track), "%s", title);
	}

	/* 6. Playback state icon */
	enum mpd_state state = mpd_status_get_state(status);
	const char *icon = (state == MPD_STATE_PLAY)
	                 ? "^c#A3BE8C^  ^d^"
	                 : "^c#81A1C1^  ^d^";

	/* 7. Elapsed/total time */
	unsigned int elapsed = mpd_status_get_elapsed_time(status);
	unsigned int total   = mpd_status_get_total_time(status);
	char timebuf[16] = "";
	if (total > 0)
		snprintf(timebuf, sizeof(timebuf), "%u:%02u/%u:%02u",
		         elapsed / 60, elapsed % 60,
		         total   / 60, total   % 60);

	/* 8. Scrolling window with UTF-8 boundary safety */
	if (len > MAX_VISIBLE) {
		int max_offset = (int)len - MAX_VISIBLE;
		if (offset < 0 || offset > max_offset)
			offset = 0;
		utf8_window_copy(window, sizeof(window), title, offset);
		if (state == MPD_STATE_PLAY) {
			int step = utf8_char_len((unsigned char)title[offset]);
			offset += step;
			if (offset > max_offset)
				offset = 0;
		}
	} else {
		snprintf(window, sizeof(window), "%s", title);
	}

	/* 9. Order-dependent output assembly */
	size_t pos = 0;
	buf[0] = '\0';

	for (const char *p = fmt ? fmt : "itn"; *p; p++) {
		switch (*p) {
		case 'i':
			pos += snprintf(buf + pos, sizeof(buf) - pos,
			                "%s%s", pos ? " " : "", icon);
			break;
		case 't':
			if (timebuf[0])
				pos += snprintf(buf + pos, sizeof(buf) - pos,
				                "%s^c#EBC88B^%s^d^", pos ? " " : "", timebuf);
			break;
		case 'n':
			pos += snprintf(buf + pos, sizeof(buf) - pos,
			                "%s^c#D8DEE9^%s^d^", pos ? " " : "", window);
			break;
		}
	}

	/* 10. Local resource cleanup */
	mpd_song_free(song);
	mpd_status_free(status);
	return buf;
}

