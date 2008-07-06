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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_ipo_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_view3d_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BIF_editsound.h"
#include "BIF_editseq.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BIF_interface.h"
#include "BIF_renderwin.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_sequence.h"
#include "BSE_seqeffects.h"

#include "RE_pipeline.h"		// talks to entire render API

#include "blendef.h"

#include "BLI_threads.h"
#include <pthread.h>

#ifdef WIN32
#define snprintf _snprintf
#endif

int seqrectx, seqrecty;

/* **********************************************************************
   alloc / free functions
   ********************************************************************** */

void free_tstripdata(int len, TStripElem *se)
{
	TStripElem *seo;
	int a;

	seo= se;
	if (!se) {
		return;
	}

	for(a=0; a<len; a++, se++) {
		if(se->ibuf) {
			IMB_freeImBuf(se->ibuf);
			se->ibuf = 0;
		}
		if(se->ibuf_comp) {
			IMB_freeImBuf(se->ibuf_comp);
			se->ibuf_comp = 0;
		}
	}

	MEM_freeN(seo);

}

void free_strip(Strip *strip)
{
	strip->us--;
	if(strip->us>0) return;
	if(strip->us<0) {
		printf("error: negative users in strip\n");
		return;
	}

	if (strip->stripdata) {
		MEM_freeN(strip->stripdata);
	}

	if (strip->proxy) {
		MEM_freeN(strip->proxy);
	}
	if (strip->crop) {
		MEM_freeN(strip->crop);
	}
	if (strip->transform) {
		MEM_freeN(strip->transform);
	}
	if (strip->color_balance) {
		MEM_freeN(strip->color_balance);
	}

	free_tstripdata(strip->len, strip->tstripdata);
	free_tstripdata(strip->endstill, strip->tstripdata_endstill);
	free_tstripdata(strip->startstill, strip->tstripdata_startstill);

	if(strip->ibuf_startstill) {
		IMB_freeImBuf(strip->ibuf_startstill);
		strip->ibuf_startstill = 0;
	}

	if(strip->ibuf_endstill) {
		IMB_freeImBuf(strip->ibuf_endstill);
		strip->ibuf_endstill = 0;
	}

	MEM_freeN(strip);
}

void new_tstripdata(Sequence *seq)
{
	if(seq->strip) {
		free_tstripdata(seq->strip->len, seq->strip->tstripdata);
		free_tstripdata(seq->strip->endstill, 
				seq->strip->tstripdata_endstill);
		free_tstripdata(seq->strip->startstill, 
				seq->strip->tstripdata_startstill);

		seq->strip->tstripdata= 0;
		seq->strip->tstripdata_endstill= 0;
		seq->strip->tstripdata_startstill= 0;

		if(seq->strip->ibuf_startstill) {
			IMB_freeImBuf(seq->strip->ibuf_startstill);
			seq->strip->ibuf_startstill = 0;
		}

		if(seq->strip->ibuf_endstill) {
			IMB_freeImBuf(seq->strip->ibuf_endstill);
			seq->strip->ibuf_endstill = 0;
		}

		seq->strip->len= seq->len;
	}
}

void free_sequence(Sequence *seq)
{
	Sequence *last_seq = get_last_seq();

	if(seq->strip) free_strip(seq->strip);

	if(seq->anim) IMB_free_anim(seq->anim);
	if(seq->hdaudio) sound_close_hdaudio(seq->hdaudio);

	if (seq->type & SEQ_EFFECT) {
		struct SeqEffectHandle sh = get_sequence_effect(seq);

		sh.free(seq);
	}

	if(seq==last_seq) set_last_seq(NULL);

	MEM_freeN(seq);
}

/*
  **********************************************************************
  * build_seqar
  **********************************************************************
  * Build a complete array of _all_ sequencies (including those
  * in metastrips!)
  **********************************************************************
*/

static void do_seq_count(ListBase *seqbase, int *totseq)
{
	Sequence *seq;

	seq= seqbase->first;
	while(seq) {
		(*totseq)++;
		if(seq->seqbase.first) do_seq_count(&seq->seqbase, totseq);
		seq= seq->next;
	}
}

static void do_build_seqar(ListBase *seqbase, Sequence ***seqar, int depth)
{
	Sequence *seq;

	seq= seqbase->first;
	while(seq) {
		seq->depth= depth;
		if(seq->seqbase.first) do_build_seqar(&seq->seqbase, seqar, depth+1);
		**seqar= seq;
		(*seqar)++;
		seq= seq->next;
	}
}

void build_seqar(ListBase *seqbase, Sequence  ***seqar, int *totseq)
{
	Sequence **tseqar;

	*totseq= 0;
	do_seq_count(seqbase, totseq);

	if(*totseq==0) {
		*seqar= 0;
		return;
	}
	*seqar= MEM_mallocN(sizeof(void *)* *totseq, "seqar");
	tseqar= *seqar;

	do_build_seqar(seqbase, seqar, 0);
	*seqar= tseqar;
}

static void do_seq_count_cb(ListBase *seqbase, int *totseq,
			    int (*test_func)(Sequence * seq))
{
	Sequence *seq;

	seq= seqbase->first;
	while(seq) {
		int test = test_func(seq);
		if (test & BUILD_SEQAR_COUNT_CURRENT) {
			(*totseq)++;
		}
		if(seq->seqbase.first && (test & BUILD_SEQAR_COUNT_CHILDREN)) {
			do_seq_count_cb(&seq->seqbase, totseq, test_func);
		}
		seq= seq->next;
	}
}

static void do_build_seqar_cb(ListBase *seqbase, Sequence ***seqar, int depth,
			      int (*test_func)(Sequence * seq))
{
	Sequence *seq;

	seq= seqbase->first;
	while(seq) {
		int test = test_func(seq);
		seq->depth= depth;

		if(seq->seqbase.first && (test & BUILD_SEQAR_COUNT_CHILDREN)) {
			do_build_seqar_cb(&seq->seqbase, seqar, depth+1, 
					  test_func);
		}
		if (test & BUILD_SEQAR_COUNT_CURRENT) {
			**seqar= seq;
			(*seqar)++;
		}
		seq= seq->next;
	}
}

void build_seqar_cb(ListBase *seqbase, Sequence  ***seqar, int *totseq,
		    int (*test_func)(Sequence * seq))
{
	Sequence **tseqar;

	*totseq= 0;
	do_seq_count_cb(seqbase, totseq, test_func);

	if(*totseq==0) {
		*seqar= 0;
		return;
	}
	*seqar= MEM_mallocN(sizeof(void *)* *totseq, "seqar");
	tseqar= *seqar;

	do_build_seqar_cb(seqbase, seqar, 0, test_func);
	*seqar= tseqar;
}


void free_editing(Editing *ed)
{
	MetaStack *ms;
	Sequence *seq;

	if(ed==NULL) return;
	set_last_seq(NULL);	/* clear_last_seq doesnt work, it screws up free_sequence */

	WHILE_SEQ(&ed->seqbase) {
		free_sequence(seq);
	}
	END_SEQ

	while( (ms= ed->metastack.first) ) {
		BLI_remlink(&ed->metastack, ms);
		MEM_freeN(ms);
	}

	MEM_freeN(ed);

}

void calc_sequence_disp(Sequence *seq)
{
	if(seq->startofs && seq->startstill) seq->startstill= 0;
	if(seq->endofs && seq->endstill) seq->endstill= 0;
	
	seq->startdisp= seq->start + seq->startofs - seq->startstill;
	seq->enddisp= seq->start+seq->len - seq->endofs + seq->endstill;
	
	seq->handsize= 10.0;	/* 10 frames */
	if( seq->enddisp-seq->startdisp < 10 ) {
		seq->handsize= (float)(0.5*(seq->enddisp-seq->startdisp));
	}
	else if(seq->enddisp-seq->startdisp > 250) {
		seq->handsize= (float)((seq->enddisp-seq->startdisp)/25);
	}
}

void calc_sequence(Sequence *seq)
{
	Sequence *seqm;
	int min, max;

	/* check all metas recursively */
	seqm= seq->seqbase.first;
	while(seqm) {
		if(seqm->seqbase.first) calc_sequence(seqm);
		seqm= seqm->next;
	}

	/* effects and meta: automatic start and end */

	if(seq->type & SEQ_EFFECT) {
		/* pointers */
		if(seq->seq2==0) seq->seq2= seq->seq1;
		if(seq->seq3==0) seq->seq3= seq->seq1;

		/* effecten go from seq1 -> seq2: test */

		/* we take the largest start and smallest end */

		// seq->start= seq->startdisp= MAX2(seq->seq1->startdisp, seq->seq2->startdisp);
		// seq->enddisp= MIN2(seq->seq1->enddisp, seq->seq2->enddisp);

		if (seq->seq1) {
			seq->start= seq->startdisp= MAX3(seq->seq1->startdisp, seq->seq2->startdisp, seq->seq3->startdisp);
			seq->enddisp= MIN3(seq->seq1->enddisp, seq->seq2->enddisp, seq->seq3->enddisp);
			seq->len= seq->enddisp - seq->startdisp;
		} else {
			calc_sequence_disp(seq);
		}

		if(seq->strip && seq->len!=seq->strip->len) {
			new_tstripdata(seq);
		}

	}
	else {
		if(seq->type==SEQ_META) {
			seqm= seq->seqbase.first;
			if(seqm) {
				min= 1000000;
				max= -1000000;
				while(seqm) {
					if(seqm->startdisp < min) min= seqm->startdisp;
					if(seqm->enddisp > max) max= seqm->enddisp;
					seqm= seqm->next;
				}
				seq->start= min + seq->anim_startofs;
				seq->len = max-min;
				seq->len -= seq->anim_startofs;
				seq->len -= seq->anim_endofs;

				if(seq->strip && seq->len!=seq->strip->len) {
					new_tstripdata(seq);
				}
			}
		}
		calc_sequence_disp(seq);
	}
}

