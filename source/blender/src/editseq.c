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
#include "BLI_winstuff.h"
#endif   
#include <sys/types.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_storage_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_ipo_types.h"
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
#include "BSE_filesel.h"
#include "BSE_drawipo.h"
#include "BSE_seqaudio.h"

#include "BDR_editobject.h"

#include "blendef.h"
#include "mydevice.h"

Sequence *last_seq=0;
char last_imagename[80]= "/";
char last_sounddir[80]= ""; 

/*  void transform_seq(int mode); already in BIF_editseq.h */

#define SEQ_DESEL	~(SELECT+SEQ_LEFTSEL+SEQ_RIGHTSEL)

static int test_overlap_seq(Sequence *);
static void shuffle_seq(Sequence *);

static void change_plugin_seq(char *str)	/* called from fileselect */
{
/*  	extern Sequence *last_seq; already done few lines before !!!*/
	
	if(last_seq && last_seq->type!=SEQ_PLUGIN) return;
	
	free_plugin_seq(last_seq->plugin);
	
	last_seq->plugin= (PluginSeq *)add_plugin_seq(str, last_seq->name+2);
	
	last_seq->machine= MAX3(last_seq->seq1->machine, last_seq->seq2->machine, last_seq->seq3->machine);
	if( test_overlap_seq(last_seq) ) shuffle_seq(last_seq);
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

Sequence *find_nearest_seq(int *hand)
{
	Sequence *seq;
	Editing *ed;
	float x, y, facx, facy;
	short mval[2];
	
	*hand= 0;
	
	ed= G.scene->ed;
	if(ed==0) return 0;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	
	seq= ed->seqbasep->first;
	while(seq) {
		if(seq->machine == (int)y) {
			if(seq->startdisp<=x && seq->enddisp>=x) {
				
				if(seq->type < SEQ_EFFECT) {
					if( seq->handsize+seq->startdisp >=x ) {
						/* within triangle? */
						facx= (x-seq->startdisp)/seq->handsize;
						if( (y - (int)y) <0.5) {
							facy= (y - 0.2 - (int)y)/0.3;
							if( facx < facy ) *hand= 1;
						}
						else {
							facy= (y - 0.5 - (int)y)/0.3;
							if( facx+facy < 1.0 ) *hand= 1;
						}
						
					}
					else if( -seq->handsize+seq->enddisp <=x ) {
						/* within triangle? */
						facx= 1.0 - (seq->enddisp-x)/seq->handsize;
						if( (y - (int)y) <0.5) {
							facy= (y - 0.2 - (int)y)/0.3;
							if( facx+facy > 1.0 ) *hand= 2;
						}
						else {
							facy= (y - 0.5 - (int)y)/0.3;
							if( facx > facy ) *hand= 2;
						}
					}
				}
				
				return seq;
			}
		}
		seq= seq->next;
	}
	return 0;
}

void clear_last_seq(void)
{
	/* from (example) ipo: when it is changed, also do effects with same ipo */
	Sequence *seq;
	Editing *ed;
	StripElem *se;
	int a;
	
	if(last_seq) {
		
		ed= G.scene->ed;
		if(ed==0) return;
		
		WHILE_SEQ(&ed->seqbase) {
			if(seq==last_seq || (last_seq->ipo && seq->ipo==last_seq->ipo)) {
				a= seq->len;
				se= seq->strip->stripdata;
				if(se) {
					while(a--) {
						if(se->ibuf) IMB_freeImBuf(se->ibuf);
						se->ibuf= 0;
						se->ok= 1;
						se++;
					}
				}
			}
		}
		END_SEQ
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
			error("No space to add sequence ");
			
			BLI_remlink(ed->seqbasep, test);
			free_sequence(test);
			return;
		}
		test->machine++;
		calc_sequence(test);
	}
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
}

