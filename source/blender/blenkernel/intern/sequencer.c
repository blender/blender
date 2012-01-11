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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/sequencer.c
 *  \ingroup bke
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_sound_types.h"

#include "BLI_math.h"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_sequencer.h"
#include "BKE_fcurve.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"

#include "RE_pipeline.h"

#include <pthread.h>

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_context.h"
#include "BKE_sound.h"

#ifdef WITH_AUDASPACE
#  include "AUD_C-API.h"
#endif

static ImBuf* seq_render_strip_stack( 
	SeqRenderData context, ListBase *seqbasep, float cfra, int chanshown);

static ImBuf * seq_render_strip(
	SeqRenderData context, Sequence * seq, float cfra);

static void seq_free_animdata(Scene *scene, Sequence *seq);


/* **** XXX ******** */
#define SELECT 1
ListBase seqbase_clipboard;
int seqbase_clipboard_frame;
SequencerDrawView sequencer_view3d_cb= NULL; /* NULL in background mode */


void printf_strip(Sequence *seq)
{
	fprintf(stderr, "name: '%s', len:%d, start:%d, (startofs:%d, endofs:%d), (startstill:%d, endstill:%d), machine:%d, (startdisp:%d, enddisp:%d)\n",
			seq->name, seq->len, seq->start, seq->startofs, seq->endofs, seq->startstill, seq->endstill, seq->machine, seq->startdisp, seq->enddisp);
	fprintf(stderr, "\tseq_tx_set_final_left: %d %d\n\n", seq_tx_get_final_left(seq, 0), seq_tx_get_final_right(seq, 0));
}

int seqbase_recursive_apply(ListBase *seqbase, int (*apply_func)(Sequence *seq, void *), void *arg)
{
	Sequence *iseq;
	for(iseq= seqbase->first; iseq; iseq= iseq->next) {
		if(seq_recursive_apply(iseq, apply_func, arg) == -1)
			return -1; /* bail out */
	}
	return 1;
}

int seq_recursive_apply(Sequence *seq, int (*apply_func)(Sequence *, void *), void *arg)
{
	int ret= apply_func(seq, arg);

	if(ret == -1)
		return -1;  /* bail out */

	if(ret && seq->seqbase.first)
		ret = seqbase_recursive_apply(&seq->seqbase, apply_func, arg);

	return ret;
}

/* **********************************************************************
   alloc / free functions
   ********************************************************************** */



void new_tstripdata(Sequence *seq)
{
	if(seq->strip) {
		seq->strip->len= seq->len;
	}
}


/* free */

static void free_proxy_seq(Sequence *seq)
{
	if (seq->strip && seq->strip->proxy && seq->strip->proxy->anim) {
		IMB_free_anim(seq->strip->proxy->anim);
		seq->strip->proxy->anim = NULL;
	}
}

void seq_free_strip(Strip *strip)
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
		if (strip->proxy->anim) {
			IMB_free_anim(strip->proxy->anim);
		}

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

	MEM_freeN(strip);
}

void seq_free_sequence(Scene *scene, Sequence *seq)
{
	if(seq->strip) seq_free_strip(seq->strip);

	if(seq->anim) IMB_free_anim(seq->anim);

	if (seq->type & SEQ_EFFECT) {
		struct SeqEffectHandle sh = get_sequence_effect(seq);

		sh.free(seq);
	}

	if(seq->sound) {
		((ID *)seq->sound)->us--; 
	}

	/* clipboard has no scene and will never have a sound handle or be active */
	if(scene) {
		Editing *ed = scene->ed;

		if (ed->act_seq==seq)
			ed->act_seq= NULL;

		if(seq->scene_sound && ELEM(seq->type, SEQ_SOUND, SEQ_SCENE))
			sound_remove_scene_sound(scene, seq->scene_sound);

		seq_free_animdata(scene, seq);
	}

	MEM_freeN(seq);
}

void seq_free_sequence_recurse(Scene *scene, Sequence *seq)
{
	Sequence *iseq;

	for(iseq= seq->seqbase.first; iseq; iseq= iseq->next) {
		seq_free_sequence_recurse(scene, iseq);
	}

	seq_free_sequence(scene, seq);
}


Editing *seq_give_editing(Scene *scene, int alloc)
{
	if (scene->ed == NULL && alloc) {
		Editing *ed;

		ed= scene->ed= MEM_callocN( sizeof(Editing), "addseq");
		ed->seqbasep= &ed->seqbase;
	}
	return scene->ed;
}

static void seq_free_clipboard_recursive(Sequence *seq_parent)
{
	Sequence *seq, *nseq;

	for(seq= seq_parent->seqbase.first; seq; seq= nseq) {
		nseq= seq->next;
		seq_free_clipboard_recursive(seq);
	}

	seq_free_sequence(NULL, seq_parent);
}

void seq_free_clipboard(void)
{
	Sequence *seq, *nseq;

	for(seq= seqbase_clipboard.first; seq; seq= nseq) {
		nseq= seq->next;
		seq_free_clipboard_recursive(seq);
	}
	seqbase_clipboard.first= seqbase_clipboard.last= NULL;
}

void seq_free_editing(Scene *scene)
{
	Editing *ed = scene->ed;
	MetaStack *ms;
	Sequence *seq;

	if(ed==NULL)
		return;

	SEQ_BEGIN(ed, seq) {
		seq_free_sequence(scene, seq);
	}
	SEQ_END

	while((ms= ed->metastack.first)) {
		BLI_remlink(&ed->metastack, ms);
		MEM_freeN(ms);
	}

	MEM_freeN(ed);
}

/* **********************************************************************
   * sequencer pipeline functions
   ********************************************************************** */

SeqRenderData seq_new_render_data(
	struct Main * bmain, struct Scene * scene,
	int rectx, int recty, int preview_render_size)
{
	SeqRenderData rval;

	rval.bmain = bmain;
	rval.scene = scene;
	rval.rectx = rectx;
	rval.recty = recty;
	rval.preview_render_size = preview_render_size;
	rval.motion_blur_samples = 0;
	rval.motion_blur_shutter = 0;

	return rval;
}

int seq_cmp_render_data(const SeqRenderData * a, const SeqRenderData * b)
{
	if (a->preview_render_size < b->preview_render_size) {
		return -1;
	}
	if (a->preview_render_size > b->preview_render_size) {
		return 1;
	}
	
	if (a->rectx < b->rectx) {
		return -1;
	}
	if (a->rectx > b->rectx) {
		return 1;
	}

	if (a->recty < b->recty) {
		return -1;
	}
	if (a->recty > b->recty) {
		return 1;
	}

	if (a->bmain < b->bmain) {
		return -1;
	}
	if (a->bmain > b->bmain) {
		return 1;
	}

	if (a->scene < b->scene) {
		return -1;
	}
	if (a->scene > b->scene) {
		return 1;
	}

	if (a->motion_blur_shutter < b->motion_blur_shutter) {
		return -1;
	}
	if (a->motion_blur_shutter > b->motion_blur_shutter) {
		return 1;
	}

	if (a->motion_blur_samples < b->motion_blur_samples) {
		return -1;
	}
	if (a->motion_blur_samples > b->motion_blur_samples) {
		return 1;
	}

	return 0;
}

unsigned int seq_hash_render_data(const SeqRenderData * a)
{
	unsigned int rval = a->rectx + a->recty;

	rval ^= a->preview_render_size;
	rval ^= ((intptr_t) a->bmain) << 6;
	rval ^= ((intptr_t) a->scene) << 6;
	rval ^= (int) (a->motion_blur_shutter * 100.0f) << 10;
	rval ^= a->motion_blur_samples << 24;
	
	return rval;
}

/* ************************* iterator ************************** */
/* *************** (replaces old WHILE_SEQ) ********************* */
/* **************** use now SEQ_BEGIN() SEQ_END ***************** */

/* sequence strip iterator:
 * - builds a full array, recursively into meta strips */

static void seq_count(ListBase *seqbase, int *tot)
{
	Sequence *seq;

	for(seq=seqbase->first; seq; seq=seq->next) {
		(*tot)++;

		if(seq->seqbase.first)
			seq_count(&seq->seqbase, tot);
	}
}

static void seq_build_array(ListBase *seqbase, Sequence ***array, int depth)
{
	Sequence *seq;

	for(seq=seqbase->first; seq; seq=seq->next) {
		seq->depth= depth;

		if(seq->seqbase.first)
			seq_build_array(&seq->seqbase, array, depth+1);

		**array= seq;
		(*array)++;
	}
}

void seq_array(Editing *ed, Sequence ***seqarray, int *tot, int use_pointer)
{
	Sequence **array;

	*seqarray= NULL;
	*tot= 0;

	if(ed == NULL)
		return;

	if(use_pointer)
		seq_count(ed->seqbasep, tot);
	else
		seq_count(&ed->seqbase, tot);

	if(*tot == 0)
		return;

	*seqarray= array= MEM_mallocN(sizeof(Sequence *)*(*tot), "SeqArray");
	if(use_pointer)
		seq_build_array(ed->seqbasep, &array, 0);
	else
		seq_build_array(&ed->seqbase, &array, 0);
}

void seq_begin(Editing *ed, SeqIterator *iter, int use_pointer)
{
	memset(iter, 0, sizeof(*iter));
	seq_array(ed, &iter->array, &iter->tot, use_pointer);

	if(iter->tot) {
		iter->cur= 0;
		iter->seq= iter->array[iter->cur];
		iter->valid= 1;
	}
}

void seq_next(SeqIterator *iter)
{
	if(++iter->cur < iter->tot)
		iter->seq= iter->array[iter->cur];
	else
		iter->valid= 0;
}

void seq_end(SeqIterator *iter)
{
	if(iter->array)
		MEM_freeN(iter->array);

	iter->valid= 0;
}

/*
  **********************************************************************
  * build_seqar
  **********************************************************************
  * Build a complete array of _all_ sequencies (including those
  * in metastrips!)
  **********************************************************************
*/

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
			do_build_seqar_cb(&seq->seqbase, seqar, depth+1, test_func);
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
		*seqar= NULL;
		return;
	}
	*seqar= MEM_mallocN(sizeof(void *)* *totseq, "seqar");
	tseqar= *seqar;

	do_build_seqar_cb(seqbase, seqar, 0, test_func);
	*seqar= tseqar;
}


void calc_sequence_disp(Scene *scene, Sequence *seq)
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

	seq_update_sound_bounds(scene, seq);
}

static void seq_update_sound_bounds_recursive(Scene *scene, Sequence *metaseq)
{
	Sequence *seq;

	/* for sound we go over full meta tree to update bounds of the sound strips,
	   since sound is played outside of evaluating the imbufs, */
	for(seq=metaseq->seqbase.first; seq; seq=seq->next) {
		if(seq->type == SEQ_META) {
			seq_update_sound_bounds_recursive(scene, seq);
		}
		else if(ELEM(seq->type, SEQ_SOUND, SEQ_SCENE)) {
			if(seq->scene_sound) {
				int startofs = seq->startofs;
				int endofs = seq->endofs;
				if(seq->startofs + seq->start < metaseq->start + metaseq->startofs)
					startofs = metaseq->start + metaseq->startofs - seq->start;

				if(seq->start + seq->len - seq->endofs > metaseq->start + metaseq->len - metaseq->endofs)
					endofs = seq->start + seq->len - metaseq->start - metaseq->len + metaseq->endofs;
				sound_move_scene_sound(scene, seq->scene_sound, seq->start + startofs, seq->start+seq->len - endofs, startofs);
			}
		}
	}
}

