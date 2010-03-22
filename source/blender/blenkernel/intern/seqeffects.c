/**
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"
#include "PIL_dynlib.h"

#include "BLI_math.h" /* windows needs for M_PI */

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_anim_types.h"

#include "BKE_fcurve.h"
#include "BKE_plugin_types.h"
#include "BKE_sequencer.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RNA_access.h"

/* **** XXX **** */
static void error() {}

#define INT	96
#define FLO	128

/* **** XXX **** */

/* Glow effect */
enum {
	GlowR=0,
	GlowG=1,
	GlowB=2,
	GlowA=3
};


/* **********************************************************************
   PLUGINS
   ********************************************************************** */

static void open_plugin_seq(PluginSeq *pis, const char *seqname)
{
	int (*version)();
	void* (*alloc_private)();
	char *cp;

	/* to be sure: (is tested for) */
	pis->doit= 0;
	pis->pname= 0;
	pis->varstr= 0;
	pis->cfra= 0;
	pis->version= 0;
	pis->instance_private_data = 0;

	/* clear the error list */
	PIL_dynlib_get_error_as_string(NULL);

	/* if(pis->handle) PIL_dynlib_close(pis->handle); */
	/* pis->handle= 0; */

	/* open the needed object */
	pis->handle= PIL_dynlib_open(pis->name);
	if(test_dlerr(pis->name, pis->name)) return;

	if (pis->handle != 0) {
		/* find the address of the version function */
		version= (int (*)())PIL_dynlib_find_symbol(pis->handle, "plugin_seq_getversion");
		if (test_dlerr(pis->name, "plugin_seq_getversion")) return;

		if (version != 0) {
			pis->version= version();
			if (pis->version >= 2 && pis->version <= 6) {
				int (*info_func)(PluginInfo *);
				PluginInfo *info= (PluginInfo*) MEM_mallocN(sizeof(PluginInfo), "plugin_info");

				info_func= (int (*)(PluginInfo *))PIL_dynlib_find_symbol(pis->handle, "plugin_getinfo");

				if(info_func == NULL) error("No info func");
				else {
					info_func(info);

					pis->pname= info->name;
					pis->vars= info->nvars;
					pis->cfra= info->cfra;

					pis->varstr= info->varstr;

					pis->doit= (void(*)(void))info->seq_doit;
					if (info->init)
						info->init();
				}
				MEM_freeN(info);

				cp= PIL_dynlib_find_symbol(pis->handle, "seqname");
				if(cp) strncpy(cp, seqname, 21);
			} else {
				printf ("Plugin returned unrecognized version number\n");
				return;
			}
		}
		alloc_private = (void* (*)())PIL_dynlib_find_symbol(
			pis->handle, "plugin_seq_alloc_private_data");
		if (alloc_private) {
			pis->instance_private_data = alloc_private();
		}
		
		pis->current_private_data = (void**) 
			PIL_dynlib_find_symbol(
				pis->handle, "plugin_private_data");
	}
}

static PluginSeq *add_plugin_seq(const char *str, const char *seqname)
{
	PluginSeq *pis;
	VarStruct *varstr;
	int a;

	pis= MEM_callocN(sizeof(PluginSeq), "PluginSeq");

	strncpy(pis->name, str, FILE_MAXDIR+FILE_MAXFILE);
	open_plugin_seq(pis, seqname);

	if(pis->doit==0) {
		if(pis->handle==0) error("no plugin: %s", str);
		else error("in plugin: %s", str);
		MEM_freeN(pis);
		return 0;
	}

	/* default values */
	varstr= pis->varstr;
	for(a=0; a<pis->vars; a++, varstr++) {
		if( (varstr->type & FLO)==FLO)
			pis->data[a]= varstr->def;
		else if( (varstr->type & INT)==INT)
			*((int *)(pis->data+a))= (int) varstr->def;
	}

	return pis;
}

static void free_plugin_seq(PluginSeq *pis)
{
	if(pis==0) return;

	/* no PIL_dynlib_close: same plugin can be opened multiple times with 1 handle */

	if (pis->instance_private_data) {
		void (*free_private)(void *);

		free_private = (void (*)(void *))PIL_dynlib_find_symbol(
			pis->handle, "plugin_seq_free_private_data");
		if (free_private) {
			free_private(pis->instance_private_data);
		}
	}

	MEM_freeN(pis);
}

static void init_plugin(Sequence * seq, const char * fname)
{
	seq->plugin= (PluginSeq *)add_plugin_seq(fname, seq->name+2);
}

/* 
 * FIXME: should query plugin! Could be generator, that needs zero inputs...
 */
static int num_inputs_plugin()
{
	return 1;
}

static void load_plugin(Sequence * seq)
{
	if (seq) {
		open_plugin_seq(seq->plugin, seq->name+2);
	}
}

static void copy_plugin(Sequence * dst, Sequence * src)
{
	if(src->plugin) {
		dst->plugin= MEM_dupallocN(src->plugin);
		open_plugin_seq(dst->plugin, dst->name+2);
	}
}

static ImBuf * IMB_cast_away_list(ImBuf * i)
{
	if (!i) {
		return 0;
	}
	return (ImBuf*) (((void**) i) + 2);
}

static void do_plugin_effect(Scene *scene, Sequence *seq, int cfra,
				 float facf0, float facf1, int x, int y, 
				 struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
				 struct ImBuf *ibuf3, struct ImBuf *out)
{
	char *cp;
	int float_rendering;
	int use_temp_bufs = 0; /* Are needed since blur.c (and maybe some other
				  old plugins) do very bad stuff
				  with imbuf-internals */

	if(seq->plugin && seq->plugin->doit) {
		
		if(seq->plugin->cfra) 
			*(seq->plugin->cfra)= cfra;
		
		cp = PIL_dynlib_find_symbol(
			seq->plugin->handle, "seqname");

		if(cp) strncpy(cp, seq->name+2, 22);

		if (seq->plugin->current_private_data) {
			*seq->plugin->current_private_data 
				= seq->plugin->instance_private_data;
		}

		float_rendering = (out->rect_float != NULL);

		if (seq->plugin->version<=3 && float_rendering) {
			use_temp_bufs = 1;

			if (ibuf1) {
				ibuf1 = IMB_dupImBuf(ibuf1);
				IMB_rect_from_float(ibuf1);
				imb_freerectfloatImBuf(ibuf1);
				ibuf1->flags &= ~IB_rectfloat;
			}
			if (ibuf2) {
				ibuf2 = IMB_dupImBuf(ibuf2);
				IMB_rect_from_float(ibuf2);
				imb_freerectfloatImBuf(ibuf2);
				ibuf2->flags &= ~IB_rectfloat;
			} 
			if (ibuf3) {
				ibuf3 = IMB_dupImBuf(ibuf3);
				IMB_rect_from_float(ibuf3);
				imb_freerectfloatImBuf(ibuf3);
				ibuf3->flags &= ~IB_rectfloat;
			} 
			if (!out->rect) imb_addrectImBuf(out);
			imb_freerectfloatImBuf(out);
			out->flags &= ~IB_rectfloat;
		}

		if (seq->plugin->version<=2) {
			if(ibuf1) IMB_convert_rgba_to_abgr(ibuf1);
			if(ibuf2) IMB_convert_rgba_to_abgr(ibuf2);
			if(ibuf3) IMB_convert_rgba_to_abgr(ibuf3);
		}

		if (seq->plugin->version<=4) {
			((SeqDoit)seq->plugin->doit)(
				seq->plugin->data, facf0, facf1, x, y,
				IMB_cast_away_list(ibuf1), 
				IMB_cast_away_list(ibuf2), 
				IMB_cast_away_list(out), 
				IMB_cast_away_list(ibuf3));
		} else {
			((SeqDoit)seq->plugin->doit)(
				seq->plugin->data, facf0, facf1, x, y,
				ibuf1, ibuf2, out, ibuf3);
		}

		if (seq->plugin->version<=2) {
			if (!use_temp_bufs) {
				if(ibuf1) IMB_convert_rgba_to_abgr(ibuf1);
				if(ibuf2) IMB_convert_rgba_to_abgr(ibuf2);
				if(ibuf3) IMB_convert_rgba_to_abgr(ibuf3);
			}
			IMB_convert_rgba_to_abgr(out);
		}
		if (seq->plugin->version<=3 && float_rendering) {
			IMB_float_from_rect(out);
		}

		if (use_temp_bufs) {
			if (ibuf1) IMB_freeImBuf(ibuf1);
			if (ibuf2) IMB_freeImBuf(ibuf2);
			if (ibuf3) IMB_freeImBuf(ibuf3);
		}
	}
}

static int do_plugin_early_out(struct Sequence *seq,
				   float facf0, float facf1)
{
	return 0;
}

static void free_plugin(struct Sequence * seq)
{
	free_plugin_seq(seq->plugin);
	seq->plugin = 0;
}

/* **********************************************************************
   ALPHA OVER
   ********************************************************************** */

static void init_alpha_over_or_under(Sequence * seq)
{
	Sequence * seq1 = seq->seq1;
	Sequence * seq2 = seq->seq2;

	seq->seq2= seq1;
	seq->seq1= seq2;
}

static void do_alphaover_effect_byte(float facf0, float facf1, int x, int y, 
					 char * rect1, char *rect2, char *out)
{
	int fac2, mfac, fac, fac4;
	int xo, tempc;
	char *rt1, *rt2, *rt;

	xo= x;
	rt1= (char *)rect1;
	rt2= (char *)rect2;
	rt= (char *)out;

	fac2= (int)(256.0*facf0);
	fac4= (int)(256.0*facf1);

	while(y--) {

		x= xo;
		while(x--) {

			/* rt = rt1 over rt2  (alpha from rt1) */

			fac= fac2;
			mfac= 256 - ( (fac2*rt1[3])>>8 );

			if(fac==0) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
			else if(mfac==0) *( (unsigned int *)rt) = *( (unsigned int *)rt1);
			else {
				tempc= ( fac*rt1[0] + mfac*rt2[0])>>8;
				if(tempc>255) rt[0]= 255; else rt[0]= tempc;
				tempc= ( fac*rt1[1] + mfac*rt2[1])>>8;
				if(tempc>255) rt[1]= 255; else rt[1]= tempc;
				tempc= ( fac*rt1[2] + mfac*rt2[2])>>8;
				if(tempc>255) rt[2]= 255; else rt[2]= tempc;
				tempc= ( fac*rt1[3] + mfac*rt2[3])>>8;
				if(tempc>255) rt[3]= 255; else rt[3]= tempc;
			}
			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			fac= fac4;
			mfac= 256 - ( (fac4*rt1[3])>>8 );

			if(fac==0) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
			else if(mfac==0) *( (unsigned int *)rt) = *( (unsigned int *)rt1);
			else {
				tempc= ( fac*rt1[0] + mfac*rt2[0])>>8;
				if(tempc>255) rt[0]= 255; else rt[0]= tempc;
				tempc= ( fac*rt1[1] + mfac*rt2[1])>>8;
				if(tempc>255) rt[1]= 255; else rt[1]= tempc;
				tempc= ( fac*rt1[2] + mfac*rt2[2])>>8;
				if(tempc>255) rt[2]= 255; else rt[2]= tempc;
				tempc= ( fac*rt1[3] + mfac*rt2[3])>>8;
				if(tempc>255) rt[3]= 255; else rt[3]= tempc;
			}
			rt1+= 4; rt2+= 4; rt+= 4;
		}
	}
}

static void do_alphaover_effect_float(float facf0, float facf1, int x, int y, 
					  float * rect1, float *rect2, float *out)
{
	float fac2, mfac, fac, fac4;
	int xo;
	float *rt1, *rt2, *rt;

	xo= x;
	rt1= rect1;
	rt2= rect2;
	rt= out;

	fac2= facf0;
	fac4= facf1;

	while(y--) {

		x= xo;
		while(x--) {

			/* rt = rt1 over rt2  (alpha from rt1) */

			fac= fac2;
			mfac= 1.0 - (fac2*rt1[3]) ;

			if(fac <= 0.0) {
				memcpy(rt, rt2, 4 * sizeof(float));
			} else if(mfac <=0) {
				memcpy(rt, rt1, 4 * sizeof(float));
			} else {
				rt[0] = fac*rt1[0] + mfac*rt2[0];
				rt[1] = fac*rt1[1] + mfac*rt2[1];
				rt[2] = fac*rt1[2] + mfac*rt2[2];
				rt[3] = fac*rt1[3] + mfac*rt2[3];
			}
			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			fac= fac4;
			mfac= 1.0 - (fac4*rt1[3]);

			if(fac <= 0.0) {
				memcpy(rt, rt2, 4 * sizeof(float));
			} else if(mfac <= 0.0) {
				memcpy(rt, rt1, 4 * sizeof(float));
			} else {
				rt[0] = fac*rt1[0] + mfac*rt2[0];
				rt[1] = fac*rt1[1] + mfac*rt2[1];
				rt[2] = fac*rt1[2] + mfac*rt2[2];
				rt[3] = fac*rt1[3] + mfac*rt2[3];
			}
			rt1+= 4; rt2+= 4; rt+= 4;
		}
	}
}

