/**
 * $Id: 
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

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_userdef_types.h"
#include "DNA_packedFile_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_sound.h"
#include "BKE_packedFile.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

#include "BSE_filesel.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"

#include "BIF_butspace.h"

#include "mydevice.h"
#include "blendef.h"

/* -----includes for this file specific----- */

#include "render.h"
#include "DNA_image_types.h"
#include "BKE_writeavi.h"
#include "BKE_image.h"
#include "BIF_renderwin.h"
#include "BIF_writeimage.h"
#include "BIF_writeavicodec.h"
#include "BIF_editsound.h"
#include "BSE_seqaudio.h"
#include "BSE_headerbuttons.h"
#include "butspace.h" // own module

#ifdef WITH_QUICKTIME
#include "quicktime_export.h"
#endif


/* here the calls for scene buttons
   - render
   - world
   - anim settings, audio
*/


/* ************************ SOUND *************************** */
static void load_new_sample(char *str)	/* called from fileselect */
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
	bSound *sound;
	bSample *sample, *newsample;

	sound = G.buts->lockpoin;

	if (sound) {
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
		}
	}

	allqueue(REDRAWBUTSSCENE, 0);

}


void do_soundbuts(unsigned short event)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
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
		if (G.buts->menunr == -2) {
			if (sound) {
				activate_databrowse((ID *)sound->sample, ID_SAMPLE, 0, B_SOUND_MENU_SAMPLE, &G.buts->menunr, do_soundbuts);
			}
		} else if (G.buts->menunr > 0) {
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
			do_soundbuts(B_SOUND_REDRAW);
		}
		break;

	case B_SOUND_RECALC:
		waitcursor(1);
		sound = G.main->sound.first;
		while (sound) {
			MEM_freeN(sound->stream);
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


static void sound_panel_listener()
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
	xco,yco,195,24,&G.listener->dopplervelocity, 0.0, 10.0, 1.0, 0, "Use this for scaling the doppler effect");

	
}

static void sound_panel_sequencer()
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
	uiDefButS(block, TOG|BIT|1, B_SOUND_CHANGED, "Sync",	xco,yco,115,20, &G.scene->audio.flag, 0, 0, 0, 0, "Use sample clock for syncing animation to audio");
	uiDefButS(block, TOG|BIT|2, B_SOUND_CHANGED, "Scrub",		xco+120,yco,115,20, &G.scene->audio.flag, 0, 0, 0, 0, "Scrub when changing frames");

	yco -= 25;
	uiDefBut(block, LABEL, 0, "Main mix", xco,yco,295,20, 0, 0, 0, 0, 0, "");

	yco -= 25;		
	uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Main (dB): ",
		xco,yco,235,24,&G.scene->audio.main, -24.0, 6.0, 0, 0, "Set the audio master gain/attenuation in dB");

	yco -= 25;
	uiDefButS(block, TOG|BIT|0, 0, "Mute",	xco,yco,235,24, &G.scene->audio.flag, 0, 0, 0, 0, "Mute audio from sequencer");		
	
	yco -= 35;
	uiDefBut(block, BUT, B_SOUND_MIXDOWN, "MIXDOWN",	xco,yco,235,24, 0, 0, 0, 0, 0, "Create WAV file from sequenced audio");
	
}

