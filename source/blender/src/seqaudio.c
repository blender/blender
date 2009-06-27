/*
 *  $Id$
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
 * Contributor(s): intrr, Peter Schlaile
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#define __USE_XOPEN /* Needed for swab on linux */
#include <unistd.h>
#undef __USE_XOPEN
#else
#include <io.h>
#endif   

#include <fcntl.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_userdef_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_ipo_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_library.h" 
#include "BKE_blender.h" 
#include "BKE_main.h"    
#include "BKE_ipo.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"

#include "BSE_view.h"
#include "BSE_seqaudio.h"
#include "BIF_editsound.h"

#include "mydevice.h"
#include "blendef.h"


static void audio_fill(void *mixdown, uint8_t *sstream, int len);
/* ************ GLOBALS ************* */

static int audio_pos;
static int audio_scrub=0;
static int audio_playing=0;
static int audio_initialised=0;
static int audio_startframe=0;
static double audio_starttime = 0.0;
static Scene * audio_scene = 0; /* we can't use G.scene, since
				   Sequence Scene strips can change G.scene
				   (and SDL-audio-fill callback can be
				   called while we have G.scene changed!)
				*/

#define AFRA2TIME(a)           ((((double) audio_scene->r.frs_sec_base) * (a)) / audio_scene->r.frs_sec)
#define ATIME2FRA(a)           ((((double) audio_scene->r.frs_sec) * (a)) / audio_scene->r.frs_sec_base)

/* we do currently stereo 16 bit mixing only */
#define AUDIO_CHANNELS 2
#define SAMPLE_SIZE (AUDIO_CHANNELS * sizeof(short))

/////
//
/* local protos ------------------- */
void audio_mixdown(void);

void makewavstring (char *string) 
{
	char txt[64];

	if (string==0) return;

	strcpy(string, G.scene->r.pic);
	BLI_convertstringcode(string, G.sce);

	BLI_make_existing_file(string);

	if (BLI_strcasecmp(string + strlen(string) - 4, ".wav")) {
		sprintf(txt, "%04d_%04d.wav", (G.scene->r.sfra) , (G.scene->r.efra) );
		strcat(string, txt);
	}
}

void audio_mixdown()
{
#ifndef DISABLE_SDL
	int file, c, totlen, totframe, i, oldcfra;
	char *buf;

	buf = MEM_mallocN(65536, "audio_mixdown");
	makewavstring(buf);

	file = open(buf, O_BINARY+O_WRONLY+O_CREAT+O_TRUNC, 0666);

	if(file == -1) 
	{
		error("Can't open output file");
		return;
	}
	
	waitcursor(1);
	
	printf("Saving: %s ", buf);

	strcpy(buf, "RIFFlengWAVEfmt fmln01ccRATEbsecBP16dataDLEN");
	totframe = (EFRA - SFRA + 1);
	totlen = (int) ( FRA2TIME(totframe) 
			 * (float)G.scene->audio.mixrate * SAMPLE_SIZE);
	printf(" totlen %d\n", totlen+36+8);
	
	totlen+= 36;	/* len is filesize-8 in WAV spec, total header is 44 bytes */
	memcpy(buf+4, &totlen, 4);
	totlen-= 36;
	
	buf[16] = 0x10; buf[17] = buf[18] = buf[19] = 0; buf[20] = 1; buf[21] = 0;
	buf[22] = 2; buf[23]= 0;
	memcpy(buf+24, &G.scene->audio.mixrate, 4);
	i = G.scene->audio.mixrate * SAMPLE_SIZE;
	memcpy(buf+28, &i, 4);
	buf[32] = 4; buf[33] = 0; buf[34] = 16; buf[35] = 0;
	i = totlen;
	memcpy(buf+40, &i, 4);

	if (G.order == B_ENDIAN) {
		/* swap the four ints to little endian */
		
		/* length */
		SWITCH_INT(buf[4]);
		
		/* audio rate */
		SWITCH_INT(buf[24]);

		/* audio mixrate * 4 */
		SWITCH_INT(buf[28]);
		
		/* length */
		SWITCH_INT(buf[40]);
	}
	
	c = write(file, buf, 44);
	
	oldcfra = CFRA;
	audiostream_play(SFRA, 0, 1);
	
	i= 0;
	while ( totlen > 0 ) {
		totlen -= 64;
		
		memset(buf+i, 0, 64);
		
		CFRA=(int) ( ((float)(audio_pos-64)
			      / ( G.scene->audio.mixrate*SAMPLE_SIZE ))*FPS );
			
		audio_fill(buf+i, NULL, 64);
		if (G.order == B_ENDIAN) {
			char tbuf[64];
			memcpy(tbuf, buf+i, 64);
			swab(tbuf, buf+i, 64);
		}
		if (i == (65536-64)) {
			i=0;
			write(file, buf, 65536);			
		} 
		else i+=64;
	}
	write(file, buf, i);
	
	waitcursor(0);
	CFRA = oldcfra;
	close(file);
	MEM_freeN(buf);

	return;
#endif
}

