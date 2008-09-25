/*
 * SND_Utils.cpp
 *
 * Util functions for soundthingies
 *
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

#include "SND_Utils.h"
#include "SoundDefines.h"
#include "SND_DependKludge.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>

#if defined(WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#define BUFFERSIZE 32


/*****************************************************************************
 * Begin of temporary Endian stuff.
 * I think there should be a central place to handle endian conversion but for
 * the time being it suffices. Note that the defines come from the Blender
 * source.
 *****************************************************************************/
typedef enum
{
	SND_endianBig = 0,
	SND_endianLittle
} SND_TEndian;

#if defined(__BIG_ENDIAN__) || defined(__sparc) || defined(__sparc__)
const SND_TEndian SND_fEndian = SND_endianBig;
#else
const SND_TEndian SND_fEndian = SND_endianLittle;
#endif

/* This one swaps the bytes in a short */
#define SWITCH_SHORT(a) { \
    char s_i, *p_i; \
    p_i= (char *)&(a); \
    s_i=p_i[0]; \
    p_i[0] = p_i[1]; \
    p_i[1] = s_i; }

/* This one rotates the bytes in an int */
#define SWITCH_INT(a) { \
    char s_i, *p_i; \
    p_i= (char *)&(a); \
    s_i=p_i[0]; p_i[0]=p_i[3]; p_i[3]=s_i; \
    s_i=p_i[1]; p_i[1]=p_i[2]; p_i[2]=s_i; }
/*****************************************************************************
 * End of temporary Endian stuff.
 *****************************************************************************/


/* loads a file */
void* SND_LoadSample(char *filename)
{
	int file, filelen, buffersize = BUFFERSIZE;
	void* data = NULL;

#if defined(WIN32)	
	file = open(filename, O_BINARY|O_RDONLY);
#else
	file = open(filename, 0|O_RDONLY);
#endif

	if (file == -1)
	{
		//printf("can't open file.\n");
		//printf("press q for quit.\n");
	}
	else
	{
		filelen = lseek(file, 0, SEEK_END);
		lseek(file, 0, SEEK_SET);
		
		if (filelen != 0)
		{
			data = malloc(buffersize);

			if (read(file, data, buffersize) != buffersize)
			{
				free(data);
				data = NULL;
			}
		}
		close(file);
		
	}
	return (data);
}



bool SND_IsSampleValid(const STR_String& name, void* memlocation)
{
	bool result = false;
	bool loadedsample = false;
	char buffer[BUFFERSIZE];
	
	if (!memlocation)
	{
		STR_String samplename = name;
		memlocation = SND_LoadSample(samplename.Ptr());
		
		if (memlocation)
			loadedsample = true;
	}
	
	if (memlocation)
	{
		memcpy(&buffer, memlocation, BUFFERSIZE);
		
		if(!(memcmp(buffer, "RIFF", 4) && memcmp(&(buffer[8]), "WAVEfmt ", 8)))
		{
			/* This was endian unsafe. See top of the file for the define. */
			short shortbuf = *((short *) &buffer[20]);
			if (SND_fEndian == SND_endianBig) SWITCH_SHORT(shortbuf);

			if (shortbuf == SND_WAVE_FORMAT_PCM)
				result = true;
			
			/* only fmod supports compressed wav */
#ifdef USE_FMOD
			switch (shortbuf)
			{
				case SND_WAVE_FORMAT_ADPCM:
				case SND_WAVE_FORMAT_ALAW:
				case SND_WAVE_FORMAT_MULAW:
				case SND_WAVE_FORMAT_DIALOGIC_OKI_ADPCM:
				case SND_WAVE_FORMAT_CONTROL_RES_VQLPC:
				case SND_WAVE_FORMAT_GSM_610:
				case SND_WAVE_FORMAT_MPEG3:
					result = true;
					break;
				default:
					{
						break;
					}
			}
#endif
		}
#ifdef USE_FMOD
		/* only valid publishers may use ogg vorbis */
		else if (!memcmp(buffer, "OggS", 4))
		{
			result = true;
		}
		/* only valid publishers may use mp3 */
		else if (((!memcmp(buffer, "ID3", 3)) || (!memcmp(buffer, "ÿû", 2))))
		{
			result = true;
		}
#endif
	}
	if (loadedsample)
	{
		free(memlocation);
		memlocation = NULL;
	}

	return result;
}



/* checks if the passed pointer is a valid sample */
bool CheckSample(void* sample)
{
	bool valid = false;
	char buffer[32];
    
	memcpy(buffer, sample, 16);

	if(!(memcmp(buffer, "RIFF", 4) && memcmp(&(buffer[8]), "WAVEfmt ", 8)))
	{
		valid = true;
	}
	
	return valid;
}



/* gets the type of the sample (0 == unknown, 1 == PCM etc */
unsigned int SND_GetSampleFormat(void* sample)
{
	short sampletype = 0;

	if (CheckSample(sample))
	{
		memcpy(&sampletype, ((char*)sample) + 20, 2);
	}
	/* This was endian unsafe. See top of the file for the define. */
	if (SND_fEndian == SND_endianBig) SWITCH_SHORT(sampletype);

	return (unsigned int)sampletype;
}



/* gets the number of channels in a sample */
unsigned int SND_GetNumberOfChannels(void* sample)
{
	short numberofchannels = 0;

	if (CheckSample(sample))
	{
		memcpy(&numberofchannels, ((char*)sample) + 22, 2);
	}
	/* This was endian unsafe. See top of the file for the define. */
	if (SND_fEndian == SND_endianBig) SWITCH_SHORT(numberofchannels);

	return (unsigned int)numberofchannels;
}



