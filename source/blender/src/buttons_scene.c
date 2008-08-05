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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_sequence_types.h"
#include "DNA_userdef_types.h"
#include "DNA_packedFile_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_library.h"
#include "BKE_scene.h"
#include "BKE_sound.h"
#include "BKE_packedFile.h"
#include "BKE_utildefines.h"
#include "BKE_idprop.h"

#include "BLI_blenlib.h"

#include "BSE_filesel.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_imasel.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"
#include "BIF_mywindow.h"
#include "BIF_meshtools.h"
#include "BIF_resources.h"
#include "BIF_renderwin.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_editseq.h"

#include "BIF_butspace.h"

#include "mydevice.h"
#include "blendef.h"

/* -----includes for this file specific----- */

#include "DNA_image_types.h"

#include "BKE_writeavi.h"
#include "BKE_writeffmpeg.h"
#include "BKE_image.h"
#include "BKE_plugin_types.h"

#include "BLI_threads.h"

#include "BIF_editsound.h"
#include "BIF_writeimage.h"
#include "BIF_writeavicodec.h"

#include "BSE_headerbuttons.h"
#include "BSE_sequence.h"
#include "BSE_seqeffects.h"
#include "BSE_seqscopes.h"
#include "BSE_seqaudio.h"

#include "RE_pipeline.h"

#include "butspace.h" // own module

#ifdef WITH_QUICKTIME
#include "quicktime_export.h"
#endif

#ifdef WITH_FFMPEG

#include <ffmpeg/avcodec.h> /* for PIX_FMT_* and CODEC_ID_* */
#include <ffmpeg/avformat.h>
#include <ffmpeg/opt.h>

static int ffmpeg_preset_sel = 0;

extern int is_container(int);

extern void makeffmpegstring(char* string);

#endif

/* here the calls for scene buttons
   - render
   - world
   - anim settings, audio
*/

/* prototypes */
void playback_anim(void);

/* ************************ SOUND *************************** */
static void load_new_sample(char *str)	/* called from fileselect */
{
	char name[FILE_MAX];
	bSound *sound;
	bSample *sample, *newsample;

	sound = G.buts->lockpoin;
	
	/* No Sound or Selected the same sample as we alredy have, just ignore */
	if (sound==NULL || str==sound->name)
		return;
		
	if (sizeof(sound->sample->name) < strlen(str)) {
		error("Path too long: %s", str);
		return;
	}
		
	// save values
	sample = sound->sample;
	strcpy(name, sound->sample->name);	
	strcpy(sound->name, str);
	sound_set_sample(sound, NULL);
	sound_initialize_sample(sound);

	if (sound->sample->type == SAMPLE_INVALID) {
		error("Not a valid sample: %s", str);

		newsample = sound->sample;

		// restore values
		strcpy(sound->name, name);
		sound_set_sample(sound, sample);

		// remove invalid sample

		sound_free_sample(newsample);
		BLI_remlink(samples, newsample);
		MEM_freeN(newsample);
		return;
	}
	
	BIF_undo_push("Load new audio file");
	allqueue(REDRAWBUTSSCENE, 0);
}


void do_soundbuts(unsigned short event)
{
	char name[FILE_MAX];
	bSound *sound;
	bSample *sample;
	bSound* tempsound;
	ID *id;
	
	sound = G.buts->lockpoin;
	
	switch(event) {
	case B_SOUND_REDRAW:
		allqueue(REDRAWBUTSSCENE, 0);
		break;

	case B_SOUND_LOAD_SAMPLE:
		if (sound) strcpy(name, sound->name);
		else strcpy(name, U.sounddir);
			
		activate_fileselect(FILE_SPECIAL, "SELECT WAV FILE", name, load_new_sample);
		break;

	case B_SOUND_PLAY_SAMPLE:
		if (sound) {
			if (sound->sample->type != SAMPLE_INVALID) {
				sound_play_sound(sound);
				allqueue(REDRAWBUTSSCENE, 0);
			}
		}
		break;

	case B_SOUND_MENU_SAMPLE:
		if (G.buts->menunr > 0) {
			sample = BLI_findlink(samples, G.buts->menunr - 1);
			if (sample && sound) {
				BLI_strncpy(sound->name, sample->name, sizeof(sound->name));
				sound_set_sample(sound, sample);
				do_soundbuts(B_SOUND_REDRAW);
			}
		}
			
		break;
	case B_SOUND_NAME_SAMPLE:
		load_new_sample(sound->name);
		break;
	
	case B_SOUND_UNPACK_SAMPLE:
		if(sound && sound->sample) {
			sample = sound->sample;
			
			if (sample->packedfile) {
				if (G.fileflags & G_AUTOPACK) {
					if (okee("Disable AutoPack ?")) {
						G.fileflags &= ~G_AUTOPACK;
					}
				}
				
				if ((G.fileflags & G_AUTOPACK) == 0) {
					unpackSample(sample, PF_ASK);
				}
			} else {
				sound_set_packedfile(sample, newPackedFile(sample->name));
			}
			allqueue(REDRAWHEADERS, 0);
			do_soundbuts(B_SOUND_REDRAW);
		}
		break;

	case B_SOUND_COPY_SOUND:
		if (sound) {
			tempsound = sound_make_copy(sound);
			sound = tempsound;
			id = &sound->id;
			G.buts->lockpoin = (bSound*)id;
			BIF_undo_push("Copy sound");
			do_soundbuts(B_SOUND_REDRAW);
		}
		break;

	case B_SOUND_RECALC:
		waitcursor(1);
		sound = G.main->sound.first;
		while (sound) {
			free(sound->stream);
			sound->stream = 0;
			audio_makestream(sound);
			sound = (bSound *) sound->id.next;
		}
		waitcursor(0);
		allqueue(REDRAWSEQ, 0);
		break;

	case B_SOUND_RATECHANGED:

		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWSEQ, 0);
		break;

	case B_SOUND_MIXDOWN:
		audio_mixdown();
		break;

	default: 
		if (G.f & G_DEBUG) {
			printf("do_soundbuts: unhandled event %d\n", event);
		}
	}
}


static void sound_panel_listener(void)
{
	uiBlock *block;
	int xco= 100, yco=100, mixrate;
	char mixrateinfo[256];
	
	block= uiNewBlock(&curarea->uiblocks, "sound_panel_listener", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Listener", "Sound", 320, 0, 318, 204)==0) return;

	mixrate = sound_get_mixrate();
	sprintf(mixrateinfo, "Game Mixrate: %d Hz", mixrate);
	uiDefBut(block, LABEL, 0, mixrateinfo, xco,yco,295,20, 0, 0, 0, 0, 0, "");

	yco -= 30;
	uiDefBut(block, LABEL, 0, "Game listener settings:",xco,yco,195,20, 0, 0, 0, 0, 0, "");

	yco -= 30;
	uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Volume: ",
		xco,yco,195,24,&G.listener->gain, 0.0, 1.0, 1.0, 0, "Sets the maximum volume for the overall sound");
	
	yco -= 30;
	uiDefBut(block, LABEL, 0, "Game Doppler effect settings:",xco,yco,195,20, 0, 0, 0, 0, 0, "");

	yco -= 30;
	uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Doppler: ",
	xco,yco,195,24,&G.listener->dopplerfactor, 0.0, 10.0, 1.0, 0, "Use this for scaling the doppler effect");
	
	yco -=30;
	uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Velocity: ",
	xco,yco,195,24,&G.listener->dopplervelocity,0.0,10000.0, 1.0,0, "Sets the propagation speed of sound");

	
}

static void sound_panel_sequencer(void)
{
	uiBlock *block;
	short xco, yco;
	char mixrateinfo[256];
	
	block= uiNewBlock(&curarea->uiblocks, "sound_panel_sequencer", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Sequencer", "Sound", 640, 0, 318, 204)==0) return;

	/* audio sequence engine settings ------------------------------------------------------------------ */

	xco = 1010;
	yco = 195;

	uiDefBut(block, LABEL, 0, "Audio sequencer settings", xco,yco,295,20, 0, 0, 0, 0, 0, "");

	yco -= 25;
	sprintf(mixrateinfo, "Mixing/Sync (latency: %d ms)", (int)( (((float)U.mixbufsize)/(float)G.scene->audio.mixrate)*1000.0 ) );
	uiDefBut(block, LABEL, 0, mixrateinfo, xco,yco,295,20, 0, 0, 0, 0, 0, "");

	yco -= 25;		
	uiDefButI(block, ROW, B_SOUND_RATECHANGED, "44.1 kHz",	xco,yco,75,20, &G.scene->audio.mixrate, 2.0, 44100.0, 0, 0, "Mix at 44.1 kHz");
	uiDefButI(block, ROW, B_SOUND_RATECHANGED, "48.0 kHz",		xco+80,yco,75,20, &G.scene->audio.mixrate, 2.0, 48000.0, 0, 0, "Mix at 48 kHz");
	uiDefBut(block, BUT, B_SOUND_RECALC, "Recalc",		xco+160,yco,75,20, 0, 0, 0, 0, 0, "Recalculate samples");

	yco -= 25;
	uiDefButBitS(block, TOG, AUDIO_SYNC, B_SOUND_CHANGED, "Sync",	xco,yco,115,20, &G.scene->audio.flag, 0, 0, 0, 0, "Use sample clock for syncing animation to audio");
	uiDefButBitS(block, TOG, AUDIO_SCRUB, B_SOUND_CHANGED, "Scrub",		xco+120,yco,115,20, &G.scene->audio.flag, 0, 0, 0, 0, "Scrub when changing frames");

	yco -= 25;
	uiDefBut(block, LABEL, 0, "Main mix", xco,yco,295,20, 0, 0, 0, 0, 0, "");

	yco -= 25;		
	uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Main (dB): ",
		xco,yco,235,24,&G.scene->audio.main, -24.0, 6.0, 0, 0, "Set the audio master gain/attenuation in dB");

	yco -= 25;
	uiDefButBitS(block, TOG, AUDIO_MUTE, 0, "Mute",	xco,yco,235,24, &G.scene->audio.flag, 0, 0, 0, 0, "Mute audio from sequencer");		
	
	yco -= 35;
	uiDefBut(block, BUT, B_SOUND_MIXDOWN, "MIXDOWN",	xco,yco,235,24, 0, 0, 0, 0, 0, "Create WAV file from sequenced audio (output goes to render output dir)");
	
}

static char *make_sample_menu(void)
{
	int len= BLI_countlist(samples);	/* BKE_sound.h */
	
	if(len) {
		bSample *sample;
		char *str;
		int nr, a=0;
		
		str= MEM_callocN(32*len, "menu");
		
		for(nr=1, sample= samples->first; sample; sample= sample->id.next, nr++) {
			a+= sprintf(str+a, "|%s %%x%d", sample->id.name+2, nr);
		}
		return str;
	}
	return NULL;
}

static void sound_panel_sound(bSound *sound)
{
	static int packdummy=0;
	ID *id, *idfrom;
	uiBlock *block;
	bSample *sample;
	char *strp, ch[256];

	block= uiNewBlock(&curarea->uiblocks, "sound_panel_sound", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Sound", "Sound", 0, 0, 318, 204)==0) return;
	
	uiDefBut(block, LABEL, 0, "Blender Sound block",10,180,195,20, 0, 0, 0, 0, 0, "");
	
	// warning: abuse of texnr here! (ton didnt code!)
	buttons_active_id(&id, &idfrom);
	std_libbuttons(block, 10, 160, 0, NULL, B_SOUNDBROWSE2, ID_SO, 0, id, idfrom, &(G.buts->texnr), 1, 0, 0, 0, 0);

	if (sound) {
	
		uiDefBut(block, BUT, B_SOUND_COPY_SOUND, "Copy sound", 220,160,90,20, 0, 0, 0, 0, 0, "Make another copy (duplicate) of the current sound");

		uiSetButLock(sound->id.lib!=0, ERROR_LIBDATA_MESSAGE);
		sound_initialize_sample(sound);
		sample = sound->sample;

		/* info string */
		if (sound->sample && sound->sample->len && sound->sample->channels && sound->sample->bits) {
			char *tmp;
			if (sound->sample->channels == 1) tmp= "Mono";
			else if (sound->sample->channels == 2) tmp= "Stereo";
			else tmp= "Unknown";
			
			sprintf(ch, "Sample: %s, %d bit, %d Hz, %d samples", tmp, sound->sample->bits, sound->sample->rate, (sound->sample->len/(sound->sample->bits/8)/sound->sample->channels));
			uiDefBut(block, LABEL, 0, ch, 			35,140,225,20, 0, 0, 0, 0, 0, "");
		}
		else {
			uiDefBut(block, LABEL, 0, "Sample: No sample info available.",35,140,225,20, 0, 0, 0, 0, 0, "");
		}

		/* sample browse buttons */
		uiBlockBeginAlign(block);
		strp= make_sample_menu();
		if (strp) {
			uiDefButS(block, MENU, B_SOUND_MENU_SAMPLE, strp, 10,120,23,20, &(G.buts->menunr), 0, 0, 0, 0, "Select another loaded sample");
			MEM_freeN(strp);
		}
		uiDefBut(block, TEX, B_SOUND_NAME_SAMPLE, "",		35,120,250,20, sound->name, 0.0, 79.0, 0, 0, "The sample file used by this Sound");
		
		if (sound->sample->packedfile) packdummy = 1;
		else packdummy = 0;
		
		uiDefIconButBitI(block, TOG, 1, B_SOUND_UNPACK_SAMPLE, ICON_PACKAGE,
			285, 120,25,20, &packdummy, 0, 0, 0, 0,"Pack/Unpack this sample");
		
		uiBlockBeginAlign(block);
		uiDefBut(block, BUT, B_SOUND_LOAD_SAMPLE, "Load sample", 10, 95,150,24, 0, 0, 0, 0, 0, "Load a different sample file");

		uiDefBut(block, BUT, B_SOUND_PLAY_SAMPLE, "Play", 	160, 95, 150, 24, 0, 0.0, 0, 0, 0, "Playback sample using settings below");
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Volume: ",
			10,70,150,20, &sound->volume, 0.0, 1.0, 0, 0, "Game engine only: Set the volume of this sound");

		uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Pitch: ",
			160,70,150,20, &sound->pitch, -12.0, 12.0, 0, 0, "Game engine only: Set the pitch of this sound");

		/* looping */
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, SOUND_FLAGS_LOOP, B_SOUND_REDRAW, "Loop",
			10, 50, 95, 20, &sound->flags, 0.0, 0.0, 0, 0, "Game engine only: Toggle between looping on/off");

		if (sound->flags & SOUND_FLAGS_LOOP) {
			uiDefButBitI(block, TOG, SOUND_FLAGS_BIDIRECTIONAL_LOOP, B_SOUND_REDRAW, "Ping Pong",
				105, 50, 95, 20, &sound->flags, 0.0, 0.0, 0, 0, "Game engine only: Toggle between A->B and A->B->A looping");
			
		}
	

		/* 3D settings ------------------------------------------------------------------ */
		uiBlockBeginAlign(block);

		if (sound->sample->channels == 1) {
			uiDefButBitI(block, TOG, SOUND_FLAGS_3D, B_SOUND_REDRAW, "3D Sound",
				10, 10, 90, 20, &sound->flags, 0, 0, 0, 0, "Game engine only: Turns 3D sound on");
			
			if (sound->flags & SOUND_FLAGS_3D) {
				uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Scale: ",
					100,10,210,20, &sound->attenuation, 0.0, 5.0, 1.0, 0, "Game engine only: Sets the surround scaling factor for this sound");
				
			}
		}
	}
}

/* ************************* Sequencer *********************** */

#define SEQ_PANEL_EDITING 1
#define SEQ_PANEL_INPUT   2
#define SEQ_PANEL_FILTER  4
#define SEQ_PANEL_EFFECT  8
#define SEQ_PANEL_PROXY   16

static char* seq_panel_blend_modes()
{
	static char string[2048];

	Sequence *last_seq = get_last_seq();

	sprintf(string, "Blend mode: %%t|%s %%x%d",
		"Replace", SEQ_BLEND_REPLACE);

	/*
	  Blending can only work without effect strips. 
	  Otherwise, one would have
	  to decide, what the effect strips IPO should do:
	  - drive the effect _or_
	  - drive the blend mode ?

	  Also: effectdata is used by these implicit effects,
	  so that would collide also.
	*/

	if ( seq_can_blend(last_seq) ) {
		int i;

		for (i = SEQ_EFFECT; i <= SEQ_EFFECT_MAX; i++) {
			if (get_sequence_effect_num_inputs(i) == 2) {
				sprintf(string + strlen(string), 
					"|%s %%x%d", 
					give_seqname_by_type(i), i);
			}
		}
	}
	return string;
}

static char* seq_panel_scenes()
{
	static char rstr[8192];
	char * str;

	IDnames_to_pupstring(&str, NULL, NULL, 
			     &G.main->scene, (ID *)G.scene, NULL);

	strncpy(rstr, str, 8192);
	MEM_freeN(str);

	return rstr;
}

static void seq_update_scenenr(Sequence * seq)
{
	Scene * sce;
	int nr;
	if (seq->type != SEQ_SCENE) {
		return;
	}

	seq->scenenr = 0;

	sce = G.main->scene.first;
	nr = 1;
	while(sce) {
		if (sce == seq->scene) {
			seq->scenenr = nr;
			break;
		}
		nr++;
		sce = sce->id.next;
	}
}


