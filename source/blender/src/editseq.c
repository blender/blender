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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <sys/types.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
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

#include "BKE_utildefines.h"
#include "BKE_plugin_types.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "BIF_space.h"
#include "BIF_interface.h"
#include "BIF_screen.h"
#include "BIF_drawseq.h"
#include "BIF_editseq.h"
#include "BIF_mywindow.h"
#include "BIF_toolbox.h"
#include "BIF_writemovie.h"
#include "BIF_editview.h"
#include "BIF_scrarea.h"
#include "BIF_editsound.h"

#include "BSE_edit.h"
#include "BSE_sequence.h"
#include "BSE_seqeffects.h"
#include "BSE_filesel.h"
#include "BSE_drawipo.h"
#include "BSE_seqaudio.h"

#include "BDR_editobject.h"

#include "blendef.h"
#include "mydevice.h"

static Sequence *_last_seq=0;
static int _last_seq_init=0;

#ifdef WIN32
char last_imagename[FILE_MAXDIR+FILE_MAXFILE]= "c:\\";
#else
char last_imagename[FILE_MAXDIR+FILE_MAXFILE]= "/";
#endif

char last_sounddir[FILE_MAXDIR+FILE_MAXFILE]= "";

#define SEQ_DESEL	~(SELECT+SEQ_LEFTSEL+SEQ_RIGHTSEL)

static int test_overlap_seq(Sequence *);
static void shuffle_seq(Sequence *);

Sequence *get_last_seq()
{
	if(!_last_seq_init) {
		Editing *ed;
		Sequence *seq;

		ed= G.scene->ed;
		if(!ed) return NULL;

		for(seq= ed->seqbasep->first; seq; seq=seq->next)
			if(seq->flag & SELECT)
				_last_seq= seq;

		_last_seq_init = 1;
	}

	return _last_seq;
}

void set_last_seq(Sequence *seq)
{
	_last_seq = seq;
	_last_seq_init = 1;
}

void clear_last_seq()
{
	_last_seq = NULL;
	_last_seq_init = 0;
}

static void change_plugin_seq(char *str)	/* called from fileselect */
{
	struct SeqEffectHandle sh;
	Sequence *last_seq= get_last_seq();

	if(last_seq && last_seq->type != SEQ_PLUGIN) return;

	sh = get_sequence_effect(last_seq);
	sh.free(last_seq);
	sh.init_plugin(last_seq, str);

	last_seq->machine = MAX3(last_seq->seq1->machine, 
				 last_seq->seq2->machine, 
				 last_seq->seq3->machine);

	if( test_overlap_seq(last_seq) ) shuffle_seq(last_seq);
	
	BIF_undo_push("Load/change Sequencer plugin");
}


void boundbox_seq(void)
{
	Sequence *seq;
	Editing *ed;
	float min[2], max[2];

	ed= G.scene->ed;
	if(ed==0) return;

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

	G.v2d->tot.xmin= min[0];
	G.v2d->tot.xmax= max[0];
	G.v2d->tot.ymin= min[1];
	G.v2d->tot.ymax= max[1];

}

int sequence_is_free_transformable(Sequence * seq)
{
	return seq->type < SEQ_EFFECT
		|| (get_sequence_effect_num_inputs(seq->type) == 0);
}

Sequence *find_nearest_seq(int *hand)
{
	Sequence *seq;
	Editing *ed;
	float x, y;
	short mval[2];
	float pixelx;
	float handsize;
	float minhandle, maxhandle;
	View2D *v2d = G.v2d;
	*hand= 0;

	ed= G.scene->ed;
	if(ed==0) return 0;
	
	pixelx = (v2d->cur.xmax - v2d->cur.xmin)/(v2d->mask.xmax - v2d->mask.xmin);

	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	
	seq= ed->seqbasep->first;
	
	while(seq) {
		/* clamp handles to defined size in pixel space */
		handsize = seq->handsize;
		minhandle = 7;
		maxhandle = 28;
		CLAMP(handsize, minhandle*pixelx, maxhandle*pixelx);
		
		if(seq->machine == (int)y) {
			/* check for both normal strips, and strips that have been flipped horizontally */
			if( ((seq->startdisp < seq->enddisp) && (seq->startdisp<=x && seq->enddisp>=x)) ||
				((seq->startdisp > seq->enddisp) && (seq->startdisp>=x && seq->enddisp<=x)) )
			{
				if(sequence_is_free_transformable(seq)) {
					if( handsize+seq->startdisp >=x )
						*hand= 1;
					else if( -handsize+seq->enddisp <=x )
						*hand= 2;
				}
				return seq;
			}
		}
		seq= seq->next;
	}
	return 0;
}

void update_seq_ipo_rect(Sequence * seq)
{
	float start;
	float end;

	if (!seq || !seq->ipo) {
		return;
	}
	start =  -5.0;
	end   =  105.0;

	/* Adjust IPO window to sequence and 
	   avoid annoying snap-back to startframe 
	   when Lock Time is on */
	if (G.v2d->flag & V2D_VIEWLOCK) {
		if ((seq->flag & SEQ_IPO_FRAME_LOCKED) != 0) {
			start = -5.0 + seq->startdisp;
			end = 5.0 + seq->enddisp;
		} else {
			start = (float)G.scene->r.sfra - 0.1;
			end = G.scene->r.efra;
		}
	}

	seq->ipo->cur.xmin= start;
	seq->ipo->cur.xmax= end;
}

void update_seq_icu_rects(Sequence * seq)
{
	IpoCurve *icu= NULL;
	struct SeqEffectHandle sh;

	if (!seq || !seq->ipo) {
		return;
	}

	if(!(seq->type & SEQ_EFFECT)) {
		return;
	}

	sh = get_sequence_effect(seq);

	for(icu= seq->ipo->curve.first; icu; icu= icu->next) {
		sh.store_icu_yrange(seq, icu->adrcode, &icu->ymin, &icu->ymax);
	}
}

static int test_overlap_seq(Sequence *test)
{
	Sequence *seq;
	Editing *ed;

	ed= G.scene->ed;
	if(ed==0) return 0;

	seq= ed->seqbasep->first;
	while(seq) {
		if(seq!=test) {
			if(test->machine==seq->machine) {
				if(test->depth==seq->depth) {
					if( (test->enddisp <= seq->startdisp) || (test->startdisp >= seq->enddisp) );
					else return 1;
				}
			}
		}
		seq= seq->next;
	}
	return 0;
}

static void shuffle_seq(Sequence *test)
{
	Editing *ed;
	Sequence *seq;
	int a, start;

	ed= G.scene->ed;
	if(ed==0) return;

	/* is there more than 1 select: only shuffle y */
	a=0;
	seq= ed->seqbasep->first;
	while(seq) {
		if(seq->flag & SELECT) a++;
		seq= seq->next;
	}

	if(a<2 && test->type==SEQ_IMAGE) {
		start= test->start;

		for(a= 1; a<50; a++) {
			test->start= start+a;
			calc_sequence(test);
			if( test_overlap_seq(test)==0) return;
			test->start= start-a;
			calc_sequence(test);
			if( test_overlap_seq(test)==0) return;
		}
		test->start= start;
	}

	test->machine++;
	calc_sequence(test);
	while( test_overlap_seq(test) ) {
		if(test->machine >= MAXSEQ) {
			error("There is no more space to add a sequence strip");

			BLI_remlink(ed->seqbasep, test);
			free_sequence(test);
			return;
		}
		test->machine++;
		calc_sequence(test);
	}
}

static int seq_is_parent(Sequence *par, Sequence *seq)
{
	return ((par->seq1 == seq) || (par->seq2 == seq) || (par->seq3 == seq));
}

static int seq_is_predecessor(Sequence *pred, Sequence *seq)
{
	if(pred == seq) return 0;
	else if(seq_is_parent(pred, seq)) return 1;
	else if(pred->seq1 && seq_is_predecessor(pred->seq1, seq)) return 1;
	else if(pred->seq2 && seq_is_predecessor(pred->seq2, seq)) return 1;
	else if(pred->seq3 && seq_is_predecessor(pred->seq3, seq)) return 1;

	return 0;
}

static void deselect_all_seq(void)
{
	Sequence *seq;
	Editing *ed;

	ed= G.scene->ed;
	if(ed==0) return;

	WHILE_SEQ(ed->seqbasep) {
		seq->flag &= SEQ_DESEL;
	}
	END_SEQ
		
	BIF_undo_push("(De)select all Sequencer");
}