void mouse_select_seq(void)
{
	Sequence *seq;
	int hand;
	
	seq= find_nearest_seq(&hand);
	
	if(G.qual==0) deselect_all_seq();
	
	if(seq) {
		last_seq= seq;
		
		if ((seq->type == SEQ_IMAGE) || (seq->type == SEQ_MOVIE)) {
			if(seq->strip) {
				strcpy(last_imagename, seq->strip->dir);
			}
		} else
		if (seq->type == SEQ_SOUND) {
			if(seq->strip) {
				strcpy(last_sounddir, seq->strip->dir);
			}
		}
	
		if(G.qual==0) {
			seq->flag |= SELECT;
			if(hand==1) seq->flag |= SEQ_LEFTSEL;
			if(hand==2) seq->flag |= SEQ_RIGHTSEL;
		}
		else {
			if(seq->flag & SELECT) {
				if(hand==0) seq->flag &= SEQ_DESEL;
				else if(hand==1) {
					if(seq->flag & SEQ_LEFTSEL) seq->flag &= ~SEQ_LEFTSEL;
					else seq->flag |= SEQ_LEFTSEL;
				}
				else if(hand==2) {
					if(seq->flag & SEQ_RIGHTSEL) seq->flag &= ~SEQ_RIGHTSEL;
					else seq->flag |= SEQ_RIGHTSEL;
				}
			}
			else {
				seq->flag |= SELECT;
				if(hand==1) seq->flag |= SEQ_LEFTSEL;
				if(hand==2) seq->flag |= SEQ_RIGHTSEL;
			}
		}
		recurs_sel_seq(seq);
	}
	
	force_draw();
	
	if(last_seq) allqueue(REDRAWIPO, 0);
	
	std_rmouse_transform(transform_seq);
}

static Sequence *alloc_sequence(int cfra, int machine)
{
	Editing *ed;
	Sequence *seq;
	
	ed= G.scene->ed;
	
	seq= MEM_callocN( sizeof(Sequence), "addseq");
	BLI_addtail(ed->seqbasep, seq);
	
	last_seq= seq;
	
	*( (short *)seq->name )= ID_SEQ;
	seq->name[2]= 0;
	
	seq->flag= SELECT;
	seq->start= cfra;
	seq->machine= machine;
	
	return seq;
}

static Sequence *sfile_to_sequence(SpaceFile *sfile, int cfra, int machine, int last)
{
	Sequence *seq;
	Strip *strip;
	StripElem *se;
	int totsel, a;

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
	seq= alloc_sequence(cfra, machine);
	seq->len= totsel;
	
	if(totsel==1) {
		seq->startstill= 25;
		seq->endstill= 24;
	}

	calc_sequence(seq);
	
	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totsel;
	strip->us= 1;
	strcpy(strip->dir, sfile->dir);
	strip->stripdata= se= MEM_callocN(totsel*sizeof(StripElem), "stripelem");

	for(a=0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			if( (sfile->filelist[a].type & S_IFDIR)==0 ) {
				strcpy(se->name, sfile->filelist[a].relname);
				se->ok= 1;
				se++;
			}
		}
	}
	/* no selected file: */
	if(totsel==1 && se==strip->stripdata) {
		strcpy(se->name, sfile->file);
		se->ok= 1;
	}

	/* last active name */
	strcpy(last_imagename, seq->strip->dir);

	return seq;
}

static void sfile_to_mv_sequence(SpaceFile *sfile, int cfra, int machine)
{
	Sequence *seq;
	struct anim *anim;
	Strip *strip;
	StripElem *se;
	int totframe, a;
	char str[256];
	
	totframe= 0;

	strcpy(str, sfile->dir);
	strcat(str, sfile->file);
	
	/* is it a movie? */
	anim = openanim(str, IB_rect);
	if(anim==0) {
		error("Not a movie");
		return;
	}
	
	totframe= IMB_anim_get_duration(anim);
	
	/* make seq */
	seq= alloc_sequence(cfra, machine);
	seq->len= totframe;
	seq->type= SEQ_MOVIE;
	seq->anim= anim;

	calc_sequence(seq);
	
	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totframe;
	strip->us= 1;
	strcpy(strip->dir, sfile->dir);
	strip->stripdata= se= MEM_callocN(totframe*sizeof(StripElem), "stripelem");

	/* name movie in first strip */
	strcpy(se->name, sfile->file);

	for(a=1; a<=totframe; a++, se++) {
		se->ok= 1;
		se->nr= a;
	}

	/* last active name */
	strcpy(last_imagename, seq->strip->dir);
}

