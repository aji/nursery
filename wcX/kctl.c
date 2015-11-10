/* key controller */

#include <stdlib.h>
#include <math.h>

#include "kctl.h"
#include "modular.h"

/* change-of-base factor. e^(x*this) = 2^(x/12) */
#define MAGIC 0.05776226504666212
/* ln 440. e^(x+this) = 440 * e^(this) */
#define LOG_440 6.0867747269123065

double MidiNoteTo440Freq(double n) {
	return exp(LOG_440 + (n - 69.0) * MAGIC);
}

struct MonoSynthPrivate {
	MonoSynth      ms;
	double         target;
	double         alpha;
	int            key;
};

static void MonoSynthKeyDown(KeyController *kc, int k, int v) {
	struct MonoSynthPrivate *ms = kc->priv;

	*ms->ms.vel = v / 127.0;
	*ms->ms.trig = 1.0;
	ms->target = MidiNoteTo440Freq(k);

	if (ms->key == -1)
		*ms->ms.freq = ms->target;

	ms->key = k;
}

static void MonoSynthKeyUp(KeyController *kc, int k) {
	struct MonoSynthPrivate *ms = kc->priv;

	if (k != ms->key)
		return;

	ms->key = -1;
	*ms->ms.trig = 0.0;
}

static void MonoSynthUpdate(KeyController *kc) {
	struct MonoSynthPrivate *ms = kc->priv;

	ms->alpha = 0.9993; /* TODO: factor in ms->ms.porta */
	*ms->ms.freq = (*ms->ms.freq * ms->alpha) + ms->target * (1.0 - ms->alpha);
}

KeyController *NewMonoSynth(void) {
	KeyController *kc = calloc(1, sizeof(*kc));
	struct MonoSynthPrivate *ms = calloc(1, sizeof(*ms));

	kc->keydown     = MonoSynthKeyDown;
	kc->keyup       = MonoSynthKeyUp;
	kc->update      = MonoSynthUpdate;
	kc->priv        = ms;

	ms->ms.freq     = NewSignal(0.0);
	ms->ms.vel      = NewSignal(0.0);
	ms->ms.trig     = NewSignal(0.0);
	ms->ms.porta    = NewSignal(0.0);

	ms->key         = -1;

	return kc;
}

MonoSynth *MonoSynthGet(KeyController *kc) {
	return &((struct MonoSynthPrivate*)kc->priv)->ms;
}