static void sound_panel_sound(bSound *sound)
{
	static int packdummy=0;
	ID *id, *idfrom;
	uiBlock *block;
	bSample *sample;
	char *strp, str[32], ch[256];

	block= uiNewBlock(&curarea->uiblocks, "sound_panel_sound", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Sound", "Sound", 0, 0, 318, 204)==0) return;
	
	uiDefBut(block, LABEL, 0, "Blender Sound block",10,180,195,20, 0, 0, 0, 0, 0, "");
	
	// warning: abuse of texnr here! (ton didnt code!)
	buttons_active_id(&id, &idfrom);
	std_libbuttons(block, 10, 160, 0, NULL, B_SOUNDBROWSE2, id, idfrom, &(G.buts->texnr), 1, 0, 0, 0, 0);

	uiDefBut(block, BUT, B_SOUND_COPY_SOUND, "Copy sound", 220,160,90,20, 0, 0, 0, 0, 0, "Make another copy (duplicate) of the current sound");

	if (sound) {
	
		uiSetButLock(sound->id.lib!=0, "Can't edit library data");
		sound_initialize_sample(sound);
		sample = sound->sample;

		/* info string */
		if (sound->sample && sound->sample->len) {
			if (sound->sample->channels == 1) strcpy(ch, "Mono");
			else if (sound->sample->channels == 2) strcpy(ch, "Stereo");
			else strcpy(ch, "Unknown");
			
			sprintf(ch, "Sample: %s, %d bit, %d Hz, %d samples", ch, sound->sample->bits, sound->sample->rate, (sound->sample->len/(sound->sample->bits/8)/sound->sample->channels));
			uiDefBut(block, LABEL, 0, ch, 			35,140,225,20, 0, 0, 0, 0, 0, "");
		}
		else {
			uiDefBut(block, LABEL, 0, "Sample: No sample info available.",35,140,225,20, 0, 0, 0, 0, 0, "");
		}

		/* sample browse buttons */

		id= (ID *)sound->sample;
		IDnames_to_pupstring(&strp, NULL, NULL, samples, id, &(G.buts->menunr));
		if (strp[0]) uiDefButS(block, MENU, B_SOUND_MENU_SAMPLE, strp, 10,120,23,20, &(G.buts->menunr), 0, 0, 0, 0, "Select another loaded sample");
		MEM_freeN(strp);
		
		uiDefBut(block, TEX, B_SOUND_NAME_SAMPLE, "",		35,120,225,20, sound->name, 0.0, 79.0, 0, 0, "The sample file used by this Sound");
		
		sprintf(str, "%d", sample->id.us);
		uiDefBut(block, BUT, B_SOUND_UNLINK_SAMPLE, str,	260,120,25,20, 0, 0, 0, 0, 0, "The number of users");
		
		if (sound->sample->packedfile) packdummy = 1;
		else packdummy = 0;
		
		uiDefIconButI(block, TOG|BIT|0, B_SOUND_UNPACK_SAMPLE, ICON_PACKAGE,
			285, 120,25,24, &packdummy, 0, 0, 0, 0,"Pack/Unpack this sample");
		
		uiDefBut(block, BUT, B_SOUND_LOAD_SAMPLE, "Load sample", 10, 95,150,24, 0, 0, 0, 0, 0, "Load a different sample file");

		uiDefBut(block, BUT, B_SOUND_PLAY_SAMPLE, "Play", 	160, 95, 150, 24, 0, 0.0, 0, 0, 0, "Playback sample using settings below");
		
		uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Volume: ",
			10,70,150,20, &sound->volume, 0.0, 1.0, 0, 0, "Set the volume of this sound");

		uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Pitch: ",
			160,70,150,20, &sound->pitch, -12.0, 12.0, 0, 0, "Set the pitch of this sound");

		/* looping */
		uiDefButI(block, TOG|BIT|SOUND_FLAGS_LOOP_BIT, B_SOUND_REDRAW, "Loop",
			10, 50, 95, 20, &sound->flags, 0.0, 0.0, 0, 0, "Toggle between looping on/off");

		if (sound->flags & SOUND_FLAGS_LOOP) {
			uiDefButI(block, TOG|BIT|SOUND_FLAGS_BIDIRECTIONAL_LOOP_BIT, B_SOUND_REDRAW, "Ping Pong",
				105, 50, 95, 20, &sound->flags, 0.0, 0.0, 0, 0, "Toggle between A->B and A->B->A looping");
			
		}
	

		/* 3D settings ------------------------------------------------------------------ */

		if (sound->sample->channels == 1) {
			uiDefButI(block, TOG|BIT|SOUND_FLAGS_3D_BIT, B_SOUND_REDRAW, "3D Sound",
				10, 10, 90, 20, &sound->flags, 0, 0, 0, 0, "Turns 3D sound on");
			
			if (sound->flags & SOUND_FLAGS_3D) {
				uiDefButF(block, NUMSLI, B_SOUND_CHANGED, "Scale: ",
					100,10,210,20, &sound->attenuation, 0.0, 5.0, 1.0, 0, "Sets the surround scaling factor for this sound");
				
			}
		}
	}
}


/* ************************* SCENE *********************** */


static void output_pic(char *name)
{
	strcpy(G.scene->r.pic, name);
	allqueue(REDRAWBUTSSCENE, 0);
}

static void backbuf_pic(char *name)
{
	Image *ima;
	
	strcpy(G.scene->r.backbuf, name);
	allqueue(REDRAWBUTSSCENE, 0);

	ima= add_image(name);
	if(ima) {
		free_image_buffers(ima);	/* force read again */
		ima->ok= 1;
	}
}

static void ftype_pic(char *name)
{
	strcpy(G.scene->r.ftype, name);
	allqueue(REDRAWBUTSSCENE, 0);
}