static void seq_panel_editing()
{
	Sequence *last_seq = get_last_seq();
	uiBlock *block;
	static char strdata[1024];
	char * str = strdata;
	char * p;
	int yco;

	block = uiNewBlock(&curarea->uiblocks, "seq_panel_editing", 
			   UI_EMBOSS, UI_HELV, curarea->win);

	if(uiNewPanel(curarea, block, "Edit", "Sequencer", 
		      10, 230, 318, 204) == 0) return;

	uiDefBut(block, LABEL, 
		 0, give_seqname(last_seq), 
		 10,140,60,19, 0, 
		 0, 0, 0, 0, "");

	uiDefBut(block, TEX, 
		 B_NOP, "Name: ", 
		 70,140,180,19, last_seq->name+2, 
		 0.0, 21.0, 100, 0, "");

	uiDefButI(block, MENU, B_SEQ_BUT_RELOAD, seq_panel_blend_modes(), 
		  10, 120, 120, 19, &last_seq->blend_mode, 
		  0,0,0,0, "Strip Blend Mode");

	uiDefButF(block, NUM, B_SEQ_BUT_RELOAD, "Blend:",
		  130, 120, 120, 19, &last_seq->blend_opacity, 
		  0.0, 100.0, 100.0, 0, 
		  "Blend opacity");

	uiDefButBitI(block, TOG, SEQ_MUTE,
		     B_SEQ_BUT_RELOAD_ALL, "Mute",
		     10,100,60,19, &last_seq->flag,
		     0.0, 1.0, 0, 0,
		     "Mute the current strip.");

	uiDefButBitI(block, TOG, SEQ_LOCK,
		     B_REDR, "Lock",
		     70,100,60,19, &last_seq->flag,
		     0.0, 1.0, 0, 0,
		     "Lock strip, so that it can't be transformed.");
	
	uiDefButBitI(block, TOG, SEQ_IPO_FRAME_LOCKED,
		     B_SEQ_BUT_RELOAD_ALL, "IPO Frame locked",
		     130,100,120,19, &last_seq->flag,
		     0.0, 1.0, 0, 0,
		     "Lock the IPO coordinates to the "
		     "global frame counter.");
	
	if (!(last_seq->flag & SEQ_LOCK)) {
		uiDefButI(block, NUM, 
			  B_SEQ_BUT_TRANSFORM, "Start", 
			  10, 80, 120, 20, &last_seq->start, 
			  -MAXFRAMEF, MAXFRAMEF, 0.0, 0.0, "Start of strip");
		uiDefButI(block, NUM, 
			  B_SEQ_BUT_TRANSFORM, "Chan", 
			  130, 80, 120, 20, &last_seq->machine, 
			  0.0, MAXSEQ, 0.0, 0.0, "Channel used (Y position)");
		
		if (check_single_seq(last_seq) || last_seq->len == 0) {
			uiDefButI(block, NUM, 
				B_SEQ_BUT_TRANSFORM, "End-Still", 
				130, 60, 120, 19, &last_seq->endstill, 
				0.0, MAXFRAMEF, 0.0, 0.0, "End still");
		} else {
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_TRANSFORM, "Start-Still", 
				  10, 60, 120, 20, &last_seq->startstill, 
				  0.0, MAXFRAMEF, 0.0, 0.0, "Start still");
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_TRANSFORM, "End-Still", 
				  130, 60, 120, 19, &last_seq->endstill, 
				  0.0, MAXFRAMEF, 0.0, 0.0, "End still");
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_TRANSFORM, "Start-Ofs", 
				  10, 40, 120, 20, &last_seq->startofs, 
				  0.0, last_seq->len - last_seq->endofs, 
				  0.0, 0.0, "Start offset");
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_TRANSFORM, "End-Ofs", 
				  130, 40, 120, 19, &last_seq->endofs, 
				  0.0, last_seq->len - last_seq->startofs, 
				  0.0, 0.0, "End offset");
		}
	}


	if(last_seq->type & SEQ_EFFECT)
		sprintf(str, "Len: %d\nFrom %d - %d\n", last_seq->len, last_seq->startdisp, last_seq->enddisp-1);
	else
		sprintf(str, "Len: %d(%d)\n", last_seq->enddisp-last_seq->startdisp, last_seq->len);

	str += strlen(str);

	if(last_seq->type==SEQ_IMAGE) {
		if (last_seq->len > 1) {
			/* CURRENT */
			StripElem * se= give_stripelem(last_seq, CFRA);
			StripElem * last;

			/* FIRST AND LAST */
	
			if(last_seq->strip) {
				se= last_seq->strip->stripdata;
				last= se+last_seq->len-1;
				if(last_seq->startofs) se+= last_seq->startofs;
				if(last_seq->endofs) last-= last_seq->endofs;
	
				sprintf(str, "First: %s at %d\nLast: %s at %d\n", se->name, last_seq->startdisp, last->name, last_seq->enddisp-1);
			}
		} else { /* single image */
			if (last_seq->strip) {
				sprintf(str, "Len: %d\n", last_seq->enddisp-last_seq->startdisp);
			}
		}

		str += strlen(str);

		/* orig size */
		if(last_seq->strip) {
			sprintf(str, "OrigSize: %d x %d\n", last_seq->strip->orx, last_seq->strip->ory);
		}
	}
	else if(last_seq->type==SEQ_MOVIE) {
		int sta= last_seq->startofs;
		int end= last_seq->len-1-last_seq->endofs;

		sprintf(str, "First: %d at %d\nLast: %d at %d\nCur: %d\n",
			sta, last_seq->startdisp, end, last_seq->enddisp-1,  
			(G.scene->r.cfra)-last_seq->startdisp);

		str += strlen(str);
		/* orig size */
		if(last_seq->strip) {
			sprintf(str, "OrigSize: %d x %d\n", 
				last_seq->strip->orx, last_seq->strip->ory);
		}
	}
	else if(last_seq->type==SEQ_SCENE) {
		TStripElem * se= give_tstripelem(last_seq,  (G.scene->r.cfra));
		if(se && last_seq->scene) {
			sprintf(str, "First: %d\nLast: %d\nCur: %d\n", last_seq->sfra+se->nr, last_seq->sfra, last_seq->sfra+last_seq->len-1); 
		}
		str += strlen(str);
		/* orig size */
		if(last_seq->strip) {
			sprintf(str, "OrigSize: %d x %d\n", 
				last_seq->strip->orx, last_seq->strip->ory);
		}
	}
	else if(last_seq->type==SEQ_RAM_SOUND
		|| last_seq->type == SEQ_HD_SOUND) {

		int sta= last_seq->startofs;
		int end= last_seq->len-1-last_seq->endofs;

		sprintf(str, "First: %d at %d\nLast: %d at %d\nCur: %d\n",
			sta, last_seq->startdisp, end, last_seq->enddisp-1,  
			(G.scene->r.cfra)-last_seq->startdisp);
	}
	else if(last_seq->type == SEQ_SPEED) {
		SpeedControlVars * vars = 
			(SpeedControlVars*) last_seq->effectdata;

		if (vars) {
			sprintf(str, "Last mapped frame: %d at %d\n", 
				vars->lastValidFrame, 
				vars->lastValidFrame 
				+ last_seq->startdisp);
		}
	}

	str = strdata;
	yco = 20;

	while ((p = strchr(str, '\n'))) {
		*p = 0;
		uiDefBut(block, LABEL, 0, str, 10,yco,240,17, 0, 
			 0, 0, 0, 0, "");
		str = p+1;
		yco -= 18;
	}
}

static void seq_panel_input()
{
	Sequence *last_seq = get_last_seq();
	uiBlock *block;

	block = uiNewBlock(&curarea->uiblocks, "seq_panel_input", 
			   UI_EMBOSS, UI_HELV, curarea->win);

	if(uiNewPanel(curarea, block, "Input", "Sequencer", 
		      10, 230, 318, 204) == 0) return;

	if (SEQ_HAS_PATH(last_seq)) {
		uiDefBut(block, TEX, 
			 B_SEQ_BUT_RELOAD_FILE, "Dir: ", 
			 10,140,240,19, last_seq->strip->dir, 
			 0.0, 160.0, 100, 0, "");
	}

	if (last_seq->type == SEQ_IMAGE) {
		int cfra = CFRA;
		StripElem * se;

		if(last_seq->startdisp >cfra) {
			cfra = last_seq->startdisp;
		} else if (last_seq->enddisp <= cfra) {
			cfra = last_seq->enddisp - 1;
		}

		se = give_stripelem(last_seq, cfra);

		if (se) {
			uiDefBut(block, TEX, 
				 B_SEQ_BUT_RELOAD_FILE, "File: ", 
				 10, 120, 190,19, se->name, 
				 0.0, 80.0, 100, 0, "");
		}

	} else if (last_seq->type == SEQ_MOVIE || 
		   last_seq->type == SEQ_HD_SOUND ||
		   last_seq->type == SEQ_RAM_SOUND) {
		uiDefBut(block, TEX, 
			 B_SEQ_BUT_RELOAD_FILE, "File: ", 
			 10,120,190,19, last_seq->strip->stripdata->name, 
			 0.0, 80.0, 100, 0, "");
	} else if (last_seq->type == SEQ_SCENE) {
		seq_update_scenenr(last_seq);
		uiDefButI(block, MENU, B_SEQ_BUT_RELOAD_FILE, 
			  seq_panel_scenes(), 
			  10, 120, 190, 19, &last_seq->scenenr, 
			  0,0,0,0, "Linked Scene");
	}

	uiDefBut(block, BUT, B_SEQ_BUT_RELOAD_FILE, 
		 "Reload",
		 200,120,50,19, 0, 0, 0, 0, 0, 
		 "Reload files/scenes from disk and update strip length.");

	if (last_seq->type == SEQ_MOVIE 
	    || last_seq->type == SEQ_IMAGE 
	    || last_seq->type == SEQ_SCENE
	    || last_seq->type == SEQ_META) {
		uiDefButBitI(block, TOG, SEQ_USE_CROP,
			     B_SEQ_BUT_RELOAD, "Use Crop",
			     10,100,240,19, &last_seq->flag,
			     0.0, 1.0, 0, 0,
			     "Crop image before processing.");

		if (last_seq->flag & SEQ_USE_CROP) {
			if (!last_seq->strip->crop) {
				last_seq->strip->crop = 
					MEM_callocN(sizeof(struct StripCrop), 
						    "StripCrop");
			}
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_RELOAD, "Top", 
				  10, 80, 120, 20, 
				  &last_seq->strip->crop->top, 
				  0.0, 4096, 0.0, 0.0, "Top of source image");
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_RELOAD, "Bottom", 
				  130, 80, 120, 20, 
				  &last_seq->strip->crop->bottom, 
				  0.0, 4096, 0.0, 0.0,
				  "Bottom of source image");
			
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_RELOAD, "Left", 
				  10, 60, 120, 20,
				  &last_seq->strip->crop->left, 
				  0.0, 4096, 0.0, 0.0, "Left");
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_RELOAD, "Right", 
				  130, 60, 120, 19, 
				  &last_seq->strip->crop->right, 
				  0.0, 4096, 0.0, 0.0, "Right");
		}
		
		uiDefButBitI(block, TOG, SEQ_USE_TRANSFORM,
			     B_SEQ_BUT_RELOAD, "Use Translate",
			     10,40,240,19, &last_seq->flag,
			     0.0, 1.0, 0, 0,
			     "Translate image before processing.");
		
		if (last_seq->flag & SEQ_USE_TRANSFORM) {
			if (!last_seq->strip->transform) {
				last_seq->strip->transform = 
					MEM_callocN(
						sizeof(struct StripTransform), 
						"StripTransform");
			}
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_RELOAD, "X-Ofs", 
				  10, 20, 120, 20, 
				  &last_seq->strip->transform->xofs, 
				  -4096.0, 4096, 0.0, 0.0, "X Offset");
			uiDefButI(block, NUM, 
				  B_SEQ_BUT_RELOAD, "Y-Ofs", 
				  130, 20, 120, 20, 
				  &last_seq->strip->transform->yofs, 
				  -4096.0, 4096, 0.0, 0.0, "Y Offset");
		}
	}

	uiDefButI(block, NUM, 
		  B_SEQ_BUT_RELOAD_FILE, "A-Start", 
		  10, 0, 120, 20, &last_seq->anim_startofs, 
		  0.0, last_seq->len + last_seq->anim_startofs, 0.0, 0.0, 
		  "Animation start offset (trim start)");
	uiDefButI(block, NUM, 
		  B_SEQ_BUT_RELOAD_FILE, "A-End", 
		  130, 0, 120, 20, &last_seq->anim_endofs, 
		  0.0, last_seq->len + last_seq->anim_endofs, 0.0, 0.0, 
		  "Animation end offset (trim end)");


	if (last_seq->type == SEQ_MOVIE) {
		uiDefButI(block, NUM, B_SEQ_BUT_RELOAD, "MPEG-Preseek:",
			  10, -20, 240,19, &last_seq->anim_preseek, 
			  0.0, 50.0, 100,0,
			  "On MPEG-seeking preseek this many frames");
	}

}

static void seq_panel_filter_video()
{
	Sequence *last_seq = get_last_seq();
	uiBlock *block;
	block = uiNewBlock(&curarea->uiblocks, "seq_panel_filter", 
			   UI_EMBOSS, UI_HELV, curarea->win);

	if(uiNewPanel(curarea, block, "Filter", "Sequencer", 
		      10, 230, 318, 204) == 0) return;


	uiBlockBeginAlign(block);


	uiDefButBitI(block, TOG, SEQ_MAKE_PREMUL, 
		     B_SEQ_BUT_RELOAD, "Premul", 
		     10,110,80,19, &last_seq->flag, 
		     0.0, 21.0, 100, 0, 
		     "Converts RGB values to become premultiplied with Alpha");

	uiDefButBitI(block, TOG, SEQ_MAKE_FLOAT, 
		     B_SEQ_BUT_RELOAD, "Float",	
		     90,110,80,19, &last_seq->flag, 
		     0.0, 21.0, 100, 0, 
		     "Convert input to float data");

	uiDefButBitI(block, TOG, SEQ_FILTERY, 
		     B_SEQ_BUT_RELOAD_FILE, "De-Inter",	
		     170,110,80,19, &last_seq->flag, 
		     0.0, 21.0, 100, 0, 
		     "For video movies to remove fields");

	uiDefButBitI(block, TOG, SEQ_FLIPX, 
		     B_SEQ_BUT_RELOAD, "FlipX",	
		     10,90,80,19, &last_seq->flag, 
		     0.0, 21.0, 100, 0, 
		     "Flip on the X axis");
	uiDefButBitI(block, TOG, SEQ_FLIPY, 
		     B_SEQ_BUT_RELOAD, "FlipY",	
		     90,90,80,19, &last_seq->flag, 
		     0.0, 21.0, 100, 0, 
		     "Flip on the Y axis");

	uiDefButBitI(block, TOG, SEQ_REVERSE_FRAMES,
		     B_SEQ_BUT_RELOAD, "Flip Time", 
		     170,90,80,19, &last_seq->flag, 
		     0.0, 21.0, 100, 0, 
		     "Reverse frame order");
		
	uiDefButF(block, NUM, B_SEQ_BUT_RELOAD, "Mul:",
		  10,70,120,19, &last_seq->mul, 
		  0.001, 20.0, 0.1, 0, 
		  "Multiply colors");

	uiDefButF(block, NUM, B_SEQ_BUT_RELOAD, "Strobe:",
		  130,70,120,19, &last_seq->strobe, 
		  1.0, 30.0, 100, 0, 
		  "Only display every nth frame");

	uiDefButBitI(block, TOG, SEQ_USE_COLOR_BALANCE,
		     B_SEQ_BUT_RELOAD, "Use Color Balance", 
		     10,50,240,19, &last_seq->flag, 
		     0.0, 21.0, 100, 0, 
		     "Activate Color Balance "
		     "(3-Way color correction) on input");


	if (last_seq->flag & SEQ_USE_COLOR_BALANCE) {
		if (!last_seq->strip->color_balance) {
			int c;
			StripColorBalance * cb 
				= last_seq->strip->color_balance 
				= MEM_callocN(
					sizeof(struct StripColorBalance), 
					"StripColorBalance");
			for (c = 0; c < 3; c++) {
				cb->lift[c] = 1.0;
				cb->gamma[c] = 1.0;
				cb->gain[c] = 1.0;
			}
		}

		uiDefBut(block, LABEL, 0, "Lift",
			 10,30,80,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, LABEL, 0, "Gamma",
			 90,30,80,19, 0, 0, 0, 0, 0, "");
		uiDefBut(block, LABEL, 0, "Gain",
			 170,30,80,19, 0, 0, 0, 0, 0, "");

		uiDefButF(block, COL, B_SEQ_BUT_RELOAD, "Lift",
			  10,10,80,19, last_seq->strip->color_balance->lift, 
			  0, 0, 0, 0, "Lift (shadows)");

		uiDefButF(block, COL, B_SEQ_BUT_RELOAD, "Gamma",
			  90,10,80,19, last_seq->strip->color_balance->gamma, 
			  0, 0, 0, 0, "Gamma (midtones)");

		uiDefButF(block, COL, B_SEQ_BUT_RELOAD, "Gain",
			  170,10,80,19, last_seq->strip->color_balance->gain, 
			  0, 0, 0, 0, "Gain (highlights)");

		uiDefButBitI(block, TOG, SEQ_COLOR_BALANCE_INVERSE_LIFT,
			     B_SEQ_BUT_RELOAD, "Inv Lift", 
			     10,-10,80,19, 
			     &last_seq->strip->color_balance->flag, 
			     0.0, 21.0, 100, 0, 
			     "Inverse Lift");
		uiDefButBitI(block, TOG, SEQ_COLOR_BALANCE_INVERSE_GAMMA,
			     B_SEQ_BUT_RELOAD, "Inv Gamma", 
			     90,-10,80,19, 
			     &last_seq->strip->color_balance->flag, 
			     0.0, 21.0, 100, 0, 
			     "Inverse Gamma");
		uiDefButBitI(block, TOG, SEQ_COLOR_BALANCE_INVERSE_GAIN,
			     B_SEQ_BUT_RELOAD, "Inv Gain", 
			     170,-10,80,19, 
			     &last_seq->strip->color_balance->flag, 
			     0.0, 21.0, 100, 0, 
			     "Inverse Gain");
	}


	uiBlockEndAlign(block);

}


static void seq_panel_filter_audio()
{
	Sequence *last_seq = get_last_seq();
	uiBlock *block;
	block = uiNewBlock(&curarea->uiblocks, "seq_panel_filter", 
			   UI_EMBOSS, UI_HELV, curarea->win);

	if(uiNewPanel(curarea, block, "Filter", "Sequencer", 
		      10, 230, 318, 204) == 0) return;

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_SEQ_BUT_RELOAD, "Gain (dB):", 10,50,150,19, &last_seq->level, -96.0, 6.0, 100, 0, "");
	uiDefButF(block, NUM, B_SEQ_BUT_RELOAD, "Pan:", 	10,30,150,19, &last_seq->pan, -1.0, 1.0, 100, 0, "");
	uiBlockEndAlign(block);
}

