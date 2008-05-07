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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32 
#define __USE_XOPEN /* Needed for swab on linux */
#include <unistd.h>
#undef __USE_XOPEN
#else

#include <io.h>
#endif
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_userdef_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sound.h"
#include "BKE_library.h"
#include "BKE_packedFile.h"

#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_interface.h"
#include "BIF_editsound.h"
#include "BIF_mywindow.h"
#include "BIF_toolbox.h"

#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_time.h"

#include "BDR_editobject.h"

#include "blendef.h"

#include "mydevice.h"

#include "SND_C-api.h"
#include "SND_DependKludge.h"

#include "SYS_System.h"

#include "PIL_time.h"


/* this might move to the external header */
void* sound_get_libraryinterface(void);

static SND_SceneHandle ghSoundScene=NULL;
static SND_AudioDeviceInterfaceHandle ghAudioDeviceInterface=NULL;

/* que? why only here? because of the type define? */
bSound *sound_find_sound(char *id_name);
void sound_read_wav_data(bSound * sound, PackedFile * pf);
void sound_stop_sound(void *object, bSound *sound);
void winqreadsoundspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);
/*  void sound_stop_all_sounds(void); already in BIF_editsound.h */



/* Right. Now for some implementation: */
void winqreadsoundspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	SpaceSound *ssound= spacedata;
	unsigned short event= evt->event;
	short val= evt->val;
	float dx, dy;
	int doredraw= 0, cfra, first = 0;
	short mval[2], nr;
	short mousebut = L_MOUSE;
	
	if(curarea->win==0) return;

	if(val) {
		
		if( uiDoBlocks(&curarea->uiblocks, event, 1)!=UI_NOTHING ) event= 0;

		/* swap mouse buttons based on user preference */
		if (U.flag & USER_LMOUSESELECT) {
			if (event == LEFTMOUSE) {
				event = RIGHTMOUSE;
				mousebut = L_MOUSE;
			} else if (event == RIGHTMOUSE) {
				event = LEFTMOUSE;
				mousebut = R_MOUSE;
			}
		}

		switch(event) {
		case LEFTMOUSE:
			ssound->flag |= SND_CFRA_NUM;
			do {
				getmouseco_areawin(mval);
				areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
				
				cfra = (int)(dx+0.5f);
				if(cfra< 1) cfra= 1;
				
				if( cfra!=CFRA || first )
				{
					first= 0;
					CFRA= cfra;
					update_for_newframe();
					force_draw_plus(SPACE_VIEW3D, 1);
				}
				else PIL_sleep_ms(30);
			
			} while(get_mbut() & mousebut);
			ssound->flag &= ~SND_CFRA_NUM;
			
			doredraw= 1;

			break;
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
		case RIGHTMOUSE:
			{
				TimeMarker *marker;
				
				getmouseco_areawin(mval);
				areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);

				marker = find_nearest_marker(SCE_MARKERS, 0);
				if (marker) {
					if ((G.qual & LR_SHIFTKEY)==0)
						deselect_markers(0, 0);
						
					if (marker->flag & SELECT)
						marker->flag &= ~SELECT;
					else
						marker->flag |= SELECT;
				}
				
				force_draw(0);
				std_rmouse_transform(transform_markers);
			}
			break;
			
		case PADPLUSKEY:
			dx= (float)(0.1154*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin+= dx;
			G.v2d->cur.xmax-= dx;
			test_view2d(G.v2d, curarea->winx, curarea->winy);
			view2d_do_locks(curarea, V2D_LOCK_COPY);
			doredraw= 1;
			break;
		case PADMINUS:
			dx= (float)(0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin-= dx;
			G.v2d->cur.xmax+= dx;
			test_view2d(G.v2d, curarea->winx, curarea->winy);
			view2d_do_locks(curarea, V2D_LOCK_COPY);
			doredraw= 1;
			break;
		case HOMEKEY:
			do_sound_buttons(B_SOUNDHOME);
			break;
			
		case PAGEUPKEY: /* cfra to next marker */
			nextprev_marker(1);
			break;
		case PAGEDOWNKEY: /* cfra to prev marker */
			nextprev_marker(-1);
			break;
		
		case AKEY: /* select/deselect all  */
			deselect_markers(1, 0);
			
			allqueue(REDRAWMARKER, 0);
			break;
			
		case BKEY: /* borderselect markers */
			borderselect_markers();
			break;
		
		case DKEY: /* duplicate selected marker(s) */
			if (G.qual & LR_SHIFTKEY) {
				duplicate_marker();
				
				allqueue(REDRAWMARKER, 0);
			}
			break;
			
		case GKEY:
			transform_markers('g', 0);
			break;
			
		case MKEY: /* add marker or rename first selected */
			if (G.qual & LR_CTRLKEY)
				rename_marker();
			else
				add_marker(CFRA);
			
			allqueue(REDRAWMARKER, 0);
			break;		
		
		case TKEY: /* toggle time display */
			nr= pupmenu("Time value%t|Frames %x1|Seconds%x2");
			if (nr>0) {
				if(nr==1) ssound->flag |= SND_DRAWFRAMES;
				else ssound->flag &= ~SND_DRAWFRAMES;
				doredraw= 1;
			}

			break;
			
		case DELKEY: /* delete selected markers */
		case XKEY:
			if (okee("Erase selected")) {
				remove_marker();
				allqueue(REDRAWMARKER, 0);
			}
			break;
		}
	}

	if(doredraw)
		scrarea_queue_winredraw(curarea);
}