static void scene_change_set(Scene *sc, Scene *set) {
	if (sc->set!=set) {
		sc->set= set;
		
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
}

static void run_playanim(char *file) {
	extern char bprogname[];	/* usiblender.c */
	char str[FILE_MAXDIR+FILE_MAXFILE];
	int pos[2], size[2];

	calc_renderwin_rectangle(R.winpos, pos, size);

	sprintf(str, "%s -a -p %d %d \"%s\"", bprogname, pos[0], pos[1], file);
	system(str);
}

void do_render_panels(unsigned short event)
{
	ScrArea *sa;
	ID *id;
	char file[FILE_MAXDIR+FILE_MAXFILE];

	switch(event) {

	case B_DORENDER:
		BIF_do_render(0);
		break;
	case B_RTCHANGED:
		allqueue(REDRAWALL, 0);
		break;
	case B_PLAYANIM:
#ifdef WITH_QUICKTIME
		if(G.scene->r.imtype == R_QUICKTIME) 
			makeqtstring(file);
		else
#endif
			makeavistring(file);
		if(BLI_exist(file)) {
			run_playanim(file);
		}
		else {
			makepicstring(file, G.scene->r.sfra);
			if(BLI_exist(file)) {
				run_playanim(file);
			}
			else error("Can't find image: %s", file);
		}
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

	case B_FS_FTYPE:
		sa= closest_bigger_area();
		areawinset(sa->win);
		if(G.qual == LR_CTRLKEY)
			activate_imageselect(FILE_SPECIAL, "SELECT FTYPE", G.scene->r.ftype, ftype_pic);
		else
			activate_fileselect(FILE_SPECIAL, "SELECT FTYPE", G.scene->r.ftype, ftype_pic);
		break;
	
	case B_PR_PAL:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 54;
		G.scene->r.yasp= 51;
		G.scene->r.size= 100;
		G.scene->r.frs_sec= 25;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;
		
		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEWCAM, 0);
		break;

#ifdef WITH_QUICKTIME
	case B_FILETYPEMENU:
		allqueue(REDRAWBUTSSCENE, 0);
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
#else /* libquicktime */
		if(G.scene->r.imtype == R_QUICKTIME) {
			/* i'm not sure if this should be here... */
			/* set default quicktime codec */
			if (!G.scene->r.qtcodecdata) {
				G.scene->r.qtcodecdata = MEM_callocN(sizeof(QtCodecData),   "QtCodecData");
				qtcodec_idx = 1;
			}
			
			qt_init_codecs();
			if (qtcodec_idx < 1) qtcodec_idx = 1;	
			
			G.scene->r.qtcodecdata->fourcc =  qtcodecidx_to_fcc(qtcodec_idx-1);
			qt_init_codecdata(G.scene->r.qtcodecdata);
/* I'm not sure if this is really needed, so don't remove it yet */
#if 0
			/* get index of codec that can handle a given fourcc */
			if (qtcodec_idx < 1)
				qtcodec_idx = get_qtcodec_idx(G.scene->r.qtcodecdata->fourcc)+1;

			/* no suitable codec found, alert user */
			if (qtcodec_idx < -1) {
				error("no suitable codec found!");
				qtcodec_idx = 1;
			}
#endif /* 0 */
		}
#endif /*_WIN32 || __APPLE__ */

	case B_SELECTCODEC:
#if defined (_WIN32) || defined (__APPLE__)
		if ((G.scene->r.imtype == R_QUICKTIME)) { /* || (G.scene->r.qtcodecdata)) */
			notice("Warning: the 'Options' button in the next dialog causes a freeze...");
			get_qtcodec_settings();
		}
#ifdef _WIN32
		else
			get_avicodec_settings();
#endif /* _WIN32 */
#else /* libquicktime */
		if (!G.scene->r.qtcodecdata) {
			G.scene->r.qtcodecdata = MEM_callocN(sizeof(QtCodecData),  "QtCodecData");
			qtcodec_idx = 1;
		}
		if (qtcodec_idx < 1) {
			qtcodec_idx = 1;
			qt_init_codecs();
		}

		G.scene->r.qtcodecdata->fourcc = qtcodecidx_to_fcc(qtcodec_idx-1);
		/* if the selected codec differs from the previous one, reinit it */
		qt_init_codecdata(G.scene->r.qtcodecdata);	
		allqueue(REDRAWBUTSSCENE, 0);
#endif /* _WIN32 || __APPLE__ */
		break;
#endif /* WITH_QUICKTIME */

	case B_PR_FULL:
		G.scene->r.xsch= 1280;
		G.scene->r.ysch= 1024;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
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
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_CDI:
		G.scene->r.xsch= 384;
		G.scene->r.ysch= 280;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.15, 0.85, 0.15, 0.85);
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
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_D2MAC:
		G.scene->r.xsch= 1024;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 1;
		G.scene->r.yasp= 1;
		G.scene->r.size= 50;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_MPEG:
		G.scene->r.xsch= 368;
		G.scene->r.ysch= 272;
		G.scene->r.xasp= 105;
		G.scene->r.yasp= 100;
		G.scene->r.size= 100;
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
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
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.0, 1.0, 0.0, 1.0);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_PRESET:
		G.scene->r.xsch= 720;
		G.scene->r.ysch= 576;
		G.scene->r.xasp= 54;
		G.scene->r.yasp= 51;
		G.scene->r.size= 100;
		G.scene->r.mode= R_OSA+R_SHADOW+R_FIELDS;
		G.scene->r.imtype= R_TARGA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWVIEWCAM, 0);
		allqueue(REDRAWBUTSSCENE, 0);
		break;
	case B_PR_PANO:
		G.scene->r.xsch= 36;
		G.scene->r.ysch= 176;
		G.scene->r.xasp= 115;
		G.scene->r.yasp= 100;
		G.scene->r.size= 100;
		G.scene->r.mode |= R_PANORAMA;
		G.scene->r.xparts=  16;
		G.scene->r.yparts= 1;

		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
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
		G.scene->r.mode &= ~R_PANORAMA;
		G.scene->r.xparts=  G.scene->r.yparts= 1;
		
		BLI_init_rctf(&G.scene->r.safety, 0.1, 0.9, 0.1, 0.9);
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEWCAM, 0);
		break;

	case B_SETBROWSE:
		id= (ID*) G.scene->set;
		
		if (G.buts->menunr==-2) {
			 activate_databrowse(id, ID_SCE, 0, B_SETBROWSE, &G.buts->menunr, do_render_panels);
		} else if (G.buts->menunr>0) {
			Scene *newset= (Scene*) BLI_findlink(&G.main->scene, G.buts->menunr-1);
			
			if (newset==G.scene)
				error("Not allowed");
			else if (newset)
				scene_change_set(G.scene, newset);
		}  
		break;
	case B_CLEARSET:
		scene_change_set(G.scene, NULL);
		break;
	}
}