static void recurs_sel_seq(Sequence *seqm)
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

void swap_select_seq(void)
{
	Sequence *seq;
	Editing *ed;
	int sel=0;

	ed= G.scene->ed;
	if(ed==0) return;

	WHILE_SEQ(ed->seqbasep) {
		if(seq->flag & SELECT) sel= 1;
	}
	END_SEQ

	WHILE_SEQ(ed->seqbasep) {
		/* always deselect all to be sure */
		seq->flag &= SEQ_DESEL;
		if(sel==0) seq->flag |= SELECT;
	}
	END_SEQ

	allqueue(REDRAWSEQ, 0);
	BIF_undo_push("Swap select all Sequencer");

}

void mouse_select_seq(void)
{
	Sequence *seq;
	int hand;

	seq= find_nearest_seq(&hand);

	if(!(G.qual & LR_SHIFTKEY)) deselect_all_seq();

	if(seq) {
		set_last_seq(seq);

		if ((seq->type == SEQ_IMAGE) || (seq->type == SEQ_MOVIE)) {
			if(seq->strip) {
				strncpy(last_imagename, seq->strip->dir, FILE_MAXDIR-1);
			}
		} else
		if (seq->type == SEQ_HD_SOUND || seq->type == SEQ_RAM_SOUND) {
			if(seq->strip) {
				strncpy(last_sounddir, seq->strip->dir, FILE_MAXDIR-1);
			}
		}

		if((G.qual & LR_SHIFTKEY) && (seq->flag & SELECT)) {
			if(hand==0) seq->flag &= SEQ_DESEL;
			else if(hand==1) {
				if(seq->flag & SEQ_LEFTSEL) 
					seq->flag &= ~SEQ_LEFTSEL;
				else seq->flag |= SEQ_LEFTSEL;
			}
			else if(hand==2) {
				if(seq->flag & SEQ_RIGHTSEL) 
					seq->flag &= ~SEQ_RIGHTSEL;
				else seq->flag |= SEQ_RIGHTSEL;
			}
		}
		else {
			seq->flag |= SELECT;
			if(hand==1) seq->flag |= SEQ_LEFTSEL;
			if(hand==2) seq->flag |= SEQ_RIGHTSEL;
		}
		recurs_sel_seq(seq);
	}

	force_draw(0);

	if(get_last_seq()) allqueue(REDRAWIPO, 0);
	BIF_undo_push("Select Sequencer");

	std_rmouse_transform(transform_seq);
}


Sequence *alloc_sequence(ListBase *lb, int cfra, int machine)
{
	Sequence *seq;

	/*ed= G.scene->ed;*/

	seq= MEM_callocN( sizeof(Sequence), "addseq");
	BLI_addtail(lb, seq);

	set_last_seq(seq);

	*( (short *)seq->name )= ID_SEQ;
	seq->name[2]= 0;

	seq->flag= SELECT;
	seq->start= cfra;
	seq->machine= machine;
	seq->mul= 1.0;
	
	return seq;
}

static Sequence *sfile_to_sequence(SpaceFile *sfile, int cfra, int machine, int last)
{
	Sequence *seq;
	Strip *strip;
	StripElem *se;
	int totsel, a;
	char name[160], rel[160];

	/* are there selected files? */
	totsel= 0;
	for(a=0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			if( (sfile->filelist[a].type & S_IFDIR)==0 ) {
				totsel++;
			}
		}
	}

	if(last) {
		/* if not, a file handed to us? */
		if(totsel==0 && sfile->file[0]) totsel= 1;
	}

	if(totsel==0) return 0;

	/* make seq */
	seq= alloc_sequence(((Editing *)G.scene->ed)->seqbasep, cfra, machine);
	seq->len= totsel;

	if(totsel==1) {
		seq->startstill= 25;
		seq->endstill= 24;
	}

	calc_sequence(seq);
	
	if(sfile->flag & FILE_STRINGCODE) {
		strcpy(name, sfile->dir);
		strcpy(rel, G.sce);
		BLI_makestringcode(rel, name);
	} else {
		strcpy(name, sfile->dir);
	}

	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totsel;
	strip->us= 1;
	strncpy(strip->dir, name, FILE_MAXDIR-1);
	strip->stripdata= se= MEM_callocN(totsel*sizeof(StripElem), "stripelem");

	for(a=0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			if( (sfile->filelist[a].type & S_IFDIR)==0 ) {
				strncpy(se->name, sfile->filelist[a].relname, FILE_MAXFILE-1);
				se->ok= 1;
				se++;
			}
		}
	}
	/* no selected file: */
	if(totsel==1 && se==strip->stripdata) {
		strncpy(se->name, sfile->file, FILE_MAXFILE-1);
		se->ok= 1;
	}

	/* last active name */
	strncpy(last_imagename, seq->strip->dir, FILE_MAXDIR-1);

	return seq;
}


static int sfile_to_mv_sequence_load(SpaceFile *sfile, int cfra, 
				     int machine, int index )
{
	Sequence *seq;
	struct anim *anim;
	Strip *strip;
	StripElem *se;
	int totframe, a;
	char name[160], rel[160];
	char str[FILE_MAXDIR+FILE_MAXFILE];

	totframe= 0;

	strncpy(str, sfile->dir, FILE_MAXDIR-1);
	if(index<0)
		strncat(str, sfile->file, FILE_MAXDIR-1);
	else
		strncat(str, sfile->filelist[index].relname, FILE_MAXDIR-1);

	/* is it a movie? */
	anim = openanim(str, IB_rect);
	if(anim==0) {
		error("The selected file is not a movie or "
		      "FFMPEG-support not compiled in!");
		return(cfra);
	}
	
	totframe= IMB_anim_get_duration(anim);

	/* make seq */
	seq= alloc_sequence(((Editing *)G.scene->ed)->seqbasep, cfra, machine);
	seq->len= totframe;
	seq->type= SEQ_MOVIE;
	seq->anim= anim;
	seq->anim_preseek = IMB_anim_get_preseek(anim);

	calc_sequence(seq);
	
	if(sfile->flag & FILE_STRINGCODE) {
		strcpy(name, sfile->dir);
		strcpy(rel, G.sce);
		BLI_makestringcode(rel, name);
	} else {
		strcpy(name, sfile->dir);
	}

	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totframe;
	strip->us= 1;
	strncpy(strip->dir, name, FILE_MAXDIR-1);
	strip->stripdata= se= MEM_callocN(totframe*sizeof(StripElem), "stripelem");

	/* name movie in first strip */
	if(index<0)
		strncpy(se->name, sfile->file, FILE_MAXFILE-1);
	else
		strncpy(se->name, sfile->filelist[index].relname, FILE_MAXFILE-1);

	for(a=1; a<=totframe; a++, se++) {
		se->ok= 1;
		se->nr= a;
	}

	/* last active name */
	strncpy(last_imagename, seq->strip->dir, FILE_MAXDIR-1);
	return(cfra+totframe);
}

static void sfile_to_mv_sequence(SpaceFile *sfile, int cfra, int machine)
{
	int a, totsel;

	totsel= 0;
	for(a= 0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			if ((sfile->filelist[a].type & S_IFDIR)==0) {
				totsel++;
			}
		}
	}

	if((totsel==0) && (sfile->file[0])) {
		cfra= sfile_to_mv_sequence_load(sfile, cfra, machine, -1);
		return;
	}

	if(totsel==0) return;

	/* ok. check all the select file, and load it. */
	for(a= 0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			if ((sfile->filelist[a].type & S_IFDIR)==0) {
				/* load and update current frame. */
				cfra= sfile_to_mv_sequence_load(sfile, cfra, machine, a);
			}
		}
	}
}

