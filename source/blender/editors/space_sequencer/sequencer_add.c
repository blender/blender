/**
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
 * Contributor(s): Blender Foundation, 2003-2009, Campbell Barton
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

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_plugin_types.h"
#include "BKE_sequence.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

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
#include "ED_util.h"
#include "ED_fileselect.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

/* own include */
#include "sequencer_intern.h"

static void BIF_undo_push() {}
static void error() {}
static void waitcursor() {}
static void activate_fileselect() {}


static int pupmenu() {return 0;}
static int pupmenu_col() {return 0;}




static void transform_seq_nomarker() {}
	
	
	
	
	

static Sequence *sfile_to_sequence(Scene *scene, SpaceFile *sfile, int cfra, int machine, int last)
{
#if 0
	/* XXX sfile recoded... */
	Sequence *seq;
	Strip *strip;
	StripElem *se;
	int totsel, a;
	char name[160];
	Editing *ed= scene->ed;
	
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
	seq= alloc_sequence(((Editing *)scene->ed)->seqbasep, cfra, machine);
	seq->len= totsel;

	if(totsel==1) {
		seq->startstill= 25;
		seq->endstill= 24;
	}

	calc_sequence(seq);
	
	if(sfile->flag & FILE_STRINGCODE) {
		strcpy(name, sfile->dir);
		BLI_makestringcode(G.sce, name);
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
				se++;
			}
		}
	}
	/* no selected file: */
	if(totsel==1 && se==strip->stripdata) {
		strncpy(se->name, sfile->file, FILE_MAXFILE-1);
	}

	/* last active name */
	strncpy(ed->act_imagedir, seq->strip->dir, FILE_MAXDIR-1);

	return seq;
#endif
	return NULL;
}


#if 0
static int sfile_to_mv_sequence_load(Scene *scene, SpaceFile *sfile, int cfra, 
				     int machine, int index )
{
	/* XXX sfile recoded... */
	Sequence *seq;
	struct anim *anim;
	Strip *strip;
	StripElem *se;
	int totframe;
	char name[160];
	char str[FILE_MAXDIR+FILE_MAXFILE];
	Editing *ed= scene->ed;
	
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
	seq= alloc_sequence(((Editing *)scene->ed)->seqbasep, cfra, machine);
	seq->len= totframe;
	seq->type= SEQ_MOVIE;
	seq->anim= anim;
	seq->anim_preseek = IMB_anim_get_preseek(anim);

	calc_sequence(seq);
	
	if(sfile->flag & FILE_STRINGCODE) {
		strcpy(name, sfile->dir);
		BLI_makestringcode(G.sce, name);
	} else {
		strcpy(name, sfile->dir);
	}

	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totframe;
	strip->us= 1;
	strncpy(strip->dir, name, FILE_MAXDIR-1);
	strip->stripdata= se= MEM_callocN(sizeof(StripElem), "stripelem");

	/* name movie in first strip */
	if(index<0)
		strncpy(se->name, sfile->file, FILE_MAXFILE-1);
	else
		strncpy(se->name, sfile->filelist[index].relname, FILE_MAXFILE-1);

	/* last active name */
	strncpy(ed->act_imagedir, seq->strip->dir, FILE_MAXDIR-1);
	return(cfra+totframe);
}
#endif

static void sfile_to_mv_sequence(SpaceFile *sfile, int cfra, int machine)
{
#if 0
	/* XXX sfile recoded... */
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
#endif
}

static Sequence *sfile_to_ramsnd_sequence(Scene *scene, SpaceFile *sfile,  int cfra, int machine)
{
#if 0
	/* XXX sfile recoded... */
	Sequence *seq;
	bSound *sound;
	Strip *strip;
	StripElem *se;
	double totframe;
	char name[160];
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

	totframe= (int) ( ((float)(sound->streamlen-1)/
			   ( (float)scene->r.audio.mixrate*4.0 ))* FPS);

	/* make seq */
	seq= alloc_sequence(((Editing *)scene->ed)->seqbasep, cfra, machine);
	seq->len= totframe;
	seq->type= SEQ_RAM_SOUND;
	seq->sound = sound;

	calc_sequence(seq);
	
	if(sfile->flag & FILE_STRINGCODE) {
		strcpy(name, sfile->dir);
		BLI_makestringcode(G.sce, name);
	} else {
		strcpy(name, sfile->dir);
	}

	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totframe;
	strip->us= 1;
	strncpy(strip->dir, name, FILE_MAXDIR-1);
	strip->stripdata= se= MEM_callocN(sizeof(StripElem), "stripelem");

	/* name sound in first strip */
	strncpy(se->name, sfile->file, FILE_MAXFILE-1);

	/* last active name */
	strncpy(ed->act_sounddir, seq->strip->dir, FILE_MAXDIR-1);

	return seq;
#endif
	return NULL;
}

