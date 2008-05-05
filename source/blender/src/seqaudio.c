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


void audio_fill(void *mixdown, Uint8 *sstream, int len);
/* ************ GLOBALS ************* */

static int audio_pos;
static int audio_scrub=0;
static int audio_playing=0;
static int audio_initialised=0;
static int audio_startframe=0;
static double audio_starttime = 0.0;
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
	totlen = (int) ( FRA2TIME(totframe) * (float)G.scene->audio.mixrate * 4.0);
	printf(" totlen %d\n", totlen+36+8);
	
	totlen+= 36;	/* len is filesize-8 in WAV spec, total header is 44 bytes */
	memcpy(buf+4, &totlen, 4);
	totlen-= 36;
	
	buf[16] = 0x10; buf[17] = buf[18] = buf[19] = 0; buf[20] = 1; buf[21] = 0;
	buf[22] = 2; buf[23]= 0;
	memcpy(buf+24, &G.scene->audio.mixrate, 4);
	i = G.scene->audio.mixrate * 4;
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
		
		CFRA=(int) ( ((float)(audio_pos-64)/( G.scene->audio.mixrate*4 ))*FPS );
			
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
}

void audiostream_fill(Uint8 *mixdown, int len)
{    
	int oldcfra = CFRA;
	int i;

	memset(mixdown, 0, len);

	for (i = 0; i < len; i += 64) {
		CFRA = (int) ( ((float)(audio_pos-64)
				/( G.scene->audio.mixrate*4 ))
			       * FPS );

		audio_fill(mixdown + i, NULL, 
			   (len - i) > 64 ? 64 : (len - i));
	}

	CFRA = oldcfra;
}