static Sequence *sfile_to_snd_sequence(SpaceFile *sfile, int cfra, int machine)
{
	Sequence *seq;
	bSound *sound;
	Strip *strip;
	StripElem *se;
	double totframe;
	int a;
	char str[256];

	totframe= 0.0;

	strcpy(str, sfile->dir);
	strcat(str, sfile->file);
	
	sound= sound_new_sound(str);
	if (!sound || sound->sample->type == SAMPLE_INVALID) {
		error("Unsupported audio format");
		return 0;
	}
	if (sound->sample->bits != 16) {
		error("Only 16 bit audio supported");
		return 0;
	}
	sound->id.us=1;
	sound->flags |= SOUND_FLAGS_SEQUENCE;
	audio_makestream(sound);
	
	totframe= (int) ( ((float)(sound->streamlen-1)/( (float)G.scene->audio.mixrate*4.0 ))* (float)G.scene->r.frs_sec);

	/* make seq */
	seq= alloc_sequence(cfra, machine);
	seq->len= totframe;
	seq->type= SEQ_SOUND;
	seq->sound = sound;

	calc_sequence(seq);
	
	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totframe;
	strip->us= 1;
	strcpy(strip->dir, sfile->dir);
	strip->stripdata= se= MEM_callocN(totframe*sizeof(StripElem), "stripelem");

	/* name sound in first strip */
	strcpy(se->name, sfile->file);

	for(a=1; a<=totframe; a++, se++) {
		se->ok= 2; /* why? */
		se->ibuf= 0;
		se->nr= a;
	}

	/* last active name */
	strcpy(last_sounddir, seq->strip->dir);
	
	return seq;
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
				strcat(sfile->dir, files[a].relname);
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
	
	transform_seq('g');

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
	
	transform_seq('g');

} 

static void add_sound_strip(char *name)
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

	sfile_to_snd_sequence(sfile, cfra, machine);

	waitcursor(0);

	transform_seq('g');
}

#if 0
static void reload_sound_strip(char *name)
{
	Editing *ed;
	Sequence *seq, *seqact;
	SpaceFile *sfile;

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

		seq= ed->seqbasep->first;
		while(seq) {
			if(seq->type & SEQ_EFFECT) {
				/* new_stripdata is clear */
				if(seq->seq1==seqact || seq->seq2==seqact || seq->seq3==seqact) {
					calc_sequence(seq);
					new_stripdata(seq);
				}
			}
			seq= seq->next;
		}
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
	return 0;
}

static int add_seq_effect(int type)
{
	Editing *ed;
	Sequence *seq, *seq1, *seq2, *seq3;
	Strip *strip;
	float x, y;
	int cfra, machine;
	short mval[2];	

	if(G.scene->ed==0) return 0;
	ed= G.scene->ed;

	/* apart from last_seq there have to be 2 selected sequences */
	seq1= seq3= 0;
	seq2= last_seq;		/* last_seq changes with alloc_seq! */
	seq= ed->seqbasep->first;
	while(seq) {
		if(seq->flag & SELECT) {
			if (seq->type == SEQ_SOUND) { error("Cannot apply effects to audio sequence"); return 0; }		
			if(seq != seq2) {
				if(seq1==0) seq1= seq;
				else if(seq3==0) seq3= seq;
				else {
					seq1= 0;
					break;
				}
			}
		}
		seq= seq->next;
	}
	
	if(type==10) {	/* plugin: minimal 1 select */
		if(seq2==0)  {
			error("Need minimum one active sequence");
			return 0;
		}
		if(seq1==0) seq1= seq2;
		if(seq3==0) seq3= seq2;
	}
	else {
		if(seq1==0 || seq2==0) {
			error("Need 2 selected sequences");
			return 0;
		}
		if(seq3==0) seq3= seq2;
	}
	
	deselect_all_seq();

	/* where will it be (cfra is not realy needed) */
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	seq= alloc_sequence(cfra, machine);
	
	seq->type= event_to_efftype(type);
	
	if(seq->type==SEQ_ALPHAUNDER || seq->type==SEQ_ALPHAOVER) {
		seq->seq2= seq1;
		seq->seq1= seq2;
	}
	else {
		seq->seq1= seq1;
		seq->seq2= seq2;
	}
	seq->seq3= seq3;
	calc_sequence(seq);
	
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= seq->len;
	strip->us= 1;
	if(seq->len>0) strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
		
	return 1;
}

