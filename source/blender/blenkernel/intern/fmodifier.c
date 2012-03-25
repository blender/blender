/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/fmodifier.c
 *  \ingroup bke
 */



#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"

#include "BLF_translation.h"

#include "BLI_blenlib.h"
#include "BLI_math.h" /* windows needs for M_PI */
#include "BLI_utildefines.h"

#include "BKE_fcurve.h"
#include "BKE_idprop.h"


#define SMALL -1.0e-10
#define SELECT 1

/* ******************************** F-Modifiers ********************************* */

/* Info ------------------------------- */

/* F-Modifiers are modifiers which operate on F-Curves. However, they can also be defined
 * on NLA-Strips to affect all of the F-Curves referenced by the NLA-Strip. 
 */

/* Template --------------------------- */

/* Each modifier defines a set of functions, which will be called at the appropriate
 * times. In addition to this, each modifier should have a type-info struct, where
 * its functions are attached for use. 
 */
 
/* Template for type-info data:
 *	- make a copy of this when creating new modifiers, and just change the functions
 *	  pointed to as necessary
 *	- although the naming of functions doesn't matter, it would help for code
 *	  readability, to follow the same naming convention as is presented here
 * 	- any functions that a constraint doesn't need to define, don't define
 *	  for such cases, just use NULL 
 *	- these should be defined after all the functions have been defined, so that
 * 	  forward-definitions/prototypes don't need to be used!
 *	- keep this copy #if-def'd so that future constraints can get based off this
 */
#if 0
static FModifierTypeInfo FMI_MODNAME = {
	FMODIFIER_TYPE_MODNAME, /* type */
	sizeof(FMod_ModName), /* size */
	FMI_TYPE_SOME_ACTION, /* action type */
	FMI_REQUIRES_SOME_REQUIREMENT, /* requirements */
	"Modifier Name", /* name */
	"FMod_ModName", /* struct name */
	fcm_modname_free, /* free data */
	fcm_modname_relink, /* relink data */
	fcm_modname_copy, /* copy data */
	fcm_modname_new_data, /* new data */
	fcm_modname_verify, /* verify */
	fcm_modname_time, /* evaluate time */
	fcm_modname_evaluate /* evaluate */
};
#endif

/* Generator F-Curve Modifier --------------------------- */

/* Generators available:
 * 	1) simple polynomial generator:
 *		- Exanded form - (y = C[0]*(x^(n)) + C[1]*(x^(n-1)) + ... + C[n])  
 *		- Factorized form - (y = (C[0][0]*x + C[0][1]) * (C[1][0]*x + C[1][1]) * ... * (C[n][0]*x + C[n][1]))
 */

static void fcm_generator_free (FModifier *fcm)
{
	FMod_Generator *data= (FMod_Generator *)fcm->data;
	
	/* free polynomial coefficients array */
	if (data->coefficients)
		MEM_freeN(data->coefficients);
}

static void fcm_generator_copy (FModifier *fcm, FModifier *src)
{
	FMod_Generator *gen= (FMod_Generator *)fcm->data;
	FMod_Generator *ogen= (FMod_Generator *)src->data;
	
	/* copy coefficients array? */
	if (ogen->coefficients)
		gen->coefficients= MEM_dupallocN(ogen->coefficients);
}

static void fcm_generator_new_data (void *mdata)
{
	FMod_Generator *data= (FMod_Generator *)mdata;
	float *cp;
	
	/* set default generator to be linear 0-1 (gradient = 1, y-offset = 0) */
	data->poly_order= 1;
	data->arraysize= 2;
	cp= data->coefficients= MEM_callocN(sizeof(float)*2, "FMod_Generator_Coefs");
	cp[0] = 0; // y-offset 
	cp[1] = 1; // gradient
}

static void fcm_generator_verify (FModifier *fcm)
{
	FMod_Generator *data= (FMod_Generator *)fcm->data;
	
	/* requirements depend on mode */
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* expanded polynomial expression */
		{
			/* arraysize needs to be order+1, so resize if not */
			if (data->arraysize != (data->poly_order+1)) {
				float *nc;
				
				/* make new coefficients array, and copy over as much data as can fit */
				nc= MEM_callocN(sizeof(float)*(data->poly_order+1), "FMod_Generator_Coefs");
				
				if (data->coefficients) {
					if ((int)data->arraysize > (data->poly_order+1))
						memcpy(nc, data->coefficients, sizeof(float)*(data->poly_order+1));
					else
						memcpy(nc, data->coefficients, sizeof(float)*data->arraysize);
						
					/* free the old data */
					MEM_freeN(data->coefficients);
				}	
				
				/* set the new data */
				data->coefficients= nc;
				data->arraysize= data->poly_order+1;
			}
		}
			break;
		
		case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* expanded polynomial expression */
		{
			/* arraysize needs to be 2*order, so resize if not */
			if (data->arraysize != (data->poly_order * 2)) {
				float *nc;
				
				/* make new coefficients array, and copy over as much data as can fit */
				nc= MEM_callocN(sizeof(float)*(data->poly_order*2), "FMod_Generator_Coefs");
				
				if (data->coefficients) {
					if (data->arraysize > (unsigned int)(data->poly_order * 2))
						memcpy(nc, data->coefficients, sizeof(float)*(data->poly_order * 2));
					else
						memcpy(nc, data->coefficients, sizeof(float)*data->arraysize);
						
					/* free the old data */
					MEM_freeN(data->coefficients);
				}	
				
				/* set the new data */
				data->coefficients= nc;
				data->arraysize= data->poly_order * 2;
			}
		}
			break;	
	}
}