void reload_sequence_new_file(Sequence * seq)
{
	char str[FILE_MAXDIR+FILE_MAXFILE];

	if (!(seq->type == SEQ_MOVIE || seq->type == SEQ_IMAGE ||
	      seq->type == SEQ_HD_SOUND || seq->type == SEQ_RAM_SOUND ||
	      seq->type == SEQ_SCENE || seq->type == SEQ_META)) {
		return;
	}

	new_tstripdata(seq);

	if (seq->type != SEQ_SCENE && seq->type != SEQ_META &&
	    seq->type != SEQ_IMAGE) {
		BLI_join_dirfile(str, seq->strip->dir, seq->strip->stripdata->name);
		BLI_convertstringcode(str, G.sce);
		BLI_convertstringframe(str, G.scene->r.cfra);
		
	}

	if (seq->type == SEQ_IMAGE) {
		/* Hack? */
		int olen = MEM_allocN_len(seq->strip->stripdata) 
			/ sizeof(struct StripElem);
		seq->len = olen;
		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
	} else if (seq->type == SEQ_MOVIE) {
		if(seq->anim) IMB_free_anim(seq->anim);
		seq->anim = openanim(
			str, IB_rect | 
			((seq->flag & SEQ_FILTERY) 
			 ? IB_animdeinterlace : 0));

		if (!seq->anim) {
			return;
		}
	
		seq->len = IMB_anim_get_duration(seq->anim);
		
		seq->anim_preseek = IMB_anim_get_preseek(seq->anim);

		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
	} else if (seq->type == SEQ_HD_SOUND) {
		if(seq->hdaudio) sound_close_hdaudio(seq->hdaudio);
		seq->hdaudio = sound_open_hdaudio(str);

		if (!seq->hdaudio) {
			return;
		}

		seq->len = sound_hdaudio_get_duration(seq->hdaudio, FPS)
			- seq->anim_startofs - seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
	} else if (seq->type == SEQ_RAM_SOUND) {
		seq->len = (int) ( ((float)(seq->sound->streamlen-1)/
				    ((float)G.scene->audio.mixrate*4.0 ))
				   * FPS);
		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
	} else if (seq->type == SEQ_SCENE) {
		Scene * sce = G.main->scene.first;
                int nr = 1;
                while(sce) {
                        if(nr == seq->scenenr) {
                                break;
                        }
                        nr++;
                        sce= sce->id.next;
                }

		if (sce) {
			seq->scene = sce;
		} else {
			sce = seq->scene;
		}

		strncpy(seq->name + 2, sce->id.name + 2, 
			sizeof(seq->name) - 2);

		seq->len= seq->scene->r.efra - seq->scene->r.sfra + 1;
		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
	}

	calc_sequence(seq);
}

void sort_seq()
{
	/* all strips together per kind, and in order of y location ("machine") */
	ListBase seqbase, effbase;
	Editing *ed;
	Sequence *seq, *seqt;

	ed= G.scene->ed;
	if(ed==0) return;

	seqbase.first= seqbase.last= 0;
	effbase.first= effbase.last= 0;

	while( (seq= ed->seqbasep->first) ) {
		BLI_remlink(ed->seqbasep, seq);

		if(seq->type & SEQ_EFFECT) {
			seqt= effbase.first;
			while(seqt) {
				if(seqt->machine>=seq->machine) {
					BLI_insertlinkbefore(&effbase, seqt, seq);
					break;
				}
				seqt= seqt->next;
			}
			if(seqt==0) BLI_addtail(&effbase, seq);
		}
		else {
			seqt= seqbase.first;
			while(seqt) {
				if(seqt->machine>=seq->machine) {
					BLI_insertlinkbefore(&seqbase, seqt, seq);
					break;
				}
				seqt= seqt->next;
			}
			if(seqt==0) BLI_addtail(&seqbase, seq);
		}
	}

	addlisttolist(&seqbase, &effbase);
	*(ed->seqbasep)= seqbase;
}


void clear_scene_in_allseqs(Scene *sce)
{
	Scene *sce1;
	Editing *ed;
	Sequence *seq;

	/* when a scene is deleted: test all seqs */

	sce1= G.main->scene.first;
	while(sce1) {
		if(sce1!=sce && sce1->ed) {
			ed= sce1->ed;

			WHILE_SEQ(&ed->seqbase) {

				if(seq->scene==sce) seq->scene= 0;

			}
			END_SEQ
		}

		sce1= sce1->id.next;
	}
}

char *give_seqname_by_type(int type)
{
	switch(type) {
	case SEQ_META:	     return "Meta";
	case SEQ_IMAGE:      return "Image";
	case SEQ_SCENE:      return "Scene";
	case SEQ_MOVIE:      return "Movie";
	case SEQ_RAM_SOUND:  return "Audio (RAM)";
	case SEQ_HD_SOUND:   return "Audio (HD)";
	case SEQ_CROSS:      return "Cross";
	case SEQ_GAMCROSS:   return "Gamma Cross";
	case SEQ_ADD:        return "Add";
	case SEQ_SUB:        return "Sub";
	case SEQ_MUL:        return "Mul";
	case SEQ_ALPHAOVER:  return "Alpha Over";
	case SEQ_ALPHAUNDER: return "Alpha Under";
	case SEQ_OVERDROP:   return "Over Drop";
	case SEQ_WIPE:       return "Wipe";
	case SEQ_GLOW:       return "Glow";
	case SEQ_TRANSFORM:  return "Transform";
	case SEQ_COLOR:      return "Color";
	case SEQ_SPEED:      return "Speed";
	default:
		return 0;
	}
}

char *give_seqname(Sequence *seq)
{
	char * name = give_seqname_by_type(seq->type);

	if (!name) {
		if(seq->type<SEQ_EFFECT) {
			return seq->strip->dir;
		} else if(seq->type==SEQ_PLUGIN) {
			if(!(seq->flag & SEQ_EFFECT_NOT_LOADED) &&
			   seq->plugin && seq->plugin->doit) {
				return seq->plugin->pname;
			} else {
				return "Plugin";
			}
		} else {
			return "Effect";
		}
	}
	return name;
}

/* ***************** DO THE SEQUENCE ***************** */

static void make_black_ibuf(ImBuf *ibuf)
{
	unsigned int *rect;
	float *rect_float;
	int tot;

	if(ibuf==0 || (ibuf->rect==0 && ibuf->rect_float==0)) return;

	tot= ibuf->x*ibuf->y;

	rect= ibuf->rect;
	rect_float = ibuf->rect_float;

	if (rect) {
		memset(rect,       0, tot * sizeof(char) * 4);
	}

	if (rect_float) {
		memset(rect_float, 0, tot * sizeof(float) * 4);
	}
}

static void multibuf(ImBuf *ibuf, float fmul)
{
	char *rt;
	float *rt_float;

	int a, mul, icol;

	mul= (int)(256.0*fmul);
	rt= (char *)ibuf->rect;
	rt_float = ibuf->rect_float;

	if (rt) {
		a= ibuf->x*ibuf->y;
		while(a--) {

			icol= (mul*rt[0])>>8;
			if(icol>254) rt[0]= 255; else rt[0]= icol;
			icol= (mul*rt[1])>>8;
			if(icol>254) rt[1]= 255; else rt[1]= icol;
			icol= (mul*rt[2])>>8;
			if(icol>254) rt[2]= 255; else rt[2]= icol;
			icol= (mul*rt[3])>>8;
			if(icol>254) rt[3]= 255; else rt[3]= icol;
			
			rt+= 4;
		}
	}
	if (rt_float) {
		a= ibuf->x*ibuf->y;
		while(a--) {
			rt_float[0] *= fmul;
			rt_float[1] *= fmul;
			rt_float[2] *= fmul;
			rt_float[3] *= fmul;
			
			rt_float += 4;
		}
	}
}

static void do_effect(int cfra, Sequence *seq, TStripElem * se)
{
	TStripElem *se1, *se2, *se3;
	float fac, facf;
	int x, y;
	int early_out;
	struct SeqEffectHandle sh = get_sequence_effect(seq);

	if (!sh.execute) { /* effect not supported in this version... */
		make_black_ibuf(se->ibuf);
		return;
	}

	if(seq->ipo && seq->ipo->curve.first) {
		do_seq_ipo(seq, cfra);
		fac= seq->facf0;
		facf= seq->facf1;
	} else {
		sh.get_default_fac(seq, cfra, &fac, &facf);
	}

	if( !(G.scene->r.mode & R_FIELDS) ) facf = fac;

	early_out = sh.early_out(seq, fac, facf);

	if (early_out == -1) { /* no input needed */
		sh.execute(seq, cfra, fac, facf, 
			   se->ibuf->x, se->ibuf->y, 
			   0, 0, 0, se->ibuf);
		return;
	}

	switch (early_out) {
	case 0:
		if (se->se1==0 || se->se2==0 || se->se3==0) {
			make_black_ibuf(se->ibuf);
			return;
		}

		se1= se->se1;
		se2= se->se2;
		se3= se->se3;

		if (   (se1==0 || se2==0 || se3==0)
		    || (se1->ibuf==0 || se2->ibuf==0 || se3->ibuf==0)) {
			make_black_ibuf(se->ibuf);
			return;
		}

		break;
	case 1:
		if (se->se1 == 0) {
			make_black_ibuf(se->ibuf);
			return;
		}

		se1= se->se1;

		if (se1 == 0 || se1->ibuf == 0) {
			make_black_ibuf(se->ibuf);
			return;
		}

		if (se->ibuf != se1->ibuf) {
			IMB_freeImBuf(se->ibuf);
			se->ibuf = se1->ibuf;
			IMB_refImBuf(se->ibuf);
		}
		return;
	case 2:
		if (se->se2 == 0) {
			make_black_ibuf(se->ibuf);
			return;
		}

		se2= se->se2;

		if (se2 == 0 || se2->ibuf == 0) {
			make_black_ibuf(se->ibuf);
			return;
		}
		if (se->ibuf != se2->ibuf) {
			IMB_freeImBuf(se->ibuf);
			se->ibuf = se2->ibuf;
			IMB_refImBuf(se->ibuf);
		}
		return;
	default:
		make_black_ibuf(se->ibuf);
		return;
	}

	x= se2->ibuf->x;
	y= se2->ibuf->y;

	if (!se1->ibuf->rect_float && se->ibuf->rect_float) {
		IMB_float_from_rect(se1->ibuf);
	}
	if (!se2->ibuf->rect_float && se->ibuf->rect_float) {
		IMB_float_from_rect(se2->ibuf);
	}

	if (!se1->ibuf->rect && !se->ibuf->rect_float) {
		IMB_rect_from_float(se1->ibuf);
	}
	if (!se2->ibuf->rect && !se->ibuf->rect_float) {
		IMB_rect_from_float(se2->ibuf);
	}

	sh.execute(seq, cfra, fac, facf, x, y, se1->ibuf, se2->ibuf, se3->ibuf,
		   se->ibuf);
}

static int give_stripelem_index(Sequence *seq, int cfra)
{
	int nr;

	if(seq->startdisp >cfra || seq->enddisp <= cfra) return -1;
	if(seq->len == 0) return -1;
	if(seq->flag&SEQ_REVERSE_FRAMES) {	
		/*reverse frame in this sequence */
		if(cfra <= seq->start) nr= seq->len-1;
		else if(cfra >= seq->start+seq->len-1) nr= 0;
		else nr= (seq->start + seq->len) - cfra;
	} else {
		if(cfra <= seq->start) nr= 0;
		else if(cfra >= seq->start+seq->len-1) nr= seq->len-1;
		else nr= cfra-seq->start;
	}
	if (seq->strobe < 1.0) seq->strobe = 1.0;
	if (seq->strobe > 1.0) {
		nr -= (int)fmod((double)nr, (double)seq->strobe);
	}

	return nr;
}