void audiostream_fill(uint8_t *mixdown, int len)
{    
	int oldcfra = CFRA;
	int i;

	memset(mixdown, 0, len);
#ifndef DISABLE_SDL
	for (i = 0; i < len; i += 64) {
		CFRA = (int) ( ((float)(audio_pos-64)
				/( audio_scene->audio.mixrate * SAMPLE_SIZE ))
			       * FPS );

		audio_fill(mixdown + i, NULL, 
			   (len - i) > 64 ? 64 : (len - i));
	}

	CFRA = oldcfra;
#endif
}


static void audio_levels(uint8_t *buf, int len, float db, 
			 float facf_start, float facf_end, float pan)
{
	int i;
	double m = (facf_end - facf_start) / len;
	float facl, facr, fac;
	signed short *sample;
	
	if (pan>=0) { facr = 1.0; facl = 1.0-pan; }
	       else { facr = pan+1.0; facl = 1.0; }
	
	fac = pow(10.0, ((-(db+audio_scene->audio.main))/20.0));

	for (i = 0; i < len; i += SAMPLE_SIZE) {
		float facf = facf_start + ((double) i) * m;
		float f_l = facl / (fac / facf);
		float f_r = facr / (fac / facf);

		sample = (signed short*)(buf+i);
		sample[0] = (short) ((float)sample[0] * f_l);
		sample[1] = (short) ((float)sample[1] * f_r);
	}
}

/* convert mono/stereo and sampling rate, alloc a buffer for
 * sound->stream to contain the new sample, and set sound->streamlen
 * accordingly.
 */
void audio_makestream(bSound *sound)
{
	signed short *source, *dest;
	float ratio;
	int i;

	if ( (!sound)||(sound->stream)||(!sound->sample)||(!G.scene) ) {
		return;
	}
	ratio = (float)G.scene->audio.mixrate / (float)sound->sample->rate;
	sound->streamlen = (int) ( (float)sound->sample->len * ratio 
				   * AUDIO_CHANNELS 
				   / ((float)sound->sample->channels) );
	sound->stream = malloc((int) ((float)sound->streamlen * 1.05));
	if (sound->sample->rate == G.scene->audio.mixrate) {
		if (sound->sample->channels == AUDIO_CHANNELS) {
			memcpy(sound->stream, 
			       sound->sample->data, sound->streamlen);
		   	return;
		} else if (sound->sample->channels == 1) {
			for (source = (signed short*)(sound->sample->data),
			     dest = (signed short*)(sound->stream),
				 i=0;
				 i<sound->streamlen/SAMPLE_SIZE;
			     dest += 2, source++, i++) {
				int j;
				for (j = 0; j < AUDIO_CHANNELS; j++) {
					dest[j] = source[0];
				}
			}
			return;
		} else {
			fprintf(stderr, "audio_makestream: "
				"FIXME: can't handle number of channels %d\n",
				sound->sample->channels);
			return;
		}
	}
	if (sound->sample->channels == 1) {
		for (dest = (signed short*)(sound->stream), i=0, 
			     source = (signed short*)(sound->sample->data); 
		     i<(sound->streamlen/SAMPLE_SIZE); 
		     dest += AUDIO_CHANNELS, i++) {
			int j;
			int s = source[(int)((float)i/ratio)];
			for (j = 0; j < AUDIO_CHANNELS; j++) {
				dest[j] = s;
			}
		}
	}
	else if (sound->sample->channels == 2) {
		for (dest=(signed short*)(sound->stream), i=0, 
			     source = (signed short*)(sound->sample->data); 
		     i<(sound->streamlen / 2); dest += AUDIO_CHANNELS, i+=2) {
			dest[1] = source[(int)((float)i/ratio)];
			dest[0] = source[(int)((float)i/ratio)+1];
		}
	}	
}

#ifndef DISABLE_SDL

static int fra2curpos(Sequence * seq, int cfra)
{
	return (int)( (AFRA2TIME(((double) cfra) -
				 ((double) seq->start) +
				 ((double) 
				  seq->anim_startofs))
		       * ((float)audio_scene
			  ->audio.mixrate)
		       * SAMPLE_SIZE));
}