static void load_plugin_seq(char *str)		/* called from fileselect */
{
	Editing *ed;
	
	add_seq_effect(10);		/* this sets last_seq */
	
	free_plugin_seq(last_seq->plugin);
	
	last_seq->plugin= (PluginSeq *)add_plugin_seq(str, last_seq->name+2);
	
	if(last_seq->plugin==0) {
		ed= G.scene->ed;
		BLI_remlink(ed->seqbasep, last_seq);
		free_sequence(last_seq);
		last_seq= 0;
	}
	else {
		last_seq->machine= MAX3(last_seq->seq1->machine, last_seq->seq2->machine, last_seq->seq3->machine);
		if( test_overlap_seq(last_seq) ) shuffle_seq(last_seq);
	
		transform_seq('g');
	}
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
		case SEQ_SOUND:
			event = 103;
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
		default:
			event = 0;
			break;
		}
	}
	else {
		event= pupmenu("Add sequence%t|Images%x1|Movie%x102|Audio%x103|Scene%x101|Plugin%x10|Cross%x2|GammaCross%x3|Add%x4|Sub%x5|Mul%x6|AlphaOver%x7|AlphaUnder%x8|AlphaOverDrop%x9");
	}

	if(event<1) return;
	
	if(G.scene->ed==0) {
		ed= G.scene->ed= MEM_callocN( sizeof(Editing), "addseq");
		ed->seqbasep= &ed->seqbase;
	}
	else ed= G.scene->ed;
	
	switch(event) {
	case 1:
		
		activate_fileselect(FILE_SPECIAL, "SELECT IMAGES", last_imagename, add_image_strips);
		break;
	case 102:
		
		activate_fileselect(FILE_SPECIAL, "SELECT MOVIE", last_imagename, add_movie_strip);
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
		
				seq= alloc_sequence(cfra, machine);
				seq->type= SEQ_SCENE;
				seq->scene= sce;
				seq->sfra= sce->r.sfra;
				seq->len= sce->r.efra - sce->r.sfra + 1;
				
				seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
				strip->len= seq->len;
				strip->us= 1;
				if(seq->len>0) strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
				
				transform_seq('g');
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
		
		if(last_seq==0) error("Need minimum one active sequence");
		else if(event==10) {
			activate_fileselect(FILE_SPECIAL, "SELECT PLUGIN", U.plugseqdir, load_plugin_seq);
		}
		else {
			if( add_seq_effect(event) ) transform_seq('g');
		}
		
		break;
	case 103:
		if (!last_sounddir[0]) strcpy(last_sounddir, U.sounddir);
		activate_fileselect(FILE_SPECIAL, "SELECT WAV", last_sounddir, add_sound_strip);
		break;
	}	
}

void change_sequence(void)
{
	Scene *sce;
	short event;
	
	if(last_seq==0) return;

	if(last_seq->type & SEQ_EFFECT) {
		event= pupmenu("Change effect%t|Switch a-b %x1|Switch b-c %x10|Plugin%x11|Recalculate%x12|Cross%x2|GammaCross%x3|Add%x4|Sub%x5|Mul%x6|AlphaOver%x7|AlphaUnder%x8|AlphaOverdrop%x9");
		if(event>0) {
			if(event==1) {
				SWAP(Sequence *, last_seq->seq1, last_seq->seq2);
			}
			else if(event==10) {
				SWAP(Sequence *, last_seq->seq2, last_seq->seq3);
			}
			else if(event==11) {
				activate_fileselect(FILE_SPECIAL, "SELECT PLUGIN", U.plugseqdir, change_plugin_seq);				
			}
			else if(event==12);	/* recalculate: only new_stripdata */
			else {
				/* to be sure, free plugin */
				free_plugin_seq(last_seq->plugin);
				last_seq->plugin= 0;
				last_seq->type= event_to_efftype(event);
			}
			new_stripdata(last_seq);
			allqueue(REDRAWSEQ, 0);
		}
	}
	else if(last_seq->type == SEQ_IMAGE) {
		if(okee("Change images")) {
			activate_fileselect(FILE_SPECIAL, "SELECT IMAGES", last_imagename, reload_image_strip);
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
			new_stripdata(last_seq);
			calc_sequence(last_seq);
			
			allqueue(REDRAWSEQ, 0);
		}
	}

}