static uiBlock *edge_render_menu(void *arg_unused)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "edge render", UI_EMBOSS, UI_HELV, curarea->win);
		
	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",  285, -20, 230, 120, NULL,  0, 0, 0, 0, "");
	
	uiDefButS(block, NUM, 0,"Eint:",  295,50,70,19,  &G.scene->r.edgeint, 0.0, 255.0, 0, 0,
		  "Sets edge intensity for Toon shading");
	uiDefButI(block, TOG, 0,"Shift", 365,50,70,19,  &G.compat, 0, 0, 0, 0,
		  "For unified renderer: use old offsets for edges");
	uiDefButI(block, TOG, 0,"All",		435,50,70,19,  &G.notonlysolid, 0, 0, 0, 0,
		  "For unified renderer: also consider transparent faces for toon shading");

	/* colour settings for the toon shading */
	uiDefButF(block, COL, B_EDGECOLSLI, "", 295,-10,30,60,  &(G.scene->r.edgeR), 0, 0, 0, 0, "");
	
	uiDefButF(block, NUMSLI, 0, "R ",   325, 30, 180,19,   &G.scene->r.edgeR, 0.0, 1.0, B_EDGECOLSLI, 0,
		  "For unified renderer: Colour for edges in toon shading mode.");
	uiDefButF(block, NUMSLI, 0, "G ",  325, 10, 180,19,  &G.scene->r.edgeG, 0.0, 1.0, B_EDGECOLSLI, 0,
		  "For unified renderer: Colour for edges in toon shading mode.");
	uiDefButF(block, NUMSLI, 0, "B ",  325, -10, 180,19,  &G.scene->r.edgeB, 0.0, 1.0, B_EDGECOLSLI, 0,
		  "For unified renderer: Colour for edges in toon shading mode.");

	uiDefButS(block, NUM, 0,"AntiShift",   365,70,140,19,  &(G.scene->r.same_mat_redux), 0, 255.0, 0, 0,
		  "For unified renderer: reduce intensity on boundaries "
		  "with identical materials with this number.");
	
	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}

static uiBlock *post_render_menu(void *arg_unused)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "post render", UI_EMBOSS, UI_HELV, curarea->win);
		
	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-10, 10, 200, 80, NULL, 0, 0, 0, 0, "");
	
	uiDefButF(block, NUMSLI, 0,"Add:",		0,60,180,19,  &G.scene->r.postadd, -1.0, 1.0, 0, 0, "");
	uiDefButF(block, NUMSLI, 0,"Mul:",		0,40,180,19,  &G.scene->r.postmul, 0.01, 4.0, 0, 0, "");
	uiDefButF(block, NUMSLI, 0,"Gamma:",		0,20,180,19,  &G.scene->r.postgamma, 0.2, 2.0, 0, 0, "");

	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}


static uiBlock *framing_render_menu(void *arg_unused)
{
	uiBlock *block;
	short yco = 60, xco = 0;
	int randomcolorindex = 1234;

	block= uiNewBlock(&curarea->uiblocks, "framing_options", UI_EMBOSS, UI_HELV, curarea->win);

	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-10, -10, 300, 100, NULL, 0, 0, 0, 0, "");

	uiDefBut(block, LABEL, B_NOP, "Framing:", xco, yco, 68,19, 0, 0, 0, 0, 0, "");
	uiDefButC(block, ROW, 0, "Stretch",	xco += 70, yco, 68, 19, &G.scene->framing.type, 1.0, SCE_GAMEFRAMING_SCALE , 0, 0, "Stretch or squeeze the viewport to fill the display window");
	uiDefButC(block, ROW, 0, "Expose",	xco += 70, yco, 68, 19, &G.scene->framing.type, 1.0, SCE_GAMEFRAMING_EXTEND, 0, 0, "Show the entire viewport in the display window, viewing more horizontally or vertically");
	uiDefButC(block, ROW, 0, "Bars",	    xco += 70, yco, 68, 19, &G.scene->framing.type, 1.0, SCE_GAMEFRAMING_BARS  , 0, 0, "Show the entire viewport in the display window, using bar horizontally or vertically");

	yco -= 20;
	xco = 35;

	uiDefButF(block, COL, randomcolorindex, "",                0, yco - 58 + 18, 33, 58, &G.scene->framing.col[0], 0, 0, 0, 0, "");

	uiDefButF(block, NUMSLI, 0, "R ", xco,yco,243,18, &G.scene->framing.col[0], 0.0, 1.0, randomcolorindex, 0, "Set the red component of the bars");
	yco -= 20;
	uiDefButF(block, NUMSLI, 0, "G ", xco,yco,243,18, &G.scene->framing.col[1], 0.0, 1.0, randomcolorindex, 0, "Set the green component of the bars");
	yco -= 20;
	uiDefButF(block, NUMSLI, 0, "B ", xco,yco,243,18, &G.scene->framing.col[2], 0.0, 1.0, randomcolorindex, 0, "Set the blue component of the bars");

	uiBlockSetDirection(block, UI_TOP);

	return block;
}