static TStripElem* alloc_tstripdata(int len, const char * name)
{
	int i;
	TStripElem *se = MEM_callocN(len * sizeof(TStripElem), name);
	for (i = 0; i < len; i++) {
		se[i].ok = STRIPELEM_OK;
	}
	return se;
}

TStripElem *give_tstripelem(Sequence *seq, int cfra)
{
	TStripElem *se;
	int nr;

	se = seq->strip->tstripdata;
	if (se == 0 && seq->len > 0) {
		se = seq->strip->tstripdata = alloc_tstripdata(seq->len,
							       "tstripelems");
	}
	nr = give_stripelem_index(seq, cfra);

	if (nr == -1) return 0;
	if (se == 0) return 0;

	se += nr; 

	/* if there are IPOs with blend modes active, one has to watch out
	   for startstill + endstill area: we can't use the same tstripelem
	   here for all ibufs, since then, blending with IPOs won't work!
	   
	   Rather common case, if you use a single image and try to fade
	   it in and out... or want to use your strip as a watermark in
	   alpha over mode...
	*/
	if (seq->blend_mode != SEQ_BLEND_REPLACE ||
	    (seq->ipo && seq->ipo->curve.first && (
		    !(seq->type & SEQ_EFFECT) || !seq->seq1))) {
		Strip * s = seq->strip;
		if (cfra < seq->start) {
			se = s->tstripdata_startstill;
			if (seq->startstill > s->startstill) {
				free_tstripdata(s->startstill, 
						s->tstripdata_startstill);
				se = 0;
			}

			if (se == 0) {
				s->startstill = seq->startstill;
				se = seq->strip->tstripdata_startstill
					= alloc_tstripdata(
						s->startstill,
						"tstripelems_startstill");
			}
			se += seq->start - cfra - 1;

		} else if (cfra > seq->start + seq->len-1) {
			se = s->tstripdata_endstill;
			if (seq->endstill > s->endstill) {
				free_tstripdata(s->endstill, 
						s->tstripdata_endstill);
				se = 0;
			}

			if (se == 0) {
				s->endstill = seq->endstill;
				se = seq->strip->tstripdata_endstill
					= alloc_tstripdata(
						s->endstill,
						"tstripelems_endstill");
			}
			se += cfra - (seq->start + seq->len-1) - 1;
		}
	}

	
	se->nr= nr;

	return se;
}

StripElem *give_stripelem(Sequence *seq, int cfra)
{
	StripElem *se;
	int nr;

	se = seq->strip->stripdata;
	nr = give_stripelem_index(seq, cfra);

	if (nr == -1) return 0;
	if (se == 0) return 0;

	se += nr + seq->anim_startofs; 
	
	return se;
}

static int evaluate_seq_frame_gen(
	Sequence ** seq_arr, ListBase *seqbase, int cfra)
{
	Sequence *seq;
	int totseq=0;

	memset(seq_arr, 0, sizeof(Sequence*) * (MAXSEQ+1));

	seq= seqbase->first;
	while(seq) {
		if(seq->startdisp <=cfra && seq->enddisp > cfra) {
			seq_arr[seq->machine]= seq;
			totseq++;
		}
		seq= seq->next;
	}

	return totseq;
}

int evaluate_seq_frame(int cfra)
{
       Editing *ed;
       Sequence *seq_arr[MAXSEQ+1];

       ed= G.scene->ed;
       if(ed==0) return 0;
	
       return evaluate_seq_frame_gen(seq_arr, ed->seqbasep, cfra);

}

static int video_seq_is_rendered(Sequence * seq)
{
	return (seq 
		&& !(seq->flag & SEQ_MUTE) 
		&& seq->type != SEQ_RAM_SOUND 
		&& seq->type != SEQ_HD_SOUND);
}

static int get_shown_sequences(
	ListBase * seqbasep, int cfra, int chanshown, Sequence ** seq_arr_out)
{
	Sequence *seq_arr[MAXSEQ+1];
	int b = chanshown;
	int cnt = 0;

	if (b > MAXSEQ) {
		return 0;
	}

	if(evaluate_seq_frame_gen(seq_arr, seqbasep, cfra)) {
		if (b > 0) {
			if (seq_arr[b] == 0) {
				return 0;
			}
		} else {
			for (b = MAXSEQ; b > 0; b--) {
				if (video_seq_is_rendered(seq_arr[b])) {
					break;
				}
			}
		}
	}
	
	chanshown = b;

	for (;b > 0; b--) {
		if (video_seq_is_rendered(seq_arr[b])) {
			if (seq_arr[b]->blend_mode == SEQ_BLEND_REPLACE) {
				break;
			}
		}
	}

	for (;b <= chanshown; b++) {
		if (video_seq_is_rendered(seq_arr[b])) {
			seq_arr_out[cnt++] = seq_arr[b];
		}
	}

	return cnt;
}
 

/* **********************************************************************
   proxy management
   ********************************************************************** */

#define PROXY_MAXFILE (2*FILE_MAXDIR+FILE_MAXFILE)

static int seq_proxy_get_fname(Sequence * seq, int cfra, char * name)
{
	int frameno;
	char dir[FILE_MAXDIR];

	if (!seq->strip->proxy) {
		return FALSE;
	}

	if (seq->flag & SEQ_USE_PROXY_CUSTOM_DIR) {
		strcpy(dir, seq->strip->proxy->dir);
	} else {
		if (seq->type == SEQ_IMAGE || seq->type == SEQ_MOVIE) {
			snprintf(dir, FILE_MAXDIR, "%s/BL_proxy", 
				 seq->strip->dir);
		} else {
			return FALSE;
		}
	}

	/* generate a seperate proxy directory for each preview size */

	if (seq->type == SEQ_IMAGE) {
		StripElem * se = give_stripelem(seq, cfra);
		snprintf(name, PROXY_MAXFILE, "%s/images/%d/%s_proxy",
			 dir, G.scene->r.size, se->name);
		frameno = 1;
	} else if (seq->type == SEQ_MOVIE) {
		TStripElem * tse = give_tstripelem(seq, cfra);

		frameno = tse->nr + seq->anim_startofs;

		snprintf(name, PROXY_MAXFILE, "%s/%s/%d/####", dir,
			 seq->strip->stripdata->name,
			 G.scene->r.size);
	} else {
		TStripElem * tse = give_tstripelem(seq, cfra);

		frameno = tse->nr + seq->anim_startofs;

		snprintf(name, PROXY_MAXFILE, "%s/proxy_misc/%d/####", dir,
			 G.scene->r.size);
	}

	BLI_convertstringcode(name, G.sce);
	BLI_convertstringframe(name, frameno);
	

	strcat(name, ".jpg");

	return TRUE;
}

static struct ImBuf * seq_proxy_fetch(Sequence * seq, int cfra)
{
	char name[PROXY_MAXFILE];

	if (!(seq->flag & SEQ_USE_PROXY)) {
		return 0;
	}

	/* rendering at 100% ? No real sense in proxy-ing, right? */
	if (G.scene->r.size == 100.0) {
		return 0;
	}

	if (!seq_proxy_get_fname(seq, cfra, name)) {
		return 0;
	}

	if (BLI_exists(name)) {
		return IMB_loadiffname(name, IB_rect);
	} else {
		return 0;
	}
}

static void do_build_seq_ibuf(Sequence * seq, TStripElem *se, int cfra,
			      int build_proxy_run);

static void seq_proxy_build_frame(Sequence * seq, int cfra)
{
	char name[PROXY_MAXFILE];
	int quality;
	TStripElem * se;
	int ok;
	int rectx, recty;
	struct ImBuf * ibuf;

	if (!(seq->flag & SEQ_USE_PROXY)) {
		return;
	}

	/* rendering at 100% ? No real sense in proxy-ing, right? */
	if (G.scene->r.size == 100.0) {
		return;
	}

	if (!seq_proxy_get_fname(seq, cfra, name)) {
		return;
	}

	se = give_tstripelem(seq, cfra);
	if (!se) {
		return;
	}

	if(se->ibuf) {
		IMB_freeImBuf(se->ibuf);
		se->ibuf = 0;
	}
	
	do_build_seq_ibuf(seq, se, cfra, TRUE);

	if (!se->ibuf) {
		return;
	}

	rectx= (G.scene->r.size*G.scene->r.xsch)/100;
	recty= (G.scene->r.size*G.scene->r.ysch)/100;

	ibuf = se->ibuf;

	if (ibuf->x != rectx || ibuf->y != recty) {
		IMB_scalefastImBuf(ibuf, (short)rectx, (short)recty);
	}

	/* quality is fixed, otherwise one has to generate seperate
	   directories for every quality...

	   depth = 32 is intentionally left in, otherwise ALPHA channels
	   won't work... */
	quality = 90;
	ibuf->ftype= JPG | quality;

	BLI_make_existing_file(name);
	
	ok = IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat);
	if (ok == 0) {
		perror(name);
	}

	IMB_freeImBuf(ibuf);
	se->ibuf = 0;
}

void seq_proxy_rebuild(Sequence * seq)
{
	int cfra;

	waitcursor(1);

	G.afbreek = 0;

	/* flag management tries to account for strobe and 
	   other "non-linearities", that might come in the future...
	   better way would be to "touch" the files, so that _really_
	   no one is rebuild twice.
	 */

	for (cfra = seq->startdisp; cfra < seq->enddisp; cfra++) {
		TStripElem * tse = give_tstripelem(seq, cfra);

		tse->flag &= ~STRIPELEM_PREVIEW_DONE;
	}

	/* a _lot_ faster for movie files, if we read frames in
	   sequential order */
	if (seq->flag & SEQ_REVERSE_FRAMES) {
		for (cfra = seq->enddisp-seq->endstill-1; 
		     cfra >= seq->startdisp + seq->startstill; cfra--) {
			TStripElem * tse = give_tstripelem(seq, cfra);

			if (!(tse->flag & STRIPELEM_PREVIEW_DONE)) {
				seq_proxy_build_frame(seq, cfra);
				tse->flag |= STRIPELEM_PREVIEW_DONE;
			}
			if (blender_test_break()) {
				break;
			}
		}
	} else {
		for (cfra = seq->startdisp + seq->startstill; 
		     cfra < seq->enddisp - seq->endstill; cfra++) {
			TStripElem * tse = give_tstripelem(seq, cfra);

			if (!(tse->flag & STRIPELEM_PREVIEW_DONE)) {
				seq_proxy_build_frame(seq, cfra);
				tse->flag |= STRIPELEM_PREVIEW_DONE;
			}
			if (blender_test_break()) {
				break;
			}
		}
	}
	waitcursor(0);
}