static void fcm_generator_evaluate (FCurve *UNUSED(fcu), FModifier *fcm, float *cvalue, float evaltime)
{
	FMod_Generator *data= (FMod_Generator *)fcm->data;
	
	/* behavior depends on mode 
	 * NOTE: the data in its default state is fine too
	 */
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* expanded polynomial expression */
		{
			/* we overwrite cvalue with the sum of the polynomial */
			float *powers = MEM_callocN(sizeof(float)*data->arraysize, "Poly Powers");
			float value= 0.0f;
			unsigned int i;
			
			/* for each x^n, precalculate value based on previous one first... this should be 
			 * faster that calling pow() for each entry
			 */
			for (i=0; i < data->arraysize; i++) {
				/* first entry is x^0 = 1, otherwise, calculate based on previous */
				if (i)
					powers[i]= powers[i-1] * evaltime;
				else
					powers[0]= 1;
			}
			
			/* for each coefficient, add to value, which we'll write to *cvalue in one go */
			for (i=0; i < data->arraysize; i++)
				value += data->coefficients[i] * powers[i];
			
			/* only if something changed, write *cvalue in one go */
			if (data->poly_order) {
				if (data->flag & FCM_GENERATOR_ADDITIVE)
					*cvalue += value;
				else
					*cvalue= value;
			}
				
			/* cleanup */
			if (powers) 
				MEM_freeN(powers);
		}
			break;
			
		case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* Factorized polynomial */
		{
			float value= 1.0f, *cp=NULL;
			unsigned int i;
			
			/* for each coefficient pair, solve for that bracket before accumulating in value by multiplying */
			for (cp=data->coefficients, i=0; (cp) && (i < (unsigned int)data->poly_order); cp+=2, i++) 
				value *= (cp[0]*evaltime + cp[1]);
				
			/* only if something changed, write *cvalue in one go */
			if (data->poly_order) {
				if (data->flag & FCM_GENERATOR_ADDITIVE)
					*cvalue += value;
				else
					*cvalue= value;
			}
		}
			break;
	}
}

static FModifierTypeInfo FMI_GENERATOR = {
	FMODIFIER_TYPE_GENERATOR, /* type */
	sizeof(FMod_Generator), /* size */
	FMI_TYPE_GENERATE_CURVE, /* action type */
	FMI_REQUIRES_NOTHING, /* requirements */
	N_("Generator"), /* name */
	"FMod_Generator", /* struct name */
	fcm_generator_free, /* free data */
	fcm_generator_copy, /* copy data */
	fcm_generator_new_data, /* new data */
	fcm_generator_verify, /* verify */
	NULL, /* evaluate time */
	fcm_generator_evaluate /* evaluate */
};

/* Built-In Function Generator F-Curve Modifier --------------------------- */

/* This uses the general equation for equations:
 * 		y = amplitude * fn(phase_multiplier*x + phase_offset) + y_offset
 *
 * where amplitude, phase_multiplier/offset, y_offset are user-defined coefficients,
 * x is the evaluation 'time', and 'y' is the resultant value
 *
 * Functions available are
 *	sin, cos, tan, sinc (normalised sin), natural log, square root 
 */

static void fcm_fn_generator_new_data (void *mdata)
{
	FMod_FunctionGenerator *data= (FMod_FunctionGenerator *)mdata;
	
	/* set amplitude and phase multiplier to 1.0f so that something is generated */
	data->amplitude= 1.0f;
	data->phase_multiplier= 1.0f;
}

/* Unary 'normalised sine' function
 * 	y = sin(PI + x) / (PI * x),
 * except for x = 0 when y = 1.
 */
static double sinc (double x)
{
	if (fabs(x) < 0.0001)
		return 1.0;
	else
		return sin(M_PI * x) / (M_PI * x);
}

static void fcm_fn_generator_evaluate (FCurve *UNUSED(fcu), FModifier *fcm, float *cvalue, float evaltime)
{
	FMod_FunctionGenerator *data= (FMod_FunctionGenerator *)fcm->data;
	double arg= data->phase_multiplier*evaltime + data->phase_offset;
	double (*fn)(double v) = NULL;
	
	/* get function pointer to the func to use:
	 * WARNING: must perform special argument validation hereto guard against crashes  
	 */
	switch (data->type)
	{
		/* simple ones */			
		case FCM_GENERATOR_FN_SIN: /* sine wave */
			fn= sin;
			break;
		case FCM_GENERATOR_FN_COS: /* cosine wave */
			fn= cos;
			break;
		case FCM_GENERATOR_FN_SINC: /* normalised sine wave */
			fn= sinc;
			break;
			
		/* validation required */
		case FCM_GENERATOR_FN_TAN: /* tangent wave */
		{
			/* check that argument is not on one of the discontinuities (i.e. 90deg, 270 deg, etc) */
			if IS_EQ(fmod((arg - M_PI_2), M_PI), 0.0) {
				if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0)
					*cvalue = 0.0f; /* no value possible here */
			}
			else
				fn= tan;
		}
			break;
		case FCM_GENERATOR_FN_LN: /* natural log */
		{
			/* check that value is greater than 1? */
			if (arg > 1.0) {
				fn= log;
			}
			else {
				if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0)
					*cvalue = 0.0f; /* no value possible here */
			}
		}
			break;
		case FCM_GENERATOR_FN_SQRT: /* square root */
		{
			/* no negative numbers */
			if (arg > 0.0) {
				fn= sqrt;
			}
			else {
				if ((data->flag & FCM_GENERATOR_ADDITIVE) == 0)
					*cvalue = 0.0f; /* no value possible here */
			}
		}
			break;
		
		default:
			printf("Invalid Function-Generator for F-Modifier - %d \n", data->type);
	}
	
	/* execute function callback to set value if appropriate */
	if (fn) {
		float value= (float)(data->amplitude*(float)fn(arg) + data->value_offset);
		
		if (data->flag & FCM_GENERATOR_ADDITIVE)
			*cvalue += value;
		else
			*cvalue= value;
	}
}

