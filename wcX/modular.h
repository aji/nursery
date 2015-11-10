/* modular synthesizer */

#ifndef __INC_MODULAR_H__
#define __INC_MODULAR_H__

#include <stdbool.h>

typedef double                 Signal;

typedef struct ModularContext  ModularContext;

typedef struct Module          Module;
typedef struct ModuleSpec      ModuleSpec;

typedef struct Filter          Filter;
typedef struct Matrix          Matrix;
typedef struct ADSR            ADSR;
typedef struct Oscillator      Oscillator;
typedef struct MixerSlot       MixerSlot;

#include "container.h"

struct ModularContext {
	unsigned        rate;
};

struct Module {
	/* The corresponding module_spec_t for this particular module. The
	   module_spec_t includes pointers to all the functions and data
	   needed to run the given module */
	ModuleSpec     *spec;

	void           *user;

	/* A vector of pointers to module_t, representing the dependencies
	   of this module. In a single update, the dependencies will be
	   updated appropriately. For this reason, circular dependencies
	   are not allowed, however there are ways around it. */
	Vector          deps;

	unsigned        nout;
	Signal         *out;

	Module         *next;
	unsigned        step;
};

struct ModuleSpec {
	char           *name;

	unsigned        nout;

	void          (*initialize)  (ModularContext*, Module*);
	void          (*update)      (ModularContext*, Module*);
};

/* System-provided Modules */

extern ModuleSpec       ModDelay;

extern ModuleSpec       ModFilter;
struct Filter {
	enum {
		FILTER_LOWPASS_ONE,
		FILTER_HIGHPASS_ONE,
		FILTER_LOWPASS_TWO,
		FILTER_HIGHPASS_TWO,
	} type;

	Signal         *cutoff;
	Signal         *reso;
};
extern Filter          *FilterGet(Module*);
extern void             FilterSetInput(ModularContext*, Module*, Module*);

extern ModuleSpec       ModMatrix;
struct Matrix {
	/*  [ LL  RL  Loffs ] [ Lin ]     [ Lout ]
	    [ RL  RR  Roffs ] [ Rin ]  =  [ Rout ]
	                      [ 1   ]               */
	Signal         *Lin, *Rin;
	Signal         *LL, *LR, *RL, *RR;
	Signal         *Loffs, *Roffs;
};
extern Matrix          *MatrixGet(Module*);
extern void             MatrixSetInput (ModularContext*, Module*, Module*);
extern void             MatrixIdentity (Module*);
extern void             MatrixMSSplit  (Module*);
extern void             MatrixMSJoin   (Module*);
extern void             MatrixGainPan  (Module*, double gain, double pan);
extern void             MatrixScale    (Module*, double a1, double b1,
                                                 double a2, double b2);

extern ModuleSpec       ModADSR;
struct ADSR {
	Signal         *trig;
	Signal         *A, *D, *S, *R; /* A, D, R = seconds */
	Signal        (*shape)       (ADSR*, Signal time);
};
extern ADSR            *ADSRGet(Module*);
extern Signal           ADSRLinear   (ADSR*, Signal);
extern Signal           ADSRExp      (ADSR*, Signal);

extern ModuleSpec       ModOscillator;
struct Oscillator {
	Signal         *freq;
	Signal         *gain;
	Signal        (*waveform)            (Oscillator*, Signal phase);
};
extern Oscillator      *OscillatorGet        (Module*);
extern Signal           OscSine              (Oscillator*, Signal);
extern Signal           OscTriangle          (Oscillator*, Signal);
extern Signal           OscSquare            (Oscillator*, Signal);
extern Signal           OscSawtooth          (Oscillator*, Signal);
extern Signal           OscBandlimitedSquare (Oscillator*, Signal);
extern Signal           OscBandlimitedSaw    (Oscillator*, Signal);

extern ModuleSpec       ModMixer;
struct MixerSlot {
	Signal         *in[2];
	Signal         *gain, *pan;
};
extern MixerSlot       *MixerAddSlot(ModularContext*, Module*,
                                     Module *in, Signal gain, Signal pan);

extern ModuleSpec       ModLimiter;
extern Signal         **LimiterLimit(Module*);
extern void             LimiterSetInput(ModularContext*, Module*, Module*);

/* Module Manager */

extern Signal          *NewSignal(double init);

extern void             ModularInitialize(ModularContext*);
extern Module          *ModularMaster(ModularContext*);
extern Module          *ModularOutput(ModularContext*);
extern void             ModularStep(ModularContext*);

extern Module          *NewModule(ModularContext*, ModuleSpec*);
extern void             AddDependency(ModularContext*, Module*, Module *on);

#endif
