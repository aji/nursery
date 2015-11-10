/* key controller */

#ifndef __INC_KCTL_H__
#define __INC_KCTL_H__

typedef struct KeyController       KeyController;

typedef struct MonoSynth           MonoSynth;
typedef struct PolySynth           PolySynth;

#include "modular.h"

extern double MidiNoteTo440Freq(double);

struct KeyController {
	void          (*keydown) (KeyController*, int k, int v);
	void          (*keyup)   (KeyController*, int k);
	void          (*update)  (KeyController*);

	void           *priv;
};

static inline void KeyControllerKeyDown(KeyController *kc, int k, int v) {
	if (kc->keydown)
		kc->keydown(kc, k, v);
}

static inline void KeyControllerKeyUp(KeyController *kc, int k) {
	if (kc->keyup)
		kc->keyup(kc, k);
}

static inline void KeyControllerUpdate(KeyController *kc) {
	if (kc->update)
		kc->update(kc);
}

struct MonoSynth {
	/* outputs */
	Signal         *freq;
	Signal         *vel;
	Signal         *trig;

	/* parameters */
	Signal         *porta;
};

extern KeyController *NewMonoSynth(void);
extern MonoSynth *MonoSynthGet(KeyController*);

#endif