/* **********************************************************************
   color balance 
   ********************************************************************** */

static StripColorBalance calc_cb(StripColorBalance * cb_)
{
	StripColorBalance cb = *cb_;
	int c;

	if (cb.flag & SEQ_COLOR_BALANCE_INVERSE_LIFT) {
		for (c = 0; c < 3; c++) {
			cb.lift[c] = 1.0 - cb.lift[c];
		}
	} else {
		for (c = 0; c < 3; c++) {
			cb.lift[c] = -(1.0 - cb.lift[c]);
		}
	}
	if (cb.flag & SEQ_COLOR_BALANCE_INVERSE_GAIN) {
		for (c = 0; c < 3; c++) {
			if (cb.gain[c] != 0.0) {
				cb.gain[c] = 1.0/cb.gain[c];
			} else {
				cb.gain[c] = 1000000; /* should be enough :) */
			}
		}
	}

	if (!(cb.flag & SEQ_COLOR_BALANCE_INVERSE_GAMMA)) {
		for (c = 0; c < 3; c++) {
			if (cb.gamma[c] != 0.0) {
				cb.gamma[c] = 1.0/cb.gamma[c];
			} else {
				cb.gamma[c] = 1000000; /* should be enough :) */
			}
		}
	}

	return cb;
}

static void make_cb_table_byte(float lift, float gain, float gamma,
			       unsigned char * table, float mul)
{
	int y;

	for (y = 0; y < 256; y++) {
	        float v = 1.0 * y / 255;
		v *= gain;
		v += lift; 
		v = pow(v, gamma);
		v *= mul;
		if ( v > 1.0) {
			v = 1.0;
		} else if (v < 0.0) {
			v = 0.0;
		}
		table[y] = v * 255;
	}

}

static void make_cb_table_float(float lift, float gain, float gamma,
				float * table, float mul)
{
	int y;

	for (y = 0; y < 256; y++) {
	        float v = (float) y * 1.0 / 255.0;
		v *= gain;
		v += lift;
		v = pow(v, gamma);
		v *= mul;
		table[y] = v;
	}
}

static void color_balance_byte_byte(Sequence * seq, TStripElem* se,
				    float mul)
{
	unsigned char cb_tab[3][256];
	int c;
	unsigned char * p = (unsigned char*) se->ibuf->rect;
	unsigned char * e = p + se->ibuf->x * 4 * se->ibuf->y;

	StripColorBalance cb = calc_cb(seq->strip->color_balance);

	for (c = 0; c < 3; c++) {
		make_cb_table_byte(cb.lift[c], cb.gain[c], cb.gamma[c],
				   cb_tab[c], mul);
	}

	while (p < e) {
		p[0] = cb_tab[0][p[0]];
		p[1] = cb_tab[1][p[1]];
		p[2] = cb_tab[2][p[2]];
		
		p += 4;
	}
}

static void color_balance_byte_float(Sequence * seq, TStripElem* se,
				     float mul)
{
	float cb_tab[4][256];
	int c,i;
	unsigned char * p = (unsigned char*) se->ibuf->rect;
	unsigned char * e = p + se->ibuf->x * 4 * se->ibuf->y;
	float * o;
	StripColorBalance cb;

	imb_addrectfloatImBuf(se->ibuf);

	o = se->ibuf->rect_float;

	cb = calc_cb(seq->strip->color_balance);

	for (c = 0; c < 3; c++) {
		make_cb_table_float(cb.lift[c], cb.gain[c], cb.gamma[c],
				    cb_tab[c], mul);
	}

	for (i = 0; i < 256; i++) {
		cb_tab[3][i] = ((float)i)*(1.0f/255.0f);
	}

	while (p < e) {
		o[0] = cb_tab[0][p[0]];
		o[1] = cb_tab[1][p[1]];
		o[2] = cb_tab[2][p[2]];
		o[3] = cb_tab[3][p[3]];

		p += 4; o += 4;
	}
}

static void color_balance_float_float(Sequence * seq, TStripElem* se,
				      float mul)
{
	float * p = se->ibuf->rect_float;
	float * e = se->ibuf->rect_float + se->ibuf->x * 4* se->ibuf->y;
	StripColorBalance cb = calc_cb(seq->strip->color_balance);

	while (p < e) {
		int c;
		for (c = 0; c < 3; c++) {
			p[c] = pow(p[c] * cb.gain[c] + cb.lift[c], 
				   cb.gamma[c]) * mul;
		}
		p += 4;
	}
}

static void color_balance(Sequence * seq, TStripElem* se, float mul)
{
	if (se->ibuf->rect_float) {
		color_balance_float_float(seq, se, mul);
	} else if(seq->flag & SEQ_MAKE_FLOAT) {
		color_balance_byte_float(seq, se, mul);
	} else {
		color_balance_byte_byte(seq, se, mul);
	}
}

/*
  input preprocessing for SEQ_IMAGE, SEQ_MOVIE and SEQ_SCENE

  Do all the things you can't really do afterwards using sequence effects
  (read: before rescaling to render resolution has been done)

  Order is important!

  - Deinterlace
  - Crop and transform in image source coordinate space
  - Flip X + Flip Y (could be done afterwards, backward compatibility)
  - Promote image to float data (affects pipeline operations afterwards)
  - Color balance (is most efficient in the byte -> float 
    (future: half -> float should also work fine!)
    case, if done on load, since we can use lookup tables)
  - Premultiply

*/

static int input_have_to_preprocess(Sequence * seq, TStripElem* se, int cfra)
{
	float mul;

	if ((seq->flag & SEQ_FILTERY) || 
	    (seq->flag & SEQ_USE_CROP) ||
	    (seq->flag & SEQ_USE_TRANSFORM) ||
	    (seq->flag & SEQ_FLIPX) ||
	    (seq->flag & SEQ_FLIPY) ||
	    (seq->flag & SEQ_USE_COLOR_BALANCE) ||
	    (seq->flag & SEQ_MAKE_PREMUL) ||
	    (se->ibuf->x != seqrectx || se->ibuf->y != seqrecty)) {
		return TRUE;
	}

	mul = seq->mul;

	if(seq->blend_mode == SEQ_BLEND_REPLACE) {
		if (seq->ipo && seq->ipo->curve.first) {
			do_seq_ipo(seq, cfra);
			mul *= seq->facf0;
		}
		mul *= seq->blend_opacity / 100.0;
	}

	if (mul != 1.0) {
		return TRUE;
	}
		
	return FALSE;
}

static void input_preprocess(Sequence * seq, TStripElem* se, int cfra)
{
	float mul;

	seq->strip->orx= se->ibuf->x;
	seq->strip->ory= se->ibuf->y;

	if((seq->flag & SEQ_FILTERY) && seq->type != SEQ_MOVIE) {
		IMB_filtery(se->ibuf);
	}

	if(seq->flag & SEQ_USE_CROP || seq->flag & SEQ_USE_TRANSFORM) {
		StripCrop c;
		StripTransform t;
		int sx,sy,dx,dy;

		memset(&c, 0, sizeof(StripCrop));
		memset(&t, 0, sizeof(StripTransform));

		if(seq->flag & SEQ_USE_CROP && seq->strip->crop) {
			c = *seq->strip->crop;
		}
		if(seq->flag & SEQ_USE_TRANSFORM && seq->strip->transform) {
			t = *seq->strip->transform;
		}

		sx = se->ibuf->x - c.left - c.right;
		sy = se->ibuf->y - c.top - c.bottom;
		dx = sx;
		dy = sy;

		if (seq->flag & SEQ_USE_TRANSFORM) {
			dx = seqrectx;
			dy = seqrecty;
		}

		if (c.top + c.bottom >= se->ibuf->y ||
		    c.left + c.right >= se->ibuf->x ||
		    t.xofs >= dx || t.yofs >= dy) {
			make_black_ibuf(se->ibuf);
		} else {
			ImBuf * i;

			if (se->ibuf->rect_float) {
				i = IMB_allocImBuf(dx, dy,32, IB_rectfloat, 0);
			} else {
				i = IMB_allocImBuf(dx, dy,32, IB_rect, 0);
			}

			IMB_rectcpy(i, se->ibuf, 
				    t.xofs, t.yofs, 
				    c.left, c.bottom, 
				    sx, sy);

			IMB_freeImBuf(se->ibuf);

			se->ibuf = i;
		}
	} 

	if(seq->flag & SEQ_FLIPX) {
		IMB_flipx(se->ibuf);
	}
	if(seq->flag & SEQ_FLIPY) {
		IMB_flipy(se->ibuf);
	}

	if(seq->mul == 0.0) {
		seq->mul = 1.0;
	}

	mul = seq->mul;

	if(seq->blend_mode == SEQ_BLEND_REPLACE) {
		if (seq->ipo && seq->ipo->curve.first) {
			do_seq_ipo(seq, cfra);
			mul *= seq->facf0;
		}
		mul *= seq->blend_opacity / 100.0;
	}

	if(seq->flag & SEQ_USE_COLOR_BALANCE && seq->strip->color_balance) {
		color_balance(seq, se, mul);
		mul = 1.0;
	}

	if(seq->flag & SEQ_MAKE_FLOAT) {
		if (!se->ibuf->rect_float) {
			IMB_float_from_rect(se->ibuf);
		}
		if (se->ibuf->rect) {
			imb_freerectImBuf(se->ibuf);
		}
	}

	if(mul != 1.0) {
		multibuf(se->ibuf, mul);
	}

	if(seq->flag & SEQ_MAKE_PREMUL) {
		if(se->ibuf->depth == 32 && se->ibuf->zbuf == 0) {
			converttopremul(se->ibuf);
		}
	}


	if(se->ibuf->x != seqrectx || se->ibuf->y != seqrecty ) {
		if(G.scene->r.mode & R_OSA) {
			IMB_scaleImBuf(se->ibuf, 
				       (short)seqrectx, (short)seqrecty);
		} else {
			IMB_scalefastImBuf(se->ibuf, 
					   (short)seqrectx, (short)seqrecty);
		}
	}
}

/* test if image too small or discarded from cache: reload */