static void seq_panel_effect()
{
	Sequence *last_seq = get_last_seq();
	uiBlock *block;
	block = uiNewBlock(&curarea->uiblocks, "seq_panel_effect", 
			   UI_EMBOSS, UI_HELV, curarea->win);

	if(uiNewPanel(curarea, block, "Effect", "Sequencer", 
		      10, 230, 318, 204) == 0) return;

	if(last_seq->type == SEQ_PLUGIN) {
		PluginSeq *pis;
		VarStruct *varstr;
		int a, xco, yco;

		get_sequence_effect(last_seq);/* make sure, plugin is loaded */

		pis= last_seq->plugin;
		if(pis->vars==0) return;

		varstr= pis->varstr;
		if(varstr) {
			for(a=0; a<pis->vars; a++, varstr++) {
				xco= 150*(a/6)+10;
				yco= 125 - 20*(a % 6)+1;
				uiDefBut(block, varstr->type, B_SEQ_BUT_PLUGIN, varstr->name, xco,yco,150,19, &(pis->data[a]), varstr->min, varstr->max, 100, 0, varstr->tip);

			}
		}
		return;
	} 

	uiBlockBeginAlign(block);

	if(last_seq->type==SEQ_WIPE){
		WipeVars *wipe = (WipeVars *)last_seq->effectdata;
		char formatstring[256];
			
		strncpy(formatstring, "Transition Type %t|Single Wipe%x0|Double Wipe %x1|Iris Wipe %x4|Clock Wipe %x5", 255);
		uiDefButS(block, MENU,B_SEQ_BUT_EFFECT, formatstring,	10,65,220,22, &wipe->wipetype, 0, 0, 0, 0, "What type of wipe should be performed");
		uiDefButF(block, NUM,B_SEQ_BUT_EFFECT,"Blur:",	10,40,220,22, &wipe->edgeWidth,0.0,1.0, 1, 2, "The percent width of the blur edge");
		switch(wipe->wipetype){ /*Skip Types that do not require angle*/
		case DO_IRIS_WIPE:
		case DO_CLOCK_WIPE:
			break;
			
		default:
			uiDefButF(block, NUM,B_SEQ_BUT_EFFECT,"Angle:",	10,15,220,22, &wipe->angle,-90.0,90.0, 1, 2, "The Angle of the Edge");
		}
		uiDefButS(block, TOG,B_SEQ_BUT_EFFECT,"Wipe In",  10,-10,220,22, &wipe->forward,0,0, 0, 0, "Controls Primary Direction of Wipe");				
	} else if(last_seq->type==SEQ_GLOW){
		GlowVars *glow = (GlowVars *)last_seq->effectdata;

		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "Threshold:", 	10,70,150,19, &glow->fMini, 0.0, 1.0, 0, 0, "Trigger Intensity");
		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "Clamp:",			10,50,150,19, &glow->fClamp, 0.0, 1.0, 0, 0, "Brightness limit of intensity");
		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "Boost factor:", 	10,30,150,19, &glow->fBoost, 0.0, 10.0, 0, 0, "Brightness multiplier");
		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "Blur distance:", 	10,10,150,19, &glow->dDist, 0.5, 20.0, 0, 0, "Radius of glow effect");
		uiDefButI(block, NUM, B_NOP, "Quality:", 10,-5,150,19, &glow->dQuality, 1.0, 5.0, 0, 0, "Accuracy of the blur effect");
		uiDefButI(block, TOG, B_NOP, "Only boost", 10,-25,150,19, &glow->bNoComp, 0.0, 0.0, 0, 0, "Show the glow buffer only");
	}
	else if(last_seq->type==SEQ_TRANSFORM){
		TransformVars *transform = (TransformVars *)last_seq->effectdata;

		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "xScale Start:", 	10,70,150,19, &transform->ScalexIni, 0.0, 10.0, 0, 0, "X Scale Start");
		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "xScale End:", 	160,70,150,19, &transform->ScalexFin, 0.0, 10.0, 0, 0, "X Scale End");
		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "yScale Start:",	10,50,150,19, &transform->ScaleyIni, 0.0, 10.0, 0, 0, "Y Scale Start");
		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "yScale End:", 	160,50,150,19, &transform->ScaleyFin, 0.0, 10.0, 0, 0, "Y Scale End");
		
		uiDefButI(block, ROW, B_SEQ_BUT_EFFECT, "Percent", 10, 30, 150, 19, &transform->percent, 0.0, 1.0, 0.0, 0.0, "Percent Translate");
		uiDefButI(block, ROW, B_SEQ_BUT_EFFECT, "Pixels", 160, 30, 150, 19, &transform->percent, 0.0, 0.0, 0.0, 0.0, "Pixels Translate");
		if(transform->percent==1){
			uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "x Start:", 	10,10,150,19, &transform->xIni, -500.0, 500.0, 0, 0, "X Position Start");
			uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "x End:", 	160,10,150,19, &transform->xFin, -500.0, 500.0, 0, 0, "X Position End");
			uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "y Start:", 	10,-10,150,19, &transform->yIni, -500.0, 500.0, 0, 0, "Y Position Start");
			uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "y End:", 	160,-10,150,19, &transform->yFin, -500.0, 500.0, 0, 0, "Y Position End");
		} else {
			uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "x Start:", 	10,10,150,19, &transform->xIni, -10000.0, 10000.0, 0, 0, "X Position Start");
			uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "x End:", 	160,10,150,19, &transform->xFin, -10000.0, 10000.0, 0, 0, "X Position End");
			uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "y Start:", 	10,-10,150,19, &transform->yIni, -10000.0, 10000.0, 0, 0, "Y Position Start");
			uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "y End:", 	160,-10,150,19, &transform->yFin, -10000.0, 10000.0, 0, 0, "Y Position End");
			
		}
		
		
		
		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "rot Start:",10,-30,150,19, &transform->rotIni, 0.0, 360.0, 0, 0, "Rotation Start");
		uiDefButF(block, NUM, B_SEQ_BUT_EFFECT, "rot End:",160,-30,150,19, &transform->rotFin, 0.0, 360.0, 0, 0, "Rotation End");
		
		uiDefButI(block, ROW, B_SEQ_BUT_EFFECT, "No Interpolat", 10, -50, 100, 19, &transform->interpolation, 0.0, 0.0, 0.0, 0.0, "No interpolation");
		uiDefButI(block, ROW, B_SEQ_BUT_EFFECT, "Bilinear", 101, -50, 100, 19, &transform->interpolation, 0.0, 1.0, 0.0, 0.0, "Bilinear interpolation");
		uiDefButI(block, ROW, B_SEQ_BUT_EFFECT, "Bicubic", 202, -50, 100, 19, &transform->interpolation, 0.0, 2.0, 0.0, 0.0, "Bicubic interpolation");
	} else if(last_seq->type==SEQ_COLOR) {
		SolidColorVars *colvars = (SolidColorVars *)last_seq->effectdata;
		uiDefButF(block, COL, B_SEQ_BUT_RELOAD, "",10,90,150,19, colvars->col, 0, 0, 0, 0, "");
	} else if(last_seq->type==SEQ_SPEED){
		SpeedControlVars *sp = 
			(SpeedControlVars *)last_seq->effectdata;
		
		uiDefButF(block, NUM, B_SEQ_BUT_RELOAD, "Global Speed:", 	10,70,150,19, &sp->globalSpeed, 0.0, 100.0, 0, 0, "Global Speed");
		
		uiDefButBitI(block, TOG, SEQ_SPEED_INTEGRATE,
			     B_SEQ_BUT_RELOAD, 
			     "IPO is velocity",
			     10,50,150,19, &sp->flags, 
			     0.0, 1.0, 0, 0, 
			     "Interpret the IPO value as a "
			     "velocity instead of a frame number");

		uiDefButBitI(block, TOG, SEQ_SPEED_BLEND,
			     B_SEQ_BUT_RELOAD, 
			     "Enable frame blending",
			     10,30,150,19, &sp->flags, 
			     0.0, 1.0, 0, 0, 
			     "Blend two frames into the "
			     "target for a smoother result");
		
		uiDefButBitI(block, TOG, SEQ_SPEED_COMPRESS_IPO_Y,
			     B_SEQ_BUT_RELOAD, 
			     "IPO value runs from [0..1]",
			     10,10,150,19, &sp->flags, 
			     0.0, 1.0, 0, 0, 
			     "Scale IPO value to get the "
			     "target frame number.");
	}

	uiBlockEndAlign(block);
}

static void seq_panel_proxy()
{
	Sequence *last_seq = get_last_seq();
	uiBlock *block;
	block = uiNewBlock(&curarea->uiblocks, "seq_panel_proxy", 
			   UI_EMBOSS, UI_HELV, curarea->win);

	if(uiNewPanel(curarea, block, "Proxy", "Sequencer", 
		      10, 230, 318, 204) == 0) return;

	uiBlockBeginAlign(block);

	uiDefButBitI(block, TOG, SEQ_USE_PROXY, 
		     B_SEQ_BUT_RELOAD, "Use Proxy", 
		     10,140,120,19, &last_seq->flag, 
		     0.0, 21.0, 100, 0, 
		     "Use a preview proxy for this strip");

	if (last_seq->flag & SEQ_USE_PROXY) {
		if (!last_seq->strip->proxy) {
			last_seq->strip->proxy = 
				MEM_callocN(sizeof(struct StripProxy),
					    "StripProxy");
		}

		uiDefButBitI(block, TOG, SEQ_USE_PROXY_CUSTOM_DIR, 
			     B_SEQ_BUT_RELOAD, "Custom Dir", 
			     130,140,120,19, &last_seq->flag, 
			     0.0, 21.0, 100, 0, 
			     "Use a custom directory to store data");

		if (last_seq->flag & SEQ_USE_PROXY_CUSTOM_DIR) {
			uiDefIconBut(block, BUT, B_SEQ_SEL_PROXY_DIR, 
				     ICON_FILESEL, 10, 120, 20, 20, 0, 0, 0, 0, 0, 
				     "Select the directory/name for "
				     "the proxy storage");

			uiDefBut(block, TEX, 
				 B_SEQ_BUT_RELOAD, "Dir: ", 
				 30,120,220,20, last_seq->strip->proxy->dir, 
				 0.0, 160.0, 100, 0, "");
		}
	}

	if (last_seq->flag & SEQ_USE_PROXY) {
		if (G.scene->r.size == 100) {
			uiDefBut(block, LABEL, 0, 
				 "Full render size selected, ",
				 10,100,240,19, 0, 0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, 
				 "so no proxy enabled!",
				 10,80,240,19, 0, 0, 0, 0, 0, "");
		} else if (last_seq->type != SEQ_MOVIE 
			   && last_seq->type != SEQ_IMAGE
			   && !(last_seq->flag & SEQ_USE_PROXY_CUSTOM_DIR)) {
			uiDefBut(block, LABEL, 0, 
				 "Cannot proxy this strip without ",
				 10,100,240,19, 0, 0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, 
				 "custom directory selection!",
				 10,80,240,19, 0, 0, 0, 0, 0, "");

		} else {
			uiDefBut(block, BUT, B_SEQ_BUT_REBUILD_PROXY, 
				 "Rebuild proxy",
				 10,100,240,19, 0, 0, 0, 0, 0, 
				 "Rebuild proxy for the "
				 "currently selected strip.");
		}
	}

	uiBlockEndAlign(block);
}


void sequencer_panels()
{
	Sequence *last_seq = get_last_seq();
	int panels = 0;
	int type;

	if(last_seq == NULL) {
		return;
	}
	
	type = last_seq->type;

	panels = SEQ_PANEL_EDITING;

	if (type == SEQ_MOVIE || type == SEQ_IMAGE || type == SEQ_SCENE
	    || type == SEQ_META) {
		panels |= SEQ_PANEL_INPUT | SEQ_PANEL_FILTER | SEQ_PANEL_PROXY;
	}

	if (type == SEQ_RAM_SOUND || type == SEQ_HD_SOUND) {
		panels |= SEQ_PANEL_FILTER | SEQ_PANEL_INPUT;
	}

	if (type == SEQ_PLUGIN || type >= SEQ_EFFECT) {
		panels |= SEQ_PANEL_EFFECT | SEQ_PANEL_PROXY;
	}

	if (panels & SEQ_PANEL_EDITING) {
		seq_panel_editing();
	}

	if (panels & SEQ_PANEL_INPUT) {
		seq_panel_input();
	}

	if (panels & SEQ_PANEL_FILTER) {
		if (type == SEQ_RAM_SOUND || type == SEQ_HD_SOUND) {
			seq_panel_filter_audio();
		} else {
			seq_panel_filter_video();
		}
	}

	if (panels & SEQ_PANEL_EFFECT) {
		seq_panel_effect();
	}

	if (panels & SEQ_PANEL_PROXY) {
		seq_panel_proxy();
	}
}

static void sel_proxy_dir(char *name)
{
	Sequence *last_seq = get_last_seq();
	strcpy(last_seq->strip->proxy->dir, name);

	allqueue(REDRAWBUTSSCENE, 0);

	BIF_undo_push("Change proxy directory");
}

void do_sequencer_panels(unsigned short event)
{
	Sequence *last_seq = get_last_seq();
	ScrArea * sa;

	if (!last_seq) {
		return;
	}

	switch(event) {
	case B_SEQ_BUT_PLUGIN:
	case B_SEQ_BUT_EFFECT:
		update_changed_seq_and_deps(last_seq, 0, 1);
		break;
	case B_SEQ_BUT_RELOAD_FILE:
		reload_sequence_new_file(last_seq);
		break;
	case B_SEQ_BUT_REBUILD_PROXY:
		seq_proxy_rebuild(last_seq);
		break;
	case B_SEQ_SEL_PROXY_DIR:
		sa= closest_bigger_area();
		areawinset(sa->win);
		activate_fileselect(FILE_SPECIAL, "SELECT PROXY DIR", 
				    last_seq->strip->proxy->dir, 
				    sel_proxy_dir);
		break;
	case B_SEQ_BUT_RELOAD:
	case B_SEQ_BUT_RELOAD_ALL:
		update_seq_ipo_rect(last_seq);
		update_seq_icu_rects(last_seq);

		free_imbuf_seq();	// frees all

		break;
	case B_SEQ_BUT_TRANSFORM:
		calc_sequence(last_seq);
		if (test_overlap_seq(last_seq))
			shuffle_seq(last_seq);
		break;
	}

	if (event == B_SEQ_BUT_RELOAD_ALL) {
		allqueue(REDRAWALL, 0);
	} else {
		allqueue(REDRAWSEQ, 0);
		allqueue(REDRAWBUTSSCENE, 0);
	}
}


/* ************************* SCENE *********************** */


static void output_pic(char *name)
{
	strcpy(G.scene->r.pic, name);
	allqueue(REDRAWBUTSSCENE, 0);
	BIF_undo_push("Change output picture directory");
}

static void backbuf_pic(char *name)
{
	Image *ima;
	
	strcpy(G.scene->r.backbuf, name);
	allqueue(REDRAWBUTSSCENE, 0);

	ima= BKE_add_image_file(name);
	if(ima)
		BKE_image_signal(ima, NULL, IMA_SIGNAL_RELOAD);

	BIF_undo_push("Change background picture");
}

static void run_playanim(char *file) 
{
	extern char bprogname[];	/* usiblender.c */
	char str[FILE_MAX*2]; /* FILE_MAX*2 is a bit arbitary, but this should roughly allow for the args + the max-file-length */
	int pos[2], size[2];

	/* use current settings for defining position of window. it actually should test image size */
	calc_renderwin_rectangle((G.scene->r.xsch*G.scene->r.size)/100, 
							 (G.scene->r.ysch*G.scene->r.size)/100, G.winpos, pos, size);
#ifdef WIN32
	sprintf(str, "%s -a -s %d -e %d -p %d %d -f %d %g \"%s\"", bprogname, G.scene->r.sfra, G.scene->r.efra, pos[0], pos[1], G.scene->r.frs_sec, G.scene->r.frs_sec_base, file);
#else
	sprintf(str, "\"%s\" -a -s %d -e %d  -p %d %d -f %d %g \"%s\"", bprogname, G.scene->r.sfra, G.scene->r.efra, pos[0], pos[1], G.scene->r.frs_sec, G.scene->r.frs_sec_base, file);
#endif
	system(str);
}

void playback_anim(void)
{	
	char file[FILE_MAX];

	if(BKE_imtype_is_movie(G.scene->r.imtype)) {
		switch (G.scene->r.imtype) {
#ifdef WITH_QUICKTIME
			case R_QUICKTIME:
				makeqtstring(file);
				break;
#endif
#ifdef WITH_FFMPEG
		case R_FFMPEG:
			makeffmpegstring(file);
			break;
#endif
		default:
			makeavistring(&G.scene->r, file);
			break;
		}
		if(BLI_exist(file)) {
			run_playanim(file);
		}
		else error("Can't find movie: %s", file);
	}
	else {
		BKE_makepicstring(file, G.scene->r.pic, G.scene->r.sfra, G.scene->r.imtype);
		if(BLI_exist(file)) {
			run_playanim(file);
		}
		else error("Can't find image: %s", file);
	}
}

#ifdef WITH_FFMPEG
static void set_ffmpeg_preset(int preset);
static int ffmpeg_property_add_string(const char * type, const char * str);
static char ffmpeg_option_to_add[255] = "";
#endif

void do_render_panels(unsigned short event)
{
	ScrArea *sa;
	ID *id;

	switch(event) {

	case B_DORENDER:
		BIF_do_render(0);
		break;
	case B_RTCHANGED:
		allqueue(REDRAWALL, 0);
		break;
	case B_SWITCHRENDER:
		/* new panels added, so... */
		G.buts->re_align= 1;
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PLAYANIM:
		playback_anim();
		break;
		
	case B_DOANIM:
		BIF_do_render(1);
		break;
	
	case B_FS_PIC:
		sa= closest_bigger_area();
		areawinset(sa->win);
		if(G.qual == LR_CTRLKEY)
			activate_imageselect(FILE_SPECIAL, "SELECT OUTPUT PICTURES", G.scene->r.pic, output_pic);
		else
			activate_fileselect(FILE_SPECIAL, "SELECT OUTPUT PICTURES", G.scene->r.pic, output_pic);
		break;

	case B_FS_BACKBUF:
		sa= closest_bigger_area();
		areawinset(sa->win);
		if(G.qual == LR_CTRLKEY)
			activate_imageselect(FILE_SPECIAL, "SELECT BACKBUF PICTURE", G.scene->r.backbuf, backbuf_pic);
		else
			activate_fileselect(FILE_SPECIAL, "SELECT BACKBUF PICTURE", G.scene->r.backbuf, backbuf_pic);
		break;
	
	case B_PR_PAL:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 54;
		G.scene->r.yasp= 51;
		G.scene->r.size= 100;
		G.scene->r.frs_sec= 25;
		G.scene->r.frs_sec_base= 1;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 4;
#ifdef WITH_FFMPEG
		G.scene->r.ffcodecdata.gop_size = 15;
#endif		
		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		BIF_undo_push("Set PAL");
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEWCAM, 0);
		break;

	case B_FILETYPEMENU:
		allqueue(REDRAWBUTSSCENE, 0);
#ifdef WITH_FFMPEG
                if (G.scene->r.imtype == R_FFMPEG) {
			if (G.scene->r.ffcodecdata.type <= 0 ||
			    G.scene->r.ffcodecdata.codec <= 0 ||
			    G.scene->r.ffcodecdata.audio_codec <= 0 ||
			    G.scene->r.ffcodecdata.video_bitrate <= 1) {
				G.scene->r.ffcodecdata.codec 
					= CODEC_ID_MPEG2VIDEO;
				set_ffmpeg_preset(FFMPEG_PRESET_DVD);
			}

			if (G.scene->r.ffcodecdata.audio_codec <= 0) {
				G.scene->r.ffcodecdata.audio_codec 
					= CODEC_ID_MP2;
				G.scene->r.ffcodecdata.audio_bitrate = 128;
			}
			break;
                }
#endif
#if defined (_WIN32) || defined (__APPLE__)
		// fall through to codec settings if this is the first
		// time R_AVICODEC is selected for this scene.
		if (((G.scene->r.imtype == R_AVICODEC) 
			 && (G.scene->r.avicodecdata == NULL)) ||
			((G.scene->r.imtype == R_QUICKTIME) 
			 && (G.scene->r.qtcodecdata == NULL))) {
		} else {
		  break;
		}
#endif /*_WIN32 || __APPLE__ */

	case B_SELECTCODEC:
#if defined (_WIN32) || defined (__APPLE__)
		if ((G.scene->r.imtype == R_QUICKTIME)) { /* || (G.scene->r.qtcodecdata)) */
#ifdef WITH_QUICKTIME
			get_qtcodec_settings();
#endif /* WITH_QUICKTIME */
		}
#if defined (_WIN32) && !defined(FREE_WINDOWS)
		else
			get_avicodec_settings();
#endif /* _WIN32 && !FREE_WINDOWS */
#endif /* _WIN32 || __APPLE__ */
		break;

	case B_PR_HD:
		G.scene->r.xsch= 1920;
		G.scene->r.ysch= 1080;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 4;
		
		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		BIF_undo_push("Set FULL");
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEWCAM, 0);
		break;
	case B_PR_FULL:
		G.scene->r.xsch= 1280;
		G.scene->r.ysch= 1024;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 4;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		BIF_undo_push("Set FULL");
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEWCAM, 0);
		break;
	case B_PR_PRV:
		G.scene->r.xsch= 640;
		G.scene->r.ysch= 512;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 50;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 2;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_PAL169:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 64;
		G.scene->r.yasp= 45;
		G.scene->r.size= 100;
		G.scene->r.frs_sec= 25;
		G.scene->r.frs_sec_base= 1;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 4;
#ifdef WITH_FFMPEG
		G.scene->r.ffcodecdata.gop_size = 15;
#endif		

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		BIF_undo_push("Set PAL 16/9");
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_PC:
		G.scene->r.xsch= 640;
		G.scene->r.ysch= 480;
		G.scene->r.xasp= 100;
		G.scene->r.yasp= 100;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 4;

		BLI_init_rctf(&G.scene->r.safety, 0.0, 1.0, 0.0, 1.0);
		BIF_undo_push("Set PC");
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_PRESET:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 54;
		G.scene->r.yasp= 51;
		G.scene->r.size= 100;
		G.scene->r.mode= R_OSA+R_SHADOW+R_FIELDS+R_SSS;
		G.scene->r.imtype= R_TARGA;
		G.scene->r.xparts=  G.scene->r.yparts= 4;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		BIF_undo_push("Set Default");
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_PANO:
		G.scene->r.xsch= 576;
		G.scene->r.ysch= 176;
		G.scene->r.xasp= 115;
		G.scene->r.yasp= 100;
		G.scene->r.size= 100;
		G.scene->r.mode |= R_PANORAMA;
		G.scene->r.xparts=  16;
		G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		BIF_undo_push("Set Panorama");
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_NTSC:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 480;
		G.scene->r.xasp= 10;
		G.scene->r.yasp= 11;
		G.scene->r.size= 100;
		G.scene->r.frs_sec= 30;
		G.scene->r.frs_sec_base = 1.001;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 2;
