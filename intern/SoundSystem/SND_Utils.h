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
#ifndef	SND_UTILS_H
#define SND_UTILS_H

#include "SND_WaveSlot.h"

#ifdef __cplusplus
extern "C"
{ 
#endif

typedef struct
{
	unsigned char	riff[4];
	signed int		size;
	unsigned char	type[4];
} WavFileHeader;

typedef struct
{
	unsigned short	format;
	unsigned short	numberofchannels;
	unsigned int	samplerate;
	unsigned int	bytespersec;
	unsigned short	blockalignment;
	unsigned short	bitrate;
} WavFmtHeader;

typedef struct
{
	unsigned short	size;
	unsigned short	samplesperblock;
} WavFmtExHeader;

typedef struct
{
	unsigned int		Manufacturer;
	unsigned int		Product;
	unsigned int		SamplePeriod;
	unsigned int		Note;
	unsigned int		FineTune;
	unsigned int		SMPTEFormat;
	unsigned int		SMPTEOffest;
	unsigned int		loops;
	unsigned int		SamplerData;
	struct
	{
		unsigned int	Identifier;
		unsigned int	Type;
		unsigned int	Start;
		unsigned int	End;
		unsigned int	Fraction;
		unsigned int	Count;
	} Loop[1];
} WavSampleHeader;

typedef struct
{
	unsigned char	id[4];
	unsigned int	size;
} WavChunkHeader;

/**  
 *	loads a sample and returns a pointer
 */
extern void* SND_LoadSample(char *filename);

extern bool SND_IsSampleValid(const STR_String& name, void* memlocation);
extern unsigned int SND_GetSampleFormat(void* sample);
extern unsigned int SND_GetNumberOfChannels(void* sample);
extern unsigned int SND_GetSampleRate(void* sample);
extern unsigned int SND_GetBitRate(void* sample);
extern unsigned int SND_GetNumberOfSamples(void* sample);
extern unsigned int SND_GetHeaderSize(void* sample);
extern unsigned int SND_GetExtraChunk(void* sample);

extern void SND_GetSampleInfo(signed char* sample, SND_WaveSlot* waveslot);

#ifdef __cplusplus
}
#endif

#endif