static Sequence *sfile_to_ramsnd_sequence(SpaceFile *sfile, 
					  int cfra, int machine)
{
	Sequence *seq;
	bSound *sound;
	Strip *strip;
	StripElem *se;
	double totframe;
	int a;
	char name[160], rel[160];
	char str[256];

	totframe= 0.0;

	strncpy(str, sfile->dir, FILE_MAXDIR-1);
	strncat(str, sfile->file, FILE_MAXFILE-1);

	sound= sound_new_sound(str);
	if (!sound || sound->sample->type == SAMPLE_INVALID) {
		error("Unsupported audio format");
		return 0;
	}
	if (sound->sample->bits != 16) {
		error("Only 16 bit audio is supported");
		return 0;
	}
	sound->id.us=1;
	sound->flags |= SOUND_FLAGS_SEQUENCE;
	audio_makestream(sound);

	totframe= (int) ( ((float)(sound->streamlen-1)/( (float)G.scene->audio.mixrate*4.0 ))* (float)G.scene->r.frs_sec);

	/* make seq */
	seq= alloc_sequence(((Editing *)G.scene->ed)->seqbasep, cfra, machine);
	seq->len= totframe;
	seq->type= SEQ_RAM_SOUND;
	seq->sound = sound;

	calc_sequence(seq);
	
	if(sfile->flag & FILE_STRINGCODE) {
		strcpy(name, sfile->dir);
		strcpy(rel, G.sce);
		BLI_makestringcode(rel, name);
	} else {
		strcpy(name, sfile->dir);
	}

	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totframe;
	strip->us= 1;
	strncpy(strip->dir, name, FILE_MAXDIR-1);
	strip->stripdata= se= MEM_callocN(totframe*sizeof(StripElem), "stripelem");

	/* name sound in first strip */
	strncpy(se->name, sfile->file, FILE_MAXFILE-1);

	for(a=1; a<=totframe; a++, se++) {
		se->ok= 2; /* why? */
		se->ibuf= 0;
		se->nr= a;
	}

	/* last active name */
	strncpy(last_sounddir, seq->strip->dir, FILE_MAXDIR-1);

	return seq;
}

static int sfile_to_hdsnd_sequence_load(SpaceFile *sfile, int cfra, 
					int machine, int index)
{
	Sequence *seq;
	struct hdaudio *hdaudio;
	Strip *strip;
	StripElem *se;
	int totframe, a;
	char name[160], rel[160];
	char str[FILE_MAXDIR+FILE_MAXFILE];

	totframe= 0;

	strncpy(str, sfile->dir, FILE_MAXDIR-1);
	if(index<0)
		strncat(str, sfile->file, FILE_MAXDIR-1);
	else
		strncat(str, sfile->filelist[index].relname, FILE_MAXDIR-1);

	/* is it a sound file? */
	hdaudio = sound_open_hdaudio(str);
	if(hdaudio==0) {
		error("The selected file is not a sound file or "
		      "FFMPEG-support not compiled in!");
		return(cfra);
	}

	totframe= sound_hdaudio_get_duration(hdaudio, G.scene->r.frs_sec);

	/* make seq */
	seq= alloc_sequence(((Editing *)G.scene->ed)->seqbasep, cfra, machine);
	seq->len= totframe;
	seq->type= SEQ_HD_SOUND;
	seq->hdaudio= hdaudio;

	calc_sequence(seq);
	
	if(sfile->flag & FILE_STRINGCODE) {
		strcpy(name, sfile->dir);
		strcpy(rel, G.sce);
		BLI_makestringcode(rel, name);
	} else {
		strcpy(name, sfile->dir);
	}

	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totframe;
	strip->us= 1;
	strncpy(strip->dir, name, FILE_MAXDIR-1);
	strip->stripdata= se= MEM_callocN(totframe*sizeof(StripElem), "stripelem");

	/* name movie in first strip */
	if(index<0)
		strncpy(se->name, sfile->file, FILE_MAXFILE-1);
	else
		strncpy(se->name, sfile->filelist[index].relname, FILE_MAXFILE-1);

	for(a=1; a<=totframe; a++, se++) {
		se->ok= 2;
		se->ibuf = 0;
		se->nr= a;
	}

	/* last active name */
	strncpy(last_sounddir, seq->strip->dir, FILE_MAXDIR-1);
	return(cfra+totframe);
}

static void sfile_to_hdsnd_sequence(SpaceFile *sfile, int cfra, int machine)
{
	int totsel, a;

	totsel= 0;
	for(a= 0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			if((sfile->filelist[a].type & S_IFDIR)==0) {
				totsel++;
			}
		}
	}

	if((totsel==0) && (sfile->file[0])) {
		cfra= sfile_to_hdsnd_sequence_load(sfile, cfra, machine, -1);
		return;
	}

	if(totsel==0) return;

	/* ok, check all the select file, and load it. */
	for(a= 0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			if((sfile->filelist[a].type & S_IFDIR)==0) {
				/* load and update current frame. */
				cfra= sfile_to_hdsnd_sequence_load(sfile, cfra, machine, a);
			}
		}
	}
}


static void add_image_strips(char *name)
{
	SpaceFile *sfile;
	struct direntry *files;
	float x, y;
	int a, totfile, cfra, machine;
	short mval[2];

	deselect_all_seq();

	/* restore windowmatrices */
	areawinset(curarea->win);
	drawseqspace(curarea, curarea->spacedata.first);

	/* search sfile */
	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	/* where will it be */
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	waitcursor(1);

	/* also read contents of directories */
	files= sfile->filelist;
	totfile= sfile->totfile;
	sfile->filelist= 0;
	sfile->totfile= 0;

	for(a=0; a<totfile; a++) {
		if(files[a].flags & ACTIVE) {
			if( (files[a].type & S_IFDIR) ) {
				strncat(sfile->dir, files[a].relname, FILE_MAXFILE-1);
				strcat(sfile->dir,"/");
				read_dir(sfile);

				/* select all */
				swapselect_file(sfile);

				if ( sfile_to_sequence(sfile, cfra, machine, 0) ) machine++;

				parent(sfile);
			}
		}
	}

	sfile->filelist= files;
	sfile->totfile= totfile;

	/* read directory itself */
	sfile_to_sequence(sfile, cfra, machine, 1);

	waitcursor(0);

	BIF_undo_push("Add image strip Sequencer");
	transform_seq('g', 0);

}

static void add_movie_strip(char *name)
{
	SpaceFile *sfile;
	float x, y;
	int cfra, machine;
	short mval[2];

	deselect_all_seq();

	/* restore windowmatrices */
	areawinset(curarea->win);
	drawseqspace(curarea, curarea->spacedata.first);

	/* search sfile */
	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	/* where will it be */
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	waitcursor(1);

	/* read directory itself */
	sfile_to_mv_sequence(sfile, cfra, machine);

	waitcursor(0);

	BIF_undo_push("Add movie strip Sequencer");
	transform_seq('g', 0);

}

static void add_movie_and_hdaudio_strip(char *name)
{
	SpaceFile *sfile;
	float x, y;
	int cfra, machine;
	short mval[2];

	deselect_all_seq();

	/* restore windowmatrices */
	areawinset(curarea->win);
	drawseqspace(curarea, curarea->spacedata.first);

	/* search sfile */
	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	/* where will it be */
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	waitcursor(1);

	/* read directory itself */
	sfile_to_hdsnd_sequence(sfile, cfra, machine);
	sfile_to_mv_sequence(sfile, cfra, machine);

	waitcursor(0);

	BIF_undo_push("Add movie and HD-audio strip Sequencer");
	transform_seq('g', 0);

}

static void add_sound_strip_ram(char *name)
{
	SpaceFile *sfile;
	float x, y;
	int cfra, machine;
	short mval[2];

	deselect_all_seq();

	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if (sfile==0) return;

	/* where will it be */
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	waitcursor(1);

	sfile_to_ramsnd_sequence(sfile, cfra, machine);

	waitcursor(0);

	BIF_undo_push("Add ram sound strip Sequencer");
	transform_seq('g', 0);
}

static void add_sound_strip_hd(char *name)
{
	SpaceFile *sfile;
	float x, y;
	int cfra, machine;
	short mval[2];

	deselect_all_seq();

	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if (sfile==0) return;

	/* where will it be */
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	waitcursor(1);

	sfile_to_hdsnd_sequence(sfile, cfra, machine);

	waitcursor(0);

	BIF_undo_push("Add hd sound strip Sequencer");
	transform_seq('g', 0);
}

#if 0
static void reload_sound_strip(char *name)
{
	Editing *ed;
	Sequence *seq, *seqact;
	SpaceFile *sfile;
	Sequence *last_seq= get_last_seq();

	ed= G.scene->ed;

	if(last_seq==0 || last_seq->type!=SEQ_SOUND) return;
	seqact= last_seq;	/* last_seq changes in alloc_sequence */

	/* search sfile */
	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	waitcursor(1);

	seq = sfile_to_snd_sequence(sfile, seqact->start, seqact->machine);
	printf("seq->type: %i\n", seq->type);
	if(seq && seq!=seqact) {
		/* i'm not sure about this one, seems to work without it -- sgefant */
		free_strip(seqact->strip);

		seqact->strip= seq->strip;

		seqact->len= seq->len;
		calc_sequence(seqact);

		seq->strip= 0;
		free_sequence(seq);
		BLI_remlink(ed->seqbasep, seq);

		seq= ed->seqbasep->first;

	}

	waitcursor(0);

	allqueue(REDRAWSEQ, 0);
}
#endif