void sound_initialize_sounds(void)
{
	bSound *sound;

	if(ghSoundScene) {

		/* clear the soundscene */
		SND_RemoveAllSounds(ghSoundScene);
		SND_RemoveAllSamples(ghSoundScene);
	}
	
	/* initialize sample blocks (doesnt call audio system, needs to be done once after load */
	sound = G.main->sound.first;
	while (sound) {
		sound_sample_is_null(sound);
		sound = (bSound *) sound->id.next;
	}
}



bSound *sound_make_copy(bSound *originalsound)
{
	bSound *sound = NULL;
	char name[160];
	int len;
	
	if(ghSoundScene==NULL) sound_init_audio();
	
	/* only copy sounds that are sounds */
	if (originalsound)
	{
		/* do some name magic */
		strcpy(name, originalsound->name);
		len = strlen(name);
		while ((len > 0) && (name[len - 1] != '/') && (name[len - 1] != '\\'))
			len--;
		
		/* allocate the needed memory */
		sound = alloc_libblock(&G.main->sound, ID_SO, name + len);
		
		/* create a soundobject */
		sound->snd_sound = SND_CreateSound();

		/* set the samplename */
		strcpy(sound->name, name);
		SND_SetSampleName(sound->snd_sound, sound->name);

		/* add the new object to the soundscene */
		SND_AddSound(ghSoundScene, sound->snd_sound);
		
		/* and copy the data from the original */
		sound->attenuation = originalsound->attenuation;
		sound->distance = originalsound->distance;
		sound->max_gain = originalsound->max_gain;
		sound->min_gain = originalsound->min_gain;
		sound->newpackedfile = originalsound->newpackedfile;
		sound->panning = originalsound->panning;
		sound->pitch = originalsound->pitch;
		sound->sample = originalsound->sample;
		sound->volume = originalsound->volume;
		
		if (originalsound->flags & SOUND_FLAGS_3D)
			sound->flags |= SOUND_FLAGS_3D;
		else
			sound->flags &= ~SOUND_FLAGS_3D;
	}
	
	return sound;
}



void sound_initialize_sample(bSound *sound)
{
	if(ghSoundScene==NULL) sound_init_audio();

	if (sound && sound->sample == NULL)
		sound_sample_is_null(sound);
}


