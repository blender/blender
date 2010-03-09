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
 * Contributor(s): Blender Foundation, 2003-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <sys/types.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_storage_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_ipo_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_sound_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_plugin_types.h"
#include "BKE_sequencer.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"
#include "BKE_sound.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

/* for menu/popup icons etc etc*/
#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_anim_api.h"
#include "ED_space_api.h"
#include "ED_types.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_util.h"
#include "ED_sequencer.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

/* own include */
#include "sequencer_intern.h"

/* XXX */
//static Sequence *_last_seq=0;
//static int _last_seq_init=0;
/* XXX */
static void error() {}
static void waitcursor(int val) {}
static void activate_fileselect() {}
static int pupmenu() {return 0;}
static int okee() {return 0;}


/* XXX */
/* RNA Enums, used in multiple files */
EnumPropertyItem sequencer_prop_effect_types[] = {
	{SEQ_CROSS, "CROSS", 0, "Crossfade", "Crossfade effect strip type"},
	{SEQ_ADD, "ADD", 0, "Add", "Add effect strip type"},
	{SEQ_SUB, "SUBTRACT", 0, "Subtract", "Subtract effect strip type"},
	{SEQ_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", "Alpha Over effect strip type"},
	{SEQ_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", "Alpha Under effect strip type"},
	{SEQ_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Cross", "Gamma Cross effect strip type"},
	{SEQ_MUL, "MULTIPLY", 0, "Multiply", "Multiply effect strip type"},
	{SEQ_OVERDROP, "OVER_DROP", 0, "Alpha Over Drop", "Alpha Over Drop effect strip type"},
	{SEQ_PLUGIN, "PLUGIN", 0, "Plugin", "Plugin effect strip type"},
	{SEQ_WIPE, "WIPE", 0, "Wipe", "Wipe effect strip type"},
	{SEQ_GLOW, "GLOW", 0, "Glow", "Glow effect strip type"},
	{SEQ_TRANSFORM, "TRANSFORM", 0, "Transform", "Transform effect strip type"},
	{SEQ_COLOR, "COLOR", 0, "Color", "Color effect strip type"},
	{SEQ_SPEED, "SPEED", 0, "Speed", "Color effect strip type"},
	{0, NULL, 0, NULL, NULL}
};

/* mute operator */

EnumPropertyItem prop_side_types[] = {
	{SEQ_SIDE_LEFT, "LEFT", 0, "Left", ""},
	{SEQ_SIDE_RIGHT, "RIGHT", 0, "Right", ""},
	{SEQ_SIDE_BOTH, "BOTH", 0, "Both", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem prop_side_lr_types[] = {
	{SEQ_SIDE_LEFT, "LEFT", 0, "Left", ""},
	{SEQ_SIDE_RIGHT, "RIGHT", 0, "Right", ""},
	{0, NULL, 0, NULL, NULL}
};

typedef struct TransSeq {
	int start, machine;
	int startstill, endstill;
	int startdisp, enddisp;
	int startofs, endofs;
	int final_left, final_right;
	int len;
} TransSeq;

Sequence *get_foreground_frame_seq(Scene *scene, int frame)
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

void seq_rectf(Sequence *seq, rctf *rectf)
{
	if(seq->startstill) rectf->xmin= seq->start;
	else rectf->xmin= seq->startdisp;
	rectf->ymin= seq->machine+SEQ_STRIP_OFSBOTTOM;
	if(seq->endstill) rectf->xmax= seq->start+seq->len;
	else rectf->xmax= seq->enddisp;
	rectf->ymax= seq->machine+SEQ_STRIP_OFSTOP;
}

static void change_plugin_seq(Scene *scene, char *str)	/* called from fileselect */
{
	Editing *ed= seq_give_editing(scene, FALSE);
	struct SeqEffectHandle sh;
	Sequence *last_seq= active_seq_get(scene);

	if(last_seq && last_seq->type != SEQ_PLUGIN) return;

	sh = get_sequence_effect(last_seq);
	sh.free(last_seq);
	sh.init_plugin(last_seq, str);

	last_seq->machine = MAX3(last_seq->seq1->machine, 
				 last_seq->seq2->machine, 
				 last_seq->seq3->machine);

	if( seq_test_overlap(ed->seqbasep, last_seq) ) shuffle_seq(ed->seqbasep, last_seq, scene);
	
}


void boundbox_seq(Scene *scene, rctf *rect)
{
	Sequence *seq;
	Editing *ed= seq_give_editing(scene, FALSE);
	float min[2], max[2];

	
	if(ed==NULL) return;

	min[0]= 0.0;
	max[0]= EFRA+1;
	min[1]= 0.0;
	max[1]= 8.0;

	seq= ed->seqbasep->first;
	while(seq) {

		if( min[0] > seq->startdisp-1) min[0]= seq->startdisp-1;
		if( max[0] < seq->enddisp+1) max[0]= seq->enddisp+1;
		if( max[1] < seq->machine+2.0) max[1]= seq->machine+2.0;

		seq= seq->next;
	}

	rect->xmin= min[0];
	rect->xmax= max[0];
	rect->ymin= min[1];
	rect->ymax= max[1];

}

int mouse_frame_side(View2D *v2d, short mouse_x, int frame ) 
{
	short mval[2];
	float mouseloc[2];
	
	mval[0]= mouse_x;
	mval[1]= 0;
	
	/* choose the side based on which side of the playhead the mouse is on */
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &mouseloc[0], &mouseloc[1]);
	
	return mouseloc[0] > frame ? SEQ_SIDE_RIGHT : SEQ_SIDE_LEFT;
}


Sequence *find_neighboring_sequence(Scene *scene, Sequence *test, int lr, int sel) 
{
	/* sel - 0==unselected, 1==selected, -1==done care*/
	Sequence *seq;
	Editing *ed= seq_give_editing(scene, FALSE);

	if(ed==NULL) return NULL;

	if (sel>0) sel = SELECT;
	
	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if(	(seq!=test) &&
			(test->machine==seq->machine) &&
			((sel == -1) || (sel && (seq->flag & SELECT)) || (sel==0 && (seq->flag & SELECT)==0)  ))
		{
			switch (lr) {
			case SEQ_SIDE_LEFT:
				if (test->startdisp == (seq->enddisp)) {
					return seq;
				}
				break;
			case SEQ_SIDE_RIGHT:
				if (test->enddisp == (seq->startdisp)) {
					return seq;
				}
				break;
			}
		}
	}
	return NULL;
}

Sequence *find_next_prev_sequence(Scene *scene, Sequence *test, int lr, int sel) 
{
	/* sel - 0==unselected, 1==selected, -1==done care*/
	Sequence *seq,*best_seq = NULL;
	Editing *ed= seq_give_editing(scene, FALSE);
	
	int dist, best_dist;
	best_dist = MAXFRAME*2;

	
	if(ed==NULL) return NULL;

	seq= ed->seqbasep->first;
	while(seq) {
		if(		(seq!=test) &&
				(test->machine==seq->machine) &&
				(test->depth==seq->depth) &&
				((sel == -1) || (sel==(seq->flag & SELECT))))
		{
			dist = MAXFRAME*2;
			
			switch (lr) {
			case SEQ_SIDE_LEFT:
				if (seq->enddisp <= test->startdisp) {
					dist = test->enddisp - seq->startdisp;
				}
				break;
			case SEQ_SIDE_RIGHT:
				if (seq->startdisp >= test->enddisp) {
					dist = seq->startdisp - test->enddisp;
				}
				break;
			}
			
			if (dist==0) {
				best_seq = seq;
				break;
			} else if (dist < best_dist) {
				best_dist = dist;
				best_seq = seq;
			}
		}
		seq= seq->next;
	}
	return best_seq; /* can be null */
}


Sequence *find_nearest_seq(Scene *scene, View2D *v2d, int *hand, short mval[2])
{
	Sequence *seq;
	Editing *ed= seq_give_editing(scene, FALSE);
	float x, y;
	float pixelx;
	float handsize;
	float displen;
	*hand= SEQ_SIDE_NONE;

	
	if(ed==NULL) return NULL;
	
	pixelx = (v2d->cur.xmax - v2d->cur.xmin)/(v2d->mask.xmax - v2d->mask.xmin);

	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	
	seq= ed->seqbasep->first;
	
	while(seq) {
		if(seq->machine == (int)y) {
			/* check for both normal strips, and strips that have been flipped horizontally */
			if( ((seq->startdisp < seq->enddisp) && (seq->startdisp<=x && seq->enddisp>=x)) ||
				((seq->startdisp > seq->enddisp) && (seq->startdisp>=x && seq->enddisp<=x)) )
			{
				if(seq_tx_test(seq)) {
					
					/* clamp handles to defined size in pixel space */
					
					handsize = seq->handsize;
					displen = (float)abs(seq->startdisp - seq->enddisp);
					
					if (displen / pixelx > 16) { /* dont even try to grab the handles of small strips */
						/* Set the max value to handle to 1/3 of the total len when its less then 28.
						* This is important because otherwise selecting handles happens even when you click in the middle */
						
						if ((displen/3) < 30*pixelx) {
							handsize = displen/3;
						} else {
							CLAMP(handsize, 7*pixelx, 30*pixelx);
						}
						
						if( handsize+seq->startdisp >=x )
							*hand= SEQ_SIDE_LEFT;
						else if( -handsize+seq->enddisp <=x )
							*hand= SEQ_SIDE_RIGHT;
					}
				}
				return seq;
			}
		}
		seq= seq->next;
	}
	return 0;
}


static int seq_is_parent(Sequence *par, Sequence *seq)
{
	return ((par->seq1 == seq) || (par->seq2 == seq) || (par->seq3 == seq));
}

static int seq_is_predecessor(Sequence *pred, Sequence *seq)
{
	if (!pred) return 0;
	if(pred == seq) return 0;
	else if(seq_is_parent(pred, seq)) return 1;
	else if(pred->seq1 && seq_is_predecessor(pred->seq1, seq)) return 1;
	else if(pred->seq2 && seq_is_predecessor(pred->seq2, seq)) return 1;
	else if(pred->seq3 && seq_is_predecessor(pred->seq3, seq)) return 1;

	return 0;
}

void deselect_all_seq(Scene *scene)
{
	Sequence *seq;
	Editing *ed= seq_give_editing(scene, FALSE);

	
	if(ed==NULL) return;

	SEQP_BEGIN(ed, seq) {
		seq->flag &= SEQ_DESEL;
	}
	SEQ_END
		
}

void recurs_sel_seq(Sequence *seqm)
{
	Sequence *seq;

	seq= seqm->seqbase.first;
	while(seq) {

		if(seqm->flag & (SEQ_LEFTSEL+SEQ_RIGHTSEL)) seq->flag &= SEQ_DESEL;
		else if(seqm->flag & SELECT) seq->flag |= SELECT;
		else seq->flag &= SEQ_DESEL;

		if(seq->seqbase.first) recurs_sel_seq(seq);

		seq= seq->next;
	}
}

int event_to_efftype(int event)
{
	if(event==2) return SEQ_CROSS;
	if(event==3) return SEQ_GAMCROSS;
	if(event==4) return SEQ_ADD;
	if(event==5) return SEQ_SUB;
	if(event==6) return SEQ_MUL;
	if(event==7) return SEQ_ALPHAOVER;
	if(event==8) return SEQ_ALPHAUNDER;
	if(event==9) return SEQ_OVERDROP;
	if(event==10) return SEQ_PLUGIN;
	if(event==13) return SEQ_WIPE;
	if(event==14) return SEQ_GLOW;
	if(event==15) return SEQ_TRANSFORM;
	if(event==16) return SEQ_COLOR;
	if(event==17) return SEQ_SPEED;
	return 0;
}

#if 0
static void reload_sound_strip(Scene *scene, char *name)
{
	Editing *ed;
	Sequence *seq, *seqact;
	SpaceFile *sfile;
	Sequence *last_seq= active_seq_get(scene);

	ed= scene->ed;

	if(last_seq==0 || last_seq->type!=SEQ_SOUND) return;
	seqact= last_seq;	/* last_seq changes in alloc_sequence */

	/* search sfile */
//	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	waitcursor(1);

	seq = sfile_to_snd_sequence(sfile, seqact->start, seqact->machine);
	printf("seq->type: %i\n", seq->type);
	if(seq && seq!=seqact) {
		/* i'm not sure about this one, seems to work without it -- sgefant */
		seq_free_strip(seqact->strip);

		seqact->strip= seq->strip;

		seqact->len= seq->len;
		calc_sequence(scene, seqact);

		seq->strip= 0;
		seq_free_sequence(scene, seq);
		BLI_remlink(ed->seqbasep, seq);

		seq= ed->seqbasep->first;

	}

	waitcursor(0);

}
#endif

static void reload_image_strip(Scene *scene, char *name)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq=NULL, *seqact;
	SpaceFile *sfile=NULL;
	Sequence *last_seq= active_seq_get(scene);



	if(last_seq==0 || last_seq->type!=SEQ_IMAGE) return;
	seqact= last_seq;	/* last_seq changes in alloc_sequence */

	/* search sfile */
//	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	waitcursor(1);

//	seq= sfile_to_sequence(scene, sfile, seqact->start, seqact->machine, 1); // XXX ADD BACK
	if(seq && seq!=seqact) {
		seq_free_strip(seqact->strip);

		seqact->strip= seq->strip;

		seqact->len= seq->len;
		calc_sequence(scene, seqact);

		seq->strip= 0;
		seq_free_sequence(scene, seq);
		BLI_remlink(ed->seqbasep, seq);

		update_changed_seq_and_deps(scene, seqact, 1, 1);
	}
	waitcursor(0);

}


void change_sequence(Scene *scene)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *last_seq= active_seq_get(scene);
	Scene *sce;
	short event;

	if(last_seq==0) return;

	if(last_seq->type & SEQ_EFFECT) {
		event = pupmenu("Change Effect%t"
				"|Switch A <-> B %x1"
				"|Switch B <-> C %x10"
				"|Plugin%x11"
				"|Recalculate%x12"
				"|Cross%x2"
				"|Gamma Cross%x3"
				"|Add%x4"
				"|Sub%x5"
				"|Mul%x6"
				"|Alpha Over%x7"
				"|Alpha Under%x8"
				"|Alpha Over Drop%x9"
				"|Wipe%x13"
				"|Glow%x14"
				"|Transform%x15"
				"|Color Generator%x16"
				"|Speed Control%x17");
		if(event > 0) {
			if(event==1) {
				SWAP(Sequence *,last_seq->seq1,last_seq->seq2);
			}
			else if(event==10) {
				SWAP(Sequence *,last_seq->seq2,last_seq->seq3);
			}
			else if(event==11) {
				activate_fileselect(
					FILE_SPECIAL, "Select Plugin", 
					U.plugseqdir, change_plugin_seq);
			}
			else if(event==12);	
                                /* recalculate: only new_stripdata */
			else {
				/* free previous effect and init new effect */
				struct SeqEffectHandle sh;

				if (get_sequence_effect_num_inputs(
					    last_seq->type)
				    < get_sequence_effect_num_inputs(
					    event_to_efftype(event))) {
					error("New effect needs more "
					      "input strips!");
				} else {
					sh = get_sequence_effect(last_seq);
					sh.free(last_seq);
					
					last_seq->type 
						= event_to_efftype(event);
					
					sh = get_sequence_effect(last_seq);
					sh.init(last_seq);
				}
			}

			update_changed_seq_and_deps(scene, last_seq, 0, 1);
		}
	}
	else if(last_seq->type == SEQ_IMAGE) {
		if(okee("Change images")) {
			activate_fileselect(FILE_SPECIAL, 
					    "Select Images", 
					    ed->act_imagedir, 
					    reload_image_strip);
		}
	}
	else if(last_seq->type == SEQ_MOVIE) {
		;
	}
	else if(last_seq->type == SEQ_SCENE) {
		event= pupmenu("Change Scene%t|Update Start and End");

		if(event==1) {
			sce= last_seq->scene;

			last_seq->len= sce->r.efra - sce->r.sfra + 1;
			last_seq->sfra= sce->r.sfra;
			
			/* bad code to change seq->len? update_changed_seq_and_deps() expects the strip->len to be OK */
			new_tstripdata(last_seq);
			
			update_changed_seq_and_deps(scene, last_seq, 1, 1);

		}
	}

}

int seq_effect_find_selected(Scene *scene, Sequence *activeseq, int type, Sequence **selseq1, Sequence **selseq2, Sequence **selseq3, char **error_str)
{
	Editing *ed = seq_give_editing(scene, FALSE);
	Sequence *seq1= 0, *seq2= 0, *seq3= 0, *seq;
	
	*error_str= NULL;

	if (!activeseq)
		seq2= active_seq_get(scene);

	for(seq=ed->seqbasep->first; seq; seq=seq->next) {
		if(seq->flag & SELECT) {
			if (seq->type == SEQ_SOUND) {
				*error_str= "Can't apply effects to audio sequence strips";
				return 0;
			}
			if((seq != activeseq) && (seq != seq2)) {
                                if(seq2==0) seq2= seq;
                                else if(seq1==0) seq1= seq;
                                else if(seq3==0) seq3= seq;
                                else {
									*error_str= "Can't apply effect to more than 3 sequence strips";
									return 0;
                                }
			}
		}
	}
       
	/* make sequence selection a little bit more intuitive
	   for 3 strips: the last-strip should be sequence3 */
	if (seq3 != 0 && seq2 != 0) {
		Sequence *tmp = seq2;
		seq2 = seq3;
		seq3 = tmp;
	}
	

	switch(get_sequence_effect_num_inputs(type)) {
	case 0:
		*selseq1 = *selseq2 = *selseq3 = 0;
		return 1; /* succsess */
	case 1:
		if(seq2==0)  {
			*error_str= "Need at least one selected sequence strip";
			return 0;
		}
		if(seq1==0) seq1= seq2;
		if(seq3==0) seq3= seq2;
	case 2:
		if(seq1==0 || seq2==0) {
			*error_str= "Need 2 selected sequence strips";
			return 0;
		}
		if(seq3==0) seq3= seq2;
	}
	
	if (seq1==NULL && seq2==NULL && seq3==NULL) {
		*error_str= "TODO: in what cases does this happen?";
		return 0;
	}
	
	*selseq1= seq1;
	*selseq2= seq2;
	*selseq3= seq3;

	return 1;
}

void reassign_inputs_seq_effect(Scene *scene)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq1, *seq2, *seq3, *last_seq = active_seq_get(scene);
	char *error_msg;

	if(last_seq==0 || !(last_seq->type & SEQ_EFFECT)) return;
	if(ed==NULL) return;

	if(!seq_effect_find_selected(scene, last_seq, last_seq->type, &seq1, &seq2, &seq3, &error_msg)) {
		//BKE_report(op->reports, RPT_ERROR, error_msg); // XXX operatorify
		return;
	}
	/* see reassigning would create a cycle */
	if(	seq_is_predecessor(seq1, last_seq) ||
		seq_is_predecessor(seq2, last_seq) ||
		seq_is_predecessor(seq3, last_seq)
	) {
		//BKE_report(op->reports, RPT_ERROR, "Can't reassign inputs: no cycles allowed"); // XXX operatorify
	   	return;
	}
	
	last_seq->seq1 = seq1;
	last_seq->seq2 = seq2;
	last_seq->seq3 = seq3;

	update_changed_seq_and_deps(scene, last_seq, 1, 1);

}

