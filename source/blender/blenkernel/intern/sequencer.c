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

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_sequencer.h"
#include "BKE_fcurve.h"
#include "BKE_scene.h"
#include "RNA_access.h"
#include "RE_pipeline.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include <pthread.h>

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"



#include "BKE_context.h"
#include "BKE_sound.h"
#include "AUD_C-API.h"

#ifdef WIN32
#define snprintf _snprintf
#endif

/* **** XXX ******** */
//static void waitcursor(int val) {}
//static int blender_test_break() {return 0;}

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

void seqbase_recursive_apply(ListBase *seqbase, int (*apply_func)(Sequence *seq, void *), void *arg)
{
	Sequence *iseq;
	for(iseq= seqbase->first; iseq; iseq= iseq->next) {
		seq_recursive_apply(iseq, apply_func, arg);
	}
}

void seq_recursive_apply(Sequence *seq, int (*apply_func)(Sequence *, void *), void *arg)
{
	if(apply_func(seq, arg) && seq->seqbase.first)
		seqbase_recursive_apply(&seq->seqbase, apply_func, arg);
}

/* **********************************************************************
   alloc / free functions
   ********************************************************************** */

static void free_tstripdata(int len, TStripElem *se)
{
	TStripElem *seo;
	int a;

	seo= se;
	if (!se)
		return;

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


/* free */

static void free_proxy_seq(Sequence *seq)
{
	if (seq->strip && seq->strip->proxy && seq->strip->proxy->anim) {
		IMB_free_anim(seq->strip->proxy->anim);
		seq->strip->proxy->anim = 0;
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

void seq_free_sequence(Scene *scene, Sequence *seq)
{
	if(seq->strip) seq_free_strip(seq->strip);

	if(seq->anim) IMB_free_anim(seq->anim);

	if (seq->type & SEQ_EFFECT) {
		struct SeqEffectHandle sh = get_sequence_effect(seq);

		sh.free(seq);
	}

	/* clipboard has no scene and will never have a sound handle or be active */
	if(scene) {
		Editing *ed = scene->ed;

		if (ed->act_seq==seq)
			ed->act_seq= NULL;

		if(seq->scene_sound)
			sound_remove_scene_sound(scene, seq->scene_sound);
	}

	MEM_freeN(seq);
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

void seq_free_clipboard(void)
{
	Sequence *seq, *nseq;

	for(seq= seqbase_clipboard.first; seq; seq= nseq) {
		nseq= seq->next;
		seq_free_sequence(NULL, seq);
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

/* ************************* itterator ************************** */
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

	seq_update_sound(scene, seq);
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
		calc_sequence_disp(scene, seq);
	}
}

void reload_sequence_new_file(Scene *scene, Sequence * seq)
{
	char str[FILE_MAXDIR+FILE_MAXFILE];

	if (!(seq->type == SEQ_MOVIE || seq->type == SEQ_IMAGE ||
		  seq->type == SEQ_SOUND ||
		  seq->type == SEQ_SCENE || seq->type == SEQ_META)) {
		return;
	}

	new_tstripdata(seq);

	if (seq->type != SEQ_SCENE && seq->type != SEQ_META &&
		seq->type != SEQ_IMAGE) {
		BLI_join_dirfile(str, seq->strip->dir, seq->strip->stripdata->name);
		BLI_path_abs(str, G.sce);
	}

	if (seq->type == SEQ_IMAGE) {
		/* Hack? */
		int olen = MEM_allocN_len(seq->strip->stripdata)/sizeof(struct StripElem);
		seq->len = olen;
		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
	} else if (seq->type == SEQ_MOVIE) {
		if(seq->anim) IMB_free_anim(seq->anim);
		seq->anim = openanim(str, IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0));

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
	} else if (seq->type == SEQ_SOUND) {
		seq->len = ceil(AUD_getInfo(seq->sound->playback_handle).length * FPS);
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

		BLI_strncpy(seq->name+2, sce->id.name + 2, SEQ_NAME_MAXSTR-2);
		seqbase_unique_name_recursive(&scene->ed->seqbase, seq);
		
		seq->len= seq->scene->r.efra - seq->scene->r.sfra + 1;
		seq->len -= seq->anim_startofs;
		seq->len -= seq->anim_endofs;
		if (seq->len < 0) {
			seq->len = 0;
		}
		seq->strip->len = seq->len;
	}

	free_proxy_seq(seq);

	calc_sequence(scene, seq);
}

void sort_seq(Scene *scene)
{
	/* all strips together per kind, and in order of y location ("machine") */
	ListBase seqbase, effbase;
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq, *seqt;

	
	if(ed==NULL) return;

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


static int clear_scene_in_allseqs_cb(Sequence *seq, void *arg_pt)
{
	if(seq->scene==(Scene *)arg_pt)
		seq->scene= NULL;
	return 1;
}

void clear_scene_in_allseqs(Scene *scene)
{
	Scene *scene_iter;

	/* when a scene is deleted: test all seqs */
	for(scene_iter= G.main->scene.first; scene_iter; scene_iter= scene_iter->id.next) {
		if(scene_iter != scene && scene_iter->ed) {
			seqbase_recursive_apply(&scene_iter->ed->seqbase, clear_scene_in_allseqs_cb, scene);
		}
	}
}

typedef struct SeqUniqueInfo {
	Sequence *seq;
	char name_src[32];
	char name_dest[32];
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
			sprintf(sui->name_dest, "%.18s.%03d",  sui->name_src, sui->count++);
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
	strcpy(sui.name_src, seq->name+2);
	strcpy(sui.name_dest, seq->name+2);

	/* Strip off the suffix */
	if ((dot=strrchr(sui.name_src, '.')))
		*dot= '\0';

	sui.count= 1;
	sui.match= 1; /* assume the worst to start the loop */

	while(sui.match) {
		sui.match= 0;
		seqbase_unique_name(seqbasep, &sui);
		seqbase_recursive_apply(seqbasep, seqbase_unique_name_recursive_cb, &sui);
	}

	strcpy(seq->name+2, sui.name_dest);
}

static char *give_seqname_by_type(int type)
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

static void do_effect(Scene *scene, int cfra, Sequence *seq, TStripElem * se,
		      int render_size)
{
	TStripElem *se1, *se2, *se3;
	float fac, facf;
	int x, y;
	int early_out;
	struct SeqEffectHandle sh = get_sequence_effect(seq);
	FCurve *fcu= NULL;

	if (!sh.execute) { /* effect not supported in this version... */
		make_black_ibuf(se->ibuf);
		return;
	}

	if ((seq->flag & SEQ_USE_EFFECT_DEFAULT_FADE) != 0) {
		sh.get_default_fac(seq, cfra, &fac, &facf);
		if( scene->r.mode & R_FIELDS ); else facf= fac;
	} else {
		fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, 
					  "effect_fader", 0);
		if (fcu) {
			fac = facf = evaluate_fcurve(fcu, cfra);
			if( scene->r.mode & R_FIELDS ) {
				facf = evaluate_fcurve(fcu, cfra + 0.5);
			}
		} else {
			fac = facf = seq->effect_fader;
		}
	}

	early_out = sh.early_out(seq, fac, facf);

	if (early_out == -1) { /* no input needed */
		sh.execute(scene, seq, cfra, fac, facf, 
			   se->ibuf->x, se->ibuf->y, render_size,
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
	if (!se3->ibuf->rect_float && se->ibuf->rect_float) {
		IMB_float_from_rect(se3->ibuf);
	}
	
	if (!se1->ibuf->rect && !se->ibuf->rect_float) {
		IMB_rect_from_float(se1->ibuf);
	}
	if (!se2->ibuf->rect && !se->ibuf->rect_float) {
		IMB_rect_from_float(se2->ibuf);
	}
	if (!se3->ibuf->rect && !se->ibuf->rect_float) {
		IMB_rect_from_float(se3->ibuf);
	}

	sh.execute(scene, seq, cfra, fac, facf, x, y, render_size,
		   se1->ibuf, se2->ibuf, se3->ibuf,
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
		else nr= (seq->start + seq->len - 1) - cfra;
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

static TStripElem *give_tstripelem(Sequence *seq, int cfra)
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
		(/*seq->ipo && seq->ipo->curve.first &&*/ 
		   (!(seq->type & SEQ_EFFECT) || !seq->seq1))) {
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
	StripElem *se= seq->strip->stripdata;

	if(seq->type == SEQ_MOVIE) {
		/* use the first */
	}
	else {
		int nr = give_stripelem_index(seq, cfra);

		if (nr == -1) return 0;
		if (se == 0) return 0;
	
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
	return (seq
		&& !(seq->flag & SEQ_MUTE)
		&& seq->type != SEQ_SOUND);
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

static int seq_proxy_get_fname(Scene *scene, Sequence * seq, int cfra, char * name, int render_size)
{
	int frameno;
	char dir[FILE_MAXDIR];

	if (!seq->strip->proxy) {
		return FALSE;
	}

	if ((seq->flag & SEQ_USE_PROXY_CUSTOM_DIR)
	    || (seq->flag & SEQ_USE_PROXY_CUSTOM_FILE)) {
		strcpy(dir, seq->strip->proxy->dir);
	} else {
		if (seq->type == SEQ_IMAGE || seq->type == SEQ_MOVIE) {
			snprintf(dir, FILE_MAXDIR, "%s/BL_proxy", 
				 seq->strip->dir);
		} else {
			return FALSE;
		}
	}

	if (seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) {
		BLI_join_dirfile(name, dir, seq->strip->proxy->file);
		BLI_path_abs(name, G.sce);

		return TRUE;
	}

	/* generate a separate proxy directory for each preview size */

	if (seq->type == SEQ_IMAGE) {
		StripElem * se = give_stripelem(seq, cfra);
		snprintf(name, PROXY_MAXFILE, "%s/images/%d/%s_proxy",
			 dir, render_size, se->name);
		frameno = 1;
	} else if (seq->type == SEQ_MOVIE) {
		TStripElem * tse = give_tstripelem(seq, cfra);

		frameno = tse->nr + seq->anim_startofs;

		snprintf(name, PROXY_MAXFILE, "%s/%s/%d/####", dir,
			 seq->strip->stripdata->name,
			 render_size);
	} else {
		TStripElem * tse = give_tstripelem(seq, cfra);

		frameno = tse->nr + seq->anim_startofs;

		snprintf(name, PROXY_MAXFILE, "%s/proxy_misc/%d/####", dir,
			 render_size);
	}

	BLI_path_abs(name, G.sce);
	BLI_path_frame(name, frameno, 0);


	strcat(name, ".jpg");

	return TRUE;
}

static struct ImBuf * seq_proxy_fetch(Scene *scene, Sequence * seq, int cfra, int render_size)
{
	char name[PROXY_MAXFILE];

	if (!(seq->flag & SEQ_USE_PROXY)) {
		return 0;
	}

	/* rendering at 100% ? No real sense in proxy-ing, right? */
	if (render_size == 100) {
		return 0;
	}

	if (seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) {
		TStripElem * tse = give_tstripelem(seq, cfra);
		int frameno = tse->nr + seq->anim_startofs;
		if (!seq->strip->proxy->anim) {
			if (!seq_proxy_get_fname(scene, seq, cfra, name, render_size)) {
				return 0;
			}
 
			seq->strip->proxy->anim = openanim(name, IB_rect);
		}
		if (!seq->strip->proxy->anim) {
			return 0;
		}
 
		return IMB_anim_absolute(seq->strip->proxy->anim, frameno);
	}
 
	if (!seq_proxy_get_fname(scene, seq, cfra, name, render_size)) {
		return 0;
	}

	if (BLI_exists(name)) {
		return IMB_loadiffname(name, IB_rect);
	} else {
		return 0;
	}
}

#if 0
static void do_build_seq_ibuf(Scene *scene, Sequence * seq, TStripElem *se, int cfra,
				  int build_proxy_run, int render_size);

static void seq_proxy_build_frame(Scene *scene, Sequence * seq, int cfra, int render_size, int seqrectx, int seqrecty)
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
	if (render_size == 100) {
		return;
	}

	/* that's why it is called custom... */
	if (seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) {
		return;
	}

	if (!seq_proxy_get_fname(scene, seq, cfra, name, render_size)) {
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
	
	do_build_seq_ibuf(scene, seq, se, cfra, TRUE, render_size,
			  seqrectx, seqrecty);

	if (!se->ibuf) {
		return;
	}

	rectx= (render_size*scene->r.xsch)/100;
	recty= (render_size*scene->r.ysch)/100;

	ibuf = se->ibuf;

	if (ibuf->x != rectx || ibuf->y != recty) {
		IMB_scalefastImBuf(ibuf, (short)rectx, (short)recty);
	}

	/* quality is fixed, otherwise one has to generate separate
	   directories for every quality...

	   depth = 32 is intentionally left in, otherwise ALPHA channels
	   won't work... */
	quality = seq->strip->proxy->quality;
	ibuf->ftype= JPG | quality;

	BLI_make_existing_file(name);
	
	ok = IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat);
	if (ok == 0) {
		perror(name);
	}

	IMB_freeImBuf(ibuf);
	se->ibuf = 0;
}

static void seq_proxy_rebuild(Scene *scene, Sequence * seq, int seqrectx,
			      int seqrecty)
{
	int cfra;
	float rsize = seq->strip->proxy->size;

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
//XXX				set_timecursor(cfra);
				seq_proxy_build_frame(scene, seq, cfra, rsize,
						      seqrectx, seqrecty);
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
//XXX				set_timecursor(cfra);
				seq_proxy_build_frame(scene, seq, cfra, rsize,
						      seqrectx, seqrecty);
				tse->flag |= STRIPELEM_PREVIEW_DONE;
			}
			if (blender_test_break()) {
				break;
			}
		}
	}
	waitcursor(0);
}
#endif


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

static void color_balance_byte_byte(Sequence * seq, TStripElem* se, float mul)
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

static void color_balance_byte_float(Sequence * seq, TStripElem* se, float mul)
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

static void color_balance_float_float(Sequence * seq, TStripElem* se, float mul)
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

static int input_have_to_preprocess(Scene *scene, Sequence * seq, TStripElem* se, int cfra, int seqrectx, int seqrecty)
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

	if(seq->blend_mode == SEQ_BLEND_REPLACE &&
	   !(seq->type & SEQ_EFFECT)) {
		mul *= seq->blend_opacity / 100.0;
	}

	if (mul != 1.0) {
		return TRUE;
	}
		
	return FALSE;
}

static void input_preprocess(Scene *scene, Sequence *seq, TStripElem *se, int cfra, int seqrectx, int seqrecty)
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
			dx = scene->r.xsch;
			dy = scene->r.ysch;
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
		mul *= seq->blend_opacity / 100.0;
	}

	if(seq->flag & SEQ_USE_COLOR_BALANCE && seq->strip->color_balance) {
		color_balance(seq, se, mul);
		mul = 1.0;
	}

	if(seq->flag & SEQ_MAKE_FLOAT) {
		if (!se->ibuf->rect_float) {
			int profile = IB_PROFILE_NONE;
			
			/* no color management:
			 * don't disturb the existing profiles */
			SWAP(int, se->ibuf->profile, profile);

			IMB_float_from_rect(se->ibuf);
			
			SWAP(int, se->ibuf->profile, profile);
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
		if(scene->r.mode & R_OSA) {
			IMB_scaleImBuf(se->ibuf, 
					   (short)seqrectx, (short)seqrecty);
		} else {
			IMB_scalefastImBuf(se->ibuf, 
					   (short)seqrectx, (short)seqrecty);
		}
	}
}

/* test if image too small or discarded from cache: reload */

static void test_and_auto_discard_ibuf(TStripElem * se, 
				       int seqrectx, int seqrecty)
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

static void check_limiter_refcount(const char * func, TStripElem *se)
{
	if (se && se->ibuf) {
		int refcount = IMB_cache_limiter_get_refcount(se->ibuf);
		if (refcount != 1) {
			/* can happen on complex pipelines */
			if (refcount > 1 && (G.f & G_DEBUG) == 0) {
				return;
			}
 
			fprintf(stderr, 
				"sequencer: (ibuf) %s: "
				"suspicious memcache "
				"limiter refcount: %d\n", func, refcount);
		}
	}
}
 
static void check_limiter_refcount_comp(const char * func, TStripElem *se)
{
	if (se && se->ibuf_comp) {
		int refcount = IMB_cache_limiter_get_refcount(se->ibuf_comp);
		if (refcount != 1) {
			/* can happen on complex pipelines */
			if (refcount > 1 && (G.f & G_DEBUG) == 0) {
				return;
			}
			fprintf(stderr, 
				"sequencer: (ibuf comp) %s: "
				"suspicious memcache "
				"limiter refcount: %d\n", func, refcount);
		}
	}
}

static TStripElem* do_build_seq_array_recursively(
	Scene *scene,
	ListBase *seqbasep, int cfra, int chanshown, int render_size,
	int seqrectx, int seqrecty);

static void do_build_seq_ibuf(Scene *scene, Sequence * seq, TStripElem *se, int cfra,
				  int build_proxy_run, int render_size, int seqrectx, int seqrecty)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
	int use_limiter = TRUE;

	test_and_auto_discard_ibuf(se, seqrectx, seqrecty);
	test_and_auto_discard_ibuf_stills(seq->strip);

	if(seq->type == SEQ_META) {
		TStripElem * meta_se = 0;
		int use_preprocess = FALSE;
		use_limiter = FALSE;

		if (!build_proxy_run && se->ibuf == 0) {
			se->ibuf = seq_proxy_fetch(scene, seq, cfra, render_size);
			if (se->ibuf) {
				use_limiter = TRUE;
				use_preprocess = TRUE;
			}
		}

		if(!se->ibuf && seq->seqbase.first) {
			meta_se = do_build_seq_array_recursively(scene,
				&seq->seqbase, seq->start + se->nr, 0,
				render_size, seqrectx, seqrecty);

			check_limiter_refcount("do_build_seq_ibuf: for META", meta_se);
		}

		se->ok = STRIPELEM_OK;

		if(!se->ibuf && meta_se) {
			se->ibuf = meta_se->ibuf_comp;
			if(se->ibuf &&
			   (!input_have_to_preprocess(scene, seq, se, cfra,
						      seqrectx, seqrecty) ||
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
				use_preprocess = TRUE;
			}
		} else if (se->ibuf) {
			use_limiter = TRUE;
		}
		if (meta_se) {
			free_metastrip_imbufs(
				&seq->seqbase, seq->start + se->nr, 0);
		}

		if (use_preprocess) {
			input_preprocess(scene, seq, se, cfra, seqrectx,
					 seqrecty);
		}
	} else if(seq->type & SEQ_EFFECT) {
		int use_preprocess = FALSE;
		/* should the effect be recalculated? */
		
		if (!build_proxy_run && se->ibuf == 0) {
			se->ibuf = seq_proxy_fetch(scene, seq, cfra, render_size);
			if (se->ibuf) {
				use_preprocess = TRUE;
			}
		}

		if(se->ibuf == 0) {
			/* if any inputs are rectfloat, output is float too */
			if((se->se1 && se->se1->ibuf && se->se1->ibuf->rect_float) ||
			   (se->se2 && se->se2->ibuf && se->se2->ibuf->rect_float) ||
			   (se->se3 && se->se3->ibuf && se->se3->ibuf->rect_float))
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rectfloat, 0);
			else
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rect, 0);
			
			do_effect(scene, cfra, seq, se, render_size);
			if (input_have_to_preprocess(scene, seq, se, cfra,
						     seqrectx, seqrecty) &&
				!build_proxy_run) {
				if ((se->se1 && (se->ibuf == se->se1->ibuf)) ||
					(se->se2 && (se->ibuf == se->se2->ibuf))) {
					struct ImBuf * i
						= IMB_dupImBuf(se->ibuf);

					IMB_freeImBuf(se->ibuf);

					se->ibuf = i;
				}
				use_preprocess = TRUE;
			}
		}
		if (use_preprocess) {
			input_preprocess(scene, seq, se, cfra, seqrectx,
					 seqrecty);
		}
	} else if(seq->type == SEQ_IMAGE) {
		if(se->ok == STRIPELEM_OK && se->ibuf == 0) {
			StripElem * s_elem = give_stripelem(seq, cfra);
			BLI_join_dirfile(name, seq->strip->dir, s_elem->name);
			BLI_path_abs(name, G.sce);
			if (!build_proxy_run) {
				se->ibuf = seq_proxy_fetch(scene, seq, cfra, render_size);
			}
			copy_from_ibuf_still(seq, se);

			if (!se->ibuf) {
				se->ibuf= IMB_loadiffname(
					name, IB_rect);
				/* we don't need both (speed reasons)! */
				if (se->ibuf &&
					se->ibuf->rect_float && se->ibuf->rect) {
					imb_freerectImBuf(se->ibuf);
				}

				copy_to_ibuf_still(seq, se);
			}
			
			if(se->ibuf == 0) {
				se->ok = STRIPELEM_FAILED;
			} else if (!build_proxy_run) {
				input_preprocess(scene, seq, se, cfra,
						 seqrectx, seqrecty);
			}
		}
	} else if(seq->type == SEQ_MOVIE) {
		if(se->ok == STRIPELEM_OK && se->ibuf==0) {
			if(!build_proxy_run) {
				se->ibuf = seq_proxy_fetch(scene, seq, cfra, render_size);
			}
			copy_from_ibuf_still(seq, se);

			if (se->ibuf == 0) {
				if(seq->anim==0) {
					BLI_join_dirfile(name, seq->strip->dir, seq->strip->stripdata->name);
					BLI_path_abs(name, G.sce);
					
					seq->anim = openanim(
						name, IB_rect | 
						((seq->flag & SEQ_FILTERY) 
						 ? IB_animdeinterlace : 0));
				}
				if(seq->anim) {
					IMB_anim_set_preseek(seq->anim, seq->anim_preseek);
					se->ibuf = IMB_anim_absolute(seq->anim, se->nr + seq->anim_startofs);
					/* we don't need both (speed reasons)! */
					if (se->ibuf 
						&& se->ibuf->rect_float 
						&& se->ibuf->rect) {
						imb_freerectImBuf(se->ibuf);
					}

				}
				copy_to_ibuf_still(seq, se);
			}
			
			if(se->ibuf == 0) {
				se->ok = STRIPELEM_FAILED;
			} else if (!build_proxy_run) {
				input_preprocess(scene, seq, se, cfra,
						 seqrectx, seqrecty);
			}
		}
	} else if(seq->type == SEQ_SCENE) {	// scene can be NULL after deletions
		Scene *sce= seq->scene;// *oldsce= scene;
		int have_seq= FALSE;
		int sce_valid= FALSE;

		if(sce) {
			have_seq= (sce->r.scemode & R_DOSEQ) && sce->ed && sce->ed->seqbase.first;
			sce_valid= (sce->camera || have_seq);
		}

		if (se->ibuf == NULL && sce_valid && !build_proxy_run) {
			se->ibuf = seq_proxy_fetch(scene, seq, cfra, render_size);
			if (se->ibuf) {
				input_preprocess(scene, seq, se, cfra,
						 seqrectx, seqrecty);
			}
		}

		if (se->ibuf == NULL && sce_valid) {
			copy_from_ibuf_still(seq, se);
			if (se->ibuf) {
				input_preprocess(scene, seq, se, cfra,
						 seqrectx, seqrecty);
			}
		}
		
		if (!sce_valid) {
			se->ok = STRIPELEM_FAILED;
		}
		else if (se->ibuf==NULL && sce_valid) {
			int frame= seq->sfra + se->nr + seq->anim_startofs;
			int oldcfra = seq->scene->r.cfra;
			Object *oldcamera= seq->scene->camera;
			ListBase oldmarkers;

			/* Hack! This function can be called from do_render_seq(), in that case
			   the seq->scene can already have a Render initialized with same name,
			   so we have to use a default name. (compositor uses scene name to
			   find render).
			   However, when called from within the UI (image preview in sequencer)
			   we do want to use scene Render, that way the render result is defined
			   for display in render/imagewindow

			   Hmm, don't see, why we can't do that all the time,
			   and since G.rendering is uhm, gone... (Peter)
			*/

			int rendering = 1;
			int doseq;
			int doseq_gl= G.rendering ? (scene->r.seq_flag & R_SEQ_GL_REND) : (scene->r.seq_flag & R_SEQ_GL_PREV);

			/* prevent eternal loop */
			doseq= scene->r.scemode & R_DOSEQ;
			scene->r.scemode &= ~R_DOSEQ;

			seq->scene->r.cfra= frame;
			if(seq->scene_camera)	seq->scene->camera= seq->scene_camera;
			else					scene_camera_switch_update(seq->scene);

#ifdef DURIAN_CAMERA_SWITCH
			/* stooping to new low's in hackyness :( */
			oldmarkers= seq->scene->markers;
			seq->scene->markers.first= seq->scene->markers.last= NULL;
#endif

			if(sequencer_view3d_cb && doseq_gl && (seq->scene == scene || have_seq==0)) {
				/* opengl offscreen render */
				scene_update_for_newframe(seq->scene, seq->scene->lay);
				se->ibuf= sequencer_view3d_cb(seq->scene, seqrectx, seqrecty, scene->r.seq_prev_type);
			}
			else {
				Render *re;
				RenderResult rres;

				if(rendering)
					re= RE_NewRender(" do_build_seq_ibuf");
				else
					re= RE_NewRender(sce->id.name);

				RE_BlenderFrame(re, sce, NULL, sce->lay, frame);

				RE_AcquireResultImage(re, &rres);

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

				RE_ReleaseResultImage(re);

				// BIF_end_render_callbacks();
			}
			
			/* restore */
			scene->r.scemode |= doseq;

			seq->scene->r.cfra = oldcfra;
			seq->scene->camera= oldcamera;

#ifdef DURIAN_CAMERA_SWITCH
			/* stooping to new low's in hackyness :( */
			seq->scene->markers= oldmarkers;
#endif

			copy_to_ibuf_still(seq, se);

			if (!build_proxy_run) {
				if(se->ibuf == NULL) {
					se->ok = STRIPELEM_FAILED;
				} else {
					input_preprocess(scene, seq, se, cfra,
							 seqrectx, seqrecty);
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

static TStripElem* do_build_seq_recursively(Scene *scene, Sequence *seq, int cfra, int render_size, int seqrectx, int seqrecty);

static void do_effect_seq_recursively(Scene *scene, Sequence *seq, TStripElem *se, int cfra, int render_size, int seqrectx, int seqrecty)
{
	float fac, facf;
	struct SeqEffectHandle sh = get_sequence_effect(seq);
	int early_out;
	FCurve *fcu= NULL;

	se->se1 = 0;
	se->se2 = 0;
	se->se3 = 0;

	if ((seq->flag & SEQ_USE_EFFECT_DEFAULT_FADE) != 0) {
		sh.get_default_fac(seq, cfra, &fac, &facf);
		if( scene->r.mode & R_FIELDS ); else facf= fac;
	} else {
		fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, 
					  "effect_fader", 0);
		if (fcu) {
			fac = facf = evaluate_fcurve(fcu, cfra);
			if( scene->r.mode & R_FIELDS ) {
				facf = evaluate_fcurve(fcu, cfra + 0.5);
			}
		} else {
			fac = facf = seq->effect_fader;
		}
	}

	early_out = sh.early_out(seq, fac, facf);
	switch (early_out) {
	case -1:
		/* no input needed */
		break;
	case 0:
		se->se1 = do_build_seq_recursively(scene, seq->seq1, cfra, render_size, seqrectx, seqrecty);
		se->se2 = do_build_seq_recursively(scene, seq->seq2, cfra, render_size, seqrectx, seqrecty);
		if (seq->seq3) {
			se->se3 = do_build_seq_recursively(scene, seq->seq3, cfra, render_size, seqrectx, seqrecty);
		}
		break;
	case 1:
		se->se1 = do_build_seq_recursively(scene, seq->seq1, cfra, render_size, seqrectx, seqrecty);
		break;
	case 2:
		se->se2 = do_build_seq_recursively(scene, seq->seq2, cfra, render_size, seqrectx, seqrecty);
		break;
	}


	do_build_seq_ibuf(scene, seq, se, cfra, FALSE, render_size, seqrectx, seqrecty);

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
	check_limiter_refcount("do_effect_seq_recursively", se);
}

static TStripElem* do_build_seq_recursively_impl(Scene *scene, Sequence * seq, int cfra, int render_size, int seqrectx, int seqrecty)
{
	TStripElem *se;

	se = give_tstripelem(seq, cfra);

	if(se) {
		if (seq->type & SEQ_EFFECT) {
			do_effect_seq_recursively(scene, seq, se, cfra, render_size, seqrectx, seqrecty);
		} else {
			do_build_seq_ibuf(scene, seq, se, cfra, FALSE, render_size, seqrectx, seqrecty);
		}
	}
	return se;
}

/* FIXME:
   
If cfra was float throughout blender (especially in the render
pipeline) one could even _render_ with subframe precision
instead of faking using the blend code below...

*/

static TStripElem* do_handle_speed_effect(Scene *scene, Sequence * seq, int cfra, int render_size, int seqrectx, int seqrecty)
{
	SpeedControlVars * s = (SpeedControlVars *)seq->effectdata;
	int nr = cfra - seq->start;
	float f_cfra;
	int cfra_left;
	int cfra_right;
	TStripElem * se = 0;
	TStripElem * se1 = 0;
	TStripElem * se2 = 0;
	
	sequence_effect_speed_rebuild_map(scene, seq, 0);
	
	f_cfra = seq->start + s->frameMap[nr];
	
	cfra_left = (int) floor(f_cfra);
	cfra_right = (int) ceil(f_cfra);

	se = give_tstripelem(seq, cfra);

	if (!se) {
		return se;
	}

	if (cfra_left == cfra_right || 
		(s->flags & SEQ_SPEED_BLEND) == 0) {
		test_and_auto_discard_ibuf(se, seqrectx, seqrecty);

		if (se->ibuf == NULL) {
			se1 = do_build_seq_recursively(scene, seq->seq1, cfra_left, render_size, seqrectx, seqrecty);

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
			se1 = do_build_seq_recursively(scene, seq->seq1, cfra_left, render_size, seqrectx, seqrecty);
			se2 = do_build_seq_recursively(scene, seq->seq1, cfra_right, render_size, seqrectx, seqrecty);

			if((se1 && se1->ibuf && se1->ibuf->rect_float))
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rectfloat, 0);
			else
				se->ibuf= IMB_allocImBuf((short)seqrectx, (short)seqrecty, 32, IB_rect, 0);
			
			if (!se1 || !se2) {
				make_black_ibuf(se->ibuf);
			} else {
				sh = get_sequence_effect(seq);

				sh.execute(scene, seq, cfra, 
					   f_cfra - (float) cfra_left, 
					   f_cfra - (float) cfra_left, 
					   se->ibuf->x, se->ibuf->y, 
					   render_size,
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

	check_limiter_refcount("do_handle_speed_effect", se);

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

static TStripElem* do_build_seq_recursively(Scene *scene, Sequence * seq, int cfra, int render_size, int seqrectx, int seqrecty)
{
	TStripElem *se;

	/* BAD HACK! Seperate handling for speed effects needed, since
	   a) you can't just fetch a different cfra within an effect strip
	   b) we have to blend two frames, and CFRA is not float...
	*/
	if (seq->type == SEQ_SPEED) {
		se = do_handle_speed_effect(scene, seq, cfra, render_size, seqrectx, seqrecty);
	} else {
		se = do_build_seq_recursively_impl(scene, seq, cfra, render_size, seqrectx, seqrecty);
	}

	check_limiter_refcount("do_build_seq_recursively", se);

	return se;
}

static int seq_must_swap_input_in_blend_mode(Sequence * seq)
{
	int swap_input = FALSE;

	/* bad hack, to fix crazy input ordering of 
	   those two effects */

	if (seq->blend_mode == SEQ_ALPHAOVER ||
		seq->blend_mode == SEQ_ALPHAUNDER ||
		seq->blend_mode == SEQ_OVERDROP) {
		swap_input = TRUE;
	}
	
	return swap_input;
}

static int seq_get_early_out_for_blend_mode(Sequence * seq)
{
	struct SeqEffectHandle sh = get_sequence_blend(seq);
	float facf = seq->blend_opacity / 100.0;
	int early_out = sh.early_out(seq, facf, facf);
	
	if (early_out < 1) {
		return early_out;
	}

	if (seq_must_swap_input_in_blend_mode(seq)) {
		if (early_out == 2) {
			return 1;
		} else if (early_out == 1) {
			return 2;
		}
	}
	return early_out;
}

static TStripElem* do_build_seq_array_recursively(
	Scene *scene, ListBase *seqbasep, int cfra, int chanshown, 
	int render_size, int seqrectx, int seqrecty)
{
	Sequence* seq_arr[MAXSEQ+1];
	int count;
	int i;
	TStripElem* se = 0;

	count = get_shown_sequences(seqbasep, cfra, chanshown, 
					(Sequence **)&seq_arr);

	if (!count) {
		return 0;
	}

	se = give_tstripelem(seq_arr[count - 1], cfra);

	if (!se) {
		return 0;
	}

	test_and_auto_discard_ibuf(se, seqrectx, seqrecty);

	if (se->ibuf_comp != 0) {
		IMB_cache_limiter_insert(se->ibuf_comp);
		IMB_cache_limiter_ref(se->ibuf_comp);
		IMB_cache_limiter_touch(se->ibuf_comp);
		return se;
	}

	
	if(count == 1) {
		se = do_build_seq_recursively(scene, seq_arr[0],
					      cfra, render_size,
					      seqrectx, seqrecty);
		if (se->ibuf) {
			se->ibuf_comp = se->ibuf;
			IMB_refImBuf(se->ibuf_comp);
		}
		return se;
	}


	for (i = count - 1; i >= 0; i--) {
		int early_out;
		Sequence * seq = seq_arr[i];

		se = give_tstripelem(seq, cfra);

		test_and_auto_discard_ibuf(se, seqrectx, seqrecty);

		if (se->ibuf_comp != 0) {
			break;
		}
		if (seq->blend_mode == SEQ_BLEND_REPLACE) {
			do_build_seq_recursively(
				scene, seq, cfra, render_size,
				seqrectx, seqrecty);

			if (se->ibuf) {
				se->ibuf_comp = se->ibuf;
				IMB_refImBuf(se->ibuf);
			} else {
				se->ibuf_comp = IMB_allocImBuf(
					(short)seqrectx, (short)seqrecty, 
					32, IB_rect, 0);
				IMB_cache_limiter_insert(se->ibuf_comp);
				IMB_cache_limiter_ref(se->ibuf_comp);
				IMB_cache_limiter_touch(se->ibuf_comp);
			}
			break;
		}

		early_out = seq_get_early_out_for_blend_mode(seq);

		switch (early_out) {
		case -1:
		case 2:
			do_build_seq_recursively(
				scene, seq, cfra, render_size,
				seqrectx, seqrecty);

			if (se->ibuf) {
				se->ibuf_comp = se->ibuf;
				IMB_refImBuf(se->ibuf_comp);
			} else {
				se->ibuf_comp = IMB_allocImBuf(
					(short)seqrectx, (short)seqrecty, 
					32, IB_rect, 0);
				IMB_cache_limiter_insert(se->ibuf_comp);
				IMB_cache_limiter_ref(se->ibuf_comp);
				IMB_cache_limiter_touch(se->ibuf_comp);
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
			do_build_seq_recursively(
				scene, seq, cfra, render_size,
				seqrectx, seqrecty);

			if (!se->ibuf) {
				se->ibuf = IMB_allocImBuf(
					(short)seqrectx, (short)seqrecty, 
					32, IB_rect, 0);
				IMB_cache_limiter_insert(se->ibuf);
				IMB_cache_limiter_ref(se->ibuf);
				IMB_cache_limiter_touch(se->ibuf);
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

		float facf = seq->blend_opacity / 100.0;
		int swap_input = seq_must_swap_input_in_blend_mode(seq);
		int early_out = seq_get_early_out_for_blend_mode(seq);

		switch (early_out) {
		case 0: {
			int x= se2->ibuf->x;
			int y= se2->ibuf->y;

			if(se1->ibuf_comp == NULL)
				continue;

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
			
			if (swap_input) {
				sh.execute(scene, seq, cfra, 
					   facf, facf, x, y, render_size,
					   se2->ibuf, se1->ibuf_comp, 0,
					   se2->ibuf_comp);
			} else {
				sh.execute(scene, seq, cfra, 
					   facf, facf, x, y, render_size,
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
			se2->ibuf_comp = se1->ibuf_comp;
			if(se2->ibuf_comp)
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

static ImBuf *give_ibuf_seq_impl(Scene *scene, int rectx, int recty, int cfra, int chanshown, int render_size)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	int count;
	ListBase *seqbasep;
	TStripElem *se;

	
	if(ed==NULL) return NULL;

	count = BLI_countlist(&ed->metastack);
	if((chanshown < 0) && (count > 0)) {
		count = MAX2(count + chanshown, 0);
		seqbasep= ((MetaStack*)BLI_findlink(&ed->metastack, count))->oldbasep;
	} else {
		seqbasep= ed->seqbasep;
	}

	se = do_build_seq_array_recursively(scene, seqbasep, cfra, chanshown, render_size, rectx, recty);

	if(!se) { 
		return 0;
	}

	check_limiter_refcount_comp("give_ibuf_seq_impl", se);

	return se->ibuf_comp;
}

ImBuf *give_ibuf_seqbase(struct Scene *scene, int rectx, int recty, int cfra, int chanshown, int render_size, ListBase *seqbasep)
{
	TStripElem *se;

	se = do_build_seq_array_recursively(scene, seqbasep, cfra, chanshown, render_size, rectx, recty);

	if(!se) { 
		return 0;
	}

	check_limiter_refcount_comp("give_ibuf_seqbase", se);

	if (se->ibuf_comp) {
		IMB_cache_limiter_unref(se->ibuf_comp);
	}

	return se->ibuf_comp;
}


ImBuf *give_ibuf_seq_direct(Scene *scene, int rectx, int recty, int cfra, int render_size, Sequence *seq)
{
	TStripElem* se;

	se = do_build_seq_recursively(scene, seq, cfra, render_size, rectx, recty);

	if(!se) { 
		return 0;
	}

	check_limiter_refcount("give_ibuf_seq_direct", se);

	if (se->ibuf) {
		IMB_cache_limiter_unref(se->ibuf);
	}

	return se->ibuf;
}

ImBuf *give_ibuf_seq(Scene *scene, int rectx, int recty, int cfra, int chanshown, int render_size)
{
	ImBuf* i = give_ibuf_seq_impl(scene, rectx, recty, cfra, chanshown, render_size);

	if (i) {
		IMB_cache_limiter_unref(i);
	}
	return i;
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

static volatile int seq_thread_shutdown = FALSE;
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
	int cfra;
	int chanshown;
	int render_size;

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
				e->render_size);
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
#endif

void give_ibuf_prefetch_request(int rectx, int recty, int cfra, int chanshown,
				int render_size)
{
	PrefetchQueueElem *e;
	if (seq_thread_shutdown) {
		return;
	}

	e = MEM_callocN(sizeof(PrefetchQueueElem), "prefetch_queue_elem");
	e->rectx = rectx;
	e->recty = recty;
	e->cfra = cfra;
	e->chanshown = chanshown;
	e->render_size = render_size;
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

ImBuf *give_ibuf_seq_threaded(Scene *scene, int rectx, int recty, int cfra, int chanshown, int render_size)
{
	PrefetchQueueElem *e = NULL;
	int found_something = FALSE;

	if (seq_thread_shutdown) {
		return give_ibuf_seq(scene, rectx, recty, cfra, chanshown, render_size);
	}

	while (!e) {
		int success = FALSE;
		pthread_mutex_lock(&queue_lock);

		for (e = prefetch_done.first; e; e = e->next) {
			if (cfra == e->cfra &&
				chanshown == e->chanshown &&
				rectx == e->rectx && 
				recty == e->recty &&
				render_size == e->render_size) {
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
					recty == e->recty &&
					render_size == e->render_size) {
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
					recty == tslot->current->recty &&
					render_size== tslot->current->render_size){
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

#if 0
static void free_imbuf_seq_except(Scene *scene, int cfra)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;
	TStripElem *se;
	int a;

	if(ed==NULL) return;

	SEQ_BEGIN(ed, seq) {
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
			free_proxy_seq(seq);
		}
	}
	SEQ_END
}
#endif

void free_imbuf_seq(Scene *scene, ListBase * seqbase, int check_mem_usage)
{
	Sequence *seq;
	TStripElem *se;
	int a;

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

	
	for(seq= seqbase->first; seq; seq= seq->next) {
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
				sequence_effect_speed_rebuild_map(scene, seq, 1);
			}
		}
		if(seq->type==SEQ_META) {
			free_imbuf_seq(scene, &seq->seqbase, FALSE);
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
	int a, free_imbuf = 0;
	TStripElem *se;
	
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
			se= seq->strip->tstripdata;
			if (se) {
				for(a=0; a<seq->len; a++, se++)
					free_imbuf_strip_elem(se);
			}
			
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

#if 0 // XXX from 2.4x, needs updating
void free_imbuf_seq()
{
	Scene * sce = G.main->scene.first;
	while(sce) {
		free_imbuf_seq_editing(sce->ed);
		sce= sce->id.next;
	}
}
#endif 

#if 0 // XXX old animation system
static void free_imbuf_seq_with_ipo(Scene *scene, struct Ipo *ipo)
{
	/* force update of all sequences with this ipo, on ipo changes */
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	if(ed==NULL) return;

	SEQ_BEGIN(ed, seq) {
		if(seq->ipo == ipo) {
			update_changed_seq_and_deps(scene, seq, 0, 1);
			if(seq->type == SEQ_SPEED) {
				sequence_effect_speed_rebuild_map(seq, 1);
			}
			free_proxy_seq(seq);
		}
	}
	SEQ_END
}
#endif

/* seq funcs's for transforming internally
 notice the difference between start/end and left/right.

 left and right are the bounds at which the sequence is rendered,
start and end are from the start and fixed length of the sequence.
*/
int seq_tx_get_start(Sequence *seq) {
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
	if ( seq->len==1 && (seq->type == SEQ_IMAGE || seq->type == SEQ_COLOR
			     || seq->type == SEQ_MULTICAM))
		return 1;
	else
		return 0;
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
		if(seq->flag & SELECT) {
			if(seq->type & SEQ_EFFECT) {
				if(seq->seq1 && (seq->seq1->flag & SELECT)==0) return FALSE;
				if(seq->seq2 && (seq->seq2->flag & SELECT)==0) return FALSE;
				if(seq->seq3 && (seq->seq3->flag & SELECT)==0) return FALSE;
			}
		}
		else if(seq->type & SEQ_EFFECT) {
			if(seq->seq1 && (seq->seq1->flag & SELECT)) return FALSE;
			if(seq->seq2 && (seq->seq2->flag & SELECT)) return FALSE;
			if(seq->seq3 && (seq->seq3->flag & SELECT)) return FALSE;
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
	if(seq1 != seq2)
		if(seq1->machine==seq2->machine)
			if(((seq1->enddisp <= seq2->startdisp) || (seq1->startdisp >= seq2->enddisp))==0)
				return 1;

	return 0;
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


static void seq_translate(Scene *evil_scene, Sequence *seq, int delta)
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

void seq_update_sound(Scene* scene, Sequence *seq)
{
	if(seq->scene_sound)
	{
		sound_move_scene_sound(scene, seq->scene_sound, seq->startdisp, seq->enddisp, seq->startofs + seq->anim_startofs);
		/* mute is set in seq_update_muting_recursive */
	}
}

static void seq_update_muting_recursive(Scene *scene, ListBase *seqbasep, Sequence *metaseq, int mute)
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

			seq_update_muting_recursive(scene, &seq->seqbase, metaseq, seqmute);
		}
		else if((seq->type == SEQ_SOUND) || (seq->type == SEQ_SCENE)) {
			if(seq->scene_sound) {
				sound_mute_scene_sound(scene, seq->scene_sound, seqmute);
			}
		}
	}
}

void seq_update_muting(Scene *scene, Editing *ed)
{
	if(ed) {
		/* mute all sounds up to current metastack list */
		MetaStack *ms= ed->metastack.last;

		if(ms)
			seq_update_muting_recursive(scene, &ed->seqbase, ms->parseq, 1);
		else
			seq_update_muting_recursive(scene, &ed->seqbase, NULL, 0);
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

/* XXX - hackish function needed for transforming strips! TODO - have some better solution */
void seq_offset_animdata(Scene *scene, Sequence *seq, int ofs)
{
	char str[32];
	FCurve *fcu;

	if(scene->adt==NULL || ofs==0 || scene->adt->action==NULL)
		return;

	sprintf(str, "[\"%s\"]", seq->name+2);

	for (fcu= scene->adt->action->curves.first; fcu; fcu= fcu->next) {
		if(strstr(fcu->rna_path, "sequence_editor.sequences_all[") && strstr(fcu->rna_path, str)) {
			int i;
			for (i = 0; i < fcu->totvert; i++) {
				BezTriple *bezt= &fcu->bezt[i];
				bezt->vec[0][0] += ofs;
				bezt->vec[1][0] += ofs;
				bezt->vec[2][0] += ofs;
			}
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


Sequence *active_seq_get(Scene *scene)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	if(ed==NULL) return NULL;
	return ed->act_seq;
}

void active_seq_set(Scene *scene, Sequence *seq)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	if(ed==NULL) return;

	ed->act_seq= seq;
}

/* api like funcs for adding */

void seq_load_apply(Scene *scene, Sequence *seq, SeqLoadInfo *seq_load)
{
	if(seq) {
		strcpy(seq->name, seq_load->name);
		seqbase_unique_name_recursive(&scene->ed->seqbase, seq);

		if(seq_load->flag & SEQ_LOAD_FRAME_ADVANCE) {
			seq_load->start_frame += (seq->enddisp - seq->startdisp);
		}

		if(seq_load->flag & SEQ_LOAD_REPLACE_SEL) {
			seq_load->flag |= SELECT;
			active_seq_set(scene, seq);
		}

		if(seq_load->flag & SEQ_LOAD_SOUND_CACHE) {
			if(seq->sound)
				sound_cache(seq->sound, 0);
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
	seq->mul= 1.0;
	seq->blend_opacity = 100.0;
	seq->volume = 1.0f;

	return seq;
}

/* NOTE: this function doesn't fill in image names */
Sequence *sequencer_add_image_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
	Scene *scene= CTX_data_scene(C); /* only for active seq */
	Sequence *seq;
	Strip *strip;
	StripElem *se;

	seq = alloc_sequence(seqbasep, seq_load->start_frame, seq_load->channel);
	seq->type= SEQ_IMAGE;
	BLI_strncpy(seq->name+2, "Image", SEQ_NAME_MAXSTR-2);
	seqbase_unique_name_recursive(&scene->ed->seqbase, seq);
	
	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");

	strip->len = seq->len = seq_load->len ? seq_load->len : 1;
	strip->us= 1;
	strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
	BLI_split_dirfile(seq_load->path, strip->dir, se->name);
	
	seq_load_apply(scene, seq, seq_load);

	return seq;
}

Sequence *sequencer_add_sound_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
	Scene *scene= CTX_data_scene(C); /* only for sound */
	Editing *ed= seq_give_editing(scene, TRUE);
	bSound *sound;

	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;

	AUD_SoundInfo info;

	sound = sound_new_file(CTX_data_main(C), seq_load->path);

	if (sound==NULL || sound->playback_handle == NULL) {
		//if(op)
		//	BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return NULL;
	}

	info = AUD_getInfo(sound->playback_handle);

	if (info.specs.channels == AUD_CHANNELS_INVALID) {
		sound_delete(C, sound);
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

	strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");

	BLI_split_dirfile(seq_load->path, strip->dir, se->name);

	seq->scene_sound = sound_add_scene_sound(scene, seq, seq_load->start_frame, seq_load->start_frame + strip->len, 0);

	calc_sequence_disp(scene, seq);

	/* last active name */
	strncpy(ed->act_sounddir, strip->dir, FILE_MAXDIR-1);

	seq_load_apply(scene, seq, seq_load);

	return seq;
}

Sequence *sequencer_add_movie_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
	Scene *scene= CTX_data_scene(C); /* only for sound */

	Sequence *seq, *soundseq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;

	struct anim *an;

	an = openanim(seq_load->path, IB_rect);

	if(an==NULL)
		return NULL;

	seq = alloc_sequence(seqbasep, seq_load->start_frame, seq_load->channel);

	seq->type= SEQ_MOVIE;
	seq->anim= an;
	seq->anim_preseek = IMB_anim_get_preseek(an);
	BLI_strncpy(seq->name+2, "Movie", SEQ_NAME_MAXSTR-2);
	seqbase_unique_name_recursive(&scene->ed->seqbase, seq);

	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len = IMB_anim_get_duration( an );
	strip->us= 1;

	strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");

	BLI_split_dirfile(seq_load->path, strip->dir, se->name);

	calc_sequence_disp(scene, seq);


	if(seq_load->flag & SEQ_LOAD_MOVIE_SOUND) {
		int start_frame_back= seq_load->start_frame;
		seq_load->channel++;

		soundseq = sequencer_add_sound_strip(C, seqbasep, seq_load);

		seq_load->start_frame= start_frame_back;
		seq_load->channel--;
	}

	/* can be NULL */
	seq_load_apply(scene, seq, seq_load);

	return seq;
}
