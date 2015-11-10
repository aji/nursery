#include <stdio.h>
#include <stdint.h>
#include <limits.h>

#include "modular.h"
#include "kctl.h"

#define SAMPLE_RATE (48000)
#define OUTPUT_SCALE 32767

static void put_frame(int16_t L, int16_t R) {
	write(1, &L, 2);
	write(1, &R, 2);
}

static int bass[16] = {
	36, 36, 48, 36,
	36, 48, 36, 31,
	33, 33, 48, 33,
	33, 48, 33, 31,
};
static int seqs[2][4] = {
	{ 69, 72, 74, 76 },
	{ 72, 76, 79, 81 },
};
static int *seq = seqs[1];

int main(int argc, char *argv[]) {
	ModularContext mctx;
	Module *master;
	Module *output;
	int timer;

	srand(0);

	mctx.rate = SAMPLE_RATE;

	ModularInitialize(&mctx);
	master = ModularMaster(&mctx);
	output = ModularOutput(&mctx);

	Module *osc = NewModule(&mctx, &ModOscillator);
	Module *env = NewModule(&mctx, &ModADSR);

	OscillatorGet(osc)->waveform = OscBandlimitedSaw;

	*ADSRGet(env)->A = 0.006;
	*ADSRGet(env)->D = 0.700;
	*ADSRGet(env)->S = 0.000;
	*ADSRGet(env)->R = 0.100;
	*ADSRGet(env)->trig = 0.0;

	OscillatorGet(osc)->gain = env->out;
	AddDependency(&mctx, osc, env);

	Module *filt = NewModule(&mctx, &ModFilter);
	FilterSetInput(&mctx, filt, osc);

	Module *env2filt = NewModule(&mctx, &ModMatrix);
	MatrixSetInput(&mctx, env2filt, env);
	MatrixScale(env2filt, 0.0, 1.0, 10.0, 10000.0);
	FilterGet(filt)->cutoff = env2filt->out;
	AddDependency(&mctx, filt, env2filt);

	Module *env2reso = NewModule(&mctx, &ModMatrix);
	MatrixSetInput(&mctx, env2reso, env);
	MatrixScale(env2reso, 0.0, 1.0, 0.1, 2.0);
	FilterGet(filt)->reso = env2reso->out;
	AddDependency(&mctx, filt, env2reso);

	MixerSlot *slot = MixerAddSlot(&mctx, master, filt, 0.3, 0.0);

	KeyController *kc = NewMonoSynth();
	OscillatorGet(osc)->freq  = MonoSynthGet(kc)->freq;
	ADSRGet(env)->trig        = MonoSynthGet(kc)->trig;


	Module *bassosc = NewModule(&mctx, &ModOscillator);
	Module *bassenv = NewModule(&mctx, &ModADSR);

	OscillatorGet(bassosc)->waveform = OscBandlimitedSaw;

	*ADSRGet(bassenv)->A = 0.006;
	*ADSRGet(bassenv)->D = 0.200;
	*ADSRGet(bassenv)->S = 0.000;
	*ADSRGet(bassenv)->R = 0.100;
	*ADSRGet(bassenv)->trig = 0.0;

	//OscillatorGet(bassosc)->gain = bassenv->out;
	AddDependency(&mctx, bassosc, bassenv);

	Module *bassfilt = NewModule(&mctx, &ModFilter);
	FilterSetInput(&mctx, bassfilt, bassosc);

	Module *bassenv2filt = NewModule(&mctx, &ModMatrix);
	MatrixSetInput(&mctx, bassenv2filt, bassenv);
	MatrixScale(bassenv2filt, 0.0, 1.0, 50.0, 5000.0);
	FilterGet(bassfilt)->cutoff = bassenv2filt->out;
	AddDependency(&mctx, bassfilt, bassenv2filt);
	*FilterGet(bassfilt)->reso = 0.4;

	MixerSlot *bassslot = MixerAddSlot(&mctx, master, bassfilt, 0.3, 0.0);

	KeyController *bkc = NewMonoSynth();
	OscillatorGet(bassosc)->freq  = MonoSynthGet(bkc)->freq;
	ADSRGet(bassenv)->trig        = MonoSynthGet(bkc)->trig;


	for (timer=0; ; timer++) {
		ModularStep(&mctx);

		if (timer % 96000 == 84000)
			seq = seqs[((timer / 96000) % 2)];

		if (timer % 12000 == 0)
			KeyControllerKeyDown(kc, seq[(timer / 12000) % 4] - 12, 64);
		if (timer % 24000 == 22000)
			KeyControllerKeyUp(kc, seq[(timer / 12000) % 4] - 12);
		KeyControllerUpdate(kc);

		if (timer % 12000 == 0)
			KeyControllerKeyDown(bkc, bass[(timer / 12000) % 16], 64);
		if (timer % 12000 == 11000)
			KeyControllerKeyUp(bkc, bass[(timer / 12000) % 16]);
		KeyControllerUpdate(bkc);

		put_frame(output->out[0] * OUTPUT_SCALE,
		          output->out[1] * OUTPUT_SCALE);
	}

	return 0;
}