static void reload_image_strip(char *name)
{
	Editing *ed;
	Sequence *seq, *seqact;
	SpaceFile *sfile;
	Sequence *last_seq= get_last_seq();

	ed= G.scene->ed;

	if(last_seq==0 || last_seq->type!=SEQ_IMAGE) return;
	seqact= last_seq;	/* last_seq changes in alloc_sequence */

	/* search sfile */
	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	waitcursor(1);

	seq= sfile_to_sequence(sfile, seqact->start, seqact->machine, 1);
	if(seq && seq!=seqact) {
		free_strip(seqact->strip);

		seqact->strip= seq->strip;

		seqact->len= seq->len;
		calc_sequence(seqact);

		seq->strip= 0;
		free_sequence(seq);
		BLI_remlink(ed->seqbasep, seq);

		update_changed_seq_and_deps(seqact, 1, 1);
	}
	waitcursor(0);

	allqueue(REDRAWSEQ, 0);
}

static int event_to_efftype(int event)
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

static int seq_effect_find_selected(Editing *ed, Sequence *activeseq, int type, Sequence **selseq1, Sequence **selseq2, Sequence **selseq3)
{
	Sequence *seq1= 0, *seq2= 0, *seq3= 0, *seq;
	
	if (!activeseq)
		seq2= get_last_seq();

	for(seq=ed->seqbasep->first; seq; seq=seq->next) {
		if(seq->flag & SELECT) {
			if (seq->type == SEQ_RAM_SOUND
			    || seq->type == SEQ_HD_SOUND) { 
				error("Can't apply effects to "
				      "audio sequence strips");
				return 0;
			}
			if((seq != activeseq) && (seq != seq2)) {
                                if(seq2==0) seq2= seq;
                                else if(seq1==0) seq1= seq;
                                else if(seq3==0) seq3= seq;
                                else {
                                       error("Can't apply effect to more than 3 sequence strips");
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
		seq1 = seq2 = seq3 = 0;
		break;
	case 1:
		if(seq2==0)  {
			error("Need at least one selected sequence strip");
			return 0;
		}
		if(seq1==0) seq1= seq2;
		if(seq3==0) seq3= seq2;
	case 2:
		if(seq1==0 || seq2==0) {
			error("Need 2 selected sequence strips");
			return 0;
		}
		if(seq3==0) seq3= seq2;
	}

	*selseq1= seq1;
	*selseq2= seq2;
	*selseq3= seq3;

	return 1;
}

static int add_seq_effect(int type, char *str)
{
	Editing *ed;
	Sequence *newseq, *seq1, *seq2, *seq3;
	Strip *strip;
	float x, y;
	int cfra, machine;
	short mval[2];
	struct SeqEffectHandle sh;

	if(G.scene->ed==0) return 0;
	ed= G.scene->ed;

	if(!seq_effect_find_selected(ed, NULL, event_to_efftype(type), &seq1, &seq2, &seq3))
		return 0;

	deselect_all_seq();

	/* where will it be (cfra is not realy needed) */
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	/* allocate and initialize */
	newseq= alloc_sequence(((Editing *)G.scene->ed)->seqbasep, cfra, machine);
	newseq->type= event_to_efftype(type);

	sh = get_sequence_effect(newseq);

	newseq->seq1= seq1;
	newseq->seq2= seq2;
	newseq->seq3= seq3;

	sh.init(newseq);

	if (!seq1) {
		newseq->len= 1;
		newseq->startstill= 25;
		newseq->endstill= 24;
	}

	calc_sequence(newseq);

	newseq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= newseq->len;
	strip->us= 1;
	if(newseq->len>0)
		strip->stripdata= MEM_callocN(newseq->len*sizeof(StripElem), "stripelem");

	/* initialize plugin */
	if(newseq->type == SEQ_PLUGIN) {
		sh.init_plugin(newseq, str);

		if(newseq->plugin==0) {
			BLI_remlink(ed->seqbasep, newseq);
			free_sequence(newseq);
			set_last_seq(NULL);
			return 0;
		}
	}

	/* set find a free spot to but the strip */
	if (newseq->seq1) {
		newseq->machine= MAX3(newseq->seq1->machine, 
				      newseq->seq2->machine,
				      newseq->seq3->machine);
	}
	if(test_overlap_seq(newseq)) shuffle_seq(newseq);

	update_changed_seq_and_deps(newseq, 1, 1);

	/* push undo and go into grab mode */
	if(newseq->type == SEQ_PLUGIN) {
		BIF_undo_push("Add plugin strip Sequencer");
	} else {
		BIF_undo_push("Add effect strip Sequencer");
	}

	transform_seq('g', 0);

	return 1;
}

static void load_plugin_seq(char *str)		/* called from fileselect */
{
	add_seq_effect(10, str);
}

void add_sequence(int type)
{
	Editing *ed;
	Sequence *seq;
	Strip *strip;
	Scene *sce;
	float x, y;
	int cfra, machine;
	short nr, event, mval[2];
	char *str;

	if (type >= 0){
		/* bypass pupmenu for calls from menus (aphex) */
		switch(type){
		case SEQ_SCENE:
			event = 101;
			break;
		case SEQ_IMAGE:
			event = 1;
			break;
		case SEQ_MOVIE:
			event = 102;
			break;
		case SEQ_RAM_SOUND:
			event = 103;
			break;
		case SEQ_HD_SOUND:
			event = 104;
			break;
		case SEQ_MOVIE_AND_HD_SOUND:
			event = 105;
			break;
		case SEQ_PLUGIN:
			event = 10;
			break;
		case SEQ_CROSS:
			event = 2;
			break;
		case SEQ_ADD:
			event = 4;
			break;
		case SEQ_SUB:
			event = 5;
			break;
		case SEQ_ALPHAOVER:
			event = 7;
			break;
		case SEQ_ALPHAUNDER:
			event = 8;
			break;
		case SEQ_GAMCROSS:
			event = 3;
			break;
		case SEQ_MUL:
			event = 6;
			break;
		case SEQ_OVERDROP:
			event = 9;
			break;
		case SEQ_WIPE:
			event = 13;
			break;
		case SEQ_GLOW:
			event = 14;
			break;
		case SEQ_TRANSFORM:
			event = 15;
			break;
		case SEQ_COLOR:
			event = 16;
			break;
		case SEQ_SPEED:
			event = 17;
			break;
		default:
			event = 0;
			break;
		}
	}
	else {
		event= pupmenu("Add Sequence Strip%t"
			       "|Images%x1"
			       "|Movie%x102"
#ifdef WITH_FFMPEG
				   "|Movie + Audio (HD)%x105"
			       "|Audio (RAM)%x103"
			       "|Audio (HD)%x104"
#else
				   "|Audio (Wav)%x103"
#endif
			       "|Scene%x101"
			       "|Plugin%x10"
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
			       "|Transforms%x15"
			       "|Color Generator%x16"
			       "|Speed Control%x17");
	}

	if(event<1) return;

	if(G.scene->ed==0) {
		ed= G.scene->ed= MEM_callocN( sizeof(Editing), "addseq");
		ed->seqbasep= &ed->seqbase;
	}
	else ed= G.scene->ed;

	switch(event) {
	case 1:

		activate_fileselect(FILE_SPECIAL, "Select Images", last_imagename, add_image_strips);
		break;
	case 105:
		activate_fileselect(FILE_SPECIAL, "Select Movie+Audio", last_imagename, add_movie_and_hdaudio_strip);
		break;
	case 102:

		activate_fileselect(FILE_SPECIAL, "Select Movie", last_imagename, add_movie_strip);
		break;
	case 101:
		/* new menu: */
		IDnames_to_pupstring(&str, NULL, NULL, &G.main->scene, (ID *)G.scene, NULL);

		event= pupmenu_col(str, 20);

		if(event> -1) {
			nr= 1;
			sce= G.main->scene.first;
			while(sce) {
				if( event==nr) break;
				nr++;
				sce= sce->id.next;
			}
			if(sce) {

				deselect_all_seq();

				/* where ? */
				getmouseco_areawin(mval);
				areamouseco_to_ipoco(G.v2d, mval, &x, &y);
				cfra= (int)(x+0.5);
				machine= (int)(y+0.5);

				seq= alloc_sequence(((Editing *)G.scene->ed)->seqbasep, cfra, machine);
				seq->type= SEQ_SCENE;
				seq->scene= sce;
				seq->sfra= sce->r.sfra;
				seq->len= sce->r.efra - sce->r.sfra + 1;

				seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
				strncpy(seq->name + 2, sce->id.name + 2, 
					sizeof(seq->name) - 2);
				strip->len= seq->len;
				strip->us= 1;
				if(seq->len>0) strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");

				BIF_undo_push("Add scene strip Sequencer");
				transform_seq('g', 0);
			}
		}
		MEM_freeN(str);

		break;
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
		if(get_last_seq()==0 && 
		   get_sequence_effect_num_inputs( event_to_efftype(event))> 0)
			error("Need at least one active sequence strip");
		else if(event==10)
			activate_fileselect(FILE_SPECIAL, "Select Plugin", U.plugseqdir, load_plugin_seq);
		else
			add_seq_effect(event, NULL);

		break;
	case 103:
		if (!last_sounddir[0]) strncpy(last_sounddir, U.sounddir, FILE_MAXDIR-1);
		activate_fileselect(FILE_SPECIAL, "Select Audio (RAM)", last_sounddir, add_sound_strip_ram);
		break;
	case 104:
		if (!last_sounddir[0]) strncpy(last_sounddir, U.sounddir, FILE_MAXDIR-1);
		activate_fileselect(FILE_SPECIAL, "Select Audio (HD)", last_sounddir, add_sound_strip_hd);
		break;
	}
}

void change_sequence(void)
{
	Sequence *last_seq= get_last_seq();
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

			update_changed_seq_and_deps(last_seq, 0, 1);
			allqueue(REDRAWSEQ, 0);
			BIF_undo_push("Change effect Sequencer");
		}
	}
	else if(last_seq->type == SEQ_IMAGE) {
		if(okee("Change images")) {
			activate_fileselect(FILE_SPECIAL, 
					    "Select Images", 
					    last_imagename, 
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
			update_changed_seq_and_deps(last_seq, 1, 1);

			allqueue(REDRAWSEQ, 0);
		}
	}

}

void reassign_inputs_seq_effect()
{
	Editing *ed= G.scene->ed;
	Sequence *seq1, *seq2, *seq3, *last_seq = get_last_seq();

	if(last_seq==0 || !(last_seq->type & SEQ_EFFECT)) return;
	if(ed==0) return;

	if(!seq_effect_find_selected(ed, last_seq, last_seq->type, &seq1, &seq2, &seq3))
		return;

	/* see reassigning would create a cycle */
	if(seq_is_predecessor(seq1, last_seq) || seq_is_predecessor(seq2, last_seq) ||
	   seq_is_predecessor(seq3, last_seq)) {
		error("Can't reassign inputs: no cycles allowed");
	   	return;
	}
	
	last_seq->seq1 = seq1;
	last_seq->seq2 = seq2;
	last_seq->seq3 = seq3;

	update_changed_seq_and_deps(last_seq, 1, 1);

	allqueue(REDRAWSEQ, 0);
}

static Sequence *del_seq_find_replace_recurs(Sequence *seq)
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
		seq1= del_seq_find_replace_recurs(seq->seq1);
		seq2= del_seq_find_replace_recurs(seq->seq2);
		seq3= del_seq_find_replace_recurs(seq->seq3);

		if(seq1==seq->seq1 && seq2==seq->seq2 && seq3==seq->seq3);
		else if(seq1 || seq2 || seq3) {
			seq->seq1= (seq1)? seq1: (seq2)? seq2: seq3;
			seq->seq2= (seq2)? seq2: (seq1)? seq1: seq3;
			seq->seq3= (seq3)? seq3: (seq1)? seq1: seq2;

			update_changed_seq_and_deps(seq, 1, 1);
		}
		else
			seq->flag |= SELECT; /* mark for delete */
	}

	if (seq->flag & SELECT) {
		if((seq1 = del_seq_find_replace_recurs(seq->seq1))) return seq1;
		if((seq2 = del_seq_find_replace_recurs(seq->seq2))) return seq2;
		if((seq3 = del_seq_find_replace_recurs(seq->seq3))) return seq3;
		else return NULL;
	}
	else
		return seq;
}