static int curpos2fra(Sequence * seq, int curpos)
{
	return ((int) floor(
			ATIME2FRA(
				((double) curpos) / SAMPLE_SIZE 
				/audio_scene->audio.mixrate)))
		- seq->anim_startofs + seq->start;
}

static int get_curpos(Sequence * seq, int cfra)
{
	return audio_pos + 
		(((int)((FRA2TIME(((double) cfra) 
				 - ((double) audio_scene->r.cfra)
				 - ((double) seq->start) 
				 + ((double) seq->anim_startofs))
			* ((float)audio_scene->audio.mixrate)
			* SAMPLE_SIZE )))
		 & (~(SAMPLE_SIZE - 1))); /* has to be sample aligned! */
}

static void do_audio_seq_ipo(Sequence * seq, int len, float * facf_start,
			     float * facf_end, int cfra)
{
	int seq_curpos = get_curpos(seq, cfra);
	int cfra_start = curpos2fra(seq, seq_curpos);
	int cfra_end = cfra_start + 1;
	int ipo_curpos_start = fra2curpos(seq, curpos2fra(seq, seq_curpos));
	int ipo_curpos_end = fra2curpos(seq, cfra_end);
	double ipo_facf_start;
	double ipo_facf_end;
	double m;

	do_seq_ipo(seq, cfra_start);
	ipo_facf_start = seq->facf0;

	do_seq_ipo(seq, cfra_end);
	ipo_facf_end = seq->facf0;

	m = (ipo_facf_end- ipo_facf_start)/(ipo_curpos_end - ipo_curpos_start);
	
	*facf_start = ipo_facf_start + (seq_curpos - ipo_curpos_start) * m;
	*facf_end = ipo_facf_start + (seq_curpos + len-ipo_curpos_start) * m;
}

#endif

#ifndef DISABLE_SDL
static void audio_fill_ram_sound(Sequence *seq, void * mixdown, 
				 uint8_t * sstream, int len,
				 int cfra)
{
	uint8_t* cvtbuf;
	bSound* sound;
	float facf_start;
	float facf_end;
	int seq_curpos = get_curpos(seq, cfra);

	/* catch corner case at the beginning of strip */
	if (seq_curpos < 0 && (seq_curpos + len > 0)) {
		seq_curpos *= -1;
		len -= seq_curpos;
		sstream += seq_curpos;
		seq_curpos = 0;
	}

	sound = seq->sound;
	audio_makestream(sound);
	if ((seq_curpos < sound->streamlen -len) && (seq_curpos >= 0) &&
	    (seq->startdisp <= cfra) && ((seq->enddisp) > cfra))
	{
		if(seq->ipo && seq->ipo->curve.first) {
			do_audio_seq_ipo(seq, len, &facf_start, &facf_end,
					 cfra);
		} else {
			facf_start = 1.0;
			facf_end = 1.0;
		}
		cvtbuf = malloc(len);					
		memcpy(cvtbuf, ((uint8_t*)sound->stream)+(seq_curpos), len);
		audio_levels(cvtbuf, len, seq->level, facf_start, facf_end, 
			     seq->pan);
		if (!mixdown) {
			SDL_MixAudio(sstream, cvtbuf, len, SDL_MIX_MAXVOLUME);
		} else {
			SDL_MixAudio((uint8_t*)mixdown, cvtbuf, len, SDL_MIX_MAXVOLUME);
		}
		free(cvtbuf);
	}
}
#endif

#ifndef DISABLE_SDL
static void audio_fill_hd_sound(Sequence *seq, 
				void * mixdown, uint8_t * sstream, 
				int len, int cfra)
{
	uint8_t* cvtbuf;
	float facf_start;
	float facf_end;
	int seq_curpos = get_curpos(seq, cfra);

	/* catch corner case at the beginning of strip */
	if (seq_curpos < 0 && (seq_curpos + len > 0)) {
		seq_curpos *= -1;
		len -= seq_curpos;
		sstream += seq_curpos;
		seq_curpos = 0;
	}

	if ((seq_curpos >= 0) &&
	    (seq->startdisp <= cfra) && ((seq->enddisp) > cfra))
	{
		if(seq->ipo && seq->ipo->curve.first) {
			do_audio_seq_ipo(seq, len, &facf_start, &facf_end,
					 cfra);
		} else {
			facf_start = 1.0;
			facf_end = 1.0;
		}
		cvtbuf = malloc(len);
		
		sound_hdaudio_extract(seq->hdaudio, (short*) cvtbuf,
				      seq_curpos / SAMPLE_SIZE,
				      audio_scene->audio.mixrate,
				      AUDIO_CHANNELS,
				      len / SAMPLE_SIZE);
		audio_levels(cvtbuf, len, seq->level, facf_start, facf_end,
			     seq->pan);
		if (!mixdown) {
			SDL_MixAudio(sstream, 
				     cvtbuf, len, SDL_MIX_MAXVOLUME);
		} else {
			SDL_MixAudio((uint8_t*)mixdown, 
				     cvtbuf, len, SDL_MIX_MAXVOLUME);
		}
		free(cvtbuf);
	}
}
#endif