static Sequence *del_seq_find_replace_recurs(Scene *scene, Sequence *seq)
{
	Sequence *seq1, *seq2, *seq3;

	/* try to find a replacement input sequence, and flag for later deletion if
	   no replacement can be found */

	if(!seq)
		return NULL;
	else if(!(seq->type & SEQ_EFFECT))
		return ((seq->flag & SELECT)? NULL: seq);
	else if(!(seq->flag & SELECT)) {
		/* try to find replacement for effect inputs */
		seq1= del_seq_find_replace_recurs(scene, seq->seq1);
		seq2= del_seq_find_replace_recurs(scene, seq->seq2);
		seq3= del_seq_find_replace_recurs(scene, seq->seq3);

		if(seq1==seq->seq1 && seq2==seq->seq2 && seq3==seq->seq3);
		else if(seq1 || seq2 || seq3) {
			seq->seq1= (seq1)? seq1: (seq2)? seq2: seq3;
			seq->seq2= (seq2)? seq2: (seq1)? seq1: seq3;
			seq->seq3= (seq3)? seq3: (seq1)? seq1: seq2;

			update_changed_seq_and_deps(scene, seq, 1, 1);
		}
		else
			seq->flag |= SELECT; /* mark for delete */
	}

	if (seq->flag & SELECT) {
		if((seq1 = del_seq_find_replace_recurs(scene, seq->seq1))) return seq1;
		if((seq2 = del_seq_find_replace_recurs(scene, seq->seq2))) return seq2;
		if((seq3 = del_seq_find_replace_recurs(scene, seq->seq3))) return seq3;
		else return NULL;
	}
	else
		return seq;
}

