/* modular synthesizer */

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include "common.h"
#include "container.h"
#include "modular.h"

/*  FILTER MODULE                                                           */
/*  ======================================================================  */

/* Based on the filter implementation in the Calf LADSPA plugins,
   which is Copyright (C) 2001-2007 Krzysztof Foltman */

struct FilterPrivate {
	Filter f;

	Signal *in[2];

	double cutoff, reso;
	double Lx1, Lx2, Ly1, Ly2;
	double Rx1, Rx2, Ry1, Ry2;
	double a0, a1, b1, a2, b2;
};

Filter *FilterGet(Module *m) {
	return &((struct FilterPrivate*)m->user)->f;
}

void FilterSetInput(ModularContext *ctx, Module *m, Module *in) {
	struct FilterPrivate *priv = m->user;

	priv->in[0] = in->out;
	priv->in[1] = in->out;
	if (in->nout == 2) {
		priv->in[1] = in->out + 1;
	} else if (in->nout != 1) {
		Abort("filter can only take mono or stereo inputs");
	}

	AddDependency(ctx, m, in);
}

static void FilterInitialize(ModularContext *ctx, Module *m) {
	struct FilterPrivate *priv = calloc(1, sizeof(*priv));
	m->user = priv;
	priv->f.type = FILTER_LOWPASS_TWO;
	priv->f.cutoff = NewSignal(1000.0);
	priv->f.reso = NewSignal(1.5);
	priv->cutoff = 0.0;
}

static void FilterUpdate(ModularContext *ctx, Module *m) {
	struct FilterPrivate *filt = m->user;

	if (filt->cutoff != *filt->f.cutoff || filt->reso != *filt->f.reso) {
		double x, q;
		double omega, sn, cs, alpha, inv;

		filt->cutoff = *filt->f.cutoff;
		filt->reso   = *filt->f.reso;

		x = tan(M_PI * filt->cutoff / (2.0 * ctx->rate));
		q = 1.0 / (1.0 + x);

		omega = 2 * M_PI * filt->cutoff / ctx->rate;
		sn = sin(omega);
		cs = cos(omega);
		alpha = sn / (2 * filt->reso);
		inv = 1.0 / (1.0 + alpha);

		switch (filt->f.type) {
		case FILTER_LOWPASS_ONE:
			filt->a0 = x * q;
			filt->a1 = filt->a0;
			filt->b1 = (x - 1) * q;
			filt->a2 = 0;
			filt->b2 = 0;
			break;

		case FILTER_HIGHPASS_ONE:
			filt->a0 = q;
			filt->a1 = -filt->a0;
			filt->b1 = (x - 1) * q;
			filt->a2 = 0;
			filt->b2 = 0;
			break;

		case FILTER_LOWPASS_TWO:
			filt->a2 = filt->a0 = inv * (1 - cs) * 0.5;
			filt->a1 = filt->a0 + filt->a0;
			filt->b1 = -2 * cs * inv;
			filt->b2 = (1 - alpha) * inv;
			break;

		case FILTER_HIGHPASS_TWO:

			filt->a0 = inv * (1 + cs) / 2;
			filt->a1 = -2 * filt->a0;
			filt->a2 = filt->a0;
			filt->b1 = -2 * cs * inv;
			filt->b2 = (1 - alpha) * inv;
			break;
		}
	}

	m->out[0] = *filt->in[0] * filt->a0
	             + filt->Lx1 * filt->a1
	             + filt->Lx2 * filt->a2
	             - filt->Ly1 * filt->b1
	             - filt->Ly2 * filt->b2;
	m->out[1] = *filt->in[1] * filt->a0
	             + filt->Rx1 * filt->a1
	             + filt->Rx2 * filt->a2
	             - filt->Ry1 * filt->b1
	             - filt->Ry2 * filt->b2;

	filt->Lx2 = filt->Lx1;
	filt->Ly2 = filt->Ly1;
	filt->Rx2 = filt->Rx1;
	filt->Ry2 = filt->Ry1;
	filt->Lx1 = *filt->in[0];
	filt->Rx1 = *filt->in[1];
	filt->Ly1 = m->out[0];
	filt->Ry1 = m->out[1];
}

ModuleSpec ModFilter = {
	.name         = "Filter",
	.nout         = 2,
	.initialize   = FilterInitialize,
	.update       = FilterUpdate,
};

/*  MATRIX MODULE                                                           */
/*  ======================================================================  */

struct MatrixPrivate {
	Matrix M;
};