static FModifierTypeInfo FMI_FN_GENERATOR = {
	FMODIFIER_TYPE_FN_GENERATOR, /* type */
	sizeof(FMod_FunctionGenerator), /* size */
	FMI_TYPE_GENERATE_CURVE, /* action type */
	FMI_REQUIRES_NOTHING, /* requirements */
	N_("Built-In Function"), /* name */
	"FMod_FunctionGenerator", /* struct name */
	NULL, /* free data */
	NULL, /* copy data */
	fcm_fn_generator_new_data, /* new data */
	NULL, /* verify */
	NULL, /* evaluate time */
	fcm_fn_generator_evaluate /* evaluate */
};

/* Envelope F-Curve Modifier --------------------------- */

static void fcm_envelope_free (FModifier *fcm)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm->data;
	
	/* free envelope data array */
	if (env->data)
		MEM_freeN(env->data);
}

static void fcm_envelope_copy (FModifier *fcm, FModifier *src)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm->data;
	FMod_Envelope *oenv= (FMod_Envelope *)src->data;
	
	/* copy envelope data array */
	if (oenv->data)
		env->data= MEM_dupallocN(oenv->data);
}

static void fcm_envelope_new_data (void *mdata)
{
	FMod_Envelope *env= (FMod_Envelope *)mdata;
	
	/* set default min/max ranges */
	env->min= -1.0f;
	env->max= 1.0f;
}

static void fcm_envelope_verify (FModifier *fcm)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm->data;
	
	/* if the are points, perform bubble-sort on them, as user may have changed the order */
	if (env->data) {
		// XXX todo...
	}
}

static void fcm_envelope_evaluate (FCurve *UNUSED(fcu), FModifier *fcm, float *cvalue, float evaltime)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm->data;
	FCM_EnvelopeData *fed, *prevfed, *lastfed;
	float min=0.0f, max=0.0f, fac=0.0f;
	int a;
	
	/* get pointers */
	if (env->data == NULL) return;
	prevfed= env->data;
	fed= prevfed + 1;
	lastfed= prevfed + (env->totvert-1);
	
	/* get min/max values for envelope at evaluation time (relative to mid-value) */
	if (prevfed->time >= evaltime) {
		/* before or on first sample, so just extend value */
		min= prevfed->min;
		max= prevfed->max;
	}
	else if (lastfed->time <= evaltime) {
		/* after or on last sample, so just extend value */
		min= lastfed->min;
		max= lastfed->max;
	}
	else {
		/* evaltime occurs somewhere between segments */
		// TODO: implement binary search for this to make it faster?
		for (a=0; prevfed && fed && (a < env->totvert-1); a++, prevfed=fed, fed++) {  
			/* evaltime occurs within the interval defined by these two envelope points */
			if ((prevfed->time <= evaltime) && (fed->time >= evaltime)) {
				float afac, bfac, diff;
				
				diff= fed->time - prevfed->time;
				afac= (evaltime - prevfed->time) / diff;
				bfac= (fed->time - evaltime) / diff;
				
				min= bfac*prevfed->min + afac*fed->min;
				max= bfac*prevfed->max + afac*fed->max;
				
				break;
			}
		}
	}
	
	/* adjust *cvalue 
	 *	- fac is the ratio of how the current y-value corresponds to the reference range
	 *	- thus, the new value is found by mapping the old range to the new!
	 */
	fac= (*cvalue - (env->midval + env->min)) / (env->max - env->min);
	*cvalue= min + fac*(max - min); 
}

static FModifierTypeInfo FMI_ENVELOPE = {
	FMODIFIER_TYPE_ENVELOPE, /* type */
	sizeof(FMod_Envelope), /* size */
	FMI_TYPE_REPLACE_VALUES, /* action type */
	0, /* requirements */
	N_("Envelope"), /* name */
	"FMod_Envelope", /* struct name */
	fcm_envelope_free, /* free data */
	fcm_envelope_copy, /* copy data */
	fcm_envelope_new_data, /* new data */
	fcm_envelope_verify, /* verify */
	NULL, /* evaluate time */
	fcm_envelope_evaluate /* evaluate */
};

/* Cycles F-Curve Modifier  --------------------------- */

/* This modifier changes evaltime to something that exists within the curve's frame-range, 
 * then re-evaluates modifier stack up to this point using the new time. This re-entrant behavior
 * is very likely to be more time-consuming than the original approach... (which was tightly integrated into
 * the calculation code...).
 *
 * NOTE: this needs to be at the start of the stack to be of use, as it needs to know the extents of the
 * keyframes/sample-data.
 *
 * Possible TODO - store length of cycle information that can be initialized from the extents of the
 * keyframes/sample-data, and adjusted as appropriate.
 */

/* temp data used during evaluation */
typedef struct tFCMED_Cycles {
	float cycyofs;		/* y-offset to apply */
} tFCMED_Cycles;
 
static void fcm_cycles_new_data (void *mdata)
{
	FMod_Cycles *data= (FMod_Cycles *)mdata;
	
	/* turn on cycles by default */
	data->before_mode= data->after_mode= FCM_EXTRAPOLATE_CYCLIC;
}