static void recurs_del_seq_flag(Scene *scene, ListBase *lb, short flag, short deleteall)
{
	Sequence *seq, *seqn;
	Sequence *last_seq = active_seq_get(scene);

	seq= lb->first;
	while(seq) {
		seqn= seq->next;
		if((seq->flag & flag) || deleteall) {
			if(seq->type==SEQ_SOUND && seq->sound)
				seq->sound->id.us--;

			BLI_remlink(lb, seq);
			if(seq==last_seq) active_seq_set(scene, NULL);
			if(seq->type==SEQ_META) recurs_del_seq_flag(scene, &seq->seqbase, flag, 1);
			if(seq->ipo) seq->ipo->id.us--;
			seq_free_sequence(scene, seq);
		}
		seq= seqn;
	}
}

static Sequence *dupli_seq(struct Scene *scene, Sequence *seq)
{
	Sequence *seqn = MEM_dupallocN(seq);

	seq->tmp = seqn;
	seqn->strip= MEM_dupallocN(seq->strip);

	// XXX: add F-Curve duplication stuff?
		
	seqn->strip->tstripdata = 0;
	seqn->strip->tstripdata_startstill = 0;
	seqn->strip->tstripdata_endstill = 0;
	seqn->strip->ibuf_startstill = 0;
	seqn->strip->ibuf_endstill = 0;

	if (seq->strip->crop) {
		seqn->strip->crop = MEM_dupallocN(seq->strip->crop);
	}

	if (seq->strip->transform) {
		seqn->strip->transform = MEM_dupallocN(seq->strip->transform);
	}

	if (seq->strip->proxy) {
		seqn->strip->proxy = MEM_dupallocN(seq->strip->proxy);
	}

	if (seq->strip->color_balance) {
		seqn->strip->color_balance 
			= MEM_dupallocN(seq->strip->color_balance);
	}
	
	if(seq->type==SEQ_META) {
		seqn->strip->stripdata = 0;

		seqn->seqbase.first= seqn->seqbase.last= 0;
		/* WATCH OUT!!! - This metastrip is not recursively duplicated here - do this after!!! */
		/* - recurs_dupli_seq(&seq->seqbase,&seqn->seqbase);*/
	} else if(seq->type == SEQ_SCENE) {
		seqn->strip->stripdata = 0;
	} else if(seq->type == SEQ_MOVIE) {
		seqn->strip->stripdata = 
				MEM_dupallocN(seq->strip->stripdata);
		seqn->anim= 0;
	} else if(seq->type == SEQ_SOUND) {
		seqn->strip->stripdata =
				MEM_dupallocN(seq->strip->stripdata);
		if(seq->scene_sound)
			seqn->scene_sound = sound_add_scene_sound(scene, seqn, seq->startdisp, seq->enddisp, seq->startofs + seq->anim_startofs);

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

		seqn->strip->stripdata = 0;
		
	} else {
		fprintf(stderr, "Aiiiiekkk! sequence type not "
				"handled in duplicate!\nExpect a crash"
						" now...\n");
	}

	seqbase_unique_name_recursive(&scene->ed->seqbase, seqn);

	return seqn;
}

static Sequence * deep_dupli_seq(struct Scene *scene, Sequence * seq)
{
	Sequence * seqn = dupli_seq(scene, seq);
	if (seq->type == SEQ_META) {
		Sequence * s;
		for(s= seq->seqbase.first; s; s = s->next) {
			Sequence * n = deep_dupli_seq(scene, s);
			if (n) { 
				BLI_addtail(&seqn->seqbase, n);
			}
		}
	}
	return seqn;
}


static void recurs_dupli_seq(Scene *scene, ListBase *old, ListBase *new, int do_context)
{
	Sequence *seq;
	Sequence *seqn = 0;
	Sequence *last_seq = active_seq_get(scene);

	for(seq= old->first; seq; seq= seq->next) {
		seq->tmp= NULL;
		if(seq->flag & SELECT) {
			seqn = dupli_seq(scene, seq);
			if (seqn) { /*should never fail */
				if(do_context) {
					seq->flag &= SEQ_DESEL;
					seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL+SEQ_LOCK);
				}

				BLI_addtail(new, seqn);
				if(seq->type==SEQ_META)
					recurs_dupli_seq(scene, &seq->seqbase,&seqn->seqbase, do_context);
				
				if(do_context) {
					if (seq == last_seq) {
						active_seq_set(scene, seqn);
					}
				}
			}
		}
	}
}