static void test_and_auto_discard_ibuf(TStripElem * se)
{
	if (se->ibuf) {
		if(se->ibuf->x != seqrectx || se->ibuf->y != seqrecty 
		   || !(se->ibuf->rect || se->ibuf->rect_float)) {
			IMB_freeImBuf(se->ibuf);

			se->ibuf= 0;
			se->ok= STRIPELEM_OK;
		}
	}
	if (se->ibuf_comp) {
		if(se->ibuf_comp->x != seqrectx || se->ibuf_comp->y != seqrecty 
		   || !(se->ibuf_comp->rect || se->ibuf_comp->rect_float)) {
			IMB_freeImBuf(se->ibuf_comp);

			se->ibuf_comp = 0;
		}
	}
}

static void test_and_auto_discard_ibuf_stills(Strip * strip)
{
	if (strip->ibuf_startstill) {
		if (!strip->ibuf_startstill->rect &&
		    !strip->ibuf_startstill->rect_float) {
			IMB_freeImBuf(strip->ibuf_startstill);
			strip->ibuf_startstill = 0;
		}
	}
	if (strip->ibuf_endstill) {
		if (!strip->ibuf_endstill->rect &&
		    !strip->ibuf_endstill->rect_float) {
			IMB_freeImBuf(strip->ibuf_endstill);
			strip->ibuf_endstill = 0;
		}
	}
}

static void copy_from_ibuf_still(Sequence * seq, TStripElem * se)
{
	if (!se->ibuf) {
		if (se->nr == 0 && seq->strip->ibuf_startstill) {
			IMB_cache_limiter_touch(seq->strip->ibuf_startstill);

			se->ibuf = IMB_dupImBuf(seq->strip->ibuf_startstill);
		}
		if (se->nr == seq->len - 1 
		    && (seq->len != 1)
		    && seq->strip->ibuf_endstill) {
			IMB_cache_limiter_touch(seq->strip->ibuf_endstill);

			se->ibuf = IMB_dupImBuf(seq->strip->ibuf_endstill);
		}
	}
}

static void copy_to_ibuf_still(Sequence * seq, TStripElem * se)
{
	if (se->ibuf) {
		if (se->nr == 0) {
			seq->strip->ibuf_startstill = IMB_dupImBuf(se->ibuf);

			IMB_cache_limiter_insert(seq->strip->ibuf_startstill);
			IMB_cache_limiter_touch(seq->strip->ibuf_startstill);
		}
		if (se->nr == seq->len - 1 && seq->len != 1) {
			seq->strip->ibuf_endstill = IMB_dupImBuf(se->ibuf);

			IMB_cache_limiter_insert(seq->strip->ibuf_endstill);
			IMB_cache_limiter_touch(seq->strip->ibuf_endstill);
		}
	}
}

static void free_metastrip_imbufs(ListBase *seqbasep, int cfra, int chanshown)
{
	Sequence* seq_arr[MAXSEQ+1];
	int i;
	TStripElem* se = 0;

	evaluate_seq_frame_gen(seq_arr, seqbasep, cfra);

	for (i = 0; i < MAXSEQ; i++) {
		if (!video_seq_is_rendered(seq_arr[i])) {
			continue;
		}
		se = give_tstripelem(seq_arr[i], cfra);
		if (se) {
			if (se->ibuf) {
				IMB_freeImBuf(se->ibuf);

				se->ibuf= 0;
				se->ok= STRIPELEM_OK;
			}

			if (se->ibuf_comp) {
				IMB_freeImBuf(se->ibuf_comp);

				se->ibuf_comp = 0;
			}
		}
	}
	
}

static TStripElem* do_build_seq_array_recursively(
	ListBase *seqbasep, int cfra, int chanshown);

static void do_build_seq_ibuf(Sequence * seq, TStripElem *se, int cfra,
			      int build_proxy_run)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
	int use_limiter = TRUE;

	test_and_auto_discard_ibuf(se);
	test_and_auto_discard_ibuf_stills(seq->strip);

	if(seq->type == SEQ_META) {
		TStripElem * meta_se = 0;
		use_limiter = FALSE;

		if (!build_proxy_run && se->ibuf == 0) {
			se->ibuf = seq_proxy_fetch(seq, cfra);
			if (se->ibuf) {
				use_limiter = TRUE;
			}
		}

		if(!se->ibuf && seq->seqbase.first) {
			meta_se = do_build_seq_array_recursively(
				&seq->seqbase, seq->start + se->nr, 0);
		}

		se->ok = STRIPELEM_OK;

		if(!se->ibuf && meta_se) {
			se->ibuf = meta_se->ibuf_comp;
			if(se->ibuf &&
			   (!input_have_to_preprocess(seq, se, cfra) ||
			    build_proxy_run)) {
				IMB_refImBuf(se->ibuf);
				if (build_proxy_run) {
					IMB_cache_limiter_unref(se->ibuf);
				}
			} else if (se->ibuf) {
				struct ImBuf * i = IMB_dupImBuf(se->ibuf);

				IMB_cache_limiter_unref(se->ibuf);

				se->ibuf = i;

				use_limiter = TRUE;
			}
		}
		if (meta_se) {
			free_metastrip_imbufs(
				&seq->seqbase, seq->start + se->nr, 0);
		}

		if (use_limiter) {
			input_preprocess(seq, se, cfra);
		}
	} else if(seq->type & SEQ_EFFECT) {
		/* should the effect be recalculated? */
		
		if (!build_proxy_run && se->ibuf == 0) {
			se->ibuf = seq_proxy_fetch(seq, cfra);
		}

		if(se->ibuf == 0) {
			/* if one of two first inputs are rectfloat, output is float too */
			if((se->se1 && se->se1->ibuf && se->se1->ibuf->rect_float) ||
			   (se->se2 && se->se2->ibuf && se->se2->ibuf->rect_float))
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rectfloat, 0);
			else
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rect, 0);
			
			do_effect(cfra, seq, se);
		}
	} else if(seq->type == SEQ_IMAGE) {
		if(se->ok == STRIPELEM_OK && se->ibuf == 0) {
			StripElem * s_elem = give_stripelem(seq, cfra);
			BLI_join_dirfile(name, seq->strip->dir, s_elem->name);
			BLI_convertstringcode(name, G.sce);
			BLI_convertstringframe(name, G.scene->r.cfra);
			if (!build_proxy_run) {
				se->ibuf = seq_proxy_fetch(seq, cfra);
			}
			copy_from_ibuf_still(seq, se);

			if (!se->ibuf) {
				se->ibuf= IMB_loadiffname(
					name, IB_rect);
				copy_to_ibuf_still(seq, se);
			}
			
			if(se->ibuf == 0) {
				se->ok = STRIPELEM_FAILED;
			} else if (!build_proxy_run) {
				input_preprocess(seq, se, cfra);
			}
		}
	} else if(seq->type == SEQ_MOVIE) {
		if(se->ok == STRIPELEM_OK && se->ibuf==0) {
			if(!build_proxy_run) {
				se->ibuf = seq_proxy_fetch(seq, cfra);
			}
			copy_from_ibuf_still(seq, se);

			if (se->ibuf == 0) {
				if(seq->anim==0) {
					BLI_join_dirfile(name, seq->strip->dir, seq->strip->stripdata->name);
					BLI_convertstringcode(name, G.sce);
					BLI_convertstringframe(name, G.scene->r.cfra);
					
					seq->anim = openanim(
						name, IB_rect | 
						((seq->flag & SEQ_FILTERY) 
						 ? IB_animdeinterlace : 0));
				}
				if(seq->anim) {
					IMB_anim_set_preseek(seq->anim, seq->anim_preseek);
					se->ibuf = IMB_anim_absolute(seq->anim, se->nr + seq->anim_startofs);
				}
				copy_to_ibuf_still(seq, se);
			}
			
			if(se->ibuf == 0) {
				se->ok = STRIPELEM_FAILED;
			} else if (!build_proxy_run) {
				input_preprocess(seq, se, cfra);
			}
		}
	} else if(seq->type == SEQ_SCENE) {	// scene can be NULL after deletions
		int oldcfra = CFRA;
		Sequence * oldseq = get_last_seq();
		Scene *sce= seq->scene, *oldsce= G.scene;
		Render *re;
		RenderResult rres;
		int doseq, rendering= G.rendering;
		char scenename[64];
		int sce_valid =sce&& (sce->camera || sce->r.scemode & R_DOSEQ);
			
		if (se->ibuf == NULL && sce_valid && !build_proxy_run) {
			se->ibuf = seq_proxy_fetch(seq, cfra);
			if (se->ibuf) {
				input_preprocess(seq, se, cfra);
			}
		}

		if (se->ibuf == NULL && sce_valid) {
			copy_from_ibuf_still(seq, se);
			if (se->ibuf) {
				input_preprocess(seq, se, cfra);
			}
		}
		
		if (!sce_valid) {
			se->ok = STRIPELEM_FAILED;
		} else if (se->ibuf==NULL && sce_valid) {
			waitcursor(1);
			
			/* Hack! This function can be called from do_render_seq(), in that case
			   the seq->scene can already have a Render initialized with same name, 
			   so we have to use a default name. (compositor uses G.scene name to
			   find render).
			   However, when called from within the UI (image preview in sequencer)
			   we do want to use scene Render, that way the render result is defined
			   for display in render/imagewindow */
			if(rendering) {
				BLI_strncpy(scenename, sce->id.name+2, 64);
				strcpy(sce->id.name+2, " do_build_seq_ibuf");
			}
			re= RE_NewRender(sce->id.name);
			
			/* prevent eternal loop */
			doseq= G.scene->r.scemode & R_DOSEQ;
			G.scene->r.scemode &= ~R_DOSEQ;
			
			BIF_init_render_callbacks(re, 0);	/* 0= no display callbacks */
			
			/* hrms, set_scene still needed? work on that... */
			if(sce!=oldsce) set_scene_bg(sce);
			RE_BlenderFrame(re, sce,
					seq->sfra+se->nr+seq->anim_startofs);
			if(sce!=oldsce) set_scene_bg(oldsce);
			
			/* UGLY WARNING, it is set to zero in  RE_BlenderFrame */
			G.rendering= rendering;
			if(rendering)
				BLI_strncpy(sce->id.name+2, scenename, 64);
			
			RE_GetResultImage(re, &rres);
			
			if(rres.rectf) {
				se->ibuf= IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_rectfloat, 0);
				memcpy(se->ibuf->rect_float, rres.rectf, 4*sizeof(float)*rres.rectx*rres.recty);
				if(rres.rectz) {
					addzbuffloatImBuf(se->ibuf);
					memcpy(se->ibuf->zbuf_float, rres.rectz, sizeof(float)*rres.rectx*rres.recty);
				}
			} else if (rres.rect32) {
				se->ibuf= IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_rect, 0);
				memcpy(se->ibuf->rect, rres.rect32, 4*rres.rectx*rres.recty);
			}
			
			BIF_end_render_callbacks();
			
			/* restore */
			G.scene->r.scemode |= doseq;
			
			if((G.f & G_PLAYANIM)==0) /* bad, is set on do_render_seq */
				waitcursor(0);
			CFRA = oldcfra;
			set_last_seq(oldseq);

			copy_to_ibuf_still(seq, se);

			if (!build_proxy_run) {
				if(se->ibuf == NULL) {
					se->ok = STRIPELEM_FAILED;
				} else {
					input_preprocess(seq, se, cfra);
				}
			}

		}	
	}
	if (!build_proxy_run) {
		if (se->ibuf && use_limiter) {
			IMB_cache_limiter_insert(se->ibuf);
			IMB_cache_limiter_ref(se->ibuf);
			IMB_cache_limiter_touch(se->ibuf);
		}
	}
}