static float fcm_cycles_time (FCurve *fcu, FModifier *fcm, float UNUSED(cvalue), float evaltime)
{
	FMod_Cycles *data= (FMod_Cycles *)fcm->data;
	float prevkey[2], lastkey[2], cycyofs=0.0f;
	short side=0, mode=0;
	int cycles=0, ofs=0;
	
	/* check if modifier is first in stack, otherwise disable ourself... */
	// FIXME...
	if (fcm->prev) {
		fcm->flag |= FMODIFIER_FLAG_DISABLED;
		return evaltime;
	}
	
	/* calculate new evaltime due to cyclic interpolation */
	if (fcu && fcu->bezt) {
		BezTriple *prevbezt= fcu->bezt;
		BezTriple *lastbezt= prevbezt + fcu->totvert-1;
		
		prevkey[0]= prevbezt->vec[1][0];
		prevkey[1]= prevbezt->vec[1][1];
		
		lastkey[0]= lastbezt->vec[1][0];
		lastkey[1]= lastbezt->vec[1][1];
	}
	else if (fcu && fcu->fpt) {
		FPoint *prevfpt= fcu->fpt;
		FPoint *lastfpt= prevfpt + fcu->totvert-1;
		
		prevkey[0]= prevfpt->vec[0];
		prevkey[1]= prevfpt->vec[1];
		
		lastkey[0]= lastfpt->vec[0];
		lastkey[1]= lastfpt->vec[1];
	}
	else
		return evaltime;
		
	/* check if modifier will do anything
	 *	1) if in data range, definitely don't do anything
	 *	2) if before first frame or after last frame, make sure some cycling is in use
	 */
	if (evaltime < prevkey[0]) {
		if (data->before_mode) {
			side= -1;
			mode= data->before_mode;
			cycles= data->before_cycles;
			ofs= prevkey[0];
		}
	}
	else if (evaltime > lastkey[0]) {
		if (data->after_mode) {
			side= 1;
			mode= data->after_mode;
			cycles= data->after_cycles;
			ofs= lastkey[0];
		}
	}
	if ELEM(0, side, mode)
		return evaltime;
		
	/* find relative place within a cycle */
	{
		float cycdx=0, cycdy=0;
		float cycle= 0, cyct=0;
		
		/* calculate period and amplitude (total height) of a cycle */
		cycdx= lastkey[0] - prevkey[0];
		cycdy= lastkey[1] - prevkey[1];
		
		/* check if cycle is infinitely small, to be point of being impossible to use */
		if (cycdx == 0)
			return evaltime;
			
		/* calculate the 'number' of the cycle */
		cycle= ((float)side * (evaltime - ofs) / cycdx);
		
		/* calculate the time inside the cycle */
		cyct= fmod(evaltime - ofs, cycdx);
		
		/* check that cyclic is still enabled for the specified time */
		if (cycles == 0) {
			/* catch this case so that we don't exit when we have cycles=0
			 * as this indicates infinite cycles...
			 */
		}
		else if (cycle > cycles) {
			/* we are too far away from range to evaluate
			 * TODO: but we should still hold last value... 
			 */
			return evaltime;
		}
		
		/* check if 'cyclic extrapolation', and thus calculate y-offset for this cycle */
		if (mode == FCM_EXTRAPOLATE_CYCLIC_OFFSET) {
			if (side < 0)
				cycyofs = (float)floor((evaltime - ofs) / cycdx);
			else
				cycyofs = (float)ceil((evaltime - ofs) / cycdx);
			cycyofs *= cycdy;
		}
		
		/* special case for cycle start/end */
		if (cyct == 0.0f) {
			evaltime = (side == 1 ? lastkey[0] : prevkey[0]);
			
			if ((mode == FCM_EXTRAPOLATE_MIRROR) && ((int)cycle % 2))
				evaltime = (side == 1 ? prevkey[0] : lastkey[0]);
		}
		/* calculate where in the cycle we are (overwrite evaltime to reflect this) */
		else if ((mode == FCM_EXTRAPOLATE_MIRROR) && ((int)(cycle+1) % 2)) {
			/* when 'mirror' option is used and cycle number is odd, this cycle is played in reverse 
			 *	- for 'before' extrapolation, we need to flip in a different way, otherwise values past
			 *	  then end of the curve get referenced (result of fmod will be negative, and with different phase)
			 */
			if (side < 0)
				evaltime= prevkey[0] - cyct;
			else
				evaltime= lastkey[0] - cyct;
		}
		else {
			/* the cycle is played normally... */
			evaltime= prevkey[0] + cyct;
		}
		if (evaltime < prevkey[0]) evaltime += cycdx;
	}
	
	/* store temp data if needed */
	if (mode == FCM_EXTRAPOLATE_CYCLIC_OFFSET) {
		tFCMED_Cycles *edata;
		
		/* for now, this is just a float, but we could get more stuff... */
		fcm->edata= edata= MEM_callocN(sizeof(tFCMED_Cycles), "tFCMED_Cycles");
		edata->cycyofs= cycyofs;
	}
	
	/* return the new frame to evaluate */
	return evaltime;
}
 
static void fcm_cycles_evaluate (FCurve *UNUSED(fcu), FModifier *fcm, float *cvalue, float UNUSED(evaltime))
{
	tFCMED_Cycles *edata= (tFCMED_Cycles *)fcm->edata;
	
	/* use temp data */
	if (edata) {
		/* add cyclic offset - no need to check for now, otherwise the data wouldn't exist! */
		*cvalue += edata->cycyofs;
		
		/* free temp data */
		MEM_freeN(edata);
		fcm->edata= NULL;
	}
}

static FModifierTypeInfo FMI_CYCLES = {
	FMODIFIER_TYPE_CYCLES, /* type */
	sizeof(FMod_Cycles), /* size */
	FMI_TYPE_EXTRAPOLATION, /* action type */
	FMI_REQUIRES_ORIGINAL_DATA, /* requirements */
	N_("Cycles"), /* name */
	"FMod_Cycles", /* struct name */
	NULL, /* free data */
	NULL, /* copy data */
	fcm_cycles_new_data, /* new data */
	NULL /*fcm_cycles_verify*/, /* verify */
	fcm_cycles_time, /* evaluate time */
	fcm_cycles_evaluate /* evaluate */
};

/* Noise F-Curve Modifier  --------------------------- */

static void fcm_noise_new_data (void *mdata)
{
	FMod_Noise *data= (FMod_Noise *)mdata;
	
	/* defaults */
	data->size= 1.0f;
	data->strength= 1.0f;
	data->phase= 1.0f;
	data->depth = 0;
	data->modification = FCM_NOISE_MODIF_REPLACE;
}
 