static char *imagetype_pup(void)
{
	static char string[1024];
	char formatstring[1024];

	strcpy(formatstring, "Save image as: %%t|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d");

#ifdef __sgi
	strcat(formatstring, "|%s %%x%d");	// add space for Movie
#endif

	strcat(formatstring, "|%s %%x%d");	// add space for PNG

#ifdef _WIN32
	strcat(formatstring, "|%s %%x%d");	// add space for AVI Codec
#endif

#ifdef WITH_QUICKTIME
	if(G.have_quicktime)
		strcat(formatstring, "|%s %%x%d");	// add space for Quicktime
#endif

	if(G.have_quicktime) {
		sprintf(string, formatstring,
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
			"Jpeg",           R_JPEG90,
			"HamX",           R_HAMX,
			"Iris",           R_IRIS,
			"Iris + Zbuffer", R_IRIZ,
			"Ftype",          R_FTYPE,
			"Movie",          R_MOVIE
		);
	} else {
		sprintf(string, formatstring,
			"AVI Raw",        R_AVIRAW,
			"AVI Jpeg",       R_AVIJPEG,
#ifdef _WIN32
			"AVI Codec",      R_AVICODEC,
#endif
			"Targa",          R_TARGA,
			"Targa Raw",      R_RAWTGA,
			"PNG",            R_PNG,
			"Jpeg",           R_JPEG90,
			"HamX",           R_HAMX,
			"Iris",           R_IRIS,
			"Iris + Zbuffer", R_IRIZ,
			"Ftype",          R_FTYPE,
			"Movie",          R_MOVIE
		);
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

static void render_panel_output()
{
	ID *id;
	int a,b;
	uiBlock *block;
	char *strp;


	block= uiNewBlock(&curarea->uiblocks, "render_panel_output", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Output", "Render", 0, 0, 318, 204)==0) return;
	
	uiDefBut(block, TEX,0,"",							30, 170, 268, 19,G.scene->r.pic, 0.0,79.0, 0, 0, "Directory/name to save rendered Pics to");
	uiDefIconBut(block, BUT, B_FS_PIC, ICON_FILESEL,	8, 170, 20, 19, 0, 0, 0, 0, 0, "Open Fileselect to get Pics dir/name");
	uiDefBut(block, TEX,0,"",							30, 148, 268, 19,G.scene->r.backbuf, 0.0,79.0, 0, 0, "Image to use as background for rendering");
	uiDefIconBut(block, BUT,B_FS_BACKBUF, ICON_FILESEL, 8, 148, 20, 19, 0, 0, 0, 0, 0, "Open Fileselect to get Backbuf image");
	uiDefBut(block, TEX,0,"",							30, 125, 268, 19,G.scene->r.ftype,0.0,79.0, 0, 0, "Image to use with FTYPE Image type");
	uiDefIconBut(block, BUT,B_FS_FTYPE, ICON_FILESEL,	8, 125, 20, 19, 0, 0, 0, 0, 0, "Open Fileselect to get Ftype image");
	uiDefIconBut(block, BUT, B_CLEARSET, ICON_X, 		131, 95, 20, 19, 0, 0, 0, 0, 0, "Remove Set link");

	/* SET BUTTON */
	id= (ID *)G.scene->set;
	IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->scene), id, &(G.buts->menunr));
	if(strp[0])
		uiDefButS(block, MENU, B_SETBROWSE, strp, 8, 96, 20, 19, &(G.buts->menunr), 0, 0, 0, 0, "Scene to link as a Set");
	MEM_freeN(strp);

	uiBlockSetCol(block, TH_BUT_SETTING1);

	if(G.scene->set) {
		uiSetButLock(1, NULL);
		uiDefIDPoinBut(block, test_scenepoin_but, 0, "",			25, 97, 104, 19, &(G.scene->set), "Name of the Set");
		uiClearButLock();
	}


	uiDefButS(block, TOG|BIT|0, 0,"Backbuf",	8, 70, 62, 19, &G.scene->r.bufflag, 0, 0, 0, 0, "Enable/Disable use of Backbuf image");	
	
	uiBlockSetCol(block, TH_AUTO);
			
	for(b=0; b<3; b++) 
		for(a=0; a<3; a++)
			uiDefButS(block, TOG|BIT|(3*b+a), 800,"",	(short)(9+18*a),(short)(7+12*b),16,10, &R.winpos, 0, 0, 0, 0, "Render window placement on screen");

	uiDefButS(block, ROW, B_REDR, "DispView",	72, 7, 65, 19, &R.displaymode, 0.0, (float)R_DISPLAYVIEW, 0, 0, "Sets render output to display in 3D view");
	uiDefButS(block, ROW, B_REDR, "DispWin",	139, 7, 62, 19, &R.displaymode, 0.0, (float)R_DISPLAYWIN, 0, 0, "Sets render output to display in a seperate window");

	uiDefButS(block, TOG|BIT|4, 0, "Extensions",	228, 8, 67, 18, &G.scene->r.scemode, 0.0, 0.0, 0, 0, "Adds extensions to the output when rendering animations");

	/* Toon shading buttons */
	uiDefButS(block, TOG|BIT|5, 0,"Edge",	154, 70, 47, 19, &G.scene->r.mode, 0, 0, 0, 0, "Enable Toon shading");
	uiDefBlockBut(block, edge_render_menu, NULL, "Edge Settings |>> ", 204, 71, 93, 19, "Display edge settings");

	/* unified render buttons */
	if(G.scene->r.mode & R_UNIFIED) {
		uiDefBlockBut(block, post_render_menu, NULL, "Post process |>> ", 205, 48, 92, 19, "Only for unified render");
		if (G.scene->r.mode & R_GAMMA) {
			uiDefButF(block, NUMSLI, 0,"Gamma:",		8, 48, 143, 19,
					 &(G.scene->r.gamma), 0.2, 5.0, B_GAMMASLI, 0,
					 "The gamma value for blending oversampled images (1.0 = no correction).");
		}		
	}
}

static void render_panel_render()
{
	uiBlock *block;


	block= uiNewBlock(&curarea->uiblocks, "render_panel_render", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Render", "Render", 320, 0, 318, 204)==0) return;

 	uiDefBut(block, BUT,B_DORENDER,"RENDER",	369,142,192,47, 0, 0, 0, 0, 0, "Start the rendering");
	
	uiDefButS(block, TOG|BIT|0, 0, "OSA",		369,114,124,20,&G.scene->r.mode, 0, 0, 0, 0, "Enables Oversampling (Anti-aliasing)");
	uiDefButF(block, NUM,B_DIFF,"Bf:",							495,90,65,20,&G.scene->r.blurfac, 0.01, 5.0, 10, 0, "Sets motion blur factor");
	uiDefButS(block, TOG|BIT|14, 0, "MBLUR",	495,114,66,20,&G.scene->r.mode, 0, 0, 0, 0, "Enables Motion Blur calculation");
	
	uiDefButS(block, ROW,B_DIFF,"5",			369,90,29,20,&G.scene->r.osa,2.0,5.0, 0, 0, "Sets oversample level to 5");
	uiDefButS(block, ROW,B_DIFF,"8",			400,90,29,20,&G.scene->r.osa,2.0,8.0, 0, 0, "Sets oversample level to 8 (Recommended)");
	uiDefButS(block, ROW,B_DIFF,"11",			431,90,33,20,&G.scene->r.osa,2.0,11.0, 0, 0, "Sets oversample level to 11");
	uiDefButS(block, ROW,B_DIFF,"16",			466,90,28,20,&G.scene->r.osa,2.0,16.0, 0, 0, "Sets oversample level to 16");
		
	uiDefButS(block, NUM,B_DIFF,"Xparts:",		369,42,99,31,&G.scene->r.xparts,1.0, 64.0, 0, 0, "Sets the number of horizontal parts to render image in (For panorama sets number of camera slices)");
	uiDefButS(block, NUM,B_DIFF,"Yparts:",		472,42,86,31,&G.scene->r.yparts,1.0, 64.0, 0, 0, "Sets the number of vertical parts to render image in");

	uiDefButS(block, ROW,800,"Sky",		369,11,38,24,&G.scene->r.alphamode,3.0,0.0, 0, 0, "Fill background with sky");
	uiDefButS(block, ROW,800,"Premul",	410,11,54,24,&G.scene->r.alphamode,3.0,1.0, 0, 0, "Multiply alpha in advance");
	uiDefButS(block, ROW,800,"Key",		467,11,44,24,&G.scene->r.alphamode,3.0,2.0, 0, 0, "Alpha and colour values remain unchanged");

	uiDefButS(block, TOG|BIT|1,0,"Shadow",	565,167,61,22, &G.scene->r.mode, 0, 0, 0, 0, "Enable shadow calculation");
	uiDefButS(block, TOG|BIT|4,0,"EnvMap",	626,167,61,22, &G.scene->r.mode, 0, 0, 0, 0, "Enable environment map renering");
	uiDefButS(block, TOG|BIT|10,0,"Pano",	565,142,61,22, &G.scene->r.mode, 0, 0, 0, 0, "Enable panorama rendering (output width is multiplied by Xparts)");
	uiDefButS(block, TOG|BIT|8,0,"Radio",	626,142,61,22, &G.scene->r.mode, 0, 0, 0, 0, "Enable radiosity rendering");
	
	uiDefButS(block, ROW,B_DIFF,"100%",			565,114,121,20,&G.scene->r.size,1.0,100.0, 0, 0, "Set render size to defined size");
	uiDefButS(block, ROW,B_DIFF,"75%",			565,90,36,20,&G.scene->r.size,1.0,75.0, 0, 0, "Set render size to 3/4 of defined size");
	uiDefButS(block, ROW,B_DIFF,"50%",			604,90,40,20,&G.scene->r.size,1.0,50.0, 0, 0, "Set render size to 1/2 of defined size");
	uiDefButS(block, ROW,B_DIFF,"25%",			647,90,39,20,&G.scene->r.size,1.0,25.0, 0, 0, "Set render size to 1/4 of defined size");

	uiDefButS(block, TOG|BIT|6,0,"Fields", 564,42,90,31,&G.scene->r.mode, 0, 0, 0, 0, "Enables field rendering");

	uiDefButS(block, TOG|BIT|13,0,"Odd",	655,57,30,16,&G.scene->r.mode, 0, 0, 0, 0, "Enables Odd field first rendering (Default: Even field)");
	uiDefButS(block, TOG|BIT|7,0,"x",		655,42,30,15,&G.scene->r.mode, 0, 0, 0, 0, "Disables time difference in field calculations");

	uiDefButS(block, TOG|BIT|9,REDRAWVIEWCAM, "Border",	565,11,58,24, &G.scene->r.mode, 0, 0, 0, 0, "Render a small cut-out of the image");
	uiDefButS(block, TOG|BIT|2,0, "Gamma",	626,11,58,24, &G.scene->r.mode, 0, 0, 0, 0, "Enable gamma correction");


}

static void render_panel_anim()
{
	uiBlock *block;


	block= uiNewBlock(&curarea->uiblocks, "render_panel_anim", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Anim", "Render", 640, 0, 318, 204)==0) return;


	uiDefBut(block, BUT,B_DOANIM,"ANIM",		692,142,192,47, 0, 0, 0, 0, 0, "Start rendering a sequence");
	
	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|0, 0, "Do Sequence",	692,114,192,20, &G.scene->r.scemode, 0, 0, 0, 0, "Enables sequence output rendering (Default: 3D rendering)");
	uiDefButS(block, TOG|BIT|1, 0, "Render Daemon",	692,90,192,20, &G.scene->r.scemode, 0, 0, 0, 0, "Let external network render current scene");
	
	uiBlockSetCol(block, TH_AUTO);
	uiDefBut(block, BUT,B_PLAYANIM, "PLAY",	692,40,94,33, 0, 0, 0, 0, 0, "Play animation of rendered images/avi (searches Pics: field)");
	uiDefButS(block, NUM, B_RTCHANGED, "rt:",	790,40,95,33, &G.rt, -1000.0, 1000.0, 0, 0, "General testing/debug button");

	uiDefButS(block, NUM,REDRAWSEQ,"Sta:",	692,10,94,24, &G.scene->r.sfra,1.0,18000.0, 0, 0, "The start frame of the animation");
	uiDefButS(block, NUM,REDRAWSEQ,"End:",	790,10,95,24, &G.scene->r.efra,1.0,18000.0, 0, 0, "The end  frame of the animation");

}

static void render_panel_format()
{
	uiBlock *block;
	int yofs;


	block= uiNewBlock(&curarea->uiblocks, "render_panel_format", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Format", "Render", 960, 0, 318, 204)==0) return;

	uiDefBlockBut(block, framing_render_menu, NULL, "Game framing settings |>> ", 892, 169, 227, 20, "Display game framing settings");

	uiDefButS(block, NUM,REDRAWVIEWCAM,"SizeX:",	892 ,136,112,27, &G.scene->r.xsch, 4.0, 10000.0, 0, 0, "The image width in pixels");
	uiDefButS(block, NUM,REDRAWVIEWCAM,"SizeY:",	1007,136,112,27, &G.scene->r.ysch, 4.0,10000.0, 0, 0, "The image height in scanlines");
	uiDefButS(block, NUM,REDRAWVIEWCAM,"AspX:",	892 ,114,112,20, &G.scene->r.xasp, 1.0,200.0, 0, 0, "The horizontal aspect ratio");
	uiDefButS(block, NUM,REDRAWVIEWCAM,"AspY:",	1007,114,112,20, &G.scene->r.yasp, 1.0,200.0, 0, 0, "The vertical aspect ratio");
	

	yofs = 54;

#ifdef __sgi
	yofs = 76;
	uiDefButS(block, NUM,B_DIFF,"MaxSize:", 892,32,165,20, &G.scene->r.maximsize, 0.0, 500.0, 0, 0, "Maximum size per frame to save in an SGI movie");
	uiDefButS(block, TOG|BIT|12,0,"Cosmo", 1059,32,60,20, &G.scene->r.mode, 0, 0, 0, 0, "Attempt to save SGI movies using Cosmo hardware");
#endif

	uiDefButS(block, MENU,B_FILETYPEMENU,imagetype_pup(),	892,yofs,174,20, &G.scene->r.imtype, 0, 0, 0, 0, "Images are saved in this file format");
	uiDefButS(block, TOG|BIT|11,0, "Crop",          1068,yofs,51,20, &G.scene->r.mode, 0, 0, 0, 0, "Exclude border rendering from total image");

	yofs -= 22;

	if(G.scene->r.quality==0) G.scene->r.quality= 90;

#ifdef WITH_QUICKTIME
	if (G.scene->r.imtype == R_AVICODEC || G.scene->r.imtype == R_QUICKTIME) {
#else /* WITH_QUICKTIME */
	if (0) {
#endif
		if(G.scene->r.imtype == R_QUICKTIME) {
#ifdef WITH_QUICKTIME
#if defined (_WIN32) || defined (__APPLE__)
			//glColor3f(0.65, 0.65, 0.7);
			//glRecti(892,yofs+46,892+225,yofs+45+20);
			if(G.scene->r.qtcodecdata == NULL)
				uiDefBut(block, LABEL, 0, "Codec: not set",  892,yofs+44,225,20, 0, 0, 0, 0, 0, "");
			else
				uiDefBut(block, LABEL, 0, G.scene->r.qtcodecdata->qtcodecname,  892,yofs+44,225,20, 0, 0, 0, 0, 0, "");
			uiDefBut(block, BUT,B_SELECTCODEC, "Set codec",  892,yofs,112,20, 0, 0, 0, 0, 0, "Set codec settings for Quicktime");
#else /* libquicktime */
			if (!G.scene->r.qtcodecdata) G.scene->r.qtcodecdata = MEM_callocN(sizeof(QtCodecData), "QtCodecData");
			uiDefButI(block, MENU, B_SELECTCODEC, qtcodecs_pup(), 892,yofs, 112, 20, &qtcodec_idx, 0, 0, 0, 0, "Codec");
			/* make sure the codec stored in G.scene->r.qtcodecdata matches the selected
			 * one, especially if it's not set.. */
			if (!G.scene->r.qtcodecdata->fourcc) {
				G.scene->r.qtcodecdata->fourcc = qtcodecidx_to_fcc(qtcodec_idx-1);
				qt_init_codecdata(G.scene->r.qtcodecdata);	
			}
			
			yofs -= 22;
			uiDefBlockBut(block, qtcodec_menu, NULL, "Codec Settings |>> ", 892,yofs, 227, 20, "Edit Codec settings for QuickTime");
			yofs +=22;

#endif /* libquicktime */
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
			uiDefBut(block, BUT,B_SELECTCODEC, "Set codec",  892,yofs,112,20, 0, 0, 0, 0, 0, "Set codec settings for AVI");
		}
	} else {
		uiDefButS(block, NUM,0, "Quality:",           892,yofs,112,20, &G.scene->r.quality, 10.0, 100.0, 0, 0, "Quality setting for JPEG images, AVI Jpeg and SGI movies");
	}
	uiDefButS(block, NUM,REDRAWALL,"Frs/sec:",   1006,yofs,113,20, &G.scene->r.frs_sec, 1.0, 120.0, 100.0, 0, "Frames per second");



	uiDefButS(block, ROW,B_DIFF,"BW",			892, 10,74,20, &G.scene->r.planes, 5.0,(float)R_PLANESBW, 0, 0, "Images are saved with BW (grayscale) data");
	uiDefButS(block, ROW,B_DIFF,"RGB",		    968, 10,74,20, &G.scene->r.planes, 5.0,(float)R_PLANES24, 0, 0, "Images are saved with RGB (color) data");
	uiDefButS(block, ROW,B_DIFF,"RGBA",		   1044, 10,75,20, &G.scene->r.planes, 5.0,(float)R_PLANES32, 0, 0, "Images are saved with RGB and Alpha data (if supported)");


	uiDefBut(block, BUT,B_PR_PAL, "PAL",		1146,170,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 720x576, Aspect ratio - 54x51, 25 fps");
	uiDefBut(block, BUT,B_PR_NTSC, "NTSC",		1146,150,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 720x480, Aspect ratio - 10x11, 30 fps");
	uiDefBut(block, BUT,B_PR_PRESET, "Default",	1146,130,100,18, 0, 0, 0, 0, 0, "Same as PAL, with render settings (OSA, Shadows, Fields)");
	uiDefBut(block, BUT,B_PR_PRV, "Preview",	1146,110,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 640x512, Render size 50%");
	uiDefBut(block, BUT,B_PR_PC, "PC",			1146,90,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 640x480, Aspect ratio - 100x100");
	uiDefBut(block, BUT,B_PR_PAL169, "PAL 16:9",1146,70,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 720x576, Aspect ratio - 64x45");
	uiDefBut(block, BUT,B_PR_PANO, "PANO",		1146,50,100,18, 0, 0, 0, 0, 0, "Standard panorama settings");
	uiDefBut(block, BUT,B_PR_FULL, "FULL",		1146,30,100,18, 0, 0, 0, 0, 0, "Size preset: Image size - 1280x1024, Aspect ratio - 1x1");
	uiDefButS(block, TOG|BIT|15, B_REDR, "Unified Renderer", 1146,10,100,18,  &G.scene->r.mode, 0, 0, 0, 0, "Use the unified renderer.");

}


void render_panels()
{

	render_panel_output();
	render_panel_render();
	render_panel_anim();
	render_panel_format();
	
}

/* --------------------------------------------- */

void anim_panels()
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "anim_panel", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Anim", "Anim", 0, 0, 318, 204)==0) return;

	uiDefButS(block, NUM,REDRAWSEQ,"Sta:",	320,17,93,27,&G.scene->r.sfra,1.0,18000.0, 0, 0, "Specify the start frame of the animation");
	uiDefButS(block, NUM,REDRAWSEQ,"End:",	416,17,95,27,&G.scene->r.efra,1.0,18000.0, 0, 0, "Specify the end frame of the animation");

	uiDefButS(block, NUM,B_FRAMEMAP,"Map Old:",	320,69,93,22,&G.scene->r.framapto,1.0,900.0, 0, 0, "Specify old map value in frames");
	uiDefButS(block, NUM,B_FRAMEMAP,"Map New:",	416,69,95,22,&G.scene->r.images,1.0,900.0, 0, 0, "Specify new map value in frames");

	uiDefButS(block, NUM,REDRAWSEQ,"Frs/sec:",   320,47,93,19, &G.scene->r.frs_sec, 1.0, 120.0, 100.0, 0, "Frames per second");
	
	uiDefButS(block, TOG|BIT|1, B_SOUND_CHANGED, "Sync",	416,47,95,19, &G.scene->audio.flag, 0, 0, 0, 0, "Use sample clock for syncing animation to audio");


}

/* --------------------------------------------- */

void sound_panels()
{
	bSound *sound;
	
	sound = G.buts->lockpoin;
	if ((sound) && (sound->flags & SOUND_FLAGS_SEQUENCE)) sound = NULL;

	sound_panel_sound(sound);
	sound_panel_listener();
	sound_panel_sequencer();
}