#if 0
static int sfile_to_hdsnd_sequence_load(SpaceFile *sfile, int cfra, 
					int machine, int index)
{
	/* XXX sfile recoded... */
	Sequence *seq;
	struct hdaudio *hdaudio;
	Strip *strip;
	StripElem *se;
	int totframe;
	char name[160];
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

	totframe= sound_hdaudio_get_duration(hdaudio, FPS);

	/* make seq */
	seq= alloc_sequence(((Editing *)scene->ed)->seqbasep, cfra, machine);
	seq->len= totframe;
	seq->type= SEQ_HD_SOUND;
	seq->hdaudio= hdaudio;

	calc_sequence(seq);
	
	if(sfile->flag & FILE_STRINGCODE) {
		strcpy(name, sfile->dir);
		BLI_makestringcode(G.sce, name);
	} else {
		strcpy(name, sfile->dir);
	}

	/* strip and stripdata */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len= totframe;
	strip->us= 1;
	strncpy(strip->dir, name, FILE_MAXDIR-1);
	strip->stripdata= se= MEM_callocN(sizeof(StripElem), "stripelem");

	/* name movie in first strip */
	if(index<0)
		strncpy(se->name, sfile->file, FILE_MAXFILE-1);
	else
		strncpy(se->name, sfile->filelist[index].relname, FILE_MAXFILE-1);

	/* last active name */
	strncpy(ed->act_sounddir, seq->strip->dir, FILE_MAXDIR-1);
	return(cfra+totframe);
}
#endif

static void sfile_to_hdsnd_sequence(SpaceFile *sfile, int cfra, int machine)
{
#if 0
	/* XXX sfile recoded... */
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
#endif
}


static void add_image_strips(Scene *scene, char *name)
{
#if 0
	/* XXX sfile recoded... */

	SpaceFile *sfile;
	struct direntry *files;
	float x, y;
	int a, totfile, cfra, machine;
	short mval[2];

	deselect_all_seq(scene);

	/* restore windowmatrices */
// XXX	drawseqspace(curarea, curarea->spacedata.first);

	/* search sfile */
//	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	/* where will it be */
//	getmouseco_areawin(mval);
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
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

				if ( sfile_to_sequence(scene, sfile, cfra, machine, 0) ) machine++;

				parent(sfile);
			}
		}
	}

	sfile->filelist= files;
	sfile->totfile= totfile;

	/* read directory itself */
	sfile_to_sequence(scene, sfile, cfra, machine, 1);

	waitcursor(0);

	BIF_undo_push("Add Image Strip, Sequencer");
	transform_seq_nomarker('g', 0);
#endif
}

static void add_movie_strip(Scene *scene, View2D *v2d, char *name)
{

	/* XXX sfile recoded... */
	SpaceFile *sfile;
	float x, y;
	int cfra, machine;
	short mval[2];

	deselect_all_seq(scene);

	/* restore windowmatrices */
//	drawseqspace(curarea, curarea->spacedata.first);

	/* search sfile */
//	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	/* where will it be */
//	getmouseco_areawin(mval);
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	waitcursor(1);

	/* read directory itself */
	sfile_to_mv_sequence(sfile, cfra, machine);

	waitcursor(0);

	BIF_undo_push("Add Movie Strip, Sequencer");
	transform_seq_nomarker('g', 0);

}