static int is_a_sequence(Sequence *test)
{
	Sequence *seq;
	Editing *ed;
	
	ed= G.scene->ed;
	if(ed==0 || test==0) return 0;
	
	seq= ed->seqbasep->first;
	while(seq) {
		if(seq==test) return 1;
		seq= seq->next;
	}
	
	return 0;
}

static void recurs_del_seq(ListBase *lb)
{
	Sequence *seq, *seqn;

	seq= lb->first;
	while(seq) {
		seqn= seq->next;
		if(seq->flag & SELECT) {
			if(seq->type==SEQ_SOUND && seq->sound) seq->sound->id.us--;		
			BLI_remlink(lb, seq);
			if(seq==last_seq) last_seq= 0;
			if(seq->type==SEQ_META) recurs_del_seq(&seq->seqbase);
			if(seq->ipo) seq->ipo->id.us--;
			free_sequence(seq);
		}
		seq= seqn;
	}
}

void del_seq(void)
{
	Sequence *seq, *seqn;
	MetaStack *ms;
	Editing *ed;
	int doit;
	
	if(okee("Erase selected")==0) return;

	ed= G.scene->ed;
	if(ed==0) return;
	
	recurs_del_seq(ed->seqbasep);
	
	/* test effects */
	doit= 1;
	while(doit) {
		doit= 0;
		seq= ed->seqbasep->first;
		while(seq) {
			seqn= seq->next;
			if(seq->type & SEQ_EFFECT) {
				if( is_a_sequence(seq->seq1)==0 || is_a_sequence(seq->seq2)==0 || is_a_sequence(seq->seq3)==0 ) {
					BLI_remlink(ed->seqbasep, seq);
					if(seq==last_seq) last_seq= 0;
					free_sequence(seq);
					doit= 1;
				}
			}
			seq= seqn;
		}
	}
	
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
	
	allqueue(REDRAWSEQ, 0);
}