static void recurs_del_seq_flag(ListBase *lb, short flag, short deleteall)
{
	Sequence *seq, *seqn;
	Sequence *last_seq = get_last_seq();

	seq= lb->first;
	while(seq) {
		seqn= seq->next;
		if((seq->flag & flag) || deleteall) {
			if(seq->type==SEQ_RAM_SOUND && seq->sound) 
				seq->sound->id.us--;

			BLI_remlink(lb, seq);
			if(seq==last_seq) set_last_seq(0);
			if(seq->type==SEQ_META) recurs_del_seq_flag(&seq->seqbase, flag, 1);
			if(seq->ipo) seq->ipo->id.us--;
			free_sequence(seq);
		}
		seq= seqn;
	}
}

void del_seq(void)
{
	Sequence *seq;
	MetaStack *ms;
	Editing *ed;

	if(okee("Erase selected")==0) return;

	ed= G.scene->ed;
	if(ed==0) return;

	/* free imbufs of all dependent strips */
	for(seq=ed->seqbasep->first; seq; seq=seq->next)
		if(seq->flag & SELECT)
			update_changed_seq_and_deps(seq, 1, 0);

	/* for effects, try to find a replacement input */
	for(seq=ed->seqbasep->first; seq; seq=seq->next)
		if((seq->type & SEQ_EFFECT) && !(seq->flag & SELECT))
			del_seq_find_replace_recurs(seq);

	/* delete all selected strips */
	recurs_del_seq_flag(ed->seqbasep, SELECT, 0);

	/* updates lengths etc */
	seq= ed->seqbasep->first;
	while(seq) {
		calc_sequence(seq);
		seq= seq->next;
	}

	/* free parent metas */
	ms= ed->metastack.last;
	while(ms) {
		ms->parseq->strip->len= 0;		/* force new alloc */
		calc_sequence(ms->parseq);
		ms= ms->prev;
	}

	BIF_undo_push("Delete from Sequencer");
	allqueue(REDRAWSEQ, 0);
}