static TStripElem* do_build_seq_recursively(Sequence * seq, int cfra);

static void do_effect_seq_recursively(Sequence * seq, TStripElem *se, int cfra)
{
	float fac, facf;
	struct SeqEffectHandle sh = get_sequence_effect(seq);
	int early_out;

	se->se1 = 0;
	se->se2 = 0;
	se->se3 = 0;

	if(seq->ipo && seq->ipo->curve.first) {
		do_seq_ipo(seq, cfra);
		fac= seq->facf0;
		facf= seq->facf1;
	} else {
		sh.get_default_fac(seq, cfra, &fac, &facf);
	} 

	if( G.scene->r.mode & R_FIELDS ); else facf= fac;
	
	early_out = sh.early_out(seq, fac, facf);
	switch (early_out) {
	case -1:
		/* no input needed */
		break;
	case 0:
		se->se1 = do_build_seq_recursively(seq->seq1, cfra);
		se->se2 = do_build_seq_recursively(seq->seq2, cfra);
		if (seq->seq3) {
			se->se3 = do_build_seq_recursively(seq->seq3, cfra);
		}
		break;
	case 1:
		se->se1 = do_build_seq_recursively(seq->seq1, cfra);
		break;
	case 2:
		se->se2 = do_build_seq_recursively(seq->seq2, cfra);
		break;
	}


	do_build_seq_ibuf(seq, se, cfra, FALSE);

	/* children are not needed anymore ... */

	if (se->se1 && se->se1->ibuf) {
		IMB_cache_limiter_unref(se->se1->ibuf);
	}
	if (se->se2 && se->se2->ibuf) {
		IMB_cache_limiter_unref(se->se2->ibuf);
	}
	if (se->se3 && se->se3->ibuf) {
		IMB_cache_limiter_unref(se->se3->ibuf);
	}
}

static TStripElem* do_build_seq_recursively_impl(Sequence * seq, int cfra)
{
	TStripElem *se;

	se = give_tstripelem(seq, cfra);

	if(se) {
		if (seq->type & SEQ_EFFECT) {
			do_effect_seq_recursively(seq, se, cfra);
		} else {
			do_build_seq_ibuf(seq, se, cfra, FALSE);
		}
	}
	return se;
}

/* FIXME:
   
If cfra was float throughout blender (especially in the render
pipeline) one could even _render_ with subframe precision
instead of faking using the blend code below...

*/

static TStripElem* do_handle_speed_effect(Sequence * seq, int cfra)
{
	SpeedControlVars * s = (SpeedControlVars *)seq->effectdata;
	int nr = cfra - seq->start;
	float f_cfra;
	int cfra_left;
	int cfra_right;
	TStripElem * se = 0;
	TStripElem * se1 = 0;
	TStripElem * se2 = 0;
	
	sequence_effect_speed_rebuild_map(seq, 0);
	
	f_cfra = seq->start + s->frameMap[nr];
	
	cfra_left = (int) floor(f_cfra);
	cfra_right = (int) ceil(f_cfra);

	se = give_tstripelem(seq, cfra);

	if (!se) {
		return se;
	}

	if (cfra_left == cfra_right || 
	    (s->flags & SEQ_SPEED_BLEND) == 0) {
		test_and_auto_discard_ibuf(se);

		if (se->ibuf == NULL) {
			se1 = do_build_seq_recursively_impl(
				seq->seq1, cfra_left);

			if((se1 && se1->ibuf && se1->ibuf->rect_float))
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rectfloat, 0);
			else
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rect, 0);

			if (se1 == 0 || se1->ibuf == 0) {
				make_black_ibuf(se->ibuf);
			} else {
				if (se->ibuf != se1->ibuf) {
					if (se->ibuf) {
						IMB_freeImBuf(se->ibuf);
					}

					se->ibuf = se1->ibuf;
					IMB_refImBuf(se->ibuf);
				}
			}
		}
	} else {
		struct SeqEffectHandle sh;

		if(se->ibuf) {
			if(se->ibuf->x < seqrectx || se->ibuf->y < seqrecty 
			   || !(se->ibuf->rect || se->ibuf->rect_float)) {
				IMB_freeImBuf(se->ibuf);
				se->ibuf= 0;
			}
		}

		if (se->ibuf == NULL) {
			se1 = do_build_seq_recursively_impl(
				seq->seq1, cfra_left);
			se2 = do_build_seq_recursively_impl(
				seq->seq1, cfra_right);

			if((se1 && se1->ibuf && se1->ibuf->rect_float))
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rectfloat, 0);
			else
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rect, 0);
			
			if (!se1 || !se2) {
				make_black_ibuf(se->ibuf);
			} else {
				sh = get_sequence_effect(seq);

				sh.execute(seq, cfra, 
					   f_cfra - (float) cfra_left, 
					   f_cfra - (float) cfra_left, 
					   se->ibuf->x, se->ibuf->y, 
					   se1->ibuf, se2->ibuf, 0, se->ibuf);
			}
		}

	}

	/* caller expects this to be referenced, so do it! */
	if (se->ibuf) {
		IMB_cache_limiter_insert(se->ibuf);
		IMB_cache_limiter_ref(se->ibuf);
		IMB_cache_limiter_touch(se->ibuf);
	}

	/* children are no longer needed */
	if (se1 && se1->ibuf)
		IMB_cache_limiter_unref(se1->ibuf);
	if (se2 && se2->ibuf)
		IMB_cache_limiter_unref(se2->ibuf);

	return se;
}

/* 
 * build all ibufs recursively
 * 
 * if successfull, the returned TStripElem contains the (referenced!) imbuf
 * that means: you _must_ call 
 *
 * IMB_cache_limiter_unref(rval);
 * 
 * if rval != 0
 * 
 */

static TStripElem* do_build_seq_recursively(Sequence * seq, int cfra)
{
	if (seq->type == SEQ_SPEED) {
		return do_handle_speed_effect(seq, cfra);
	} else {
		return do_build_seq_recursively_impl(seq, cfra);
	}
}

static TStripElem* do_build_seq_array_recursively(
	ListBase *seqbasep, int cfra, int chanshown)
{
	Sequence* seq_arr[MAXSEQ+1];
	int count;
	int i;
	TStripElem* se = 0;

	count = get_shown_sequences(seqbasep, cfra, chanshown, (Sequence **)&seq_arr);

	if (!count) {
		return 0;
	}

	se = give_tstripelem(seq_arr[count - 1], cfra);

	if (!se) {
		return 0;
	}

	test_and_auto_discard_ibuf(se);

	if (se->ibuf_comp != 0) {
		IMB_cache_limiter_insert(se->ibuf_comp);
		IMB_cache_limiter_ref(se->ibuf_comp);
		IMB_cache_limiter_touch(se->ibuf_comp);
		return se;
	}

	
	if(count == 1) {
		se = do_build_seq_recursively(seq_arr[0], cfra);
		if (se->ibuf) {
			se->ibuf_comp = se->ibuf;
			IMB_refImBuf(se->ibuf_comp);
		}
		return se;
	}


	for (i = count - 1; i >= 0; i--) {
		int early_out;
		Sequence * seq = seq_arr[i];
		struct SeqEffectHandle sh;

		se = give_tstripelem(seq, cfra);

		test_and_auto_discard_ibuf(se);

		if (se->ibuf_comp != 0) {
			break;
		}
		if (seq->blend_mode == SEQ_BLEND_REPLACE) {
			do_build_seq_recursively(seq, cfra);
			if (se->ibuf) {
				se->ibuf_comp = se->ibuf;
				IMB_refImBuf(se->ibuf);
			} else {
				se->ibuf_comp = IMB_allocImBuf(
					(short)seqrectx, (short)seqrecty, 
					32, IB_rect, 0);
			}
			break;
		}

		sh = get_sequence_blend(seq);

		seq->facf0 = seq->facf1 = 1.0;

		if(seq->ipo && seq->ipo->curve.first) {
			do_seq_ipo(seq, cfra);
		} 

		if( G.scene->r.mode & R_FIELDS ); else seq->facf0 = seq->facf1;

		seq->facf0 *= seq->blend_opacity / 100.0;
		seq->facf1 *= seq->blend_opacity / 100.0;

		early_out = sh.early_out(seq, seq->facf0, seq->facf1);

		switch (early_out) {
		case -1:
		case 2:
			do_build_seq_recursively(seq, cfra);
			if (se->ibuf) {
				se->ibuf_comp = se->ibuf;
				IMB_refImBuf(se->ibuf_comp);
			} else {
				se->ibuf_comp = IMB_allocImBuf(
					(short)seqrectx, (short)seqrecty, 
					32, IB_rect, 0);
			}
			break;
		case 1:
			if (i == 0) {
				se->ibuf_comp = IMB_allocImBuf(
					(short)seqrectx, (short)seqrecty, 
					32, IB_rect, 0);
				IMB_cache_limiter_insert(se->ibuf_comp);
				IMB_cache_limiter_ref(se->ibuf_comp);
				IMB_cache_limiter_touch(se->ibuf_comp);
			}
			break;
		case 0:
			do_build_seq_recursively(seq, cfra);
			if (!se->ibuf) {
				se->ibuf = IMB_allocImBuf(
					(short)seqrectx, (short)seqrecty, 
					32, IB_rect, 0);
			}
			if (i == 0) {
				se->ibuf_comp = se->ibuf;
				IMB_refImBuf(se->ibuf_comp);
			}
			break;
		}
	
		if (se->ibuf_comp) {
			break;
		}
	}

	i++;

	for (; i < count; i++) {
		Sequence * seq = seq_arr[i];
		struct SeqEffectHandle sh = get_sequence_blend(seq);
		TStripElem* se1 = give_tstripelem(seq_arr[i-1], cfra);
		TStripElem* se2 = give_tstripelem(seq_arr[i], cfra);
	
		int early_out = sh.early_out(seq, seq->facf0, seq->facf1);
		switch (early_out) {
		case 0: {
			int x= se2->ibuf->x;
			int y= se2->ibuf->y;
			int swap_input = FALSE;

			if (se1->ibuf_comp->rect_float ||
			    se2->ibuf->rect_float) {
				se2->ibuf_comp = IMB_allocImBuf(
					(short)seqrectx, (short)seqrecty, 
					32, IB_rectfloat, 0);
			} else {
				se2->ibuf_comp = IMB_allocImBuf(
					(short)seqrectx, (short)seqrecty, 
					32, IB_rect, 0);
			}


			if (!se1->ibuf_comp->rect_float && 
			    se2->ibuf_comp->rect_float) {
				IMB_float_from_rect(se1->ibuf_comp);
			}
			if (!se2->ibuf->rect_float && 
			    se2->ibuf_comp->rect_float) {
				IMB_float_from_rect(se2->ibuf);
			}

			if (!se1->ibuf_comp->rect && 
			    !se2->ibuf_comp->rect_float) {
				IMB_rect_from_float(se1->ibuf_comp);
			}
			if (!se2->ibuf->rect && 
			    !se2->ibuf_comp->rect_float) {
				IMB_rect_from_float(se2->ibuf);
			}

			/* bad hack, to fix crazy input ordering of 
			   those two effects */

			if (seq->blend_mode == SEQ_ALPHAOVER ||
			    seq->blend_mode == SEQ_ALPHAUNDER ||
			    seq->blend_mode == SEQ_OVERDROP) {
				swap_input = TRUE;
			}

			if (swap_input) {
				sh.execute(seq, cfra, 
					   seq->facf0, seq->facf1, x, y, 
					   se2->ibuf, se1->ibuf_comp, 0,
					   se2->ibuf_comp);
			} else {
				sh.execute(seq, cfra, 
					   seq->facf0, seq->facf1, x, y, 
					   se1->ibuf_comp, se2->ibuf, 0,
					   se2->ibuf_comp);
			}
			
			IMB_cache_limiter_insert(se2->ibuf_comp);
			IMB_cache_limiter_ref(se2->ibuf_comp);
			IMB_cache_limiter_touch(se2->ibuf_comp);

			IMB_cache_limiter_unref(se1->ibuf_comp);
			IMB_cache_limiter_unref(se2->ibuf);

			break;
		}
		case 1: {
			se2->ibuf_comp = se1->ibuf;
			IMB_refImBuf(se2->ibuf_comp);

			break;
		}
		}
		se = se2;
	}

	return se;
}