static void add_movie_and_hdaudio_strip(Scene *scene, View2D *v2d, char *name)
{
	SpaceFile *sfile;
	float x, y;
	int cfra, machine;
	short mval[2];

	deselect_all_seq(scene);

	/* restore windowmatrices */
//	areawinset(curarea->win);
//	drawseqspace(curarea, curarea->spacedata.first);

	/* search sfile */
//	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==0) return;

	/* where will it be */
//	getmouseco_areawin(mval);
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	waitcursor(1);

	/* read directory itself */
	sfile_to_hdsnd_sequence(sfile, cfra, machine);
	sfile_to_mv_sequence(sfile, cfra, machine);

	waitcursor(0);

	BIF_undo_push("Add Movie and HD-Audio Strip, Sequencer");
	transform_seq_nomarker('g', 0);

}

static void add_sound_strip_ram(Scene *scene, View2D *v2d, char *name)
{
	SpaceFile *sfile;
	float x, y;
	int cfra, machine;
	short mval[2];

	deselect_all_seq(scene);

//	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if (sfile==0) return;

	/* where will it be */
//	getmouseco_areawin(mval);
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	waitcursor(1);

	sfile_to_ramsnd_sequence(scene, sfile, cfra, machine);

	waitcursor(0);

	BIF_undo_push("Add Sound (RAM) Strip, Sequencer");
	transform_seq_nomarker('g', 0);
}

static void add_sound_strip_hd(Scene *scene, View2D *v2d, char *name)
{
	SpaceFile *sfile;
	float x, y;
	int cfra, machine;
	short mval[2];

	deselect_all_seq(scene);

//	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if (sfile==0) return;

	/* where will it be */
//	getmouseco_areawin(mval);
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	waitcursor(1);

	sfile_to_hdsnd_sequence(sfile, cfra, machine);

	waitcursor(0);

	BIF_undo_push("Add Sound (HD) Strip, Sequencer");
	transform_seq_nomarker('g', 0);
}

static void add_scene_strip(Scene *scene, View2D *v2d, short event)
{
	Sequence *seq;
	Strip *strip;
	float x, y;
	int cfra, machine;
	short mval[2];

	if(event> -1) {
		int nr= 1;
		Scene * sce= G.main->scene.first;
		while(sce) {
			if( event==nr) break;
			nr++;
			sce= sce->id.next;
		}
		if(sce) {

			deselect_all_seq(scene);

			/* where ? */
//			getmouseco_areawin(mval);
			UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
			cfra= (int)(x+0.5);
			machine= (int)(y+0.5);
			
			seq= alloc_sequence(((Editing *)scene->ed)->seqbasep, cfra, machine);
			seq->type= SEQ_SCENE;
			seq->scene= sce;
			seq->sfra= sce->r.sfra;
			seq->len= sce->r.efra - sce->r.sfra + 1;
			
			seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
			strncpy(seq->name + 2, sce->id.name + 2, 
				sizeof(seq->name) - 2);
			strip->len= seq->len;
			strip->us= 1;
			
			BIF_undo_push("Add Scene Strip, Sequencer");
			transform_seq_nomarker('g', 0);
		}
	}
}



static int add_seq_effect(Scene *scene, View2D *v2d, int type, char *str)
{
	Editing *ed;
	Sequence *newseq, *seq1, *seq2, *seq3;
	Strip *strip;
	float x, y;
	int cfra, machine;
	short mval[2];
	struct SeqEffectHandle sh;

	if(scene->ed==NULL) return 0;
	ed= scene->ed;

	if(!seq_effect_find_selected(scene, NULL, event_to_efftype(type), &seq1, &seq2, &seq3))
		return 0;

	deselect_all_seq(scene);

	/* where will it be (cfra is not realy needed) */
//	getmouseco_areawin(mval);
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
	cfra= (int)(x+0.5);
	machine= (int)(y+0.5);

	/* allocate and initialize */
	newseq= alloc_sequence(((Editing *)scene->ed)->seqbasep, cfra, machine);
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
			seq_free_sequence(ed, newseq);
			set_last_seq(scene, NULL);
			return 0;
		}
	}

	/* set find a free spot to but the strip */
	if (newseq->seq1) {
		newseq->machine= MAX3(newseq->seq1->machine, 
				      newseq->seq2->machine,
				      newseq->seq3->machine);
	}
	if(test_overlap_seq(scene, newseq)) shuffle_seq(scene, newseq);

	update_changed_seq_and_deps(scene, newseq, 1, 1);

	/* push undo and go into grab mode */
	if(newseq->type == SEQ_PLUGIN) {
		BIF_undo_push("Add Plugin Strip, Sequencer");
	} else {
		BIF_undo_push("Add Effect Strip, Sequencer");
	}

	transform_seq_nomarker('g', 0);

	return 1;
}