static void recurs_dupli_seq(ListBase *old, ListBase *new)
{
	Sequence *seq, *seqn;
	Sequence *last_seq = get_last_seq();
	StripElem *se;
	int a;

	seq= old->first;

	while(seq) {
		seq->newseq= 0;
		if(seq->flag & SELECT) {

			if(seq->type==SEQ_META) {
				seqn= MEM_dupallocN(seq);
				seq->newseq= seqn;
				BLI_addtail(new, seqn);

				seqn->strip= MEM_dupallocN(seq->strip);

				if(seq->len>0) seq->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");

				seq->flag &= SEQ_DESEL;
				seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL);

				seqn->seqbase.first= seqn->seqbase.last= 0;
				recurs_dupli_seq(&seq->seqbase,&seqn->seqbase);

			}
			else if(seq->type == SEQ_SCENE) {
				seqn= MEM_dupallocN(seq);
				seq->newseq= seqn;
				BLI_addtail(new, seqn);

				seqn->strip= MEM_dupallocN(seq->strip);

				if(seq->len>0) seqn->strip->stripdata = MEM_callocN(seq->len*sizeof(StripElem), "stripelem");

				seq->flag &= SEQ_DESEL;
				seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL);
			}
			else if(seq->type == SEQ_MOVIE) {
				seqn= MEM_dupallocN(seq);
				seq->newseq= seqn;
				BLI_addtail(new, seqn);

				seqn->strip= MEM_dupallocN(seq->strip);
				seqn->anim= 0;

				if(seqn->len>0) {
					seqn->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
					/* copy first elem */
					*seqn->strip->stripdata= *seq->strip->stripdata;
					se= seqn->strip->stripdata;
					a= seq->len;
					while(a--) {
						se->ok= 1;
						se++;
					}
				}

				seq->flag &= SEQ_DESEL;
				seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL);
			}
			else if(seq->type == SEQ_RAM_SOUND) {
				seqn= MEM_dupallocN(seq);
				seq->newseq= seqn;
				BLI_addtail(new, seqn);

				seqn->strip= MEM_dupallocN(seq->strip);
				seqn->anim= 0;
				seqn->sound->id.us++;
				if(seqn->ipo) seqn->ipo->id.us++;

				if(seqn->len>0) {
					seqn->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
					/* copy first elem */
					*seqn->strip->stripdata= *seq->strip->stripdata;
					se= seqn->strip->stripdata;
					a= seq->len;
					while(a--) {
						se->ok= 1;
						se++;
					}
				}

				seq->flag &= SEQ_DESEL;
				seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL);
			}
			else if(seq->type == SEQ_HD_SOUND) {
				seqn= MEM_dupallocN(seq);
				seq->newseq= seqn;
				BLI_addtail(new, seqn);

				seqn->strip= MEM_dupallocN(seq->strip);
				seqn->anim= 0;
				seqn->hdaudio = 0;
				if(seqn->ipo) seqn->ipo->id.us++;

				if(seqn->len>0) {
					seqn->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
					/* copy first elem */
					*seqn->strip->stripdata= *seq->strip->stripdata;
					se= seqn->strip->stripdata;
					a= seq->len;
					while(a--) {
						se->ok= 1;
						se++;
					}
				}

				seq->flag &= SEQ_DESEL;
				seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL);
			}
			else if(seq->type < SEQ_EFFECT) {
				seqn= MEM_dupallocN(seq);
				seq->newseq= seqn;
				BLI_addtail(new, seqn);

				seqn->strip->us++;
				seq->flag &= SEQ_DESEL;

				seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL);
			}
			else {
				seqn= MEM_dupallocN(seq);
				seq->newseq= seqn;
				BLI_addtail(new, seqn);

				if(seq->seq1 && seq->seq1->newseq) seqn->seq1= seq->seq1->newseq;
				if(seq->seq2 && seq->seq2->newseq) seqn->seq2= seq->seq2->newseq;
				if(seq->seq3 && seq->seq3->newseq) seqn->seq3= seq->seq3->newseq;

				if(seqn->ipo) seqn->ipo->id.us++;

				if (seq->type & SEQ_EFFECT) {
					struct SeqEffectHandle sh;
					sh = get_sequence_effect(seq);
					if(sh.copy)
						sh.copy(seq, seqn);
				}

				seqn->strip= MEM_dupallocN(seq->strip);

				if(seq->len>0) seq->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");

				seq->flag &= SEQ_DESEL;
				
				seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL);
			}
			if (seq == last_seq) {
				set_last_seq(seqn);
			}
		}
		seq= seq->next;
	}
}

void add_duplicate_seq(void)
{
	Editing *ed;
	ListBase new;

	ed= G.scene->ed;
	if(ed==0) return;

	new.first= new.last= 0;

	recurs_dupli_seq(ed->seqbasep, &new);
	addlisttolist(ed->seqbasep, &new);

	BIF_undo_push("Add duplicate Sequencer");
	transform_seq('g', 0);
}

int insert_gap(int gap, int cfra)
{
	Sequence *seq;
	Editing *ed;
	int done=0;

	/* all strips >= cfra are shifted */
	ed= G.scene->ed;
	if(ed==0) return 0;

	WHILE_SEQ(ed->seqbasep) {
		if(seq->startdisp >= cfra) {
			seq->start+= gap;
			calc_sequence(seq);
			done= 1;
		}
	}
	END_SEQ

	return done;
}

void touch_seq_files(void)
{
	Sequence *seq;
	Editing *ed;
	char str[256];

	/* touch all strips with movies */
	ed= G.scene->ed;
	if(ed==0) return;

	if(okee("Touch and print selected movies")==0) return;

	waitcursor(1);

	WHILE_SEQ(ed->seqbasep) {
		if(seq->flag & SELECT) {
			if(seq->type==SEQ_MOVIE) {
				if(seq->strip && seq->strip->stripdata) {
					BLI_make_file_string(G.sce, str, seq->strip->dir, seq->strip->stripdata->name);
					BLI_touch(seq->name);
				}
			}

		}
	}
	END_SEQ

	waitcursor(0);
}

void set_filter_seq(void)
{
	Sequence *seq;
	Editing *ed;

	ed= G.scene->ed;
	if(ed==0) return;

	if(okee("Set FilterY")==0) return;

	WHILE_SEQ(ed->seqbasep) {
		if(seq->flag & SELECT) {
			if(seq->type==SEQ_MOVIE) {
				seq->flag |= SEQ_FILTERY;
			}

		}
	}
	END_SEQ

}



void no_gaps(void)
{
	Editing *ed;
	int cfra, first= 0, done;

	ed= G.scene->ed;
	if(ed==0) return;

	for(cfra= CFRA; cfra<=EFRA; cfra++) {
		if(first==0) {
			if( evaluate_seq_frame(cfra) ) first= 1;
		}
		else {
			done= 1;
			while( evaluate_seq_frame(cfra) == 0) {
				done= insert_gap(-1, cfra);
				if(done==0) break;
			}
			if(done==0) break;
		}
	}

	BIF_undo_push("No gaps Sequencer");
	allqueue(REDRAWSEQ, 0);
}


/* ****************** META ************************* */

void make_meta(void)
{
	Sequence *seq, *seqm, *next;
	Editing *ed;
	int tot;
	
	ed= G.scene->ed;
	if(ed==0) return;

	/* is there more than 1 select */
	tot= 0;
	seq= ed->seqbasep->first;
	while(seq) {
		if(seq->flag & SELECT) {
			tot++;
			if (seq->type == SEQ_RAM_SOUND) { 
				error("Can't make Meta Strip from audio"); 
				return; 
			}
		}
		seq= seq->next;
	}
	if(tot < 2) return;

	if(okee("Make Meta Strip")==0) return;

	/* test relationships */
	seq= ed->seqbasep->first;
	while(seq) {
		if(seq->flag & SELECT) {
			if(seq->type & SEQ_EFFECT) {
				if(seq->seq1 && 
				   (seq->seq1->flag & SELECT)==0) tot= 0;
				if(seq->seq2 &&
				   (seq->seq2->flag & SELECT)==0) tot= 0;
				if(seq->seq3 &&
				   (seq->seq3->flag & SELECT)==0) tot= 0;
			}
		}
		else if(seq->type & SEQ_EFFECT) {
			if(seq->seq1 &&
			   (seq->seq1->flag & SELECT)) tot= 0;
			if(seq->seq2 &&
			   (seq->seq2->flag & SELECT)) tot= 0;
			if(seq->seq3 &&
			   (seq->seq3->flag & SELECT)) tot= 0;
		}
		if(tot==0) break;
		seq= seq->next;
	}
	if(tot==0) {
		error("Please select all related strips");
		return;
	}

	/* remove all selected from main list, and put in meta */

	seqm= alloc_sequence(((Editing *)G.scene->ed)->seqbasep, 1, 1);
	seqm->type= SEQ_META;
	seqm->flag= SELECT;

	seq= ed->seqbasep->first;
	while(seq) {
		next= seq->next;
		if(seq!=seqm && (seq->flag & SELECT)) {
			BLI_remlink(ed->seqbasep, seq);
			BLI_addtail(&seqm->seqbase, seq);
		}
		seq= next;
	}
	calc_sequence(seqm);

	seqm->strip= MEM_callocN(sizeof(Strip), "metastrip");
	seqm->strip->len= seqm->len;
	seqm->strip->us= 1;
	if(seqm->len) seqm->strip->stripdata= MEM_callocN(seqm->len*sizeof(StripElem), "metastripdata");
	set_meta_stripdata(seqm);

	BIF_undo_push("Make Meta Sequencer");
	allqueue(REDRAWSEQ, 0);
}

static int seq_depends_on_meta(Sequence *seq, Sequence *seqm)
{
	if (seq == seqm) return 1;
	else if (seq->seq1 && seq_depends_on_meta(seq->seq1, seqm)) return 1;
	else if (seq->seq2 && seq_depends_on_meta(seq->seq2, seqm)) return 1;
	else if (seq->seq3 && seq_depends_on_meta(seq->seq3, seqm)) return 1;
	else return 0;
}