static void fcm_noise_evaluate (FCurve *UNUSED(fcu), FModifier *fcm, float *cvalue, float evaltime)
{
	FMod_Noise *data= (FMod_Noise *)fcm->data;
	float noise;
	
	/* generate noise using good ol' Blender Noise
	 *	- 0.1 is passed as the 'z' value, otherwise evaluation fails for size = phase = 1
	 *	  with evaltime being an integer (which happens when evaluating on frame by frame basis)
	 */
	noise = BLI_turbulence(data->size, evaltime, data->phase, 0.1f, data->depth);
	
	/* combine the noise with existing motion data */
	switch (data->modification) {
		case FCM_NOISE_MODIF_ADD:
			*cvalue= *cvalue + noise * data->strength;
			break;
		case FCM_NOISE_MODIF_SUBTRACT:
			*cvalue= *cvalue - noise * data->strength;
			break;
		case FCM_NOISE_MODIF_MULTIPLY:
			*cvalue= *cvalue * noise * data->strength;
			break;
		case FCM_NOISE_MODIF_REPLACE:
		default:
			*cvalue= *cvalue + (noise - 0.5f) * data->strength;
			break;
	}
}

static FModifierTypeInfo FMI_NOISE = {
	FMODIFIER_TYPE_NOISE, /* type */
	sizeof(FMod_Noise), /* size */
	FMI_TYPE_REPLACE_VALUES, /* action type */
	0, /* requirements */
	N_("Noise"), /* name */
	"FMod_Noise", /* struct name */
	NULL, /* free data */
	NULL, /* copy data */
	fcm_noise_new_data, /* new data */
	NULL /*fcm_noise_verify*/, /* verify */
	NULL, /* evaluate time */
	fcm_noise_evaluate /* evaluate */
};

/* Filter F-Curve Modifier --------------------------- */

#if 0 // XXX not yet implemented 
static FModifierTypeInfo FMI_FILTER = {
	FMODIFIER_TYPE_FILTER, /* type */
	sizeof(FMod_Filter), /* size */
	FMI_TYPE_REPLACE_VALUES, /* action type */
	0, /* requirements */
	N_("Filter"), /* name */
	"FMod_Filter", /* struct name */
	NULL, /* free data */
	NULL, /* copy data */
	NULL, /* new data */
	NULL /*fcm_filter_verify*/, /* verify */
	NULL, /* evlauate time */
	fcm_filter_evaluate /* evaluate */
};
#endif // XXX not yet implemented


/* Python F-Curve Modifier --------------------------- */

static void fcm_python_free (FModifier *fcm)
{
	FMod_Python *data= (FMod_Python *)fcm->data;
	
	/* id-properties */
	IDP_FreeProperty(data->prop);
	MEM_freeN(data->prop);
}

static void fcm_python_new_data (void *mdata) 
{
	FMod_Python *data= (FMod_Python *)mdata;
	
	/* everything should be set correctly by calloc, except for the prop->type constant.*/
	data->prop = MEM_callocN(sizeof(IDProperty), "PyFModifierProps");
	data->prop->type = IDP_GROUP;
}

static void fcm_python_copy (FModifier *fcm, FModifier *src)
{
	FMod_Python *pymod = (FMod_Python *)fcm->data;
	FMod_Python *opymod = (FMod_Python *)src->data;
	
	pymod->prop = IDP_CopyProperty(opymod->prop);
}

static void fcm_python_evaluate (FCurve *UNUSED(fcu), FModifier *UNUSED(fcm), float *UNUSED(cvalue), float UNUSED(evaltime))
{
#ifdef WITH_PYTHON
	//FMod_Python *data= (FMod_Python *)fcm->data;
	
	/* FIXME... need to implement this modifier...
	 *	It will need it execute a script using the custom properties 
	 */
#endif /* WITH_PYTHON */
}

static FModifierTypeInfo FMI_PYTHON = {
	FMODIFIER_TYPE_PYTHON, /* type */
	sizeof(FMod_Python), /* size */
	FMI_TYPE_GENERATE_CURVE, /* action type */
	FMI_REQUIRES_RUNTIME_CHECK, /* requirements */
	N_("Python"), /* name */
	"FMod_Python", /* struct name */
	fcm_python_free, /* free data */
	fcm_python_copy, /* copy data */
	fcm_python_new_data, /* new data */
	NULL /*fcm_python_verify*/, /* verify */
	NULL /*fcm_python_time*/, /* evaluate time */
	fcm_python_evaluate /* evaluate */
};


/* Limits F-Curve Modifier --------------------------- */

static float fcm_limits_time (FCurve *UNUSED(fcu), FModifier *fcm, float UNUSED(cvalue), float evaltime)
{
	FMod_Limits *data= (FMod_Limits *)fcm->data;
	
	/* check for the time limits */
	if ((data->flag & FCM_LIMIT_XMIN) && (evaltime < data->rect.xmin))
		return data->rect.xmin;
	if ((data->flag & FCM_LIMIT_XMAX) && (evaltime > data->rect.xmax))
		return data->rect.xmax;
		
	/* modifier doesn't change time */
	return evaltime;
}

static void fcm_limits_evaluate (FCurve *UNUSED(fcu), FModifier *fcm, float *cvalue, float UNUSED(evaltime))
{
	FMod_Limits *data= (FMod_Limits *)fcm->data;
	
	/* value limits now */
	if ((data->flag & FCM_LIMIT_YMIN) && (*cvalue < data->rect.ymin))
		*cvalue= data->rect.ymin;
	if ((data->flag & FCM_LIMIT_YMAX) && (*cvalue > data->rect.ymax))
		*cvalue= data->rect.ymax;
}

static FModifierTypeInfo FMI_LIMITS = {
	FMODIFIER_TYPE_LIMITS, /* type */
	sizeof(FMod_Limits), /* size */
	FMI_TYPE_GENERATE_CURVE, /* action type */  /* XXX... err... */   
	FMI_REQUIRES_RUNTIME_CHECK, /* requirements */
	N_("Limits"), /* name */
	"FMod_Limits", /* struct name */
	NULL, /* free data */
	NULL, /* copy data */
	NULL, /* new data */
	NULL, /* verify */
	fcm_limits_time, /* evaluate time */
	fcm_limits_evaluate /* evaluate */
};