#ifndef DISABLE_SDL
static void audio_fill_seq(Sequence * seq, void * mixdown,
			   uint8_t *sstream, int len, int cfra);

static void audio_fill_scene_strip(Sequence * seq, void * mixdown,
				   uint8_t *sstream, int len, int cfra)
{
	Editing *ed;

	/* prevent eternal loop */
	seq->scene->r.scemode |= R_RECURS_PROTECTION;

	ed = seq->scene->ed;

	if (ed) {
		int sce_cfra = seq->sfra + seq->anim_startofs
			+ cfra - seq->startdisp;

		audio_fill_seq(ed->seqbasep->first,
			       mixdown,
			       sstream, len, sce_cfra);
	}
	
	/* restore */
	seq->scene->r.scemode &= ~R_RECURS_PROTECTION;
}
#endif

#ifndef DISABLE_SDL
static void audio_fill_seq(Sequence * seq, void * mixdown,
			   uint8_t *sstream, int len, int cfra)
{
	while(seq) {
		if (seq->type == SEQ_META &&
		    (!(seq->flag & SEQ_MUTE))) {
			if (seq->startdisp <= cfra && seq->enddisp > cfra) {
				audio_fill_seq(seq->seqbase.first,
					       mixdown, sstream, len, 
					       cfra);
			}
		}
		if (seq->type == SEQ_SCENE 
		    && (!(seq->flag & SEQ_MUTE))
		    && seq->scene
		    && (seq->scene->r.scemode & R_DOSEQ)
		    && !(seq->scene->r.scemode & R_RECURS_PROTECTION)) {
			if (seq->startdisp <= cfra && seq->enddisp > cfra) {
				audio_fill_scene_strip(
					seq, mixdown, sstream, len,
					cfra);
			}
		}
		if ( (seq->type == SEQ_RAM_SOUND) &&
		     (seq->sound) &&
		     (!(seq->flag & SEQ_MUTE))) {
			audio_fill_ram_sound(seq, mixdown, sstream, len, cfra);
		}
		if ( (seq->type == SEQ_HD_SOUND) &&
		     (!(seq->flag & SEQ_MUTE)))	{
			if (!seq->hdaudio) {
				char name[FILE_MAXDIR+FILE_MAXFILE];
				
				BLI_join_dirfile(name, seq->strip->dir, seq->strip->stripdata->name);
				BLI_convertstringcode(name, G.sce);
				
				seq->hdaudio= sound_open_hdaudio(name);
			}
			if (seq->hdaudio) {
				audio_fill_hd_sound(seq, mixdown, sstream, len,
						    cfra);
			}
		}
		seq = seq->next;
	}
}
#endif

#ifndef DISABLE_SDL
static void audio_fill(void *mixdown, uint8_t *sstream, int len)
{    
	Editing *ed;
	Sequence *seq;

	if (!audio_scene) {
		return;
	}

	ed = audio_scene->ed;
	if((ed) && (!(audio_scene->audio.flag & AUDIO_MUTE))) {
		seq = ed->seqbasep->first;
		audio_fill_seq(seq, mixdown, sstream, len, 
			       audio_scene->r.cfra);
	}
       
	audio_pos += len;

	if (audio_scrub > 0) { 
		audio_scrub-= len;
		if (audio_scrub <= 0) {
			audiostream_stop();
		}
	}
}    
#endif

#ifndef DISABLE_SDL
static int audio_init(SDL_AudioSpec *desired)
{
	SDL_AudioSpec *obtained, *hardware_spec;

	SDL_CloseAudio();

	obtained = (SDL_AudioSpec*)MEM_mallocN(sizeof(SDL_AudioSpec), 
					       "SDL_AudioSpec");
	audio_initialised = 0;
	desired->callback=audio_fill;

	if ( SDL_OpenAudio(desired, obtained) < 0 ) {
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
		if (obtained) MEM_freeN(obtained);
		return 0;
	}
	audio_initialised = 1;
	hardware_spec=obtained;
	
	MEM_freeN(obtained);

	SDL_PauseAudio(0);
	return 1;
}
#endif