static Sequence *cut_seq_hard(Scene *scene, Sequence * seq, int cutframe)
{
	TransSeq ts;
	Sequence *seqn = 0;
	int skip_dup = FALSE;

	/* backup values */
	ts.start= seq->start;
	ts.machine= seq->machine;
	ts.startstill= seq->startstill;
	ts.endstill= seq->endstill;
	ts.startdisp= seq->startdisp;
	ts.enddisp= seq->enddisp;
	ts.startofs= seq->anim_startofs;
	ts.endofs= seq->anim_endofs;
	ts.len= seq->len;
	
	/* First Strip! */
	/* strips with extended stillfames before */
	
	if ((seq->startstill) && (cutframe <seq->start)) {
		/* don't do funny things with METAs ... */
		if (seq->type == SEQ_META) {
			skip_dup = TRUE;
			seq->startstill = seq->start - cutframe;
		} else {
			seq->start= cutframe -1;
			seq->startstill= cutframe -seq->startdisp -1;
			seq->anim_endofs += seq->len - 1;
			seq->endstill= 0;
		}
	}
	/* normal strip */
	else if ((cutframe >=seq->start)&&(cutframe <=(seq->start+seq->len))) {
		seq->endofs = 0;
		seq->endstill = 0;
		seq->anim_endofs += (seq->start+seq->len) - cutframe;
	}
	/* strips with extended stillframes after */
	else if (((seq->start+seq->len) < cutframe) && (seq->endstill)) {
		seq->endstill -= seq->enddisp - cutframe;
		/* don't do funny things with METAs ... */
		if (seq->type == SEQ_META) {
			skip_dup = TRUE;
		}
	}
	
	reload_sequence_new_file(scene, seq);
	calc_sequence(scene, seq);

	if (!skip_dup) {
		/* Duplicate AFTER the first change */
		seqn = deep_dupli_seq(scene, seq);
	}
	
	if (seqn) { 
		seqn->flag |= SELECT;
			
		/* Second Strip! */
		/* strips with extended stillframes before */
		if ((seqn->startstill) && (cutframe == seqn->start + 1)) {
			seqn->start = ts.start;
			seqn->startstill= ts.start- cutframe;
			seqn->anim_endofs = ts.endofs;
			seqn->endstill = ts.endstill;
		}
		
		/* normal strip */
		else if ((cutframe>=seqn->start)&&(cutframe<=(seqn->start+seqn->len))) {
			seqn->start = cutframe;
			seqn->startstill = 0;
			seqn->startofs = 0;
			seqn->anim_startofs += cutframe - ts.start;
			seqn->anim_endofs = ts.endofs;
			seqn->endstill = ts.endstill;
		}				
		
		/* strips with extended stillframes after */
		else if (((seqn->start+seqn->len) < cutframe) && (seqn->endstill)) {
			seqn->start = cutframe;
			seqn->startofs = 0;
			seqn->anim_startofs += ts.len-1;
			seqn->endstill = ts.enddisp - cutframe -1;
			seqn->startstill = 0;
		}
		
		reload_sequence_new_file(scene, seqn);
		calc_sequence(scene, seqn);
	}
	return seqn;
}

static Sequence *cut_seq_soft(Scene *scene, Sequence * seq, int cutframe)
{
	TransSeq ts;
	Sequence *seqn = 0;
	int skip_dup = FALSE;

	/* backup values */
	ts.start= seq->start;
	ts.machine= seq->machine;
	ts.startstill= seq->startstill;
	ts.endstill= seq->endstill;
	ts.startdisp= seq->startdisp;
	ts.enddisp= seq->enddisp;
	ts.startofs= seq->startofs;
	ts.endofs= seq->endofs;
	ts.len= seq->len;
	
	/* First Strip! */
	/* strips with extended stillfames before */
	
	if ((seq->startstill) && (cutframe <seq->start)) {
		/* don't do funny things with METAs ... */
		if (seq->type == SEQ_META) {
			skip_dup = TRUE;
			seq->startstill = seq->start - cutframe;
		} else {
			seq->start= cutframe -1;
			seq->startstill= cutframe -seq->startdisp -1;
			seq->endofs = seq->len - 1;
			seq->endstill= 0;
		}
	}
	/* normal strip */
	else if ((cutframe >=seq->start)&&(cutframe <=(seq->start+seq->len))) {
		seq->endofs = (seq->start+seq->len) - cutframe;
	}
	/* strips with extended stillframes after */
	else if (((seq->start+seq->len) < cutframe) && (seq->endstill)) {
		seq->endstill -= seq->enddisp - cutframe;
		/* don't do funny things with METAs ... */
		if (seq->type == SEQ_META) {
			skip_dup = TRUE;
		}
	}
	
	calc_sequence(scene, seq);

	if (!skip_dup) {
		/* Duplicate AFTER the first change */
		seqn = deep_dupli_seq(scene, seq);
	}
	
	if (seqn) { 
		seqn->flag |= SELECT;
			
		/* Second Strip! */
		/* strips with extended stillframes before */
		if ((seqn->startstill) && (cutframe == seqn->start + 1)) {
			seqn->start = ts.start;
			seqn->startstill= ts.start- cutframe;
			seqn->endofs = ts.endofs;
			seqn->endstill = ts.endstill;
		}
		
		/* normal strip */
		else if ((cutframe>=seqn->start)&&(cutframe<=(seqn->start+seqn->len))) {
			seqn->startstill = 0;
			seqn->startofs = cutframe - ts.start;
			seqn->endofs = ts.endofs;
			seqn->endstill = ts.endstill;
		}				
		
		/* strips with extended stillframes after */
		else if (((seqn->start+seqn->len) < cutframe) && (seqn->endstill)) {
			seqn->start = cutframe - ts.len +1;
			seqn->startofs = ts.len-1;
			seqn->endstill = ts.enddisp - cutframe -1;
			seqn->startstill = 0;
		}
		
		calc_sequence(scene, seqn);
	}
	return seqn;
}


/* like duplicate, but only duplicate and cut overlapping strips,
 * strips to the left of the cutframe are ignored and strips to the right are moved into the new list */
static int cut_seq_list(Scene *scene, ListBase *old, ListBase *new, int cutframe,
			Sequence * (*cut_seq)(Scene *, Sequence *, int))
{
	int did_something = FALSE;
	Sequence *seq, *seq_next;
	
	seq= old->first;
	
	while(seq) {
		seq_next = seq->next; /* we need this because we may remove seq */
		
		seq->tmp= NULL;
		if(seq->flag & SELECT) {
			if(cutframe > seq->startdisp && 
			   cutframe < seq->enddisp) {
				Sequence * seqn = cut_seq(scene, seq, cutframe);
				if (seqn) {
					BLI_addtail(new, seqn);
				}
				did_something = TRUE;
			} else if (seq->enddisp <= cutframe) {
				/* do nothing */
			} else if (seq->startdisp >= cutframe) {
				/* move into new list */
				BLI_remlink(old, seq);
				BLI_addtail(new, seq);
			}
		}
		seq = seq_next;
	}
	return did_something;
}

int insert_gap(Scene *scene, int gap, int cfra)
{
	Sequence *seq;
	Editing *ed= seq_give_editing(scene, FALSE);
	int done=0;

	/* all strips >= cfra are shifted */
	
	if(ed==NULL) return 0;

	SEQP_BEGIN(ed, seq) {
		if(seq->startdisp >= cfra) {
			seq->start+= gap;
			calc_sequence(scene, seq);
			done= 1;
		}
	}
	SEQ_END

	return done;
}

void touch_seq_files(Scene *scene)
{
	Sequence *seq;
	Editing *ed= seq_give_editing(scene, FALSE);
	char str[256];

	/* touch all strips with movies */
	
	if(ed==NULL) return;

	if(okee("Touch and print selected movies")==0) return;

	waitcursor(1);

	SEQP_BEGIN(ed, seq) {
		if(seq->flag & SELECT) {
			if(seq->type==SEQ_MOVIE) {
				if(seq->strip && seq->strip->stripdata) {
					BLI_make_file_string(G.sce, str, seq->strip->dir, seq->strip->stripdata->name);
					BLI_touch(seq->name);
				}
			}

		}
	}
	SEQ_END

	waitcursor(0);
}

void set_filter_seq(Scene *scene)
{
	Sequence *seq;
	Editing *ed= seq_give_editing(scene, FALSE);

	
	if(ed==NULL) return;

	if(okee("Set Deinterlace")==0) return;

	SEQP_BEGIN(ed, seq) {
		if(seq->flag & SELECT) {
			if(seq->type==SEQ_MOVIE) {
				seq->flag |= SEQ_FILTERY;
				reload_sequence_new_file(scene, seq);
			}

		}
	}
	SEQ_END

}

void seq_remap_paths(Scene *scene)
{
	Sequence *seq, *last_seq = active_seq_get(scene);
	Editing *ed= seq_give_editing(scene, FALSE);
	char from[FILE_MAX], to[FILE_MAX], stripped[FILE_MAX];
	
	
	if(last_seq==NULL) 
		return;
	
	BLI_strncpy(from, last_seq->strip->dir, FILE_MAX);
// XXX	if (0==sbutton(from, 0, sizeof(from)-1, "From: "))
//		return;
	
	strcpy(to, from);
// XXX	if (0==sbutton(to, 0, sizeof(to)-1, "To: "))
//		return;
	
	if (strcmp(to, from)==0)
		return;
	
	SEQP_BEGIN(ed, seq) {
		if(seq->flag & SELECT) {
			if(strncmp(seq->strip->dir, from, strlen(from))==0) {
				printf("found %s\n", seq->strip->dir);
				
				/* strip off the beginning */
				stripped[0]= 0;
				BLI_strncpy(stripped, seq->strip->dir + strlen(from), FILE_MAX);
				
				/* new path */
				BLI_strncpy(seq->strip->dir, to, FILE_MAX);
				strcat(seq->strip->dir, stripped);
				printf("new %s\n", seq->strip->dir);
			}
		}
	}
	SEQ_END
		
}