static void do_alphaover_effect(Scene *scene, Sequence *seq, int cfra,
				float facf0, float facf1, int x, int y, 
				struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
				struct ImBuf *ibuf3, struct ImBuf *out)
{
	if (out->rect_float) {
		do_alphaover_effect_float(
			facf0, facf1, x, y,
			ibuf1->rect_float, ibuf2->rect_float,
			out->rect_float);
	} else {
		do_alphaover_effect_byte(
			facf0, facf1, x, y,
			(char*) ibuf1->rect, (char*) ibuf2->rect,
			(char*) out->rect);
	}
}


/* **********************************************************************
   ALPHA UNDER
   ********************************************************************** */

void do_alphaunder_effect_byte(
	float facf0, float facf1, int x, int y, char *rect1, 
	char *rect2, char *out)
{
	int fac2, mfac, fac, fac4;
	int xo;
	char *rt1, *rt2, *rt;

	xo= x;
	rt1= rect1;
	rt2= rect2;
	rt= out;

	fac2= (int)(256.0*facf0);
	fac4= (int)(256.0*facf1);

	while(y--) {

		x= xo;
		while(x--) {

			/* rt = rt1 under rt2  (alpha from rt2) */

			/* this complex optimalisation is because the
			 * 'skybuf' can be crossed in
			 */
			if(rt2[3]==0 && fac2==256) *( (unsigned int *)rt) = *( (unsigned int *)rt1);
			else if(rt2[3]==255) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
			else {
				mfac= rt2[3];
				fac= (fac2*(256-mfac))>>8;

				if(fac==0) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
				else {
					rt[0]= ( fac*rt1[0] + mfac*rt2[0])>>8;
					rt[1]= ( fac*rt1[1] + mfac*rt2[1])>>8;
					rt[2]= ( fac*rt1[2] + mfac*rt2[2])>>8;
					rt[3]= ( fac*rt1[3] + mfac*rt2[3])>>8;
				}
			}
			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			if(rt2[3]==0 && fac4==256) *( (unsigned int *)rt) = *( (unsigned int *)rt1);
			else if(rt2[3]==255) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
			else {
				mfac= rt2[3];
				fac= (fac4*(256-mfac))>>8;

				if(fac==0) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
				else {
					rt[0]= ( fac*rt1[0] + mfac*rt2[0])>>8;
					rt[1]= ( fac*rt1[1] + mfac*rt2[1])>>8;
					rt[2]= ( fac*rt1[2] + mfac*rt2[2])>>8;
					rt[3]= ( fac*rt1[3] + mfac*rt2[3])>>8;
				}
			}
			rt1+= 4; rt2+= 4; rt+= 4;
		}
	}
}


static void do_alphaunder_effect_float(float facf0, float facf1, int x, int y, 
					   float *rect1, float *rect2, 
					   float *out)
{
	float fac2, mfac, fac, fac4;
	int xo;
	float *rt1, *rt2, *rt;

	xo= x;
	rt1= rect1;
	rt2= rect2;
	rt= out;

	fac2= facf0;
	fac4= facf1;

	while(y--) {

		x= xo;
		while(x--) {

			/* rt = rt1 under rt2  (alpha from rt2) */

			/* this complex optimalisation is because the
			 * 'skybuf' can be crossed in
			 */
			if( rt2[3]<=0 && fac2>=1.0) {
				memcpy(rt, rt1, 4 * sizeof(float));
			} else if(rt2[3]>=1.0) {
				memcpy(rt, rt2, 4 * sizeof(float));
			} else {
				mfac = rt2[3];
				fac = fac2 * (1.0 - mfac);

				if(fac == 0) {
					memcpy(rt, rt2, 4 * sizeof(float));
				} else {
					rt[0]= fac*rt1[0] + mfac*rt2[0];
					rt[1]= fac*rt1[1] + mfac*rt2[1];
					rt[2]= fac*rt1[2] + mfac*rt2[2];
					rt[3]= fac*rt1[3] + mfac*rt2[3];
				}
			}
			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			if(rt2[3]<=0 && fac4 >= 1.0) {
				memcpy(rt, rt1, 4 * sizeof(float));
 
			} else if(rt2[3]>=1.0) {
				memcpy(rt, rt2, 4 * sizeof(float));
			} else {
				mfac= rt2[3];
				fac= fac4*(1.0-mfac);

				if(fac == 0) {
					memcpy(rt, rt2, 4 * sizeof(float));
				} else {
					rt[0]= fac * rt1[0] + mfac * rt2[0];
					rt[1]= fac * rt1[1] + mfac * rt2[1];
					rt[2]= fac * rt1[2] + mfac * rt2[2];
					rt[3]= fac * rt1[3] + mfac * rt2[3];
				}
			}
			rt1+= 4; rt2+= 4; rt+= 4;
		}
	}
}

static void do_alphaunder_effect(Scene *scene, Sequence *seq, int cfra,
				float facf0, float facf1, int x, int y, 
				struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
				struct ImBuf *ibuf3, struct ImBuf *out)
{
	if (out->rect_float) {
		do_alphaunder_effect_float(
			facf0, facf1, x, y,
			ibuf1->rect_float, ibuf2->rect_float,
			out->rect_float);
	} else {
		do_alphaunder_effect_byte(
			facf0, facf1, x, y,
			(char*) ibuf1->rect, (char*) ibuf2->rect,
			(char*) out->rect);
	}
}


/* **********************************************************************
   CROSS
   ********************************************************************** */

void do_cross_effect_byte(float facf0, float facf1, int x, int y, 
			  char *rect1, char *rect2, 
			  char *out)
{
	int fac1, fac2, fac3, fac4;
	int xo;
	char *rt1, *rt2, *rt;

	xo= x;
	rt1= rect1;
	rt2= rect2;
	rt= out;

	fac2= (int)(256.0*facf0);
	fac1= 256-fac2;
	fac4= (int)(256.0*facf1);
	fac3= 256-fac4;

	while(y--) {

		x= xo;
		while(x--) {

			rt[0]= (fac1*rt1[0] + fac2*rt2[0])>>8;
			rt[1]= (fac1*rt1[1] + fac2*rt2[1])>>8;
			rt[2]= (fac1*rt1[2] + fac2*rt2[2])>>8;
			rt[3]= (fac1*rt1[3] + fac2*rt2[3])>>8;

			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			rt[0]= (fac3*rt1[0] + fac4*rt2[0])>>8;
			rt[1]= (fac3*rt1[1] + fac4*rt2[1])>>8;
			rt[2]= (fac3*rt1[2] + fac4*rt2[2])>>8;
			rt[3]= (fac3*rt1[3] + fac4*rt2[3])>>8;

			rt1+= 4; rt2+= 4; rt+= 4;
		}

	}
}

void do_cross_effect_float(float facf0, float facf1, int x, int y, 
			   float *rect1, float *rect2, float *out)
{
	float fac1, fac2, fac3, fac4;
	int xo;
	float *rt1, *rt2, *rt;

	xo= x;
	rt1= rect1;
	rt2= rect2;
	rt= out;

	fac2= facf0;
	fac1= 1.0 - fac2;
	fac4= facf1;
	fac3= 1.0 - fac4;

	while(y--) {

		x= xo;
		while(x--) {

			rt[0]= fac1*rt1[0] + fac2*rt2[0];
			rt[1]= fac1*rt1[1] + fac2*rt2[1];
			rt[2]= fac1*rt1[2] + fac2*rt2[2];
			rt[3]= fac1*rt1[3] + fac2*rt2[3];

			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			rt[0]= fac3*rt1[0] + fac4*rt2[0];
			rt[1]= fac3*rt1[1] + fac4*rt2[1];
			rt[2]= fac3*rt1[2] + fac4*rt2[2];
			rt[3]= fac3*rt1[3] + fac4*rt2[3];

			rt1+= 4; rt2+= 4; rt+= 4;
		}

	}
}

/* carefull: also used by speed effect! */

static void do_cross_effect(Scene *scene, Sequence *seq, int cfra,
				float facf0, float facf1, int x, int y, 
				struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
				struct ImBuf *ibuf3, struct ImBuf *out)
{
	if (out->rect_float) {
		do_cross_effect_float(
			facf0, facf1, x, y,
			ibuf1->rect_float, ibuf2->rect_float,
			out->rect_float);
	} else {
		do_cross_effect_byte(
			facf0, facf1, x, y,
			(char*) ibuf1->rect, (char*) ibuf2->rect,
			(char*) out->rect);
	}
}


/* **********************************************************************
   GAMMA CROSS
   ********************************************************************** */

/* copied code from initrender.c */
static unsigned short gamtab[65536];
static unsigned short igamtab1[256];
static int gamma_tabs_init = FALSE;

#define RE_GAMMA_TABLE_SIZE 400

static float gamma_range_table[RE_GAMMA_TABLE_SIZE + 1];
static float gamfactor_table[RE_GAMMA_TABLE_SIZE];
static float inv_gamma_range_table[RE_GAMMA_TABLE_SIZE + 1];
static float inv_gamfactor_table[RE_GAMMA_TABLE_SIZE];
static float color_domain_table[RE_GAMMA_TABLE_SIZE + 1]; 
static float color_step;
static float inv_color_step;
static float valid_gamma;
static float valid_inv_gamma;

static void makeGammaTables(float gamma)
{
	/* we need two tables: one forward, one backward */
	int i;

	valid_gamma        = gamma;
	valid_inv_gamma    = 1.0 / gamma;
	color_step        = 1.0 / RE_GAMMA_TABLE_SIZE;
	inv_color_step    = (float) RE_GAMMA_TABLE_SIZE; 

	/* We could squeeze out the two range tables to gain some memory.        */	
	for (i = 0; i < RE_GAMMA_TABLE_SIZE; i++) {
		color_domain_table[i]   = i * color_step;
		gamma_range_table[i]     = pow(color_domain_table[i],
										valid_gamma);
		inv_gamma_range_table[i] = pow(color_domain_table[i],
										valid_inv_gamma);
	}

	/* The end of the table should match 1.0 carefully. In order to avoid    */
	/* rounding errors, we just set this explicitly. The last segment may    */
	/* have a different lenght than the other segments, but our              */
	/* interpolation is insensitive to that.                                 */
	color_domain_table[RE_GAMMA_TABLE_SIZE]   = 1.0;
	gamma_range_table[RE_GAMMA_TABLE_SIZE]     = 1.0;
	inv_gamma_range_table[RE_GAMMA_TABLE_SIZE] = 1.0;

	/* To speed up calculations, we make these calc factor tables. They are  */
	/* multiplication factors used in scaling the interpolation.             */
	for (i = 0; i < RE_GAMMA_TABLE_SIZE; i++ ) {
		gamfactor_table[i] = inv_color_step
			* (gamma_range_table[i + 1] - gamma_range_table[i]) ;
		inv_gamfactor_table[i] = inv_color_step
			* (inv_gamma_range_table[i + 1] - inv_gamma_range_table[i]) ;
	}

} /* end of void makeGammaTables(float gamma) */


static float gammaCorrect(float c)
{
	int i;
	float res = 0.0;
	
	i = floor(c * inv_color_step);
	/* Clip to range [0,1]: outside, just do the complete calculation.       */
	/* We may have some performance problems here. Stretching up the LUT     */
	/* may help solve that, by exchanging LUT size for the interpolation.    */
	/* Negative colors are explicitly handled.                              */
	if (i < 0) res = -pow(abs(c), valid_gamma);
	else if (i >= RE_GAMMA_TABLE_SIZE ) res = pow(c, valid_gamma);
	else res = gamma_range_table[i] + 
			   ( (c - color_domain_table[i]) * gamfactor_table[i]); 
	
	return res;
} /* end of float gammaCorrect(float col) */