static int audiostream_play_seq(Sequence * seq, int startframe)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
	int have_sound = 0;

	while(seq) {
		if (seq->type == SEQ_META) {
			if (audiostream_play_seq(
				    seq->seqbase.first, startframe)) {
				have_sound = 1;
			}
		}
		if (seq->type == SEQ_SCENE
		    && seq->scene
		    && (seq->scene->r.scemode & R_DOSEQ)
		    && !(seq->scene->r.scemode & R_RECURS_PROTECTION)) {
			Editing *ed;

			/* prevent eternal loop */
			seq->scene->r.scemode |= R_RECURS_PROTECTION;

			ed = seq->scene->ed;

			if (ed) {
				int sce_cfra = seq->sfra + seq->anim_startofs
					+ startframe - seq->startdisp;

				if (audiostream_play_seq(ed->seqbasep->first,
							 sce_cfra)) {
					have_sound = 1;
				}
			}
	
			/* restore */
			seq->scene->r.scemode &= ~R_RECURS_PROTECTION;
		}
		if ((seq->type == SEQ_RAM_SOUND) && (seq->sound)) {
			have_sound = 1;
		}
		if ((seq->type == SEQ_HD_SOUND)) {
			if (!seq->hdaudio) {
				strncpy(name, seq->strip->dir, FILE_MAXDIR-1);
				strncat(name, seq->strip->stripdata->name, 
					FILE_MAXFILE-1);
				
				seq->hdaudio = sound_open_hdaudio(name);
			}
		}
		seq= seq->next;
	}
	return have_sound;
}

static void audiostream_reset_recurs_protection()
{
	Scene * sce = G.main->scene.first;

	while(sce) {
		sce->r.scemode &= ~R_RECURS_PROTECTION;
		sce= sce->id.next;
	}
	/* fix for silly case, when people try testing with
	   the same scene ... */
	audio_scene->r.scemode |= R_RECURS_PROTECTION;
}

void audiostream_play(int startframe, uint32_t duration, int mixdown)
{
#ifndef DISABLE_SDL
	static SDL_AudioSpec desired;
	Editing *ed;
	int have_sound = 0;

	audio_scene = G.scene;

	audiostream_reset_recurs_protection();

	ed= audio_scene->ed;
	if(ed) {
		have_sound = 
			audiostream_play_seq(ed->seqbasep->first, startframe);
	}

	if(have_sound) {
		/* this call used to be in startup */
		sound_init_audio();
	}

   	if (U.mixbufsize && 
	    (!audio_initialised 
	     || desired.freq != audio_scene->audio.mixrate
	     || desired.samples != U.mixbufsize) 
	    && !mixdown) {
   		desired.freq=audio_scene->audio.mixrate;
		desired.format=AUDIO_S16SYS;
   		desired.channels=2;
   		desired.samples=U.mixbufsize;
   		desired.userdata=0;

   		audio_init(&desired);
   	}

	audio_startframe = startframe;
	audio_pos = ( ((int)( FRA2TIME(startframe)
			      *(audio_scene->audio.mixrate)*4 )) & (~3) );
	audio_starttime = PIL_check_seconds_timer();

	/* if audio already is playing, just reseek, otherwise
	   remember scrub-duration */
	if (!(audio_playing && !(audio_scrub > 0))) {
		audio_scrub = duration;
	}
	if (!mixdown) {
		SDL_PauseAudio(0);
		audio_playing++;
	}
#endif
}

void audiostream_start(int frame)
{
	audiostream_play(frame, 0, 0);
}

void audiostream_scrub(int frame)
{
	audiostream_play(frame, 4096, 0);
}

void audiostream_stop(void)
{
#ifndef DISABLE_SDL
	SDL_PauseAudio(1);
	audio_playing=0;
#endif
}

int audiostream_pos(void) 
{
	int pos;

	if (audio_initialised && audio_scene) {
		pos = (int) (((double)(audio_pos-U.mixbufsize)
			      / ( audio_scene->audio.mixrate*4 ))
			     * FPS );
	} else { /* fallback to seconds_timer when no audio available */
		pos = (int) ((PIL_check_seconds_timer() - audio_starttime) 
			     * FPS);
	}

	if (pos < audio_startframe) pos = audio_startframe;
	return ( pos );
}