void no_gaps(Scene *scene)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	int cfra, first= 0, done;

	
	if(ed==NULL) return;

	for(cfra= CFRA; cfra<=EFRA; cfra++) {
		if(first==0) {
			if( evaluate_seq_frame(scene, cfra) ) first= 1;
		}
		else {
			done= 1;
			while( evaluate_seq_frame(scene, cfra) == 0) {
				done= insert_gap(scene, -1, cfra);
				if(done==0) break;
			}
			if(done==0) break;
		}
	}

}

#if 0
static int seq_get_snaplimit(View2D *v2d)
{
	/* fake mouse coords to get the snap value
	a bit lazy but its only done once pre transform */
	float xmouse, ymouse, x;
	short mval[2] = {24, 0}; /* 24 screen px snap */
	
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &xmouse, &ymouse);
	x = xmouse;
	mval[0] = 0;
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &xmouse, &ymouse);
	return (int)(x - xmouse);
}
#endif

/* Operator functions */

/* snap operator*/
static int sequencer_snap_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;
	int snap_frame;
	
	if(ed==NULL) return OPERATOR_CANCELLED;

	snap_frame= RNA_int_get(op->ptr, "frame");

	/* problem: contents of meta's are all shifted to the same position... */

	/* also check metas */
	SEQP_BEGIN(ed, seq) {
		if (seq->flag & SELECT && !(seq->depth==0 && seq->flag & SEQ_LOCK) &&
		    seq_tx_test(seq)) {
			if((seq->flag & (SEQ_LEFTSEL+SEQ_RIGHTSEL))==0) {
				seq->start= snap_frame-seq->startofs+seq->startstill;
			} else { 
				if(seq->flag & SEQ_LEFTSEL) {
					seq_tx_set_final_left(seq, snap_frame);
				} else { /* SEQ_RIGHTSEL */
					seq_tx_set_final_right(seq, snap_frame);
				}
				seq_tx_handle_xlimits(seq, seq->flag & SEQ_LEFTSEL, seq->flag & SEQ_RIGHTSEL);
			}
			calc_sequence(scene, seq);
		}
	}
	SEQ_END

	/* test for effects and overlap */
	SEQP_BEGIN(ed, seq) {
		if(seq->flag & SELECT && !(seq->depth==0 && seq->flag & SEQ_LOCK)) {
			seq->flag &= ~SEQ_OVERLAP;
			if( seq_test_overlap(ed->seqbasep, seq) ) {
				shuffle_seq(ed->seqbasep, seq, scene);
			}
		}
		else if(seq->type & SEQ_EFFECT) {
			if(seq->seq1 && (seq->seq1->flag & SELECT)) 
				calc_sequence(scene, seq);
			else if(seq->seq2 && (seq->seq2->flag & SELECT)) 
				calc_sequence(scene, seq);
			else if(seq->seq3 && (seq->seq3->flag & SELECT)) 
				calc_sequence(scene, seq);
		}
	}
	SEQ_END;

	/* as last: */
	sort_seq(scene);
	
	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);
	
	return OPERATOR_FINISHED;
}

static int sequencer_snap_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene = CTX_data_scene(C);
	
	int snap_frame;
	
	snap_frame= CFRA;
	
	RNA_int_set(op->ptr, "frame", snap_frame);
	return sequencer_snap_exec(C, op);
}

void SEQUENCER_OT_snap(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Snap strips";
	ot->idname= "SEQUENCER_OT_snap";
	ot->description="Frame where selected strips will be snapped";
	
	/* api callbacks */
	ot->invoke= sequencer_snap_invoke;
	ot->exec= sequencer_snap_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "frame", 0, INT_MIN, INT_MAX, "Frame", "Frame where selected strips will be snapped", INT_MIN, INT_MAX);
}

/* mute operator */
static int sequencer_mute_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;
	int selected;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	selected= !RNA_boolean_get(op->ptr, "unselected");
	
	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if ((seq->flag & SEQ_LOCK)==0) {
			if(selected){ /* mute unselected */
				if(seq->flag & SELECT)
					seq->flag |= SEQ_MUTE;
			}
			else {
				if((seq->flag & SELECT)==0)
					seq->flag |= SEQ_MUTE;
			}
		}
	}
	
	seq_update_muting(scene, ed);
	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);
	
	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_mute(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mute Strips";
	ot->idname= "SEQUENCER_OT_mute";
	ot->description="Mute selected strips";
	
	/* api callbacks */
	ot->exec= sequencer_mute_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Mute unselected rather than selected strips.");
}


/* unmute operator */
static int sequencer_unmute_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;
	int selected;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	selected= !RNA_boolean_get(op->ptr, "unselected");
	
	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if ((seq->flag & SEQ_LOCK)==0) {
			if(selected){ /* unmute unselected */
				if(seq->flag & SELECT)
					seq->flag &= ~SEQ_MUTE;
			}
			else {
				if((seq->flag & SELECT)==0)
					seq->flag &= ~SEQ_MUTE;
			}
		}
	}
	
	seq_update_muting(scene, ed);
	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);
	
	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_unmute(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "UnMute Strips";
	ot->idname= "SEQUENCER_OT_unmute";
	ot->description="UnMute unselected rather than selected strips";
	
	/* api callbacks */
	ot->exec= sequencer_unmute_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "UnMute unselected rather than selected strips.");
}


/* lock operator */
static int sequencer_lock_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if (seq->flag & SELECT) {
			seq->flag |= SEQ_LOCK;
		}
	}

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_lock(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Lock Strips";
	ot->idname= "SEQUENCER_OT_lock";
	ot->description="Lock the active strip so that it can't be transformed";
	
	/* api callbacks */
	ot->exec= sequencer_lock_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* unlock operator */
static int sequencer_unlock_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if (seq->flag & SELECT) {
			seq->flag &= ~SEQ_LOCK;
		}
	}

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_unlock(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "UnLock Strips";
	ot->idname= "SEQUENCER_OT_unlock";
	ot->description="Unlock the active strip so that it can't be transformed";
	
	/* api callbacks */
	ot->exec= sequencer_unlock_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* reload operator */
static int sequencer_reload_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if(seq->flag & SELECT) {
			update_changed_seq_and_deps(scene, seq, 0, 1);
		}
	}

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_reload(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reload Strips";
	ot->idname= "SEQUENCER_OT_reload";
	ot->description="Reload strips in the sequencer";
	
	/* api callbacks */
	ot->exec= sequencer_reload_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* reload operator */
static int sequencer_refresh_all_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	
	if(ed==NULL)
		return OPERATOR_CANCELLED;

	free_imbuf_seq(scene, &ed->seqbase, FALSE);

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_refresh_all(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Refresh Sequencer";
	ot->idname= "SEQUENCER_OT_refresh_all";
	ot->description="Refresh the sequencer editor";
	
	/* api callbacks */
	ot->exec= sequencer_refresh_all_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* cut operator */
static EnumPropertyItem prop_cut_types[] = {
	{SEQ_CUT_SOFT, "SOFT", 0, "Soft", ""},
	{SEQ_CUT_HARD, "HARD", 0, "Hard", ""},
	{0, NULL, 0, NULL, NULL}
};

static int sequencer_cut_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	int cut_side, cut_hard, cut_frame;

	ListBase newlist;
	int changed;
	
	if(ed==NULL)
		return OPERATOR_CANCELLED;

	cut_frame= RNA_int_get(op->ptr, "frame");
	cut_hard= RNA_enum_get(op->ptr, "type");
	cut_side= RNA_enum_get(op->ptr, "side");
	
	newlist.first= newlist.last= NULL;

	if (cut_hard==SEQ_CUT_HARD) {
		changed = cut_seq_list(scene,
			ed->seqbasep, &newlist, cut_frame, cut_seq_hard);
	} else {
		changed = cut_seq_list(scene,
			ed->seqbasep, &newlist, cut_frame, cut_seq_soft);
	}
	
	if (newlist.first) { /* got new strips ? */
		Sequence *seq;
		addlisttolist(ed->seqbasep, &newlist);

		if (cut_side != SEQ_SIDE_BOTH) {
			SEQP_BEGIN(ed, seq) {
				if (cut_side==SEQ_SIDE_LEFT) {
					if ( seq->startdisp >= cut_frame ) {
						seq->flag &= SEQ_DESEL;
					}
				} else {
					if ( seq->enddisp <= cut_frame ) {
						seq->flag &= SEQ_DESEL;
					}
				}
			}
			SEQ_END;
		}
		/* as last: */
		sort_seq(scene);
	}

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);
	
	return OPERATOR_FINISHED;
}