/* ------------------------------------------------------------------------- */

static float invGammaCorrect(float col)
{
	int i;
	float res = 0.0;

	i = floor(col*inv_color_step);
	/* Negative colors are explicitly handled.                              */
	if (i < 0) res = -pow(abs(col), valid_inv_gamma);
	else if (i >= RE_GAMMA_TABLE_SIZE) res = pow(col, valid_inv_gamma);
	else res = inv_gamma_range_table[i] + 
			   ( (col - color_domain_table[i]) * inv_gamfactor_table[i]);
 
	return res;
} /* end of float invGammaCorrect(float col) */


static void gamtabs(float gamma)
{
	float val, igamma= 1.0f/gamma;
	int a;
	
	/* gamtab: in short, out short */
	for(a=0; a<65536; a++) {
		val= a;
		val/= 65535.0;
		
		if(gamma==2.0) val= sqrt(val);
		else if(gamma!=1.0) val= pow(val, igamma);
		
		gamtab[a]= (65535.99*val);
	}
	/* inverse gamtab1 : in byte, out short */
	for(a=1; a<=256; a++) {
		if(gamma==2.0) igamtab1[a-1]= a*a-1;
		else if(gamma==1.0) igamtab1[a-1]= 256*a-1;
		else {
			val= a/256.0;
			igamtab1[a-1]= (65535.0*pow(val, gamma)) -1 ;
		}
	}

}

static void build_gammatabs()
{
	if (gamma_tabs_init == FALSE) {
		gamtabs(2.0f);
		makeGammaTables(2.0f);
		gamma_tabs_init = TRUE;
	}
}

static void init_gammacross(Sequence * seq)
{
}

static void load_gammacross(Sequence * seq)
{
}

static void free_gammacross(Sequence * seq)
{
}

static void do_gammacross_effect_byte(float facf0, float facf1, 
					  int x, int y, 
					  unsigned char *rect1, 
					  unsigned char *rect2, 
					  unsigned char *out)
{
	int fac1, fac2, col;
	int xo;
	unsigned char *rt1, *rt2, *rt;
	
	xo= x;
	rt1= (unsigned char *)rect1;
	rt2= (unsigned char *)rect2;
	rt= (unsigned char *)out;

	fac2= (int)(256.0*facf0);
	fac1= 256-fac2;

	while(y--) {

		x= xo;
		while(x--) {

			col= (fac1*igamtab1[rt1[0]] + fac2*igamtab1[rt2[0]])>>8;
			if(col>65535) rt[0]= 255; else rt[0]= ( (char *)(gamtab+col))[MOST_SIG_BYTE];
			col=(fac1*igamtab1[rt1[1]] + fac2*igamtab1[rt2[1]])>>8;
			if(col>65535) rt[1]= 255; else rt[1]= ( (char *)(gamtab+col))[MOST_SIG_BYTE];
			col= (fac1*igamtab1[rt1[2]] + fac2*igamtab1[rt2[2]])>>8;
			if(col>65535) rt[2]= 255; else rt[2]= ( (char *)(gamtab+col))[MOST_SIG_BYTE];
			col= (fac1*igamtab1[rt1[3]] + fac2*igamtab1[rt2[3]])>>8;
			if(col>65535) rt[3]= 255; else rt[3]= ( (char *)(gamtab+col))[MOST_SIG_BYTE];

			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			col= (fac1*igamtab1[rt1[0]] + fac2*igamtab1[rt2[0]])>>8;
			if(col>65535) rt[0]= 255; else rt[0]= ( (char *)(gamtab+col))[MOST_SIG_BYTE];
			col= (fac1*igamtab1[rt1[1]] + fac2*igamtab1[rt2[1]])>>8;
			if(col>65535) rt[1]= 255; else rt[1]= ( (char *)(gamtab+col))[MOST_SIG_BYTE];
			col= (fac1*igamtab1[rt1[2]] + fac2*igamtab1[rt2[2]])>>8;
			if(col>65535) rt[2]= 255; else rt[2]= ( (char *)(gamtab+col))[MOST_SIG_BYTE];
			col= (fac1*igamtab1[rt1[3]] + fac2*igamtab1[rt2[3]])>>8;
			if(col>65535) rt[3]= 255; else rt[3]= ( (char *)(gamtab+col))[MOST_SIG_BYTE];

			rt1+= 4; rt2+= 4; rt+= 4;
		}
	}

}

static void do_gammacross_effect_float(float facf0, float facf1, 
					   int x, int y, 
					   float *rect1, float *rect2, 
					   float *out)
{
	float fac1, fac2;
	int xo;
	float *rt1, *rt2, *rt;

	xo= x;
	rt1= rect1;
	rt2= rect2;
	rt= out;

	fac2= facf0;
	fac1= 1.0 - fac2;

	while(y--) {

		x= xo * 4;
		while(x--) {

			*rt= gammaCorrect(
				fac1 * invGammaCorrect(*rt1) 
				+ fac2 * invGammaCorrect(*rt2));
			rt1++; rt2++; rt++;
		}

		if(y==0) break;
		y--;

		x= xo * 4;
		while(x--) {

			*rt= gammaCorrect(
				fac1*invGammaCorrect(*rt1) 
				+ fac2*invGammaCorrect(*rt2));

			rt1++; rt2++; rt++;
		}
	}
}

static void do_gammacross_effect(Scene *scene, Sequence *seq, int cfra,
				 float facf0, float facf1, int x, int y, 
				 struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
				 struct ImBuf *ibuf3, struct ImBuf *out)
{
	build_gammatabs();

	if (out->rect_float) {
		do_gammacross_effect_float(
			facf0, facf1, x, y,
			ibuf1->rect_float, ibuf2->rect_float,
			out->rect_float);
	} else {
		do_gammacross_effect_byte(
			facf0, facf1, x, y,
			(unsigned char*) ibuf1->rect, (unsigned char*) ibuf2->rect,
			(unsigned char*) out->rect);
	}
}


/* **********************************************************************
   ADD
   ********************************************************************** */

static void do_add_effect_byte(float facf0, float facf1, int x, int y, 
				   unsigned char *rect1, unsigned char *rect2, 
				   unsigned char *out)
{
	int col, xo, fac1, fac3;
	char *rt1, *rt2, *rt;

	xo= x;
	rt1= (char *)rect1;
	rt2= (char *)rect2;
	rt= (char *)out;

	fac1= (int)(256.0*facf0);
	fac3= (int)(256.0*facf1);

	while(y--) {

		x= xo;
		while(x--) {

			col= rt1[0]+ ((fac1*rt2[0])>>8);
			if(col>255) rt[0]= 255; else rt[0]= col;
			col= rt1[1]+ ((fac1*rt2[1])>>8);
			if(col>255) rt[1]= 255; else rt[1]= col;
			col= rt1[2]+ ((fac1*rt2[2])>>8);
			if(col>255) rt[2]= 255; else rt[2]= col;
			col= rt1[3]+ ((fac1*rt2[3])>>8);
			if(col>255) rt[3]= 255; else rt[3]= col;

			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			col= rt1[0]+ ((fac3*rt2[0])>>8);
			if(col>255) rt[0]= 255; else rt[0]= col;
			col= rt1[1]+ ((fac3*rt2[1])>>8);
			if(col>255) rt[1]= 255; else rt[1]= col;
			col= rt1[2]+ ((fac3*rt2[2])>>8);
			if(col>255) rt[2]= 255; else rt[2]= col;
			col= rt1[3]+ ((fac3*rt2[3])>>8);
			if(col>255) rt[3]= 255; else rt[3]= col;

			rt1+= 4; rt2+= 4; rt+= 4;
		}
	}
}

static void do_add_effect_float(float facf0, float facf1, int x, int y, 
				float *rect1, float *rect2, 
				float *out)
{
	int xo;
	float fac1, fac3;
	float *rt1, *rt2, *rt;

	xo= x;
	rt1= rect1;
	rt2= rect2;
	rt= out;

	fac1= facf0;
	fac3= facf1;

	while(y--) {

		x= xo * 4;
		while(x--) {
			*rt = *rt1 + fac1 * (*rt2);

			rt1++; rt2++; rt++;
		}

		if(y==0) break;
		y--;

		x= xo * 4;
		while(x--) {
			*rt = *rt1 + fac3 * (*rt2);

			rt1++; rt2++; rt++;
		}
	}
}

static void do_add_effect(Scene *scene, Sequence *seq, int cfra,
			  float facf0, float facf1, int x, int y, 
			  struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
			  struct ImBuf *ibuf3, struct ImBuf *out)
{
	if (out->rect_float) {
		do_add_effect_float(
			facf0, facf1, x, y,
			ibuf1->rect_float, ibuf2->rect_float,
			out->rect_float);
	} else {
		do_add_effect_byte(
			facf0, facf1, x, y,
			(unsigned char*) ibuf1->rect, (unsigned char*) ibuf2->rect,
			(unsigned char*) out->rect);
	}
}


/* **********************************************************************
   SUB
   ********************************************************************** */

static void do_sub_effect_byte(float facf0, float facf1, 
				   int x, int y, 
				   char *rect1, char *rect2, char *out)
{
	int col, xo, fac1, fac3;
	char *rt1, *rt2, *rt;

	xo= x;
	rt1= (char *)rect1;
	rt2= (char *)rect2;
	rt= (char *)out;

	fac1= (int)(256.0*facf0);
	fac3= (int)(256.0*facf1);

	while(y--) {

		x= xo;
		while(x--) {

			col= rt1[0]- ((fac1*rt2[0])>>8);
			if(col<0) rt[0]= 0; else rt[0]= col;
			col= rt1[1]- ((fac1*rt2[1])>>8);
			if(col<0) rt[1]= 0; else rt[1]= col;
			col= rt1[2]- ((fac1*rt2[2])>>8);
			if(col<0) rt[2]= 0; else rt[2]= col;
			col= rt1[3]- ((fac1*rt2[3])>>8);
			if(col<0) rt[3]= 0; else rt[3]= col;

			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			col= rt1[0]- ((fac3*rt2[0])>>8);
			if(col<0) rt[0]= 0; else rt[0]= col;
			col= rt1[1]- ((fac3*rt2[1])>>8);
			if(col<0) rt[1]= 0; else rt[1]= col;
			col= rt1[2]- ((fac3*rt2[2])>>8);
			if(col<0) rt[2]= 0; else rt[2]= col;
			col= rt1[3]- ((fac3*rt2[3])>>8);
			if(col<0) rt[3]= 0; else rt[3]= col;

			rt1+= 4; rt2+= 4; rt+= 4;
		}
	}
}

static void do_sub_effect_float(float facf0, float facf1, int x, int y, 
				float *rect1, float *rect2, 
				float *out)
{
	int xo;
	float fac1, fac3;
	float *rt1, *rt2, *rt;

	xo= x;
	rt1= rect1;
	rt2= rect2;
	rt= out;

	fac1= facf0;
	fac3= facf1;

	while(y--) {

		x= xo * 4;
		while(x--) {
			*rt = *rt1 - fac1 * (*rt2);

			rt1++; rt2++; rt++;
		}

		if(y==0) break;
		y--;

		x= xo * 4;
		while(x--) {
			*rt = *rt1 - fac3 * (*rt2);

			rt1++; rt2++; rt++;
		}
	}
}

static void do_sub_effect(Scene *scene, Sequence *seq, int cfra,
			  float facf0, float facf1, int x, int y, 
			  struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
			  struct ImBuf *ibuf3, struct ImBuf *out)
{
	if (out->rect_float) {
		do_sub_effect_float(
			facf0, facf1, x, y,
			ibuf1->rect_float, ibuf2->rect_float,
			out->rect_float);
	} else {
		do_sub_effect_byte(
			facf0, facf1, x, y,
			(char*) ibuf1->rect, (char*) ibuf2->rect,
			(char*) out->rect);
	}
}

/* **********************************************************************
   DROP
   ********************************************************************** */

/* Must be > 0 or add precopy, etc to the function */
#define XOFF	8
#define YOFF	8