void sound_read_wav_data(bSound *sound, PackedFile *pf)
{
	int i, temp;
	short shortbuf, *temps;
	int longbuf;
	char buffer[25];
	char *data = NULL;
	char *tempc;
	bSample *sample = NULL;
	int channels, rate, bits, len;
	
	/* prepare for the worst... */
	sound->sample->type = SAMPLE_INVALID;

	rewindPackedFile(pf);

	/* check to see if it is a file in "RIFF WAVE fmt" format */
	if (readPackedFile(pf, buffer, 16) != 16) {
		if (G.f & G_DEBUG) printf("File too short\n");
		return;
	}

	if(!(memcmp(buffer, "RIFF", 4) && memcmp(&(buffer[8]), "WAVEfmt ", 8))) {
		readPackedFile(pf, &i, 4);// start of data
		if(G.order==B_ENDIAN)
			SWITCH_INT(i);

		/* read the sampleformat */
		readPackedFile(pf, &shortbuf, 2);
		if(G.order==B_ENDIAN) {
			SWITCH_SHORT(shortbuf);
		}

		/* read the number of channels */
		readPackedFile(pf, &shortbuf, 2);

		if(G.order==B_ENDIAN) {
			SWITCH_SHORT(shortbuf);
		}

		/* check the number of channels */
		if(shortbuf != 1 && shortbuf != 2) {
			if (G.f & G_DEBUG) printf("Unsupported number of channels\n");
			return;
		}
		channels = shortbuf;
		
		/* read the samplerate */
		readPackedFile(pf, &longbuf, 4);


		if(G.order==B_ENDIAN)
			SWITCH_INT(longbuf);
		rate = longbuf;
		
		/* read the bitrate */
		// Ton's way
		readPackedFile(pf, &temp, 4);

		if(G.order==B_ENDIAN)
			SWITCH_INT(temp);

		if(channels && rate)
			bits= 8*temp/(rate * channels);
		
		// Frank's way

		readPackedFile(pf, &shortbuf, 2);
		readPackedFile(pf, &shortbuf, 2);
		if(G.order==B_ENDIAN) {
			SWITCH_SHORT(shortbuf);
		}
		bits = shortbuf;
		
		seekPackedFile(pf, i-16, SEEK_CUR);
		readPackedFile(pf, buffer, 4);
		/* check if we have a 'data' chunk */
		while(memcmp(buffer, "data", 4)!=0) {
			if (readPackedFile(pf, &i, 4) != 4)
				break;
			
			if(G.order==B_ENDIAN)
				SWITCH_INT(i);

			seekPackedFile(pf, i, SEEK_CUR);
			if (readPackedFile(pf, buffer, 4) != 4)
				break;
		}
		
		/* guess not */
		if (memcmp(buffer, "data", 4) !=0) {
			if (G.f & G_DEBUG) printf("No data found\n");
		}
		/* or maybe we do! */
		else {
			readPackedFile(pf, &longbuf, 4); 
			if(G.order==B_ENDIAN) SWITCH_INT(longbuf);
			
			/* handle 8 and 16 bit samples differently */
			/* intrr: removed, longbuf is length in bytes, not samples */
			if (bits == 16)
				data = (char *)MEM_mallocN(longbuf, "sample data");
			else 
				data = (char *)MEM_mallocN(longbuf*2, "sample data");

			len = longbuf /*/ 4.0*/; /* for some strange reason the sample length is off by a factor of 4... */
			/* intrr's comment: Funny eh, how one 16-bit stereo sample is 4 bytes? :-) */
			
			if(data) {
				readPackedFile(pf, data, len);
				/* data is only used to draw! */
				if (bits == 8) {
					temps = (short *) data;
					tempc = (char *) data;
					for (i = len - 1; i >= 0; i--)
						temps[i] = tempc[i] << 8;
				}
				else {
					if(G.order==B_ENDIAN) {
						swab(data, data, len);
					}
				}
				/* fill the sound with the found data */
				sample = sound->sample;
				sample->channels = channels;
				sample->rate = rate;
				sample->bits = bits;
				sample->len = len;
				sample->data = data;
				sample->type = SAMPLE_WAV;
			}
		}
	}
	else {
		sound->sample->type = SAMPLE_INVALID;
		if (G.f & G_DEBUG) printf("Unsupported sound format: %s\n", sound->name);
	}
}



