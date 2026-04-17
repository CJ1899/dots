#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <libnotify/notify.h>

#define CARD "hw:1" // T440p Analog PCH

/* Forward Declarations */
void notify(long vol, int sw);
void set_alsa_state(const char* selem_name, int op, int val);
void get_alsa_state(const char* selem_name, long *vol, int *mute);

static int parse_int(const char *str) {
    char *endptr;
    long val = strtol(str, &endptr, 10);
    if (endptr == str || (*endptr != '\0' && *endptr != '\n')) return 0;
    return (int)val;
}

void notify(long vol, int sw) {
    char msg[32];
    const char *icon = "audio-volume-high";

    /* sw == 0 means muted in ALSA playback switch */
    if (sw == 0 || vol == 0) {
        icon = "audio-volume-muted";
        snprintf(msg, sizeof(msg), "Muted");
    } else {
        snprintf(msg, sizeof(msg), "Volume: %ld%%", vol);
        if (vol < 30) icon = "audio-volume-low";
        else if (vol < 70) icon = "audio-volume-medium";
    }

    NotifyNotification *n = notify_notification_new("Audio", msg, icon);
    if (!n) return;

    notify_notification_set_hint(n, "value", g_variant_new_int32(vol));
    /* x-canonical-private-synchronous replaces previous notification instead of stacking */
    notify_notification_set_hint(n, "x-canonical-private-synchronous", g_variant_new_string("vol"));

    notify_notification_show(n, NULL);
    g_object_unref(G_OBJECT(n));
}

void get_alsa_state(const char* selem_name, long *vol, int *mute) {
    snd_mixer_t *handle = NULL;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t* elem = NULL;

    if (snd_mixer_open(&handle, 0) < 0) return;
    if (snd_mixer_attach(handle, CARD) < 0 ||
        snd_mixer_selem_register(handle, NULL, NULL) < 0 ||
        snd_mixer_load(handle) < 0) {
        goto cleanup;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, selem_name);
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) goto cleanup;

    long min, max, raw_vol;
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &raw_vol);

    if (strcmp(selem_name, "Master") == 0)
        snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, mute);
    else
        snd_mixer_selem_get_capture_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, mute);

    *vol = (long)(((float)raw_vol / (float)max) * 100.0f + 0.5f);

cleanup:
    if (handle) snd_mixer_close(handle);
}

void set_alsa_state(const char* selem_name, int op, int val) {
    snd_mixer_t *handle = NULL;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t* elem = NULL;

    if (snd_mixer_open(&handle, 0) < 0) return;
    if (snd_mixer_attach(handle, CARD) < 0 ||
        snd_mixer_selem_register(handle, NULL, NULL) < 0 ||
        snd_mixer_load(handle) < 0) {
        goto cleanup;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, selem_name);
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) goto cleanup;

    long min, max, volume;
    int sw;
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);

    if (strcmp(selem_name, "Master") == 0)
        snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &sw);
    else
        snd_mixer_selem_get_capture_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &sw);

    if (op == 'm') {
        int new_sw = !sw;
        if (strcmp(selem_name, "Master") == 0)
            snd_mixer_selem_set_playback_switch_all(elem, new_sw);
        else
            snd_mixer_selem_set_capture_switch_all(elem, new_sw);
        sw = new_sw;
    } else {
        int target = (int)(((float)volume / (float)max) * 100.0f + 0.5f);
        if (op == 's') target = val;
        else if (op == 'i') target += val;
        else if (op == 'd') target -= val;

        if (target > 100) target = 100;
        if (target < 0) target = 0;

        long raw_target = (long)(((float)target / 100.0f) * (float)max);
        snd_mixer_selem_set_playback_volume_all(elem, raw_target);
        snd_mixer_selem_set_playback_switch_all(elem, 1);
        sw = 1;
        volume = raw_target;
    }

    if (strcmp(selem_name, "Master") == 0) {
        long final_vol = (long)(((float)volume / (float)max) * 100.0f + 0.5f);
        notify(final_vol, sw);
    }

cleanup:
    if (handle) snd_mixer_close(handle);
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"bar",     no_argument,       0, 'b'},
        {"set",     required_argument, 0, 's'},
        {"inc",     required_argument, 0, 'i'},
        {"dec",     required_argument, 0, 'd'},
        {"mute",    no_argument,       0, 'm'},
        {"micmute", no_argument,       0, 'c'},
        {"MUTE",    no_argument,       0, 'M'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    if (argc == 1) {
        printf("Try 'vol --help'\n");
        return 1;
    }

    notify_init("vol-control");

    int c, opt_idx = 0;
    while ((c = getopt_long(argc, argv, "bs:i:d:mcMh", long_options, &opt_idx)) != -1) {
        switch (c) {
            case 'b': {
                long v; int m;
                get_alsa_state("Master", &v, &m);
                /* m == 0 is muted in ALSA */
                printf("%s %ld%%\n", (m ? (v > 70 ? "🔊" : v > 30 ? "🔉" : "🔈") : "🔇"), v);
                break;
            }
            case 's': set_alsa_state("Master", 's', parse_int(optarg)); break;
            case 'i': set_alsa_state("Master", 'i', parse_int(optarg)); break;
            case 'd': set_alsa_state("Master", 'd', parse_int(optarg)); break;
            case 'm': set_alsa_state("Master", 'm', 0); break;
            case 'c': set_alsa_state("Capture", 'm', 0); break;
            case 'M':
                set_alsa_state("Master", 'm', 0);
                set_alsa_state("Capture", 'm', 0);
                break;
            case 'h':
                printf("Usage: vol [OPTIONS]\n"
                       "  -b, --bar       Output for status bar\n"
                       "  -s, --set N     Set volume to N%%\n"
                       "  -i, --inc N     Increase volume by N%%\n"
                       "  -d, --dec N     Decrease volume by N%%\n"
                       "  -m, --mute      Toggle Speaker mute\n"
                       "  -c, --micmute   Toggle Microphone mute\n"
                       "  -M, --MUTE      Mute BOTH Speakers and Mic\n");
                break;
        }
    }

    notify_uninit();
    return 0;
}