static void do_drop_effect_byte(float facf0, float facf1, int x, int y, 
				unsigned char *rect2i, unsigned char *rect1i, 
				unsigned char *outi)
{
	int height, width, temp, fac, fac1, fac2;
	char *rt1, *rt2, *out;
	int field= 1;

	width= x;
	height= y;

	fac1= (int)(70.0*facf0);
	fac2= (int)(70.0*facf1);

	rt2= (char*) (rect2i + YOFF*width);
	rt1= (char*) rect1i;
	out= (char*) outi;
	for (y=0; y<height-YOFF; y++) {
		if(field) fac= fac1;
		else fac= fac2;
		field= !field;

		memcpy(out, rt1, sizeof(int)*XOFF);
		rt1+= XOFF*4;
		out+= XOFF*4;

		for (x=XOFF; x<width; x++) {
			temp= ((fac*rt2[3])>>8);

			*(out++)= MAX2(0, *rt1 - temp); rt1++;
			*(out++)= MAX2(0, *rt1 - temp); rt1++;
			*(out++)= MAX2(0, *rt1 - temp); rt1++;
			*(out++)= MAX2(0, *rt1 - temp); rt1++;
			rt2+=4;
		}
		rt2+=XOFF*4;
	}
	memcpy(out, rt1, sizeof(int)*YOFF*width);
}

static void do_drop_effect_float(float facf0, float facf1, int x, int y, 
				 float *rect2i, float *rect1i, 
				 float *outi)
{
	int height, width;
	float temp, fac, fac1, fac2;
	float *rt1, *rt2, *out;
	int field= 1;

	width= x;
	height= y;

	fac1= 70.0*facf0;
	fac2= 70.0*facf1;

	rt2=  (rect2i + YOFF*width);
	rt1=  rect1i;
	out=  outi;
	for (y=0; y<height-YOFF; y++) {
		if(field) fac= fac1;
		else fac= fac2;
		field= !field;

		memcpy(out, rt1, 4 * sizeof(float)*XOFF);
		rt1+= XOFF*4;
		out+= XOFF*4;

		for (x=XOFF; x<width; x++) {
			temp= fac * rt2[3];

			*(out++)= MAX2(0.0, *rt1 - temp); rt1++;
			*(out++)= MAX2(0.0, *rt1 - temp); rt1++;
			*(out++)= MAX2(0.0, *rt1 - temp); rt1++;
			*(out++)= MAX2(0.0, *rt1 - temp); rt1++;
			rt2+=4;
		}
		rt2+=XOFF*4;
	}
	memcpy(out, rt1, 4 * sizeof(float)*YOFF*width);
}


static void do_drop_effect(Scene *scene, Sequence *seq, int cfra,
			   float facf0, float facf1, int x, int y, 
			   struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
			   struct ImBuf * ibuf3,
			   struct ImBuf *out)
{
	if (out->rect_float) {
		do_drop_effect_float(
			facf0, facf1, x, y,
			ibuf1->rect_float, ibuf2->rect_float,
			out->rect_float);
	} else {
		do_drop_effect_byte(
			facf0, facf1, x, y,
			(unsigned char*) ibuf1->rect, (unsigned char*) ibuf2->rect,
			(unsigned char*) out->rect);
	}
}

/* **********************************************************************
   MUL
   ********************************************************************** */

static void do_mul_effect_byte(float facf0, float facf1, int x, int y, 
				   unsigned char *rect1, unsigned char *rect2, 
				   unsigned char *out)
{
	int xo, fac1, fac3;
	char *rt1, *rt2, *rt;

	xo= x;
	rt1= (char *)rect1;
	rt2= (char *)rect2;
	rt= (char *)out;

	fac1= (int)(256.0*facf0);
	fac3= (int)(256.0*facf1);

	/* formula:
	 *		fac*(a*b) + (1-fac)*a  => fac*a*(b-1)+axaux= c*px + py*s ;//+centx
			yaux= -s*px + c*py;//+centy
	 */

	while(y--) {

		x= xo;
		while(x--) {

			rt[0]= rt1[0] + ((fac1*rt1[0]*(rt2[0]-256))>>16);
			rt[1]= rt1[1] + ((fac1*rt1[1]*(rt2[1]-256))>>16);
			rt[2]= rt1[2] + ((fac1*rt1[2]*(rt2[2]-256))>>16);
			rt[3]= rt1[3] + ((fac1*rt1[3]*(rt2[3]-256))>>16);

			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			rt[0]= rt1[0] + ((fac3*rt1[0]*(rt2[0]-256))>>16);
			rt[1]= rt1[1] + ((fac3*rt1[1]*(rt2[1]-256))>>16);
			rt[2]= rt1[2] + ((fac3*rt1[2]*(rt2[2]-256))>>16);
			rt[3]= rt1[3] + ((fac3*rt1[3]*(rt2[3]-256))>>16);

			rt1+= 4; rt2+= 4; rt+= 4;
		}
	}
}

static void do_mul_effect_float(float facf0, float facf1, int x, int y, 
					float *rect1, float *rect2, 
					float *out)
{
	int xo;
	float fac1, fac3;
	float *rt1, *rt2, *rt;

	xo= x;
	rt1= rect1;
	rt2= rect2;
	rt= out;

	fac1= facf0;
	fac3= facf1;

	/* formula:
	 *		fac*(a*b) + (1-fac)*a  => fac*a*(b-1)+a
	 */

	while(y--) {

		x= xo;
		while(x--) {

			rt[0]= rt1[0] + fac1*rt1[0]*(rt2[0]-1.0);
			rt[1]= rt1[1] + fac1*rt1[1]*(rt2[1]-1.0);
			rt[2]= rt1[2] + fac1*rt1[2]*(rt2[2]-1.0);
			rt[3]= rt1[3] + fac1*rt1[3]*(rt2[3]-1.0);

			rt1+= 4; rt2+= 4; rt+= 4;
		}

		if(y==0) break;
		y--;

		x= xo;
		while(x--) {

			rt[0]= rt1[0] + fac3*rt1[0]*(rt2[0]-1.0);
			rt[1]= rt1[1] + fac3*rt1[1]*(rt2[1]-1.0);
			rt[2]= rt1[2] + fac3*rt1[2]*(rt2[2]-1.0);
			rt[3]= rt1[3] + fac3*rt1[3]*(rt2[3]-1.0);

			rt1+= 4; rt2+= 4; rt+= 4;
		}
	}
}

static void do_mul_effect(Scene *scene, Sequence *seq, int cfra,
			  float facf0, float facf1, int x, int y, 
			  struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
			  struct ImBuf *ibuf3, struct ImBuf *out)
{
	if (out->rect_float) {
		do_mul_effect_float(
			facf0, facf1, x, y,
			ibuf1->rect_float, ibuf2->rect_float,
			out->rect_float);
	} else {
		do_mul_effect_byte(
			facf0, facf1, x, y,
			(unsigned char*) ibuf1->rect, (unsigned char*) ibuf2->rect,
			(unsigned char*) out->rect);
	}
}

/* **********************************************************************
   WIPE
   ********************************************************************** */

typedef struct WipeZone {
	float angle;
	int flip;
	int xo, yo;
	int width;
	float invwidth;
	float pythangle;
} WipeZone;

static void precalc_wipe_zone(WipeZone *wipezone, WipeVars *wipe, int xo, int yo)
{
	wipezone->flip = (wipe->angle < 0);
	wipezone->angle = pow(fabs(wipe->angle)/45.0f, log(xo)/log(2.0f));
	wipezone->xo = xo;
	wipezone->yo = yo;
	wipezone->width = (int)(wipe->edgeWidth*((xo+yo)/2.0f));
	wipezone->pythangle = 1.0f/sqrt(wipe->angle*wipe->angle + 1.0f);

	if(wipe->wipetype == DO_SINGLE_WIPE)
		wipezone->invwidth = 1.0f/wipezone->width;
	else
		wipezone->invwidth = 1.0f/(0.5f*wipezone->width);
}

// This function calculates the blur band for the wipe effects
static float in_band(WipeZone *wipezone,float width,float dist,float perc,int side,int dir)
{
	float t1,t2,alpha,percwidth;

	if(width == 0)
		return (float)side;

	if(side == 1)
		percwidth = width * perc;
	else
		percwidth = width * (1 - perc);
	
	if(width < dist)
		return side;
	
	t1 = dist * wipezone->invwidth;  //percentange of width that is
	t2 = wipezone->invwidth;  //amount of alpha per % point
	
	if(side == 1)
		alpha = (t1*t2*100) + (1-perc); // add point's alpha contrib to current position in wipe
	else
		alpha = (1-perc) - (t1*t2*100);
	
	if(dir == 0)
		alpha = 1-alpha;

	return alpha;
}