/* ugly, but it works (for now) */
static int sound_get_filetype_from_header(bSound *sound, PackedFile *pf)
{
	int filetype = SAMPLE_INVALID;
	int i;
	char buffer[25];
	short shortbuf;
	
	rewindPackedFile(pf);
	
	if (readPackedFile(pf, buffer, 16) != 16) {
		if (G.f & G_DEBUG) printf("File too short\n");
		return filetype;
	}
	
	if(!(memcmp(buffer, "RIFF", 4) && memcmp(&(buffer[8]), "WAVEfmt ", 8))) {
		readPackedFile(pf, &i, 4);
		if(G.order==B_ENDIAN)
			SWITCH_INT(i);
		
		/* read the sampleformat */
		readPackedFile(pf, &shortbuf, 2);
		if(G.order==B_ENDIAN) {
			char s_i, *p_i;
			p_i= (char *)&(shortbuf);
			s_i= p_i[0];
			p_i[0]= p_i[1];
			p_i[1]= s_i;
		}
		
		if (shortbuf == SND_WAVE_FORMAT_PCM) {
			filetype = SAMPLE_WAV;
		}
		else
			/* only fmod supports compressed wav */
#ifdef USE_FMOD
		{
			/* and only valid publishers may use compressed wav */
			switch (shortbuf)
			{
			case SND_WAVE_FORMAT_ADPCM:
			case SND_WAVE_FORMAT_ALAW:
			case SND_WAVE_FORMAT_MULAW:
			case SND_WAVE_FORMAT_DIALOGIC_OKI_ADPCM:
			case SND_WAVE_FORMAT_CONTROL_RES_VQLPC:
			case SND_WAVE_FORMAT_GSM_610:
			case SND_WAVE_FORMAT_MPEG3:
				filetype = SAMPLE_WAV;
				break;
			default:
#endif
				{
					filetype = SAMPLE_INVALID;
					if (G.f & G_DEBUG) printf("Unsupported wav compression\n");
				}
			}
#ifdef USE_FMOD
		}
	}
	else if (!memcmp(buffer, "OggS", 4)) {
		filetype = SAMPLE_OGG_VORBIS;
	}
	else if ((!memcmp(buffer, "ID3", 3)) || (!memcmp(buffer, "", 2))) {
		filetype = SAMPLE_MP3;
	}
#endif
	else {
		filetype = SAMPLE_INVALID;
		if (G.f & G_DEBUG) printf("Unsupported sound format: %s\n", sound->name);
	}
	
	return filetype;
}



static int check_filetype(bSound *sound, PackedFile *pf)
{
//	char* pdest;
	sound->sample->type = SAMPLE_INVALID;
/*	
	// parse the name for the extension to see what kind of sample it is
	pdest = strrchr(sound->sample->name, '.');

	// a simple check to see what kind of sample we're dealing with
	if (stricmp(pdest, ".wav") == 0)
		sound->sample->type = SAMPLE_WAV;

#ifdef USE_FMOD
	if (stricmp(pdest, ".mp2") == 0)
		sound->sample->type = SAMPLE_MP2;
	if (stricmp(pdest, ".mp3") == 0)
		sound->sample->type = SAMPLE_MP3;
	if (stricmp(pdest, ".ogg") == 0)
		sound->sample->type = SAMPLE_OGG_VORBIS;
	if (stricmp(pdest, ".raw") == 0)
		sound->sample->type = SAMPLE_RAW;
	if (stricmp(pdest, ".wma") == 0)
		sound->sample->type = SAMPLE_WMA;
	if (stricmp(pdest, ".asf") == 0)
		sound->sample->type = SAMPLE_ASF;
#endif
*/
	sound->sample->type = sound_get_filetype_from_header(sound, pf);

	/* get some info from the sample */
	switch (sound->sample->type) {
	case SAMPLE_WAV:
		{
			sound_read_wav_data(sound, pf);
			break;
		}
	case SAMPLE_OGG_VORBIS:
	case SAMPLE_MP3:
	case SAMPLE_MP2:
	case SAMPLE_RAW:
	case SAMPLE_WMA:
	case SAMPLE_ASF:
		break;
	default:
		{
			if (G.f & G_DEBUG) printf("No valid sample: %s\n", sound->name);
			break;
		}
	}
	
	return sound->sample->type;
}