Matrix *MatrixGet(Module *m) {
	return &((struct MatrixPrivate*)m->user)->M;
}

static void MatrixInitialize(ModularContext *ctx, Module *m) {
	struct MatrixPrivate *priv = calloc(1, sizeof(*priv));
	m->user = priv;
	priv->M.Lin = NewSignal(0.0);
	priv->M.Rin = NewSignal(0.0);
	priv->M.LL = NewSignal(0.0);
	priv->M.LR = NewSignal(0.0);
	priv->M.RL = NewSignal(0.0);
	priv->M.RR = NewSignal(0.0);
	priv->M.Loffs = NewSignal(0.0);
	priv->M.Roffs = NewSignal(0.0);
	MatrixIdentity(m);
}

static void MatrixUpdate(ModularContext *ctx, Module *m) {
	Matrix *M = MatrixGet(m);
	if (!M->Lin || !M->Rin) {
		m->out[0] = m->out[1] = 0.0;
		return;
	}
	m->out[0] = *M->LL * *M->Lin + *M->RL * *M->Rin + *M->Loffs;
	m->out[1] = *M->LR * *M->Lin + *M->RR * *M->Rin + *M->Roffs;
}

void MatrixSetInput(ModularContext *ctx, Module *m, Module *in) {
	Matrix *M = MatrixGet(m);

	M->Lin = in->out;
	M->Rin = in->out;
	if (in->nout == 2) {
		M->Rin = in->out + 1;
	} else if (in->nout != 1) {
		Abort("matrix can only take mono or stereo inputs");
	}

	AddDependency(ctx, m, in);
}

void MatrixIdentity(Module *m) {
	Matrix *M = MatrixGet(m);
	*M->LL =  1.0; *M->RL =  0.0; *M->Loffs = 0.0;
	*M->LR =  0.0; *M->RR =  1.0; *M->Roffs = 0.0;
}

void MatrixMSSplit(Module *m) {
	Matrix *M = MatrixGet(m);
	*M->LL =  0.5; *M->RL =  0.5; *M->Loffs = 0.0;
	*M->LR =  0.5; *M->RR = -0.5; *M->Roffs = 0.0;
}

void MatrixMSJoin(Module *m) {
	Matrix *M = MatrixGet(m);
	*M->LL =  1.0; *M->RL =  1.0; *M->Loffs = 0.0;
	*M->LR =  1.0; *M->RR = -1.0; *M->Roffs = 0.0;
}

void MatrixGainPan(Module *m, double gain, double pan) {
	Matrix *M = MatrixGet(m);
	*M->LL = gain * (1.0 - pan);
	*M->RR = gain * (1.0 + pan);
	*M->LR = *M->RL = *M->Loffs = *M->Roffs = 0.0;
}

void MatrixScale(Module *m, double a1, double b1, double a2, double b2) {
	Matrix *M = MatrixGet(m);
	*M->LL = *M->RR = (b2 - a2) / (b1 - a1);
	*M->Loffs = *M->Roffs = a2 - a1;
	*M->LR = *M->RL = 0.0;
}

ModuleSpec ModMatrix = {
	.name         = "Matrix",
	.nout         = 2,
	.initialize   = MatrixInitialize,
	.update       = MatrixUpdate,
};

/*  ADSR MODULE                                                             */
/*  ======================================================================  */

#define ADSR_TRIGGER 0.7

struct ADSRPrivate {
	ADSR env;
	Signal ptrig;
	bool attack;
	double amt;
	double rfrom;
};

ADSR *ADSRGet(Module *m) {
	struct ADSRPrivate *priv = m->user;
	return &priv->env;
}

static void ADSRInitialize(ModularContext *ctx, Module *m) {
	struct ADSRPrivate *priv = calloc(1, sizeof(*priv));
	m->user = priv;
	priv->env.trig = NewSignal(0.0);
	priv->env.shape = ADSRExp;
	priv->env.A = NewSignal(0.0);
	priv->env.D = NewSignal(0.0);
	priv->env.S = NewSignal(0.0);
	priv->env.R = NewSignal(0.0);
	priv->ptrig = 0.0;
	priv->attack = false;
	priv->amt = 0.0;
	priv->rfrom = 0.0;
}