static void load_plugin_seq(Scene *scene, View2D *v2d, char *str)		/* called from fileselect */
{
	add_seq_effect(scene, v2d, 10, str);
}

void add_sequence(Scene *scene, View2D *v2d, int type)
{
	Editing *ed= scene->ed;
	short event;
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
			       "|Image Sequence%x1"
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

	if(scene->ed==NULL) {
		ed= scene->ed= MEM_callocN( sizeof(Editing), "addseq");
		ed->seqbasep= &ed->seqbase;
	}

	switch(event) {
	case 1:
		/* Image Dosnt work at the moment - TODO */
		//if(G.qual & LR_CTRLKEY)
		//	activate_imageselect(FILE_SPECIAL, "Select Images", ed->act_imagedir, add_image_strips);
		//else
			activate_fileselect(FILE_SPECIAL, "Select Images", ed->act_imagedir, add_image_strips);
		break;
	case 105:
		activate_fileselect(FILE_SPECIAL, "Select Movie+Audio", ed->act_imagedir, add_movie_and_hdaudio_strip);
		break;
	case 102:

		activate_fileselect(FILE_SPECIAL, "Select Movie", ed->act_imagedir, add_movie_strip);
		break;
	case 101:
		/* new menu: */
		IDnames_to_pupstring(&str, NULL, NULL, &G.main->scene, (ID *)scene, NULL);

		add_scene_strip(scene, v2d, pupmenu_col(str, 20));

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
		if(get_last_seq(scene)==0 && 
		   get_sequence_effect_num_inputs( event_to_efftype(event))> 0)
			error("Need at least one active sequence strip");
		else if(event==10)
			activate_fileselect(FILE_SPECIAL, "Select Plugin", U.plugseqdir, load_plugin_seq);
		else
			add_seq_effect(scene, v2d, event, NULL);

		break;
	case 103:
		if (ed->act_sounddir[0]=='\0') strncpy(ed->act_sounddir, U.sounddir, FILE_MAXDIR-1);
		activate_fileselect(FILE_SPECIAL, "Select Audio (RAM)", ed->act_sounddir, add_sound_strip_ram);
		break;
	case 104:
		if (ed->act_sounddir[0]=='\0') strncpy(ed->act_sounddir, U.sounddir, FILE_MAXDIR-1);
		activate_fileselect(FILE_SPECIAL, "Select Audio (HD)", ed->act_sounddir, add_sound_strip_hd);
		break;
	}
}


/* Generic functions, reused by add strip operators */
static void sequencer_generic_props__internal(wmOperatorType *ot, int do_filename, int do_endframe)
{
	RNA_def_string(ot->srna, "name", "", MAX_ID_NAME-2, "Name", "Name of the new sequence strip");
	RNA_def_int(ot->srna, "start_frame", 0, INT_MIN, INT_MAX, "Start Frame", "Start frame of the sequence strip", INT_MIN, INT_MAX);
	
	if (do_endframe)
		RNA_def_int(ot->srna, "end_frame", 0, INT_MIN, INT_MAX, "End Frame", "End frame for the color strip", INT_MIN, INT_MAX); /* not useual since most strips have a fixed length */
	
	RNA_def_int(ot->srna, "channel", 1, 1, MAXSEQ, "Channel", "Channel to place this strip into", 1, MAXSEQ);
	
	if (do_filename)
		RNA_def_string(ot->srna, "filename", "", FILE_MAX, "Scene Name", "full path to load the strip data from");
	
	RNA_def_boolean(ot->srna, "replace_sel", 1, "Replace Selection", "replace the current selection");
}