/* gets the samplerate of a sample */
unsigned int SND_GetSampleRate(void* sample)
{
	unsigned int samplerate = 0;
	
	if (CheckSample(sample))
	{
		memcpy(&samplerate, ((char*)sample) + 24, 4);
	}
	/* This was endian unsafe. See top of the file for the define. */
	if (SND_fEndian == SND_endianBig) SWITCH_INT(samplerate);

	return samplerate;
}



/* gets the bitrate of a sample */
unsigned int SND_GetBitRate(void* sample)
{
	short bitrate = 0;

	if (CheckSample(sample))
	{
		memcpy(&bitrate, ((char*)sample) + 34, 2);
	}
	/* This was endian unsafe. See top of the file for the define. */
	if (SND_fEndian == SND_endianBig) SWITCH_SHORT(bitrate);

	return (unsigned int)bitrate;
}



/* gets the length of the actual sample data (without the header) */
unsigned int SND_GetNumberOfSamples(void* sample)
{
	unsigned int chunklength, length = 0, offset = 16;
	char data[4];
	
	if (CheckSample(sample))
	{
		memcpy(&chunklength, ((char*)sample) + offset, 4);
		/* This was endian unsafe. See top of the file for the define. */
		if (SND_fEndian == SND_endianBig) SWITCH_INT(chunklength);

		offset = offset + chunklength + 4;
		memcpy(data, ((char*)sample) + offset, 4);

		/* This seems very unsafe, what if data is never found (f.i. corrupt file)... */
		// lets find "data"
		while (memcmp(data, "data", 4))
		{
			offset += 4;
			memcpy(data, ((char*)sample) + offset, 4);
		}
		offset += 4;
		memcpy(&length, ((char*)sample) + offset, 4);

		/* This was endian unsafe. See top of the file for the define. */
		if (SND_fEndian == SND_endianBig) SWITCH_INT(length);
	}

	return length;
}



/* gets the size of the entire header (file - sampledata) */
unsigned int SND_GetHeaderSize(void* sample)
{
	unsigned int chunklength, headersize = 0, offset = 16;
	char data[4];
	
	if (CheckSample(sample))
	{
		memcpy(&chunklength, ((char*)sample) + offset, 4);
		/* This was endian unsafe. See top of the file for the define. */
		if (SND_fEndian == SND_endianBig) SWITCH_INT(chunklength);
		offset = offset + chunklength + 4;
		memcpy(data, ((char*)sample) + offset, 4);

		// lets find "data"
		while (memcmp(data, "data", 4))
		{
			offset += 4;
			memcpy(data, ((char*)sample) + offset, 4);
		}
		headersize = offset + 8;
	}


	return headersize;
}



unsigned int SND_GetExtraChunk(void* sample)
{
	unsigned int extrachunk = 0, chunklength, offset = 16;
	char data[4];

	if (CheckSample(sample))
	{
		memcpy(&chunklength, ((char*)sample) + offset, 4);
		offset = offset + chunklength + 4;
		memcpy(data, ((char*)sample) + offset, 4);

		// lets find "cue"
		while (memcmp(data, "cue", 3))
		{
			offset += 4;
			memcpy(data, ((char*)sample) + offset, 4);
		}
	}

	return extrachunk;
}



void SND_GetSampleInfo(signed char* sample, SND_WaveSlot* waveslot)
{	
	WavFileHeader	fileheader;
	WavFmtHeader	fmtheader;
	WavFmtExHeader	fmtexheader;
	WavSampleHeader	sampleheader;
	WavChunkHeader	chunkheader;
	
	if (CheckSample(sample))
	{
		memcpy(&fileheader, sample, sizeof(WavFileHeader));
		fileheader.size = SND_GetHeaderSize(sample);
		sample += sizeof(WavFileHeader);
		fileheader.size = ((fileheader.size+1) & ~1) - 4;

		while ((fileheader.size > 0) && (memcpy(&chunkheader, sample, sizeof(WavChunkHeader))))
		{
			sample += sizeof(WavChunkHeader);
			if (!memcmp(chunkheader.id, "fmt ", 4))
			{
				memcpy(&fmtheader, sample, sizeof(WavFmtHeader));
				waveslot->SetSampleFormat(fmtheader.format);

				if (fmtheader.format == 0x0001)
				{
					waveslot->SetNumberOfChannels(fmtheader.numberofchannels);
					waveslot->SetBitRate(fmtheader.bitrate);
					waveslot->SetSampleRate(fmtheader.samplerate);
					sample += chunkheader.size;
				} 
				else
				{
					memcpy(&fmtexheader, sample, sizeof(WavFmtExHeader));
					sample += chunkheader.size;
				}
			}
			else if (!memcmp(chunkheader.id, "data", 4))
			{
				if (fmtheader.format == 0x0001)
				{
					waveslot->SetNumberOfSamples(chunkheader.size);
					sample += chunkheader.size;
				}
				else if (fmtheader.format == 0x0011)
				{
					//IMA ADPCM
				}
				else if (fmtheader.format == 0x0055)
				{
					//MP3 WAVE
				}
			}
			else if (!memcmp(chunkheader.id, "smpl", 4))
			{
				memcpy(&sampleheader, sample, sizeof(WavSampleHeader));
				//loop = sampleheader.loops;
				sample += chunkheader.size;
			}
			else
				sample += chunkheader.size;

			sample += chunkheader.size & 1;
			fileheader.size -= (((chunkheader.size + 1) & ~1) + 8);
		}
	}
}