static int sequencer_cut_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	View2D *v2d= UI_view2d_fromcontext(C);
	
	int cut_side, cut_frame;
	
	cut_frame= CFRA;
	cut_side= mouse_frame_side(v2d, event->x - ar->winrct.xmin, cut_frame);
	
	RNA_int_set(op->ptr, "frame", cut_frame);
	RNA_enum_set(op->ptr, "side", cut_side);
	/*RNA_enum_set(op->ptr, "type", cut_hard); */ /*This type is set from the key shortcut */

	return sequencer_cut_exec(C, op);
}


void SEQUENCER_OT_cut(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Cut Strips";
	ot->idname= "SEQUENCER_OT_cut";
	ot->description="Cut the selected strips";
	
	/* api callbacks */
	ot->invoke= sequencer_cut_invoke;
	ot->exec= sequencer_cut_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "frame", 0, INT_MIN, INT_MAX, "Frame", "Frame where selected strips will be cut", INT_MIN, INT_MAX);
	RNA_def_enum(ot->srna, "type", prop_cut_types, SEQ_CUT_SOFT, "Type", "The type of cut operation to perform on strips");
	RNA_def_enum(ot->srna, "side", prop_side_types, SEQ_SIDE_BOTH, "Side", "The side that remains selected after cutting");
}

/* duplicate operator */
static int sequencer_add_duplicate_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);

	ListBase new= {NULL, NULL};

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	recurs_dupli_seq(scene, ed->seqbasep, &new, TRUE);

	if(new.first) {
		Sequence * seq= new.first;
		/* rely on the new list being added at the end */
		addlisttolist(ed->seqbasep, &new);

		for( ; seq; seq= seq->next)
			seqbase_unique_name_recursive(&ed->seqbase, seq);

		WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

static int sequencer_add_duplicate_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sequencer_add_duplicate_exec(C, op);

	RNA_int_set(op->ptr, "mode", TFM_TRANSLATION);
	WM_operator_name_call(C, "TRANSFORM_OT_transform", WM_OP_INVOKE_REGION_WIN, op->ptr);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_duplicate(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "Duplicate";
	ot->idname= "SEQUENCER_OT_duplicate";
	ot->description="Duplicate the selected strips";
	
	/* api callbacks */
	ot->invoke= sequencer_add_duplicate_invoke;
	ot->exec= sequencer_add_duplicate_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* to give to transform */
	RNA_def_int(ot->srna, "mode", TFM_TRANSLATION, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

/* delete operator */
static int sequencer_delete_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;
	MetaStack *ms;
	int nothingSelected = TRUE;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	seq=active_seq_get(scene);
	if (seq && seq->flag & SELECT) { /* avoid a loop since this is likely to be selected */
		nothingSelected = FALSE;
	} else {
		for (seq = ed->seqbasep->first; seq; seq = seq->next) {
			if (seq->flag & SELECT) {
				nothingSelected = FALSE;
				break;
			}
		}
	}

	if (nothingSelected)
		return OPERATOR_FINISHED;

	/* free imbufs of all dependent strips */
	for(seq=ed->seqbasep->first; seq; seq=seq->next)
		if(seq->flag & SELECT)
			update_changed_seq_and_deps(scene, seq, 1, 0);

	/* for effects, try to find a replacement input */
	for(seq=ed->seqbasep->first; seq; seq=seq->next)
		if((seq->type & SEQ_EFFECT) && !(seq->flag & SELECT))
			del_seq_find_replace_recurs(scene, seq);

	/* delete all selected strips */
	recurs_del_seq_flag(scene, ed->seqbasep, SELECT, 0);

	/* updates lengths etc */
	seq= ed->seqbasep->first;
	while(seq) {
		calc_sequence(scene, seq);
		seq= seq->next;
	}

	/* free parent metas */
	ms= ed->metastack.last;
	while(ms) {
		ms->parseq->strip->len= 0;		/* force new alloc */
		calc_sequence(scene, ms->parseq);
		ms= ms->prev;
	}

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);
	
	return OPERATOR_FINISHED;
}


void SEQUENCER_OT_delete(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "Erase Strips";
	ot->idname= "SEQUENCER_OT_delete";
	ot->description="Erase selected strips from the sequencer";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= sequencer_delete_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


/* separate_images operator */
static int sequencer_separate_images_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	
	Sequence *seq, *seq_new, *seq_next;
	Strip *strip_new;
	StripElem *se, *se_new;
	int start_ofs, cfra, frame_end;
	int step= RNA_int_get(op->ptr, "length");

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	seq= ed->seqbasep->first;

	while (seq) {
		if((seq->flag & SELECT) && (seq->type == SEQ_IMAGE) && (seq->len > 1)) {
			/* remove seq so overlap tests dont conflict,
			see seq_free_sequence below for the real free'ing */
			seq_next = seq->next;
			BLI_remlink(ed->seqbasep, seq);
			if(seq->ipo) seq->ipo->id.us--;

			start_ofs = cfra = seq_tx_get_final_left(seq, 0);
			frame_end = seq_tx_get_final_right(seq, 0);

			while (cfra < frame_end) {
				/* new seq */
				se = give_stripelem(seq, cfra);

				seq_new= alloc_sequence(ed->seqbasep, start_ofs, seq->machine);
				seq_new->type= SEQ_IMAGE;
				seq_new->len = 1;
				seq_new->endstill = step-1;

				/* new strip */
				seq_new->strip= strip_new= MEM_callocN(sizeof(Strip)*1, "strip");
				strip_new->len= 1;
				strip_new->us= 1;
				strncpy(strip_new->dir, seq->strip->dir, FILE_MAXDIR-1);

				/* new stripdata */
				strip_new->stripdata= se_new= MEM_callocN(sizeof(StripElem)*1, "stripelem");
				strncpy(se_new->name, se->name, FILE_MAXFILE-1);
				calc_sequence(scene, seq_new);
				seq_new->flag &= ~SEQ_OVERLAP;
				if (seq_test_overlap(ed->seqbasep, seq_new)) {
					shuffle_seq(ed->seqbasep, seq_new, scene);
				}

				seqbase_unique_name_recursive(&scene->ed->seqbase, seq_new);

				cfra++;
				start_ofs += step;
			}

			seq_free_sequence(scene, seq);
			seq = seq->next;
		} else {
			seq = seq->next;
		}
	}

	/* as last: */
	sort_seq(scene);
	
	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}


void SEQUENCER_OT_images_separate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Separate Images";
	ot->idname= "SEQUENCER_OT_images_separate";
	ot->description="On image sequences strips, it return a strip for each image";
	
	/* api callbacks */
	ot->invoke= WM_operator_props_popup;
	ot->exec= sequencer_separate_images_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_int(ot->srna, "length", 1, 1, 1000, "Length", "Length of each frame", 1, INT_MAX);
}


/* META Operators */

/* separate_meta_toggle operator */
static int sequencer_meta_toggle_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *last_seq= active_seq_get(scene);
	MetaStack *ms;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	if(last_seq && last_seq->type==SEQ_META && last_seq->flag & SELECT) {
		/* Enter Metastrip */
		ms= MEM_mallocN(sizeof(MetaStack), "metastack");
		BLI_addtail(&ed->metastack, ms);
		ms->parseq= last_seq;
		ms->oldbasep= ed->seqbasep;

		ed->seqbasep= &last_seq->seqbase;

		active_seq_set(scene, NULL);

	}
	else {
		/* Exit Metastrip (if possible) */

		Sequence *seq;

		if(ed->metastack.first==NULL)
			return OPERATOR_CANCELLED;

		ms= ed->metastack.last;
		BLI_remlink(&ed->metastack, ms);

		ed->seqbasep= ms->oldbasep;

		/* recalc all: the meta can have effects connected to it */
		for(seq= ed->seqbasep->first; seq; seq= seq->next)
			calc_sequence(scene, seq);

		active_seq_set(scene, ms->parseq);

		ms->parseq->flag |= SELECT;
		recurs_sel_seq(ms->parseq);

		MEM_freeN(ms);

	}

	seq_update_muting(scene, ed);
	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Meta Strip";
	ot->idname= "SEQUENCER_OT_meta_toggle";
	ot->description="Toggle a metastrip (to edit enclosed strips)";
	
	/* api callbacks */
	ot->exec= sequencer_meta_toggle_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