static void sequencer_generic_invoke_xy__internal(bContext *C, wmOperator *op, wmEvent *event, int do_endframe)
{
	ARegion *ar= CTX_wm_region(C);
	View2D *v2d= UI_view2d_fromcontext(C);
	
	short mval[2];	
	float mval_v2d[2];

	mval[0]= event->x - ar->winrct.xmin;
	mval[1]= event->y - ar->winrct.ymin;
	
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &mval_v2d[0], &mval_v2d[1]);
	
	RNA_int_set(op->ptr, "channel", (int)mval_v2d[1]+0.5f);
	RNA_int_set(op->ptr, "start_frame", (int)mval_v2d[0]);
	if (do_endframe)
		RNA_int_set(op->ptr, "end_frame", (int)mval_v2d[0] + 25); // XXX arbitary but ok for now.
	
}

static void sequencer_generic_filesel__internal(bContext *C, wmOperator *op, char *title, char *path)
{	
	SpaceFile *sfile;

	ED_screen_full_newspace(C, CTX_wm_area(C), SPACE_FILE);

	/* settings for filebrowser */
	sfile= (SpaceFile*)CTX_wm_space_data(C);
	sfile->op = op;
	ED_fileselect_set_params(sfile, FILE_BLENDER, title, path, 0, 0, 0);

	/* screen and area have been reset already in ED_screen_full_newspace */
}

/* add operators */
static int sequencer_add_color_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= scene->ed;
	
	
	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;
	
	SolidColorVars *colvars; /* type spesific vars */
	
	int start_frame, end_frame, channel; /* operator props */
	
	start_frame= RNA_int_get(op->ptr, "start_frame");
	end_frame= RNA_int_get(op->ptr, "end_frame");
	channel= RNA_int_get(op->ptr, "channel");
	
	if (end_frame <= start_frame) /* XXX use error reporter for bad frame values? */
		end_frame= start_frame+1;
	
	seq = alloc_sequence(ed->seqbasep, start_frame, channel); /* warning, this sets last */
	
	seq->effectdata = MEM_callocN(sizeof(struct SolidColorVars), "solidcolor");
	colvars= (SolidColorVars *)seq->effectdata;
	
	seq->type= SEQ_COLOR;
	
	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len = 1; /* Color strips are different in that they can be any length */
	strip->us= 1;
	
	strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
	
	RNA_string_get(op->ptr, "name", seq->name);
	RNA_float_get_array(op->ptr, "color", colvars->col);
	
	seq_tx_set_final_right(seq, end_frame);

	calc_sequence_disp(seq);
	sort_seq(scene);
	
	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		deselect_all_seq(scene);
		set_last_seq(scene, seq);
		seq->flag |= SELECT;
	}
	ED_undo_push(C, "Add Color Strip, Sequencer");
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}


/* add color */
static int sequencer_add_color_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sequencer_generic_invoke_xy__internal(C, op, event, 1);
	return sequencer_add_color_strip_exec(C, op);
}


void SEQUENCER_OT_add_color_strip(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Color Strip";
	ot->idname= "SEQUENCER_OT_add_color_strip";

	/* api callbacks */
	ot->invoke= sequencer_add_color_strip_invoke;
	ot->exec= sequencer_add_color_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	ot->flag= OPTYPE_REGISTER;

	sequencer_generic_props__internal(ot, 0, 1);
	RNA_def_float_vector(ot->srna, "color", 3, NULL, 0.0f, 1.0f, "Color", "Initialize the strip with this color", 0.0f, 1.0f);
}


/* add scene operator */
static int sequencer_add_scene_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= scene->ed;
	
	Scene *sce_seq;
	char sce_name[MAX_ID_NAME-2];
	
	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;
	
	int start_frame, channel; /* operator props */
	
	start_frame= RNA_int_get(op->ptr, "start_frame");
	channel= RNA_int_get(op->ptr, "channel");
	
	RNA_string_get(op->ptr, "scene", sce_name);

	sce_seq= find_id("SC", sce_name);
	
	if (sce_seq==NULL) {
		BKE_reportf(op->reports, RPT_ERROR, "Scene \"%s\" not found", sce_name);
		return OPERATOR_CANCELLED;
	}
	
	seq = alloc_sequence(ed->seqbasep, start_frame, channel); /* warning, this sets last */
	
	seq->type= SEQ_SCENE;
	seq->scene= sce_seq;
	
	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len = sce_seq->r.efra - sce_seq->r.sfra + 1;
	strip->us= 1;
	
	strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
	
	
	RNA_string_get(op->ptr, "name", seq->name);
	
	calc_sequence_disp(seq);
	sort_seq(scene);
	
	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		deselect_all_seq(scene);
		set_last_seq(scene, seq);
		seq->flag |= SELECT;
	}
	
	ED_undo_push(C, "Add Scene Strip, Sequencer");
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}