void calc_sequence(Scene *scene, Sequence *seq)
{
	Sequence *seqm;
	int min, max;

	/* check all metas recursively */
	seqm= seq->seqbase.first;
	while(seqm) {
		if(seqm->seqbase.first) calc_sequence(scene, seqm);
		seqm= seqm->next;
	}

	/* effects and meta: automatic start and end */

	if(seq->type & SEQ_EFFECT) {
		/* pointers */
		if(seq->seq2==NULL) seq->seq2= seq->seq1;
		if(seq->seq3==NULL) seq->seq3= seq->seq1;

		/* effecten go from seq1 -> seq2: test */

		/* we take the largest start and smallest end */

		// seq->start= seq->startdisp= MAX2(seq->seq1->startdisp, seq->seq2->startdisp);
		// seq->enddisp= MIN2(seq->seq1->enddisp, seq->seq2->enddisp);

		if (seq->seq1) {
			/* XXX These resets should not be necessary, but users used to be able to
			 *     edit effect's length, leading to strange results. See #29190. */
			seq->startofs = seq->endofs = seq->startstill = seq->endstill = 0;
			seq->start= seq->startdisp= MAX3(seq->seq1->startdisp, seq->seq2->startdisp, seq->seq3->startdisp);
			seq->enddisp= MIN3(seq->seq1->enddisp, seq->seq2->enddisp, seq->seq3->enddisp);
			/* we cant help if strips don't overlap, it wont give useful results.
			 * but at least ensure 'len' is never negative which causes bad bugs elsewhere. */
			if(seq->enddisp < seq->startdisp) {
				/* simple start/end swap */
				seq->start= seq->enddisp;
				seq->enddisp = seq->startdisp;
				seq->startdisp= seq->start;
				seq->flag |= SEQ_INVALID_EFFECT;
			}
			else {
				seq->flag &= ~SEQ_INVALID_EFFECT;
			}

			seq->len= seq->enddisp - seq->startdisp;
		}
		else {
			calc_sequence_disp(scene, seq);
		}

		if(seq->strip && seq->len!=seq->strip->len) {
			new_tstripdata(seq);
		}

	}
	else {
		if(seq->type==SEQ_META) {
			seqm= seq->seqbase.first;
			if(seqm) {
				min=  MAXFRAME * 2;
				max= -MAXFRAME * 2;
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
			seq_update_sound_bounds_recursive(scene, seq);
		}
		calc_sequence_disp(scene, seq);
	}
}

/* note: caller should run calc_sequence(scene, seq) after */
void reload_sequence_new_file(Scene *scene, Sequence * seq, int lock_range)
{
	char str[FILE_MAX];
	int prev_startdisp=0, prev_enddisp=0;
	/* note: dont rename the strip, will break animation curves */

	if (ELEM5(seq->type, SEQ_MOVIE, SEQ_IMAGE, SEQ_SOUND, SEQ_SCENE, SEQ_META)==0) {
		return;
	}

	if(lock_range) {
		/* keep so we dont have to move the actual start and end points (only the data) */
		calc_sequence_disp(scene, seq);
		prev_startdisp= seq->startdisp;
		prev_enddisp= seq->enddisp;
	}


	new_tstripdata(seq);

	if (ELEM3(seq->type, SEQ_SCENE, SEQ_META, SEQ_IMAGE)==0) {
		BLI_join_dirfile(str, sizeof(str), seq->strip->dir, seq->strip->stripdata->name);
		BLI_path_abs(str, G.main->name);
	}

	switch(seq->type) {
	case SEQ_IMAGE:
	{
		/* Hack? */
		size_t olen = MEM_allocN_len(seq->strip->stripdata)/sizeof(struct StripElem);

		seq->len = olen;
		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
		break;
	}
	case SEQ_MOVIE:
		if(seq->anim) IMB_free_anim(seq->anim);
		seq->anim = openanim(str, IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0), seq->streamindex);

		if (!seq->anim) {
			return;
		}
	
		seq->len = IMB_anim_get_duration(seq->anim,
						 seq->strip->proxy ?
						 seq->strip->proxy->tc :
						 IMB_TC_RECORD_RUN);
		
		seq->anim_preseek = IMB_anim_get_preseek(seq->anim);

		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
		break;
	case SEQ_SOUND:
#ifdef WITH_AUDASPACE
		if(!seq->sound)
			return;
		seq->len = ceil(AUD_getInfo(seq->sound->playback_handle).length * FPS);
		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
#else
		return;
#endif
		break;
	case SEQ_SCENE:
	{
		/* 'seq->scenenr' should be replaced with something more reliable */
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
		}

		seq->len= seq->scene->r.efra - seq->scene->r.sfra + 1;
		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
		break;
	}
	}

	free_proxy_seq(seq);

	if(lock_range) {
		seq_tx_set_final_left(seq, prev_startdisp);
		seq_tx_set_final_right(seq, prev_enddisp);
		seq_single_fix(seq);
	}
	
	calc_sequence(scene, seq);
}

void sort_seq(Scene *scene)
{
	/* all strips together per kind, and in order of y location ("machine") */
	ListBase seqbase, effbase;
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq, *seqt;

	
	if(ed==NULL) return;

	seqbase.first= seqbase.last= NULL;
	effbase.first= effbase.last= NULL;

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
			if(seqt==NULL) BLI_addtail(&effbase, seq);
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
			if(seqt==NULL) BLI_addtail(&seqbase, seq);
		}
	}

	BLI_movelisttolist(&seqbase, &effbase);
	*(ed->seqbasep)= seqbase;
}


static int clear_scene_in_allseqs_cb(Sequence *seq, void *arg_pt)
{
	if(seq->scene==(Scene *)arg_pt)
		seq->scene= NULL;
	return 1;
}

void clear_scene_in_allseqs(Main *bmain, Scene *scene)
{
	Scene *scene_iter;

	/* when a scene is deleted: test all seqs */
	for(scene_iter= bmain->scene.first; scene_iter; scene_iter= scene_iter->id.next) {
		if(scene_iter != scene && scene_iter->ed) {
			seqbase_recursive_apply(&scene_iter->ed->seqbase, clear_scene_in_allseqs_cb, scene);
		}
	}
}

typedef struct SeqUniqueInfo {
	Sequence *seq;
	char name_src[SEQ_NAME_MAXSTR];
	char name_dest[SEQ_NAME_MAXSTR];
	int count;
	int match;
} SeqUniqueInfo;

/*
static void seqbase_unique_name(ListBase *seqbasep, Sequence *seq)
{
	BLI_uniquename(seqbasep, seq, "Sequence", '.', offsetof(Sequence, name), SEQ_NAME_MAXSTR);
}*/

static void seqbase_unique_name(ListBase *seqbasep, SeqUniqueInfo *sui)
{
	Sequence *seq;
	for(seq=seqbasep->first; seq; seq= seq->next) {
		if (sui->seq != seq && strcmp(sui->name_dest, seq->name+2)==0) {
			sprintf(sui->name_dest, "%.17s.%03d",  sui->name_src, sui->count++); /*24 - 2 for prefix, -1 for \0 */
			sui->match= 1; /* be sure to re-scan */
		}
	}
}

static int seqbase_unique_name_recursive_cb(Sequence *seq, void *arg_pt)
{
	if(seq->seqbase.first)
		seqbase_unique_name(&seq->seqbase, (SeqUniqueInfo *)arg_pt);
	return 1;
}

void seqbase_unique_name_recursive(ListBase *seqbasep, struct Sequence *seq)
{
	SeqUniqueInfo sui;
	char *dot;
	sui.seq= seq;
	BLI_strncpy(sui.name_src, seq->name+2, sizeof(sui.name_src));
	BLI_strncpy(sui.name_dest, seq->name+2, sizeof(sui.name_dest));

	sui.count= 1;
	sui.match= 1; /* assume the worst to start the loop */

	/* Strip off the suffix */
	if ((dot=strrchr(sui.name_src, '.'))) {
		*dot= '\0';
		dot++;

		if(*dot)
			sui.count= atoi(dot) + 1;
	}

	while(sui.match) {
		sui.match= 0;
		seqbase_unique_name(seqbasep, &sui);
		seqbase_recursive_apply(seqbasep, seqbase_unique_name_recursive_cb, &sui);
	}

	BLI_strncpy(seq->name+2, sui.name_dest, sizeof(seq->name)-2);
}

static const char *give_seqname_by_type(int type)
{
	switch(type) {
	case SEQ_META:	     return "Meta";
	case SEQ_IMAGE:      return "Image";
	case SEQ_SCENE:      return "Scene";
	case SEQ_MOVIE:      return "Movie";
	case SEQ_SOUND:      return "Audio";
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
	case SEQ_MULTICAM:   return "Multicam";
	case SEQ_ADJUSTMENT: return "Adjustment";
	case SEQ_SPEED:      return "Speed";
	default:
		return NULL;
	}
}

const char *give_seqname(Sequence *seq)
{
	const char *name = give_seqname_by_type(seq->type);

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

	if(ibuf==NULL || (ibuf->rect==NULL && ibuf->rect_float==NULL)) return;

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

	mul= (int)(256.0f * fmul);
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

static float give_stripelem_index(Sequence *seq, float cfra)
{
	float nr;
	int sta = seq->start;
	int end = seq->start+seq->len-1;

	if (seq->type & SEQ_EFFECT) {
		end = seq->enddisp;
	} 

	if(end < sta) {
		return -1;
	}

	if(seq->flag&SEQ_REVERSE_FRAMES) {	
		/*reverse frame in this sequence */
		if(cfra <= sta) nr= end - sta;
		else if(cfra >= end) nr= 0;
		else nr= end - cfra;
	} else {
		if(cfra <= sta) nr= 0;
		else if(cfra >= end) nr= end - sta;
		else nr= cfra - sta;
	}
	
	if (seq->strobe < 1.0f) seq->strobe = 1.0f;
	
	if (seq->strobe > 1.0f) {
		nr -= fmodf((double)nr, (double)seq->strobe);
	}

	return nr;
}

StripElem *give_stripelem(Sequence *seq, int cfra)
{
	StripElem *se= seq->strip->stripdata;

	if(seq->type == SEQ_IMAGE) { /* only 
					IMAGE strips use the whole array,
					MOVIE strips use only 
					the first element, all other strips
					don't use this... */
		int nr = (int) give_stripelem_index(seq, cfra);

		if (nr == -1 || se == NULL) return NULL;
	
		se += nr + seq->anim_startofs;
	}
	return se;
}

static int evaluate_seq_frame_gen(Sequence ** seq_arr, ListBase *seqbase, int cfra)
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

int evaluate_seq_frame(Scene *scene, int cfra)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq_arr[MAXSEQ+1];

	if(ed==NULL) return 0;
	return evaluate_seq_frame_gen(seq_arr, ed->seqbasep, cfra);
}

static int video_seq_is_rendered(Sequence * seq)
{
	return (seq && !(seq->flag & SEQ_MUTE) && seq->type != SEQ_SOUND);
}