static void ADSRUpdate(ModularContext *ctx, Module *m) {
	struct ADSRPrivate *priv = m->user;

	if (*priv->env.trig < ADSR_TRIGGER) {
		priv->amt -= priv->rfrom / (ctx->rate * *priv->env.R);
		if (priv->amt < 0)
			priv->amt = 0;
	} else {
		if (priv->ptrig < ADSR_TRIGGER && *priv->env.trig > ADSR_TRIGGER)
			priv->attack = true;

		if (priv->attack) {
			priv->amt += 1.0 / (ctx->rate * *priv->env.A);
			if (priv->amt > 1.0) {
				priv->amt = 1.0;
				priv->attack = false;
			}
		} else {
			priv->amt -= 1.0 / (ctx->rate * *priv->env.D);
			if (priv->amt < *priv->env.S)
				priv->amt = *priv->env.S;
		}

		priv->rfrom = priv->amt;
	}

	priv->ptrig = *priv->env.trig;
	m->out[0] = priv->env.shape(&priv->env, priv->amt);
}

Signal ADSRLinear(ADSR *env, Signal f) {
	return f;
}

Signal ADSRExp(ADSR *env, Signal f) {
	return (pow(10.0, f - 1.0) - 0.10) / 0.90;
}

ModuleSpec ModADSR = {
	.name         = "ADSR",
	.nout         = 1,
	.initialize   = ADSRInitialize,
	.update       = ADSRUpdate,
};

/*  OSCILLATOR MODULE                                                       */
/*  ======================================================================  */

struct OscillatorPrivate {
	Oscillator osc;
	double phase;
};

Oscillator *OscillatorGet(Module *m) {
	struct OscillatorPrivate *priv = m->user;
	return &priv->osc;
}

static void OscillatorInitialize(ModularContext *ctx, Module *m) {
	struct OscillatorPrivate *priv = calloc(1, sizeof(*priv));
	m->user = priv;
	priv->osc.freq = NewSignal(432.0);
	priv->osc.gain = NewSignal(1.0);
	priv->osc.waveform = OscSine;
	priv->phase = (rand() % 500) / 500.0;
}

static void OscillatorUpdate(ModularContext *ctx, Module *m) {
	struct OscillatorPrivate *priv = m->user;
	priv->phase = fmod(priv->phase + *priv->osc.freq / ctx->rate, 1.0);
	m->out[0] = priv->osc.waveform(&priv->osc, priv->phase) * *priv->osc.gain;
}

Signal OscSine(Oscillator *osc, Signal phase) {
	return sin(2 * M_PI * phase);
}

Signal OscTriangle(Oscillator *osc, Signal phase) {
	if (phase < 0.5)
		return  1.0 - 4.0 * phase;
	else
		return -3.0 + 4.0 * phase;
}

Signal OscSquare(Oscillator *osc, Signal phase) {
	if (phase < 0.5)
		return  1.0;
	else
		return -1.0;
}

Signal OscSawtooth(Oscillator *osc, Signal phase) {
	return -1.0 + 2.0 * phase;
}

Signal OscBandlimitedSquare(Oscillator *osc, Signal phase) {
	int i;
	double f;
	Signal out = 0.0;

	if (*osc->freq < 1)
		return OscSquare(osc, phase);

	for (i=1, f=*osc->freq; f < 24000; i+=2, f = *osc->freq * i)
		out += sin(2 * M_PI * phase * i) / i;

	return out;
}

Signal OscBandlimitedSaw(Oscillator *osc, Signal phase) {
	int i;
	double f;
	Signal out = 0.0;

	if (*osc->freq < 1)
		return OscSawtooth(osc, phase);

	for (i=1, f=*osc->freq; f < 24000; i++, f = *osc->freq * i)
		out += sin(2 * M_PI * phase * i) / i;

	return out;
}

ModuleSpec ModOscillator = {
	.name         = "Oscillator",
	.nout         = 1,
	.initialize   = OscillatorInitialize,
	.update       = OscillatorUpdate,
};

/*  MIXER MODULE                                                            */
/*  ======================================================================  */

struct MixerPrivate {
	Vector slots;
	Signal (*pan)(Signal);
};

MixerSlot *MixerAddSlot(ModularContext *ctx, Module *m,
                        Module *in, Signal gain, Signal pan) {
	struct MixerPrivate *priv = m->user;

	if (m->spec != &ModMixer)
		Abort("Called mixer slot add fn. on non-mixer!");

	MixerSlot *slot   = calloc(1, sizeof(*slot));
	slot->gain        = NewSignal(gain);
	slot->pan         = NewSignal(pan);

	slot->in[0] = in->out;
	slot->in[1] = in->out;
	if (in->nout == 2) {
		slot->in[1] = in->out + 1;
	} else if (in->nout != 1) {
		Abort("mixer can only take mono or stereo inputs");
	}

	Vec_Push(&priv->slots, slot);

	AddDependency(ctx, m, in);

	return slot;
}