static float check_zone(WipeZone *wipezone, int x, int y,
	Sequence *seq, float facf0) 
{
	float posx, posy,hyp,hyp2,angle,hwidth,b1,b2,b3,pointdist;
/*some future stuff
float hyp3,hyp4,b4,b5	   
*/
	float temp1,temp2,temp3,temp4; //some placeholder variables
	int xo = wipezone->xo;
	int yo = wipezone->yo;
	float halfx = xo*0.5f;
	float halfy = yo*0.5f;
	float widthf,output=0;
	WipeVars *wipe = (WipeVars *)seq->effectdata;
	int width;

	if(wipezone->flip) x = xo - x;
	angle = wipezone->angle;

	posy = facf0 * yo;

	if(wipe->forward){
		posx = facf0 * xo;
		posy = facf0 * yo;
	} else{
		posx = xo - facf0 * xo;
		posy = yo - facf0 * yo;
	}

	switch (wipe->wipetype) {
		case DO_SINGLE_WIPE:
			width = wipezone->width;
			hwidth = width*0.5f;

			if(angle == 0.0f) {
				b1 = posy;
				b2 = y;
				hyp = fabs(y - posy);
			}
			else {
				b1 = posy - (-angle)*posx;
				b2 = y - (-angle)*x;
				hyp = fabs(angle*x+y+(-posy-angle*posx))*wipezone->pythangle;
			}

			if(angle < 0) {
				temp1 = b1;
				b1 = b2;
				b2 = temp1;
			}

			if(wipe->forward) {
				if(b1 < b2)
					output = in_band(wipezone,width,hyp,facf0,1,1);
				else
					output = in_band(wipezone,width,hyp,facf0,0,1);
			 }
			else {
				if(b1 < b2)
					output = in_band(wipezone,width,hyp,facf0,0,1);
				else
					output = in_band(wipezone,width,hyp,facf0,1,1);
			 }
		break;
	 
		case DO_DOUBLE_WIPE:
			if(!wipe->forward)
				facf0 = 1.0f-facf0;   // Go the other direction

			width = wipezone->width;  // calculate the blur width
			hwidth = width*0.5f;
			if (angle == 0) {
				b1 = posy*0.5f;
				b3 = yo-posy*0.5f;
				b2 = y;

				hyp = abs(y - posy*0.5f);
				hyp2 = abs(y - (yo-posy*0.5f));
			}
			else {
				b1 = posy*0.5f - (-angle)*posx*0.5f;
				b3 = (yo-posy*0.5f) - (-angle)*(xo-posx*0.5f);
				b2 = y - (-angle)*x;

				hyp = abs(angle*x+y+(-posy*0.5f-angle*posx*0.5f))*wipezone->pythangle;
				hyp2 = abs(angle*x+y+(-(yo-posy*0.5f)-angle*(xo-posx*0.5f)))*wipezone->pythangle;
			}

			temp1 = xo*(1-facf0*0.5f)-xo*facf0*0.5f;
			temp2 = yo*(1-facf0*0.5f)-yo*facf0*0.5f;
			pointdist = sqrt(temp1*temp1 + temp2*temp2);

			if(b2 < b1 && b2 < b3 ){
				if(hwidth < pointdist)
					output = in_band(wipezone,hwidth,hyp,facf0,0,1);
			} else if(b2 > b1 && b2 > b3 ){
				if(hwidth < pointdist)
					output = in_band(wipezone,hwidth,hyp2,facf0,0,1);	
			} else {
				if(  hyp < hwidth && hyp2 > hwidth )
					output = in_band(wipezone,hwidth,hyp,facf0,1,1);
				else if( hyp > hwidth && hyp2 < hwidth )
					  output = in_band(wipezone,hwidth,hyp2,facf0,1,1);
				else
					  output = in_band(wipezone,hwidth,hyp2,facf0,1,1) * in_band(wipezone,hwidth,hyp,facf0,1,1);
			}
			if(!wipe->forward)output = 1-output;
		 break;
		 case DO_CLOCK_WIPE:
			  /*
				  temp1: angle of effect center in rads
				  temp2: angle of line through (halfx,halfy) and (x,y) in rads
				  temp3: angle of low side of blur
				  temp4: angle of high side of blur
			  */
			 output = 1.0f - facf0;
			 widthf = wipe->edgeWidth*2.0f*(float)M_PI;
			  temp1 = 2.0f * (float)M_PI * facf0;
	 	 	
			 if(wipe->forward){
				 temp1 = 2.0f*(float)M_PI - temp1;
			 }
 	 		
			  x = x - halfx;
			  y = y - halfy;

			  temp2 = asin(abs(y)/sqrt(x*x + y*y));
			  if(x <= 0 && y >= 0) temp2 = (float)M_PI - temp2;
			  else if(x<=0 && y <= 0) temp2 += (float)M_PI;
			  else if(x >= 0 && y <= 0) temp2 = 2.0f*(float)M_PI - temp2;

			  if(wipe->forward){
				  temp3 = temp1-(widthf*0.5f)*facf0;
				  temp4 = temp1+(widthf*0.5f)*(1-facf0);
			  } else{
				  temp3 = temp1-(widthf*0.5f)*(1-facf0);
				  temp4 = temp1+(widthf*0.5f)*facf0;
			}
			  if (temp3 < 0) temp3 = 0;
			  if (temp4 > 2.0f*(float)M_PI) temp4 = 2.0f*(float)M_PI;
 	 		
 	 		
			  if(temp2 < temp3) output = 0;
			  else if (temp2 > temp4) output = 1;
			  else output = (temp2-temp3)/(temp4-temp3);
			  if(x == 0 && y == 0) output = 1;
			if(output != output) output = 1;
			if(wipe->forward) output = 1 - output;
		break;
	/* BOX WIPE IS NOT WORKING YET */
	/* case DO_CROSS_WIPE: */
	/* BOX WIPE IS NOT WORKING YET */
	/* 
		case DO_BOX_WIPE: 
			if(invert)facf0 = 1-facf0;

			width = (int)(wipe->edgeWidth*((xo+yo)/2.0));
			hwidth = (float)width/2.0;
			if (angle == 0)angle = 0.000001;
			b1 = posy/2 - (-angle)*posx/2;
			b3 = (yo-posy/2) - (-angle)*(xo-posx/2);
			b2 = y - (-angle)*x;

			hyp = abs(angle*x+y+(-posy/2-angle*posx/2))*wipezone->pythangle;
			hyp2 = abs(angle*x+y+(-(yo-posy/2)-angle*(xo-posx/2)))*wipezone->pythangle;

			temp1 = xo*(1-facf0/2)-xo*facf0/2;
			temp2 = yo*(1-facf0/2)-yo*facf0/2;
			pointdist = sqrt(temp1*temp1 + temp2*temp2);

			if(b2 < b1 && b2 < b3 ){
				if(hwidth < pointdist)
					output = in_band(wipezone,hwidth,hyp,facf0,0,1);
			} else if(b2 > b1 && b2 > b3 ){
				if(hwidth < pointdist)
					output = in_band(wipezone,hwidth,hyp2,facf0,0,1);	
			} else {
				if( hyp < hwidth && hyp2 > hwidth )
					output = in_band(wipezone,hwidth,hyp,facf0,1,1);
				else if( hyp > hwidth && hyp2 < hwidth )
					 output = in_band(wipezone,hwidth,hyp2,facf0,1,1);
				else
					 output = in_band(wipezone,hwidth,hyp2,facf0,1,1) * in_band(wipezone,hwidth,hyp,facf0,1,1);
			}

			if(invert)facf0 = 1-facf0;
			angle = -1/angle;
			b1 = posy/2 - (-angle)*posx/2;
			b3 = (yo-posy/2) - (-angle)*(xo-posx/2);
			b2 = y - (-angle)*x;

			hyp = abs(angle*x+y+(-posy/2-angle*posx/2))*wipezone->pythangle;
			hyp2 = abs(angle*x+y+(-(yo-posy/2)-angle*(xo-posx/2)))*wipezone->pythangle;

			if(b2 < b1 && b2 < b3 ){
				if(hwidth < pointdist)
					output *= in_band(wipezone,hwidth,hyp,facf0,0,1);
			} else if(b2 > b1 && b2 > b3 ){
				if(hwidth < pointdist)
					output *= in_band(wipezone,hwidth,hyp2,facf0,0,1);	
			} else {
				if( hyp < hwidth && hyp2 > hwidth )
					output *= in_band(wipezone,hwidth,hyp,facf0,1,1);
				else if( hyp > hwidth && hyp2 < hwidth )
					output *= in_band(wipezone,hwidth,hyp2,facf0,1,1);
				else
					output *= in_band(wipezone,hwidth,hyp2,facf0,1,1) * in_band(wipezone,hwidth,hyp,facf0,1,1);
			}

		break;
*/
		case DO_IRIS_WIPE:
			if(xo > yo) yo = xo;
			else xo = yo;

			if(!wipe->forward) facf0 = 1-facf0;

			width = wipezone->width;
			hwidth = width*0.5f;

			temp1 = (halfx-(halfx)*facf0);
			 pointdist = sqrt(temp1*temp1 + temp1*temp1);
		 
			 temp2 = sqrt((halfx-x)*(halfx-x) + (halfy-y)*(halfy-y));
			 if(temp2 > pointdist) output = in_band(wipezone,hwidth,fabs(temp2-pointdist),facf0,0,1);
			 else output = in_band(wipezone,hwidth,fabs(temp2-pointdist),facf0,1,1);
		 
			if(!wipe->forward) output = 1-output;
			
		break;
	}
	if (output < 0) output = 0;
	else if(output > 1) output = 1;
	return output;
}

static void init_wipe_effect(Sequence *seq)
{
	if(seq->effectdata)MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct WipeVars), "wipevars");
}

static int num_inputs_wipe()
{
	return 1;
}

static void free_wipe_effect(Sequence *seq)
{
	if(seq->effectdata)MEM_freeN(seq->effectdata);
	seq->effectdata = 0;
}

static void copy_wipe_effect(Sequence *dst, Sequence *src)
{
	dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void do_wipe_effect_byte(Sequence *seq, float facf0, float facf1, 
				int x, int y, 
				unsigned char *rect1, 
				unsigned char *rect2, unsigned char *out)
{
	WipeZone wipezone;
	WipeVars *wipe = (WipeVars *)seq->effectdata;
	int xo, yo;
	char *rt1, *rt2, *rt;

	precalc_wipe_zone(&wipezone, wipe, x, y);

	rt1 = (char *)rect1;
	rt2 = (char *)rect2;
	rt = (char *)out;

	xo = x;
	yo = y;
	for(y=0;y<yo;y++) {
		for(x=0;x<xo;x++) {
			float check = check_zone(&wipezone,x,y,seq,facf0);
			if (check) {
				if (rt1) {
					rt[0] = (int)(rt1[0]*check)+ (int)(rt2[0]*(1-check));
					rt[1] = (int)(rt1[1]*check)+ (int)(rt2[1]*(1-check));
					rt[2] = (int)(rt1[2]*check)+ (int)(rt2[2]*(1-check));
					rt[3] = (int)(rt1[3]*check)+ (int)(rt2[3]*(1-check));
				} else {
					rt[0] = 0;
					rt[1] = 0;
					rt[2] = 0;
					rt[3] = 255;
				}
			} else {
				if (rt2) {
					rt[0] = rt2[0];
					rt[1] = rt2[1];
					rt[2] = rt2[2];
					rt[3] = rt2[3];
				} else {
					rt[0] = 0;
					rt[1] = 0;
					rt[2] = 0;
					rt[3] = 255;
				}
			}

			rt+=4;
			if(rt1 !=NULL){
				rt1+=4;
			}
			if(rt2 !=NULL){
				rt2+=4;
			}
		}
	}
}

static void do_wipe_effect_float(Sequence *seq, float facf0, float facf1, 
				 int x, int y, 
				 float *rect1, 
				 float *rect2, float *out)
{
	WipeZone wipezone;
	WipeVars *wipe = (WipeVars *)seq->effectdata;
	int xo, yo;
	float *rt1, *rt2, *rt;

	precalc_wipe_zone(&wipezone, wipe, x, y);

	rt1 = rect1;
	rt2 = rect2;
	rt = out;

	xo = x;
	yo = y;
	for(y=0;y<yo;y++) {
		for(x=0;x<xo;x++) {
			float check = check_zone(&wipezone,x,y,seq,facf0);
			if (check) {
				if (rt1) {
					rt[0] = rt1[0]*check+ rt2[0]*(1-check);
					rt[1] = rt1[1]*check+ rt2[1]*(1-check);
					rt[2] = rt1[2]*check+ rt2[2]*(1-check);
					rt[3] = rt1[3]*check+ rt2[3]*(1-check);
				} else {
					rt[0] = 0;
					rt[1] = 0;
					rt[2] = 0;
					rt[3] = 1.0;
				}
			} else {
				if (rt2) {
					rt[0] = rt2[0];
					rt[1] = rt2[1];
					rt[2] = rt2[2];
					rt[3] = rt2[3];
				} else {
					rt[0] = 0;
					rt[1] = 0;
					rt[2] = 0;
					rt[3] = 1.0;
				}
			}

			rt+=4;
			if(rt1 !=NULL){
				rt1+=4;
			}
			if(rt2 !=NULL){
				rt2+=4;
			}
		}
	}
}

static void do_wipe_effect(Scene *scene, Sequence *seq, int cfra,
			   float facf0, float facf1, int x, int y, 
			   struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
			   struct ImBuf *ibuf3, struct ImBuf *out)
{
	if (out->rect_float) {
		do_wipe_effect_float(seq,
					 facf0, facf1, x, y,
					 ibuf1->rect_float, ibuf2->rect_float,
					 out->rect_float);
	} else {
		do_wipe_effect_byte(seq,
					facf0, facf1, x, y,
					(unsigned char*) ibuf1->rect, (unsigned char*) ibuf2->rect,
					(unsigned char*) out->rect);
	}
}
/* **********************************************************************
   TRANSFORM
   ********************************************************************** */
static void init_transform_effect(Sequence *seq)
{
	TransformVars *transform;

	if(seq->effectdata)MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct TransformVars), "transformvars");

	transform = (TransformVars *)seq->effectdata;

	transform->ScalexIni = 1.0f;
	transform->ScaleyIni = 1.0f;
	transform->ScalexFin = 1.0f;
	transform->ScalexFin = 1.0f;

	transform->xIni=0.0f;
	transform->xFin=0.0f;
	transform->yIni=0.0f;
	transform->yFin=0.0f;

	transform->rotIni=0.0f;
	transform->rotFin=0.0f;
	
	transform->interpolation=1;
	transform->percent=1;
	transform->uniform_scale=0;
}

static int num_inputs_transform()
{
	return 1;
}

static void free_transform_effect(Sequence *seq)
{
	if(seq->effectdata)MEM_freeN(seq->effectdata);
	seq->effectdata = 0;
}