/* separate_meta_make operator */
static int sequencer_meta_make_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	
	Sequence *seq, *seqm, *next;
	
	int channel_max= 1;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	if(seqbase_isolated_sel_check(ed->seqbasep)==FALSE) {
		BKE_report(op->reports, RPT_ERROR, "Please select all related strips");
		return OPERATOR_CANCELLED;
	}

	/* remove all selected from main list, and put in meta */

	seqm= alloc_sequence(ed->seqbasep, 1, 1); /* channel number set later */
	strcpy(seqm->name+2, "MetaStrip");
	seqm->type= SEQ_META;
	seqm->flag= SELECT;

	seq= ed->seqbasep->first;
	while(seq) {
		next= seq->next;
		if(seq!=seqm && (seq->flag & SELECT)) {
			channel_max= MAX2(seq->machine, channel_max);
			BLI_remlink(ed->seqbasep, seq);
			BLI_addtail(&seqm->seqbase, seq);
		}
		seq= next;
	}
	seqm->machine= channel_max;
	calc_sequence(scene, seqm);

	seqm->strip= MEM_callocN(sizeof(Strip), "metastrip");
	seqm->strip->len= seqm->len;
	seqm->strip->us= 1;
	
	active_seq_set(scene, seqm);

	if( seq_test_overlap(ed->seqbasep, seqm) ) shuffle_seq(ed->seqbasep, seqm, scene);

	seq_update_muting(scene, ed);

	seqbase_unique_name_recursive(&scene->ed->seqbase, seqm);

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_make(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Meta Strip";
	ot->idname= "SEQUENCER_OT_meta_make";
	ot->description="Group selected strips into a metastrip";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= sequencer_meta_make_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int seq_depends_on_meta(Sequence *seq, Sequence *seqm)
{
	if (seq == seqm) return 1;
	else if (seq->seq1 && seq_depends_on_meta(seq->seq1, seqm)) return 1;
	else if (seq->seq2 && seq_depends_on_meta(seq->seq2, seqm)) return 1;
	else if (seq->seq3 && seq_depends_on_meta(seq->seq3, seqm)) return 1;
	else return 0;
}

/* separate_meta_make operator */
static int sequencer_meta_separate_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);

	Sequence *seq, *last_seq = active_seq_get(scene); /* last_seq checks ed==NULL */

	if(last_seq==NULL || last_seq->type!=SEQ_META)
		return OPERATOR_CANCELLED;

	addlisttolist(ed->seqbasep, &last_seq->seqbase);

	last_seq->seqbase.first= 0;
	last_seq->seqbase.last= 0;

	BLI_remlink(ed->seqbasep, last_seq);
	seq_free_sequence(scene, last_seq);

	/* emtpy meta strip, delete all effects depending on it */
	for(seq=ed->seqbasep->first; seq; seq=seq->next)
		if((seq->type & SEQ_EFFECT) && seq_depends_on_meta(seq, last_seq))
			seq->flag |= SEQ_FLAG_DELETE;

	recurs_del_seq_flag(scene, ed->seqbasep, SEQ_FLAG_DELETE, 0);

	/* test for effects and overlap */
	SEQP_BEGIN(ed, seq) {
		if(seq->flag & SELECT) {
			seq->flag &= ~SEQ_OVERLAP;
			if( seq_test_overlap(ed->seqbasep, seq) ) {
				shuffle_seq(ed->seqbasep, seq, scene);
			}
		}
	}
	SEQ_END;

	sort_seq(scene);
	seq_update_muting(scene, ed);

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_separate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "UnMeta Strip";
	ot->idname= "SEQUENCER_OT_meta_separate";
	ot->description="Put the contents of a metastrip back in the sequencer";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= sequencer_meta_separate_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* view_all operator */
static int sequencer_view_all_exec(bContext *C, wmOperator *op)
{
	//Scene *scene= CTX_data_scene(C);
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *area= CTX_wm_area(C);
	//ARegion *ar= CTX_wm_region(C);
	View2D *v2d= UI_view2d_fromcontext(C);

	v2d->cur= v2d->tot;
	UI_view2d_curRect_validate(v2d);
	UI_view2d_sync(sc, area, v2d, V2D_LOCK_COPY);
	
	ED_area_tag_redraw(CTX_wm_area(C));
	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View All";
	ot->idname= "SEQUENCER_OT_view_all";
	ot->description="View all the strips in the sequencer";
	
	/* api callbacks */
	ot->exec= sequencer_view_all_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER;
}

/* view_all operator */
static int sequencer_view_all_preview_exec(bContext *C, wmOperator *op)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *area= CTX_wm_area(C);
#if 0
	ARegion *ar= CTX_wm_region(C);
	SpaceSeq *sseq= area->spacedata.first;
	Scene *scene= CTX_data_scene(C);
#endif
	View2D *v2d= UI_view2d_fromcontext(C);

	v2d->cur= v2d->tot;
	UI_view2d_curRect_validate(v2d);
	UI_view2d_sync(sc, area, v2d, V2D_LOCK_COPY);
	
#if 0
	/* Like zooming on an image view */
	float zoomX, zoomY;
	int width, height, imgwidth, imgheight;

	width = ar->winx;
	height = ar->winy;

	seq_reset_imageofs(sseq);

	imgwidth= (scene->r.size*scene->r.xsch)/100;
	imgheight= (scene->r.size*scene->r.ysch)/100;

	/* Apply aspect, dosnt need to be that accurate */
	imgwidth= (int)(imgwidth * ((float)scene->r.xasp / (float)scene->r.yasp));

	if (((imgwidth >= width) || (imgheight >= height)) &&
		((width > 0) && (height > 0))) {

		/* Find the zoom value that will fit the image in the image space */
		zoomX = ((float)width) / ((float)imgwidth);
		zoomY = ((float)height) / ((float)imgheight);
		sseq->zoom= (zoomX < zoomY) ? zoomX : zoomY;

		sseq->zoom = 1.0f / power_of_2(1/ MIN2(zoomX, zoomY) );
	}
	else {
		sseq->zoom= 1.0f;
	}
#endif

	ED_area_tag_redraw(CTX_wm_area(C));
	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_all_preview(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View All";
	ot->idname= "SEQUENCER_OT_view_all_preview";
	ot->description="Zoom preview to fit in the area";
	
	/* api callbacks */
	ot->exec= sequencer_view_all_preview_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER;
}

#if 0
static EnumPropertyItem view_type_items[] = {
		{SEQ_VIEW_SEQUENCE, "SEQUENCER", ICON_SEQ_SEQUENCER, "Sequencer", ""},
		{SEQ_VIEW_PREVIEW,  "PREVIEW", ICON_SEQ_PREVIEW, "Image Preview", ""},
		{SEQ_VIEW_SEQUENCE_PREVIEW,  "SEQUENCER_PREVIEW", ICON_SEQ_SEQUENCER, "Sequencer and Image Preview", ""},
		{0, NULL, 0, NULL, NULL}};
#endif

/* view_all operator */
static int sequencer_view_toggle_exec(bContext *C, wmOperator *op)
{
	SpaceSeq *sseq= (SpaceSeq *)CTX_wm_space_data(C);

	sseq->view++;
	if (sseq->view > SEQ_VIEW_SEQUENCE_PREVIEW) sseq->view = SEQ_VIEW_SEQUENCE;

	ED_sequencer_update_view(C, sseq->view);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View Toggle";
	ot->idname= "SEQUENCER_OT_view_toggle";
	ot->description="Toggle between sequencer views (sequence, preview, both)";
	
	/* api callbacks */
	ot->exec= sequencer_view_toggle_exec;
	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER;
}


/* view_selected operator */
static int sequencer_view_selected_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	View2D *v2d= UI_view2d_fromcontext(C);
	ScrArea *area= CTX_wm_area(C);
	bScreen *sc= CTX_wm_screen(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	int xmin=  MAXFRAME*2;
	int xmax= -MAXFRAME*2;
	int ymin=  MAXSEQ+1;
	int ymax= 0;
	int orig_height;
	int ymid;
	int ymargin= 1;
	int xmargin= FPS;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	for(seq=ed->seqbasep->first; seq; seq=seq->next) {
		if(seq->flag & SELECT) {
			xmin= MIN2(xmin, seq->startdisp);
			xmax= MAX2(xmax, seq->enddisp);

			ymin= MIN2(ymin, seq->machine);
			ymax= MAX2(ymax, seq->machine);
		}
	}

	if (ymax != 0) {
		
		xmax += xmargin;
		xmin -= xmargin;
		ymax += ymargin;
		ymin -= ymargin;

		orig_height= v2d->cur.ymax - v2d->cur.ymin;

		v2d->cur.xmin= xmin;
		v2d->cur.xmax= xmax;

		v2d->cur.ymin= ymin;
		v2d->cur.ymax= ymax;

		/* only zoom out vertically */
		if (orig_height > v2d->cur.ymax - v2d->cur.ymin) {
			ymid= (v2d->cur.ymax + v2d->cur.ymin) / 2;

			v2d->cur.ymin= ymid - (orig_height/2);
			v2d->cur.ymax= ymid + (orig_height/2);
		}

		UI_view2d_curRect_validate(v2d);
		UI_view2d_sync(sc, area, v2d, V2D_LOCK_COPY);

		ED_area_tag_redraw(CTX_wm_area(C));
	}
	
	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View Selected";
	ot->idname= "SEQUENCER_OT_view_selected";
	ot->description="Zoom the sequencer on the selected strips";
	
	/* api callbacks */
	ot->exec= sequencer_view_selected_exec;

	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER;
}


