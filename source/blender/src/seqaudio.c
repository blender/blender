/**
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
 * Contributor(s): intrr
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif   

#include <fcntl.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

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

#include "mydevice.h"
#include "interface.h"
#include "blendef.h"

void audio_fill(void *mixdown, Uint8 *sstream, int len);
/* ************ GLOBALS ************* */

int audio_pos;
SDL_AudioSpec wav_spec;
int audio_scrub=0;
int audio_playing=0;
/////
//


void makewavstring (char *string) 
{
	char txt[64];

	if (string==0) return;

	strcpy(string, G.scene->r.pic);
	BLI_convertstringcode(string, G.sce, G.scene->r.cfra);

	RE_make_existing_file(string);

	if (strcasecmp(string + strlen(string) - 4, ".wav")) {
		sprintf(txt, "%04d_%04d.wav", (G.scene->r.sfra) , (G.scene->r.efra) );
		strcat(string, txt);
	}
}

void audio_mixdown()
{
	int file, c, totlen, totframe, i, oldcfra, cfra2;
	char *buf;
	Editing *ed;
	Sequence *seq;

	buf = MEM_mallocN(65536, "audio_mixdown");
	makewavstring(buf);

	file = open(buf, O_BINARY+O_WRONLY+O_CREAT+O_TRUNC, 0666);

	if(file == -1) 
	{
		error("Cannot open output file!");
		return;
	}

	strcpy(buf, "RIFFlengWAVEfmt fmln01ccRATEbsecBP16dataDLEN");
	totframe = (EFRA - SFRA + 1);
	totlen = (int) ( ((float)totframe / (float)G.scene->r.frs_sec) * (float)G.scene->audio.mixrate * 4.0);
	printf("totlen %x\n", totlen);
	memcpy(buf+4, &totlen, 4);
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
	for (CFRA = SFRA, i = 0; (CFRA<=EFRA); 
	     CFRA=(int) ( ((float)(audio_pos-64)/( G.scene->audio.mixrate*4 ))*(float)G.scene->r.frs_sec )) {
		if (cfra2 != CFRA) {
			cfra2 = CFRA;
			set_timecursor(CFRA);
		}
		memset(buf+i, 0, 64);
		ed= G.scene->ed;
		if (ed) {
			seq= ed->seqbasep->first;
			while(seq) {
				if ((seq->type == SEQ_SOUND) && (seq->ipo)
				  &&(seq->startdisp<=G.scene->r.cfra+2) && (seq->enddisp>G.scene->r.cfra)) do_seq_ipo(seq);
				seq= seq->next;
			}
		}		
		audio_fill(buf+i, NULL, 64);
		if (G.order == B_ENDIAN) {
			swab(buf+i, buf+i, 64);
		}
		if (i == (65536-64)) {
			i=0;
			write(file, buf, 65536);			
		} else i+=64;
	}
	write(file, buf, i);
	waitcursor(0);
	CFRA = oldcfra;
	close(file);
	MEM_freeN(buf);

	return;
}

void audio_levels(Uint8 *buf, int len, float db, float facf, float pan)
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
	sound->stream = MEM_mallocN((int) ((float)sound->streamlen * 1.05), "stream");
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

void audio_fill(void *mixdown, Uint8 *sstream, int len)
{    
	static Editing *ed;
	static Sequence *seq;
	static Uint8* cvtbuf;
	static bSound* sound;
	static float facf;


	ed= G.scene->ed;
	if((ed) && (!(G.scene->audio.flag & AUDIO_MUTE))) {
		seq= ed->seqbasep->first;
		while(seq) {
			if ( (seq->type == SEQ_SOUND) &&
			     (seq->sound) &&
			     (!(seq->flag & SEQ_MUTE)))
   			{
   				sound = seq->sound;
   				audio_makestream(sound);
   				if ((seq->curpos<sound->streamlen -len) && (seq->curpos>=0) &&
   				    (seq->startdisp <= CFRA) && ((seq->enddisp) > CFRA))
  				{
					if(seq->ipo && seq->ipo->curve.first) facf = seq->facf0; else facf = 1.0;
					cvtbuf = MEM_callocN(len, "cvtbuf");					
					memcpy(cvtbuf, ((Uint8*)sound->stream)+(seq->curpos & (~3)), len);
					audio_levels(cvtbuf, len, seq->level, facf, seq->pan);
					if (!mixdown) {
						SDL_MixAudio(sstream, cvtbuf, len, SDL_MIX_MAXVOLUME);
					} else {
						SDL_MixAudio((Uint8*)mixdown, cvtbuf, len, SDL_MIX_MAXVOLUME);					
					}
					MEM_freeN(cvtbuf);						
  				}
    			seq->curpos += len;
			}
			seq= seq->next;
		}	
	}

       
    audio_pos += len;    
    if (audio_scrub) { 
    	audio_scrub--;
    	if (!audio_scrub) {
    		audiostream_stop();
    	}
    }
}    

int audio_init(SDL_AudioSpec *desired)
{
	SDL_AudioSpec *obtained, *hardware_spec;

	SDL_CloseAudio();

	obtained = (SDL_AudioSpec*)MEM_mallocN(sizeof(SDL_AudioSpec), "SDL_AudioSpec");

	desired->callback=audio_fill;

	if ( SDL_OpenAudio(desired, obtained) < 0 ){
	  fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
	  return 0;
	}
	hardware_spec=obtained;
	
	MEM_freeN(obtained);

	SDL_PauseAudio(1);
	return 1;
}

void audiostream_play(Uint32 startframe, Uint32 duration, int mixdown)
{
	static SDL_AudioSpec desired;
	Editing *ed;
	Sequence *seq;
	bSound *sound;

	ed= G.scene->ed;
	if(ed) {
		seq= ed->seqbasep->first;
		while(seq) {
			if ((seq->type == SEQ_SOUND) && (seq->sound))
			{
				sound = ((bSound*)seq->sound);
				seq->curpos = (int)( (((float)((float)startframe-(float)seq->start)/(float)G.scene->r.frs_sec)*((float)G.scene->audio.mixrate)*4 ));
			}
			seq= seq->next;
		}	
	}

   	if (!(duration + mixdown)) {
   		desired.freq=G.scene->audio.mixrate;
		desired.format=AUDIO_S16SYS;
   		desired.channels=2;
   		desired.samples=U.mixbufsize;
   		desired.userdata=0;	
   		if (audio_init(&desired)==0) {
   			U.mixbufsize = 0;	/* no audio */
   		}
   	}
    audio_pos = ( ((int)( (((float)startframe)/(float)G.scene->r.frs_sec)*(G.scene->audio.mixrate)*4 )) & (~3) );		

    audio_scrub = duration;
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
	
	pos = (int) ( ((float)(audio_pos-U.mixbufsize)/( G.scene->audio.mixrate*4 ))*(float)G.scene->r.frs_sec );
	if (pos<1) pos=1;
	return ( pos );
}