void un_meta(void)
{
	Editing *ed;
	Sequence *seq, *last_seq = get_last_seq();

	ed= G.scene->ed;
	if(ed==0) return;

	if(last_seq==0 || last_seq->type!=SEQ_META) return;

	if(okee("Un Meta")==0) return;

	addlisttolist(ed->seqbasep, &last_seq->seqbase);

	last_seq->seqbase.first= 0;
	last_seq->seqbase.last= 0;

	BLI_remlink(ed->seqbasep, last_seq);
	free_sequence(last_seq);

	/* emtpy meta strip, delete all effects depending on it */
	for(seq=ed->seqbasep->first; seq; seq=seq->next)
		if((seq->type & SEQ_EFFECT) && seq_depends_on_meta(seq, last_seq))
			seq->flag |= SEQ_FLAG_DELETE;

	recurs_del_seq_flag(ed->seqbasep, SEQ_FLAG_DELETE, 0);

	/* test for effects and overlap */
	WHILE_SEQ(ed->seqbasep) {
		if(seq->flag & SELECT) {
			seq->flag &= ~SEQ_OVERLAP;
			if( test_overlap_seq(seq) ) {
				shuffle_seq(seq);
			}
		}
	}
	END_SEQ;

	sort_seq();

	BIF_undo_push("Un-make Meta Sequencer");
	allqueue(REDRAWSEQ, 0);

}

void exit_meta(void)
{
	Sequence *seq;
	MetaStack *ms;
	Editing *ed;

	ed= G.scene->ed;
	if(ed==0) return;

	if(ed->metastack.first==0) return;

	ms= ed->metastack.last;
	BLI_remlink(&ed->metastack, ms);

	ed->seqbasep= ms->oldbasep;

	/* recalc entire meta */
	set_meta_stripdata(ms->parseq);

	/* recalc all: the meta can have effects connected to it */
	seq= ed->seqbasep->first;
	while(seq) {
		calc_sequence(seq);
		seq= seq->next;
	}

	set_last_seq(ms->parseq);

	ms->parseq->flag= SELECT;
	recurs_sel_seq(ms->parseq);

	MEM_freeN(ms);
	allqueue(REDRAWSEQ, 0);

	BIF_undo_push("Exit meta strip Sequence");
}


void enter_meta(void)
{
	MetaStack *ms;
	Editing *ed;
	Sequence *last_seq= get_last_seq();

	ed= G.scene->ed;
	if(ed==0) return;

	if(last_seq==0 || last_seq->type!=SEQ_META || last_seq->flag==0) {
		exit_meta();
		return;
	}

	ms= MEM_mallocN(sizeof(MetaStack), "metastack");
	BLI_addtail(&ed->metastack, ms);
	ms->parseq= last_seq;
	ms->oldbasep= ed->seqbasep;

	ed->seqbasep= &last_seq->seqbase;

	set_last_seq(NULL);
	allqueue(REDRAWSEQ, 0);
	BIF_undo_push("Enter meta strip Sequence");
}


/* ****************** END META ************************* */


typedef struct TransSeq {
	int start, machine;
	int startstill, endstill;
	int startdisp, enddisp;
	int startofs, endofs;
	int len;
} TransSeq;

void transform_seq(int mode, int context)
{
	Sequence *seq;
	Editing *ed;
	float dx, dy, dvec[2], div;
	TransSeq *transmain, *ts;
	int tot=0, ix, iy, firsttime=1, afbreek=0, midtog= 0, proj= 0;
	unsigned short event = 0;
	short mval[2], val, xo, yo, xn, yn;
	char str[32];

	if(mode!='g') return;	/* from gesture */

	/* which seqs are involved */
	ed= G.scene->ed;
	if(ed==0) return;

	WHILE_SEQ(ed->seqbasep) {
		if(seq->flag & SELECT) tot++;
	}
	END_SEQ

	if(tot==0) return;

	G.moving= 1;

	ts=transmain= MEM_callocN(tot*sizeof(TransSeq), "transseq");

	WHILE_SEQ(ed->seqbasep) {

		if(seq->flag & SELECT) {

			ts->start= seq->start;
			ts->machine= seq->machine;
			ts->startstill= seq->startstill;
			ts->endstill= seq->endstill;
			ts->startofs= seq->startofs;
			ts->endofs= seq->endofs;

			ts++;
		}
	}
	END_SEQ

	getmouseco_areawin(mval);
	xo=xn= mval[0];
	yo=yn= mval[1];
	dvec[0]= dvec[1]= 0.0;

	while(afbreek==0) {
		getmouseco_areawin(mval);
		if(mval[0]!=xo || mval[1]!=yo || firsttime) {
			firsttime= 0;

			if(mode=='g') {

				dx= mval[0]- xo;
				dy= mval[1]- yo;

				div= G.v2d->mask.xmax-G.v2d->mask.xmin;
				dx= (G.v2d->cur.xmax-G.v2d->cur.xmin)*(dx)/div;

				div= G.v2d->mask.ymax-G.v2d->mask.ymin;
				dy= (G.v2d->cur.ymax-G.v2d->cur.ymin)*(dy)/div;

				if(G.qual & LR_SHIFTKEY) {
					if(dx>1.0) dx= 1.0; else if(dx<-1.0) dx= -1.0;
				}

				dvec[0]+= dx;
				dvec[1]+= dy;

				if(midtog) dvec[proj]= 0.0;
				ix= floor(dvec[0]+0.5);
				iy= floor(dvec[1]+0.5);


				ts= transmain;

				WHILE_SEQ(ed->seqbasep) {
					if(seq->flag & SELECT) {
						if(seq->flag & SEQ_LEFTSEL) {
							if(ts->startstill) {
								seq->startstill= ts->startstill-ix;
								if(seq->startstill<0) seq->startstill= 0;
							}
							else if(ts->startofs) {
								seq->startofs= ts->startofs+ix;
								if(seq->startofs<0) seq->startofs= 0;
							}
							else {
								if(ix>0) {
									seq->startofs= ix;
									seq->startstill= 0;
								}
								else if (seq->type != SEQ_RAM_SOUND && seq->type != SEQ_HD_SOUND) {
									seq->startstill= -ix;
									seq->startofs= 0;
								}
							}
							if(seq->len <= seq->startofs+seq->endofs) {
								seq->startofs= seq->len-seq->endofs-1;
							}
						}
						if(seq->flag & SEQ_RIGHTSEL) {
							if(ts->endstill) {
								seq->endstill= ts->endstill+ix;
								if(seq->endstill<0) seq->endstill= 0;
							}
							else if(ts->endofs) {
								seq->endofs= ts->endofs-ix;
								if(seq->endofs<0) seq->endofs= 0;
							}
							else {
								if(ix<0) {
									seq->endofs= -ix;
									seq->endstill= 0;
								}
								else if (seq->type != SEQ_RAM_SOUND && seq->type != SEQ_HD_SOUND) {
									seq->endstill= ix;
									seq->endofs= 0;
								}
							}
							if(seq->len <= seq->startofs+seq->endofs) {
								seq->endofs= seq->len-seq->startofs-1;
							}
						}
						if( (seq->flag & (SEQ_LEFTSEL+SEQ_RIGHTSEL))==0 ) {
							if(sequence_is_free_transformable(seq)) seq->start= ts->start+ ix;

							if(seq->depth==0) seq->machine= ts->machine+ iy;

							if(seq->machine<1) seq->machine= 1;
							else if(seq->machine>= MAXSEQ) seq->machine= MAXSEQ;
						}

						calc_sequence(seq);

						ts++;
					}
				}
				END_SEQ

				sprintf(str, "X: %d   Y: %d  ", ix, iy);
				headerprint(str);
			}

			xo= mval[0];
			yo= mval[1];

			/* test for effect and overlap */

			WHILE_SEQ(ed->seqbasep) {
				if(seq->flag & SELECT) {
					seq->flag &= ~SEQ_OVERLAP;
					if( test_overlap_seq(seq) ) {
						seq->flag |= SEQ_OVERLAP;
					}
				}
				else if(seq->type & SEQ_EFFECT) {
					if(seq->seq1 && seq->seq1->flag & SELECT) calc_sequence(seq);
					else if(seq->seq2 && seq->seq2->flag & SELECT) calc_sequence(seq);
					else if(seq->seq3 && seq->seq3->flag & SELECT) calc_sequence(seq);
				}
			}
			END_SEQ;

			force_draw(0);
		}
		else BIF_wait_for_statechange();

		while(qtest()) {
			event= extern_qread(&val);
			if(val) {
				switch(event) {
				case ESCKEY:
				case LEFTMOUSE:
				case RIGHTMOUSE:
				case SPACEKEY:
				case RETKEY:
					afbreek= 1;
					break;
				case MIDDLEMOUSE:
					midtog= ~midtog;
					if(midtog) {
						if( abs(mval[0]-xn) > abs(mval[1]-yn)) proj= 1;
						else proj= 0;
						firsttime= 1;
					}
					break;
				default:
					arrows_move_cursor(event);
				}
			}
			if(afbreek) break;
		}
	}

	if((event==ESCKEY) || (event==RIGHTMOUSE)) {

		ts= transmain;
		WHILE_SEQ(ed->seqbasep) {
			if(seq->flag & SELECT) {
				seq->start= ts->start;
				seq->machine= ts->machine;
				seq->startstill= ts->startstill;
				seq->endstill= ts->endstill;
				seq->startofs= ts->startofs;
				seq->endofs= ts->endofs;

				calc_sequence(seq);
				seq->flag &= ~SEQ_OVERLAP;

				ts++;
			} else if(seq->type & SEQ_EFFECT) {
				if(seq->seq1 && seq->seq1->flag & SELECT) calc_sequence(seq);
				else if(seq->seq2 && seq->seq2->flag & SELECT) calc_sequence(seq);
				else if(seq->seq3 && seq->seq3->flag & SELECT) calc_sequence(seq);
			}

		}
		END_SEQ
	}
	else {

		/* images, effects and overlap */
		WHILE_SEQ(ed->seqbasep) {
			if(seq->type == SEQ_META) {
				calc_sequence(seq);
				seq->flag &= ~SEQ_OVERLAP;
				if( test_overlap_seq(seq) ) shuffle_seq(seq);
			}
			else if(seq->flag & SELECT) {
				calc_sequence(seq);
				seq->flag &= ~SEQ_OVERLAP;
				if( test_overlap_seq(seq) ) shuffle_seq(seq);
			}
			else if(seq->type & SEQ_EFFECT) calc_sequence(seq);
		}
		END_SEQ

		/* as last: */
		sort_seq();
	}

	G.moving= 0;
	MEM_freeN(transmain);

	BIF_undo_push("Transform Sequencer");
	allqueue(REDRAWSEQ, 0);
}