/* Stepped F-Curve Modifier --------------------------- */

static void fcm_stepped_new_data (void *mdata) 
{
	FMod_Stepped *data= (FMod_Stepped *)mdata;
	
	/* just need to set the step-size to 2-frames by default */
	// XXX: or would 5 be more normal?
	data->step_size = 2.0f;
}

static float fcm_stepped_time (FCurve *UNUSED(fcu), FModifier *fcm, float UNUSED(cvalue), float evaltime)
{
	FMod_Stepped *data= (FMod_Stepped *)fcm->data;
	int snapblock;
	
	/* check range clamping to see if we should alter the timing to achieve the desired results */
	if (data->flag & FCM_STEPPED_NO_BEFORE) {
		if (evaltime < data->start_frame)
			return evaltime;
	}
	if (data->flag & FCM_STEPPED_NO_AFTER) {
		if (evaltime > data->end_frame)
			return evaltime;
	}
	
	/* we snap to the start of the previous closest block of 'step_size' frames 
	 * after the start offset has been discarded 
	 *	- i.e. round down
	 */
	snapblock = (int)((evaltime - data->offset) / data->step_size);
	
	/* reapply the offset, and multiple the snapblock by the size of the steps to get 
	 * the new time to evaluate at 
	 */
	return ((float)snapblock * data->step_size) + data->offset;
}

static FModifierTypeInfo FMI_STEPPED = {
	FMODIFIER_TYPE_STEPPED, /* type */
	sizeof(FMod_Limits), /* size */
	FMI_TYPE_GENERATE_CURVE, /* action type */  /* XXX... err... */   
	FMI_REQUIRES_RUNTIME_CHECK, /* requirements */
	N_("Stepped"), /* name */
	"FMod_Stepped", /* struct name */
	NULL, /* free data */
	NULL, /* copy data */
	fcm_stepped_new_data, /* new data */
	NULL, /* verify */
	fcm_stepped_time, /* evaluate time */
	NULL /* evaluate */
};

/* F-Curve Modifier API --------------------------- */
/* All of the F-Curve Modifier api functions use FModifierTypeInfo structs to carry out
 * and operations that involve F-Curve modifier specific code.
 */

/* These globals only ever get directly accessed in this file */
static FModifierTypeInfo *fmodifiersTypeInfo[FMODIFIER_NUM_TYPES];
static short FMI_INIT= 1; /* when non-zero, the list needs to be updated */

/* This function only gets called when FMI_INIT is non-zero */
static void fmods_init_typeinfo (void) 
{
	fmodifiersTypeInfo[0]=  NULL; 					/* 'Null' F-Curve Modifier */
	fmodifiersTypeInfo[1]=  &FMI_GENERATOR; 		/* Generator F-Curve Modifier */
	fmodifiersTypeInfo[2]=	&FMI_FN_GENERATOR;		/* Built-In Function Generator F-Curve Modifier */
	fmodifiersTypeInfo[3]=  &FMI_ENVELOPE;			/* Envelope F-Curve Modifier */
	fmodifiersTypeInfo[4]=  &FMI_CYCLES;			/* Cycles F-Curve Modifier */
	fmodifiersTypeInfo[5]=  &FMI_NOISE;				/* Apply-Noise F-Curve Modifier */
	fmodifiersTypeInfo[6]=  NULL/*&FMI_FILTER*/;			/* Filter F-Curve Modifier */  // XXX unimplemented
	fmodifiersTypeInfo[7]=  &FMI_PYTHON;			/* Custom Python F-Curve Modifier */
	fmodifiersTypeInfo[8]= 	&FMI_LIMITS;			/* Limits F-Curve Modifier */
	fmodifiersTypeInfo[9]= 	&FMI_STEPPED;			/* Stepped F-Curve Modifier */
}

/* This function should be used for getting the appropriate type-info when only
 * a F-Curve modifier type is known
 */
FModifierTypeInfo *get_fmodifier_typeinfo (int type)
{
	/* initialize the type-info list? */
	if (FMI_INIT) {
		fmods_init_typeinfo();
		FMI_INIT = 0;
	}
	
	/* only return for valid types */
	if ( (type >= FMODIFIER_TYPE_NULL) && 
		 (type <= FMODIFIER_NUM_TYPES ) ) 
	{
		/* there shouldn't be any segfaults here... */
		return fmodifiersTypeInfo[type];
	}
	else {
		printf("No valid F-Curve Modifier type-info data available. Type = %i \n", type);
	}
	
	return NULL;
} 
 
/* This function should always be used to get the appropriate type-info, as it
 * has checks which prevent segfaults in some weird cases.
 */
FModifierTypeInfo *fmodifier_get_typeinfo (FModifier *fcm)
{
	/* only return typeinfo for valid modifiers */
	if (fcm)
		return get_fmodifier_typeinfo(fcm->type);
	else
		return NULL;
}

/* API --------------------------- */

/* Add a new F-Curve Modifier to the given F-Curve of a certain type */
FModifier *add_fmodifier (ListBase *modifiers, int type)
{
	FModifierTypeInfo *fmi= get_fmodifier_typeinfo(type);
	FModifier *fcm;
	
	/* sanity checks */
	if ELEM(NULL, modifiers, fmi)
		return NULL;
	
	/* special checks for whether modifier can be added */
	if ((modifiers->first) && (type == FMODIFIER_TYPE_CYCLES)) {
		/* cycles modifier must be first in stack, so for now, don't add if it can't be */
		// TODO: perhaps there is some better way, but for now, 
		printf("Error: Cannot add 'Cycles' modifier to F-Curve, as 'Cycles' modifier can only be first in stack. \n");
		return NULL;
	}
	
	/* add modifier itself */
	fcm= MEM_callocN(sizeof(FModifier), "F-Curve Modifier");
	fcm->type = type;
	fcm->flag = FMODIFIER_FLAG_EXPANDED;
	fcm->influence = 1.0f;
	BLI_addtail(modifiers, fcm);
	
	/* tag modifier as "active" if no other modifiers exist in the stack yet */
	if (modifiers->first == modifiers->last)
		fcm->flag |= FMODIFIER_FLAG_ACTIVE;
	
	/* add modifier's data */
	fcm->data= MEM_callocN(fmi->size, fmi->structName);
	
	/* init custom settings if necessary */
	if (fmi->new_data)	
		fmi->new_data(fcm->data);
		
	/* return modifier for further editing */
	return fcm;
}