static void recurs_dupli_seq(ListBase *old, ListBase *new)
{
	Sequence *seq, *seqn;
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
				recurs_dupli_seq(&seq->seqbase, &seqn->seqbase);
				
			}
			else if(seq->type == SEQ_SCENE) {
				seqn= MEM_dupallocN(seq);
				seq->newseq= seqn;
				BLI_addtail(new, seqn);
				
				seqn->strip= MEM_dupallocN(seq->strip);
				
				if(seq->len>0) seqn->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
				
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
			else if(seq->type == SEQ_SOUND) {
				seqn= MEM_dupallocN(seq);
				seq->newseq= seqn;
				BLI_addtail(new, seqn);
				
				seqn->strip= MEM_dupallocN(seq->strip);
				seqn->anim= 0;
				seqn->sound->id.us++;
				
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
				if(seq->seq1->newseq) {
				
					seqn= MEM_dupallocN(seq);
					seq->newseq= seqn;
					BLI_addtail(new, seqn);
					
					seqn->seq1= seq->seq1->newseq;
					if(seq->seq2 && seq->seq2->newseq) seqn->seq2= seq->seq2->newseq;
					if(seq->seq3 && seq->seq3->newseq) seqn->seq3= seq->seq3->newseq;
					
					if(seqn->ipo) seqn->ipo->id.us++;
					
					if(seq->plugin) {
						seqn->plugin= MEM_dupallocN(seq->plugin);
						open_plugin_seq(seqn->plugin, seqn->name+2);
					}
					seqn->strip= MEM_dupallocN(seq->strip);
					
					if(seq->len>0) seq->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
					
					seq->flag &= SEQ_DESEL;
					
					seqn->flag &= ~(SEQ_LEFTSEL+SEQ_RIGHTSEL);
				}
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
	
	transform_seq('g');
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
	
	if(okee("Touch & print selected Movies")==0) return;
	
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
			if (seq->type == SEQ_SOUND) { error("Cannot make Meta from audio"); return; }
		}
		seq= seq->next;
	}
	if(tot < 2) return;
	
	if(okee("Make Meta")==0) return;
	
	/* test relationships */
	seq= ed->seqbasep->first;
	while(seq) {
		if(seq->flag & SELECT) {
			if(seq->type & SEQ_EFFECT) {
				if((seq->seq1->flag & SELECT)==0) tot= 0;
				if((seq->seq2->flag & SELECT)==0) tot= 0;
				if((seq->seq3->flag & SELECT)==0) tot= 0;
			}
		}
		else if(seq->type & SEQ_EFFECT) {
			if(seq->seq1->flag & SELECT) tot= 0;
			if(seq->seq2->flag & SELECT) tot= 0;
			if(seq->seq3->flag & SELECT) tot= 0;
		}
		if(tot==0) break;
		seq= seq->next;
	}
	if(tot==0) {
		error("Select all related strips");
		return;
	}

	/* remove all selected from main list, and put in meta */

	seqm= alloc_sequence(1, 1);
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
	
	allqueue(REDRAWSEQ, 0);
}

void un_meta(void)
{
	Editing *ed;
	Sequence *seq, *seqn;
	int doit;
	
	ed= G.scene->ed;
	if(ed==0) return;
	
	if(last_seq==0 || last_seq->type!=SEQ_META) return;
	
	if(okee("Un Meta")==0) return;

	addlisttolist(ed->seqbasep, &last_seq->seqbase);
	
	last_seq->seqbase.first= 0;
	last_seq->seqbase.last= 0;
	
	BLI_remlink(ed->seqbasep, last_seq);
	free_sequence(last_seq);

	/* test effects */
	doit= 1;
	while(doit) {
		doit= 0;
		seq= ed->seqbasep->first;
		while(seq) {
			seqn= seq->next;
			if(seq->type & SEQ_EFFECT) {
				if( is_a_sequence(seq->seq1)==0 || is_a_sequence(seq->seq2)==0 || is_a_sequence(seq->seq3)==0 ) {
					BLI_remlink(ed->seqbasep, seq);
					if(seq==last_seq) last_seq= 0;
					free_sequence(seq);
					doit= 1;
				}
			}
			seq= seqn;
		}
	}


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

	last_seq= ms->parseq;
	
	last_seq->flag= SELECT;
	recurs_sel_seq(last_seq);
	
	MEM_freeN(ms);
	allqueue(REDRAWSEQ, 0);
}


void enter_meta(void)
{
	MetaStack *ms;
	Editing *ed;
	
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
	
	last_seq= 0;
	allqueue(REDRAWSEQ, 0);
}


/* ****************** END META ************************* */


typedef struct TransSeq {
	int start, machine;
	int startstill, endstill;
	int startofs, endofs;
} TransSeq;