int sound_load_sample(bSound *sound)
{
	int result = FALSE;
	PackedFile *pf;
	int freePF = FALSE;
	int buffer = -1;

	if(ghSoundScene==NULL) sound_init_audio();

	/* check the sample (valid?) */
	if (sound->sample->type == SAMPLE_UNKNOWN || sound->snd_sound == NULL) {
		/* find... */
		pf = sound_find_packedfile(sound);

		/* ...or create a (temp)packedfile */
		if (pf == NULL) {
			pf = newPackedFile(sound->name);
			
			/* if autopack is off, free the pf afterwards */
			if ((G.fileflags & G_AUTOPACK) == 0)
				freePF = TRUE;
		}
		
		/* if we have a valid pf... */
		if (pf) {
			/* check the content of the pf */
			check_filetype(sound, pf);

			/* check if the sampletype is supported */
			if (sound->sample->type != SAMPLE_INVALID && sound->sample->type != SAMPLE_UNKNOWN) {
				/* register the sample at the audiodevice */
				buffer = SND_AddSample(ghSoundScene, sound->sample->name, pf->data, pf->size);

				/* create a soundobject */
				sound->snd_sound = SND_CreateSound();
				SND_SetSampleName(sound->snd_sound, sound->sample->name);
					
				/* add the soundobject to the soundscene  */
				if (SND_CheckBuffer(ghSoundScene, sound->snd_sound))
					SND_AddSound(ghSoundScene, sound->snd_sound);
				else
					if (G.f & G_DEBUG) printf("error: sample didn't load properly\n");
				
				/* if it was places in buffer[0] or higher, it succeeded */
				if (buffer >= 0)
					result = TRUE;
			}
			/* if not, free the pf */
			else {
				freePF = TRUE;
			}
			
			/* if you want it freed, make it so */
			if (freePF) {
				freePackedFile(pf);
				pf = NULL;
			} 
			/* or else connect the pf to the sound and sample */
//			else {
				sound->newpackedfile = pf;
				sound->sample->packedfile = pf;
//			}
		}
		else  {
			if (G.f & G_DEBUG) printf("%s: File not found!\n", sound->name);
			sound->sample->type = SAMPLE_INVALID;
		}
	}
	/* if the sample ain't invalid, we're ready to go! */
	else if (sound->sample->type != SAMPLE_INVALID) {
		result = TRUE;
	}

	return result;
}



bSound *sound_new_sound(char *name)
{
	bSound *sound = NULL;
	int len, file;
	char str[FILE_MAXDIR+FILE_MAXFILE];

	if(ghSoundScene==NULL) sound_init_audio();

	if (!G.scene->audio.mixrate) G.scene->audio.mixrate = 44100;
	/* convert the name to absolute path */
	strcpy(str, name);
	BLI_convertstringcode(str, G.sce);

	/* check if the sample on disk can be opened */
	file = open(str, O_BINARY|O_RDONLY);
	
	if (file != -1) {
		close(file);


		/* do some name magic */
		len = strlen(name);
		while (len > 0 && name[len - 1] != '/' && name[len - 1] != '\\')
			len--;
		
		/* allocate some memory for the sound */
		sound = alloc_libblock(&G.main->sound, ID_SO, name + len);
		strcpy(sound->name, name);
		
		/* intialize and check the sample */
		sound_initialize_sample(sound);

		/* load the sample & check if this blender supports the sound format */
//		sound_load_sample(sound);

		if (sound->sample->type == SAMPLE_INVALID) {
			free_libblock(&G.main->sound, sound);
			sound = NULL;
		} 
		else
		{
			sound->volume = 1.0;
			sound->attenuation = 1.0;
			sound->distance = 1.0;
			sound->min_gain = 0.0;
			sound->max_gain = 1.0;
		}
	}
	
	return (sound);
}



int sound_set_sample(bSound *sound, bSample *sample)
{
	int result = TRUE;
	
	if(ghSoundScene==NULL) sound_init_audio();
	
	/* delete the soundobject */
	if (sound->snd_sound) {
		SND_RemoveSound(ghSoundScene, sound->snd_sound);
		sound->snd_sound = NULL;
	}

	/* connect the sample to the sound */
	sound->sample = sample;
	sound->newpackedfile = NULL;
	
	if (sound->sample) {

		/* and set the right pf */
		sound->newpackedfile = sample->packedfile;

		/* if the sampletype is unknown initialize it */
		if (sound->sample->type == SAMPLE_UNKNOWN) {
			sound_initialize_sample(sound);
			
			/* load the sample & check if this blender supports the sound format */
			if (!sound_load_sample(sound)) {
				result = FALSE;
			}
		}
	}

	return result;
}