static int sequencer_add_scene_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sequencer_generic_invoke_xy__internal(C, op, event, 0);
	
	/* scene can be left default */
	RNA_string_set(op->ptr, "scene", "Scene"); // XXX should popup a menu but ton says 2.5 will have some better feature for this

	return sequencer_add_scene_strip_exec(C, op);
}


void SEQUENCER_OT_add_scene_strip(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Add Scene Strip";
	ot->idname= "SEQUENCER_OT_add_scene_strip";

	/* api callbacks */
	ot->invoke= sequencer_add_scene_strip_invoke;
	ot->exec= sequencer_add_scene_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	ot->flag= OPTYPE_REGISTER;

	sequencer_generic_props__internal(ot, 0, 0);
	RNA_def_string(ot->srna, "scene", "", MAX_ID_NAME-2, "Scene Name", "Scene name to add as a strip");
}

/* add movie operator */
static int sequencer_add_movie_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= scene->ed;
	
	struct anim *an;
	char filename[FILE_MAX];

	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;
	
	int start_frame, channel; /* operator props */
	
	start_frame= RNA_int_get(op->ptr, "start_frame");
	channel= RNA_int_get(op->ptr, "channel");
	
	RNA_string_get(op->ptr, "filename", filename);
	
	an = openanim(filename, IB_rect);

	if (an==NULL) {
		BKE_reportf(op->reports, RPT_ERROR, "Filename \"%s\" could not be loaded as a movie", filename);
		return OPERATOR_CANCELLED;
	}
	
	seq = alloc_sequence(ed->seqbasep, start_frame, channel); /* warning, this sets last */
	
	seq->type= SEQ_MOVIE;
	seq->anim= an;
	seq->anim_preseek = IMB_anim_get_preseek(an);
	
	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len = IMB_anim_get_duration( an ); 
	strip->us= 1;
	
	strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
	
	BLI_split_dirfile_basic(filename, strip->dir, se->name);

	RNA_string_get(op->ptr, "name", seq->name);
	
	calc_sequence_disp(seq);
	sort_seq(scene);

	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		deselect_all_seq(scene);
		set_last_seq(scene, seq);
		seq->flag |= SELECT;
	}
	
	ED_undo_push(C, "Add Movie Strip, Sequencer");
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}


static int sequencer_add_movie_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{	
	sequencer_generic_invoke_xy__internal(C, op, event, 0);
	sequencer_generic_filesel__internal(C, op, "Load Movie", "/");
	return OPERATOR_RUNNING_MODAL;	
	//return sequencer_add_movie_strip_exec(C, op);
}


void SEQUENCER_OT_add_movie_strip(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Add Movie Strip";
	ot->idname= "SEQUENCER_OT_add_movie_strip";

	/* api callbacks */
	ot->invoke= sequencer_add_movie_strip_invoke;
	ot->exec= sequencer_add_movie_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	ot->flag= OPTYPE_REGISTER;
	
	sequencer_generic_props__internal(ot, 1, 0);
}


/* add sound operator */
static int sequencer_add_sound_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= scene->ed;
	
	bSound *sound;

	char filename[FILE_MAX];

	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;
	
	int start_frame, channel; /* operator props */
	
	start_frame= RNA_int_get(op->ptr, "start_frame");
	channel= RNA_int_get(op->ptr, "channel");
	
	RNA_string_get(op->ptr, "filename", filename);

// XXX	sound= sound_new_sound(filename);
	sound= NULL;

	if (sound==NULL || sound->sample->type == SAMPLE_INVALID) {
		BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
		return OPERATOR_CANCELLED;
	}

	if (sound==NULL || sound->sample->bits != 16) {
		BKE_report(op->reports, RPT_ERROR, "Only 16 bit audio is supported");
		return OPERATOR_CANCELLED;
	}
	
	sound->flags |= SOUND_FLAGS_SEQUENCE;