static int get_shown_sequences(	ListBase * seqbasep, int cfra, int chanshown, Sequence ** seq_arr_out)
{
	Sequence *seq_arr[MAXSEQ+1];
	int b = chanshown;
	int cnt = 0;

	if (b > MAXSEQ) {
		return 0;
	}

	if(evaluate_seq_frame_gen(seq_arr, seqbasep, cfra)) {
		if (b == 0) {
			b = MAXSEQ;
		}
		for (; b > 0; b--) {
			if (video_seq_is_rendered(seq_arr[b])) {
				break;
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

	for (;b <= chanshown && b >= 0; b++) {
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

static IMB_Proxy_Size seq_rendersize_to_proxysize(int size)
{
	if (size >= 100) {
		return IMB_PROXY_NONE;
	}
	if (size >= 99) {
		return IMB_PROXY_100;
	}
	if (size >= 75) {
		return IMB_PROXY_75;
	}
	if (size >= 50) {
		return IMB_PROXY_50;
	}
	return IMB_PROXY_25;
}

static void seq_open_anim_file(Sequence * seq)
{
	char name[FILE_MAX];
	StripProxy * proxy;

	if(seq->anim != NULL) {
		return;
	}

	BLI_join_dirfile(name, sizeof(name),
	                 seq->strip->dir, seq->strip->stripdata->name);
	BLI_path_abs(name, G.main->name);
	
	seq->anim = openanim(name, IB_rect |
	                     ((seq->flag & SEQ_FILTERY) ?
	                          IB_animdeinterlace : 0), seq->streamindex);

	if (seq->anim == NULL) {
		return;
	}

	proxy = seq->strip->proxy;

	if (proxy == NULL) {
		return;
	}

	if (seq->flag & SEQ_USE_PROXY_CUSTOM_DIR) {
		IMB_anim_set_index_dir(seq->anim, seq->strip->proxy->dir);
	}
}


static int seq_proxy_get_fname(SeqRenderData context, Sequence * seq, int cfra, char * name)
{
	int frameno;
	char dir[PROXY_MAXFILE];
	int render_size = context.preview_render_size;

	if (!seq->strip->proxy) {
		return FALSE;
	}

	/* MOVIE tracks (only exception: custom files) are now handled 
	   internally by ImBuf module for various reasons: proper time code 
	   support, quicker index build, using one file instead 
	   of a full directory of jpeg files, etc. Trying to support old
	   and new method at once could lead to funny effects, if people
	   have both, a directory full of jpeg files and proxy avis, so
	   sorry folks, please rebuild your proxies... */

	if (seq->flag & (SEQ_USE_PROXY_CUSTOM_DIR|SEQ_USE_PROXY_CUSTOM_FILE)) {
		BLI_strncpy(dir, seq->strip->proxy->dir, sizeof(dir));
	} else if (seq->type == SEQ_IMAGE) {
		BLI_snprintf(dir, PROXY_MAXFILE, "%s/BL_proxy", seq->strip->dir);
	} else {
		return FALSE;
	}

	if (seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) {
		BLI_join_dirfile(name, PROXY_MAXFILE,
		                 dir, seq->strip->proxy->file);
		BLI_path_abs(name, G.main->name);

		return TRUE;
	}

	/* dirty hack to distinguish 100% render size from PROXY_100 */
	if (render_size == 99) {
		render_size = 100;
	}

	/* generate a separate proxy directory for each preview size */

	if (seq->type == SEQ_IMAGE) {
		BLI_snprintf(name, PROXY_MAXFILE, "%s/images/%d/%s_proxy", dir,
		             context.preview_render_size,
		             give_stripelem(seq, cfra)->name);
		frameno = 1;
	} else {
		frameno = (int) give_stripelem_index(seq, cfra) + seq->anim_startofs;
		BLI_snprintf(name, PROXY_MAXFILE, "%s/proxy_misc/%d/####", dir, 
		             context.preview_render_size);
	}

	BLI_path_abs(name, G.main->name);
	BLI_path_frame(name, frameno, 0);

	strcat(name, ".jpg");

	return TRUE;
}

static struct ImBuf * seq_proxy_fetch(SeqRenderData context, Sequence * seq, int cfra)
{
	char name[PROXY_MAXFILE];
	IMB_Proxy_Size psize = seq_rendersize_to_proxysize(
		context.preview_render_size);
	int size_flags;

	if (!(seq->flag & SEQ_USE_PROXY)) {
		return NULL;
	}

	size_flags = seq->strip->proxy->build_size_flags;

	/* only use proxies, if they are enabled (even if present!) */
	if (psize == IMB_PROXY_NONE || ((size_flags & psize) != psize)) {
		return NULL;
	}

	if (seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) {
		int frameno = (int) give_stripelem_index(seq, cfra) + seq->anim_startofs;
		if (seq->strip->proxy->anim == NULL) {
			if (seq_proxy_get_fname(context, seq, cfra, name)==0) {
				return NULL;
			}
 
			seq->strip->proxy->anim = openanim(name, IB_rect, 0);
		}
		if (seq->strip->proxy->anim==NULL) {
			return NULL;
		}
 
		seq_open_anim_file(seq);

		frameno = IMB_anim_index_get_frame_index(
			seq->anim, seq->strip->proxy->tc, frameno);

		return IMB_anim_absolute(seq->strip->proxy->anim, frameno,
					 IMB_TC_NONE, IMB_PROXY_NONE);
	}
 
	if (seq_proxy_get_fname(context, seq, cfra, name) == 0) {
		return NULL;
	}

	if (BLI_exists(name)) {
		return IMB_loadiffname(name, IB_rect);
	} else {
		return NULL;
	}
}

static void seq_proxy_build_frame(SeqRenderData context,
				  Sequence* seq, int cfra,
				  int proxy_render_size)
{
	char name[PROXY_MAXFILE];
	int quality;
	int rectx, recty;
	int ok;
	struct ImBuf * ibuf;

	if (!seq_proxy_get_fname(context, seq, cfra, name)) {
		return;
	}

	ibuf = seq_render_strip(context, seq, cfra);

	rectx = (proxy_render_size * context.scene->r.xsch) / 100;
	recty = (proxy_render_size * context.scene->r.ysch) / 100;

	if (ibuf->x != rectx || ibuf->y != recty) {
		IMB_scalefastImBuf(ibuf, (short)rectx, (short)recty);
	}

	/* depth = 32 is intentionally left in, otherwise ALPHA channels
	   won't work... */
	quality = seq->strip->proxy->quality;
	ibuf->ftype= JPG | quality;

	/* unsupported feature only confuses other s/w */
	if(ibuf->planes==32)
		ibuf->planes= 24;

	BLI_make_existing_file(name);
	
	ok = IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat);
	if (ok == 0) {
		perror(name);
	}

	IMB_freeImBuf(ibuf);
}

void seq_proxy_rebuild(struct Main * bmain, Scene *scene, Sequence * seq,
		       short *stop, short *do_update, float *progress)
{
	SeqRenderData context;
	int cfra;
	int tc_flags;
	int size_flags;
	int quality;

	if (!seq->strip || !seq->strip->proxy) {
		return;
	}

	if (!(seq->flag & SEQ_USE_PROXY)) {
		return;
	}

	tc_flags   = seq->strip->proxy->build_tc_flags;
	size_flags = seq->strip->proxy->build_size_flags;
	quality    = seq->strip->proxy->quality;

	if (seq->type == SEQ_MOVIE) {
		seq_open_anim_file(seq);

		if (seq->anim) {
			IMB_anim_index_rebuild(
				seq->anim, tc_flags, size_flags, quality,
				stop, do_update, progress);
		}
		return;
	}

	if (!(seq->flag & SEQ_USE_PROXY)) {
		return;
	}

	/* that's why it is called custom... */
	if (seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) {
		return;
	}

	/* fail safe code */

	context = seq_new_render_data(
		bmain, scene, 
		(scene->r.size * (float) scene->r.xsch) / 100.0f + 0.5f, 
		(scene->r.size * (float) scene->r.ysch) / 100.0f + 0.5f, 
		100);

	for (cfra = seq->startdisp + seq->startstill; 
	     cfra < seq->enddisp - seq->endstill; cfra++) {
		if (size_flags & IMB_PROXY_25) {
			seq_proxy_build_frame(context, seq, cfra, 25);
		}
		if (size_flags & IMB_PROXY_50) {
			seq_proxy_build_frame(context, seq, cfra, 50);
		}
		if (size_flags & IMB_PROXY_75) {
			seq_proxy_build_frame(context, seq, cfra, 75);
		}
		if (size_flags & IMB_PROXY_100) {
			seq_proxy_build_frame(context, seq, cfra, 100);
		}

		*progress= (float)cfra/(seq->enddisp - seq->endstill 
					- seq->startdisp + seq->startstill);
		*do_update= 1;

		if(*stop || G.afbreek)
			break;
	}
}


/* **********************************************************************
   color balance 
   ********************************************************************** */

static StripColorBalance calc_cb(StripColorBalance * cb_)
{
	StripColorBalance cb = *cb_;
	int c;

	for (c = 0; c < 3; c++) {
		cb.lift[c] = 2.0f - cb.lift[c];
	}

	if(cb.flag & SEQ_COLOR_BALANCE_INVERSE_LIFT) {
		for (c = 0; c < 3; c++) {
			/* tweak to give more subtle results
			 * values above 1.0 are scaled */
			if(cb.lift[c] > 1.0f)
				cb.lift[c] = pow(cb.lift[c] - 1.0f, 2.0) + 1.0;

			cb.lift[c] = 2.0f - cb.lift[c];
		}
	}

	if (cb.flag & SEQ_COLOR_BALANCE_INVERSE_GAIN) {
		for (c = 0; c < 3; c++) {
			if (cb.gain[c] != 0.0f) {
				cb.gain[c] = 1.0f / cb.gain[c];
			} else {
				cb.gain[c] = 1000000; /* should be enough :) */
			}
		}
	}

	if (!(cb.flag & SEQ_COLOR_BALANCE_INVERSE_GAMMA)) {
		for (c = 0; c < 3; c++) {
			if (cb.gamma[c] != 0.0f) {
				cb.gamma[c] = 1.0f/cb.gamma[c];
			} else {
				cb.gamma[c] = 1000000; /* should be enough :) */
			}
		}
	}

	return cb;
}

/* note: lift is actually 2-lift */
MINLINE float color_balance_fl(float in, const float lift, const float gain, const float gamma, const float mul)
{
	float x= (((in - 1.0f) * lift) + 1.0f) * gain;

	/* prevent NaN */
	if (x < 0.f) x = 0.f;

	return powf(x, gamma) * mul;
}

static void make_cb_table_byte(float lift, float gain, float gamma,
				   unsigned char * table, float mul)
{
	int y;

	for (y = 0; y < 256; y++) {
		float v= color_balance_fl((float)y * (1.0f / 255.0f), lift, gain, gamma, mul);
		CLAMP(v, 0.0f, 1.0f);
		table[y] = v * 255;
	}
}

static void make_cb_table_float(float lift, float gain, float gamma,
				float * table, float mul)
{
	int y;

	for (y = 0; y < 256; y++) {
		float v= color_balance_fl((float)y * (1.0f / 255.0f), lift, gain, gamma, mul);
		table[y] = v;
	}
}

static void color_balance_byte_byte(Sequence * seq, ImBuf* ibuf, float mul)
{
	unsigned char cb_tab[3][256];
	int c;
	unsigned char * p = (unsigned char*) ibuf->rect;
	unsigned char * e = p + ibuf->x * 4 * ibuf->y;

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

static void color_balance_byte_float(Sequence * seq, ImBuf* ibuf, float mul)
{
	float cb_tab[4][256];
	int c,i;
	unsigned char * p = (unsigned char*) ibuf->rect;
	unsigned char * e = p + ibuf->x * 4 * ibuf->y;
	float * o;
	StripColorBalance cb;

	imb_addrectfloatImBuf(ibuf);

	o = ibuf->rect_float;

	cb = calc_cb(seq->strip->color_balance);

	for (c = 0; c < 3; c++) {
		make_cb_table_float(cb.lift[c], cb.gain[c], cb.gamma[c], cb_tab[c], mul);
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

static void color_balance_float_float(Sequence * seq, ImBuf* ibuf, float mul)
{
	float * p = ibuf->rect_float;
	float * e = ibuf->rect_float + ibuf->x * 4* ibuf->y;
	StripColorBalance cb = calc_cb(seq->strip->color_balance);

	while (p < e) {
		int c;
		for (c = 0; c < 3; c++) {
			p[c]= color_balance_fl(p[c], cb.lift[c], cb.gain[c], cb.gamma[c], mul);
		}
		p += 4;
	}
}

static void color_balance(Sequence * seq, ImBuf* ibuf, float mul)
{
	if (ibuf->rect_float) {
		color_balance_float_float(seq, ibuf, mul);
	} else if(seq->flag & SEQ_MAKE_FLOAT) {
		color_balance_byte_float(seq, ibuf, mul);
	} else {
		color_balance_byte_byte(seq, ibuf, mul);
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

int input_have_to_preprocess(
	SeqRenderData UNUSED(context), Sequence * seq, float UNUSED(cfra))
{
	float mul;

	if (seq->flag & (SEQ_FILTERY|SEQ_USE_CROP|SEQ_USE_TRANSFORM|SEQ_FLIPX|
			 SEQ_FLIPY|SEQ_USE_COLOR_BALANCE|SEQ_MAKE_PREMUL)) {
		return TRUE;
	}

	mul = seq->mul;

	if(seq->blend_mode == SEQ_BLEND_REPLACE) {
		mul *= seq->blend_opacity / 100.0f;
	}

	if (mul != 1.0f) {
		return TRUE;
	}

	if (seq->sat != 1.0f) {
		return TRUE;
	}
		
	return FALSE;
}

static ImBuf * input_preprocess(
	SeqRenderData context, Sequence *seq, float UNUSED(cfra), ImBuf * ibuf)
{
	float mul;

	ibuf = IMB_makeSingleUser(ibuf);

	if((seq->flag & SEQ_FILTERY) && seq->type != SEQ_MOVIE) {
		IMB_filtery(ibuf);
	}

	if(seq->flag & (SEQ_USE_CROP|SEQ_USE_TRANSFORM)) {
		StripCrop c= {0};
		StripTransform t= {0};
		int sx,sy,dx,dy;

		if(seq->flag & SEQ_USE_CROP && seq->strip->crop) {
			c = *seq->strip->crop;
		}
		if(seq->flag & SEQ_USE_TRANSFORM && seq->strip->transform) {
			t = *seq->strip->transform;
		}

		sx = ibuf->x - c.left - c.right;
		sy = ibuf->y - c.top - c.bottom;
		dx = sx;
		dy = sy;

		if (seq->flag & SEQ_USE_TRANSFORM) {
			dx = context.scene->r.xsch;
			dy = context.scene->r.ysch;
		}

		if (c.top + c.bottom >= ibuf->y || c.left + c.right >= ibuf->x ||
				t.xofs >= dx || t.yofs >= dy) {
			make_black_ibuf(ibuf);
		} else {
			ImBuf * i = IMB_allocImBuf(dx, dy,32, ibuf->rect_float ? IB_rectfloat : IB_rect);

			IMB_rectcpy(i, ibuf, t.xofs, t.yofs, c.left, c.bottom, sx, sy);
			
			IMB_freeImBuf(ibuf);

			ibuf = i;
		}
	} 

	if(seq->flag & SEQ_FLIPX) {
		IMB_flipx(ibuf);
	}
	
	if(seq->flag & SEQ_FLIPY) {
		IMB_flipy(ibuf);
	}

	if(seq->sat != 1.0f) {
		/* inline for now, could become an imbuf function */
		int i;
		unsigned char *rct= (unsigned char *)ibuf->rect;
		float *rctf= ibuf->rect_float;
		const float sat= seq->sat;
		float hsv[3];

		if(rct) {
			float rgb[3];
			for (i = ibuf->x * ibuf->y; i > 0; i--, rct+=4) {
				rgb_byte_to_float(rct, rgb);
				rgb_to_hsv(rgb[0], rgb[1], rgb[2], hsv, hsv+1, hsv+2);
				hsv_to_rgb(hsv[0], hsv[1] * sat, hsv[2], rgb, rgb+1, rgb+2);
				rgb_float_to_byte(rgb, rct);
			}
		}

		if(rctf) {
			for (i = ibuf->x * ibuf->y; i > 0; i--, rctf+=4) {
				rgb_to_hsv(rctf[0], rctf[1], rctf[2], hsv, hsv+1, hsv+2);
				hsv_to_rgb(hsv[0], hsv[1] * sat, hsv[2], rctf, rctf+1, rctf+2);
			}
		}
	}

	mul = seq->mul;

	if(seq->blend_mode == SEQ_BLEND_REPLACE) {
		mul *= seq->blend_opacity / 100.0f;
	}

	if(seq->flag & SEQ_USE_COLOR_BALANCE && seq->strip->color_balance) {
		color_balance(seq, ibuf, mul);
		mul = 1.0;
	}

	if(seq->flag & SEQ_MAKE_FLOAT) {
		if (!ibuf->rect_float)
			IMB_float_from_rect_simple(ibuf);

		if (ibuf->rect) {
			imb_freerectImBuf(ibuf);
		}
	}

	if(mul != 1.0f) {
		multibuf(ibuf, mul);
	}

	if(seq->flag & SEQ_MAKE_PREMUL) {
		if(ibuf->planes == 32 && ibuf->zbuf == NULL) {
			IMB_premultiply_alpha(ibuf);
		}
	}


	if(ibuf->x != context.rectx || ibuf->y != context.recty ) {
		if(context.scene->r.mode & R_OSA) {
			IMB_scaleImBuf(ibuf, (short)context.rectx, (short)context.recty);
		} else {
			IMB_scalefastImBuf(ibuf, (short)context.rectx, (short)context.recty);
		}
	}
	return ibuf;
}

static ImBuf * copy_from_ibuf_still(SeqRenderData context, Sequence * seq, 
				    float nr)
{
	ImBuf * rval = NULL;
	ImBuf * ibuf = NULL;

	if (nr == 0) {
		ibuf = seq_stripelem_cache_get(
			context, seq, seq->start, 
			SEQ_STRIPELEM_IBUF_STARTSTILL);
	} else if (nr == seq->len - 1) {
		ibuf = seq_stripelem_cache_get(
			context, seq, seq->start, 
			SEQ_STRIPELEM_IBUF_ENDSTILL);
	}

	if (ibuf) {
		rval = IMB_dupImBuf(ibuf);
		IMB_freeImBuf(ibuf);
	}

	return rval;
}

static void copy_to_ibuf_still(SeqRenderData context, Sequence * seq, float nr,
			       ImBuf * ibuf)
{
	if (nr == 0 || nr == seq->len - 1) {
		/* we have to store a copy, since the passed ibuf
		   could be preprocessed afterwards (thereby silently
		   changing the cached image... */
		ibuf = IMB_dupImBuf(ibuf);

		if (nr == 0) {
			seq_stripelem_cache_put(
				context, seq, seq->start, 
				SEQ_STRIPELEM_IBUF_STARTSTILL, ibuf);
		} 

		if (nr == seq->len - 1) {
			seq_stripelem_cache_put(
				context, seq, seq->start, 
				SEQ_STRIPELEM_IBUF_ENDSTILL, ibuf);
		}

		IMB_freeImBuf(ibuf);
	}
}

/* **********************************************************************
   strip rendering functions
   ********************************************************************** */

static ImBuf* seq_render_strip_stack( 
	SeqRenderData context, ListBase *seqbasep, float cfra, int chanshown);

static ImBuf * seq_render_strip(
	SeqRenderData context, Sequence * seq, float cfra);


static ImBuf* seq_render_effect_strip_impl(
	SeqRenderData context, Sequence *seq, float cfra)
{
	float fac, facf;
	int early_out;
	int i;
	struct SeqEffectHandle sh = get_sequence_effect(seq);
	FCurve *fcu= NULL;
	ImBuf * ibuf[3];
	Sequence *input[3];
	ImBuf * out = NULL;

	ibuf[0] = ibuf[1] = ibuf[2] = NULL;

	input[0] = seq->seq1; input[1] = seq->seq2; input[2] = seq->seq3;

	if (!sh.execute) { /* effect not supported in this version... */
		out = IMB_allocImBuf((short)context.rectx, 
				     (short)context.recty, 32, IB_rect);
		return out;
	}

	if (seq->flag & SEQ_USE_EFFECT_DEFAULT_FADE) {
		sh.get_default_fac(seq, cfra, &fac, &facf);
		
		if ((context.scene->r.mode & R_FIELDS)==0)
			facf= fac;
	}
	else {
		fcu = id_data_find_fcurve(&context.scene->id, seq, &RNA_Sequence, "effect_fader", 0, NULL);
		if (fcu) {
			fac = facf = evaluate_fcurve(fcu, cfra);
			if( context.scene->r.mode & R_FIELDS ) {
				facf = evaluate_fcurve(fcu, cfra + 0.5f);
			}
		} else {
			fac = facf = seq->effect_fader;
		}
	}

	early_out = sh.early_out(seq, fac, facf);

	switch (early_out) {
	case EARLY_NO_INPUT:
		out = sh.execute(context, seq, cfra, fac, facf,
		                 NULL, NULL, NULL);
		break;
	case EARLY_DO_EFFECT:
		for(i=0; i<3; i++) {
			if(input[i])
				ibuf[i] = seq_render_strip(
				            context, input[i], cfra);
		}

		if (ibuf[0] && ibuf[1]) {
			out = sh.execute(context, seq, cfra, fac, facf,  
					 ibuf[0], ibuf[1], ibuf[2]);
		}
		break;
	case EARLY_USE_INPUT_1:
		if (input[0]) {
			ibuf[0] = seq_render_strip(context, input[0], cfra);
		}
		if (ibuf[0]) {
			if (input_have_to_preprocess(context, seq, cfra)) {
				out = IMB_dupImBuf(ibuf[0]);
			} else {
				out = ibuf[0];
				IMB_refImBuf(out);
			}
		}
		break;
	case EARLY_USE_INPUT_2:
		if (input[1]) {
			ibuf[1] = seq_render_strip(context, input[1], cfra);
		}
		if (ibuf[1]) {
			if (input_have_to_preprocess(context, seq, cfra)) {
				out = IMB_dupImBuf(ibuf[1]);
			} else {
				out = ibuf[1];
				IMB_refImBuf(out);
			}
		}
		break;
	}

	for (i = 0; i < 3; i++) {
		IMB_freeImBuf(ibuf[i]);
	}

	if (out == NULL) {
		out = IMB_allocImBuf((short)context.rectx, (short)context.recty, 32, IB_rect);
	}

	return out;
}


static ImBuf * seq_render_scene_strip_impl(
	SeqRenderData context, Sequence * seq, float nr)
{
	ImBuf * ibuf = NULL;
	float frame= seq->sfra + nr + seq->anim_startofs;
	float oldcfra;
	Object *camera;
	ListBase oldmarkers;
	
	/* Old info:
	   Hack! This function can be called from do_render_seq(), in that case
	   the seq->scene can already have a Render initialized with same name,
	   so we have to use a default name. (compositor uses scene name to
	   find render).
	   However, when called from within the UI (image preview in sequencer)
	   we do want to use scene Render, that way the render result is defined
	   for display in render/imagewindow
	   
	   Hmm, don't see, why we can't do that all the time,
	   and since G.rendering is uhm, gone... (Peter)
	*/

	/* New info:
	   Using the same name for the renders works just fine as the do_render_seq()
	   render is not used while the scene strips are rendered.
	   
	   However rendering from UI (through sequencer_preview_area_draw) can crash in
	   very many cases since other renders (material preview, an actual render etc.)
	   can be started while this sequence preview render is running. The only proper
	   solution is to make the sequencer preview render a proper job, which can be
	   stopped when needed. This would also give a nice progress bar for the preview
	   space so that users know there's something happening.

	   As a result the active scene now only uses OpenGL rendering for the sequencer
	   preview. This is far from nice, but is the only way to prevent crashes at this
	   time. 

	   -jahka
	*/

	int rendering = G.rendering;
	int doseq;
	int doseq_gl= G.rendering ? /*(scene->r.seq_flag & R_SEQ_GL_REND)*/ 0 : /*(scene->r.seq_flag & R_SEQ_GL_PREV)*/ 1;
	int have_seq= FALSE;
	Scene *scene;

	/* dont refer to seq->scene above this point!, it can be NULL */
	if(seq->scene == NULL) {
		return NULL;
	}

	scene= seq->scene;

	have_seq= (scene->r.scemode & R_DOSEQ) && scene->ed && scene->ed->seqbase.first;

	oldcfra= scene->r.cfra;	
	scene->r.cfra= frame;

	if(seq->scene_camera)	
		camera= seq->scene_camera;
	else {	
		scene_camera_switch_update(scene);
		camera= scene->camera;
	}

	if(have_seq==FALSE && camera==NULL) {
		scene->r.cfra= oldcfra;
		return NULL;
	}

	/* prevent eternal loop */
	doseq= context.scene->r.scemode & R_DOSEQ;
	context.scene->r.scemode &= ~R_DOSEQ;
	
#ifdef DURIAN_CAMERA_SWITCH
	/* stooping to new low's in hackyness :( */
	oldmarkers= scene->markers;
	scene->markers.first= scene->markers.last= NULL;
#else
	(void)oldmarkers;
#endif
	
	if(sequencer_view3d_cb && BLI_thread_is_main() && doseq_gl && (scene == context.scene || have_seq==0) && camera) {
		char err_out[256]= "unknown";
		/* for old scened this can be uninitialized, should probably be added to do_versions at some point if the functionality stays */
		if(context.scene->r.seq_prev_type==0)
			context.scene->r.seq_prev_type = 3 /* ==OB_SOLID */; 

		/* opengl offscreen render */
		scene_update_for_newframe(context.bmain, scene, scene->lay);
		ibuf= sequencer_view3d_cb(scene, camera, context.rectx, context.recty, IB_rect, context.scene->r.seq_prev_type, err_out);
		if(ibuf == NULL) {
			fprintf(stderr, "seq_render_scene_strip_impl failed to get opengl buffer: %s\n", err_out);
		}
	}
	else {
		Render *re = RE_GetRender(scene->id.name);
		RenderResult rres;

		/* XXX: this if can be removed when sequence preview rendering uses the job system */
		if(rendering || context.scene != scene) {
			if(re==NULL)
				re= RE_NewRender(scene->id.name);
			
			RE_BlenderFrame(re, context.bmain, scene, NULL, camera, scene->lay, frame, FALSE);

			/* restore previous state after it was toggled on & off by RE_BlenderFrame */
			G.rendering = rendering;
		}
		
		RE_AcquireResultImage(re, &rres);
		
		if(rres.rectf) {
			ibuf= IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_rectfloat);
			memcpy(ibuf->rect_float, rres.rectf, 4*sizeof(float)*rres.rectx*rres.recty);
			if(rres.rectz) {
				addzbuffloatImBuf(ibuf);
				memcpy(ibuf->zbuf_float, rres.rectz, sizeof(float)*rres.rectx*rres.recty);
			}

			/* float buffers in the sequencer are not linear */
			if(scene->r.color_mgt_flag & R_COLOR_MANAGEMENT)
				ibuf->profile= IB_PROFILE_LINEAR_RGB;
			else
				ibuf->profile= IB_PROFILE_NONE;
			IMB_convert_profile(ibuf, IB_PROFILE_SRGB);			
		}
		else if (rres.rect32) {
			ibuf= IMB_allocImBuf(rres.rectx, rres.recty, 32, IB_rect);
			memcpy(ibuf->rect, rres.rect32, 4*rres.rectx*rres.recty);
		}
		
		RE_ReleaseResultImage(re);
		
		// BIF_end_render_callbacks();
	}
	
	/* restore */
	context.scene->r.scemode |= doseq;
	
	scene->r.cfra = oldcfra;

	if(frame != oldcfra)
		scene_update_for_newframe(context.bmain, scene, scene->lay);
	
#ifdef DURIAN_CAMERA_SWITCH
	/* stooping to new low's in hackyness :( */
	scene->markers= oldmarkers;
#endif

	return ibuf;
}

static ImBuf * seq_render_strip(SeqRenderData context, Sequence * seq, float cfra)
{
	ImBuf * ibuf = NULL;
	char name[FILE_MAX];
	int use_preprocess = input_have_to_preprocess(context, seq, cfra);
	float nr = give_stripelem_index(seq, cfra);
	/* all effects are handled similarly with the exception of speed effect */
	int type = (seq->type & SEQ_EFFECT && seq->type != SEQ_SPEED) ? SEQ_EFFECT : seq->type;

	ibuf = seq_stripelem_cache_get(context, seq, cfra, SEQ_STRIPELEM_IBUF);

	/* currently, we cache preprocessed images in SEQ_STRIPELEM_IBUF,
	   but not(!) on SEQ_STRIPELEM_IBUF_ENDSTILL and ..._STARTSTILL */
	if (ibuf)
		use_preprocess = FALSE;

	if (ibuf == NULL)
		ibuf = copy_from_ibuf_still(context, seq, nr);
	
	if (ibuf == NULL)
		ibuf = seq_proxy_fetch(context, seq, cfra);

	if(ibuf == NULL) switch(type) {
		case SEQ_META:
		{
			ImBuf * meta_ibuf = NULL;

			if(seq->seqbase.first)
				meta_ibuf = seq_render_strip_stack(
					context, &seq->seqbase,
					seq->start + nr, 0);

			if(meta_ibuf) {
				ibuf = meta_ibuf;
				if(ibuf && use_preprocess) {
					struct ImBuf * i = IMB_dupImBuf(ibuf);

					IMB_freeImBuf(ibuf);

					ibuf = i;
				}
			}
			break;
		}
		case SEQ_SPEED:
		{
			ImBuf * child_ibuf = NULL;

			float f_cfra;
			SpeedControlVars * s = (SpeedControlVars *)seq->effectdata;

			sequence_effect_speed_rebuild_map(context.scene,seq, 0);

			/* weeek! */
			f_cfra = seq->start + s->frameMap[(int) nr];

			child_ibuf = seq_render_strip(context,seq->seq1,f_cfra);

			if (child_ibuf) {
				ibuf = child_ibuf;
				if(ibuf && use_preprocess) {
					struct ImBuf * i = IMB_dupImBuf(ibuf);

					IMB_freeImBuf(ibuf);

					ibuf = i;
				}
			}
			break;
		}
		case SEQ_EFFECT:
		{
			ibuf = seq_render_effect_strip_impl(
				context, seq, seq->start + nr);
			break;
		}
		case SEQ_IMAGE:
		{
			StripElem * s_elem = give_stripelem(seq, cfra);

			if (s_elem) {
				BLI_join_dirfile(name, sizeof(name), seq->strip->dir, s_elem->name);
				BLI_path_abs(name, G.main->name);
			}

			if (s_elem && (ibuf = IMB_loadiffname(name, IB_rect))) {
				/* we don't need both (speed reasons)! */
				if (ibuf->rect_float && ibuf->rect)
					imb_freerectImBuf(ibuf);

				/* all sequencer color is done in SRGB space, linear gives odd crossfades */
				if(ibuf->profile == IB_PROFILE_LINEAR_RGB)
					IMB_convert_profile(ibuf, IB_PROFILE_NONE);

				copy_to_ibuf_still(context, seq, nr, ibuf);

				s_elem->orig_width  = ibuf->x;
				s_elem->orig_height = ibuf->y;
			}
			break;
		}
		case SEQ_MOVIE:
		{
			seq_open_anim_file(seq);

			if(seq->anim) {
				IMB_anim_set_preseek(seq->anim,
						     seq->anim_preseek);

				ibuf = IMB_anim_absolute(
					seq->anim, nr + seq->anim_startofs, 
					seq->strip->proxy ? 
					seq->strip->proxy->tc
					: IMB_TC_RECORD_RUN, 
					seq_rendersize_to_proxysize(
						context.preview_render_size));

				/* we don't need both (speed reasons)! */
				if (ibuf && ibuf->rect_float && ibuf->rect)
					imb_freerectImBuf(ibuf);
				if (ibuf) {
					seq->strip->stripdata->orig_width = ibuf->x;
					seq->strip->stripdata->orig_height = ibuf->y;
				}
			}
			copy_to_ibuf_still(context, seq, nr, ibuf);
			break;
		}
		case SEQ_SCENE:
		{	// scene can be NULL after deletions
			ibuf = seq_render_scene_strip_impl(context, seq, nr);

			/* Scene strips update all animation, so we need to restore original state.*/
			BKE_animsys_evaluate_all_animation(context.bmain, context.scene, cfra);

			copy_to_ibuf_still(context, seq, nr, ibuf);
			break;
		}
	}

	if (ibuf == NULL)
		ibuf = IMB_allocImBuf((short)context.rectx, (short)context.recty, 32, IB_rect);

	if (ibuf->x != context.rectx || ibuf->y != context.recty)
		use_preprocess = TRUE;

	if (use_preprocess)
		ibuf = input_preprocess(context, seq, cfra, ibuf);

	seq_stripelem_cache_put(context, seq, cfra, SEQ_STRIPELEM_IBUF, ibuf);

	return ibuf;
}

/* **********************************************************************
   strip stack rendering functions
   ********************************************************************** */

static int seq_must_swap_input_in_blend_mode(Sequence * seq)
{
	int swap_input = FALSE;

	/* bad hack, to fix crazy input ordering of 
	   those two effects */

	if (ELEM3(seq->blend_mode, SEQ_ALPHAOVER, SEQ_ALPHAUNDER, SEQ_OVERDROP)) {
		swap_input = TRUE;
	}
	
	return swap_input;
}

static int seq_get_early_out_for_blend_mode(Sequence * seq)
{
	struct SeqEffectHandle sh = get_sequence_blend(seq);
	float facf = seq->blend_opacity / 100.0f;
	int early_out = sh.early_out(seq, facf, facf);
	
	if (ELEM(early_out, EARLY_DO_EFFECT, EARLY_NO_INPUT)) {
		return early_out;
	}

	if (seq_must_swap_input_in_blend_mode(seq)) {
		if (early_out == EARLY_USE_INPUT_2) {
			return EARLY_USE_INPUT_1;
		} else if (early_out == EARLY_USE_INPUT_1) {
			return EARLY_USE_INPUT_2;
		}
	}
	return early_out;
}

static ImBuf* seq_render_strip_stack(
	SeqRenderData context, ListBase *seqbasep, float cfra, int chanshown)
{
	Sequence* seq_arr[MAXSEQ+1];
	int count;
	int i;
	ImBuf* out = NULL;

	count = get_shown_sequences(seqbasep, cfra, chanshown, (Sequence **)&seq_arr);

	if (count == 0) {
		return NULL;
	}

#if 0 /* commentind since this breaks keyframing, since it resets the value on draw */
	if(scene->r.cfra != cfra) {
		// XXX for prefetch and overlay offset!..., very bad!!!
		AnimData *adt= BKE_animdata_from_id(&scene->id);
		BKE_animsys_evaluate_animdata(scene, &scene->id, adt, cfra, ADT_RECALC_ANIM);
	}
#endif

	out = seq_stripelem_cache_get(context, seq_arr[count - 1], 
				      cfra, SEQ_STRIPELEM_IBUF_COMP);

	if (out) {
		return out;
	}
	
	if(count == 1) {
		out = seq_render_strip(context, seq_arr[0], cfra);
		seq_stripelem_cache_put(context, seq_arr[0], cfra, 
					SEQ_STRIPELEM_IBUF_COMP, out);

		return out;
	}


	for (i = count - 1; i >= 0; i--) {
		int early_out;
		Sequence *seq = seq_arr[i];

		out = seq_stripelem_cache_get(
			context, seq, cfra, SEQ_STRIPELEM_IBUF_COMP);

		if (out) {
			break;
		}
		if (seq->blend_mode == SEQ_BLEND_REPLACE) {
			out = seq_render_strip(context, seq, cfra);
			break;
		}

		early_out = seq_get_early_out_for_blend_mode(seq);

		switch (early_out) {
		case EARLY_NO_INPUT:
		case EARLY_USE_INPUT_2:
			out = seq_render_strip(context, seq, cfra);
			break;
		case EARLY_USE_INPUT_1:
			if (i == 0) {
				out = IMB_allocImBuf((short)context.rectx, (short)context.recty, 32, IB_rect);
			}
			break;
		case EARLY_DO_EFFECT:
			if (i == 0) {
				out = seq_render_strip(context, seq, cfra);
			}

			break;
		}
		if (out) {
			break;
		}
	}

	seq_stripelem_cache_put(context, seq_arr[i], cfra, 
				SEQ_STRIPELEM_IBUF_COMP, out);


	i++;

	for (; i < count; i++) {
		Sequence * seq = seq_arr[i];

		if (seq_get_early_out_for_blend_mode(seq) == EARLY_DO_EFFECT) {
			struct SeqEffectHandle sh = get_sequence_blend(seq);
			ImBuf * ibuf1 = out;
			ImBuf * ibuf2 = seq_render_strip(context, seq, cfra);

			float facf = seq->blend_opacity / 100.0f;
			int swap_input = seq_must_swap_input_in_blend_mode(seq);

			if (swap_input) {
				out = sh.execute(context, seq, cfra, 
						 facf, facf, 
						 ibuf2, ibuf1, NULL);
			} else {
				out = sh.execute(context, seq, cfra, 
						 facf, facf, 
						 ibuf1, ibuf2, NULL);
			}
		
			IMB_freeImBuf(ibuf1);
			IMB_freeImBuf(ibuf2);
		}

		seq_stripelem_cache_put(context, seq_arr[i], cfra,
					SEQ_STRIPELEM_IBUF_COMP, out);
	}

	return out;
}

/*
 * returned ImBuf is refed!
 * you have to free after usage!
 */

ImBuf *give_ibuf_seq(SeqRenderData context, float cfra, int chanshown)
{
	Editing *ed= seq_give_editing(context.scene, FALSE);
	int count;
	ListBase *seqbasep;
	
	if(ed==NULL) return NULL;

	count = BLI_countlist(&ed->metastack);
	if((chanshown < 0) && (count > 0)) {
		count = MAX2(count + chanshown, 0);
		seqbasep= ((MetaStack*)BLI_findlink(&ed->metastack, count))->oldbasep;
	} else {
		seqbasep= ed->seqbasep;
	}

	return seq_render_strip_stack(context, seqbasep, cfra, chanshown);
}

ImBuf *give_ibuf_seqbase(SeqRenderData context, float cfra, int chanshown, ListBase *seqbasep)
{
	return seq_render_strip_stack(context, seqbasep, cfra, chanshown);
}


ImBuf *give_ibuf_seq_direct(SeqRenderData context, float cfra, Sequence *seq)
{
	return seq_render_strip(context, seq, cfra);
}

#if 0
/* check used when we need to change seq->blend_mode but not to effect or audio strips */
static int seq_can_blend(Sequence *seq)
{
	if (ELEM4(seq->type, SEQ_IMAGE, SEQ_META, SEQ_SCENE, SEQ_MOVIE)) {
		return 1;
	} else {
		return 0;
	}
}
#endif

/* *********************** threading api ******************* */

static ListBase running_threads;
static ListBase prefetch_wait;
static ListBase prefetch_done;

static pthread_mutex_t queue_lock          = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t wakeup_lock         = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  wakeup_cond         = PTHREAD_COND_INITIALIZER;

//static pthread_mutex_t prefetch_ready_lock = PTHREAD_MUTEX_INITIALIZER;
//static pthread_cond_t  prefetch_ready_cond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t frame_done_lock     = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  frame_done_cond     = PTHREAD_COND_INITIALIZER;

static volatile int seq_thread_shutdown = TRUE; 
static volatile int seq_last_given_monoton_cfra = 0;
static int monoton_cfra = 0;

typedef struct PrefetchThread {
	struct PrefetchThread *next, *prev;
	
	Scene *scene;
	struct PrefetchQueueElem *current;
	pthread_t pthread;
	int running;
	
} PrefetchThread;

typedef struct PrefetchQueueElem {
	struct PrefetchQueueElem *next, *prev;
	
	int rectx;
	int recty;
	float cfra;
	int chanshown;
	int preview_render_size;

	int monoton_cfra;

	struct ImBuf * ibuf;
} PrefetchQueueElem;

#if 0
static void *seq_prefetch_thread(void * This_)
{
	PrefetchThread * This = This_;

	while (!seq_thread_shutdown) {
		PrefetchQueueElem *e;
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
			e->ibuf = give_ibuf_seq_impl(This->scene, 
				e->rectx, e->recty, e->cfra, e->chanshown,
				e->preview_render_size);
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

static void seq_start_threads(Scene *scene)
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
		PrefetchThread *t = MEM_callocN(sizeof(PrefetchThread), "prefetch_thread");
		t->scene= scene;
		t->running = TRUE;
		BLI_addtail(&running_threads, t);

		pthread_create(&t->pthread, NULL, seq_prefetch_thread, t);
	}

	/* init malloc mutex */
	BLI_init_threads(0, 0, 0);
}

static void seq_stop_threads()
{
	PrefetchThread *tslot;
	PrefetchQueueElem *e;

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
		BLI_remlink(&prefetch_done, e);
		MEM_freeN(e);
	}

	BLI_freelistN(&running_threads);

	/* deinit malloc mutex */
	BLI_end_threads(0);
}
#endif

void give_ibuf_prefetch_request(SeqRenderData context, float cfra, int chanshown)
{
	PrefetchQueueElem *e;
	if (seq_thread_shutdown) {
		return;
	}

	e = MEM_callocN(sizeof(PrefetchQueueElem), "prefetch_queue_elem");
	e->rectx = context.rectx;
	e->recty = context.recty;
	e->cfra = cfra;
	e->chanshown = chanshown;
	e->preview_render_size = context.preview_render_size;
	e->monoton_cfra = monoton_cfra++;

	pthread_mutex_lock(&queue_lock);
	BLI_addtail(&prefetch_wait, e);
	pthread_mutex_unlock(&queue_lock);
	
	pthread_mutex_lock(&wakeup_lock);
	pthread_cond_signal(&wakeup_cond);
	pthread_mutex_unlock(&wakeup_lock);
}

#if 0
static void seq_wait_for_prefetch_ready()
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
#endif

ImBuf *give_ibuf_seq_threaded(SeqRenderData context, float cfra, int chanshown)
{
	PrefetchQueueElem *e = NULL;
	int found_something = FALSE;

	if (seq_thread_shutdown) {
		return give_ibuf_seq(context, cfra, chanshown);
	}

	while (!e) {
		int success = FALSE;
		pthread_mutex_lock(&queue_lock);

		for (e = prefetch_done.first; e; e = e->next) {
			if (cfra == e->cfra &&
				chanshown == e->chanshown &&
				context.rectx == e->rectx && 
				context.recty == e->recty &&
				context.preview_render_size == e->preview_render_size) {
				success = TRUE;
				found_something = TRUE;
				break;
			}
		}

		if (!e) {
			for (e = prefetch_wait.first; e; e = e->next) {
				if (cfra == e->cfra &&
					chanshown == e->chanshown &&
					context.rectx == e->rectx && 
					context.recty == e->recty &&
					context.preview_render_size == e->preview_render_size) {
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
					context.rectx == tslot->current->rectx && 
					context.recty == tslot->current->recty &&
					context.preview_render_size== tslot->current->preview_render_size){
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
	
	return e ? e->ibuf : NULL;
}

/* Functions to free imbuf and anim data on changes */

static void free_anim_seq(Sequence *seq)
{
	if(seq->anim) {
		IMB_free_anim(seq->anim);
		seq->anim = NULL;
	}
}

void free_imbuf_seq(Scene *scene, ListBase * seqbase, int check_mem_usage,
		    int keep_file_handles)
{
	Sequence *seq;

	if (check_mem_usage) {
		/* Let the cache limitor take care of this (schlaile) */
		/* While render let's keep all memory available for render 
		   (ton)
		   At least if free memory is tight...
		   This can make a big difference in encoding speed
		   (it is around 4 times(!) faster, if we do not waste time
		   on freeing _all_ buffers every time on long timelines...)
		   (schlaile)
		*/
	
		uintptr_t mem_in_use;
		uintptr_t mmap_in_use;
		uintptr_t max;
	
		mem_in_use= MEM_get_memory_in_use();
		mmap_in_use= MEM_get_mapped_memory_in_use();
		max = MEM_CacheLimiter_get_maximum();
	
		if (max == 0 || mem_in_use + mmap_in_use <= max) {
			return;
		}
	}

	seq_stripelem_cache_cleanup();
	
	for(seq= seqbase->first; seq; seq= seq->next) {
		if(seq->strip) {
			if(seq->type==SEQ_MOVIE && !keep_file_handles)
				free_anim_seq(seq);
			if(seq->type==SEQ_SPEED) {
				sequence_effect_speed_rebuild_map(scene, seq, 1);
			}
		}
		if(seq->type==SEQ_META) {
			free_imbuf_seq(scene, &seq->seqbase, FALSE, keep_file_handles);
		}
		if(seq->type==SEQ_SCENE) {
			/* FIXME: recurs downwards, 
			   but do recurs protection somehow! */
		}
	}
	
}

static int update_changed_seq_recurs(Scene *scene, Sequence *seq, Sequence *changed_seq, int len_change, int ibuf_change)
{
	Sequence *subseq;
	int free_imbuf = 0;
	
	/* recurs downwards to see if this seq depends on the changed seq */
	
	if(seq == NULL)
		return 0;
	
	if(seq == changed_seq)
		free_imbuf = 1;
	
	for(subseq=seq->seqbase.first; subseq; subseq=subseq->next)
		if(update_changed_seq_recurs(scene, subseq, changed_seq, len_change, ibuf_change))
			free_imbuf = TRUE;
	
	if(seq->seq1)
		if(update_changed_seq_recurs(scene, seq->seq1, changed_seq, len_change, ibuf_change))
			free_imbuf = TRUE;
	if(seq->seq2 && (seq->seq2 != seq->seq1))
		if(update_changed_seq_recurs(scene, seq->seq2, changed_seq, len_change, ibuf_change))
			free_imbuf = TRUE;
	if(seq->seq3 && (seq->seq3 != seq->seq1) && (seq->seq3 != seq->seq2))
		if(update_changed_seq_recurs(scene, seq->seq3, changed_seq, len_change, ibuf_change))
			free_imbuf = TRUE;
	
	if(free_imbuf) {
		if(ibuf_change) {
			if(seq->type == SEQ_MOVIE)
				free_anim_seq(seq);
			if(seq->type == SEQ_SPEED) {
				sequence_effect_speed_rebuild_map(scene, seq, 1);
			}
		}
		
		if(len_change)
			calc_sequence(scene, seq);
	}
	
	return free_imbuf;
}

void update_changed_seq_and_deps(Scene *scene, Sequence *changed_seq, int len_change, int ibuf_change)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;
	
	if (ed==NULL) return;
	
	for (seq=ed->seqbase.first; seq; seq=seq->next)
		update_changed_seq_recurs(scene, seq, changed_seq, len_change, ibuf_change);
}

/* seq funcs's for transforming internally
 notice the difference between start/end and left/right.

 left and right are the bounds at which the sequence is rendered,
start and end are from the start and fixed length of the sequence.
*/
int seq_tx_get_start(Sequence *seq)
{
	return seq->start;
}
int seq_tx_get_end(Sequence *seq)
{
	return seq->start+seq->len;
}

int seq_tx_get_final_left(Sequence *seq, int metaclip)
{
	if (metaclip && seq->tmp) {
		/* return the range clipped by the parents range */
		return MAX2( seq_tx_get_final_left(seq, 0), seq_tx_get_final_left((Sequence *)seq->tmp, 1) );
	} else {
		return (seq->start - seq->startstill) + seq->startofs;
	}

}
int seq_tx_get_final_right(Sequence *seq, int metaclip)
{
	if (metaclip && seq->tmp) {
		/* return the range clipped by the parents range */
		return MIN2( seq_tx_get_final_right(seq, 0), seq_tx_get_final_right((Sequence *)seq->tmp, 1) );
	} else {
		return ((seq->start+seq->len) + seq->endstill) - seq->endofs;
	}
}

void seq_tx_set_final_left(Sequence *seq, int val)
{
	if (val < (seq)->start) {
		seq->startstill = abs(val - (seq)->start);
		seq->startofs = 0;
	} else {
		seq->startofs = abs(val - (seq)->start);
		seq->startstill = 0;
	}
}

void seq_tx_set_final_right(Sequence *seq, int val)
{
	if (val > (seq)->start + (seq)->len) {
		seq->endstill = abs(val - (seq->start + (seq)->len));
		seq->endofs = 0;
	} else {
		seq->endofs = abs(val - ((seq)->start + (seq)->len));
		seq->endstill = 0;
	}
}

/* used so we can do a quick check for single image seq
   since they work a bit differently to normal image seq's (during transform) */
int seq_single_check(Sequence *seq)
{
	return (seq->len==1 && (
			seq->type == SEQ_IMAGE 
			|| ((seq->type & SEQ_EFFECT) && 
	            get_sequence_effect_num_inputs(seq->type) == 0)));
}

/* check if the selected seq's reference unselected seq's */
int seqbase_isolated_sel_check(ListBase *seqbase)
{
	Sequence *seq;
	/* is there more than 1 select */
	int ok= FALSE;

	for(seq= seqbase->first; seq; seq= seq->next) {
		if(seq->flag & SELECT) {
			ok= TRUE;
			break;
		}
	}

	if(ok == FALSE)
		return FALSE;

	/* test relationships */
	for(seq= seqbase->first; seq; seq= seq->next) {
		if((seq->type & SEQ_EFFECT)==0)
			continue;

		if(seq->flag & SELECT) {
			if( (seq->seq1 && (seq->seq1->flag & SELECT)==0) ||
				(seq->seq2 && (seq->seq2->flag & SELECT)==0) ||
				(seq->seq3 && (seq->seq3->flag & SELECT)==0) )
				return FALSE;
		}
		else {
			if( (seq->seq1 && (seq->seq1->flag & SELECT)) ||
				(seq->seq2 && (seq->seq2->flag & SELECT)) ||
				(seq->seq3 && (seq->seq3->flag & SELECT)) )
				return FALSE;
		}
	}

	return TRUE;
}

/* use to impose limits when dragging/extending - so impossible situations dont happen
 * Cant use the SEQ_LEFTSEL and SEQ_LEFTSEL directly because the strip may be in a metastrip */
void seq_tx_handle_xlimits(Sequence *seq, int leftflag, int rightflag)
{
	if(leftflag) {
		if (seq_tx_get_final_left(seq, 0) >= seq_tx_get_final_right(seq, 0)) {
			seq_tx_set_final_left(seq, seq_tx_get_final_right(seq, 0)-1);
		}

		if (seq_single_check(seq)==0) {
			if (seq_tx_get_final_left(seq, 0) >= seq_tx_get_end(seq)) {
				seq_tx_set_final_left(seq, seq_tx_get_end(seq)-1);
			}

			/* dosnt work now - TODO */
			/*
			if (seq_tx_get_start(seq) >= seq_tx_get_final_right(seq, 0)) {
				int ofs;
				ofs = seq_tx_get_start(seq) - seq_tx_get_final_right(seq, 0);
				seq->start -= ofs;
				seq_tx_set_final_left(seq, seq_tx_get_final_left(seq, 0) + ofs );
			}*/

		}
	}

	if(rightflag) {
		if (seq_tx_get_final_right(seq, 0) <=  seq_tx_get_final_left(seq, 0)) {
			seq_tx_set_final_right(seq, seq_tx_get_final_left(seq, 0)+1);
		}

		if (seq_single_check(seq)==0) {
			if (seq_tx_get_final_right(seq, 0) <= seq_tx_get_start(seq)) {
				seq_tx_set_final_right(seq, seq_tx_get_start(seq)+1);
			}
		}
	}

	/* sounds cannot be extended past their endpoints */
	if (seq->type == SEQ_SOUND) {
		seq->startstill= 0;
		seq->endstill= 0;
	}
}

void seq_single_fix(Sequence *seq)
{
	int left, start, offset;
	if (!seq_single_check(seq))
		return;

	/* make sure the image is always at the start since there is only one,
	   adjusting its start should be ok */
	left = seq_tx_get_final_left(seq, 0);
	start = seq->start;
	if (start != left) {
		offset = left - start;
		seq_tx_set_final_left( seq, seq_tx_get_final_left(seq, 0) - offset );
		seq_tx_set_final_right( seq, seq_tx_get_final_right(seq, 0) - offset );
		seq->start += offset;
	}
}

int seq_tx_test(Sequence * seq)
{
	return (seq->type < SEQ_EFFECT) || (get_sequence_effect_num_inputs(seq->type) == 0);
}

static int seq_overlap(Sequence *seq1, Sequence *seq2)
{
	return (seq1 != seq2 && seq1->machine == seq2->machine &&
			((seq1->enddisp <= seq2->startdisp) || (seq1->startdisp >= seq2->enddisp))==0);
}

int seq_test_overlap(ListBase * seqbasep, Sequence *test)
{
	Sequence *seq;

	seq= seqbasep->first;
	while(seq) {
		if(seq_overlap(test, seq))
			return 1;

		seq= seq->next;
	}
	return 0;
}


void seq_translate(Scene *evil_scene, Sequence *seq, int delta)
{
	seq_offset_animdata(evil_scene, seq, delta);
	seq->start += delta;

	if(seq->type==SEQ_META) {
		Sequence *seq_child;
		for(seq_child= seq->seqbase.first; seq_child; seq_child= seq_child->next) {
			seq_translate(evil_scene, seq_child, delta);
		}
	}

	calc_sequence_disp(evil_scene, seq);
}

void seq_sound_init(Scene *scene, Sequence *seq)
{
	if(seq->type==SEQ_META) {
		Sequence *seq_child;
		for(seq_child= seq->seqbase.first; seq_child; seq_child= seq_child->next) {
			seq_sound_init(scene, seq_child);
		}
	}
	else {
		if(seq->sound) {
			seq->scene_sound = sound_add_scene_sound_defaults(scene, seq);
		}
		if(seq->scene) {
			sound_scene_add_scene_sound_defaults(scene, seq);
		}
	}
}

Sequence *seq_foreground_frame_get(Scene *scene, int frame)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq, *best_seq=NULL;
	int best_machine = -1;
	
	if(!ed) return NULL;
	
	for (seq=ed->seqbasep->first; seq; seq= seq->next) {
		if(seq->flag & SEQ_MUTE || seq->startdisp > frame || seq->enddisp <= frame)
			continue;
		/* only use elements you can see - not */
		if (ELEM5(seq->type, SEQ_IMAGE, SEQ_META, SEQ_SCENE, SEQ_MOVIE, SEQ_COLOR)) {
			if (seq->machine > best_machine) {
				best_seq = seq;
				best_machine = seq->machine;
			}
		}
	}
	return best_seq;
}

/* return 0 if there werent enough space */
int shuffle_seq(ListBase * seqbasep, Sequence *test, Scene *evil_scene)
{
	int orig_machine= test->machine;
	test->machine++;
	calc_sequence(evil_scene, test);
	while( seq_test_overlap(seqbasep, test) ) {
		if(test->machine >= MAXSEQ) {
			break;
		}
		test->machine++;
		calc_sequence(evil_scene, test); // XXX - I dont think this is needed since were only moving vertically, Campbell.
	}

	
	if(test->machine >= MAXSEQ) {
		/* Blender 2.4x would remove the strip.
		 * nicer to move it to the end */

		Sequence *seq;
		int new_frame= test->enddisp;

		for(seq= seqbasep->first; seq; seq= seq->next) {
			if (seq->machine == orig_machine)
				new_frame = MAX2(new_frame, seq->enddisp);
		}

		test->machine= orig_machine;
		new_frame = new_frame + (test->start-test->startdisp); /* adjust by the startdisp */
		seq_translate(evil_scene, test, new_frame - test->start);

		calc_sequence(evil_scene, test);
		return 0;
	} else {
		return 1;
	}
}

static int shuffle_seq_time_offset_test(ListBase * seqbasep, char dir)
{
	int offset= 0;
	Sequence *seq, *seq_other;

	for(seq= seqbasep->first; seq; seq= seq->next) {
		if(seq->tmp) {
			for(seq_other= seqbasep->first; seq_other; seq_other= seq_other->next) {
				if(!seq_other->tmp && seq_overlap(seq, seq_other)) {
					if(dir=='L') {
						offset= MIN2(offset, seq_other->startdisp - seq->enddisp);
					}
					else {
						offset= MAX2(offset, seq_other->enddisp - seq->startdisp);
					}
				}
			}
		}
	}
	return offset;
}

static int shuffle_seq_time_offset(Scene* scene, ListBase * seqbasep, char dir)
{
	int ofs= 0;
	int tot_ofs= 0;
	Sequence *seq;
	while( (ofs= shuffle_seq_time_offset_test(seqbasep, dir)) ) {
		for(seq= seqbasep->first; seq; seq= seq->next) {
			if(seq->tmp) {
				/* seq_test_overlap only tests display values */
				seq->startdisp +=	ofs;
				seq->enddisp +=		ofs;
			}
		}

		tot_ofs+= ofs;
	}

	for(seq= seqbasep->first; seq; seq= seq->next) {
		if(seq->tmp)
			calc_sequence_disp(scene, seq); /* corrects dummy startdisp/enddisp values */
	}

	return tot_ofs;
}

int shuffle_seq_time(ListBase * seqbasep, Scene *evil_scene)
{
	/* note: seq->tmp is used to tag strips to move */

	Sequence *seq;

	int offset_l = shuffle_seq_time_offset(evil_scene, seqbasep, 'L');
	int offset_r = shuffle_seq_time_offset(evil_scene, seqbasep, 'R');
	int offset = (-offset_l < offset_r) ?  offset_l:offset_r;

	if(offset) {
		for(seq= seqbasep->first; seq; seq= seq->next) {
			if(seq->tmp) {
				seq_translate(evil_scene, seq, offset);
				seq->flag &= ~SEQ_OVERLAP;
			}
		}
	}

	return offset? 0:1;
}

void seq_update_sound_bounds_all(Scene *scene)
{
	Editing *ed = scene->ed;

	if(ed) {
		Sequence *seq;

		for(seq = ed->seqbase.first; seq; seq = seq->next) {
			if(seq->type == SEQ_META) {
				seq_update_sound_bounds_recursive(scene, seq);
			}
			else if(ELEM(seq->type, SEQ_SOUND, SEQ_SCENE)) {
				seq_update_sound_bounds(scene, seq);
			}
		}
	}
}

void seq_update_sound_bounds(Scene* scene, Sequence *seq)
{
	sound_move_scene_sound_defaults(scene, seq);
	/* mute is set in seq_update_muting_recursive */
}

static void seq_update_muting_recursive(ListBase *seqbasep, Sequence *metaseq, int mute)
{
	Sequence *seq;
	int seqmute;

	/* for sound we go over full meta tree to update muted state,
	   since sound is played outside of evaluating the imbufs, */
	for(seq=seqbasep->first; seq; seq=seq->next) {
		seqmute= (mute || (seq->flag & SEQ_MUTE));

		if(seq->type == SEQ_META) {
			/* if this is the current meta sequence, unmute because
			   all sequences above this were set to mute */
			if(seq == metaseq)
				seqmute= 0;

			seq_update_muting_recursive(&seq->seqbase, metaseq, seqmute);
		}
		else if(ELEM(seq->type, SEQ_SOUND, SEQ_SCENE)) {
			if(seq->scene_sound) {
				sound_mute_scene_sound(seq->scene_sound, seqmute);
			}
		}
	}
}

void seq_update_muting(Editing *ed)
{
	if(ed) {
		/* mute all sounds up to current metastack list */
		MetaStack *ms= ed->metastack.last;

		if(ms)
			seq_update_muting_recursive(&ed->seqbase, ms->parseq, 1);
		else
			seq_update_muting_recursive(&ed->seqbase, NULL, 0);
	}
}

static void seq_update_sound_recursive(Scene *scene, ListBase *seqbasep, bSound *sound)
{
	Sequence *seq;

	for(seq=seqbasep->first; seq; seq=seq->next) {
		if(seq->type == SEQ_META) {
			seq_update_sound_recursive(scene, &seq->seqbase, sound);
		}
		else if(seq->type == SEQ_SOUND) {
			if(seq->scene_sound && sound == seq->sound) {
				sound_update_scene_sound(seq->scene_sound, sound);
			}
		}
	}
}

void seq_update_sound(struct Scene *scene, struct bSound *sound)
{
	if(scene->ed) {
		seq_update_sound_recursive(scene, &scene->ed->seqbase, sound);
	}
}

/* in cases where we done know the sequence's listbase */
ListBase *seq_seqbase(ListBase *seqbase, Sequence *seq)
{
	Sequence *iseq;
	ListBase *lb= NULL;

	for(iseq= seqbase->first; iseq; iseq= iseq->next) {
		if(seq==iseq) {
			return seqbase;
		}
		else if(iseq->seqbase.first && (lb= seq_seqbase(&iseq->seqbase, seq))) {
			return lb;
		}
	}

	return NULL;
}

Sequence *seq_metastrip(ListBase * seqbase, Sequence * meta, Sequence *seq)
{
	Sequence * iseq;

	for(iseq = seqbase->first; iseq; iseq = iseq->next) {
		Sequence * rval;

		if (seq == iseq) {
			return meta;
		} else if(iseq->seqbase.first && 
			(rval = seq_metastrip(&iseq->seqbase, iseq, seq))) {
			return rval;
		}
	}

	return NULL;
}

int seq_swap(Sequence *seq_a, Sequence *seq_b, const char **error_str)
{
	char name[sizeof(seq_a->name)];

	if(seq_a->len != seq_b->len) {
		*error_str= "Strips must be the same length";
		return 0;
	}

	/* type checking, could be more advanced but disalow sound vs non-sound copy */
	if(seq_a->type != seq_b->type) {
		if(seq_a->type == SEQ_SOUND || seq_b->type == SEQ_SOUND) {
			*error_str= "Strips were not compatible";
			return 0;
		}

		/* disallow effects to swap with non-effects strips */
		if((seq_a->type & SEQ_EFFECT) != (seq_b->type & SEQ_EFFECT)) {
			*error_str= "Strips were not compatible";
			return 0;
		}

		if((seq_a->type & SEQ_EFFECT) && (seq_b->type & SEQ_EFFECT)) {
			if(get_sequence_effect_num_inputs(seq_a->type) != get_sequence_effect_num_inputs(seq_b->type)) {
				*error_str= "Strips must have the same number of inputs";
				return 0;
			}
		}
	}

	SWAP(Sequence, *seq_a, *seq_b);

	/* swap back names so animation fcurves dont get swapped */
	BLI_strncpy(name, seq_a->name+2, sizeof(name));
	BLI_strncpy(seq_a->name+2, seq_b->name+2, sizeof(seq_b->name)-2);
	BLI_strncpy(seq_b->name+2, name, sizeof(seq_b->name)-2);

	/* swap back opacity, and overlay mode */
	SWAP(int, seq_a->blend_mode, seq_b->blend_mode);
	SWAP(float, seq_a->blend_opacity, seq_b->blend_opacity);


	SWAP(void *, seq_a->prev, seq_b->prev);
	SWAP(void *, seq_a->next, seq_b->next);
	SWAP(int, seq_a->start, seq_b->start);
	SWAP(int, seq_a->startofs, seq_b->startofs);
	SWAP(int, seq_a->endofs, seq_b->endofs);
	SWAP(int, seq_a->startstill, seq_b->startstill);
	SWAP(int, seq_a->endstill, seq_b->endstill);
	SWAP(int, seq_a->machine, seq_b->machine);
	SWAP(int, seq_a->startdisp, seq_b->startdisp);
	SWAP(int, seq_a->enddisp, seq_b->enddisp);

	return 1;
}

/* XXX - hackish function needed for transforming strips! TODO - have some better solution */
void seq_offset_animdata(Scene *scene, Sequence *seq, int ofs)
{
	char str[SEQ_NAME_MAXSTR+3];
	FCurve *fcu;

	if(scene->adt==NULL || ofs==0 || scene->adt->action==NULL)
		return;

	BLI_snprintf(str, sizeof(str), "[\"%s\"]", seq->name+2);

	for (fcu= scene->adt->action->curves.first; fcu; fcu= fcu->next) {
		if(strstr(fcu->rna_path, "sequence_editor.sequences_all[") && strstr(fcu->rna_path, str)) {
			unsigned int i;
			for (i = 0; i < fcu->totvert; i++) {
				BezTriple *bezt= &fcu->bezt[i];
				bezt->vec[0][0] += ofs;
				bezt->vec[1][0] += ofs;
				bezt->vec[2][0] += ofs;
			}
		}
	}
}

void seq_dupe_animdata(Scene *scene, const char *name_src, const char *name_dst)
{
	char str_from[SEQ_NAME_MAXSTR+3];
	FCurve *fcu;
	FCurve *fcu_last;
	FCurve *fcu_cpy;
	ListBase lb= {NULL, NULL};

	if(scene->adt==NULL || scene->adt->action==NULL)
		return;

	BLI_snprintf(str_from, sizeof(str_from), "[\"%s\"]", name_src);

	fcu_last= scene->adt->action->curves.last;

	for (fcu= scene->adt->action->curves.first; fcu && fcu->prev != fcu_last; fcu= fcu->next) {
		if(strstr(fcu->rna_path, "sequence_editor.sequences_all[") && strstr(fcu->rna_path, str_from)) {
			fcu_cpy= copy_fcurve(fcu);
			BLI_addtail(&lb, fcu_cpy);
		}
	}

	/* notice validate is 0, keep this because the seq may not be added to the scene yet */
	BKE_animdata_fix_paths_rename(&scene->id, scene->adt, "sequence_editor.sequences_all", name_src, name_dst, 0, 0, 0);

	/* add the original fcurves back */
	BLI_movelisttolist(&scene->adt->action->curves, &lb);
}

/* XXX - hackish function needed to remove all fcurves belonging to a sequencer strip */
static void seq_free_animdata(Scene *scene, Sequence *seq)
{
	char str[SEQ_NAME_MAXSTR+3];
	FCurve *fcu;

	if(scene->adt==NULL || scene->adt->action==NULL)
		return;

	BLI_snprintf(str, sizeof(str), "[\"%s\"]", seq->name+2);

	fcu= scene->adt->action->curves.first; 

	while (fcu) {
		if(strstr(fcu->rna_path, "sequence_editor.sequences_all[") && strstr(fcu->rna_path, str)) {
			FCurve *next_fcu = fcu->next;
			
			BLI_remlink(&scene->adt->action->curves, fcu);
			free_fcurve(fcu);

			fcu = next_fcu;
		} else {
			fcu = fcu->next;
		}
	}
}


Sequence *get_seq_by_name(ListBase *seqbase, const char *name, int recursive)
{
	Sequence *iseq=NULL;
	Sequence *rseq=NULL;

	for (iseq=seqbase->first; iseq; iseq=iseq->next) {
		if (strcmp(name, iseq->name+2) == 0)
			return iseq;
		else if(recursive && (iseq->seqbase.first) && (rseq=get_seq_by_name(&iseq->seqbase, name, 1))) {
			return rseq;
		}
	}

	return NULL;
}


Sequence *seq_active_get(Scene *scene)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	if(ed==NULL) return NULL;
	return ed->act_seq;
}

void seq_active_set(Scene *scene, Sequence *seq)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	if(ed==NULL) return;

	ed->act_seq= seq;
}

int seq_active_pair_get(Scene *scene, Sequence **seq_act, Sequence **seq_other)
{
	Editing *ed= seq_give_editing(scene, FALSE);

	*seq_act= seq_active_get(scene);

	if(*seq_act == NULL) {
		return 0;
	}
	else {
		Sequence *seq;

		*seq_other= NULL;

		for(seq= ed->seqbasep->first; seq; seq= seq->next) {
			if(seq->flag & SELECT && (seq != (*seq_act))) {
				if(*seq_other) {
					return 0;
				}
				else {
					*seq_other= seq;
				}
			}
		}

		return (*seq_other != NULL);
	}
}

/* api like funcs for adding */

void seq_load_apply(Scene *scene, Sequence *seq, SeqLoadInfo *seq_load)
{
	if(seq) {
		BLI_strncpy(seq->name+2, seq_load->name, sizeof(seq->name)-2);
		seqbase_unique_name_recursive(&scene->ed->seqbase, seq);

		if(seq_load->flag & SEQ_LOAD_FRAME_ADVANCE) {
			seq_load->start_frame += (seq->enddisp - seq->startdisp);
		}

		if(seq_load->flag & SEQ_LOAD_REPLACE_SEL) {
			seq_load->flag |= SELECT;
			seq_active_set(scene, seq);
		}

		if(seq_load->flag & SEQ_LOAD_SOUND_CACHE) {
			if(seq->sound)
				sound_cache(seq->sound);
		}

		seq_load->tot_success++;
	}
	else {
		seq_load->tot_error++;
	}
}

Sequence *alloc_sequence(ListBase *lb, int cfra, int machine)
{
	Sequence *seq;

	seq= MEM_callocN( sizeof(Sequence), "addseq");
	BLI_addtail(lb, seq);

	*( (short *)seq->name )= ID_SEQ;
	seq->name[2]= 0;

	seq->flag= SELECT;
	seq->start= cfra;
	seq->machine= machine;
	seq->sat= 1.0;
	seq->mul= 1.0;
	seq->blend_opacity = 100.0;
	seq->volume = 1.0f;
	seq->pitch = 1.0f;
	seq->scene_sound = NULL;

	return seq;
}

/* NOTE: this function doesn't fill in image names */
Sequence *sequencer_add_image_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
	Scene *scene= CTX_data_scene(C); /* only for active seq */
	Sequence *seq;
	Strip *strip;

	seq = alloc_sequence(seqbasep, seq_load->start_frame, seq_load->channel);
	seq->type= SEQ_IMAGE;
	seq->blend_mode= SEQ_CROSS; /* so alpha adjustment fade to the strip below */
	
	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");

	strip->len = seq->len = seq_load->len ? seq_load->len : 1;
	strip->us= 1;
	strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
	BLI_strncpy(strip->dir, seq_load->path, sizeof(strip->dir));

	seq_load_apply(scene, seq, seq_load);

	return seq;
}

#ifdef WITH_AUDASPACE
Sequence *sequencer_add_sound_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C); /* only for sound */
	Editing *ed= seq_give_editing(scene, TRUE);
	bSound *sound;

	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;

	AUD_SoundInfo info;

	sound = sound_new_file(CTX_data_main(C), seq_load->path); /* handles relative paths */

	if (sound==NULL || sound->playback_handle == NULL) {
		//if(op)
		//	BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return NULL;
	}

	info = AUD_getInfo(sound->playback_handle);

	if (info.specs.channels == AUD_CHANNELS_INVALID) {
		sound_delete(bmain, sound);
		//if(op)
		//	BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return NULL;
	}

	seq = alloc_sequence(seqbasep, seq_load->start_frame, seq_load->channel);

	seq->type= SEQ_SOUND;
	seq->sound= sound;
	BLI_strncpy(seq->name+2, "Sound", SEQ_NAME_MAXSTR-2);
	seqbase_unique_name_recursive(&scene->ed->seqbase, seq);

	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len = ceil(info.length * FPS);
	strip->us= 1;

	/* we only need 1 element to store the filename */
	strip->stripdata= se= MEM_callocN(sizeof(StripElem), "stripelem");

	BLI_split_dirfile(seq_load->path, strip->dir, se->name, sizeof(strip->dir), sizeof(se->name));

	seq->scene_sound = sound_add_scene_sound(scene, seq, seq_load->start_frame, seq_load->start_frame + strip->len, 0);

	calc_sequence_disp(scene, seq);

	/* last active name */
	BLI_strncpy(ed->act_sounddir, strip->dir, FILE_MAXDIR);

	seq_load_apply(scene, seq, seq_load);

	return seq;
}
#else // WITH_AUDASPACE
Sequence *sequencer_add_sound_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
	(void)C;
	(void)seqbasep;
	(void)seq_load;
	return NULL;
}
#endif // WITH_AUDASPACE

Sequence *sequencer_add_movie_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
	Scene *scene= CTX_data_scene(C); /* only for sound */
	char path[sizeof(seq_load->path)];

	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;

	struct anim *an;

	BLI_strncpy(path, seq_load->path, sizeof(path));
	BLI_path_abs(path, G.main->name);

	an = openanim(path, IB_rect, 0);

	if(an==NULL)
		return NULL;

	seq = alloc_sequence(seqbasep, seq_load->start_frame, seq_load->channel);
	seq->type= SEQ_MOVIE;
	seq->blend_mode= SEQ_CROSS; /* so alpha adjustment fade to the strip below */

	seq->anim= an;
	seq->anim_preseek = IMB_anim_get_preseek(an);
	BLI_strncpy(seq->name+2, "Movie", SEQ_NAME_MAXSTR-2);
	seqbase_unique_name_recursive(&scene->ed->seqbase, seq);

	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len = IMB_anim_get_duration( an, IMB_TC_RECORD_RUN );
	strip->us= 1;

	/* we only need 1 element for MOVIE strips */
	strip->stripdata= se= MEM_callocN(sizeof(StripElem), "stripelem");

	BLI_split_dirfile(seq_load->path, strip->dir, se->name, sizeof(strip->dir), sizeof(se->name));

	calc_sequence_disp(scene, seq);


	if(seq_load->flag & SEQ_LOAD_MOVIE_SOUND) {
		int start_frame_back= seq_load->start_frame;
		seq_load->channel++;

		sequencer_add_sound_strip(C, seqbasep, seq_load);

		seq_load->start_frame= start_frame_back;
		seq_load->channel--;
	}

	if(seq_load->name[0] == '\0')
		BLI_strncpy(seq_load->name, se->name, sizeof(seq_load->name));

	/* can be NULL */
	seq_load_apply(scene, seq, seq_load);

	return seq;
}


static Sequence *seq_dupli(struct Scene *scene, struct Scene *scene_to, Sequence *seq, int dupe_flag)
{
	Scene *sce_audio= scene_to ? scene_to : scene;
	Sequence *seqn = MEM_dupallocN(seq);

	seq->tmp = seqn;
	seqn->strip= MEM_dupallocN(seq->strip);

	// XXX: add F-Curve duplication stuff?

	if (seq->strip->crop) {
		seqn->strip->crop = MEM_dupallocN(seq->strip->crop);
	}

	if (seq->strip->transform) {
		seqn->strip->transform = MEM_dupallocN(seq->strip->transform);
	}

	if (seq->strip->proxy) {
		seqn->strip->proxy = MEM_dupallocN(seq->strip->proxy);
		seqn->strip->proxy->anim = NULL;
	}

	if (seq->strip->color_balance) {
		seqn->strip->color_balance
			= MEM_dupallocN(seq->strip->color_balance);
	}

	if(seq->type==SEQ_META) {
		seqn->strip->stripdata = NULL;

		seqn->seqbase.first= seqn->seqbase.last= NULL;
		/* WATCH OUT!!! - This metastrip is not recursively duplicated here - do this after!!! */
		/* - seq_dupli_recursive(&seq->seqbase,&seqn->seqbase);*/
	} else if(seq->type == SEQ_SCENE) {
		seqn->strip->stripdata = NULL;
		if(seq->scene_sound)
			seqn->scene_sound = sound_scene_add_scene_sound_defaults(sce_audio, seqn);
	} else if(seq->type == SEQ_MOVIE) {
		seqn->strip->stripdata =
				MEM_dupallocN(seq->strip->stripdata);
		seqn->anim= NULL;
	} else if(seq->type == SEQ_SOUND) {
		seqn->strip->stripdata =
				MEM_dupallocN(seq->strip->stripdata);
		if(seq->scene_sound)
			seqn->scene_sound = sound_add_scene_sound_defaults(sce_audio, seqn);

		seqn->sound->id.us++;
	} else if(seq->type == SEQ_IMAGE) {
		seqn->strip->stripdata =
				MEM_dupallocN(seq->strip->stripdata);
	} else if(seq->type >= SEQ_EFFECT) {
		if(seq->seq1 && seq->seq1->tmp) seqn->seq1= seq->seq1->tmp;
		if(seq->seq2 && seq->seq2->tmp) seqn->seq2= seq->seq2->tmp;
		if(seq->seq3 && seq->seq3->tmp) seqn->seq3= seq->seq3->tmp;

		if (seq->type & SEQ_EFFECT) {
			struct SeqEffectHandle sh;
			sh = get_sequence_effect(seq);
			if(sh.copy)
				sh.copy(seq, seqn);
		}

		seqn->strip->stripdata = NULL;

	} else {
		fprintf(stderr, "Aiiiiekkk! sequence type not "
				"handled in duplicate!\nExpect a crash"
						" now...\n");
	}

	if(dupe_flag & SEQ_DUPE_UNIQUE_NAME)
		seqbase_unique_name_recursive(&scene->ed->seqbase, seqn);

	if(dupe_flag & SEQ_DUPE_ANIM)
		seq_dupe_animdata(scene, seq->name+2, seqn->name+2);

	return seqn;
}

Sequence * seq_dupli_recursive(struct Scene *scene, struct Scene *scene_to, Sequence * seq, int dupe_flag)
{
	Sequence * seqn = seq_dupli(scene, scene_to, seq, dupe_flag);
	if (seq->type == SEQ_META) {
		Sequence *s;
		for(s= seq->seqbase.first; s; s = s->next) {
			Sequence *n = seq_dupli_recursive(scene, scene_to, s, dupe_flag);
			if (n) {
				BLI_addtail(&seqn->seqbase, n);
			}
		}
	}
	return seqn;
}

void seqbase_dupli_recursive(Scene *scene, Scene *scene_to, ListBase *nseqbase, ListBase *seqbase, int dupe_flag)
{
	Sequence *seq;
	Sequence *seqn = NULL;
	Sequence *last_seq = seq_active_get(scene);

	for(seq= seqbase->first; seq; seq= seq->next) {
		seq->tmp= NULL;
		if((seq->flag & SELECT) || (dupe_flag & SEQ_DUPE_ALL)) {
			seqn = seq_dupli(scene, scene_to, seq, dupe_flag);
			if (seqn) { /*should never fail */
				if(dupe_flag & SEQ_DUPE_CONTEXT) {
					seq->flag &= ~SEQ_ALLSEL;
					seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL+SEQ_LOCK);
				}

				BLI_addtail(nseqbase, seqn);
				if(seq->type==SEQ_META)
					seqbase_dupli_recursive(scene, scene_to, &seqn->seqbase, &seq->seqbase, dupe_flag);

				if(dupe_flag & SEQ_DUPE_CONTEXT) {
					if (seq == last_seq) {
						seq_active_set(scene, seqn);
					}
				}
			}
		}
	}
}