bSample *sound_new_sample(bSound *sound)
{
	char samplename[FILE_MAX];
	bSample *sample = NULL;
	int len;
	char *name;
	
	if (sound != NULL) {
		name = sound->name;	
		len = strlen(name);
		/* do some name magic */
		while (len > 0 && name[len - 1] != '/' && name[len - 1] != '\\')
			len--;
		
		/* allocate the memory for the sample */
		sample = MEM_callocN(sizeof(bSample), "sample");
		BLI_strncpy(sample->id.name+2, name+len, 20);
		BLI_addtail(samples, sample);	/* samples is ugly global */
		
		sample->data = &sample->fakedata[0];
		sample->type = SAMPLE_UNKNOWN;
		
		/* some default settings. We get divide by zero if these values are not set	*/
		sample->channels = 1;
		sample->rate = 44100;
		sample->bits = 16;
		sample->alindex = SAMPLE_INVALID;

		/* convert sound->name to abolute filename */
		/* TODO: increase sound->name, sample->name and strip->name to FILE_MAX, to avoid
		   cutting off sample name here - elubie */
		BLI_strncpy(samplename, sound->name, FILE_MAX);		
		BLI_convertstringcode(samplename, G.sce);
		BLI_strncpy(sample->name, samplename, FILE_MAXDIR);

		/* connect the pf to the sample */
		if (sound->newpackedfile)
			sample->packedfile = sound->newpackedfile;
		else
			sample->packedfile = sound_find_packedfile(sound);
	}
	
	return(sample);
}



/* find a sample that might already be loaded */
bSample *sound_find_sample(bSound *sound)
{
	bSample *sample;
	char name[FILE_MAXDIR + FILE_MAXFILE];
	char samplename[FILE_MAXDIR + FILE_MAXFILE];
	
	// convert sound->name to abolute filename
	strcpy(name, sound->name);
	BLI_convertstringcode(name, G.sce);
	
	/* search through the list of loaded samples */
	sample = samples->first;
	while (sample) {
		strcpy(samplename, sample->name);
		BLI_convertstringcode(samplename, G.sce);
		
		if (strcmp(name, samplename) == 0)	{
			break;
		}
		sample = sample->id.next;
	}
	
	return (sample);
}



int sound_sample_is_null(bSound *sound)
{
	int result = FALSE;
	bSample *sample;
	
	if(ghSoundScene==NULL) sound_init_audio();
	
	/* find the right sample or else create one */
	if (sound->sample == NULL) {
		/* find... */
		sample = sound_find_sample(sound);

		/* or a new one? */
		if (sample == NULL)
			sample = sound_new_sample(sound);
		
		if (sound_set_sample(sound, sample))
			result = TRUE;
	}

	return result;
}



void sound_stop_all_sounds(void)
{
#if GAMEBLENDER == 1
	if(ghSoundScene) {
		SND_StopAllSounds(ghSoundScene);
		SND_Proceed(ghAudioDeviceInterface, ghSoundScene);
	}
#endif 
}



void sound_end_all_sounds(void)
{
#if GAMEBLENDER == 1
	if(ghSoundScene) {
		sound_stop_all_sounds();
		SND_RemoveAllSounds(ghSoundScene);
	}
#endif
}