/*
 * returned ImBuf is refed!
 * you have to unref after usage!
 */

static ImBuf *give_ibuf_seq_impl(int rectx, int recty, int cfra, int chanshown)
{
	Editing *ed;
	int count;
	ListBase *seqbasep;
	TStripElem *se;

	ed= G.scene->ed;
	if(ed==0) return 0;

	count = BLI_countlist(&ed->metastack);
	if((chanshown < 0) && (count > 0)) {
		count = MAX2(count + chanshown, 0);
		seqbasep= ((MetaStack*)BLI_findlink(&ed->metastack, count))->oldbasep;
	} else {
		seqbasep= ed->seqbasep;
	}

	seqrectx= rectx;	/* bad bad global! */
	seqrecty= recty;

	se = do_build_seq_array_recursively(seqbasep, cfra, chanshown);

	if(!se) { 
		return 0;
	}

	return se->ibuf_comp;
}

ImBuf *give_ibuf_seq_direct(int rectx, int recty, int cfra,
			    Sequence * seq)
{
	TStripElem* se;

	seqrectx= rectx;	/* bad bad global! */
	seqrecty= recty;

	se = do_build_seq_recursively(seq, cfra);

	if(!se) { 
		return 0;
	}

	if (se->ibuf) {
		IMB_cache_limiter_unref(se->ibuf);
	}

	return se->ibuf;
}

ImBuf *give_ibuf_seq(int rectx, int recty, int cfra, int chanshown)
{
	ImBuf* i = give_ibuf_seq_impl(rectx, recty, cfra, chanshown);

	if (i) {
		IMB_cache_limiter_unref(i);
	}
	return i;
}

/* threading api */

static ListBase running_threads;
static ListBase prefetch_wait;
static ListBase prefetch_done;

static pthread_mutex_t queue_lock          = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t wakeup_lock         = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  wakeup_cond         = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t prefetch_ready_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  prefetch_ready_cond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t frame_done_lock     = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  frame_done_cond     = PTHREAD_COND_INITIALIZER;

static volatile int seq_thread_shutdown = FALSE;
static volatile int seq_last_given_monoton_cfra = 0;
static int monoton_cfra = 0;

typedef struct PrefetchThread {
	struct PrefetchThread *next, *prev;
	struct PrefetchQueueElem *current;
	pthread_t pthread;
	int running;
} PrefetchThread;

typedef struct PrefetchQueueElem {
	struct PrefetchQueueElem *next, *prev;
	
	int rectx;
	int recty;
	int cfra;
	int chanshown;

	int monoton_cfra;

	struct ImBuf * ibuf;
} PrefetchQueueElem;

static void * seq_prefetch_thread(void * This_)
{
	PrefetchThread * This = This_;

	while (!seq_thread_shutdown) {
		PrefetchQueueElem * e;
		int s_last;

		pthread_mutex_lock(&queue_lock);
		e = prefetch_wait.first;
		if (e) {
			BLI_remlink(&prefetch_wait, e);
		}
		s_last = seq_last_given_monoton_cfra;

		This->current = e;

		pthread_mutex_unlock(&queue_lock);

		if (!e) {
			pthread_mutex_lock(&prefetch_ready_lock);

			This->running = FALSE;

			pthread_cond_signal(&prefetch_ready_cond);
			pthread_mutex_unlock(&prefetch_ready_lock);

			pthread_mutex_lock(&wakeup_lock);
			if (!seq_thread_shutdown) {
				pthread_cond_wait(&wakeup_cond, &wakeup_lock);
			}
			pthread_mutex_unlock(&wakeup_lock);
			continue;
		}

		This->running = TRUE;
		
		if (e->cfra >= s_last) { 
			e->ibuf = give_ibuf_seq_impl(
				e->rectx, e->recty, e->cfra, e->chanshown);
		}

		pthread_mutex_lock(&queue_lock);

		BLI_addtail(&prefetch_done, e);

		for (e = prefetch_wait.first; e; e = e->next) {
			if (s_last > e->monoton_cfra) {
				BLI_remlink(&prefetch_wait, e);
				MEM_freeN(e);
			}
		}

		for (e = prefetch_done.first; e; e = e->next) {
			if (s_last > e->monoton_cfra) {
				if (e->ibuf) {
					IMB_cache_limiter_unref(e->ibuf);
				}
				BLI_remlink(&prefetch_done, e);
				MEM_freeN(e);
			}
		}

		pthread_mutex_unlock(&queue_lock);

		pthread_mutex_lock(&frame_done_lock);
		pthread_cond_signal(&frame_done_cond);
		pthread_mutex_unlock(&frame_done_lock);
	}
	return 0;
}

void seq_start_threads()
{
	int i;

	running_threads.first = running_threads.last = NULL;
	prefetch_wait.first = prefetch_wait.last = NULL;
	prefetch_done.first = prefetch_done.last = NULL;

	seq_thread_shutdown = FALSE;
	seq_last_given_monoton_cfra = monoton_cfra = 0;

	/* since global structures are modified during the processing
	   of one frame, only one render thread is currently possible... 

	   (but we code, in the hope, that we can remove this restriction
	   soon...)
	*/

	fprintf(stderr, "SEQ-THREAD: seq_start_threads\n");

	for (i = 0; i < 1; i++) {
		PrefetchThread *t = MEM_callocN(sizeof(PrefetchThread), 
						"prefetch_thread");
		t->running = TRUE;
		BLI_addtail(&running_threads, t);

		pthread_create(&t->pthread, NULL, seq_prefetch_thread, t);
	}

	/* init malloc mutex */
	BLI_init_threads(0, 0, 0);
}

void seq_stop_threads()
{
	PrefetchThread *tslot;
	PrefetchQueueElem * e;

	fprintf(stderr, "SEQ-THREAD: seq_stop_threads()\n");

	if (seq_thread_shutdown) {
		fprintf(stderr, "SEQ-THREAD: ... already stopped\n");
		return;
	}
	
	pthread_mutex_lock(&wakeup_lock);

	seq_thread_shutdown = TRUE;

        pthread_cond_broadcast(&wakeup_cond);
        pthread_mutex_unlock(&wakeup_lock);

	for(tslot = running_threads.first; tslot; tslot= tslot->next) {
		pthread_join(tslot->pthread, NULL);
	}


	for (e = prefetch_wait.first; e; e = e->next) {
		BLI_remlink(&prefetch_wait, e);
		MEM_freeN(e);
	}

	for (e = prefetch_done.first; e; e = e->next) {
		if (e->ibuf) {
			IMB_cache_limiter_unref(e->ibuf);
		}
		BLI_remlink(&prefetch_done, e);
		MEM_freeN(e);
	}

	BLI_freelistN(&running_threads);

	/* deinit malloc mutex */
	BLI_end_threads(0);
}

void give_ibuf_prefetch_request(int rectx, int recty, int cfra, int chanshown)
{
	PrefetchQueueElem * e;
	if (seq_thread_shutdown) {
		return;
	}

	e = MEM_callocN(sizeof(PrefetchQueueElem), "prefetch_queue_elem");
	e->rectx = rectx;
	e->recty = recty;
	e->cfra = cfra;
	e->chanshown = chanshown;
	e->monoton_cfra = monoton_cfra++;

	pthread_mutex_lock(&queue_lock);
	BLI_addtail(&prefetch_wait, e);
	pthread_mutex_unlock(&queue_lock);
	
	pthread_mutex_lock(&wakeup_lock);
	pthread_cond_signal(&wakeup_cond);
	pthread_mutex_unlock(&wakeup_lock);
}

void seq_wait_for_prefetch_ready()
{
	PrefetchThread *tslot;

	if (seq_thread_shutdown) {
		return;
	}

	fprintf(stderr, "SEQ-THREAD: rendering prefetch frames...\n");

	pthread_mutex_lock(&prefetch_ready_lock);

	for(;;) {
		for(tslot = running_threads.first; tslot; tslot= tslot->next) {
			if (tslot->running) {
				break;
			}
		}
		if (!tslot) {
			break;
		}
		pthread_cond_wait(&prefetch_ready_cond, &prefetch_ready_lock);
	}

	pthread_mutex_unlock(&prefetch_ready_lock);

	fprintf(stderr, "SEQ-THREAD: prefetch done\n");
}