#ifdef WITH_FFMPEG
		G.scene->r.ffcodecdata.gop_size = 18;
#endif		
		
		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		BIF_undo_push("Set NTSC");
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEWCAM, 0);
		break;

	case B_SETBROWSE:
		id= (ID*) G.scene->set;
		
		if (G.buts->menunr==-2) {
			 activate_databrowse(id, ID_SCE, 0, B_SETBROWSE, &G.buts->menunr, do_render_panels);
		} 
		else if (G.buts->menunr>0) {
			Scene *newset= (Scene*) BLI_findlink(&G.main->scene, G.buts->menunr-1);
			
			if (newset==G.scene)
				error("Can't use the same scene as its own set");
			else if (newset) {
				G.scene->set= newset;
				if (scene_check_setscene(G.scene)==0)
					error("This would create a cycle");

				allqueue(REDRAWBUTSSCENE, 0);
				allqueue(REDRAWVIEW3D, 0);
				BIF_undo_push("Change Set Scene");
			}
		}  
		break;
	case B_CLEARSET:
		G.scene->set= NULL;
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Clear Set Scene");
		
		break;
	case B_SET_EDGE:
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_SET_ZBLUR:
		G.scene->r.mode &= ~R_EDGE;
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_ADD_RENDERLAYER:
		if(G.scene->r.actlay==32767) {
			scene_add_render_layer(G.scene);
			G.scene->r.actlay= BLI_countlist(&G.scene->r.layers) - 1;
		}
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWNODE, 0);
		break;
	case B_SET_PASS:
		if(G.scene->nodetree) {
			ntreeCompositForceHidden(G.scene->nodetree);
			allqueue(REDRAWNODE, 0);
		}
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWOOPS, 0);
#ifdef WITH_FFMPEG
	case B_ADD_FFMPEG_AUDIO_OPTION:
		if (ffmpeg_property_add_string("audio", ffmpeg_option_to_add)){
			*ffmpeg_option_to_add = 0;
		}
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_ADD_FFMPEG_VIDEO_OPTION:
		if (ffmpeg_property_add_string("video", ffmpeg_option_to_add)){
			*ffmpeg_option_to_add = 0;
		}
		allqueue(REDRAWBUTSSCENE, 0);
		break;
#endif
	}
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *edge_render_menu(void *arg_unused)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "edge render", UI_EMBOSS, UI_HELV, curarea->win);
		
	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",  0, 0, 220, 115, NULL,  0, 0, 0, 0, "");
	
	uiDefButS(block, NUM, 0,"Eint:",  	45,75,175,19,  &G.scene->r.edgeint, 0.0, 255.0, 0, 0,
		  "Sets edge intensity for Toon shading");

	/* color settings for the toon shading */
	uiDefButF(block, COL, 0, "", 		10, 10,30,60,  &(G.scene->r.edgeR), 0, 0, 0, B_EDGECOLSLI, "");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, 0, "R ",   45, 50, 175,19,   &G.scene->r.edgeR, 0.0, 1.0, B_EDGECOLSLI, 0,
		  "Color for edges in toon shading mode.");
	uiDefButF(block, NUMSLI, 0, "G ",  	45, 30, 175,19,  &G.scene->r.edgeG, 0.0, 1.0, B_EDGECOLSLI, 0,
		  "Color for edges in toon shading mode.");
	uiDefButF(block, NUMSLI, 0, "B ",  	45, 10, 175,19,  &G.scene->r.edgeB, 0.0, 1.0, B_EDGECOLSLI, 0,
		  "Color for edges in toon shading mode.");

	
	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}


/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *framing_render_menu(void *arg_unused)
{
	uiBlock *block;
	short yco = 190, xco = 0;
	int randomcolorindex = 1234;

	block= uiNewBlock(&curarea->uiblocks, "framing_options", UI_EMBOSS, UI_HELV, curarea->win);

	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-5, -10, 295, 224, NULL, 0, 0, 0, 0, "");

	uiDefBut(block, LABEL, 0, "Framing:", xco, yco, 68,19, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButC(block, ROW, 0, "Stretch",	xco += 70, yco, 68, 19, &G.scene->framing.type, 1.0, SCE_GAMEFRAMING_SCALE , 0, 0, "Stretch or squeeze the viewport to fill the display window");
	uiDefButC(block, ROW, 0, "Expose",	xco += 70, yco, 68, 19, &G.scene->framing.type, 1.0, SCE_GAMEFRAMING_EXTEND, 0, 0, "Show the entire viewport in the display window, viewing more horizontally or vertically");
	uiDefButC(block, ROW, 0, "Letterbox",	    xco += 70, yco, 68, 19, &G.scene->framing.type, 1.0, SCE_GAMEFRAMING_BARS  , 0, 0, "Show the entire viewport in the display window, using bar horizontally or vertically");
	uiBlockEndAlign(block);

	yco -= 25;
	xco = 40;

	uiDefButF(block, COL, 0, "",                0, yco - 58 + 18, 33, 58, &G.scene->framing.col[0], 0, 0, 0, randomcolorindex, "");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, 0, "R ", xco,yco,243,18, &G.scene->framing.col[0], 0.0, 1.0, randomcolorindex, 0, "Set the red component of the bars");
	yco -= 20;
	uiDefButF(block, NUMSLI, 0, "G ", xco,yco,243,18, &G.scene->framing.col[1], 0.0, 1.0, randomcolorindex, 0, "Set the green component of the bars");
	yco -= 20;
	uiDefButF(block, NUMSLI, 0, "B ", xco,yco,243,18, &G.scene->framing.col[2], 0.0, 1.0, randomcolorindex, 0, "Set the blue component of the bars");
	uiBlockEndAlign(block);
	
	xco = 0;
	uiDefBut(block, LABEL, 0, "Fullscreen:",		xco, yco-=30, 100, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, TOG, 0, "Fullscreen", xco+70, yco, 68, 19, &G.scene->r.fullscreen, 0.0, 0.0, 0, 0, "Starts player in a new fullscreen display");
	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, 0, "X:",		xco+40, yco-=27, 100, 19, &G.scene->r.xplay, 10.0, 2000.0, 0, 0, "Displays current X screen/window resolution. Click to change.");
	uiDefButS(block, NUM, 0, "Y:",		xco+140, yco, 100, 19, &G.scene->r.yplay,    10.0, 2000.0, 0, 0, "Displays current Y screen/window resolution. Click to change.");
	uiDefButS(block, NUM, 0, "Freq:",	xco+40, yco-=21, 100, 19, &G.scene->r.freqplay, 10.0, 2000.0, 0, 0, "Displays clock frequency of fullscreen display. Click to change.");
	uiDefButS(block, NUM, 0, "Bits:",	xco+140, yco, 100, 19, &G.scene->r.depth, 8.0, 32.0, 800.0, 0, "Displays bit depth of full screen display. Click to change.");
	uiBlockEndAlign(block);

	/* stereo settings */
	/* can't use any definition from the game engine here so hardcode it. Change it here when it changes there!
	 * RAS_IRasterizer has definitions:
	 * RAS_STEREO_NOSTEREO		 1
	 * RAS_STEREO_QUADBUFFERED 2
	 * RAS_STEREO_ABOVEBELOW	 3
	 * RAS_STEREO_INTERLACED	 4	 future
	 * RAS_STEREO_ANAGLYPH		5
	 * RAS_STEREO_SIDEBYSIDE	6
	 * RAS_STEREO_VINTERLACE	7
	 */
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, 0, "No Stereo", xco, yco-=30, 88, 19, &(G.scene->r.stereomode), 7.0, 1.0, 0, 0, "Disables stereo");
	uiDefButS(block, ROW, 0, "Pageflip", xco+=90, yco, 88, 19, &(G.scene->r.stereomode), 7.0, 2.0, 0, 0, "Enables hardware pageflip stereo method");
	uiDefButS(block, ROW, 0, "Syncdouble", xco+=90, yco, 88, 19, &(G.scene->r.stereomode), 7.0, 3.0, 0, 0, "Enables syncdoubling stereo method");
	uiDefButS(block, ROW, 0, "Anaglyph", xco-=180, yco-=21, 88, 19, &(G.scene->r.stereomode), 7.0, 5.0, 0, 0, "Enables anaglyph (Red-Blue) stereo method");
	uiDefButS(block, ROW, 0, "Side by Side", xco+=90, yco, 88, 19, &(G.scene->r.stereomode), 7.0, 6.0, 0, 0, "Enables side by side left and right images");
	uiDefButS(block, ROW, 0, "V Interlace", xco+=90, yco, 88, 19, &(G.scene->r.stereomode), 7.0, 7.0, 0, 0, "Enables interlaced vertical strips for autostereo display");
	
	uiBlockEndAlign(block);

	uiBlockSetDirection(block, UI_TOP);

	return block;
}

#ifdef WITH_FFMPEG

static char* ffmpeg_format_pup(void) 
{
	static char string[2048];
	char formatstring[2048];
#if 0
       int i = 0;
       int stroffs = 0;
       AVOutputFormat* next = first_oformat;
       formatstring = "FFMpeg format: %%t";
      sprintf(string, formatstring);
       formatstring = "|%s %%x%d";
       /* FIXME: This should only be generated once */
       while (next != NULL) {
               if (next->video_codec != CODEC_ID_NONE && !(next->flags & AVFMT_NOFILE)) {
                       sprintf(string+stroffs, formatstring, next->name, i++);
                       stroffs += strlen(string+stroffs);
               }
               next = next->next;
       }
       return string;
#endif
       strcpy(formatstring, "FFMpeg format: %%t|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d");
       sprintf(string, formatstring,
               "MPEG-1", FFMPEG_MPEG1,
               "MPEG-2", FFMPEG_MPEG2,
               "MPEG-4", FFMPEG_MPEG4,
               "AVI",    FFMPEG_AVI,
               "Quicktime", FFMPEG_MOV,
               "DV", FFMPEG_DV,
	       "H264", FFMPEG_H264,
	       "XVid", FFMPEG_XVID,
	       "FLV", FFMPEG_FLV);
       return string;
}

static char* ffmpeg_preset_pup(void) 
{
	static char string[2048];
	char formatstring[2048];

       strcpy(formatstring, "FFMpeg preset: %%t|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d");
       sprintf(string, formatstring,
               "", FFMPEG_PRESET_NONE,
               "DVD", FFMPEG_PRESET_DVD,
               "SVCD", FFMPEG_PRESET_SVCD,
               "VCD", FFMPEG_PRESET_VCD,
               "DV", FFMPEG_PRESET_DV,
	       "H264", FFMPEG_PRESET_H264);
       return string;
}


static char* ffmpeg_codec_pup(void) {
       static char string[2048];
       char formatstring[2048];
       strcpy(formatstring, "FFMpeg format: %%t|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d");
       sprintf(string, formatstring,
               "MPEG1", CODEC_ID_MPEG1VIDEO,
               "MPEG2", CODEC_ID_MPEG2VIDEO,
               "MPEG4(divx)", CODEC_ID_MPEG4,
               "HuffYUV", CODEC_ID_HUFFYUV,
	       "DV", CODEC_ID_DVVIDEO,
               "H264", CODEC_ID_H264,
	       "XVid", CODEC_ID_XVID,
	       "FlashVideo1", CODEC_ID_FLV1 );
       return string;

}

static char* ffmpeg_audio_codec_pup(void) {
       static char string[2048];
       char formatstring[2048];
       strcpy(formatstring, "FFMpeg format: %%t|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d");
       sprintf(string, formatstring,
               "MP2", CODEC_ID_MP2,
               "MP3", CODEC_ID_MP3,
               "AC3", CODEC_ID_AC3,
               "AAC", CODEC_ID_AAC,
	       "PCM", CODEC_ID_PCM_S16LE);
       return string;

}

#endif

static char *imagetype_pup(void)
{
	static char string[1024];
	char formatstring[1024];
	char appendstring[1024];

	strcpy(formatstring, "Save image as: %%t|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d");

#ifdef __sgi
	strcat(formatstring, "|%s %%x%d");	// add space for Movie
#endif

	strcat(formatstring, "|%s %%x%d");	// add space for PNG
/*  Commented out until implemented
#ifdef WITH_DDS
	strcat(formatstring, "|%s %%x%d");	// add space for DDS
#endif
*/
	strcat(formatstring, "|%s %%x%d");	// add space for BMP
	strcat(formatstring, "|%s %%x%d");	// add space for Radiance HDR
	strcat(formatstring, "|%s %%x%d");	// add space for Cineon
	strcat(formatstring, "|%s %%x%d");	// add space for DPX
	
#ifdef _WIN32
	strcat(formatstring, "|%s %%x%d");	// add space for AVI Codec
#endif

#ifdef WITH_FFMPEG
       strcat(formatstring, "|%s %%x%d"); // Add space for ffmpeg
#endif
       strcat(formatstring, "|%s %%x%d"); // Add space for frameserver

#ifdef WITH_QUICKTIME
	if(G.have_quicktime)
		strcat(formatstring, "|%s %%x%d");	// add space for Quicktime
#endif

	if(G.have_quicktime) {
		sprintf(string, formatstring,
			"Frameserver",   R_FRAMESERVER,
#ifdef WITH_FFMPEG
                       "FFMpeg",         R_FFMPEG,
#endif
			"AVI Raw",        R_AVIRAW,
			"AVI Jpeg",       R_AVIJPEG,
#ifdef _WIN32
			"AVI Codec",      R_AVICODEC,
#endif
#ifdef WITH_QUICKTIME
			"QuickTime",      R_QUICKTIME,
#endif
			"Targa",          R_TARGA,
			"Targa Raw",      R_RAWTGA,
			"PNG",            R_PNG,
/* commented out until implemented 
#ifdef WITH_DDS
			"DDS",            R_DDS,
#endif
*/
			"BMP",            R_BMP,
			"Jpeg",           R_JPEG90,
			"HamX",           R_HAMX,
			"Iris",           R_IRIS,
			"Radiance HDR",   R_RADHDR,
			"Cineon",		  R_CINEON,
			"DPX",			  R_DPX
#ifdef __sgi
			,"Movie",          R_MOVIE
#endif
		);
	} else {
		sprintf(string, formatstring,
			"Frameserver",   R_FRAMESERVER,
#ifdef WITH_FFMPEG
                       "FFMpeg",         R_FFMPEG,
#endif
			"AVI Raw",        R_AVIRAW,
			"AVI Jpeg",       R_AVIJPEG,
#ifdef _WIN32
			"AVI Codec",      R_AVICODEC,
#endif
			"Targa",          R_TARGA,
			"Targa Raw",      R_RAWTGA,
			"PNG",            R_PNG,
/*#ifdef WITH_DDS
			"DDS",            R_DDS,
#endif*/
			"BMP",            R_BMP,
			"Jpeg",           R_JPEG90,
			"HamX",           R_HAMX,
			"Iris",           R_IRIS,
			"Radiance HDR",   R_RADHDR,
			"Cineon",		  R_CINEON,
			"DPX",			  R_DPX
#ifdef __sgi
			,"Movie",          R_MOVIE
#endif
		);
	}

#ifdef WITH_OPENEXR
	strcpy(formatstring, "|%s %%x%d");
	sprintf(appendstring, formatstring, "OpenEXR", R_OPENEXR);
	strcat(string, appendstring);
	sprintf(appendstring, formatstring, "MultiLayer", R_MULTILAYER);
	strcat(string, appendstring);
#endif
	
	if (G.have_libtiff) {
		strcpy(formatstring, "|%s %%x%d");
		sprintf(appendstring, formatstring, "TIFF", R_TIFF);
		strcat(string, appendstring);
	}

	return (string);
}

#ifdef _WIN32
static char *avicodec_str(void)
{
	static char string[1024];

	sprintf(string, "Codec: %s", G.scene->r.avicodecdata->avicodecname);

	return string;
}
#endif

static void render_panel_output(void)
{
	ID *id;
	int a,b;
	uiBlock *block;
	char *strp;

	block= uiNewBlock(&curarea->uiblocks, "render_panel_output", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Output", "Render", 0, 0, 318, 204)==0) return;
	
	uiBlockBeginAlign(block);
	uiDefIconBut(block, BUT, B_FS_PIC, ICON_FILESEL,	10, 190, 20, 20, 0, 0, 0, 0, 0, "Select the directory/name for saving animations");
	uiDefBut(block, TEX,0,"",							31, 190, 279, 20,G.scene->r.pic, 0.0,79.0, 0, 0, "Directory/name to save animations, # characters defines the position and length of frame numbers");
	uiDefIconBut(block, BUT,B_FS_BACKBUF, ICON_FILESEL, 10, 168, 20, 20, 0, 0, 0, 0, 0, "Select the directory/name for a Backbuf image");
	uiDefBut(block, TEX,0,"",							31, 168, 259, 20,G.scene->r.backbuf, 0.0,79.0, 0, 0, "Image to use as background for rendering");
	uiDefIconButBitS(block, ICONTOG, R_BACKBUF, B_NOP, ICON_CHECKBOX_HLT-1,	290, 168, 20, 20, &G.scene->r.bufflag, 0.0, 0.0, 0, 0, "Enable/Disable use of Backbuf image");
	uiBlockEndAlign(block);
	
	uiDefButBitI(block, TOG, R_EXTENSION, B_NOP, "Extensions", 10, 142, 100, 20, &G.scene->r.scemode, 0.0, 0.0, 0, 0, "Adds filetype extensions to the filename when rendering animations");
	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_TOUCH, B_NOP, "Touch",	170, 142, 50, 20, &G.scene->r.mode, 0.0, 0.0, 0, 0, "Create an empty file before rendering each frame, remove if cancelled (and empty)");
	uiDefButBitI(block, TOG, R_NO_OVERWRITE, B_NOP, "No Overwrite", 220, 142, 90, 20, &G.scene->r.mode, 0.0, 0.0, 0, 0, "Skip rendering frames when the file exists (image output only)");
	uiBlockEndAlign(block);
	
	/* SET BUTTON */
	uiBlockBeginAlign(block);
	id= (ID *)G.scene->set;
	IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->scene), id, &(G.buts->menunr));
	if(strp[0])
		uiDefButS(block, MENU, B_SETBROWSE, strp,			10, 114, 20, 20, &(G.buts->menunr), 0, 0, 0, 0, "Scene to link as a Set");
	MEM_freeN(strp);

	if(G.scene->set) {
		uiSetButLock(1, NULL);
		uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, B_NOP, "",	31, 114, 100, 20, &(G.scene->set), "Name of the Set");
		uiClearButLock();
		uiDefIconBut(block, BUT, B_CLEARSET, ICON_X, 		132, 114, 20, 20, 0, 0, 0, 0, 0, "Remove Set link");
	} else {
		uiDefBut(block, LABEL, 0, "No Set Scene", 31, 114, 200, 20, 0, 0, 0, 0, 0, "");
	}
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefIconButBitI(block, TOGN, R_FIXED_THREADS, B_REDR, ICON_AUTO,	10, 63, 20, 20, &G.scene->r.mode, 0.0, 0.0, 0, 0, "Automatically set the threads to the number of processors on the system");
	if ((G.scene->r.mode & R_FIXED_THREADS)==0) {
		char thread_str[16];
		sprintf(thread_str, " Threads: %d", BLI_system_thread_count());
		uiDefBut(block, LABEL, 0, thread_str, 30, 63,80,20, 0, 0, 0, 0, 0, "");
	} else {
		uiDefButS(block, NUM, B_NOP, "Threads:", 30, 63, 80, 20, &G.scene->r.threads, 1, BLENDER_MAX_THREADS, 0, 0, "Amount of threads for render (takes advantage of multi-core and multi-processor computers)");
	}
	uiBlockEndAlign(block);
	
	uiBlockSetCol(block, TH_AUTO);
		
	uiBlockBeginAlign(block);
	for(b=2; b>=0; b--)
		for(a=0; a<3; a++)
			uiDefButBitS(block, TOG, 1<<(3*b+a), 800,"",	(short)(10+18*a),(short)(10+14*b),16,12, &G.winpos, 0, 0, 0, 0, "Render window placement on screen");
	uiBlockEndAlign(block);