void sound_play_sound(bSound *sound)
{
#if GAMEBLENDER == 1
	if(ghSoundScene==NULL) sound_init_audio();
	
	/* first check if we want sound or not */
	SND_IsPlaybackWanted(ghSoundScene);

	/* stop all previous sounds */
	SND_StopAllSounds(ghSoundScene);
	
	if (sound != NULL && sound->sample != NULL)
	{
		/* load the sample if needed */
		if (sound_load_sample(sound))
		{
			/* set all kinds of parameters */
			SND_SetListenerGain(ghSoundScene, G.listener->gain);
			SND_SetDopplerFactor(ghSoundScene, G.listener->dopplerfactor);
			SND_SetDopplerVelocity(ghSoundScene, G.listener->dopplervelocity);
			
			SND_SetGain((SND_ObjectHandle)sound->snd_sound, (sound->volume));
			SND_SetPitch((SND_ObjectHandle)sound->snd_sound, (exp((sound->pitch / 12.0) * log(2.0))));
			
			if (sound->flags & SOUND_FLAGS_LOOP)
			{
				SND_SetLoopMode((SND_ObjectHandle)sound->snd_sound, SND_LOOP_NORMAL);
#ifdef SOUND_UNDER_DEVELOPMENT
/*				SND_SetLoopPoints((SND_ObjectHandle)sound->snd_sound, sound->loopstart, sound->loopend);
*/
#endif
				if (sound->flags & SOUND_FLAGS_BIDIRECTIONAL_LOOP)
					SND_SetLoopMode((SND_ObjectHandle)sound->snd_sound, SND_LOOP_BIDIRECTIONAL);
				else
					SND_SetLoopMode((SND_ObjectHandle)sound->snd_sound, SND_LOOP_NORMAL);

			}
			else 
			{
				SND_SetLoopMode((SND_ObjectHandle)sound->snd_sound, SND_LOOP_OFF);
			}
			
			if (sound->flags & SOUND_FLAGS_3D)
			{
				SND_SetRollOffFactor((SND_ObjectHandle)sound->snd_sound, sound->attenuation);
				SND_SetReferenceDistance((SND_ObjectHandle)sound->snd_sound, sound->distance);
				SND_SetMinimumGain((SND_ObjectHandle)sound->snd_sound, sound->min_gain);
				SND_SetMaximumGain((SND_ObjectHandle)sound->snd_sound, sound->max_gain);
			}
			else
			{
				SND_SetRollOffFactor((SND_ObjectHandle)sound->snd_sound, 0);
				SND_SetReferenceDistance((SND_ObjectHandle)sound->snd_sound, 1);
				SND_SetMinimumGain((SND_ObjectHandle)sound->snd_sound, 1);
				SND_SetMaximumGain((SND_ObjectHandle)sound->snd_sound, 1);
			}
			
			if (G.f & G_DEBUG) printf("Set pitch to: %f\n", SND_GetPitch((SND_ObjectHandle)sound->snd_sound));
			if (G.f & G_DEBUG) printf("Set gain to: %f\n", SND_GetGain((SND_ObjectHandle)sound->snd_sound));
			if (G.f & G_DEBUG) printf("Set looping to: %d\n", SND_GetLoopMode((SND_ObjectHandle)sound->snd_sound));
			
			/* play the sound */
			SND_StartSound(ghSoundScene, (SND_ObjectHandle)sound->snd_sound);

			/* update the device */
			SND_Proceed(ghAudioDeviceInterface, ghSoundScene);
		}
	}
	else 
	{
		if (G.f & G_DEBUG)
		{
			printf("uninitialized sound !\n");		
			if (sound)
			{
				printf("sound: %p\n", sound);
				if (sound->sample)
				{
					printf("sample: %p\n", sound->sample);
					if (sound->snd_sound)
						printf("hSoundObject: %p\n", sound->snd_sound);
				}
			}
			else
			{
				printf("sound == NULL\n");
			}
		}
	}
#endif 
}



bSound *sound_find_sound(char *id_name)
{
	bSound *sound;
	
	// look for sound with same *id* name
	sound = G.main->sound.first;
	while (sound)
	{
		if (strcmp(sound->id.name + 2, id_name) == 0)
			break;

		sound = sound->id.next;
	}
	
	return sound;
}

void sound_init_audio(void)
{
	int noaudio;
	SYS_SystemHandle hSystem = NULL;
	
	if(ghSoundScene==NULL) {
		hSystem = SYS_GetSystem();
		noaudio = SYS_GetCommandLineInt(hSystem,"noaudio",0);
		
		if (noaudio)/*(noaudio) intrr: disable game engine audio (openal) */
			SND_SetDeviceType(snd_e_dummydevice);
	
		ghAudioDeviceInterface = SND_GetAudioDevice();
		ghSoundScene = SND_CreateScene(ghAudioDeviceInterface);
		// also called after read new file, but doesnt work when no audio initialized
		sound_initialize_sounds();
	}
}


int sound_get_mixrate(void)
{
	return MIXRATE;
}


void sound_exit_audio(void)
{
	if(ghSoundScene) {
		SND_DeleteScene(ghSoundScene);
		SND_ReleaseDevice();
		ghSoundScene = NULL;
	}
}