void seq_cut(int cutframe)
{
	Editing *ed;
	Sequence *seq;
	TransSeq *ts, *transmain;
	int tot=0;
	ListBase newlist;
	
	ed= G.scene->ed;
	if(ed==0) return;
	
	/* test for validity */
	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if(seq->flag & SELECT) {
			if(cutframe > seq->startdisp && cutframe < seq->enddisp)
				if(seq->type==SEQ_META) break;
		}
	}
	if(seq) {
		error("Cannot cut Meta strips");
		return;
	}
	
	/* we build an array of TransSeq, to denote which strips take part in cutting */
	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if(seq->flag & SELECT) {
			if(cutframe > seq->startdisp && cutframe < seq->enddisp)
				tot++;
			else
				seq->flag &= ~SELECT;	// bad code, but we need it for recurs_dupli_seq... note that this ~SELECT assumption is used in loops below too (ton)
		}
	}
	
	if(tot==0) {
		error("No strips to cut");
		return;
	}
	
	ts=transmain= MEM_callocN(tot*sizeof(TransSeq), "transseq");
	
	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if(seq->flag & SELECT) {
			
			ts->start= seq->start;
			ts->machine= seq->machine;
			ts->startstill= seq->startstill;
			ts->endstill= seq->endstill;
			ts->startdisp= seq->startdisp;
			ts->enddisp= seq->enddisp;
			ts->startofs= seq->startofs;
			ts->endofs= seq->endofs;
			ts->len= seq->len;
			
			ts++;
		}
	}
		
	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if(seq->flag & SELECT) {
			
			/* strips with extended stillframes before */
			if ((seq->startstill) && (cutframe <seq->start)) {
				seq->start= cutframe -1;
				seq->startstill= cutframe -seq->startdisp -1;
				seq->len= 1;
				seq->endstill= 0;
			}
			
			/* normal strip */
			else if ((cutframe >=seq->start)&&(cutframe <=(seq->start+seq->len))) {
				seq->endofs = (seq->start+seq->len) - cutframe;
			}
			
			/* strips with extended stillframes after */
			else if (((seq->start+seq->len) < cutframe) && (seq->endstill)) {
				seq->endstill -= seq->enddisp - cutframe;
			}
			
			calc_sequence(seq);
		}
	}
		
	newlist.first= newlist.last= NULL;
	
	/* now we duplicate the cut strip and move it into place afterwards */
	recurs_dupli_seq(ed->seqbasep, &newlist);
	addlisttolist(ed->seqbasep, &newlist);
	
	ts= transmain;
	
	/* go through all the strips and correct them based on their stored values */
	for(seq= ed->seqbasep->first; seq; seq= seq->next) {
		if(seq->flag & SELECT) {

			/* strips with extended stillframes before */
			if ((seq->startstill) && (cutframe == seq->start + 1)) {
				seq->start = ts->start;
				seq->startstill= ts->start- cutframe;
				seq->len = ts->len;
				seq->endstill = ts->endstill;
			}
			
			/* normal strip */
			else if ((cutframe>=seq->start)&&(cutframe<=(seq->start+seq->len))) {
				seq->startstill = 0;
				seq->startofs = cutframe - ts->start;
				seq->endofs = ts->endofs;
				seq->endstill = ts->endstill;
			}				
			
			/* strips with extended stillframes after */
			else if (((seq->start+seq->len) < cutframe) && (seq->endstill)) {
				seq->start = cutframe - ts->len +1;
				seq->startofs = ts->len-1;
				seq->endstill = ts->enddisp - cutframe -1;
				seq->startstill = 0;
			}
			calc_sequence(seq);
			
			ts++;
		}
	}
		
	/* as last: */	
	sort_seq();
	MEM_freeN(transmain);
	
	allqueue(REDRAWSEQ, 0);
}

void seq_snap_menu(void)
{
	short event;

	event= pupmenu("Snap %t|To Current Frame%x1");
	if(event < 1) return;

	seq_snap(event);
}

void seq_snap(short event)
{
	Editing *ed;
	Sequence *seq;

	ed= G.scene->ed;
	if(ed==0) return;

	/* problem: contents of meta's are all shifted to the same position... */

	/* also check metas */
	WHILE_SEQ(ed->seqbasep) {
		if(seq->flag & SELECT) {
			if(sequence_is_free_transformable(seq)) seq->start= CFRA-seq->startofs+seq->startstill;
			calc_sequence(seq);
		}
	}
	END_SEQ


	/* test for effects and overlap */
	WHILE_SEQ(ed->seqbasep) {
		if(seq->flag & SELECT) {
			seq->flag &= ~SEQ_OVERLAP;
			if( test_overlap_seq(seq) ) {
				shuffle_seq(seq);
			}
		}
		else if(seq->type & SEQ_EFFECT) {
			if(seq->seq1 && (seq->seq1->flag & SELECT)) 
				calc_sequence(seq);
			else if(seq->seq2 && (seq->seq2->flag & SELECT)) 
				calc_sequence(seq);
			else if(seq->seq3 && (seq->seq3->flag & SELECT)) 
				calc_sequence(seq);
		}
	}
	END_SEQ;

	/* as last: */
	sort_seq();

	BIF_undo_push("Snap menu Sequencer");
	allqueue(REDRAWSEQ, 0);
}

void borderselect_seq(void)
{
	Sequence *seq;
	Editing *ed;
	rcti rect;
	rctf rectf, rq;
	int val;
	short mval[2];

	ed= G.scene->ed;
	if(ed==0) return;

	val= get_border(&rect, 3);

	if(val) {
		mval[0]= rect.xmin;
		mval[1]= rect.ymin;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

		seq= ed->seqbasep->first;
		while(seq) {

			if(seq->startstill) rq.xmin= seq->start;
			else rq.xmin= seq->startdisp;
			rq.ymin= seq->machine+0.2;
			if(seq->endstill) rq.xmax= seq->start+seq->len;
			else rq.xmax= seq->enddisp;
			rq.ymax= seq->machine+0.8;

			if(BLI_isect_rctf(&rq, &rectf, 0)) {
				if(val==LEFTMOUSE) {
					seq->flag |= SELECT;
				}
				else {
					seq->flag &= ~SELECT;
				}
				recurs_sel_seq(seq);
			}

			seq= seq->next;
		}

		BIF_undo_push("Border select Sequencer");
		addqueue(curarea->win, REDRAW, 1);
	}
}