static void copy_transform_effect(Sequence *dst, Sequence *src)
{
	dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void transform_image(int x, int y, struct ImBuf *ibuf1, struct ImBuf *out, 
							float scale_x, float scale_y, float translate_x, float translate_y, 
							float rotate, int interpolation)
{
	int xo, yo, xi, yi;
	float xt, yt, xr, yr;
	float s,c;

	xo = x;
	yo = y;
	
	// Rotate
	s= sin(rotate);
	c= cos(rotate);

	for (yi = 0; yi < yo; yi++) {
		for (xi = 0; xi < xo; xi++) {

			//translate point
			xt = xi-translate_x;
			yt = yi-translate_y;

			//rotate point with center ref
			xr =  c*xt + s*yt;
			yr = -s*xt + c*yt;

			//scale point with center ref
			xt = xr / scale_x;
			yt = yr / scale_y;

			//undo reference center point 
			xt += (xo / 2.0f);
			yt += (yo / 2.0f);

			//interpolate
			switch(interpolation) {
			case 0:
				neareast_interpolation(ibuf1,out, xt,yt,xi,yi);
				break;
			case 1:
				bilinear_interpolation(ibuf1,out, xt,yt,xi,yi);
				break;
			case 2:
				bicubic_interpolation(ibuf1,out, xt,yt,xi,yi);
				break;
			}
		}
	}
}

static void do_transform(Scene *scene, Sequence *seq, float facf0, int x, int y, 
			  struct ImBuf *ibuf1,struct ImBuf *out)
{
	TransformVars *transform = (TransformVars *)seq->effectdata;
	float scale_x, scale_y, translate_x, translate_y, rotate_radians;
	
	// Scale
	if (transform->uniform_scale) {
		scale_x = scale_y = transform->ScalexIni;
	} else {
		scale_x = transform->ScalexIni;
		scale_y = transform->ScaleyIni;
	}

	// Translate
	if(!transform->percent){
		float rd_s = (scene->r.size/100.0f);

		translate_x = transform->xIni*rd_s+(x/2.0f);
		translate_y = transform->yIni*rd_s+(y/2.0f);
	}else{
		translate_x = x*(transform->xIni/100.0f)+(x/2.0f);
		translate_y = y*(transform->yIni/100.0f)+(y/2.0f);
	}
	
	// Rotate
	rotate_radians = (M_PI*transform->rotIni)/180.0f;

	transform_image(x,y, ibuf1, out, scale_x, scale_y, translate_x, translate_y, rotate_radians, transform->interpolation);
}


static void do_transform_effect(Scene *scene, Sequence *seq,int cfra,
			   float facf0, float facf1, int x, int y, 
			   struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
			   struct ImBuf *ibuf3, struct ImBuf *out)
{
	do_transform(scene, seq, facf0, x, y, ibuf1, out);
}


/* **********************************************************************
   GLOW
   ********************************************************************** */

static void RVBlurBitmap2_byte ( unsigned char* map, int width,int height,
				 float blur,
				 int quality)
/*	MUUUCCH better than the previous blur. */
/*	We do the blurring in two passes which is a whole lot faster. */
/*	I changed the math arount to implement an actual Gaussian */
/*	distribution. */
/* */
/*	Watch out though, it tends to misbehaven with large blur values on */
/*	a small bitmap.  Avoid avoid avoid. */
/*=============================== */
{
	unsigned char*	temp=NULL,*swap;
	float	*filter=NULL;
	int	x,y,i,fx,fy;
	int	index, ix, halfWidth;
	float	fval, k, curColor[3], curColor2[3], weight=0;

	/*	If we're not really blurring, bail out */
	if (blur<=0)
		return;

	/*	Allocate memory for the tempmap and the blur filter matrix */
	temp= MEM_mallocN( (width*height*4), "blurbitmaptemp");
	if (!temp)
		return;

	/*	Allocate memory for the filter elements */
	halfWidth = ((quality+1)*blur);
	filter = (float *)MEM_mallocN(sizeof(float)*halfWidth*2, "blurbitmapfilter");
	if (!filter){
		MEM_freeN (temp);
		return;
	}

	/*	Apparently we're calculating a bell curve */
	/*	based on the standard deviation (or radius) */
	/*	This code is based on an example */
	/*	posted to comp.graphics.algorithms by */
	/*	Blancmange (bmange@airdmhor.gen.nz) */

	k = -1.0/(2.0*3.14159*blur*blur);
	fval=0;
	for (ix = 0;ix< halfWidth;ix++){
		weight = (float)exp(k*(ix*ix));
		filter[halfWidth - ix] = weight;
		filter[halfWidth + ix] = weight;
	}
	filter[0] = weight;

	/*	Normalize the array */
	fval=0;
	for (ix = 0;ix< halfWidth*2;ix++)
		fval+=filter[ix];

	for (ix = 0;ix< halfWidth*2;ix++)
		filter[ix]/=fval;

	/*	Blur the rows */
	for (y=0;y<height;y++){
		/*	Do the left & right strips */
		for (x=0;x<halfWidth;x++){
			index=(x+y*width)*4;
			fx=0;
			curColor[0]=curColor[1]=curColor[2]=0;
			curColor2[0]=curColor2[1]=curColor2[2]=0;

			for (i=x-halfWidth;i<x+halfWidth;i++){
				if ((i>=0)&&(i<width)){
					curColor[0]+=map[(i+y*width)*4+GlowR]*filter[fx];
					curColor[1]+=map[(i+y*width)*4+GlowG]*filter[fx];
					curColor[2]+=map[(i+y*width)*4+GlowB]*filter[fx];

					curColor2[0]+=map[(width-1-i+y*width)*4+GlowR] *
						filter[fx];
					curColor2[1]+=map[(width-1-i+y*width)*4+GlowG] *
						filter[fx];
					curColor2[2]+=map[(width-1-i+y*width)*4+GlowB] *
						filter[fx];
				}
				fx++;
			}
			temp[index+GlowR]=curColor[0];
			temp[index+GlowG]=curColor[1];
			temp[index+GlowB]=curColor[2];

			temp[((width-1-x+y*width)*4)+GlowR]=curColor2[0];
			temp[((width-1-x+y*width)*4)+GlowG]=curColor2[1];
			temp[((width-1-x+y*width)*4)+GlowB]=curColor2[2];

		}
		/*	Do the main body */
		for (x=halfWidth;x<width-halfWidth;x++){
			index=(x+y*width)*4;
			fx=0;
			curColor[0]=curColor[1]=curColor[2]=0;
			for (i=x-halfWidth;i<x+halfWidth;i++){
				curColor[0]+=map[(i+y*width)*4+GlowR]*filter[fx];
				curColor[1]+=map[(i+y*width)*4+GlowG]*filter[fx];
				curColor[2]+=map[(i+y*width)*4+GlowB]*filter[fx];
				fx++;
			}
			temp[index+GlowR]=curColor[0];
			temp[index+GlowG]=curColor[1];
			temp[index+GlowB]=curColor[2];
		}
	}

	/*	Swap buffers */
	swap=temp;temp=map;map=swap;


	/*	Blur the columns */
	for (x=0;x<width;x++){
		/*	Do the top & bottom strips */
		for (y=0;y<halfWidth;y++){
			index=(x+y*width)*4;
			fy=0;
			curColor[0]=curColor[1]=curColor[2]=0;
			curColor2[0]=curColor2[1]=curColor2[2]=0;
			for (i=y-halfWidth;i<y+halfWidth;i++){
				if ((i>=0)&&(i<height)){
					/*	Bottom */
					curColor[0]+=map[(x+i*width)*4+GlowR]*filter[fy];
					curColor[1]+=map[(x+i*width)*4+GlowG]*filter[fy];
					curColor[2]+=map[(x+i*width)*4+GlowB]*filter[fy];

					/*	Top */
					curColor2[0]+=map[(x+(height-1-i)*width) *
						4+GlowR]*filter[fy];
					curColor2[1]+=map[(x+(height-1-i)*width) *
						4+GlowG]*filter[fy];
					curColor2[2]+=map[(x+(height-1-i)*width) *
						4+GlowB]*filter[fy];
				}
				fy++;
			}
			temp[index+GlowR]=curColor[0];
			temp[index+GlowG]=curColor[1];
			temp[index+GlowB]=curColor[2];
			temp[((x+(height-1-y)*width)*4)+GlowR]=curColor2[0];
			temp[((x+(height-1-y)*width)*4)+GlowG]=curColor2[1];
			temp[((x+(height-1-y)*width)*4)+GlowB]=curColor2[2];
		}
		/*	Do the main body */
		for (y=halfWidth;y<height-halfWidth;y++){
			index=(x+y*width)*4;
			fy=0;
			curColor[0]=curColor[1]=curColor[2]=0;
			for (i=y-halfWidth;i<y+halfWidth;i++){
				curColor[0]+=map[(x+i*width)*4+GlowR]*filter[fy];
				curColor[1]+=map[(x+i*width)*4+GlowG]*filter[fy];
				curColor[2]+=map[(x+i*width)*4+GlowB]*filter[fy];
				fy++;
			}
			temp[index+GlowR]=curColor[0];
			temp[index+GlowG]=curColor[1];
			temp[index+GlowB]=curColor[2];
		}
	}


	/*	Swap buffers */
	swap=temp;temp=map;map=swap;

	/*	Tidy up	 */
	MEM_freeN (filter);
	MEM_freeN (temp);
}

static void RVBlurBitmap2_float ( float* map, int width,int height,
				  float blur,
				  int quality)
/*	MUUUCCH better than the previous blur. */
/*	We do the blurring in two passes which is a whole lot faster. */
/*	I changed the math arount to implement an actual Gaussian */
/*	distribution. */
/* */
/*	Watch out though, it tends to misbehaven with large blur values on */
/*	a small bitmap.  Avoid avoid avoid. */
/*=============================== */
{
	float*	temp=NULL,*swap;
	float	*filter=NULL;
	int	x,y,i,fx,fy;
	int	index, ix, halfWidth;
	float	fval, k, curColor[3], curColor2[3], weight=0;

	/*	If we're not really blurring, bail out */
	if (blur<=0)
		return;

	/*	Allocate memory for the tempmap and the blur filter matrix */
	temp= MEM_mallocN( (width*height*4*sizeof(float)), "blurbitmaptemp");
	if (!temp)
		return;

	/*	Allocate memory for the filter elements */
	halfWidth = ((quality+1)*blur);
	filter = (float *)MEM_mallocN(sizeof(float)*halfWidth*2, "blurbitmapfilter");
	if (!filter){
		MEM_freeN (temp);
		return;
	}

	/*	Apparently we're calculating a bell curve */
	/*	based on the standard deviation (or radius) */
	/*	This code is based on an example */
	/*	posted to comp.graphics.algorithms by */
	/*	Blancmange (bmange@airdmhor.gen.nz) */

	k = -1.0/(2.0*3.14159*blur*blur);
	fval=0;
	for (ix = 0;ix< halfWidth;ix++){
		weight = (float)exp(k*(ix*ix));
		filter[halfWidth - ix] = weight;
		filter[halfWidth + ix] = weight;
	}
	filter[0] = weight;

	/*	Normalize the array */
	fval=0;
	for (ix = 0;ix< halfWidth*2;ix++)
		fval+=filter[ix];

	for (ix = 0;ix< halfWidth*2;ix++)
		filter[ix]/=fval;

	/*	Blur the rows */
	for (y=0;y<height;y++){
		/*	Do the left & right strips */
		for (x=0;x<halfWidth;x++){
			index=(x+y*width)*4;
			fx=0;
			curColor[0]=curColor[1]=curColor[2]=0.0f;
			curColor2[0]=curColor2[1]=curColor2[2]=0.0f;

			for (i=x-halfWidth;i<x+halfWidth;i++){
				if ((i>=0)&&(i<width)){
					curColor[0]+=map[(i+y*width)*4+GlowR]*filter[fx];
					curColor[1]+=map[(i+y*width)*4+GlowG]*filter[fx];
					curColor[2]+=map[(i+y*width)*4+GlowB]*filter[fx];

					curColor2[0]+=map[(width-1-i+y*width)*4+GlowR] *
						filter[fx];
					curColor2[1]+=map[(width-1-i+y*width)*4+GlowG] *
						filter[fx];
					curColor2[2]+=map[(width-1-i+y*width)*4+GlowB] *
						filter[fx];
				}
				fx++;
			}
			temp[index+GlowR]=curColor[0];
			temp[index+GlowG]=curColor[1];
			temp[index+GlowB]=curColor[2];

			temp[((width-1-x+y*width)*4)+GlowR]=curColor2[0];
			temp[((width-1-x+y*width)*4)+GlowG]=curColor2[1];
			temp[((width-1-x+y*width)*4)+GlowB]=curColor2[2];

		}
		/*	Do the main body */
		for (x=halfWidth;x<width-halfWidth;x++){
			index=(x+y*width)*4;
			fx=0;
			curColor[0]=curColor[1]=curColor[2]=0;
			for (i=x-halfWidth;i<x+halfWidth;i++){
				curColor[0]+=map[(i+y*width)*4+GlowR]*filter[fx];
				curColor[1]+=map[(i+y*width)*4+GlowG]*filter[fx];
				curColor[2]+=map[(i+y*width)*4+GlowB]*filter[fx];
				fx++;
			}
			temp[index+GlowR]=curColor[0];
			temp[index+GlowG]=curColor[1];
			temp[index+GlowB]=curColor[2];
		}
	}

	/*	Swap buffers */
	swap=temp;temp=map;map=swap;


	/*	Blur the columns */
	for (x=0;x<width;x++){
		/*	Do the top & bottom strips */
		for (y=0;y<halfWidth;y++){
			index=(x+y*width)*4;
			fy=0;
			curColor[0]=curColor[1]=curColor[2]=0;
			curColor2[0]=curColor2[1]=curColor2[2]=0;
			for (i=y-halfWidth;i<y+halfWidth;i++){
				if ((i>=0)&&(i<height)){
					/*	Bottom */
					curColor[0]+=map[(x+i*width)*4+GlowR]*filter[fy];
					curColor[1]+=map[(x+i*width)*4+GlowG]*filter[fy];
					curColor[2]+=map[(x+i*width)*4+GlowB]*filter[fy];

					/*	Top */
					curColor2[0]+=map[(x+(height-1-i)*width) *
						4+GlowR]*filter[fy];
					curColor2[1]+=map[(x+(height-1-i)*width) *
						4+GlowG]*filter[fy];
					curColor2[2]+=map[(x+(height-1-i)*width) *
						4+GlowB]*filter[fy];
				}
				fy++;
			}
			temp[index+GlowR]=curColor[0];
			temp[index+GlowG]=curColor[1];
			temp[index+GlowB]=curColor[2];
			temp[((x+(height-1-y)*width)*4)+GlowR]=curColor2[0];
			temp[((x+(height-1-y)*width)*4)+GlowG]=curColor2[1];
			temp[((x+(height-1-y)*width)*4)+GlowB]=curColor2[2];
		}
		/*	Do the main body */
		for (y=halfWidth;y<height-halfWidth;y++){
			index=(x+y*width)*4;
			fy=0;
			curColor[0]=curColor[1]=curColor[2]=0;
			for (i=y-halfWidth;i<y+halfWidth;i++){
				curColor[0]+=map[(x+i*width)*4+GlowR]*filter[fy];
				curColor[1]+=map[(x+i*width)*4+GlowG]*filter[fy];
				curColor[2]+=map[(x+i*width)*4+GlowB]*filter[fy];
				fy++;
			}
			temp[index+GlowR]=curColor[0];
			temp[index+GlowG]=curColor[1];
			temp[index+GlowB]=curColor[2];
		}
	}


	/*	Swap buffers */
	swap=temp;temp=map;map=swap;

	/*	Tidy up	 */
	MEM_freeN (filter);
	MEM_freeN (temp);
}


/*	Adds two bitmaps and puts the results into a third map. */
/*	C must have been previously allocated but it may be A or B. */
/*	We clamp values to 255 to prevent weirdness */
/*=============================== */
static void RVAddBitmaps_byte (unsigned char* a, unsigned char* b, unsigned char* c, int width, int height)
{
	int	x,y,index;

	for (y=0;y<height;y++){
		for (x=0;x<width;x++){
			index=(x+y*width)*4;
			c[index+GlowR]=MIN2(255,a[index+GlowR]+b[index+GlowR]);
			c[index+GlowG]=MIN2(255,a[index+GlowG]+b[index+GlowG]);
			c[index+GlowB]=MIN2(255,a[index+GlowB]+b[index+GlowB]);
			c[index+GlowA]=MIN2(255,a[index+GlowA]+b[index+GlowA]);
		}
	}
}

static void RVAddBitmaps_float (float* a, float* b, float* c, 
				int width, int height)
{
	int	x,y,index;

	for (y=0;y<height;y++){
		for (x=0;x<width;x++){
			index=(x+y*width)*4;
			c[index+GlowR]=MIN2(1.0,a[index+GlowR]+b[index+GlowR]);
			c[index+GlowG]=MIN2(1.0,a[index+GlowG]+b[index+GlowG]);
			c[index+GlowB]=MIN2(1.0,a[index+GlowB]+b[index+GlowB]);
			c[index+GlowA]=MIN2(1.0,a[index+GlowA]+b[index+GlowA]);
		}
	}
}

/*	For each pixel whose total luminance exceeds the threshold, */
/*	Multiply it's value by BOOST and add it to the output map */
static void RVIsolateHighlights_byte (unsigned char* in, unsigned char* out, 
					  int width, int height, int threshold, 
					  float boost, float clamp)
{
	int x,y,index;
	int	intensity;


	for(y=0;y< height;y++) {
		for (x=0;x< width;x++) {
			 index= (x+y*width)*4;

			/*	Isolate the intensity */
			intensity=(in[index+GlowR]+in[index+GlowG]+in[index+GlowB]-threshold);
			if (intensity>0){
				out[index+GlowR]=MIN2(255*clamp, (in[index+GlowR]*boost*intensity)/255);
				out[index+GlowG]=MIN2(255*clamp, (in[index+GlowG]*boost*intensity)/255);
				out[index+GlowB]=MIN2(255*clamp, (in[index+GlowB]*boost*intensity)/255);
				out[index+GlowA]=MIN2(255*clamp, (in[index+GlowA]*boost*intensity)/255);
			} else{
				out[index+GlowR]=0;
				out[index+GlowG]=0;
				out[index+GlowB]=0;
				out[index+GlowA]=0;
			}
		}
	}
}

static void RVIsolateHighlights_float (float* in, float* out, 
					  int width, int height, float threshold, 
					  float boost, float clamp)
{
	int x,y,index;
	float	intensity;


	for(y=0;y< height;y++) {
		for (x=0;x< width;x++) {
			 index= (x+y*width)*4;

			/*	Isolate the intensity */
			intensity=(in[index+GlowR]+in[index+GlowG]+in[index+GlowB]-threshold);
			if (intensity>0){
				out[index+GlowR]=MIN2(clamp, (in[index+GlowR]*boost*intensity));
				out[index+GlowG]=MIN2(clamp, (in[index+GlowG]*boost*intensity));
				out[index+GlowB]=MIN2(clamp, (in[index+GlowB]*boost*intensity));
				out[index+GlowA]=MIN2(clamp, (in[index+GlowA]*boost*intensity));
			} else{
				out[index+GlowR]=0;
				out[index+GlowG]=0;
				out[index+GlowB]=0;
				out[index+GlowA]=0;
			}
		}
	}
}

static void init_glow_effect(Sequence *seq)
{
	GlowVars *glow;

	if(seq->effectdata)MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct GlowVars), "glowvars");

	glow = (GlowVars *)seq->effectdata;
	glow->fMini = 0.25;
	glow->fClamp = 1.0;
	glow->fBoost = 0.5;
	glow->dDist = 3.0;
	glow->dQuality = 3;
	glow->bNoComp = 0;
}