// XXX	audio_makestream(sound);
	
	seq = alloc_sequence(ed->seqbasep, start_frame, channel); /* warning, this sets last */
	
	seq->type= SEQ_RAM_SOUND;
	seq->sound= sound;
	
	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len = (int) ( ((float)(sound->streamlen-1) / ( (float)scene->r.audio.mixrate*4.0 ))* FPS);
	strip->us= 1;
	
	strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
	
	BLI_split_dirfile_basic(filename, strip->dir, se->name);

	RNA_string_get(op->ptr, "name", seq->name);
	
	calc_sequence_disp(seq);
	sort_seq(scene);
	
	/* last active name */
	strncpy(ed->act_sounddir, strip->dir, FILE_MAXDIR-1);

	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		deselect_all_seq(scene);
		set_last_seq(scene, seq);
		seq->flag |= SELECT;
	}

	ED_undo_push(C, "Add Sound Strip, Sequencer");
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}


static int sequencer_add_sound_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= scene->ed;
	
	sequencer_generic_invoke_xy__internal(C, op, event, 0);
	sequencer_generic_filesel__internal(C, op, "Load Sound", ed->act_sounddir);
	return OPERATOR_RUNNING_MODAL;
	//return sequencer_add_sound_strip_exec(C, op);
}


void SEQUENCER_OT_add_sound_strip(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Add Sound Strip";
	ot->idname= "SEQUENCER_OT_add_sound_strip";

	/* api callbacks */
	ot->invoke= sequencer_add_sound_strip_invoke;
	ot->exec= sequencer_add_sound_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	ot->flag= OPTYPE_REGISTER;

	sequencer_generic_props__internal(ot, 1, 0);
}

/* add image operator */
static int sequencer_add_image_strip_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= scene->ed;


	int tot_images= 1; //XXX FIXME, we need string arrays!
	//int a;

	char filename[FILE_MAX];

	Sequence *seq;	/* generic strip vars */
	Strip *strip;
	StripElem *se;
	
	int start_frame, channel; /* operator props */
	
	start_frame= RNA_int_get(op->ptr, "start_frame");
	channel= RNA_int_get(op->ptr, "channel");
	
	RNA_string_get(op->ptr, "filename", filename);

	seq = alloc_sequence(ed->seqbasep, start_frame, channel); /* warning, this sets last */
	
	seq->type= SEQ_IMAGE;
	
	/* basic defaults */
	seq->strip= strip= MEM_callocN(sizeof(Strip), "strip");
	strip->len = seq->len = tot_images;	
	strip->us= 1;
	
	strip->stripdata= se= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
	

	BLI_split_dirfile_basic(filename, strip->dir, se->name); // XXX se->name assignment should be moved into the loop below

#if 0 // XXX
	for(a=0; a<seq->len; a++) {
	   strncpy(se->name, name, FILE_MAXFILE-1);
	   se++;
	}
#endif

	RNA_string_get(op->ptr, "name", seq->name);
	
	calc_sequence_disp(seq);
	sort_seq(scene);
	
	/* last active name */
	strncpy(ed->act_imagedir, strip->dir, FILE_MAXDIR-1);
	
	if (RNA_boolean_get(op->ptr, "replace_sel")) {
		deselect_all_seq(scene);
		set_last_seq(scene, seq);
		seq->flag |= SELECT;
	}

	ED_undo_push(C, "Add Image Strip, Sequencer");
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}


static int sequencer_add_image_strip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	Editing *ed= scene->ed;
	
	sequencer_generic_invoke_xy__internal(C, op, event, 0);
	sequencer_generic_filesel__internal(C, op, "Load Image", ed->act_imagedir);
	return OPERATOR_RUNNING_MODAL;
	
	//return sequencer_add_image_strip_exec(C, op);
}


void SEQUENCER_OT_add_image_strip(struct wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Add Image Strip";
	ot->idname= "SEQUENCER_OT_add_image_strip";

	/* api callbacks */
	ot->invoke= sequencer_add_image_strip_invoke;
	ot->exec= sequencer_add_image_strip_exec;

	ot->poll= ED_operator_sequencer_active;
	ot->flag= OPTYPE_REGISTER;
	
	sequencer_generic_props__internal(ot, 1, 0);
}