static void audio_levels(Uint8 *buf, int len, float db, float facf, float pan)
{
	int i;
	float facl, facr, fac;
	signed short *sample;
	
	if (pan>=0) { facr = 1.0; facl = 1.0-pan; }
	       else { facr = pan+1.0; facl = 1.0; }
	
	fac = pow(10.0, ((-(db+G.scene->audio.main))/20.0)) / facf;
	facl /= fac;
	facr /= fac;
	
	for (i=0; i<len; i+=4) {
		sample = (signed short*)(buf+i);
		sample[0] = (short) ((float)sample[0] * facl);
		sample[1] = (short) ((float)sample[1] * facr);
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
	sound->streamlen = (int) ( (float)sound->sample->len * ratio * 2.0/((float)sound->sample->channels) );
	sound->stream = malloc((int) ((float)sound->streamlen * 1.05));
	if (sound->sample->rate == G.scene->audio.mixrate) {
		if (sound->sample->channels == 2) {
			memcpy(sound->stream, sound->sample->data, sound->streamlen);
		   	return;
		} else {
			for (source = (signed short*)(sound->sample->data),
			     dest = (signed short*)(sound->stream),
				 i=0;
				 i<sound->streamlen/4;
				 dest += 2, source++, i++) dest[0] = dest[1] = source[0];
			return;
		}
	}
	if (sound->sample->channels == 1) {
		for (dest=(signed short*)(sound->stream), i=0, source=(signed short*)(sound->sample->data); 
		     i<(sound->streamlen/4); dest+=2, i++)
			dest[0] = dest[1] = source[(int)((float)i/ratio)];
	}
	else if (sound->sample->channels == 2) {
		for (dest=(signed short*)(sound->stream), i=0, source=(signed short*)(sound->sample->data); 
		     i<(sound->streamlen/2); dest+=2, i+=2) {
			dest[1] = source[(int)((float)i/ratio)];
			dest[0] = source[(int)((float)i/ratio)+1];			
		}
	}	
}

static void audio_fill_ram_sound(Sequence *seq, void * mixdown, 
				 Uint8 * sstream, int len)
{
	Uint8* cvtbuf;
	bSound* sound;
	float facf;

	sound = seq->sound;
	audio_makestream(sound);
	if ((seq->curpos<sound->streamlen -len) && (seq->curpos>=0) &&
	    (seq->startdisp <= CFRA) && ((seq->enddisp) > CFRA))
	{
		if(seq->ipo && seq->ipo->curve.first) {
			do_seq_ipo(seq, CFRA);
			facf = seq->facf0;
		} else {
			facf = 1.0;
		}
		cvtbuf = malloc(len);					
		memcpy(cvtbuf, ((Uint8*)sound->stream)+(seq->curpos & (~3)), len);
		audio_levels(cvtbuf, len, seq->level, facf, seq->pan);
		if (!mixdown) {
			SDL_MixAudio(sstream, cvtbuf, len, SDL_MIX_MAXVOLUME);
		} else {
			SDL_MixAudio((Uint8*)mixdown, cvtbuf, len, SDL_MIX_MAXVOLUME);
		}
		free(cvtbuf);
	}
	seq->curpos += len;
}

static void audio_fill_hd_sound(Sequence *seq, 
				void * mixdown, Uint8 * sstream, 
				int len)
{
	Uint8* cvtbuf;
	float facf;

	if ((seq->curpos >= 0) &&
	    (seq->startdisp <= CFRA) && ((seq->enddisp) > CFRA))
	{
		if(seq->ipo && seq->ipo->curve.first) {
			do_seq_ipo(seq, CFRA);
			facf = seq->facf0; 
		} else {
			facf = 1.0;
		}
		cvtbuf = malloc(len);
		
		sound_hdaudio_extract(seq->hdaudio, (short*) cvtbuf,
				      seq->curpos / 4,
				      G.scene->audio.mixrate,
				      2,
				      len / 4);
		audio_levels(cvtbuf, len, seq->level, facf, seq->pan);
		if (!mixdown) {
			SDL_MixAudio(sstream, 
				     cvtbuf, len, SDL_MIX_MAXVOLUME);
		} else {
			SDL_MixAudio((Uint8*)mixdown, 
				     cvtbuf, len, SDL_MIX_MAXVOLUME);
		}
		free(cvtbuf);
	}
	seq->curpos += len;
}

static void audio_fill_seq(Sequence * seq, void * mixdown,
			   Uint8 *sstream, int len, int advance_only)
{
	while(seq) {
		if (seq->type == SEQ_META &&
		    (!(seq->flag & SEQ_MUTE))) {
			if (seq->startdisp <= CFRA && seq->enddisp > CFRA) {
				audio_fill_seq(seq->seqbase.first,
					       mixdown, sstream, len, 
					       advance_only);
			} else {
				audio_fill_seq(seq->seqbase.first,
					       mixdown, sstream, len, 
					       1);
			}
		}
		if ( (seq->type == SEQ_RAM_SOUND) &&
		     (seq->sound) &&
		     (!(seq->flag & SEQ_MUTE))) {
			if (advance_only) {
				seq->curpos += len;
			} else {
				audio_fill_ram_sound(
					seq, mixdown, sstream, len);
			}
		}
		if ( (seq->type == SEQ_HD_SOUND) &&
		     (!(seq->flag & SEQ_MUTE)))	{
			if (advance_only) {
				seq->curpos += len;
			} else {
				if (!seq->hdaudio) {
					char name[FILE_MAXDIR+FILE_MAXFILE];

					strncpy(name, seq->strip->dir, 
						FILE_MAXDIR-1);
					strncat(name, 
						seq->strip->stripdata->name, 
						FILE_MAXFILE-1);
					BLI_convertstringcode(name, G.sce);
				
					seq->hdaudio= sound_open_hdaudio(name);
				}
				if (seq->hdaudio) {
					audio_fill_hd_sound(seq, mixdown, 
							    sstream, len);
				}
			}
		}
		seq = seq->next;
	}
}

void audio_fill(void *mixdown, Uint8 *sstream, int len)
{    
	Editing *ed;
	Sequence *seq;

	ed = G.scene->ed;
	if((ed) && (!(G.scene->audio.flag & AUDIO_MUTE))) {
		seq = ed->seqbasep->first;
		audio_fill_seq(seq, mixdown, sstream, len, 0);
	}
       
	audio_pos += len;    
	if (audio_scrub) { 
		audio_scrub--;
		if (!audio_scrub) {
			audiostream_stop();
		}
	}
}    

static int audio_init(SDL_AudioSpec *desired)
{
	SDL_AudioSpec *obtained, *hardware_spec;

	SDL_CloseAudio();

	obtained = (SDL_AudioSpec*)MEM_mallocN(sizeof(SDL_AudioSpec), 
					       "SDL_AudioSpec");

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

static int audiostream_play_seq(Sequence * seq, Uint32 startframe)
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
		if ((seq->type == SEQ_RAM_SOUND) && (seq->sound)) {
			have_sound = 1;
			seq->curpos = (int)( (FRA2TIME(
						      (double) startframe -
						      (double) seq->start +
						      (double) 
						      seq->anim_startofs)
					      * ((float)G.scene->audio.mixrate)
					      * 4 ));
		}
		if ((seq->type == SEQ_HD_SOUND)) {
			have_sound = 1;
			if (!seq->hdaudio) {
				strncpy(name, seq->strip->dir, FILE_MAXDIR-1);
				strncat(name, seq->strip->stripdata->name, 
					FILE_MAXFILE-1);
				
				seq->hdaudio = sound_open_hdaudio(name);
			}
			seq->curpos = (int)( (FRA2TIME((double) startframe - 
						       (double) seq->start +
						       (double)
						       seq->anim_startofs)
					      * ((float)G.scene->audio.mixrate)
					      * 4 ));
		}
		seq= seq->next;
	}
	return have_sound;
}