static int num_inputs_glow()
{
	return 1;
}

static void free_glow_effect(Sequence *seq)
{
	if(seq->effectdata)MEM_freeN(seq->effectdata);
	seq->effectdata = 0;
}

static void copy_glow_effect(Sequence *dst, Sequence *src)
{
	dst->effectdata = MEM_dupallocN(src->effectdata);
}

//void do_glow_effect(Cast *cast, float facf0, float facf1, int xo, int yo, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *outbuf, ImBuf *use)
static void do_glow_effect_byte(Sequence *seq, float facf0, float facf1, 
				int x, int y, char *rect1, 
				char *rect2, char *out)
{
	unsigned char *outbuf=(unsigned char *)out;
	unsigned char *inbuf=(unsigned char *)rect1;
	GlowVars *glow = (GlowVars *)seq->effectdata;
	int size= 100; // renderdata XXX
	
	RVIsolateHighlights_byte(inbuf, outbuf , x, y, glow->fMini*765, glow->fBoost * facf0, glow->fClamp);
	RVBlurBitmap2_byte (outbuf, x, y, glow->dDist * (size / 100.0f),glow->dQuality);
	if (!glow->bNoComp)
		RVAddBitmaps_byte (inbuf , outbuf, outbuf, x, y);
}

static void do_glow_effect_float(Sequence *seq, float facf0, float facf1, 
				 int x, int y, 
				 float *rect1, float *rect2, float *out)
{
	float *outbuf = out;
	float *inbuf = rect1;
	GlowVars *glow = (GlowVars *)seq->effectdata;
	int size= 100; // renderdata XXX

	RVIsolateHighlights_float(inbuf, outbuf , x, y, glow->fMini*3.0f, glow->fBoost * facf0, glow->fClamp);
	RVBlurBitmap2_float (outbuf, x, y, glow->dDist * (size / 100.0f),glow->dQuality);
	if (!glow->bNoComp)
		RVAddBitmaps_float (inbuf , outbuf, outbuf, x, y);
}

static void do_glow_effect(Scene *scene, Sequence *seq, int cfra,
			   float facf0, float facf1, int x, int y, 
			   struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
			   struct ImBuf *ibuf3, struct ImBuf *out)
{
	if (out->rect_float) {
		do_glow_effect_float(seq,
					 facf0, facf1, x, y,
					 ibuf1->rect_float, ibuf2->rect_float,
					 out->rect_float);
	} else {
		do_glow_effect_byte(seq,
					facf0, facf1, x, y,
					(char*) ibuf1->rect, (char*) ibuf2->rect,
					(char*) out->rect);
	}
}

/* **********************************************************************
   SOLID COLOR
   ********************************************************************** */

static void init_solid_color(Sequence *seq)
{
	SolidColorVars *cv;
	
	if(seq->effectdata)MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct SolidColorVars), "solidcolor");
	
	cv = (SolidColorVars *)seq->effectdata;
	cv->col[0] = cv->col[1] = cv->col[2] = 0.5;
}

static int num_inputs_color()
{
	return 0;
}

static void free_solid_color(Sequence *seq)
{
	if(seq->effectdata)MEM_freeN(seq->effectdata);
	seq->effectdata = 0;
}

static void copy_solid_color(Sequence *dst, Sequence *src)
{
	dst->effectdata = MEM_dupallocN(src->effectdata);
}

static int early_out_color(struct Sequence *seq,
			   float facf0, float facf1)
{
	return -1;
}

static void do_solid_color(Scene *scene, Sequence *seq, int cfra,
			   float facf0, float facf1, int x, int y, 
			   struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
			   struct ImBuf *ibuf3, struct ImBuf *out)
{
	SolidColorVars *cv = (SolidColorVars *)seq->effectdata;

	unsigned char *rect;
	float *rect_float;

	if (out->rect) {
		unsigned char col0[3];
		unsigned char col1[3];

		col0[0] = facf0 * cv->col[0] * 255;
		col0[1] = facf0 * cv->col[1] * 255;
		col0[2] = facf0 * cv->col[2] * 255;

		col1[0] = facf1 * cv->col[0] * 255;
		col1[1] = facf1 * cv->col[1] * 255;
		col1[2] = facf1 * cv->col[2] * 255;

		rect = (unsigned char *)out->rect;
		
		for(y=0; y<out->y; y++) {	
			for(x=0; x<out->x; x++, rect+=4) {
				rect[0]= col0[0];
				rect[1]= col0[1];
				rect[2]= col0[2];
				rect[3]= 255;
			}
			y++;
			if (y<out->y) {
				for(x=0; x<out->x; x++, rect+=4) {
					rect[0]= col1[0];
					rect[1]= col1[1];
					rect[2]= col1[2];
					rect[3]= 255;
				}	
			}
		}

	} else if (out->rect_float) {
		float col0[3];
		float col1[3];

		col0[0] = facf0 * cv->col[0];
		col0[1] = facf0 * cv->col[1];
		col0[2] = facf0 * cv->col[2];

		col1[0] = facf1 * cv->col[0];
		col1[1] = facf1 * cv->col[1];
		col1[2] = facf1 * cv->col[2];

		rect_float = out->rect_float;
		
		for(y=0; y<out->y; y++) {	
			for(x=0; x<out->x; x++, rect_float+=4) {
				rect_float[0]= col0[0];
				rect_float[1]= col0[1];
				rect_float[2]= col0[2];
				rect_float[3]= 1.0;
			}
			y++;
			if (y<out->y) {
				for(x=0; x<out->x; x++, rect_float+=4) {
					rect_float[0]= col1[0];
					rect_float[1]= col1[1];
					rect_float[2]= col1[2];
					rect_float[3]= 1.0;
				}
			}
		}
	}
}

/* **********************************************************************
   SPEED
   ********************************************************************** */
static void init_speed_effect(Sequence *seq)
{
	SpeedControlVars * v;

	if(seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct SpeedControlVars), 
					  "speedcontrolvars");

	v = (SpeedControlVars *)seq->effectdata;
	v->globalSpeed = 1.0;
	v->frameMap = 0;
	v->flags = SEQ_SPEED_COMPRESS_IPO_Y;
	v->length = 0;
}

