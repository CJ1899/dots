#include <stdio.h>
#include <alsa/asoundlib.h>
#include "../slstatus.h"
#include "../util.h"

/* Pointing directly to the T440p Analog PCH */
#define CARD "hw:1"

const char *
vol_perc(const char *unused)
{
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	long min, max, raw_vol, vol;
	int sw;
	const char *icon;

	if (snd_mixer_open(&handle, 0) < 0)
		return NULL;
	if (snd_mixer_attach(handle, CARD) < 0) {
		snd_mixer_close(handle);
		return NULL;
	}
	if (snd_mixer_selem_register(handle, NULL, NULL) < 0) {
		snd_mixer_close(handle);
		return NULL;
	}
	if (snd_mixer_load(handle) < 0) {
		snd_mixer_close(handle);
		return NULL;
	}

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_name(sid, "Master");
	elem = snd_mixer_find_selem(handle, sid);

	if (!elem) {
		snd_mixer_close(handle);
		return NULL;
	}

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &raw_vol);
	snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &sw);

	vol = (long)(((float)raw_vol / (float)max) * 100.0f + 0.5f);

	/* Icon logic based on your script */
	if (sw == 0)
		icon = "🔇";
	else if (vol > 70)
		icon = "🔊";
	else if (vol > 30)
		icon = "🔉";
	else
		icon = "🔈";

	snprintf(buf, sizeof(buf), "%s %ld%%", icon, vol);

	snd_mixer_close(handle);
	return buf;
}