static Signal MixerStandardPan(Signal x) {
	return x < 0.0 ? (1.0 - 0.2 * x) : (1.0 - x);
}

static void MixerInitialize(ModularContext *ctx, Module *m) {
	struct MixerPrivate *priv = calloc(1, sizeof(*priv));
	priv->pan  = MixerStandardPan;
	m->user = priv;
}

static void MixerUpdate(ModularContext *ctx, Module *m) {
	struct MixerPrivate *priv = m->user;
	MixerSlot **slot;

	m->out[0] = 0.0;
	m->out[1] = 0.0;

	VEC_EACH(&priv->slots, slot) {
		MixerSlot *s = *slot;

		m->out[0] += *s->in[0] * *s->gain * priv->pan( *s->pan);
		m->out[1] += *s->in[1] * *s->gain * priv->pan(-*s->pan);
	}
}

ModuleSpec ModMixer = {
	.name          = "Mixer",
	.nout          = 2,
	.initialize    = MixerInitialize,
	.update        = MixerUpdate,
};

/*  LIMITER MODULE                                                          */
/*  ======================================================================  */

struct LimiterPrivate {
	Signal *in[2];
	Signal *limit;
};

Signal **LimiterLimit(Module *m) {
	return &((struct LimiterPrivate*)m->user)->limit;
}

void LimiterSetInput(ModularContext *ctx, Module *m, Module *in) {
	struct LimiterPrivate *priv = m->user;

	priv->in[0] = in->out;
	priv->in[1] = in->out;
	if (in->nout == 2) {
		priv->in[1] = in->out + 1;
	} else if (in->nout != 1) {
		Abort("limiter can only take mono or stereo inputs");
	}

	AddDependency(ctx, m, in);
}

static void LimiterInitialize(ModularContext *ctx, Module *m) {
	struct LimiterPrivate *priv = calloc(1, sizeof(*priv));
	priv->limit = NewSignal(0.99);
	m->user = priv;
}

static void LimiterUpdate(ModularContext *ctx, Module *m) {
	struct LimiterPrivate *priv = m->user;
	int i;

	for (i=0; i<m->nout; i++) {
		m->out[i] = *priv->in[i];

		if (m->out[i] > *priv->limit)
			m->out[i] = *priv->limit;
		if (m->out[i] < -*priv->limit)
			m->out[i] = -*priv->limit;
	}
}

ModuleSpec ModLimiter = {
	.name          = "Limiter",
	.nout          = 2,
	.initialize    = LimiterInitialize,
	.update        = LimiterUpdate,
};

/*  MODULE MANAGER                                                          */
/*  ======================================================================  */

Signal *NewSignal(double init) {
	Signal *s = calloc(1, sizeof(*s));
	*s = init;
	return s;
}

Module  *modules_head  =  NULL;
Module  *modules_tail  =  NULL;
int      module_count  =  0;

unsigned this_step = 0;
Module *master = NULL;
Module *output = NULL;

void ModularInitialize(ModularContext *ctx) {
	master = NewModule(ctx, &ModMixer);
	output = NewModule(ctx, &ModLimiter);

	LimiterSetInput(ctx, output, master);
}

Module *ModularMaster(ModularContext *ctx) {
	return master;
}

Module *ModularOutput(ModularContext *ctx) {
	return output;
}

static void StepOne(ModularContext *ctx, Module *m, int depth) {
	Module **cur;

	if (m->step >= this_step)
		return;

	if (depth > module_count)
		Abort("Module cycle detected; exiting");

	VEC_EACH(&m->deps, cur)
		StepOne(ctx, *cur, depth + 1);

	m->spec->update(ctx, m);
	m->step = this_step;
}

void ModularStep(ModularContext *ctx) {
	this_step++;
	StepOne(ctx, output, 0);
}

Module *NewModule(ModularContext *ctx, ModuleSpec *spec) {
	Module *m = calloc(1, sizeof(*m));

	Log(DEBUG, "Creating %s module", spec->name);

	m->spec = spec;

	m->nout = spec->nout;
	m->out = calloc(m->nout, sizeof(Signal));

	if (modules_tail) {
		modules_tail->next = m;
	} else {
		modules_head = m;
	}

	m->spec->initialize(ctx, m);

	modules_tail = m;
	module_count++;

	return m;
}

void AddDependency(ModularContext *ctx, Module *m, Module *on) {
	Vec_Push(&m->deps, on);
}
