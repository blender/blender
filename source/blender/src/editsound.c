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

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_packedFile_types.h"

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

#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"

#include "blendef.h"

#include "interface.h"
#include "mydevice.h"

#include "SND_C-api.h"
#include "SND_DependKludge.h"

#include "SYS_System.h"

/* this might move to the external header */
void* sound_get_libraryinterface(void);

static SND_SceneHandle ghSoundScene;
static SND_AudioDeviceInterfaceHandle ghAudioDeviceInterface;

/* que? why only here? because of the type define? */
bSound *sound_find_sound(char *id_name);
void sound_read_wav_data(bSound * sound, PackedFile * pf);
void sound_stop_sound(void *object, bSound *sound);
void winqreadsoundspace(unsigned short event, short val, char ascii);
/*  void sound_stop_all_sounds(void); already in BIF_editsound.h */



/* Right. Now for some implementation: */
void winqreadsoundspace(unsigned short event, short val, char ascii)
{
	float dx, dy;
	int doredraw= 0, cfra, first = 0;
	short mval[2];
	
	if(curarea->win==0) return;

	if(val) {
		
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event)
		{
		case LEFTMOUSE:
			if( view2dmove(event)==0 )
			{
				do
				{
					getmouseco_areawin(mval);
					areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
					
					cfra = (int)dx;
					if(cfra< 1) cfra= 1;
					
					if( cfra!=CFRA || first )
					{
						first= 0;
						CFRA= cfra;
						update_for_newframe();
						force_draw_plus(SPACE_VIEW3D);
					}
				
				} while(get_mbut()&L_MOUSE);
				
			}
			break;
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
		case RIGHTMOUSE:
			/* mouse_select_seq(); */
			break;
		case PADPLUSKEY:
			dx= (float)(0.1154*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin+= dx;
			G.v2d->cur.xmax-= dx;
			test_view2d(G.v2d, curarea->winx, curarea->winy);

			doredraw= 1;
			break;
		case PADMINUS:
			dx= (float)(0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin));
			G.v2d->cur.xmin-= dx;
			G.v2d->cur.xmax+= dx;
			test_view2d(G.v2d, curarea->winx, curarea->winy);

			doredraw= 1;
			break;
		case HOMEKEY:
			do_sound_buttons(B_SOUNDHOME);
			break;
		}
	}

	if(doredraw)
		scrarea_queue_winredraw(curarea);
}



void sound_initialize_sounds(void)
{
#if GAMEBLENDER == 1
	bSound* sound;

	/* clear the soundscene */
	SND_RemoveAllSounds(ghSoundScene);
	SND_RemoveAllSamples(ghSoundScene);

	/* initialize sounds */
	sound = G.main->sound.first;
	while (sound)
	{
		sound_sample_is_null(sound);
		sound = (bSound *) sound->id.next;
	}
#endif
}



bSound* sound_make_copy(bSound* originalsound)
{
	bSound* sound = NULL;
#if GAMEBLENDER == 1
	char name[160];
	int len;
	
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
	
#endif
	return sound;
}



void sound_initialize_sample(bSound* sound)
{
	if (sound && sound->sample == NULL)
		sound_sample_is_null(sound);
}