#ifdef WITH_OPENEXR
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_EXR_TILE_FILE, B_REDR, "Save Buffers", 72, 31, 120, 19, &G.scene->r.scemode, 0.0, 0.0, 0, 0, "Save tiles for all RenderLayers and used SceneNodes to files in the temp directory (saves memory, allows Full Sampling)");
	if(G.scene->r.scemode & R_EXR_TILE_FILE)
		uiDefButBitI(block, TOG, R_FULL_SAMPLE, B_REDR, "FullSample",	 192, 31, 118, 19, &G.scene->r.scemode, 0.0, 0.0, 0, 0, "Saves for every OSA sample the entire RenderLayer results (Higher quality sampling but slower)");
	uiBlockEndAlign(block);
#endif
	
	uiDefButS(block, MENU, B_REDR, "Render Display %t|Render Window %x1|Image Editor %x0|Full Screen %x2",	
					72, 10, 120, 19, &G.displaymode, 0.0, (float)R_DISPLAYWIN, 0, 0, "Sets render output display");
	
	/* Dither control */
	uiDefButF(block, NUM,B_DIFF, "Dither:",         10,89,100,19, &G.scene->r.dither_intensity, 0.0, 2.0, 0, 0, "The amount of dithering noise present in the output image (0.0 = no dithering)");
	
	/* Toon shading buttons */
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_EDGE, B_NOP,"Edge",   115, 89, 60, 20, &G.scene->r.mode, 0, 0, 0, 0, "Enable Toon Edge-enhance");
	uiDefBlockBut(block, edge_render_menu, NULL, "Edge Settings", 175, 89, 135, 20, "Display Edge settings");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_NO_TEX, B_NOP, "Disable Tex", 115, 63, 75, 20, &G.scene->r.scemode, 0.0, 0.0, 0, 0, "Disables Textures for render");
	uiDefButBitI(block, TOG, R_FREE_IMAGE, B_NOP, "Free Tex Images", 210, 63, 100, 20, &G.scene->r.scemode, 0.0, 0.0, 0, 0, "Frees all Images used by Textures after each render");
	uiBlockEndAlign(block);
}

static void do_bake_func(void *unused_v, void *unused_p)
{
	objects_bake_render_ui(0);
}

static void render_panel_bake(void)
{
	uiBlock *block;
	uiBut *but;
	
	block= uiNewBlock(&curarea->uiblocks, "render_panel_bake", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Anim", "Render");
	if(uiNewPanel(curarea, block, "Bake", "Render", 320, 0, 318, 204)==0) return;
	
	but= uiDefBut(block, BUT, B_NOP, "BAKE",	10, 150, 190,40, 0, 0, 0, 0, 0, "Start the bake render for selected Objects");
	uiButSetFunc(but, do_bake_func, NULL, NULL);

	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, R_BAKE_TO_ACTIVE, B_DIFF, "Selected to Active", 10,120,190,20,&G.scene->r.bake_flag, 0.0, 0, 0, 0, "Bake shading on the surface of selected objects to the active object");
	uiDefButF(block, NUM, B_DIFF, "Dist:", 10,100,95,20,&G.scene->r.bake_maxdist, 0.0, 1000.0, 1, 0, "Maximum distance from active object to other object (in blender units)");
	uiDefButF(block, NUM, B_DIFF, "Bias:", 105,100,95,20,&G.scene->r.bake_biasdist, 0.0, 1000.0, 1, 0, "Bias towards faces further away from the object (in blender units)");
	uiBlockEndAlign(block);

	if(G.scene->r.bake_mode == RE_BAKE_NORMALS)
		uiDefButS(block, MENU, B_DIFF, "Normal Space %t|Camera %x0|World %x1|Object %x2|Tangent %x3", 
			10,70,190,20, &G.scene->r.bake_normal_space, 0, 0, 0, 0, "Choose normal space for baking");
	else if(G.scene->r.bake_mode == RE_BAKE_AO || G.scene->r.bake_mode == RE_BAKE_DISPLACEMENT) {
		uiDefButBitS(block, TOG, R_BAKE_NORMALIZE, B_DIFF, "Normalized", 10,70,190,20, &G.scene->r.bake_flag, 0.0, 0, 0, 0,
				G.scene->r.bake_mode == RE_BAKE_AO ?
				 "Bake ambient occlusion normalized, without taking into acount material settings":
				 "Normalized displacement value to fit the 'Dist' range"
		);
	}
	
	uiDefButS(block, MENU, B_NOP, "Quad Split Order%t|Quad Split Auto%x0|Quad Split A (0,1,2) (0,2,3)%x1|Quad Split B (1,2,3) (1,3,0)%x2", 
		10,10,190,20, &G.scene->r.bake_quad_split, 0, 0, 0, 0, "Method to divide quads (use A or B for external applications that use a fixed order)");
	
#if 0	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, R_BAKE_OSA, B_DIFF, "OSA",		10,120,190,20, &G.scene->r.bake_flag, 0, 0, 0, 0, "Enables Oversampling (Anti-aliasing)");
	uiDefButS(block, ROW,B_DIFF,"5",			10,100,50,20,&G.scene->r.bake_osa,2.0,5.0, 0, 0, "Sets oversample level to 5");
	uiDefButS(block, ROW,B_DIFF,"8",			60,100,45,20,&G.scene->r.bake_osa,2.0,8.0, 0, 0, "Sets oversample level to 8");
	uiDefButS(block, ROW,B_DIFF,"11",			105,100,45,20,&G.scene->r.bake_osa,2.0,11.0, 0, 0, "Sets oversample level to 11");
	uiDefButS(block, ROW,B_DIFF,"16",			150,100,50,20,&G.scene->r.bake_osa,2.0,16.0, 0, 0, "Sets oversample level to 16");
#endif	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_REDR,"Full Render",		210,170,120,20,&G.scene->r.bake_mode, 1.0, RE_BAKE_ALL, 0, 0, "");
	uiDefButS(block, ROW,B_REDR,"Ambient Occlusion",210,150,120,20,&G.scene->r.bake_mode, 1.0, RE_BAKE_AO, 0, 0, "");
	uiDefButS(block, ROW,B_REDR,"Shadow",			210,130,120,20,&G.scene->r.bake_mode, 1.0, RE_BAKE_SHADOW, 0, 0, "");
	uiDefButS(block, ROW,B_REDR,"Normals",			210,110,120,20,&G.scene->r.bake_mode, 1.0, RE_BAKE_NORMALS, 0, 0, "");
	uiDefButS(block, ROW,B_REDR,"Textures",			210,90,120,20,&G.scene->r.bake_mode, 1.0, RE_BAKE_TEXTURE, 0, 0, "");
	uiDefButS(block, ROW,B_REDR,"Displacement",		210,70,120,20,&G.scene->r.bake_mode, 1.0, RE_BAKE_DISPLACEMENT, 0, 0, "");
	uiBlockEndAlign(block);
	
	uiDefButBitS(block, TOG, R_BAKE_CLEAR, B_DIFF, "Clear",		210,40,120,20,&G.scene->r.bake_flag, 0.0, 0, 0, 0, "Clear Images before baking");
	
	uiDefButS(block, NUM, B_DIFF,"Margin:",				210,10,120,20,&G.scene->r.bake_filter, 0.0, 32.0, 0, 0, "Amount of pixels to extend the baked result with, as post process filter");
}

static void render_panel_simplify(void)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "render_panel_simplify", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Render", "Render");
	if(uiNewPanel(curarea, block, "Simplifcation", "Render", 320, 0, 318, 204)==0) return;

	uiDefButBitI(block, TOG, R_SIMPLIFY, B_DIFF,"Render Simplification",	10,150,190,20, &G.scene->r.mode, 0, 0, 0, 0, "Enable simplification of scene");

	uiBlockBeginAlign(block);
	uiDefButI(block, NUM,B_DIFF, "Subsurf:",	10,120,190,20, &G.scene->r.simplify_subsurf, 0.0, 6.0, 0, 0, "Global maximum subsurf level percentage");
	uiDefButF(block, NUM,B_DIFF, "Child Particles:",	10,100,190,20, &G.scene->r.simplify_particles, 0.0, 1.0, 0, 0, "Global child particle percentage");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButI(block, NUM,B_DIFF, "Shadow Samples:",	10,70,190,20, &G.scene->r.simplify_shadowsamples, 1.0, 16.0, 0, 0, "Global maximum shadow map samples");
	uiDefButF(block, NUM,B_DIFF, "AO and SSS:",	10,50,190,20, &G.scene->r.simplify_aosss, 0.0, 1.0, 0, 0, "Global approximate AO and SSS quality factor");
	uiBlockEndAlign(block);
}

static void render_panel_render(void)
{
	uiBlock *block;
	char str[256];

	block= uiNewBlock(&curarea->uiblocks, "render_panel_render", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Render", "Render", 320, 0, 318, 204)==0) return;

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_DORENDER,"RENDER",	369, 164, 191,37, 0, 0, 0, 0, 0, "Render the current frame (F12)");
#ifndef DISABLE_YAFRAY
	/* yafray: on request, render engine menu is back again, and moved to Render panel */
	uiDefButS(block, MENU, B_SWITCHRENDER, "Rendering Engine %t|Blender Internal %x0|YafRay %x1", 
												369, 142, 191, 20, &G.scene->r.renderer, 0, 0, 0, 0, "Choose rendering engine");	
#else
	uiDefButS(block, MENU, B_SWITCHRENDER, "Rendering Engine %t|Blender Internal %x0", 
												369, 142, 191, 20, &G.scene->r.renderer, 0, 0, 0, 0, "Choose rendering engine");	
#endif /* disable yafray */

	uiBlockBeginAlign(block);
	if((G.scene->r.scemode & R_FULL_SAMPLE) && (G.scene->r.scemode & R_EXR_TILE_FILE))
		uiDefButBitI(block, TOG, R_OSA, B_DIFF, "FSA",	369,109,122,20,&G.scene->r.mode, 0, 0, 0, 0, "Saves all samples, then composites, and then merges (for best Anti-aliasing)");
	else
		uiDefButBitI(block, TOG, R_OSA, B_DIFF, "OSA",	369,109,122,20,&G.scene->r.mode, 0, 0, 0, 0, "Enables Oversampling (Anti-aliasing)");
	uiDefButS(block, ROW,B_DIFF,"5",			369,88,29,20,&G.scene->r.osa,2.0,5.0, 0, 0, "Render 5 samples per pixel for smooth edges (Fast)");
	uiDefButS(block, ROW,B_DIFF,"8",			400,88,29,20,&G.scene->r.osa,2.0,8.0, 0, 0, "Render 8 samples per pixel for smooth edges (Recommended)");
	uiDefButS(block, ROW,B_DIFF,"11",			431,88,29,20,&G.scene->r.osa,2.0,11.0, 0, 0, "Render 11 samples per pixel for smooth edges (High Quality)");
	uiDefButS(block, ROW,B_DIFF,"16",			462,88,29,20,&G.scene->r.osa,2.0,16.0, 0, 0, "Render 16 samples per pixel for smooth edges (Highest Quality)");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_MBLUR, B_REDR, "MBLUR",	496,109,64,20,&G.scene->r.mode, 0, 0, 0, 0, "Enables Motion Blur calculation");
	uiDefButF(block, NUM,B_DIFF,"Bf:",			496,88,64,20,&G.scene->r.blurfac, 0.01, 5.0, 10, 2, "Sets motion blur factor");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButS(block, NUM,B_DIFF,"Xparts:",		369,46,95,29,&G.scene->r.xparts,1.0, 512.0, 0, 0, "Sets the number of horizontal parts to render image in (For panorama sets number of camera slices)");
	uiDefButS(block, NUM,B_DIFF,"Yparts:",		465,46,95,29,&G.scene->r.yparts,1.0, 64.0, 0, 0, "Sets the number of vertical parts to render image in");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,800,"Sky",		369,13,35,20,&G.scene->r.alphamode,3.0,0.0, 0, 0, "Fill background with sky");
	uiDefButS(block, ROW,800,"Premul",	405,13,50,20,&G.scene->r.alphamode,3.0,1.0, 0, 0, "Multiply alpha in advance");
	uiDefButS(block, ROW,800,"Key",		456,13,35,20,&G.scene->r.alphamode,3.0,2.0, 0, 0, "Alpha and color values remain unchanged");
	uiBlockEndAlign(block);

	uiDefButS(block, MENU, B_DIFF,"Octree resolution %t|64 %x64|128 %x128|256 %x256|512 %x512",	496,13,64,20,&G.scene->r.ocres,0.0,0.0, 0, 0, "Octree resolution for ray tracing and baking, Use higher values for complex scenes");

	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_SHADOW, B_REDR,"Shadow",	565,172,52,29, &G.scene->r.mode, 0, 0, 0, 0, "Enable shadow calculation");
	uiDefButBitI(block, TOG, R_SSS, B_REDR,"SSS",	617,172,32,29, &G.scene->r.mode, 0, 0, 0, 0, "Enable subsurface scattering map rendering");
	uiDefButBitI(block, TOG, R_PANORAMA, B_REDR,"Pano",	649,172,38,29, &G.scene->r.mode, 0, 0, 0, 0, "Enable panorama rendering (output width is multiplied by Xparts)");
	uiDefButBitI(block, TOG, R_ENVMAP, B_REDR,"EnvMap",	565,142,52,29, &G.scene->r.mode, 0, 0, 0, 0, "Enable environment map rendering");
	uiDefButBitI(block, TOG, R_RAYTRACE, B_REDR,"Ray",617,142,32,29, &G.scene->r.mode, 0, 0, 0, 0, "Enable ray tracing");
	uiDefButBitI(block, TOG, R_RADIO, B_REDR,"Radio",	649,142,38,29, &G.scene->r.mode, 0, 0, 0, 0, "Enable radiosity rendering");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_DIFF,"100%",			565,109,122,20,&G.scene->r.size,1.0,100.0, 0, 0, "Set render size to defined size");
	uiDefButS(block, ROW,B_DIFF,"75%",			565,88,40,20,&G.scene->r.size,1.0,75.0, 0, 0, "Set render size to 3/4 of defined size");
	uiDefButS(block, ROW,B_DIFF,"50%",			606,88,40,20,&G.scene->r.size,1.0,50.0, 0, 0, "Set render size to 1/2 of defined size");
	uiDefButS(block, ROW,B_DIFF,"25%",			647,88,40,20,&G.scene->r.size,1.0,25.0, 0, 0, "Set render size to 1/4 of defined size");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_FIELDS, B_REDR,"Fields",  565,55,60,20,&G.scene->r.mode, 0, 0, 0, 0, "Enables field rendering");
	uiDefButBitI(block, TOG, R_ODDFIELD, B_REDR,"Odd",	627,55,39,20,&G.scene->r.mode, 0, 0, 0, 0, "Enables Odd field first rendering (Default: Even field)");
	uiDefButBitI(block, TOG, R_FIELDSTILL, B_REDR,"X",		668,55,19,20,&G.scene->r.mode, 0, 0, 0, 0, "Disables time difference in field calculations");
	
	sprintf(str, "Filter%%t|Box %%x%d|Tent %%x%d|Quad %%x%d|Cubic %%x%d|Gauss %%x%d|CatRom %%x%d|Mitch %%x%d", R_FILTER_BOX, R_FILTER_TENT, R_FILTER_QUAD, R_FILTER_CUBIC, R_FILTER_GAUSS, R_FILTER_CATROM, R_FILTER_MITCH);
	uiDefButS(block, MENU, B_DIFF,str,		565,34,60,20, &G.scene->r.filtertype, 0, 0, 0, 0, "Set sampling filter for antialiasing");
	uiDefButF(block, NUM,B_DIFF,"",			627,34,60,20,&G.scene->r.gauss,0.5, 1.5, 10, 2, "Sets the filter size");
	
	uiDefButBitI(block, TOG, R_BORDER, REDRAWVIEWCAM, "Border",	565,13,122,20, &G.scene->r.mode, 0, 0, 0, 0, "Render a small cut-out of the image (Shift+B to set in the camera view)");
	uiBlockEndAlign(block);

}

static void render_panel_anim(void)
{
	uiBlock *block;


	block= uiNewBlock(&curarea->uiblocks, "render_panel_anim", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Anim", "Render", 640, 0, 318, 204)==0) return;


	uiDefBut(block, BUT,B_DOANIM,"ANIM",		692,142,192,47, 0, 0, 0, 0, 0, "Render the animation to disk from start to end frame, (Ctrl+F12)");

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_DOSEQ, B_NOP, "Do Sequence",692,114,192,20, &G.scene->r.scemode, 0, 0, 0, 0, "Enables sequence output rendering (Default: 3D rendering)");
	uiDefButBitI(block, TOG, R_DOCOMP, B_NOP, "Do Composite",692,90,192,20, &G.scene->r.scemode, 0, 0, 0, 0, "Uses compositing nodes for output rendering");
	uiBlockEndAlign(block);

	uiBlockSetCol(block, TH_AUTO);
	uiDefBut(block, BUT,B_PLAYANIM, "PLAY",692,40,94,33, 0, 0, 0, 0, 0, "Play rendered images/avi animation (Ctrl+F11), (Play Hotkeys: A-Noskip, P-PingPong)");
	uiDefButS(block, NUM, B_RTCHANGED, "rt:",789,40,95,33, &G.rt, -1000.0, 1000.0, 0, 0, "General testing/debug button");

	uiBlockBeginAlign(block);
	uiDefButI(block, NUM,REDRAWSEQ,"Sta:",692,10,94,24, &G.scene->r.sfra,1.0,MAXFRAMEF, 0, 0, "The start frame of the animation (inclusive)");
	uiDefButI(block, NUM,REDRAWSEQ,"End:",789,10,95,24, &G.scene->r.efra,SFRA,MAXFRAMEF, 0, 0, "The end  frame of the animation  (inclusive)");
	uiBlockEndAlign(block);
}

#ifdef WITH_FFMPEG

static void ffmpeg_property_del(void *type, void *prop_)
{
	struct IDProperty *prop = (struct IDProperty *) prop_;
	IDProperty * group;
	
	if (!G.scene->r.ffcodecdata.properties) {
		return;
	}

	group = IDP_GetPropertyFromGroup(
		G.scene->r.ffcodecdata.properties, (char*) type);
	if (group && prop) {
		IDP_RemFromGroup(group, prop);
		IDP_FreeProperty(prop);
		MEM_freeN(prop);
	}
	allqueue(REDRAWBUTSSCENE, 0);
}

static IDProperty * ffmpeg_property_add(
	char * type, int opt_index, int parent_index)
{
	AVCodecContext c;
	const AVOption * o;
	const AVOption * parent;
	IDProperty * group;
	IDProperty * prop;
	IDPropertyTemplate val;
	int idp_type;
	char name[256];

	avcodec_get_context_defaults(&c);

	o = c.av_class->option + opt_index;
	parent = c.av_class->option + parent_index;

	if (!G.scene->r.ffcodecdata.properties) {
		IDPropertyTemplate val;

		G.scene->r.ffcodecdata.properties 
			= IDP_New(IDP_GROUP, val, "ffmpeg"); 
	}

	group = IDP_GetPropertyFromGroup(
		G.scene->r.ffcodecdata.properties, (char*) type);
	
	if (!group) {
		IDPropertyTemplate val;
		
		group = IDP_New(IDP_GROUP, val, (char*) type); 
		IDP_AddToGroup(G.scene->r.ffcodecdata.properties, group);
	}

	if (parent_index) {
		sprintf(name, "%s:%s", parent->name, o->name);
	} else {
		strcpy(name, o->name);
	}

	fprintf(stderr, "ffmpeg_property_add: %s %d %d %s\n",
		type, parent_index, opt_index, name);

	prop = IDP_GetPropertyFromGroup(group, name);
	if (prop) {
		return prop;
	}

	switch (o->type) {
	case FF_OPT_TYPE_INT:
	case FF_OPT_TYPE_INT64:
		val.i = o->default_val;
		idp_type = IDP_INT;
		break;
	case FF_OPT_TYPE_DOUBLE:
	case FF_OPT_TYPE_FLOAT:
		val.f = o->default_val;
		idp_type = IDP_FLOAT;
		break;
	case FF_OPT_TYPE_STRING:
		val.str = "                                                                               ";
		idp_type = IDP_STRING;
		break;
	case FF_OPT_TYPE_CONST:
		val.i = 1;
		idp_type = IDP_INT;
		break;
	default:
		return NULL;
	}
	prop = IDP_New(idp_type, val, name);
	IDP_AddToGroup(group, prop);
	return prop;
}