static void load_speed_effect(Sequence * seq)
{
	SpeedControlVars * v = (SpeedControlVars *)seq->effectdata;

	v->frameMap = 0;
	v->length = 0;
}

static int num_inputs_speed()
{
	return 1;
}

static void free_speed_effect(Sequence *seq)
{
	SpeedControlVars * v = (SpeedControlVars *)seq->effectdata;
	if(v->frameMap) MEM_freeN(v->frameMap);
	if(seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = 0;
}

static void copy_speed_effect(Sequence *dst, Sequence *src)
{
	SpeedControlVars * v;
	dst->effectdata = MEM_dupallocN(src->effectdata);
	v = (SpeedControlVars *)dst->effectdata;
	v->frameMap = 0;
	v->length = 0;
}

static int early_out_speed(struct Sequence *seq,
			  float facf0, float facf1)
{
	return 1;
}

static void store_icu_yrange_speed(struct Sequence * seq,
				   short adrcode, float * ymin, float * ymax)
{
	SpeedControlVars * v = (SpeedControlVars *)seq->effectdata;

	/* if not already done, load / initialize data */
	get_sequence_effect(seq);

	if ((v->flags & SEQ_SPEED_INTEGRATE) != 0) {
		*ymin = -100.0;
		*ymax = 100.0;
	} else {
		if (v->flags & SEQ_SPEED_COMPRESS_IPO_Y) {
			*ymin = 0.0;
			*ymax = 1.0;
		} else {
			*ymin = 0.0;
			*ymax = seq->len;
		}
	}	
}
void sequence_effect_speed_rebuild_map(Scene *scene, Sequence * seq, int force)
{
	float ctime, div;
	int cfra;
	float fallback_fac;
	SpeedControlVars * v = (SpeedControlVars *)seq->effectdata;
	FCurve *fcu= NULL;

	/* if not already done, load / initialize data */
	get_sequence_effect(seq);

	if (!(force || seq->len != v->length || !v->frameMap)) {
		return;
	}
	if (!seq->seq1) { /* make coverity happy and check for (CID 598)
						 input strip ... */
		return;
	}

	/* XXX - new in 2.5x. should we use the animation system this way?
	 * The fcurve is needed because many frames need evaluating at once - campbell */
	fcu= id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "speed_fader", 0);


	if (!v->frameMap || v->length != seq->len) {
		if (v->frameMap) MEM_freeN(v->frameMap);

		v->length = seq->len;

		v->frameMap = MEM_callocN(sizeof(float) * v->length, 
					  "speedcontrol frameMap");
	}

	fallback_fac = 1.0;
	
	/* if there is no IPO, try to make retiming easy by stretching the
	   strip */
	// XXX old animation system - seq
	if (!fcu && seq->seq1->enddisp != seq->seq1->start && seq->seq1->len != 0) {
		fallback_fac = (float) seq->seq1->len / 
			(float) (seq->seq1->enddisp - seq->seq1->start);
		/* FIXME: this strip stretching gets screwed by stripdata
		   handling one layer up.
		   
		   So it currently works by enlarging, never by shrinking!

		   (IPOs still work, if used correctly)
		*/
		if (fallback_fac > 1.0) {
			fallback_fac = 1.0;
		}
	}

	if ((v->flags & SEQ_SPEED_INTEGRATE) != 0) {
		float cursor = 0;
		float facf;

		v->frameMap[0] = 0;
		v->lastValidFrame = 0;

		for (cfra = 1; cfra < v->length; cfra++) {
			if(fcu) {
							   if((seq->flag & SEQ_IPO_FRAME_LOCKED) != 0) {
									   ctime = seq->startdisp + cfra;
					   div = 1.0;
							   } else {
									   ctime= cfra;
									   div= v->length / 100.0f;
									   if(div==0.0) return;
							   }
			       
				   facf = evaluate_fcurve(fcu, ctime/div);
			} else {
				facf = fallback_fac;
			}
			facf *= v->globalSpeed;

			cursor += facf;

			if (cursor >= v->length) {
				v->frameMap[cfra] = v->length - 1;
			} else {
				v->frameMap[cfra] = cursor;
				v->lastValidFrame = cfra;
			}
		}
	} else {
		float facf;

		v->lastValidFrame = 0;
		for (cfra = 0; cfra < v->length; cfra++) {

			if(fcu) {
							   if((seq->flag & SEQ_IPO_FRAME_LOCKED) != 0) {
									   ctime = seq->startdisp + cfra;
									   div = 1.0;
							   } else {
									   ctime= cfra;
									   div= v->length / 100.0f;
									   if(div==0.0) return;
							   }
		
				   facf = evaluate_fcurve(fcu, ctime / div);
				   if (v->flags & SEQ_SPEED_COMPRESS_IPO_Y) {
					   facf *= v->length;
				   }
			}
			
			if (!fcu) {
				facf = (float) cfra * fallback_fac;
			}
			facf *= v->globalSpeed;
			if (facf >= v->length) {
				facf = v->length - 1;
			} else {
				v->lastValidFrame = cfra;
			}
			v->frameMap[cfra] = facf;
		}
	}
}

/*
  simply reuse do_cross_effect for blending...

static void do_speed_effect(Sequence * seq,int cfra,
			   float facf0, float facf1, int x, int y, 
			   struct ImBuf *ibuf1, struct ImBuf *ibuf2, 
			   struct ImBuf *ibuf3, struct ImBuf *out)
{

}
*/


/* **********************************************************************
   sequence effect factory
   ********************************************************************** */


static void init_noop(struct Sequence *seq)
{

}

static void load_noop(struct Sequence *seq)
{

}

static void init_plugin_noop(struct Sequence *seq, const char * fname)
{

}

static void free_noop(struct Sequence *seq)
{

}

static int num_inputs_default()
{
	return 2;
}

static int early_out_noop(struct Sequence *seq,
			  float facf0, float facf1)
{
	return 0;
}

static int early_out_fade(struct Sequence *seq,
			  float facf0, float facf1)
{
	if (facf0 == 0.0 && facf1 == 0.0) {
		return 1;
	} else if (facf0 == 1.0 && facf1 == 1.0) {
		return 2;
	}
	return 0;
}

static int early_out_mul_input2(struct Sequence *seq,
				float facf0, float facf1)
{
	if (facf0 == 0.0 && facf1 == 0.0) {
		return 1;
	}
	return 0;
}

static void store_icu_yrange_noop(struct Sequence * seq,
				  short adrcode, float * ymin, float * ymax)
{
	/* defaults are fine */
}

static void get_default_fac_noop(struct Sequence *seq, int cfra,
				 float * facf0, float * facf1)
{
	*facf0 = *facf1 = 1.0;
}

static void get_default_fac_fade(struct Sequence *seq, int cfra,
				 float * facf0, float * facf1)
{
	*facf0 = (float)(cfra - seq->startdisp);
	*facf1 = (float)(*facf0 + 0.5);
	*facf0 /= seq->len;
	*facf1 /= seq->len;
}

static void do_overdrop_effect(Scene *scene, Sequence *seq, int cfra,
				   float fac, float facf, 
				   int x, int y, struct ImBuf * ibuf1, 
				   struct ImBuf * ibuf2, 
				   struct ImBuf * ibuf3, 
				   struct ImBuf * out)
{
	do_drop_effect(scene, seq, cfra, fac, facf, x, y, 
			   ibuf1, ibuf2, ibuf3, out);
	do_alphaover_effect(scene, seq, cfra, fac, facf, x, y, 
				ibuf1, ibuf2, ibuf3, out);
}

static struct SeqEffectHandle get_sequence_effect_impl(int seq_type)
{
	struct SeqEffectHandle rval;
	int sequence_type = seq_type;

	rval.init = init_noop;
	rval.init_plugin = init_plugin_noop;
	rval.num_inputs = num_inputs_default;
	rval.load = load_noop;
	rval.free = free_noop;
	rval.early_out = early_out_noop;
	rval.get_default_fac = get_default_fac_noop;
	rval.store_icu_yrange = store_icu_yrange_noop;
	rval.execute = NULL;
	rval.copy = NULL;

	switch (sequence_type) {
	case SEQ_CROSS:
		rval.execute = do_cross_effect;
		rval.early_out = early_out_fade;
		rval.get_default_fac = get_default_fac_fade;
		break;
	case SEQ_GAMCROSS:
		rval.init = init_gammacross;
		rval.load = load_gammacross;
		rval.free = free_gammacross;
		rval.early_out = early_out_fade;
		rval.get_default_fac = get_default_fac_fade;
		rval.execute = do_gammacross_effect;
		break;
	case SEQ_ADD:
		rval.execute = do_add_effect;
		rval.early_out = early_out_mul_input2;
		break;
	case SEQ_SUB:
		rval.execute = do_sub_effect;
		rval.early_out = early_out_mul_input2;
		break;
	case SEQ_MUL:
		rval.execute = do_mul_effect;
		rval.early_out = early_out_mul_input2;
		break;
	case SEQ_ALPHAOVER:
		rval.init = init_alpha_over_or_under;
		rval.execute = do_alphaover_effect;
		break;
	case SEQ_OVERDROP:
		rval.execute = do_overdrop_effect;
		break;
	case SEQ_ALPHAUNDER:
		rval.init = init_alpha_over_or_under;
		rval.execute = do_alphaunder_effect;
		break;
	case SEQ_WIPE:
		rval.init = init_wipe_effect;
		rval.num_inputs = num_inputs_wipe;
		rval.free = free_wipe_effect;
		rval.copy = copy_wipe_effect;
		rval.early_out = early_out_fade;
		rval.get_default_fac = get_default_fac_fade;
		rval.execute = do_wipe_effect;
		break;
	case SEQ_GLOW:
		rval.init = init_glow_effect;
		rval.num_inputs = num_inputs_glow;
		rval.free = free_glow_effect;
		rval.copy = copy_glow_effect;
		rval.execute = do_glow_effect;
		break;
	case SEQ_TRANSFORM:
		rval.init = init_transform_effect;
		rval.num_inputs = num_inputs_transform;
		rval.free = free_transform_effect;
		rval.copy = copy_transform_effect;
		rval.execute = do_transform_effect;
		break;
	case SEQ_SPEED:
		rval.init = init_speed_effect;
		rval.num_inputs = num_inputs_speed;
		rval.load = load_speed_effect;
		rval.free = free_speed_effect;
		rval.copy = copy_speed_effect;
		rval.execute = do_cross_effect;
		rval.early_out = early_out_speed;
		rval.store_icu_yrange = store_icu_yrange_speed;
		break;
	case SEQ_COLOR:
		rval.init = init_solid_color;
		rval.num_inputs = num_inputs_color;
		rval.early_out = early_out_color;
		rval.free = free_solid_color;
		rval.copy = copy_solid_color;
		rval.execute = do_solid_color;
		break;
	case SEQ_PLUGIN:
		rval.init_plugin = init_plugin;
		rval.num_inputs = num_inputs_plugin;
		rval.load = load_plugin;
		rval.free = free_plugin;
		rval.copy = copy_plugin;
		rval.execute = do_plugin_effect;
		rval.early_out = do_plugin_early_out;
		rval.get_default_fac = get_default_fac_fade;
		break;
	}

	return rval;
}


struct SeqEffectHandle get_sequence_effect(Sequence * seq)
{
	struct SeqEffectHandle rval;

	memset(&rval, 0, sizeof(struct SeqEffectHandle));

	if (seq->type & SEQ_EFFECT) {
		rval = get_sequence_effect_impl(seq->type);
		if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
			rval.load(seq);
			seq->flag &= ~SEQ_EFFECT_NOT_LOADED;
		}
	}

	return rval;
}

struct SeqEffectHandle get_sequence_blend(Sequence * seq)
{
	struct SeqEffectHandle rval;

	memset(&rval, 0, sizeof(struct SeqEffectHandle));

	if (seq->blend_mode != 0) {
		rval = get_sequence_effect_impl(seq->blend_mode);
		if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
			rval.load(seq);
			seq->flag &= ~SEQ_EFFECT_NOT_LOADED;
		}
	}

	return rval;
}

int get_sequence_effect_num_inputs(int seq_type)
{
	struct SeqEffectHandle rval = get_sequence_effect_impl(seq_type);

	int cnt = rval.num_inputs();
	if (rval.execute) {
		return cnt;
	}
	return 0;
}