static int find_next_prev_edit(Scene *scene, int cfra, int side)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq,*best_seq = NULL,*frame_seq = NULL;
	
	int dist, best_dist;
	best_dist = MAXFRAME*2;

	if(ed==NULL) return cfra;
	
	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		dist = MAXFRAME*2;
			
		switch (side) {
			case SEQ_SIDE_LEFT:
				if (seq->startdisp < cfra) {
					dist = cfra - seq->startdisp;
				}
				break;
			case SEQ_SIDE_RIGHT:
				if (seq->startdisp > cfra) {
					dist = seq->startdisp - cfra;
				} else if (seq->startdisp == cfra) {
					frame_seq=seq;
				}
				break;
		}

		if (dist < best_dist) {
			best_dist = dist;
			best_seq = seq;
		}
	}

	/* if no sequence to the right is found and the
	   frame is on the start of the last sequence,
	   move to the end of the last sequence */
	if (frame_seq) cfra = frame_seq->enddisp;

	return best_seq ? best_seq->startdisp : cfra;
}

static int next_prev_edit_internal(Scene *scene, int side)
{
	int change=0;
	int cfra = CFRA;
	int nfra= find_next_prev_edit(scene, cfra, side);
	
	if (nfra != cfra) {
		CFRA = nfra;
		change= 1;
	}

	return change;
}

/* move frame to next edit point operator */
static int sequencer_next_edit_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	
	if(!next_prev_edit_internal(scene, SEQ_SIDE_RIGHT))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_next_edit(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Next Edit";
	ot->idname= "SEQUENCER_OT_next_edit";
	ot->description="Move frame to next edit point";
	
	/* api callbacks */
	ot->exec= sequencer_next_edit_exec;
	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
}

/* move frame to previous edit point operator */
static int sequencer_previous_edit_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	
	if(!next_prev_edit_internal(scene, SEQ_SIDE_LEFT))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
	
	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_previous_edit(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Previous Edit";
	ot->idname= "SEQUENCER_OT_previous_edit";
	ot->description="Move frame to previous edit point";
	
	/* api callbacks */
	ot->exec= sequencer_previous_edit_exec;
	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
}

static void swap_sequence(Scene* scene, Sequence* seqa, Sequence* seqb)
{
	int gap = seqb->startdisp - seqa->enddisp;
	seqb->start = seqa->start;
	calc_sequence(scene, seqb);
	seqa->start = seqb->enddisp + gap;
	calc_sequence(scene, seqa);
}

#if 0
static Sequence* sequence_find_parent(Scene* scene, Sequence* child)
{
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *parent= NULL;
	Sequence *seq;

	if(ed==NULL) return NULL;

	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if ( (seq != child) && seq_is_parent(seq, child) ) {
			parent = seq;
			break;
		}
	}

	return parent;
}
#endif

static int sequencer_swap_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *active_seq = active_seq_get(scene);
	Sequence *seq, *iseq;
	int side= RNA_enum_get(op->ptr, "side");

	if(ed==NULL) return OPERATOR_CANCELLED;
	if(active_seq==NULL) return OPERATOR_CANCELLED;

	seq = find_next_prev_sequence(scene, active_seq, side, -1);
	
	if(seq) {
		
		/* disallow effect strips */
		if ((seq->type!=SEQ_COLOR) && (seq->effectdata || seq->seq1 || seq->seq2 || seq->seq3))
			return OPERATOR_CANCELLED;
		if ((active_seq->type!=SEQ_COLOR) && (active_seq->effectdata || active_seq->seq1 || active_seq->seq2 || active_seq->seq3))
			return OPERATOR_CANCELLED;

		switch (side) {
			case SEQ_SIDE_LEFT: 
				swap_sequence(scene, seq, active_seq);
				break;
			case SEQ_SIDE_RIGHT: 
				swap_sequence(scene, active_seq, seq);
				break;
		}

		// XXX - should be a generic function
		for(iseq= scene->ed->seqbasep->first; iseq; iseq= iseq->next) {
			if((iseq->type & SEQ_EFFECT) && (seq_is_parent(iseq, active_seq) || seq_is_parent(iseq, seq))) {
				calc_sequence(scene, iseq);
			}
		}

		/* do this in a new loop since both effects need to be calculated first */
		for(iseq= scene->ed->seqbasep->first; iseq; iseq= iseq->next) {
			if((iseq->type & SEQ_EFFECT) && (seq_is_parent(iseq, active_seq) || seq_is_parent(iseq, seq))) {
				/* this may now overlap */
				if( seq_test_overlap(ed->seqbasep, iseq) ) {
					shuffle_seq(ed->seqbasep, iseq, scene);
				}
			}
		}



		sort_seq(scene);

		WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_swap(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Swap Strip";
	ot->idname= "SEQUENCER_OT_swap";
	ot->description="Swap active strip with strip to the left";
	
	/* api callbacks */
	ot->exec= sequencer_swap_exec;
	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_enum(ot->srna, "side", prop_side_lr_types, SEQ_SIDE_RIGHT, "Side", "Side of the strip to swap");
}

static int sequencer_rendersize_exec(bContext *C, wmOperator *op)
{
	int retval = OPERATOR_CANCELLED;
	Scene *scene= CTX_data_scene(C);
	Sequence *active_seq = active_seq_get(scene);

	if(active_seq==NULL)
		return OPERATOR_CANCELLED;

	switch (active_seq->type) {
		case SEQ_IMAGE:
		case SEQ_MOVIE:
			if (active_seq->strip) {
				// prevent setting the render size if sequence values aren't initialized
				if ( (active_seq->strip->orx>0) && (active_seq->strip->ory>0) ) {
					scene->r.xsch= active_seq->strip->orx;
					scene->r.ysch= active_seq->strip->ory;
					WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS, scene);
					retval = OPERATOR_FINISHED;
				}
			}
			break;
		case SEQ_SCENE:
		case SEQ_META:
		case SEQ_RAM_SOUND:
		case SEQ_HD_SOUND:
		default:
			break;
	}
	return retval;
}

void SEQUENCER_OT_rendersize(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Render Size";
	ot->idname= "SEQUENCER_OT_rendersize";
	ot->description="Set render size and aspect from active sequence";
	
	/* api callbacks */
	ot->exec= sequencer_rendersize_exec;
	ot->poll= ED_operator_sequencer_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
}

static void seq_del_sound(Scene *scene, Sequence *seq)
{
	if(seq->type == SEQ_META) {
		Sequence *iseq;
		for(iseq= seq->seqbase.first; iseq; iseq= iseq->next) {
			seq_del_sound(scene, iseq);
		}
	}
	else if(seq->scene_sound) {
		sound_remove_scene_sound(scene, seq->scene_sound);
		seq->scene_sound = NULL;
	}
}

/* TODO, validate scenes */
static int sequencer_copy_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;

	if(ed==NULL)
		return OPERATOR_CANCELLED;

	seq_free_clipboard();

	if(seqbase_isolated_sel_check(ed->seqbasep)==FALSE) {
		BKE_report(op->reports, RPT_ERROR, "Please select all related strips");
		return OPERATOR_CANCELLED;
	}

	recurs_dupli_seq(scene, ed->seqbasep, &seqbase_clipboard, FALSE);
	seqbase_clipboard_frame= scene->r.cfra;

	/* Need to remove anything that references the current scene */
	for(seq= seqbase_clipboard.first; seq; seq= seq->next) {
		seq_del_sound(scene, seq);
	}

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy";
	ot->idname= "SEQUENCER_OT_copy";
	ot->description="";

	/* api callbacks */
	ot->exec= sequencer_copy_exec;
	ot->poll= ED_operator_sequencer_active;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
}

static void seq_offset(Scene *scene, Sequence *seq, int ofs)
{
	if(seq->type == SEQ_META) {
		Sequence *iseq;
		for(iseq= seq->seqbase.first; iseq; iseq= iseq->next) {
			seq_offset(scene, iseq, ofs);
		}
	}
	else {
		seq->start += ofs;
	}

	calc_sequence_disp(scene, seq);
}

static int sequencer_paste_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= seq_give_editing(scene, TRUE); /* create if needed */
	ListBase new = {NULL, NULL};
	int ofs;
	Sequence *iseq;

	deselect_all_seq(scene);
	ofs = scene->r.cfra - seqbase_clipboard_frame;

	recurs_dupli_seq(scene, &seqbase_clipboard, &new, FALSE);

	/* transform pasted strips before adding */
	if(ofs) {
		for(iseq= new.first; iseq; iseq= iseq->next) {
			seq_offset(scene, iseq, ofs);
		}
	}

	addlisttolist(ed->seqbasep, &new);

	WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, scene);

	return OPERATOR_FINISHED;
}

void SEQUENCER_OT_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Paste";
	ot->idname= "SEQUENCER_OT_paste";
	ot->description="";

	/* api callbacks */
	ot->exec= sequencer_paste_exec;
	ot->poll= ED_operator_sequencer_active;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
}