/* not all versions of ffmpeg include that, so here we go ... */

static const AVOption *my_av_find_opt(void *v, const char *name, 
				      const char *unit, int mask, int flags){
	AVClass *c= *(AVClass**)v; 
	const AVOption *o= c->option;

	for(;o && o->name; o++){
		if(!strcmp(o->name, name) && 
		   (!unit || (o->unit && !strcmp(o->unit, unit))) && 
		   (o->flags & mask) == flags )
			return o;
	}
	return NULL;
}

static int ffmpeg_property_add_string(const char * type, const char * str)
{
	AVCodecContext c;
	const AVOption * o = 0;
	const AVOption * p = 0;
	char name_[128];
	char * name;
	char * param;
	IDProperty * prop;
	
	avcodec_get_context_defaults(&c);

	strncpy(name_, str, 128);

	name = name_;
	while (*name == ' ') name++;

	param = strchr(name, ':');

	if (!param) {
		param = strchr(name, ' ');
	}
	if (param) {
		*param++ = 0;
		while (*param == ' ') param++;
	}
	
	o = my_av_find_opt(&c, name, NULL, 0, 0);	
	if (!o) {
		return FALSE;
	}
	if (param && o->type == FF_OPT_TYPE_CONST) {
		return FALSE;
	}
	if (param && o->type != FF_OPT_TYPE_CONST && o->unit) {
		p = my_av_find_opt(&c, param, o->unit, 0, 0);	
		prop = ffmpeg_property_add(
			(char*) type, p - c.av_class->option, 
			o - c.av_class->option);
	} else {
		prop = ffmpeg_property_add(
			(char*) type, o - c.av_class->option, 0);
	}
		

	if (!prop) {
		return FALSE;
	}

	if (param && !p) {
		switch (prop->type) {
		case IDP_INT:
			IDP_Int(prop) = atoi(param);
			break;
		case IDP_FLOAT:
			IDP_Float(prop) = atof(param);
			break;
		case IDP_STRING:
			strncpy(IDP_String(prop), param, prop->len);
			break;
		}
	}
	return TRUE;
}

static void ffmpeg_property_add_using_menu(void * type, int opt_indices)
{
	int opt_index = opt_indices & 65535;
	int parent_index = opt_indices >> 16;

	ffmpeg_property_add((char*) type, opt_index, parent_index);

	allqueue(REDRAWBUTSSCENE, 0);
}