void audiostream_play(Uint32 startframe, Uint32 duration, int mixdown)
{
	static SDL_AudioSpec desired;
	Editing *ed;
	int have_sound = 0;

	ed= G.scene->ed;
	if(ed) {
		have_sound = 
			audiostream_play_seq(ed->seqbasep->first, startframe);
	}

	if(have_sound) {
		/* this call used to be in startup */
		sound_init_audio();
	}

   	if (U.mixbufsize && !audio_initialised && !mixdown) {
   		desired.freq=G.scene->audio.mixrate;
		desired.format=AUDIO_S16SYS;
   		desired.channels=2;
   		desired.samples=U.mixbufsize;
   		desired.userdata=0;

   		if (audio_init(&desired)==0) {
   			U.mixbufsize = 0;	/* no audio */
   		}
   	}

	audio_startframe = startframe;
	audio_pos = ( ((int)( FRA2TIME(startframe)
			      *(G.scene->audio.mixrate)*4 )) & (~3) );
	audio_starttime = PIL_check_seconds_timer();

	/* if audio already is playing, just reseek, otherwise
	   remember scrub-duration */
	if (!(audio_playing && !audio_scrub)) {
		audio_scrub = duration;
	}
	if (!mixdown) {
		SDL_PauseAudio(0);
		audio_playing++;
	}
}

void audiostream_start(Uint32 frame)
{
	audiostream_play(frame, 0, 0);
}

void audiostream_scrub(Uint32 frame)
{
	if (U.mixbufsize) audiostream_play(frame, 4096/U.mixbufsize, 0);
}

void audiostream_stop(void)
{
	SDL_PauseAudio(1);
	audio_playing=0;
}

int audiostream_pos(void) 
{
	int pos;

	if (U.mixbufsize) {
		pos = (int) (((double)(audio_pos-U.mixbufsize)
			      / ( G.scene->audio.mixrate*4 ))
			     * FPS );
	} else { /* fallback to seconds_timer when no audio available */
		pos = (int) ((PIL_check_seconds_timer() - audio_starttime) 
			     * FPS);
	}

	if (pos < audio_startframe) pos = audio_startframe;
	return ( pos );
}