/* Make a copy of the specified F-Modifier */
FModifier *copy_fmodifier (FModifier *src)
{
	FModifierTypeInfo *fmi= fmodifier_get_typeinfo(src);
	FModifier *dst;
	
	/* sanity check */
	if (src == NULL)
		return NULL;
		
	/* copy the base data, clearing the links */
	dst = MEM_dupallocN(src);
	dst->next = dst->prev = NULL;
	
	/* make a new copy of the F-Modifier's data */
	dst->data = MEM_dupallocN(src->data);
	
	/* only do specific constraints if required */
	if (fmi && fmi->copy_data)
		fmi->copy_data(dst, src);
		
	/* return the new modifier */
	return dst;
}

/* Duplicate all of the F-Modifiers in the Modifier stacks */
void copy_fmodifiers (ListBase *dst, ListBase *src)
{
	FModifier *fcm, *srcfcm;
	
	if ELEM(NULL, dst, src)
		return;
	
	dst->first= dst->last= NULL;
	BLI_duplicatelist(dst, src);
	
	for (fcm=dst->first, srcfcm=src->first; fcm && srcfcm; srcfcm=srcfcm->next, fcm=fcm->next) {
		FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
		
		/* make a new copy of the F-Modifier's data */
		fcm->data = MEM_dupallocN(fcm->data);
		
		/* only do specific constraints if required */
		if (fmi && fmi->copy_data)
			fmi->copy_data(fcm, srcfcm);
	}
}

/* Remove and free the given F-Modifier from the given stack  */
int remove_fmodifier (ListBase *modifiers, FModifier *fcm)
{
	FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
	
	/* sanity check */
	if (fcm == NULL)
		return 0;
	
	/* free modifier's special data (stored inside fcm->data) */
	if (fcm->data) {
		if (fmi && fmi->free_data)
			fmi->free_data(fcm);
			
		/* free modifier's data (fcm->data) */
		MEM_freeN(fcm->data);
	}
	
	/* remove modifier from stack */
	if (modifiers) {
		BLI_freelinkN(modifiers, fcm);
		return 1;
	} 
	else {
		// XXX this case can probably be removed some day, as it shouldn't happen...
		printf("remove_fmodifier() - no modifier stack given \n");
		MEM_freeN(fcm);
		return 0;
	}
}

/* Remove all of a given F-Curve's modifiers */
void free_fmodifiers (ListBase *modifiers)
{
	FModifier *fcm, *fmn;
	
	/* sanity check */
	if (modifiers == NULL)
		return;
	
	/* free each modifier in order - modifier is unlinked from list and freed */
	for (fcm= modifiers->first; fcm; fcm= fmn) {
		fmn= fcm->next;
		remove_fmodifier(modifiers, fcm);
	}
}

/* Find the active F-Modifier */
FModifier *find_active_fmodifier (ListBase *modifiers)
{
	FModifier *fcm;
	
	/* sanity checks */
	if ELEM(NULL, modifiers, modifiers->first)
		return NULL;
	
	/* loop over modifiers until 'active' one is found */
	for (fcm= modifiers->first; fcm; fcm= fcm->next) {
		if (fcm->flag & FMODIFIER_FLAG_ACTIVE)
			return fcm;
	}
	
	/* no modifier is active */
	return NULL;
}

/* Set the active F-Modifier */
void set_active_fmodifier (ListBase *modifiers, FModifier *fcm)
{
	FModifier *fm;
	
	/* sanity checks */
	if ELEM(NULL, modifiers, modifiers->first)
		return;
	
	/* deactivate all, and set current one active */
	for (fm= modifiers->first; fm; fm= fm->next)
		fm->flag &= ~FMODIFIER_FLAG_ACTIVE;
	
	/* make given modifier active */
	if (fcm)
		fcm->flag |= FMODIFIER_FLAG_ACTIVE;
}

/* Do we have any modifiers which match certain criteria 
 *	- mtype - type of modifier (if 0, doesn't matter)
 *	- acttype - type of action to perform (if -1, doesn't matter)
 */
short list_has_suitable_fmodifier (ListBase *modifiers, int mtype, short acttype)
{
	FModifier *fcm;
	
	/* if there are no specific filtering criteria, just skip */
	if ((mtype == 0) && (acttype == 0))
		return (modifiers && modifiers->first);
		
	/* sanity checks */
	if ELEM(NULL, modifiers, modifiers->first)
		return 0;
		
	/* find the first mdifier fitting these criteria */
	for (fcm= modifiers->first; fcm; fcm= fcm->next) {
		FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
		short mOk=1, aOk=1; /* by default 1, so that when only one test, won't fail */
		
		/* check if applicable ones are fullfilled */
		if (mtype)
			mOk= (fcm->type == mtype);
		if (acttype > -1)
			aOk= (fmi->acttype == acttype);
			
		/* if both are ok, we've found a hit */
		if (mOk && aOk)
			return 1;
	}
	
	/* no matches */
	return 0;
}  

/* Evaluation API --------------------------- */

