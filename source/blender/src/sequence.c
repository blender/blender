/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

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

int seqrectx, seqrecty;

/* ***************** END PLUGIN ************************ */

void free_stripdata(int len, StripElem *se)
{
	StripElem *seo;
	int a;

	seo= se;

	for(a=0; a<len; a++, se++) {
		if(se->ibuf && se->ok!=2) {
			IMB_freeImBuf(se->ibuf);
			se->ibuf = 0;
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

	if(strip->stripdata) {
		free_stripdata(strip->len, strip->stripdata);
	}
	MEM_freeN(strip);
}

void new_stripdata(Sequence *seq)
{

	if(seq->strip) {
		if(seq->strip->stripdata) free_stripdata(seq->strip->len, seq->strip->stripdata);
		seq->strip->stripdata= 0;
		seq->strip->len= seq->len;
		if(seq->len>0) seq->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelems");
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

	if(seq==last_seq) set_last_seq_to_null();

	MEM_freeN(seq);
}

void do_seq_count(ListBase *seqbase, int *totseq)
{
	Sequence *seq;

	seq= seqbase->first;
	while(seq) {
		(*totseq)++;
		if(seq->seqbase.first) do_seq_count(&seq->seqbase, totseq);
		seq= seq->next;
	}
}

void do_build_seqar(ListBase *seqbase, Sequence ***seqar, int depth)
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

void free_editing(Editing *ed)
{
	MetaStack *ms;
	Sequence *seq;

	if(ed==0) return;

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

		seq->start= seq->startdisp= MAX3(seq->seq1->startdisp, seq->seq2->startdisp, seq->seq3->startdisp);
		seq->enddisp= MIN3(seq->seq1->enddisp, seq->seq2->enddisp, seq->seq3->enddisp);
		seq->len= seq->enddisp - seq->startdisp;

		if(seq->strip && seq->len!=seq->strip->len) {
			new_stripdata(seq);
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
				seq->start= min;
				seq->len= max-min;

				if(seq->strip && seq->len!=seq->strip->len) {
					new_stripdata(seq);
				}
			}
		}


		if(seq->startofs && seq->startstill) seq->startstill= 0;
		if(seq->endofs && seq->endstill) seq->endstill= 0;

		seq->startdisp= seq->start + seq->startofs - seq->startstill;
		seq->enddisp= seq->start+seq->len - seq->endofs + seq->endstill;

		seq->handsize= 10.0;	/* 10 frames */
		if( seq->enddisp-seq->startdisp < 20 ) {
			seq->handsize= (float)(0.5*(seq->enddisp-seq->startdisp));
		}
		else if(seq->enddisp-seq->startdisp > 250) {
			seq->handsize= (float)((seq->enddisp-seq->startdisp)/25);
		}
	}
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

/* ***************** DO THE SEQUENCE ***************** */

void make_black_ibuf(ImBuf *ibuf)
{
	int *rect;
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

void multibuf(ImBuf *ibuf, float fmul)
{
	char *rt;
	float *rt_float;

	int a, mul, icol;

	mul= (int)(256.0*fmul);

	a= ibuf->x*ibuf->y;
	rt= (char *)ibuf->rect;
	rt_float = ibuf->rect_float;

	if (rt) {
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
		while(a--) {
			rt_float[0] *= fmul;
			rt_float[1] *= fmul;
			rt_float[2] *= fmul;
			rt_float[3] *= fmul;
			
			rt_float += 4;
		}
	}
}

void do_effect(int cfra, Sequence *seq, StripElem *se)
{
	StripElem *se1, *se2, *se3;
	float fac, facf;
	int x, y;
	struct SeqEffectHandle sh = get_sequence_effect(seq);

	if(se->se1==0 || se->se2==0 || se->se3==0) {
		make_black_ibuf(se->ibuf);
		return;
	}

	/* if metastrip: other se's */
	if(se->se1->ok==2) se1= se->se1->se1;
	else se1= se->se1;

	if(se->se2->ok==2) se2= se->se2->se1;
	else se2= se->se2;

	if(se->se3->ok==2) se3= se->se3->se1;
	else se3= se->se3;

	if(se1==0 || se2==0 || se3==0) {
		make_black_ibuf(se->ibuf);
		return;
	}

	if(seq->ipo && seq->ipo->curve.first) {
		do_seq_ipo(seq);
		fac= seq->facf0;
		facf= seq->facf1;
	} else {
		sh.get_default_fac(seq, cfra, &fac, &facf);
	}

	if( G.scene->r.mode & R_FIELDS ); else facf= fac;

	switch (sh.early_out(seq, fac, facf)) {
	case 0:
		break;
	case 1:
		if (se1->ibuf==0) {
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
		if (se2->ibuf==0) {
			make_black_ibuf(se->ibuf);
			return;
		}
		if (se->ibuf != se2->ibuf) {
			IMB_freeImBuf(se->ibuf);
			se->ibuf = se2->ibuf;
			IMB_refImBuf(se->ibuf);
		}
		return;
	}

	if (se1->ibuf==0 || se2->ibuf==0 || se3->ibuf==0) {
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

StripElem *give_stripelem(Sequence *seq, int cfra)
{
	Strip *strip;
	StripElem *se;
	int nr;

	strip= seq->strip;
	se= strip->stripdata;

	if(se==0) return 0;
	if(seq->startdisp >cfra || seq->enddisp <= cfra) return 0;

	if(seq->flag&SEQ_REVERSE_FRAMES)	{	
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

	se+= nr; /* don't get confused by the increment, this is the same as strip->stripdata[nr], which works on some compilers...*/
	se->nr= nr;

	return se;
}

static int evaluate_seq_frame_gen(
	Sequence ** seq_arr, ListBase *seqbase, int cfra)
{
	Sequence *seq;
	int totseq=0;

	memset(seq_arr, 0, sizeof(Sequence*) * MAXSEQ);

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

static Sequence * get_shown_seq_from_metastrip(Sequence * seqm, int cfra)
{
	Sequence *seq, *seqim, *seqeff;
	Sequence *seq_arr[MAXSEQ+1];
	int b;

	seq = 0;

	if(evaluate_seq_frame_gen(seq_arr, &seqm->seqbase, cfra)) {

		/* we take the upper effect strip or 
		   the lowest imagestrip/metastrip */
		seqim= seqeff= 0;

		for(b=1; b<MAXSEQ; b++) {
			if(seq_arr[b]) {
				seq= seq_arr[b];
				if(seq->type & SEQ_EFFECT) {
					if(seqeff==0) seqeff= seq;
					else if(seqeff->machine < seq->machine)
						seqeff= seq;
				}
				else {
					if(seqim==0) seqim= seq;
					else if(seqim->machine > seq->machine)
						seqim= seq;
				}
			}
		}
		if(seqeff) seq= seqeff;
		else if(seqim) seq= seqim;
		else seq= 0;
	}
	
	return seq;
}
 
void set_meta_stripdata(Sequence *seqm)
{
	Sequence *seq;
	StripElem *se;
	int a, cfra;

	/* sets all ->se1 pointers in stripdata, to read the ibuf from it */

	se= seqm->strip->stripdata;
	for(a=0; a<seqm->len; a++, se++) {
		cfra= a+seqm->start;
		seq = get_shown_seq_from_metastrip(seqm, cfra);
		if (seq) {
			se->se1= give_stripelem(seq, cfra);
		} else { 
			se->se1= 0;
		}
	}

}



/* HELP FUNCTIONS FOR GIVE_IBUF_SEQ */

static void do_seq_count_cfra(ListBase *seqbase, int *totseq, int cfra)
{
	Sequence *seq;

	seq= seqbase->first;
	while(seq) {
		if(seq->startdisp <=cfra && seq->enddisp > cfra) {
			(*totseq)++;
		}
		seq= seq->next;
	}
}

static void do_seq_unref_cfra(ListBase *seqbase, int cfra)
{
	Sequence *seq;

	seq= seqbase->first;
	while(seq) {
		if(seq->startdisp <=cfra && seq->enddisp > cfra) {

			if(seq->seqbase.first) {

				if(cfra< seq->start) do_seq_unref_cfra(
					&seq->seqbase, seq->start);
				else if(cfra> seq->start+seq->len-1) 
					do_seq_unref_cfra(
						&seq->seqbase, 
						seq->start+seq->len-1);
				else do_seq_unref_cfra(&seq->seqbase, cfra);
			}

			if (seq->curelem && seq->curelem->ibuf 
			   && seq->curelem->isneeded) {
				IMB_cache_limiter_unref(seq->curelem->ibuf);
			}
		}
		seq= seq->next;
	}
}

static void do_seq_test_unref_cfra(ListBase *seqbase, int cfra)
{
	Sequence *seq;

	seq= seqbase->first;
	while(seq) {
		if(seq->startdisp <=cfra && seq->enddisp > cfra) {

			if(seq->seqbase.first) {

				if(cfra< seq->start) 
					do_seq_test_unref_cfra(
						&seq->seqbase, seq->start);
				else if(cfra> seq->start+seq->len-1) 
					do_seq_test_unref_cfra(
						&seq->seqbase, 
						seq->start+seq->len-1);
				else do_seq_test_unref_cfra(
					&seq->seqbase, cfra);
			}

			if (seq->curelem && seq->curelem->ibuf
				&& seq->curelem->isneeded) {
				if (IMB_cache_limiter_get_refcount(
					    seq->curelem->ibuf)) {
					fprintf(stderr, 
						"sequence.c: imbuf-refcount "
						"Arggh: %p, %d\n", 
						seq, seq->type);
			  }
			}
		}
		seq= seq->next;
	}
}

static void do_build_seq_depend(Sequence * seq, int cfra);

static void do_effect_depend(int cfra, Sequence * seq, StripElem *se)
{
	float fac, facf;
	struct SeqEffectHandle sh = get_sequence_effect(seq);

	if(seq->ipo && seq->ipo->curve.first) {
		do_seq_ipo(seq);
		fac= seq->facf0;
		facf= seq->facf1;
	} else {
		sh.get_default_fac(seq, cfra, &fac, &facf);
	} 

	if( G.scene->r.mode & R_FIELDS ); else facf= fac;
	
	switch (sh.early_out(seq, fac, facf)) {
	case 0:
		do_build_seq_depend(seq->seq1, cfra);
		do_build_seq_depend(seq->seq2, cfra);
		break;
	case 1:
		do_build_seq_depend(seq->seq1, cfra);
		break;
	case 2:
		do_build_seq_depend(seq->seq2, cfra);
		break;
	}

	do_build_seq_depend(seq->seq3, cfra);
}

static void do_build_seq_depend(Sequence * seq, int cfra)
{
	StripElem *se;

	se=seq->curelem= give_stripelem(seq, cfra);

	if(se && !se->isneeded) {
		se->isneeded = 1;
		if(seq->seqbase.first) {
			Sequence * seqmshown= get_shown_seq_from_metastrip(seq, cfra);
			if (seqmshown) {
				if(cfra< seq->start) 
					do_build_seq_depend(seqmshown, seq->start);
				else if(cfra> seq->start+seq->len-1) 
					do_build_seq_depend(seqmshown, seq->start + seq->len-1);
				else do_build_seq_depend(seqmshown, cfra);
			}
		}

		if (seq->type & SEQ_EFFECT) {
			do_effect_depend(cfra, seq, se);
		}
	}
}

static void do_build_seq_ibuf(Sequence * seq, int cfra)
{
	StripElem *se;
	char name[FILE_MAXDIR+FILE_MAXFILE];

	se=seq->curelem= give_stripelem(seq, cfra);

	if(se && se->isneeded) {
		if(seq->type == SEQ_META) {
			se->ok= 2;
			if(se->se1==0) set_meta_stripdata(seq);
			if(se->se1) {
				se->ibuf= se->se1->ibuf;
			}
		}
		else if(seq->type == SEQ_RAM_SOUND || seq->type == SEQ_HD_SOUND) {
			se->ok= 2;
		}
		else if(seq->type & SEQ_EFFECT) {
			
			/* test if image is too small or discarded from cache: reload */
			if(se->ibuf) {
				if(se->ibuf->x < seqrectx || se->ibuf->y < seqrecty || !(se->ibuf->rect || se->ibuf->rect_float)) {
					IMB_freeImBuf(se->ibuf);
					se->ibuf= 0;
				}
			}
			
			/* does the effect should be recalculated? */
			
			if(se->ibuf==0 
			   || (se->se1 != seq->seq1->curelem) 
			   || (se->se2 != seq->seq2->curelem) 
			   || (se->se3 != seq->seq3->curelem)) {
				se->se1= seq->seq1->curelem;
				se->se2= seq->seq2->curelem;
				se->se3= seq->seq3->curelem;
				
				if(se->ibuf==NULL) {
					/* if one of two first inputs are rectfloat, output is float too */
					if((se->se1->ibuf && se->se1->ibuf->rect_float) ||
					   (se->se2->ibuf && se->se2->ibuf->rect_float))
						se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rectfloat, 0);
					else
						se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rect, 0);
				}
				do_effect(cfra, seq, se);
			}
			
			/* test size */
			if(se->ibuf) {
				if(se->ibuf->x != seqrectx || se->ibuf->y != seqrecty ) {
					if(G.scene->r.mode & R_OSA) {
						IMB_scaleImBuf(se->ibuf, (short)seqrectx, (short)seqrecty);
					} else {
						IMB_scalefastImBuf(se->ibuf, (short)seqrectx, (short)seqrecty);
					}
				}
			}
		}
		else if(seq->type < SEQ_EFFECT) {
			if(se->ibuf) {
				/* test if image too small 
				   or discarded from cache: reload */
				if(se->ibuf->x < seqrectx || se->ibuf->y < seqrecty || !(se->ibuf->rect || se->ibuf->rect_float)) {
					IMB_freeImBuf(se->ibuf);
					se->ibuf= 0;
					se->ok= 1;
				}
			}
			
			if(seq->type==SEQ_IMAGE) {
				if(se->ok && se->ibuf==0) {
					/* if playanim or render: 
					   no waitcursor */
					if((G.f & G_PLAYANIM)==0) 
						waitcursor(1);
					
					strncpy(name, seq->strip->dir, FILE_MAXDIR-1);
					strncat(name, se->name, FILE_MAXFILE);
					BLI_convertstringcode(name, G.sce, G.scene->r.cfra);
					se->ibuf= IMB_loadiffname(name, IB_rect);
					
					if((G.f & G_PLAYANIM)==0) 
						waitcursor(0);
					
					if(se->ibuf==0) se->ok= 0;
					else {
						if(seq->flag & SEQ_MAKE_PREMUL) {
							if(se->ibuf->depth==32 && se->ibuf->zbuf==0) converttopremul(se->ibuf);
						}
						seq->strip->orx= se->ibuf->x;
						seq->strip->ory= se->ibuf->y;
						if(seq->flag & SEQ_FILTERY) IMB_filtery(se->ibuf);
						if(seq->mul==0.0) seq->mul= 1.0;
						if(seq->mul != 1.0) multibuf(se->ibuf, seq->mul);
					}
				}
			}
			else if(seq->type==SEQ_MOVIE) {
				if(se->ok && se->ibuf==0) {
					if(seq->anim==0) {
						strncpy(name, seq->strip->dir, FILE_MAXDIR-1);
						strncat(name, seq->strip->stripdata->name, FILE_MAXFILE-1);
						BLI_convertstringcode(name, G.sce, G.scene->r.cfra);
						
						seq->anim = openanim(name, IB_rect);
					}
					if(seq->anim) {
						IMB_anim_set_preseek(seq->anim, seq->anim_preseek);
						se->ibuf = IMB_anim_absolute(seq->anim, se->nr);
					}
					
					if(se->ibuf==0) se->ok= 0;
					else {
						if(seq->flag & SEQ_MAKE_PREMUL) {
							if(se->ibuf->depth==32) converttopremul(se->ibuf);
						}
						seq->strip->orx= se->ibuf->x;
						seq->strip->ory= se->ibuf->y;
						if(seq->flag & SEQ_FILTERY) IMB_filtery(se->ibuf);
						if(seq->mul==0.0) seq->mul= 1.0;
						if(seq->mul != 1.0) multibuf(se->ibuf, seq->mul);
					}
				}
			}
			else if(seq->type==SEQ_SCENE && se->ibuf==0 && seq->scene) {	// scene can be NULL after deletions
				Scene *sce= seq->scene, *oldsce= G.scene;
				Render *re= RE_NewRender(sce->id.name);
				RenderResult rres;
				int doseq;
				
				waitcursor(1);
				
				/* prevent eternal loop */
				doseq= sce->r.scemode & R_DOSEQ;
				sce->r.scemode &= ~R_DOSEQ;
				
				/* hrms, set_scene still needed? work on that... */
				set_scene_bg(sce);
				RE_BlenderFrame(re, sce, seq->sfra + se->nr);
				set_scene_bg(oldsce);
				
				RE_GetResultImage(re, &rres);
				
				if(rres.rectf) {
					se->ibuf= IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_rectfloat, 0);
					memcpy(se->ibuf->rect_float, rres.rectf, 4*sizeof(float)*rres.rectx*rres.recty);
					if(rres.rectz) {
						/* not yet */
					}
				}
				
				/* restore */
				sce->r.scemode |= doseq;
				
				if((G.f & G_PLAYANIM)==0) /* bad, is set on do_render_seq */
					waitcursor(0);
			}
			
			/* size test */
			if(se->ibuf) {
				if(se->ibuf->x != seqrectx || se->ibuf->y != seqrecty ) {
					
					if (0) { // G.scene->r.mode & R_FIELDS) {
						
						if (seqrecty > 288) 
							IMB_scalefieldImBuf(se->ibuf, (short)seqrectx, (short)seqrecty);
						else {
							IMB_de_interlace(se->ibuf);
							
							if(G.scene->r.mode & R_OSA)
								IMB_scaleImBuf(se->ibuf, (short)seqrectx, (short)seqrecty);
							else
								IMB_scalefastImBuf(se->ibuf, (short)seqrectx, (short)seqrecty);
						}
					}
					else {
						if(G.scene->r.mode & R_OSA)
							IMB_scaleImBuf(se->ibuf,(short)seqrectx, (short)seqrecty);
						else
							IMB_scalefastImBuf(se->ibuf, (short)seqrectx, (short)seqrecty);
					}
				}
				
			}
		}
		if (se->ibuf) {
			IMB_cache_limiter_insert(se->ibuf);
			IMB_cache_limiter_ref(se->ibuf);
			IMB_cache_limiter_touch(se->ibuf);
		}
	}
}

static void do_build_seqar_cfra(ListBase *seqbase, Sequence ***seqar, int cfra)
{
	Sequence *seq;
	StripElem *se;

	if(seqar==NULL) return;
	
	seq= seqbase->first;
	while(seq) {

		/* set at zero because free_imbuf_seq... */
		seq->curelem= 0;

		if ((seq->type == SEQ_RAM_SOUND || seq->type == SEQ_HD_SOUND) && (seq->ipo)
		    && (seq->startdisp <= cfra+2) && (seq->enddisp > cfra)) {
			do_seq_ipo(seq);
		}

		if(seq->startdisp <=cfra && seq->enddisp > cfra) {
			**seqar= seq;
			(*seqar)++;

			/* nobody is needed a priori */
			se = seq->curelem= give_stripelem(seq, cfra);
	
			if (se) {
				se->isneeded = 0;
			}
		}

		seq= seq->next;
	}
}

static void do_build_seq_ibufs(ListBase *seqbase, int cfra)
{
	Sequence *seq;

	seq= seqbase->first;
	while(seq) {

		/* set at zero because free_imbuf_seq... */
		seq->curelem= 0;

		if ((seq->type == SEQ_RAM_SOUND || seq->type == SEQ_HD_SOUND) && (seq->ipo)
		    && (seq->startdisp <= cfra+2)  && (seq->enddisp > cfra)) {
			do_seq_ipo(seq);
		}

		if(seq->startdisp <=cfra && seq->enddisp > cfra) {
			if(seq->seqbase.first) {
				if(cfra< seq->start) 
					do_build_seq_ibufs(&seq->seqbase, seq->start);
				else if(cfra> seq->start+seq->len-1) 
					do_build_seq_ibufs(&seq->seqbase, seq->start + seq->len-1);
				else do_build_seq_ibufs(&seq->seqbase, cfra);
			}

			do_build_seq_ibuf(seq, cfra);
		}

		seq= seq->next;
	}
}

ImBuf *give_ibuf_seq(int rectx, int recty, int cfra, int chanshown)
{
	Sequence **tseqar, **seqar;
	Sequence *seq, *seqfirst=0;/*  , *effirst=0; */
	Editing *ed;
	StripElem *se;
	int seqnr, totseq;

	/* we make recursively a 'stack' of sequences, these are
	 * sorted nicely as well.
	 * this method has been developed especially for 
	 * stills before or after metas
	 */

	totseq= 0;
	ed= G.scene->ed;
	if(ed==0) return 0;
	do_seq_count_cfra(ed->seqbasep, &totseq, cfra);

	if(totseq==0) return 0;

	seqrectx= rectx;	/* bad bad global! */
	seqrecty= recty;

	/* tseqar is needed because in do_build_... the pointer changes */
	seqar= tseqar= MEM_callocN(sizeof(void *)*totseq, "seqar");

	/* this call creates the sequence order array */
	do_build_seqar_cfra(ed->seqbasep, &seqar, cfra);

	seqar= tseqar;

	for(seqnr=0; seqnr<totseq; seqnr++) {
		seq= seqar[seqnr];

		se= seq->curelem;
		if((seq->type != SEQ_RAM_SOUND && seq->type != SEQ_HD_SOUND) 
			&& (se) && (chanshown == 0 || seq->machine == chanshown)) {
			if(seq->type==SEQ_META) {

				/* bottom strip! */
				if(seqfirst==0) seqfirst= seq;
				else if(seqfirst->depth > seq->depth) seqfirst= seq;
				else if(seqfirst->machine > seq->machine) seqfirst= seq;

			}
			else if(seq->type & SEQ_EFFECT) {

				/* top strip! */
				if(seqfirst==0) seqfirst= seq;
				else if(seqfirst->depth > seq->depth) seqfirst= seq;
				else if(seqfirst->machine < seq->machine) seqfirst= seq;


			}
			else if(seq->type < SEQ_EFFECT) {	/* images */

				/* bottom strip! a feature that allows you to store junk in locations above */

				if(seqfirst==0) seqfirst= seq;
				else if(seqfirst->depth > seq->depth) seqfirst= seq;
				else if(seqfirst->machine > seq->machine) seqfirst= seq;

			}
		}
	}

	MEM_freeN(seqar);

	/* we know, that we have to build the ibuf of seqfirst, 
	   now build the dependencies and later the ibufs */

	if (seqfirst) {
		do_build_seq_depend(seqfirst, cfra);
		do_build_seq_ibufs(ed->seqbasep, cfra);
		do_seq_unref_cfra(ed->seqbasep, cfra);
		do_seq_test_unref_cfra(ed->seqbasep, cfra);
	}


	if(!seqfirst) return 0;
	if(!seqfirst->curelem) return 0;
	return seqfirst->curelem->ibuf;

}

void free_imbuf_effect_spec(int cfra)
{
	Sequence *seq;
	StripElem *se;
	Editing *ed;
	int a;

	ed= G.scene->ed;
	if(ed==0) return;

	WHILE_SEQ(&ed->seqbase) {

		if(seq->strip) {

			if(seq->type & SEQ_EFFECT) {
				se= seq->strip->stripdata;
				for(a=0; a<seq->len; a++, se++) {
					if(se==seq->curelem && se->ibuf) {
						IMB_freeImBuf(se->ibuf);
						se->ibuf= 0;
						se->ok= 1;
						se->se1= se->se2= se->se3= 0;
					}
				}
			}
		}
	}
	END_SEQ
}

void free_imbuf_seq_except(int cfra)
{
	Sequence *seq;
	StripElem *se;
	Editing *ed;
	int a;

	ed= G.scene->ed;
	if(ed==0) return;

	WHILE_SEQ(&ed->seqbase) {

		if(seq->strip) {

			if( seq->type==SEQ_META ) {
				;
			}
			else {
				se= seq->strip->stripdata;
				for(a=0; a<seq->len; a++, se++) {
					if(se!=seq->curelem && se->ibuf) {
						IMB_freeImBuf(se->ibuf);
						se->ibuf= 0;
						se->ok= 1;
						se->se1= se->se2= se->se3= 0;
					}
				}
			}

			if(seq->type==SEQ_MOVIE) {
				if(seq->startdisp > cfra || seq->enddisp < cfra) {
					if(seq->anim) {
						IMB_free_anim(seq->anim);
						seq->anim = 0;
					}
				}
			}
		}
	}
	END_SEQ
}

void free_imbuf_seq()
{
	Sequence *seq;
	StripElem *se;
	Editing *ed;
	int a;

	ed= G.scene->ed;
	if(ed==0) return;

	WHILE_SEQ(&ed->seqbase) {

		if(seq->strip) {

			if( seq->type==SEQ_META ) {
				;
			}
			else {
				se= seq->strip->stripdata;
				for(a=0; a<seq->len; a++, se++) {
					if(se->ibuf) {
						IMB_freeImBuf(se->ibuf);
						se->ibuf= 0;
						se->ok= 1;
						se->se1= se->se2= se->se3= 0;
					}
				}
			}

			if(seq->type==SEQ_MOVIE) {
				if(seq->anim) {
					IMB_free_anim(seq->anim);
					seq->anim = 0;
				}
			}
		}
	}
	END_SEQ
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
		/* While render let's keep all memory available for render (ton) */
		free_imbuf_seq_except(cfra);
	}
	
	G.f &= ~G_PLAYANIM;

}