static uiBlock *ffmpeg_property_add_submenu(AVOption * parent, char * type) 
{
	AVCodecContext c;
	const AVOption * o;
	uiBlock *block;
	int yco = 0;
	int flags = 0;
	int parent_index = 0;

	if (strcmp(type, "audio") == 0) {
		flags = AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_AUDIO_PARAM;
	} else if (strcmp(type, "video") == 0) {
		flags = AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM;
	} else {
		return NULL;
	}

	block= uiNewBlock(&curarea->uiblocks, "ffmpeg_property_add_submenu",
                          UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
        uiBlockSetButmFunc(block, ffmpeg_property_add_using_menu, type);

	avcodec_get_context_defaults(&c);

	if (parent) {
		parent_index = (parent - c.av_class->option);
	}
	
	for(o = c.av_class->option; o && o->name; o++){
		if (o->help && 
		    (strstr(o->help, "experimental")
		     || strstr(o->help, "obsolete")
		     || strstr(o->help, "useless")
		     || strstr(o->help, "deprecated"))) {
			continue;
		}
		if((o->flags & flags) == flags) {
			if((!parent && !o->unit) 
			   || (o->unit && parent 
			       && strcmp(o->unit, parent->unit) == 0 
			       && o->type == FF_OPT_TYPE_CONST)) {
				uiDefBut(block, BUTM, B_REDR, 
					 (char*) (o->help && o->help[0] ? 
						  o->help : o->name),
					 0, yco, 160, 15, 
					 NULL, 0, 0, 1, 
					 (o - c.av_class->option) | 
					 (parent_index << 16),
					 "");
				yco -= 16;
			}
		}
	}
	
	uiTextBoundsBlock(block, 50);
        uiBlockSetDirection(block, UI_RIGHT);

        return block;
}

static uiBlock *ffmpeg_property_add_submenu_audio(void* opt)
{
	return ffmpeg_property_add_submenu((AVOption*) opt, "audio");
}

static uiBlock *ffmpeg_property_add_submenu_video(void* opt)
{
	return ffmpeg_property_add_submenu((AVOption*) opt, "video");
}

static uiBlock *ffmpeg_property_add_menu(void* type_) 
{
	char * type = (char*) type_;
	AVCodecContext c;
	const AVOption * o;
	uiBlock *block;
	int yco = 0;
	int flags = 0;
	uiBlockFuncFP add_submenu = NULL;

	if (strcmp(type, "audio") == 0) {
		flags = AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_AUDIO_PARAM;
		add_submenu = ffmpeg_property_add_submenu_audio;
	} else if (strcmp(type, "video") == 0) {
		flags = AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM;
		add_submenu = ffmpeg_property_add_submenu_video;
	} else {
		return NULL;
	}

	block= uiNewBlock(&curarea->uiblocks, "ffmpeg_property_add_menu",
                          UI_EMBOSSP, UI_HELV, curarea->win);

	avcodec_get_context_defaults(&c);
	
	for(o = c.av_class->option; o && o->name; o++){
		if((o->flags & flags) == flags) {
			if (o->type == FF_OPT_TYPE_CONST) {
				continue;
			}
			if (o->help && 
			    (strstr(o->help, "experimental")
			     || strstr(o->help, "obsolete")
			     || strstr(o->help, "useless")
			     || strstr(o->help, "deprecated"))) {
				continue;
			}

			if (o->unit) {
	
				uiDefIconTextBlockBut(
					block, 
					add_submenu, 
					(void*) o, 
					ICON_RIGHTARROW_THIN,
					(char*) (o->help ? 
						 o->help : o->name), 
					0, yco, 160, 15, "");
				yco -= 16;
			} 
		}
	}

	uiDefIconTextBlockBut(
		block, 
		add_submenu, 
		NULL, 
		ICON_RIGHTARROW_THIN,
		"Value / string options", 
		0, yco, 160, 15, "");
	
	uiTextBoundsBlock(block, 50);
        uiBlockSetDirection(block, UI_DOWN);

        return block;
}

static int render_panel_ffmpeg_property_option(
	uiBlock *block, int xofs, int yofs, IDProperty * curr,
	const char * type)
{
	AVCodecContext c;
	const AVOption * o;
        uiBut *but;
	char name[128];
	char * param;

	strcpy(name, curr->name);
	param = strchr(name, ':');

	if (param) {
		*param++ = 0;
	}

	avcodec_get_context_defaults(&c);

	o = my_av_find_opt(&c, param ? param : name, NULL, 0, 0);
	if (!o) {
		return yofs;
	}

	switch (curr->type) {
	case IDP_STRING:
		uiDefBut(block, TEX, 
			 B_REDR, curr->name, 
			 xofs,yofs, 200,19, 
			 IDP_String(curr), 
			 0.0, curr->len - 1, 100, 0, 
			 (char*) o->help);
		break;
	case IDP_FLOAT:
		uiDefButF(block, NUM, B_REDR, curr->name, 
			  xofs, yofs, 200, 19, 
			  &IDP_Float(curr), 
			  o->min, o->max, 0, 0, (char*) o->help);
		break;
	case IDP_INT:
		if (o->type == FF_OPT_TYPE_CONST) {
			uiDefButBitI(block, TOG, 1, B_REDR,
				     curr->name,
				     xofs, yofs, 200, 19, 
				     &IDP_Int(curr), 
				     0, 1, 0,0, (char*) o->help);
		} else {
			uiDefButI(block, NUM, B_REDR, curr->name, 
				  xofs, yofs, 200, 19, 
				  &IDP_Int(curr), 
				  o->min, o->max, 0, 0, (char*) o->help);
		}
		break;
	}

	but = uiDefIconBut(block, BUT, B_REDR, VICON_X, 
			   xofs + 220, yofs, 16, 16, NULL, 
			   0.0, 0.0, 0.0, 0.0, "Delete property");

	uiButSetFunc(but, ffmpeg_property_del, (void*) type, curr);

	yofs -= 25;

	return yofs;
}

static int render_panel_ffmpeg_properties(uiBlock *block, const char * type,
					  int xofs, int yofs)
{
	short event = B_NOP;

	yofs -= 5;
	
	if (strcmp(type, "audio") == 0) {
		event = B_ADD_FFMPEG_AUDIO_OPTION;
	} else if (strcmp(type, "video") == 0) {
		event = B_ADD_FFMPEG_VIDEO_OPTION;
	}
		
	uiDefBut(block, TEX, event, "", xofs, yofs, 
		 170, 19, ffmpeg_option_to_add, 0.0, 255.0, 100, 0, 
		 "FFMPEG option to add");

	uiDefBut(block, BUT, event, "Add", xofs+170,yofs,
		 30, 19, 0, 0, 0, 0, 0, 
		 "Add FFMPEG option");

	uiDefBlockBut(block, ffmpeg_property_add_menu, (void*) type, 
		      "Menu", xofs + 200, yofs, 40, 20, 
		      "Add FFMPEG option using menu");
	yofs -= 20;

	if (G.scene->r.ffcodecdata.properties) {
		IDProperty * prop;
		void * iter;
		IDProperty * curr;

		prop = IDP_GetPropertyFromGroup(
			G.scene->r.ffcodecdata.properties, (char*) type);
		if (prop) {
			iter = IDP_GetGroupIterator(prop);

			while ((curr = IDP_GroupIterNext(iter)) != NULL) {
				yofs = render_panel_ffmpeg_property_option(
					block, xofs, yofs, curr, type);
			}
		}
	}

	uiNewPanelHeight(block, 204-yofs);

	return yofs;
}

static void set_ffmpeg_preset(int preset)
{
	int isntsc = (G.scene->r.frs_sec != 25);
	switch (preset) {
	case FFMPEG_PRESET_VCD:
		G.scene->r.ffcodecdata.type = FFMPEG_MPEG1;
		G.scene->r.ffcodecdata.video_bitrate = 1150;
		G.scene->r.xsch = 352;
		G.scene->r.ysch = isntsc ? 240 : 288;
		G.scene->r.ffcodecdata.gop_size = isntsc ? 18 : 15;
		G.scene->r.ffcodecdata.rc_max_rate = 1150;
		G.scene->r.ffcodecdata.rc_min_rate = 1150;
		G.scene->r.ffcodecdata.rc_buffer_size = 40*8;
		G.scene->r.ffcodecdata.mux_packet_size = 2324;
		G.scene->r.ffcodecdata.mux_rate = 2352 * 75 * 8;
		break;
	case FFMPEG_PRESET_SVCD:
		G.scene->r.ffcodecdata.type = FFMPEG_MPEG2;
		G.scene->r.ffcodecdata.video_bitrate = 2040;
		G.scene->r.xsch = 480;
		G.scene->r.ysch = isntsc ? 480 : 576;
		G.scene->r.ffcodecdata.gop_size = isntsc ? 18 : 15;
		G.scene->r.ffcodecdata.rc_max_rate = 2516;
		G.scene->r.ffcodecdata.rc_min_rate = 0;
		G.scene->r.ffcodecdata.rc_buffer_size = 224*8;
		G.scene->r.ffcodecdata.mux_packet_size = 2324;
		G.scene->r.ffcodecdata.mux_rate = 0;
		
		break;
	case FFMPEG_PRESET_DVD:
		G.scene->r.ffcodecdata.type = FFMPEG_MPEG2;
		G.scene->r.ffcodecdata.video_bitrate = 6000;
		G.scene->r.xsch = 720;
		G.scene->r.ysch = isntsc ? 480 : 576;
		G.scene->r.ffcodecdata.gop_size = isntsc ? 18 : 15;
		G.scene->r.ffcodecdata.rc_max_rate = 9000;
		G.scene->r.ffcodecdata.rc_min_rate = 0;
		G.scene->r.ffcodecdata.rc_buffer_size = 224*8;
		G.scene->r.ffcodecdata.mux_packet_size = 2048;
		G.scene->r.ffcodecdata.mux_rate = 10080000;
		
		break;
	case FFMPEG_PRESET_DV:
		G.scene->r.ffcodecdata.type = FFMPEG_DV;
		G.scene->r.xsch = 720;
		G.scene->r.ysch = isntsc ? 480 : 576;
		break;
	case FFMPEG_PRESET_H264:
		G.scene->r.ffcodecdata.type = FFMPEG_AVI;
		G.scene->r.ffcodecdata.codec = CODEC_ID_H264;
		G.scene->r.ffcodecdata.video_bitrate = 6000;
		G.scene->r.ffcodecdata.gop_size = isntsc ? 18 : 15;
		G.scene->r.ffcodecdata.rc_max_rate = 9000;
		G.scene->r.ffcodecdata.rc_min_rate = 0;
		G.scene->r.ffcodecdata.rc_buffer_size = 224*8;
		G.scene->r.ffcodecdata.mux_packet_size = 2048;
		G.scene->r.ffcodecdata.mux_rate = 10080000;

		ffmpeg_property_add_string("video", "coder:vlc");
		ffmpeg_property_add_string("video", "flags:loop");
		ffmpeg_property_add_string("video", "cmp:chroma");
		ffmpeg_property_add_string("video", "partitions:parti4x4");
		ffmpeg_property_add_string("video", "partitions:partp8x8");
		ffmpeg_property_add_string("video", "partitions:partb8x8");
		ffmpeg_property_add_string("video", "me:hex");
		ffmpeg_property_add_string("video", "subq:5");
		ffmpeg_property_add_string("video", "me_range:16");
		ffmpeg_property_add_string("video", "keyint_min:25");
		ffmpeg_property_add_string("video", "sc_threshold:40");
		ffmpeg_property_add_string("video", "i_qfactor:0.71");
		ffmpeg_property_add_string("video", "b_strategy:1");

		break;
	}
}

static void render_panel_ffmpeg_video(void)
{
	uiBlock *block;
	int yofs;
	int xcol1;
	int xcol2;
	
	block = uiNewBlock(&curarea->uiblocks, "render_panel_ffmpeg_video", 
					   UI_EMBOSS, UI_HELV, curarea->win);
	
	uiNewPanelTabbed("Format", "Render");
	if (uiNewPanel(curarea, block, "Video", "Render", 960, 0, 318, 204)== 0) 
		return;
	
	if (ffmpeg_preset_sel != 0) {
		set_ffmpeg_preset(ffmpeg_preset_sel);
		ffmpeg_preset_sel = 0;
		allqueue(REDRAWBUTSSCENE, 0);
	}
	
	xcol1 = 872;
	xcol2 = 1002;
	
	yofs = 54;
	uiDefBut(block, LABEL, B_DIFF, "Format", xcol1, yofs+88, 
			 110, 20, 0, 0, 0, 0, 0, "");
	uiDefBut(block, LABEL, B_DIFF, "Preset", xcol2, yofs+88, 
			 110, 20, 0, 0, 0, 0, 0, "");
	uiDefButI(block, MENU, B_DIFF, ffmpeg_format_pup(), 
			  xcol1, yofs+66, 110, 20, &G.scene->r.ffcodecdata.type, 
			  0,0,0,0, "output file format");
	uiDefButI(block, NUM, B_DIFF, "Bitrate", 
			  xcol1, yofs+44, 110, 20, 
			  &G.scene->r.ffcodecdata.video_bitrate, 
			  1, 14000, 0, 0, "Video bitrate(kb/s)");
	uiDefButI(block, NUM, B_DIFF, "Min Rate", 
		  xcol1, yofs+22, 110, 20, 
		  &G.scene->r.ffcodecdata.rc_min_rate, 
		  0, G.scene->r.ffcodecdata.rc_max_rate, 
		  0, 0, "Rate control: min rate(kb/s)");
	uiDefButI(block, NUM, B_DIFF, "Max Rate", 
			  xcol1, yofs, 110, 20, &G.scene->r.ffcodecdata.rc_max_rate, 
			  1, 14000, 0, 0, "Rate control: max rate(kb/s)");
	
	uiDefButI(block, NUM, B_DIFF, "Mux Rate", 
			  xcol1, yofs-22, 110, 20, 
			  &G.scene->r.ffcodecdata.mux_rate, 
			  0, 100000000, 0, 0, "Mux rate (bits/s(!))");
	
	
	uiDefButI(block, MENU, B_REDR, ffmpeg_preset_pup(), 
			  xcol2, yofs+66, 110, 20, &ffmpeg_preset_sel, 
			  0,0,0,0, "Output file format preset selection");
	uiDefButI(block, NUM, B_DIFF, "GOP Size", 
			  xcol2, yofs+44, 110, 20, &G.scene->r.ffcodecdata.gop_size, 
			  0, 100, 0, 0, "Distance between key frames");
	uiDefButI(block, NUM, B_DIFF, "Buffersize", 
			  xcol2, yofs+22, 110, 20,
			  &G.scene->r.ffcodecdata.rc_buffer_size, 
			  0, 2000, 0, 0, "Rate control: buffer size (kb)");
	uiDefButI(block, NUM, B_DIFF, "Mux PSize", 
			  xcol2, yofs, 110, 20, 
			  &G.scene->r.ffcodecdata.mux_packet_size, 
			  0, 16384, 0, 0, "Mux packet size (byte)");
	
	uiDefButBitI(block, TOG, FFMPEG_AUTOSPLIT_OUTPUT, B_NOP,
				 "Autosplit Output", 
				 xcol2, yofs-22, 110, 20, 
				 &G.scene->r.ffcodecdata.flags, 
				 0, 1, 0,0, "Autosplit output at 2GB boundary.");
	
	
	if (ELEM3(G.scene->r.ffcodecdata.type, FFMPEG_AVI, 
		  FFMPEG_MOV, FFMPEG_MKV)) {
		uiDefBut(block, LABEL, 0, "Codec", 
				xcol1, yofs-44, 110, 20, 0, 0, 0, 0, 0, "");
		uiDefButI(block, MENU,B_REDR, ffmpeg_codec_pup(), 
				  xcol1, yofs-66, 110, 20, 
				  &G.scene->r.ffcodecdata.codec, 
				  0,0,0,0, "FFMpeg codec to use");
	}

	render_panel_ffmpeg_properties(block, "video", xcol1, yofs-86);
}

static void render_panel_ffmpeg_audio(void)
{
	uiBlock *block;
	int yofs;
	int xcol;
	
	block = uiNewBlock(&curarea->uiblocks, "render_panel_ffmpeg_audio", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Format", "Render");
	if (uiNewPanel(curarea, block, "Audio", "Render", 960, 0, 318, 204) == 0) return;
	
	yofs = 54;
	xcol = 892;
	
	uiDefButBitI(block, TOG, FFMPEG_MULTIPLEX_AUDIO, B_NOP,
				 "Multiplex audio", xcol, yofs, 225, 20, 
				 &G.scene->r.ffcodecdata.flags, 
				 0, 1, 0,0, "Interleave audio with the output video");
	uiDefBut(block, LABEL, 0, "Codec", 
			 xcol, yofs-22, 225, 20, 0, 0, 0, 0, 0, "");
	uiDefButI(block, MENU,B_NOP, ffmpeg_audio_codec_pup(), 
			  xcol, yofs-44, 225, 20, 
			  &G.scene->r.ffcodecdata.audio_codec, 
			  0,0,0,0, "FFMpeg codec to use");
	uiDefButI(block, NUM, B_DIFF, "Bitrate", 
			  xcol, yofs-66, 110, 20, 
			  &G.scene->r.ffcodecdata.audio_bitrate, 
			  32, 384, 0, 0, "Audio bitrate(kb/s)");

	render_panel_ffmpeg_properties(block, "audio", xcol, yofs-86);
}
#endif

static void render_panel_stamp(void)
{
	uiBlock *block;
	int yofs=0, xofs=550;

	block= uiNewBlock (&curarea->uiblocks, "render_panel_stamp", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed ("Format", "Render");
	if(uiNewPanel (curarea, block, "Stamp", "Render", 960, 0, 318, 204)==0) return;

	if (G.scene->r.scemode & R_STAMP_INFO) {
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, R_STAMP_NOTE, B_REDR, "Note", xofs, yofs, 120, 19, &G.scene->r.stamp, 0, 0, 0, 0, "Stamp user data");
		uiDefBut(block, TEX, B_NOP, "", xofs+120, yofs, 180, 19, &G.scene->r.stamp_udata, 0.0, 128.0, 100, 0, "User Note");
		uiBlockEndAlign(block);
		yofs += 30; /* gap */
		
		
		yofs += 80;
 		/* Order is important for alligning ... grr */
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, R_STAMP_FILENAME, B_REDR, "Filename", xofs, yofs, 120, 19, &G.scene->r.stamp, 0, 0, 0, 0, "Stamp blend filename");
		yofs -= 20;
		uiDefButBitI(block, TOG, R_STAMP_SCENE, B_REDR, "Scene", xofs, yofs, 60, 19, &G.scene->r.stamp, 0, 0, 0, 0, "Stamp scene name");
		uiDefButBitI(block, TOG, R_STAMP_CAMERA, B_REDR, "Camera", xofs+60, yofs, 60, 19, &G.scene->r.stamp, 0, 0, 0, 0, "Stamp camera name");
		yofs -= 20;
		uiDefButBitI(block, TOG, R_STAMP_TIME, B_REDR, "Time", xofs, yofs, 60, 19, &G.scene->r.stamp, 0, 0, 0, 0, "Stamp time (HH:MM:SS)");
		uiDefButBitI(block, TOG, R_STAMP_DATE, B_REDR, "Date", xofs+60, yofs, 60, 19, &G.scene->r.stamp, 0, 0, 0, 0, "Stamp date");
		yofs -= 20;
		uiDefButBitI(block, TOG, R_STAMP_FRAME, B_REDR, "Frame", xofs, yofs, 60, 19, &G.scene->r.stamp, 0, 0, 0, 0, "Stamp frame number");
		uiDefButBitI(block, TOG, R_STAMP_MARKER, B_REDR, "Marker", xofs+60, yofs, 60, 19, &G.scene->r.stamp, 0, 0, 0, 0, "Stamp the last marker");
		yofs -= 20;
		uiDefButBitI(block, TOG, R_STAMP_SEQSTRIP, B_REDR, "Sequence Strip", xofs, yofs, 120, 19, &G.scene->r.stamp, 0, 0, 0, 0, "Stamp the forground sequence strip name");
		uiBlockEndAlign(block);
		yofs += 80;
		
		/* draw font selector */
		if (G.scene->r.stamp & R_STAMP_DRAW) {
			uiDefButS(block, MENU, B_REDR, "Stamp Font Size%t|Tiny Text%x1|Small Text%x2|Medium Text%x3|Large Text%x0|Extra Large Text%x4|",
					xofs+130, yofs, 170, 19, &G.scene->r.stamp_font_id, 0, 0, 0, 0, "Choose stamp text size");
			
			/* draw fg/bg next to the scene */
			yofs -= 25;
			uiDefBut(block, LABEL, B_NOP, "Text Color", xofs+130, yofs, 70, 19, 0, 0, 0, 0, 0, "");
			uiDefBut(block, LABEL, B_NOP, "Background", xofs+215, yofs, 70, 19, 0, 0, 0, 0, 0, "");
			yofs -= 20;
			uiDefButF(block, COL, B_NOP, "", xofs+130, yofs, 80, 19, G.scene->r.fg_stamp, 0, 0, 0, 0, "Foreground text color");
			uiDefButF(block, COL, B_NOP, "", xofs+220, yofs, 80, 19, G.scene->r.bg_stamp, 0, 0, 0, 0, "Background color");
			yofs -= 30;
			uiDefButF(block, NUMSLI, B_NOP, "A ", xofs+130, yofs, 170, 19, &G.scene->r.bg_stamp[3], 0, 1.0, 0, 0, "Alpha for text background");
			yofs += 105;
		} else {
			yofs += 30;
		}
		
		uiDefButBitI(block, TOG, R_STAMP_INFO, B_REDR, "Enable Stamp", xofs, yofs, 120, 20, &G.scene->r.scemode, 0, 0, 0, 0, "Disable stamp info in images metadata");
		uiDefButBitI(block, TOG, R_STAMP_DRAW, B_REDR, "Draw Stamp", xofs+130, yofs, 170, 20, &G.scene->r.stamp, 0, 0, 0, 0, "Draw the stamp info into each frame");
		yofs += 20;
	}
	else {
		uiDefButBitI(block, TOG, R_STAMP_INFO, B_REDR, "Enable Stamp", xofs, 142, 120, 20, &G.scene->r.scemode, 0, 0, 0, 0, "Enable stamp info to image metadata");
		yofs += 20;
		uiDefBut(block, LABEL, 0, "", xofs, yofs, 300, 19, 0, 0, 0, 0, 0, "");
	}
}

static void render_panel_format(void)
{
	uiBlock *block;
	int yofs;


	block= uiNewBlock(&curarea->uiblocks, "render_panel_format", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Format", "Render", 960, 0, 318, 204)==0) return;
	uiDefBlockBut(block, framing_render_menu, NULL, 
				  "Game framing settings", 
				  892, 169, 227, 20, "Display game framing settings");
	/* uiDefIconTextBlockBut(block, framing_render_menu, NULL, 
						   ICON_BLOCKBUT_CORNER, 
						   "Game framing settings", 
						   892, 169, 227, 20, 
						   "Display game framing settings"); */

	uiBlockBeginAlign(block);
	uiDefButS(block, NUM,REDRAWVIEWCAM,"SizeX:",	892 ,136,112,27, &G.scene->r.xsch, 4.0, 10000.0, 0, 0, "The image width in pixels");
	uiDefButS(block, NUM,REDRAWVIEWCAM,"SizeY:",	1007,136,112,27, &G.scene->r.ysch, 4.0,10000.0, 0, 0, "The image height in scanlines");
	
	uiDefButF(block, NUM, REDRAWVIEWCAM, "AspX:",
				892 ,114,112,20,
				&G.scene->r.xasp,
				1, 200, 100, 2,
				"Horizontal Aspect Ratio");
	uiDefButF(block, NUM, REDRAWVIEWCAM, "AspY:",
			 	1007,114,112,20,
			 	&G.scene->r.yasp,
			 	1, 200, 100, 2,
			 	"Vertical Aspect Ratio");
	
	uiBlockEndAlign(block);

	yofs = 54;

#ifdef __sgi
	yofs = 76;
	uiDefButS(block, NUM,B_DIFF,"MaxSize:",			892,32,165,20, &G.scene->r.maximsize, 0.0, 500.0, 0, 0, "Maximum size per frame to save in an SGI movie");
	uiDefButBitI(block, TOG, R_COSMO, 0,"Cosmo",	1059,32,60,20, &G.scene->r.mode, 0, 0, 0, 0, "Attempt to save SGI movies using Cosmo hardware");
#endif

	uiDefButS(block, MENU,B_FILETYPEMENU,imagetype_pup(),	892,yofs,174,20, &G.scene->r.imtype, 0, 0, 0, 0, "Images are saved in this file format");
	uiDefButBitI(block, TOG, R_CROP, B_DIFF, "Crop",        1068,yofs,51,20, &G.scene->r.mode, 0, 0, 0, 0, "When Border render, the resulting image gets cropped");

	yofs -= 22;

	if(G.scene->r.quality==0) G.scene->r.quality= 90;

	if (G.scene->r.imtype == R_AVICODEC || G.scene->r.imtype == R_QUICKTIME) {
		if(G.scene->r.imtype == R_QUICKTIME) {
#ifdef WITH_QUICKTIME
#if defined (_WIN32) || defined (__APPLE__)
			//glColor3f(0.65, 0.65, 0.7);
			//glRecti(892,yofs+46,892+225,yofs+45+20);
			if(G.scene->r.qtcodecdata == NULL)
				uiDefBut(block, LABEL, 0, "Codec: not set",  892,yofs+44,225,20, 0, 0, 0, 0, 0, "");
			else
				uiDefBut(block, LABEL, 0, G.scene->r.qtcodecdata->qtcodecname,  892,yofs+44,225,20, 0, 0, 0, 0, 0, "");
			uiDefBut(block, BUT,B_SELECTCODEC, "Set codec",  892,yofs,74,20, 0, 0, 0, 0, 0, "Set codec settings for Quicktime");
#endif
#endif /* WITH_QUICKTIME */
		} else {
#ifdef _WIN32
			//glColor3f(0.65, 0.65, 0.7);
			//glRecti(892,yofs+46,892+225,yofs+45+20);
			if(G.scene->r.avicodecdata == NULL)
				uiDefBut(block, LABEL, 0, "Codec: not set.",  892,yofs+43,225,20, 0, 0, 0, 0, 0, "");
			else
				uiDefBut(block, LABEL, 0, avicodec_str(),  892,yofs+43,225,20, 0, 0, 0, 0, 0, "");
#endif
			uiDefBut(block, BUT,B_SELECTCODEC, "Set codec",  892,yofs,74,20, 0, 0, 0, 0, 0, "Set codec settings for AVI");
		}
#ifdef WITH_OPENEXR
	} 
	else if (ELEM(G.scene->r.imtype, R_OPENEXR, R_MULTILAYER)) {
		if (G.scene->r.quality > 5) G.scene->r.quality = 2;
		
		if(G.scene->r.imtype==R_OPENEXR) {
			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOG, R_OPENEXR_HALF, B_NOP,"Half",	892,yofs+44,60,20, &G.scene->r.subimtype, 0, 0, 0, 0, "Use 16 bit floats instead of 32 bit floats per channel");
			uiDefButBitS(block, TOG, R_OPENEXR_ZBUF, B_NOP,"Zbuf",	952,yofs+44,60,20, &G.scene->r.subimtype, 0, 0, 0, 0, "Save the z-depth per pixel (32 bit unsigned int zbuffer)");
			uiBlockEndAlign(block);
			uiDefButBitS(block, TOG, R_PREVIEW_JPG, B_NOP,"Preview",1027,yofs+44,90,20, &G.scene->r.subimtype, 0, 0, 0, 0, "When animation render, save JPG preview images in same directory");
		}		
		uiDefButS(block, MENU,B_NOP, "Codec %t|None %x0|Pxr24 (lossy) %x1|ZIP (lossless) %x2|PIZ (lossless) %x3|RLE (lossless) %x4",  
															892,yofs,74,20, &G.scene->r.quality, 0, 0, 0, 0, "Set codec settings for OpenEXR");
		
#endif
	} else if (G.scene->r.imtype == R_DPX || G.scene->r.imtype == R_CINEON) {
		uiDefButBitS(block, TOG, R_CINEON_LOG, B_REDR, "Log",           892,yofs,74,20, &G.scene->r.subimtype, 0, 0, 0, 0, "Convert to log color space");

		if(G.scene->r.subimtype & R_CINEON_LOG) {
			uiBlockBeginAlign(block);
			uiDefButS(block, NUM, B_NOP, "B",	892,yofs+44,80,20, &G.scene->r.cineonblack, 0, 1024, 0, 0, "Log conversion reference black");
			uiDefButS(block, NUM, B_NOP, "W",	972,yofs+44,80,20, &G.scene->r.cineonwhite, 0, 1024, 0, 0, "Log conversion reference white");
			uiDefButF(block, NUM, B_NOP, "G",	1052,yofs+44,70,20, &G.scene->r.cineongamma, 0.0f, 10.0f, 1, 2, "Log conversion gamma");
			uiBlockEndAlign(block);
		}
	} else if (G.scene->r.imtype == R_TIFF) {
		uiDefButBitS(block, TOG, R_TIFF_16BIT, B_REDR, "16 Bit",           892,yofs,74,20, &G.scene->r.subimtype, 0, 0, 0, 0, "Save 16 bit per channel TIFF");
	} else {
		if(G.scene->r.quality < 5) G.scene->r.quality = 90;	/* restore from openexr */
		
		uiDefButS(block, NUM,B_DIFF, "Q:",           892,yofs,74,20, &G.scene->r.quality, 10.0, 100.0, 0, 0, "Quality setting for JPEG images, AVI Jpeg and SGI movies");
	}
	uiDefButS(block, NUM,B_FRAMEMAP,"FPS:",   968,yofs,75,20, &G.scene->r.frs_sec, 1.0, 120.0, 100.0, 0, "Frames per second");
	uiDefButF(block, NUM,B_FRAMEMAP,"/",  1043,yofs,75,20, &G.scene->r.frs_sec_base, 1.0, 120.0, 0.1, 3, "Frames per second base");


	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_DIFF,"BW",			892, 10,74,19, &G.scene->r.planes, 5.0,(float)R_PLANESBW, 0, 0, "Images are saved with BW (grayscale) data");
	uiDefButS(block, ROW,B_DIFF,"RGB",		    968, 10,74,19, &G.scene->r.planes, 5.0,(float)R_PLANES24, 0, 0, "Images are saved with RGB (color) data");
	uiDefButS(block, ROW,B_DIFF,"RGBA",		   1044, 10,75,19, &G.scene->r.planes, 5.0,(float)R_PLANES32, 0, 0, "Images are saved with RGB and Alpha data (if supported)");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_PR_PAL, "PAL",		1146,170,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 720x576, Aspect ratio - 54x51, 25 fps");
	uiDefBut(block, BUT,B_PR_NTSC, "NTSC",		1146,150,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 720x480, Aspect ratio - 10x11, 30 fps");
	uiDefBut(block, BUT,B_PR_PRESET, "Default",	1146,130,100,18, 0, 0, 0, 0, 0, "Same as PAL, with render settings (OSA, Shadows, Fields)");
	uiDefBut(block, BUT,B_PR_PRV, "Preview",	1146,110,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 640x512, Render size 50%");
	uiDefBut(block, BUT,B_PR_PC, "PC",			1146,90,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 640x480, Aspect ratio - 100x100");
	uiDefBut(block, BUT,B_PR_PAL169, "PAL 16:9",1146,70,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 720x576, Aspect ratio - 64x45");
	uiDefBut(block, BUT,B_PR_PANO, "PANO",		1146,50,100,18, 0, 0, 0, 0, 0, "Standard panorama settings");
	uiDefBut(block, BUT,B_PR_FULL, "FULL",		1146,30,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 1280x1024, Aspect ratio - 1x1");
	uiDefBut(block, BUT,B_PR_HD, "HD",		1146,10,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 1920x1080, Aspect ratio - 1x1");
	uiBlockEndAlign(block);
}

#ifndef DISABLE_YAFRAY /* disable yafray stuff */
/* yafray: global illumination options panel */
static void render_panel_yafrayGI()
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "render_panel_yafrayGI", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Render", "Render");
	if(uiNewPanel(curarea, block, "YafRay GI", "Render", 320, 0, 318, 204)==0) return;

	// label to force a boundbox for buttons not to be centered
	uiDefBut(block, LABEL, 0, " ", 305,180,10,10, 0, 0, 0, 0, 0, "");

	uiDefBut(block, LABEL, 0, "Method", 5,175,70,20, 0, 1.0, 0, 0, 0, "");
	uiDefButS(block, MENU, B_REDR, "GiMethod %t|None %x0|SkyDome %x1|Full %x2", 70,175,89,20, &G.scene->r.GImethod, 0, 0, 0, 0, "Global Illumination Method");

	uiDefBut(block, LABEL, 0, "Quality", 5,150,70,20, 0, 1.0, 0, 0, 0, "");
	uiDefButS(block, MENU, B_REDR, "GiQuality %t|None %x0|Low %x1|Medium %x2 |High %x3|Higher %x4|Best %x5|Use Blender AO settings %x6", 70,150,89,20, &G.scene->r.GIquality, 0, 0, 0, 0, "Global Illumination Quality");

	if (G.scene->r.GImethod>0) {
		uiDefButF(block, NUM, B_DIFF, "EmitPwr:", 5,35,154,20, &G.scene->r.GIpower, 0.01, 100.0, 10, 0, "arealight, material emit and background intensity scaling, 1 is normal");
		if (G.scene->r.GImethod==2) uiDefButF(block, NUM, B_DIFF, "GI Pwr:", 5,10,154,20, &G.scene->r.GIindirpower, 0.01, 100.0, 10, 0, "GI indirect lighting intensity scaling, 1 is normal");
	}

	if (G.scene->r.GImethod>0)
	{
		if (G.scene->r.GIdepth==0) G.scene->r.GIdepth=2;

		if (G.scene->r.GImethod==2) {
			uiDefButI(block, NUM, B_DIFF, "Depth:", 180,175,110,20, &G.scene->r.GIdepth, 1.0, 100.0, 10, 10, "Number of bounces of the indirect light");
			uiDefButI(block, NUM, B_DIFF, "CDepth:", 180,150,110,20, &G.scene->r.GIcausdepth, 1.0, 100.0, 10, 10, "Number of bounces inside objects (for caustics)");
			uiDefButBitS(block, TOG, 1,  B_REDR, "Photons",210,125,100,20, &G.scene->r.GIphotons, 0, 0, 0, 0, "Use global photons to help in GI");
		}

		uiDefButBitS(block, TOG, 1, B_REDR, "Cache",6,125,95,20, &G.scene->r.GIcache, 0, 0, 0, 0, "Cache occlusion/irradiance samples (faster)");
		if (G.scene->r.GIcache) 
		{
			uiDefButBitS(block,TOG, 1, B_REDR, "NoBump",108,125,95,20, &G.scene->r.YF_nobump, 0, 0, 0, 0, "Don't use bumpnormals for cache (faster, but no bumpmapping in total indirectly lit areas)");
			uiDefBut(block, LABEL, 0, "Cache parameters:", 5,105,130,20, 0, 1.0, 0, 0, 0, "");
			if (G.scene->r.GIshadowquality==0.0) G.scene->r.GIshadowquality=0.9;
			uiDefButF(block, NUM, B_DIFF,"ShadQu:", 5,85,154,20,	&(G.scene->r.GIshadowquality), 0.01, 1.0 ,1,0, "Sets the shadow quality, keep it under 0.95 :-) ");
			if (G.scene->r.GIpixelspersample==0) G.scene->r.GIpixelspersample=10;
			uiDefButI(block, NUM, B_DIFF, "Prec:",	5,60,75,20, &G.scene->r.GIpixelspersample, 1, 50, 10, 10, "Maximum number of pixels without samples, the lower the better and slower");
			if (G.scene->r.GIrefinement==0) G.scene->r.GIrefinement=1.0;
			uiDefButF(block, NUM, B_DIFF, "Ref:", 84,60,75,20, &G.scene->r.GIrefinement, 0.001, 1.0, 1, 0, "Threshold to refine shadows EXPERIMENTAL. 1 = no refinement");
		}

		if (G.scene->r.GImethod==2) {
			if (G.scene->r.GIphotons)
			{
				uiDefBut(block, LABEL, 0, "Photon parameters:", 170,105,130,20, 0, 1.0, 0, 0, 0, "");
				if(G.scene->r.GIphotoncount==0) G.scene->r.GIphotoncount=100000;
				uiDefButI(block, NUM, B_DIFF, "Count:", 170,85,140,20, &G.scene->r.GIphotoncount, 
						0, 10000000, 10, 10, "Number of photons to shoot");
				if(G.scene->r.GIphotonradius==0.0) G.scene->r.GIphotonradius=1.0;
				uiDefButF(block, NUMSLI, B_DIFF,"Radius:", 170,60,140,20,	&(G.scene->r.GIphotonradius), 
						0.00001, 100.0 ,0,0, "Radius to search for photons to mix (blur)");
				if(G.scene->r.GImixphotons==0) G.scene->r.GImixphotons=100;
				uiDefButI(block, NUM, B_DIFF, "MixCount:", 170,35,140,20, &G.scene->r.GImixphotons, 
						0, 1000, 10, 10, "Number of photons to mix");
				uiDefButBitS(block, TOG, 1, B_REDR, "Tune Photons",170,10,140,20, &G.scene->r.GIdirect, 
						0, 0, 0, 0, "Show the photonmap directly in the render for tuning");
			}
		}

	}
}

/* yafray: global  options panel */
static void render_panel_yafrayGlobal()
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "render_panel_yafrayGlobal", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Render", "Render");
	if(uiNewPanel(curarea, block, "YafRay", "Render", 320, 0, 318, 204)==0) return;

	// label to force a boundbox for buttons not to be centered
	uiDefBut(block, LABEL, 0, " ", 305,180,10,10, 0, 0, 0, 0, 0, "");

	uiDefButBitS(block, TOGN, 1, B_REDR, "xml", 5,180,75,20, &G.scene->r.YFexportxml,
					0, 0, 0, 0, "Export to an xml file and call yafray instead of plugin");

	uiDefButF(block, NUMSLI, B_DIFF,"Bi ", 5,35,150,20,	&(G.scene->r.YF_raybias), 
				0.0, 10.0 ,0,0, "Shadow ray bias to avoid self shadowing");
	uiDefButI(block, NUM, B_DIFF, "Raydepth ", 5,60,150,20,
				&G.scene->r.YF_raydepth, 1.0, 80.0, 10, 10, "Maximum render ray depth from the camera");
	uiDefButF(block, NUMSLI, B_DIFF, "Gam ", 5,10,150,20, &G.scene->r.YF_gamma, 0.001, 5.0, 0, 0, "Gamma correction, 1 is off");
	uiDefButF(block, NUMSLI, B_DIFF, "Exp ", 160,10,150,20,&G.scene->r.YF_exposure, 0.0, 10.0, 0, 0, "Exposure adjustment, 0 is off");
        
	/*AA Settings*/
	uiDefButBitS(block, TOGN, 1, B_REDR, "Auto AA", 5,140,150,20, &G.scene->r.YF_AA, 
					0, 0, 0, 0, "Set AA using OSA and GI quality, disable for manual control");
	uiDefButBitS(block, TOGN, 1, B_DIFF, "Clamp RGB", 160,140,150,20, &G.scene->r.YF_clamprgb, 1.0, 8.0, 10, 10, "For AA on fast high contrast changes. Not advisable for Bokeh! Dulls lens shape detail.");
 	if(G.scene->r.YF_AA){
		uiDefButI(block, NUM, B_DIFF, "AA Passes ", 5,115,150,20, &G.scene->r.YF_AApasses, 0, 64, 10, 10, "Number of AA passes (0 is no AA)");
		uiDefButI(block, NUM, B_DIFF, "AA Samples ", 160,115,150,20, &G.scene->r.YF_AAsamples, 0, 2048, 10, 10, "Number of samples per pass");
		uiDefButF(block, NUMSLI, B_DIFF, "Psz ", 5,90,150,20, &G.scene->r.YF_AApixelsize, 1.0, 2.0, 0, 0, "AA pixel filter size");
		uiDefButF(block, NUMSLI, B_DIFF, "Thr ", 160,90,150,20, &G.scene->r.YF_AAthreshold, 0.000001, 1.0, 0, 0, "AA threshold");
	}
}
#endif /* disable yafray stuff */

static void layer_copy_func(void *lay_v, void *lay_p)
{
	unsigned int *lay= lay_p;
	int laybit= GET_INT_FROM_POINTER(lay_v);

	if(G.qual & (LR_SHIFTKEY|LR_CTRLKEY)) {
		if(*lay==0) *lay= 1<<laybit;
	}
	else
		*lay= 1<<laybit;
	
	copy_view3d_lock(REDRAW);
	allqueue(REDRAWBUTSSCENE, 0);
}

static void delete_scene_layer_func(void *srl_v, void *act_i)
{
	if(BLI_countlist(&G.scene->r.layers)>1) {
		long act= (long)act_i;
		
		BLI_remlink(&G.scene->r.layers, srl_v);
		MEM_freeN(srl_v);
		G.scene->r.actlay= 0;
		
		if(G.scene->nodetree) {
			bNode *node;
			for(node= G.scene->nodetree->nodes.first; node; node= node->next) {
				if(node->type==CMP_NODE_R_LAYERS && node->id==NULL) {
					if(node->custom1==act)
						node->custom1= 0;
					else if(node->custom1>act)
						node->custom1--;
				}
			}
		}
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWNODE, 0);
	}
}

static void rename_scene_layer_func(void *srl_v, void *unused_v)
{
	if(G.scene->nodetree) {
		SceneRenderLayer *srl= srl_v;
		bNode *node;
		for(node= G.scene->nodetree->nodes.first; node; node= node->next) {
			if(node->type==CMP_NODE_R_LAYERS && node->id==NULL) {
				if(node->custom1==G.scene->r.actlay)
					BLI_strncpy(node->name, srl->name, NODE_MAXSTR);
			}
		}
	}
	allqueue(REDRAWBUTSSCENE, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWNODE, 0);
}

static char *scene_layer_menu(void)
{
	SceneRenderLayer *srl;
	int len= 32 + 32*BLI_countlist(&G.scene->r.layers);
	short a, nr;
	char *str= MEM_callocN(len, "menu layers");
	
	strcpy(str, "ADD NEW %x32767");
	a= strlen(str);
	for(nr=0, srl= G.scene->r.layers.first; srl; srl= srl->next, nr++) {
		if(srl->layflag & SCE_LAY_DISABLE)
			a+= sprintf(str+a, "|%s %%i%d %%x%d", srl->name, ICON_BLANK1, nr);
		else 
			a+= sprintf(str+a, "|%s %%i%d %%x%d", srl->name, ICON_CHECKBOX_HLT, nr);
	}
	
	return str;
}

static void draw_3d_layer_buttons(uiBlock *block, int type, unsigned int *poin, short xco, short yco, short dx, short dy, char *tip)
{
	uiBut *bt;
	long a;
	
	uiBlockBeginAlign(block);
	for(a=0; a<5; a++) {
		bt= uiDefButBitI(block, type, 1<<a, B_NOP, "",	(short)(xco+a*(dx/2)), yco+dy/2, (short)(dx/2), (short)(dy/2), (int *)poin, 0, 0, 0, 0, tip);
		uiButSetFunc(bt, layer_copy_func, (void *)a, poin);
	}
	for(a=0; a<5; a++) {
		bt=uiDefButBitI(block, type, 1<<(a+10), B_NOP, "",	(short)(xco+a*(dx/2)), yco, (short)(dx/2), (short)(dy/2), (int *)poin, 0, 0, 0, 0, tip);
		uiButSetFunc(bt, layer_copy_func, (void *)(a+10), poin);
	}
	
	xco+= 7;
	uiBlockBeginAlign(block);
	for(a=5; a<10; a++) {
		bt=uiDefButBitI(block, type, 1<<a, B_NOP, "",	(short)(xco+a*(dx/2)), yco+dy/2, (short)(dx/2), (short)(dy/2), (int *)poin, 0, 0, 0, 0, tip);
		uiButSetFunc(bt, layer_copy_func, (void *)a, poin);
	}
	for(a=5; a<10; a++) {
		bt=uiDefButBitI(block, type, 1<<(a+10), B_NOP, "",	(short)(xco+a*(dx/2)), yco, (short)(dx/2), (short)(dy/2), (int *)poin, 0, 0, 0, 0, tip);
		uiButSetFunc(bt, layer_copy_func, (void *)(a+10), poin);
	}
	
	uiBlockEndAlign(block);
}

static void render_panel_layers(void)
{
	uiBlock *block;
	uiBut *bt;
	SceneRenderLayer *srl= BLI_findlink(&G.scene->r.layers, G.scene->r.actlay);
	char *strp;
	
	if(srl==NULL) {
		G.scene->r.actlay= 0;
		srl= G.scene->r.layers.first;
	}
	
	block= uiNewBlock(&curarea->uiblocks, "render_panel_layers", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Output", "Render");
	if(uiNewPanel(curarea, block, "Render Layers", "Render", 320, 0, 318, 204)==0) return;
	
	/* first, as reminder, the scene layers */
	uiDefBut(block, LABEL, 0, "Scene:",				10,170,100,20, NULL, 0, 0, 0, 0, "");
	draw_3d_layer_buttons(block, TOG, &G.scene->lay,		130, 170, 35, 30, "Scene layers to render");
	
	/* layer disable, menu, name, delete button */
	uiBlockBeginAlign(block);
	uiDefIconButBitI(block, ICONTOGN, SCE_LAY_DISABLE, B_REDR, ICON_CHECKBOX_HLT-1,	10, 145, 20, 20, &srl->layflag, 0.0, 0.0, 0, 0, "Disable or enable this RenderLayer");
	strp= scene_layer_menu();
	uiDefButS(block, MENU, B_ADD_RENDERLAYER, strp, 30,145,23,20, &(G.scene->r.actlay), 0, 0, 0, 0, "Choose Active Render Layer");
	MEM_freeN(strp);
	
	/* name max 20, exr format limit... */
	bt= uiDefBut(block, TEX, REDRAWNODE, "",  53,145,172,20, srl->name, 0.0, 20.0, 0, 0, "");
	uiButSetFunc(bt, rename_scene_layer_func, srl, NULL);
	
	uiDefButBitI(block, TOG, R_SINGLE_LAYER, B_NOP, "Single",	230,145,60,20, &G.scene->r.scemode, 0, 0, 0, 0, "Only render this layer");	
	bt=uiDefIconBut(block, BUT, B_NOP, ICON_X,	285, 145, 25, 20, 0, 0, 0, 0, 0, "Deletes current Render Layer");
	uiButSetFunc(bt, delete_scene_layer_func, srl, (void *)(long)G.scene->r.actlay);
	uiBlockEndAlign(block);

	/* RenderLayer visible-layers */
	uiDefBut(block, LABEL, 0, "Layer:",			10,110,100,20, NULL, 0, 0, 0, 0, "");
	draw_3d_layer_buttons(block, BUT_TOGDUAL, &srl->lay,		130,110, 35, 30, "Scene-layers included in this render-layer (Hold CTRL for Z-mask)");
	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, SCE_LAY_ZMASK, B_REDR,"Zmask",	10, 85, 40, 20, &srl->layflag, 0, 0, 0, 0, "Only render what's in front of the solid z values");	
	if(srl->layflag & SCE_LAY_ZMASK)
		uiDefButBitI(block, TOG, SCE_LAY_NEG_ZMASK, B_NOP,"Neg",	10, 65, 40, 20, &srl->layflag, 0, 0, 0, 0, "For Zmask, only render what is behind solid z values instead of in front");
	else
		uiDefButBitI(block, TOG, SCE_LAY_ALL_Z, B_NOP,"AllZ",	10, 65, 40, 20, &srl->layflag, 0, 0, 0, 0, "Fill in Z values for solid faces in invisible layers, for masking");	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, SCE_LAY_SOLID, B_NOP,"Solid",	50,  85, 45, 20, &srl->layflag, 0, 0, 0, 0, "Render Solid faces in this Layer");	
	uiDefButBitI(block, TOG, SCE_LAY_HALO, B_NOP,"Halo",	95,  85, 40, 20, &srl->layflag, 0, 0, 0, 0, "Render Halos in this Layer (on top of Solid)");	
	uiDefButBitI(block, TOG, SCE_LAY_ZTRA, B_NOP,"Ztra",	135, 85, 40, 20, &srl->layflag, 0, 0, 0, 0, "Render Z-Transparent faces in this Layer (On top of Solid and Halos)");	
	uiDefButBitI(block, TOG, SCE_LAY_SKY, B_NOP,"Sky",		175, 85, 40, 20, &srl->layflag, 0, 0, 0, 0, "Render Sky or backbuffer in this Layer");	
	uiDefButBitI(block, TOG, SCE_LAY_EDGE, B_NOP,"Edge",	215, 85, 45, 20, &srl->layflag, 0, 0, 0, 0, "Render Edge-enhance in this Layer (only works for Solid faces)");	
	uiDefButBitI(block, TOG, SCE_LAY_STRAND, B_NOP,"Strand",260, 85, 50, 20, &srl->layflag, 0, 0, 0, 0, "Render Strands in this Layer");	
	
	uiDefIDPoinBut(block, test_grouppoin_but, ID_GR, B_SET_PASS, "Light:",	50, 65, 130, 20, &(srl->light_override), "Name of Group to use as Lamps instead");
	uiDefIDPoinBut(block, test_matpoin_but, ID_MA, B_SET_PASS, "Mat:",	180, 65, 130, 20, &(srl->mat_override), "Name of Material to use as Materials instead");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, SCE_PASS_COMBINED, B_SET_PASS,"Combined",	10, 30, 80, 20, &srl->passflag, 0, 0, 0, 0, "Deliver full combined RGBA buffer");	
	uiDefButBitI(block, TOG, SCE_PASS_Z, B_SET_PASS,"Z",			90, 30, 30, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Z values pass");	
	uiDefButBitI(block, TOG, SCE_PASS_VECTOR, B_SET_PASS,"Vec",		120, 30, 40, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Speed Vector pass");	
	uiDefButBitI(block, TOG, SCE_PASS_NORMAL, B_SET_PASS,"Nor",		160, 30, 40, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Normal pass");	
	uiDefButBitI(block, TOG, SCE_PASS_UV, B_SET_PASS,"UV",			200, 30, 40, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Texture UV pass");	
	uiDefButBitI(block, TOG, SCE_PASS_MIST, B_SET_PASS,"Mist",		240, 30, 35, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Mist factor pass (0-1)");	
	uiDefButBitI(block, TOG, SCE_PASS_INDEXOB, B_SET_PASS,"Index",	275, 30, 35, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Object Index pass");	
	
	uiDefButBitI(block, TOG, SCE_PASS_RGBA, B_SET_PASS,"Col",				10, 10, 35, 20, &srl->passflag, 0, 0, 0, 0, "Deliver shade-less Color pass");	
	uiDefButBitI(block, TOG, SCE_PASS_DIFFUSE, B_SET_PASS,"Diff",			45, 10, 35, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Diffuse pass");	
	uiDefButBitI(block, BUT_TOGDUAL, SCE_PASS_SPEC, B_SET_PASS,"Spec",		80, 10, 40, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Specular pass (Hold Ctrl to exclude from combined)");	
	uiDefButBitI(block, BUT_TOGDUAL, SCE_PASS_SHADOW, B_SET_PASS,"Shad",	120, 10, 40, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Shadow pass (Hold Ctrl to exclude from combined)");	
	uiDefButBitI(block, BUT_TOGDUAL, SCE_PASS_AO, B_SET_PASS,"AO",			160, 10, 30, 20, &srl->passflag, 0, 0, 0, 0, "Deliver AO pass (Hold Ctrl to exclude from combined)");
	uiDefButBitI(block, BUT_TOGDUAL, SCE_PASS_REFLECT, B_SET_PASS,"Refl",	190, 10, 40, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Raytraced Reflection pass (Hold Ctrl to exclude from combined)");	
	uiDefButBitI(block, BUT_TOGDUAL, SCE_PASS_REFRACT, B_SET_PASS,"Refr",	230, 10, 40, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Raytraced Refraction pass (Hold Ctrl to exclude from combined)");	
	uiDefButBitI(block, BUT_TOGDUAL, SCE_PASS_RADIO, B_SET_PASS,"Rad",		270, 10, 40, 20, &srl->passflag, 0, 0, 0, 0, "Deliver Radiosity pass (Hold Ctrl to exclude from combined)");	
}	

void render_panels()
{

	render_panel_output();
	render_panel_layers();
	render_panel_render();
	if(G.rt == 1) render_panel_simplify();
	render_panel_anim();
	render_panel_bake();

	render_panel_format();
	render_panel_stamp();
#ifdef WITH_FFMPEG
       if (G.scene->r.imtype == R_FFMPEG) {
		   render_panel_ffmpeg_video();
	       render_panel_ffmpeg_audio();
       }
#endif

#ifndef DISABLE_YAFRAY
	/* yafray: GI & Global panel, only available when yafray enabled for rendering */
	if (G.scene->r.renderer==R_YAFRAY) {
		if (G.scene->r.YF_gamma==0.0) G.scene->r.YF_gamma=1.0;
		if (G.scene->r.YF_raybias==0.0) G.scene->r.YF_raybias=0.001;
		if (G.scene->r.YF_raydepth==0) G.scene->r.YF_raydepth=5;
		if (G.scene->r.YF_AApixelsize==0.0) G.scene->r.YF_AApixelsize=1.5;
		if (G.scene->r.YF_AAthreshold==0.0) G.scene->r.YF_AAthreshold=0.05;
		if (G.scene->r.GIpower==0.0) G.scene->r.GIpower=1.0;
		if (G.scene->r.GIindirpower==0.0) G.scene->r.GIindirpower=1.0;
		render_panel_yafrayGlobal();
		render_panel_yafrayGI();
	}
#endif

}

/* --------------------------------------------- */

void anim_panels()
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "anim_panel", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Anim", "Anim", 0, 0, 318, 204)==0) return;

	uiBlockBeginAlign(block);
	uiDefButI(block, NUM,B_FRAMEMAP,"Map Old:",	10,160,150,20,&G.scene->r.framapto,1.0,900.0, 0, 0, "Specify old mapping value in frames");
	uiDefButI(block, NUM,B_FRAMEMAP,"Map New:",	160,160,150,20,&G.scene->r.images,1.0,900.0, 0, 0, "Specify how many frames the Map Old will last");

	uiBlockBeginAlign(block);
	uiDefButS(block, NUM,B_FRAMEMAP,"FPS:",  10,130,75,20, &G.scene->r.frs_sec, 1.0, 120.0, 100.0, 0, "Frames per second");
	uiDefButF(block, NUM,B_FRAMEMAP,"/",  85,130,75,20, &G.scene->r.frs_sec_base, 1.0, 120.0, 0.1, 3, "Frames per second base");

	uiDefButBitS(block, TOG, AUDIO_SYNC, B_SOUND_CHANGED, "Sync",160,130,150,20, &G.scene->audio.flag, 0, 0, 0, 0, "Use sample clock for syncing animation to audio");
	
	uiBlockBeginAlign(block);
	uiDefButI(block, NUM,REDRAWALL,"Sta:",	10,100,150,20,&G.scene->r.sfra,1.0,MAXFRAMEF, 0, 0, "Specify the start frame of the animation");
	uiDefButI(block, NUM,REDRAWALL,"End:",	160,100,150,20,&G.scene->r.efra,1.0,MAXFRAMEF, 0, 0, "Specify the end frame of the animation");

	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, REDRAWTIME, "Steps:",10, 70, 150, 20,&(G.scene->jumpframe), 1, 100, 1, 100, "Set spacing between frames changes with up and down arrow keys");


}

/* --------------------------------------------- */

void sound_panels()
{
	bSound *sound;

	/* paranoia check */
	sound = G.buts->lockpoin;
	if(sound && GS(sound->id.name)!=ID_SO) {
		sound= NULL;
		G.buts->lockpoin= NULL;
	}
	
	sound_panel_sound(sound);
	sound_panel_listener();
	sound_panel_sequencer();
}