/* helper function - calculate influence of FModifier */
static float eval_fmodifier_influence (FModifier *fcm, float evaltime)
{
	float influence;
	
	/* sanity check */
	if (fcm == NULL) 
		return 0.0f;
	
	/* should we use influence stored in modifier or not 
	 * NOTE: this is really just a hack so that we don't need to version patch old files ;)
	 */
	if (fcm->flag & FMODIFIER_FLAG_USEINFLUENCE)
		influence = fcm->influence;
	else
		influence = 1.0f;
		
	/* restricted range or full range? */
	if (fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) {
		if ((evaltime <= fcm->sfra) || (evaltime >= fcm->efra)) {
			/* out of range */
			return 0.0f;
		}
		else if ((evaltime > fcm->sfra) && (evaltime < fcm->sfra + fcm->blendin)) {
			/* blend in range */
			float a = fcm->sfra;
			float b = fcm->sfra + fcm->blendin;
			return influence * (evaltime - a) / (b - a);
		}
		else if ((evaltime < fcm->efra) && (evaltime > fcm->efra - fcm->blendout)) {
			/* blend out range */
			float a = fcm->efra;
			float b = fcm->efra - fcm->blendout;
			return influence * (evaltime - a) / (b - a);
		}
	}
	
	/* just return the influence of the modifier */
	return influence;
}

/* evaluate time modifications imposed by some F-Curve Modifiers
 *	- this step acts as an optimization to prevent the F-Curve stack being evaluated 
 *	  several times by modifiers requesting the time be modified, as the final result
 *	  would have required using the modified time
 *	- modifiers only ever receive the unmodified time, as subsequent modifiers should be
 *	  working on the 'global' result of the modified curve, not some localised segment,
 *	  so nevaltime gets set to whatever the last time-modifying modifier likes...
 *	- we start from the end of the stack, as only the last one matters for now
 */
float evaluate_time_fmodifiers (ListBase *modifiers, FCurve *fcu, float cvalue, float evaltime)
{
	FModifier *fcm;
	
	/* sanity checks */
	if ELEM(NULL, modifiers, modifiers->last)
		return evaltime;
		
	/* Starting from the end of the stack, calculate the time effects of various stacked modifiers 
	 * on the time the F-Curve should be evaluated at. 
	 *
	 * This is done in reverse order to standard evaluation, as when this is done in standard
	 * order, each modifier would cause jumps to other points in the curve, forcing all
	 * previous ones to be evaluated again for them to be correct. However, if we did in the 
	 * reverse order as we have here, we can consider them a macro to micro type of waterfall
	 * effect, which should get us the desired effects when using layered time manipulations
	 * (such as multiple 'stepped' modifiers in sequence, causing different stepping rates)
	 */
	for (fcm= modifiers->last; fcm; fcm= fcm->prev) {
		FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
		
		if (fmi == NULL) 
			continue;
		
		/* if modifier cannot be applied on this frame (whatever scale it is on, it won't affect the results)
		 * hence we shouldn't bother seeing what it would do given the chance
		 */
		if ((fcm->flag & FMODIFIER_FLAG_RANGERESTRICT)==0 || 
			((fcm->sfra <= evaltime) && (fcm->efra >= evaltime)) )
		{
			/* only evaluate if there's a callback for this */
			if (fmi->evaluate_modifier_time) {
				if ((fcm->flag & (FMODIFIER_FLAG_DISABLED|FMODIFIER_FLAG_MUTED)) == 0) {
					float influence = eval_fmodifier_influence(fcm, evaltime);
					float nval = fmi->evaluate_modifier_time(fcu, fcm, cvalue, evaltime);
					
					evaltime = interpf(nval, evaltime, influence);
				}
			}
		}
	}
	
	/* return the modified evaltime */
	return evaltime;
}

/* Evalautes the given set of F-Curve Modifiers using the given data
 * Should only be called after evaluate_time_fmodifiers() has been called...
 */
void evaluate_value_fmodifiers (ListBase *modifiers, FCurve *fcu, float *cvalue, float evaltime)
{
	FModifier *fcm;
	
	/* sanity checks */
	if ELEM(NULL, modifiers, modifiers->first)
		return;
	
	/* evaluate modifiers */
	for (fcm= modifiers->first; fcm; fcm= fcm->next) {
		FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
		
		if (fmi == NULL) 
			continue;
		
		/* only evaluate if there's a callback for this, and if F-Modifier can be evaluated on this frame */
		if ((fcm->flag & FMODIFIER_FLAG_RANGERESTRICT)==0 || 
			((fcm->sfra <= evaltime) && (fcm->efra >= evaltime)) )
		{
			if (fmi->evaluate_modifier) {
				if ((fcm->flag & (FMODIFIER_FLAG_DISABLED|FMODIFIER_FLAG_MUTED)) == 0) {
					float influence = eval_fmodifier_influence(fcm, evaltime);
					float nval = *cvalue;
					
					fmi->evaluate_modifier(fcu, fcm, &nval, evaltime);
					*cvalue = interpf(nval, *cvalue, influence);
				}
			}
		}
	}
} 

/* ---------- */

/* Bake modifiers for given F-Curve to curve sample data, in the frame range defined
 * by start and end (inclusive).
 */
void fcurve_bake_modifiers (FCurve *fcu, int start, int end)
{
	ChannelDriver *driver;
	
	/* sanity checks */
	// TODO: make these tests report errors using reports not printf's
	if ELEM(NULL, fcu, fcu->modifiers.first) {
		printf("Error: No F-Curve with F-Curve Modifiers to Bake\n");
		return;
	}
	
	/* temporarily, disable driver while we sample, so that they don't influence the outcome */
	driver= fcu->driver;
	fcu->driver= NULL;
	
	/* bake the modifiers, by sampling the curve at each frame */
	fcurve_store_samples(fcu, NULL, start, end, fcurve_samplingcb_evalcurve);
	
	/* free the modifiers now */
	free_fmodifiers(&fcu->modifiers);
	
	/* restore driver */
	fcu->driver= driver;
}