ImBuf * give_ibuf_seq_threaded(int rectx, int recty, int cfra, int chanshown)
{
	PrefetchQueueElem * e = 0;
	int found_something = FALSE;

	if (seq_thread_shutdown) {
		return give_ibuf_seq(rectx, recty, cfra, chanshown);
	}

	while (!e) {
		int success = FALSE;
		pthread_mutex_lock(&queue_lock);

		for (e = prefetch_done.first; e; e = e->next) {
			if (cfra == e->cfra &&
			    chanshown == e->chanshown &&
			    rectx == e->rectx && 
			    recty == e->recty) {
				success = TRUE;
				found_something = TRUE;
				break;
			}
		}

		if (!e) {
			for (e = prefetch_wait.first; e; e = e->next) {
				if (cfra == e->cfra &&
				    chanshown == e->chanshown &&
				    rectx == e->rectx && 
				    recty == e->recty) {
					found_something = TRUE;
					break;
				}
			}
		}

		if (!e) {
			PrefetchThread *tslot;

			for(tslot = running_threads.first; 
			    tslot; tslot= tslot->next) {
				if (tslot->current &&
				    cfra == tslot->current->cfra &&
				    chanshown == tslot->current->chanshown &&
				    rectx == tslot->current->rectx && 
				    recty == tslot->current->recty) {
					found_something = TRUE;
					break;
				}
			}
		}

		/* e->ibuf is unrefed by render thread on next round. */

		if (e) {
			seq_last_given_monoton_cfra = e->monoton_cfra;
		}

		pthread_mutex_unlock(&queue_lock);

		if (!success) {
			e = NULL;

			if (!found_something) {
				fprintf(stderr, 
					"SEQ-THREAD: Requested frame "
					"not in queue ???\n");
				break;
			}
			pthread_mutex_lock(&frame_done_lock);
			pthread_cond_wait(&frame_done_cond, &frame_done_lock);
			pthread_mutex_unlock(&frame_done_lock);
		}
	}
	
	return e ? e->ibuf : 0;
}

/* Functions to free imbuf and anim data on changes */

static void free_imbuf_strip_elem(TStripElem *se)
{
	if(se->ibuf) {
		IMB_freeImBuf(se->ibuf);
	}
	if(se->ibuf_comp) {
		IMB_freeImBuf(se->ibuf_comp);
	}
	se->ibuf_comp = 0;
	se->ibuf= 0;
	se->ok= STRIPELEM_OK;
	se->se1= se->se2= se->se3= 0;
}

static void free_anim_seq(Sequence *seq)
{
	if(seq->anim) {
		IMB_free_anim(seq->anim);
		seq->anim = 0;
	}
}

void free_imbuf_seq_except(int cfra)
{
	Editing *ed= G.scene->ed;
	Sequence *seq;
	TStripElem *se;
	int a;

	if(ed==0) return;

	WHILE_SEQ(&ed->seqbase) {
		if(seq->strip) {
			TStripElem * curelem = give_tstripelem(seq, cfra);

			for(a = 0, se = seq->strip->tstripdata; 
			    a < seq->strip->len && se; a++, se++) {
				if(se != curelem) {
					free_imbuf_strip_elem(se);
				}
			}
			for(a = 0, se = seq->strip->tstripdata_startstill;
			    a < seq->strip->startstill && se; a++, se++) {
				if(se != curelem) {
					free_imbuf_strip_elem(se);
				}
			}
			for(a = 0, se = seq->strip->tstripdata_endstill;
			    a < seq->strip->endstill && se; a++, se++) {
				if(se != curelem) {
					free_imbuf_strip_elem(se);
				}
			}
			if(seq->strip->ibuf_startstill) {
				IMB_freeImBuf(seq->strip->ibuf_startstill);
				seq->strip->ibuf_startstill = 0;
			}

			if(seq->strip->ibuf_endstill) {
				IMB_freeImBuf(seq->strip->ibuf_endstill);
				seq->strip->ibuf_endstill = 0;
			}

			if(seq->type==SEQ_MOVIE)
				if(seq->startdisp > cfra || seq->enddisp < cfra)
					free_anim_seq(seq);
		}
	}
	END_SEQ
}

void free_imbuf_seq()
{
	Editing *ed= G.scene->ed;
	Sequence *seq;
	TStripElem *se;
	int a;

	if(ed==0) return;

	WHILE_SEQ(&ed->seqbase) {
		if(seq->strip) {
			for(a = 0, se = seq->strip->tstripdata; 
			    a < seq->strip->len && se; a++, se++) {
				free_imbuf_strip_elem(se);
			}
			for(a = 0, se = seq->strip->tstripdata_startstill; 
			    a < seq->strip->startstill && se; a++, se++) {
				free_imbuf_strip_elem(se);
			}
			for(a = 0, se = seq->strip->tstripdata_endstill; 
			    a < seq->strip->endstill && se; a++, se++) {
				free_imbuf_strip_elem(se);
			}
			if(seq->strip->ibuf_startstill) {
				IMB_freeImBuf(seq->strip->ibuf_startstill);
				seq->strip->ibuf_startstill = 0;
			}

			if(seq->strip->ibuf_endstill) {
				IMB_freeImBuf(seq->strip->ibuf_endstill);
				seq->strip->ibuf_endstill = 0;
			}

			if(seq->type==SEQ_MOVIE)
				free_anim_seq(seq);
			if(seq->type==SEQ_SPEED) {
				sequence_effect_speed_rebuild_map(seq, 1);
			}
		}
	}
	END_SEQ
}

void free_imbuf_seq_with_ipo(struct Ipo *ipo)
{
	/* force update of all sequences with this ipo, on ipo changes */
	Editing *ed= G.scene->ed;
	Sequence *seq;

	if(ed==0) return;

	WHILE_SEQ(&ed->seqbase) {
		if(seq->ipo == ipo) {
			update_changed_seq_and_deps(seq, 0, 1);
			if(seq->type == SEQ_SPEED) {
				sequence_effect_speed_rebuild_map(seq, 1);
			}
		}
	}
	END_SEQ
}

static int update_changed_seq_recurs(Sequence *seq, Sequence *changed_seq, int len_change, int ibuf_change)
{
	Sequence *subseq;
	int a, free_imbuf = 0;
	TStripElem *se;

	/* recurs downwards to see if this seq depends on the changed seq */

	if(seq == NULL)
		return 0;

	if(seq == changed_seq)
		free_imbuf = 1;
	
	for(subseq=seq->seqbase.first; subseq; subseq=subseq->next)
		if(update_changed_seq_recurs(subseq, changed_seq, len_change, ibuf_change))
			free_imbuf = TRUE;
	
	if(seq->seq1)
		if(update_changed_seq_recurs(seq->seq1, changed_seq, len_change, ibuf_change))
			free_imbuf = TRUE;
	if(seq->seq2 && (seq->seq2 != seq->seq1))
		if(update_changed_seq_recurs(seq->seq2, changed_seq, len_change, ibuf_change))
			free_imbuf = TRUE;
	if(seq->seq3 && (seq->seq3 != seq->seq1) && (seq->seq3 != seq->seq2))
		if(update_changed_seq_recurs(seq->seq3, changed_seq, len_change, ibuf_change))
			free_imbuf = TRUE;
	
	if(free_imbuf) {
		if(ibuf_change) {
			se= seq->strip->tstripdata;
			if (se) {
				for(a=0; a<seq->len; a++, se++)
					free_imbuf_strip_elem(se);
			}
		
			if(seq->type == SEQ_MOVIE)
				free_anim_seq(seq);
			if(seq->type == SEQ_SPEED) {
				sequence_effect_speed_rebuild_map(seq, 1);
			}
		}

		if(len_change)
			calc_sequence(seq);
	}
	
	return free_imbuf;
}

void update_changed_seq_and_deps(Sequence *changed_seq, int len_change, int ibuf_change)
{
	Editing *ed= G.scene->ed;
	Sequence *seq;

	if (!ed) return;

	for (seq=ed->seqbase.first; seq; seq=seq->next)
		update_changed_seq_recurs(seq, changed_seq, len_change, ibuf_change);
}

/* bad levell call... */
void do_render_seq(RenderResult *rr, int cfra)
{
	ImBuf *ibuf;

	G.f |= G_PLAYANIM;	/* waitcursor patch */

	ibuf= give_ibuf_seq(rr->rectx, rr->recty, cfra, 0);
	
	if(ibuf) {
		if(ibuf->rect_float) {
			if (!rr->rectf)
				rr->rectf= MEM_mallocN(4*sizeof(float)*rr->rectx*rr->recty, "render_seq rectf");
			
			memcpy(rr->rectf, ibuf->rect_float, 4*sizeof(float)*rr->rectx*rr->recty);
			
			/* TSK! Since sequence render doesn't free the *rr render result, the old rect32
			   can hang around when sequence render has rendered a 32 bits one before */
			if(rr->rect32) {
				MEM_freeN(rr->rect32);
				rr->rect32= NULL;
			}
		}
		else if(ibuf->rect) {
			if (!rr->rect32)
				rr->rect32= MEM_mallocN(sizeof(int)*rr->rectx*rr->recty, "render_seq rect");

			memcpy(rr->rect32, ibuf->rect, 4*rr->rectx*rr->recty);

			/* if (ibuf->zbuf) { */
			/* 	if (R.rectz) freeN(R.rectz); */
			/* 	R.rectz = BLI_dupallocN(ibuf->zbuf); */
			/* } */
		}
		
		/* Let the cache limitor take care of this (schlaile) */
		/* While render let's keep all memory available for render 
		   (ton)
		   At least if free memory is tight...
		   This can make a big difference in encoding speed
		   (it is around 4 times(!) faster, if we do not waste time
		   on freeing _all_ buffers every time on long timelines...)
		   (schlaile)
		*/
		{
			extern int mem_in_use;
			extern int mmap_in_use;

			int max = MEM_CacheLimiter_get_maximum();
			if (max != 0 && mem_in_use + mmap_in_use > max) {
				fprintf(stderr, "mem_in_use = %d, max = %d\n",
					mem_in_use + mmap_in_use, max);
				fprintf(stderr, "Cleaning up, please wait...\n"
					"If this happens very often,\n"
					"consider "
					"raising the memcache limit in the "
					"user preferences.\n");
				free_imbuf_seq();
			}
		}
	}
	else {
		/* render result is delivered empty in most cases, nevertheless we handle all cases */
		if (rr->rectf)
			memset(rr->rectf, 0, 4*sizeof(float)*rr->rectx*rr->recty);
		else if (rr->rect32)
			memset(rr->rect32, 0, 4*rr->rectx*rr->recty);
		else
			rr->rect32= MEM_callocN(sizeof(int)*rr->rectx*rr->recty, "render_seq rect");
	}
	
	G.f &= ~G_PLAYANIM;

}