void transform_seq(int mode)
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
								else if (seq->type != SEQ_SOUND) {
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
								else if (seq->type != SEQ_SOUND) {
									seq->endstill= ix;
									seq->endofs= 0;
								}
							}
							if(seq->len <= seq->startofs+seq->endofs) {
								seq->endofs= seq->len-seq->startofs-1;
							}
						}
						if( (seq->flag & (SEQ_LEFTSEL+SEQ_RIGHTSEL))==0 ) {
							if(seq->type<SEQ_EFFECT) seq->start= ts->start+ ix;
							
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
					if(seq->seq1->flag & SELECT) calc_sequence(seq);
					else if(seq->seq2->flag & SELECT) calc_sequence(seq);
					else if(seq->seq3->flag & SELECT) calc_sequence(seq);
				}
			}
			END_SEQ;
			
			force_draw();
		}
		else BIF_wait_for_statechange();
		
		while(qtest()) {
			event= extern_qread(&val);
			if(val) {
				switch(event) {
				case ESCKEY:
				case LEFTMOUSE:
				case SPACEKEY:
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
	
	if(event==ESCKEY) {
		
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
				if(seq->seq1->flag & SELECT) calc_sequence(seq);
				else if(seq->seq2->flag & SELECT) calc_sequence(seq);
				else if(seq->seq3->flag & SELECT) calc_sequence(seq);
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
	
	allqueue(REDRAWSEQ, 0);
}


void clever_numbuts_seq(void)
{
	PluginSeq *pis;
	StripElem *se;
	VarStruct *varstr;
	int a;
	
	if(last_seq==0) return;
	if(last_seq->type==SEQ_PLUGIN) {
		pis= last_seq->plugin;
		if(pis->vars==0) return;
		
		varstr= pis->varstr;
		if(varstr) {
			for(a=0; a<pis->vars; a++, varstr++) {
				add_numbut(a, varstr->type, varstr->name, varstr->min, varstr->max, &(pis->data[a]), varstr->tip);
			}
			
			if( do_clever_numbuts(pis->pname, pis->vars, REDRAW) ) {
				new_stripdata(last_seq);
				free_imbuf_effect_spec(CFRA);
				allqueue(REDRAWSEQ, 0);
			}
		}
	}
	else if(last_seq->type==SEQ_MOVIE) {

		if(last_seq->mul==0.0) last_seq->mul= 1.0;

		add_numbut(0, TEX, "Name:", 0.0, 21.0, last_seq->name+2, 0);
		add_numbut(1, TOG|SHO|BIT|4, "FilterY", 0.0, 1.0, &last_seq->flag, 0);
		/* warning: only a single bit-button possible: we work at copied data! */
		add_numbut(2, NUM|FLO, "Mul", 0.01, 5.0, &last_seq->mul, 0);

		if( do_clever_numbuts("Movie", 3, REDRAW) ) {
			se= last_seq->curelem;

			if(se && se->ibuf ) {
				IMB_freeImBuf(se->ibuf);
				se->ibuf= 0;
			}
			allqueue(REDRAWSEQ, 0);
		}
	}
	else if(last_seq->type==SEQ_SOUND) {

		add_numbut(0, TEX, "Name:", 0.0, 21.0, last_seq->name+2, 0);
		add_numbut(1, NUM|FLO, "Gain (dB):", -96.0, 6.0, &last_seq->level, 0);
		add_numbut(2, NUM|FLO, "Pan:", -1.0, 1.0, &last_seq->pan, 0);
		add_numbut(3, TOG|SHO|BIT|5, "Mute", 0.0, 1.0, &last_seq->flag, 0);

		if( do_clever_numbuts("Audio", 4, REDRAW) ) {
			se= last_seq->curelem;
			allqueue(REDRAWSEQ, 0);
		}
	}		
	else if(last_seq->type==SEQ_META) {

		add_numbut(0, TEX, "Name:", 0.0, 21.0, last_seq->name+2, 0);

		if( do_clever_numbuts("Meta", 1, REDRAW) ) {
			allqueue(REDRAWSEQ, 0);
		}
	}
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
	short event;
	
	ed= G.scene->ed;
	if(ed==0) return;

	/* problem: contents of meta's are all shifted to the same position... */

	/* also check metas */
	WHILE_SEQ(ed->seqbasep) {
		if(seq->flag & SELECT) {
			if(seq->type<SEQ_EFFECT) seq->start= CFRA-seq->startofs+seq->startstill;
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
			if(seq->seq1->flag & SELECT) calc_sequence(seq);
			else if(seq->seq2->flag & SELECT) calc_sequence(seq);
			else if(seq->seq3->flag & SELECT) calc_sequence(seq);
		}
	}
	END_SEQ;

	/* as last: */
	sort_seq();
		
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
			}
			
			seq= seq->next;
		}

		addqueue(curarea->win, REDRAW, 1);
	}
}