void sound_read_wav_data(bSound* sound, PackedFile* pf)
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
	if (readPackedFile(pf, buffer, 16) != 16)
	{
		if (G.f & G_DEBUG) printf("File too short\n");
		return;
	}
	
	if(!(memcmp(buffer, "RIFF", 4) && memcmp(&(buffer[8]), "WAVEfmt ", 8)))
	{
		readPackedFile(pf, &i, 4);// start of data
		if(G.order==B_ENDIAN)
			SWITCH_INT(i);
		
		/* read the sampleformat */
		readPackedFile(pf, &shortbuf, 2);
		if(G.order==B_ENDIAN)
		{
			/* was SWITCH_SHORT before */
			char s_i, *p_i;
			p_i= (char *)&(shortbuf);
			s_i= p_i[0];
			p_i[0]= p_i[1];
			p_i[1]= s_i;
		}
		
		/* read the number of channels */
		readPackedFile(pf, &shortbuf, 2);
		if(G.order==B_ENDIAN)
		{
			/* was SWITCH_SHORT before */
			char s_i, *p_i;
			p_i= (char *)&(shortbuf);
			s_i= p_i[0];
			p_i[0]= p_i[1];
			p_i[1]= s_i;
		}
		
		/* check the number of channels */
		if(shortbuf != 1 && shortbuf != 2)
		{
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
		if(G.order==B_ENDIAN)
		{
			/* was SWITCH_SHORT before */
			char s_i, *p_i;
			p_i= (char *)&(shortbuf);
			s_i= p_i[0];
			p_i[0]= p_i[1];
			p_i[1]= s_i;
		}
		bits = shortbuf;
		
		seekPackedFile(pf, i-16, SEEK_CUR);
		readPackedFile(pf, buffer, 4);
		
		/* check if we have a 'data' chunk */
		while(memcmp(buffer, "data", 4)!=0)
		{
			if (readPackedFile(pf, &i, 4) != 4)
				break;
			
			if(G.order==B_ENDIAN)
				SWITCH_INT(i);

			seekPackedFile(pf, i, SEEK_CUR);
			
			if (readPackedFile(pf, buffer, 4) != 4)
				break;
		}
		
		/* guess not */
		if (memcmp(buffer, "data", 4) !=0)
		{
			if (G.f & G_DEBUG) printf("No data found\n");
		}
		/* or maybe we do! */
		else
		{
			readPackedFile(pf, &longbuf, 4); 
			if(G.order==B_ENDIAN) SWITCH_INT(longbuf);
			
			/* handle 8 and 16 bit samples differently */
			if (bits == 8)
				data = (char *)MEM_mallocN(2 * longbuf, "sample data");
			else if (bits == 16)
				data = (char *)MEM_mallocN(longbuf, "sample data");

			len = longbuf;
			
			if(data)
			{
				readPackedFile(pf, data, len);
				
				/* data is only used to draw! */
				if (bits == 8)
				{
					temps = (short *) data;
					tempc = (char *) data;
					for (i = len - 1; i >= 0; i--)
						temps[i] = tempc[i] << 8;
				}
				else
				{
					if(G.order==B_ENDIAN)
					{
						temps= (short *)data;
						for(i=0; i< len / 2; i++, temps++)
						{
							/* was SWITCH_SHORT before */
							char s_i, *p_i;
							p_i= (char *)&(temps);
							s_i= p_i[0];
							p_i[0]= p_i[1];
							p_i[1]= s_i;
						}
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
	else
	{
		sound->sample->type = SAMPLE_INVALID;
		if (G.f & G_DEBUG) printf("Unsupported sound format: %s\n", sound->name);
	}
}



/* ugly, but it works (for now) */
int sound_get_filetype_from_header(bSound* sound, PackedFile* pf)
{
	int i, filetype = SAMPLE_INVALID;
#if GAMEBLENDER == 1
	char buffer[25];
	short shortbuf;
	
	rewindPackedFile(pf);
	
	if (readPackedFile(pf, buffer, 16) != 16)
	{
		if (G.f & G_DEBUG) printf("File too short\n");
		return filetype;
	}
	
	if(!(memcmp(buffer, "RIFF", 4) && memcmp(&(buffer[8]), "WAVEfmt ", 8)))
	{
		readPackedFile(pf, &i, 4);
		if(G.order==B_ENDIAN)
			SWITCH_INT(i);
		
		/* read the sampleformat */
		readPackedFile(pf, &shortbuf, 2);
		if(G.order==B_ENDIAN)
		{
			char s_i, *p_i;
			p_i= (char *)&(shortbuf);
			s_i= p_i[0];
			p_i[0]= p_i[1];
			p_i[1]= s_i;
		}
		
		if (shortbuf == SND_WAVE_FORMAT_PCM)
		{
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
	else if (!memcmp(buffer, "OggS", 4))
	{
		filetype = SAMPLE_OGG_VORBIS;
	}
	else if ((!memcmp(buffer, "ID3", 3)) || (!memcmp(buffer, "ÿû", 2)))
	{
		filetype = SAMPLE_MP3;
	}
#endif
	else
	{
		filetype = SAMPLE_INVALID;
		if (G.f & G_DEBUG) printf("Unsupported sound format: %s\n", sound->name);
	}
	
#endif
	return filetype;
}



int check_filetype(bSound* sound, PackedFile* pf)
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
	switch (sound->sample->type)
	{
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



int sound_load_sample(bSound* sound)
{
	int result = FALSE;
#if GAMEBLENDER == 1
	PackedFile* pf;
	int freePF = FALSE;
	int buffer = -1;

	/* check the sample (valid?) */
	if (sound->sample->type == SAMPLE_UNKNOWN || sound->snd_sound == NULL)
	{
		/* find... */
		pf = sound_find_packedfile(sound);

		/* ...or create a (temp)packedfile */
		if (pf == NULL)
		{
			pf = newPackedFile(sound->name);
			
			/* if autopack is off, free the pf afterwards */
			if ((G.fileflags & G_AUTOPACK) == 0)
				freePF = TRUE;
		}
		
		/* if we have a valid pf... */
		if (pf)
		{
			/* check the content of the pf */
			check_filetype(sound, pf);

			/* check if the sampletype is supported */
			if (sound->sample->type != SAMPLE_INVALID && sound->sample->type != SAMPLE_UNKNOWN)
			{
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
			else
			{
				freePF = TRUE;
			}
			
			/* if you want it freed, make it so */
			if (freePF)
			{
				freePackedFile(pf);
				pf = NULL;
			} 
			/* or else connect the pf to the sound and sample */
//			else
//			{
				sound->newpackedfile = pf;
				sound->sample->packedfile = pf;
//			}
		}
		else 
		{
			if (G.f & G_DEBUG) printf("%s: File not found!\n", sound->name);
			sound->sample->type = SAMPLE_INVALID;
		}
	}
	/* if the sample ain't invalid, we're ready to go! */
	else if (sound->sample->type != SAMPLE_INVALID)
	{
		result = TRUE;
	}

#endif

	return result;
}



bSound* sound_new_sound(char* name)
{
	bSound *sound = NULL;
#if GAMEBLENDER == 1
	int len, file;
	char str[FILE_MAXDIR+FILE_MAXFILE];
	
	/* convert the name to absolute path */
	strcpy(str, name);
	BLI_convertstringcode(str, G.sce, G.scene->r.cfra);

	/* check if the sample on disk can be opened */
	file = open(str, O_BINARY|O_RDONLY);
	
	if (file != -1)
	{
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

		if (sound->sample->type == SAMPLE_INVALID)
		{
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
	
#endif 
	return (sound);
}



int sound_set_sample(bSound *sound, bSample *sample)
{
	int result = TRUE;
#if GAMEBLENDER == 1
	/* decrease the usernumber for this sample */
	if (sound->sample)
		sound->sample->id.us--;

	/* delete the soundobject */
	if (sound->snd_sound)
	{
		SND_RemoveSound(ghSoundScene, sound->snd_sound);
		sound->snd_sound = NULL;
	}

	/* connect the sample to the sound */
	sound->sample = sample;
	sound->newpackedfile = NULL;
	
	/* increase the usercount */
	if (sound->sample)
	{
		sound->sample->id.us++;

		/* and set the right pf */
		sound->newpackedfile = sample->packedfile;

		/* if the sampletype is unknown initialize it */
		if (sound->sample->type == SAMPLE_UNKNOWN)
		{
			sound_initialize_sample(sound);
			
			/* load the sample & check if this blender supports the sound format */
			if (!sound_load_sample(sound))
			{
				result = FALSE;
			}
		}
	}

#endif 

	return result;
}



bSample *sound_new_sample(bSound * sound)
{
	bSample *sample = NULL;
	int len;
	char *name;
	
	if (sound != NULL)
	{
		name = sound->name;	
		len = strlen(name);
		/* do some name magic */
		while (len > 0 && name[len - 1] != '/' && name[len - 1] != '\\')
			len--;
		
		/* allocate the memory for the sample */
		sample = alloc_libblock(samples, ID_SAMPLE, name + len);
		sample->data = &sample->fakedata[0];
		sample->type = SAMPLE_UNKNOWN;
		
		/* some default settings. We get divide by zero if these values are not set	*/
		sample->channels = 1;
		sample->rate = 44100;
		sample->bits = 16;
		sample->alindex = SAMPLE_INVALID;

		/* convert sound->name to abolute filename */
		strcpy(sample->name, sound->name);
		BLI_convertstringcode(sample->name, G.sce, G.scene->r.cfra);
		
		/* connect the pf to the sample */
		if (sound->newpackedfile)
			sample->packedfile = sound->newpackedfile;
		else
			sample->packedfile = sound_find_packedfile(sound);
	}
	
	return(sample);
}



/* find a sample that might already be loaded */
bSample* sound_find_sample(bSound* sound)
{
	bSample* sample;
	char name[FILE_MAXDIR + FILE_MAXFILE];
	char samplename[FILE_MAXDIR + FILE_MAXFILE];
	
	// convert sound->name to abolute filename
	strcpy(name, sound->name);
	BLI_convertstringcode(name, G.sce, G.scene->r.cfra);
	
	/* search through the list of loaded samples */
	sample = samples->first;
	
	while (sample)
	{
		strcpy(samplename, sample->name);
		BLI_convertstringcode(samplename, G.sce, G.scene->r.cfra);
		
		if (strcmp(name, samplename) == 0)
		{
			break;
		}
		sample = sample->id.next;
	}
	
	return (sample);
}



int sound_sample_is_null(bSound* sound)
{
	int result = FALSE;
	bSample* sample;
	
	/* find the right sample or else create one */
	if (sound->sample == NULL)
	{
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
	SND_StopAllSounds(ghSoundScene);
	SND_Proceed(ghAudioDeviceInterface, ghSoundScene);
#endif 
}



void sound_end_all_sounds(void)
{
#if GAMEBLENDER == 1
	sound_stop_all_sounds();
	SND_RemoveAllSounds(ghSoundScene);
#endif
}



void sound_play_sound(bSound* sound)
{
#if GAMEBLENDER == 1
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



static void sound_init_listener(void)
{
	G.listener = MEM_callocN(sizeof(bSoundListener), "soundlistener");
	G.listener->gain = 1.0;
	G.listener->dopplerfactor = 1.0;
	G.listener->dopplervelocity = 1.0;
}



void sound_init_audio(void)
{
#if GAMEBLENDER == 1
	int noaudio;
	SYS_SystemHandle hSystem = NULL;
	ghAudioDeviceInterface = NULL;
	
	hSystem = SYS_GetSystem();
	noaudio = SYS_GetCommandLineInt(hSystem,"noaudio",0);
	
	if (noaudio)
		SND_SetDeviceType(snd_e_dummydevice);

	ghAudioDeviceInterface = SND_GetAudioDevice();
	ghSoundScene = SND_CreateScene(ghAudioDeviceInterface);

	sound_init_listener();
#endif 
}



int sound_get_mixrate(void)
{
	return MIXRATE;
}



static void sound_exit_listener(void)
{
	MEM_freeN(G.listener);
}



void sound_exit_audio(void)
{
#if GAMEBLENDER == 1
	SND_DeleteScene(ghSoundScene);
	SND_ReleaseDevice();
	sound_exit_listener();
#endif 
}
