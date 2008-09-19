/*
 * SoundDefines.h
 *
 * this is where all kinds of defines are stored
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

#ifndef __SOUNDDEFINES_H
#define __SOUNDDEFINES_H

/* the types of devices */
enum
{
	snd_e_dummydevice = 0,
	snd_e_fmoddevice,
	snd_e_openaldevice
};

/* general stuff */
#define NUM_BUFFERS						128
#define NUM_SOURCES						16

/* openal related stuff */
#define AL_LOOPING						0x1007

/* fmod related stuff */
#ifdef WIN32
#define MIXRATE							22050
#else
#define MIXRATE							44100
#endif
#define NUM_FMOD_MIN_HW_CHANNELS		16
#define NUM_FMOD_MAX_HW_CHANNELS		16

/* activelist defines */
enum
{
	SND_REMOVE_ACTIVE_OBJECT = 0,
	SND_ADD_ACTIVE_OBJECT,
	SND_DO_NOTHING
};

/* playstate flags */
enum
{
	SND_UNKNOWN = -1,
	SND_INITIAL,
	SND_MUST_PLAY,
	SND_PLAYING,
	SND_MUST_STOP,
	SND_STOPPED,
	SND_MUST_PAUSE,
	SND_PAUSED,
	SND_MUST_RESUME,
	SND_MUST_STOP_WHEN_FINISHED,
	SND_MUST_BE_DELETED
};

/* loopmodes */
enum
{
	SND_LOOP_OFF = 0,
	SND_LOOP_NORMAL,
	SND_LOOP_BIDIRECTIONAL
};


/* cd playstate flags */
enum
{
	SND_CD_ALL = 0,
	SND_CD_TRACK,
	SND_CD_TRACKLOOP
};

/* sample types */
enum
{
	SND_WAVE_FORMAT_UNKNOWN = 0,
	SND_WAVE_FORMAT_PCM,
	SND_WAVE_FORMAT_ADPCM,
	SND_WAVE_FORMAT_ALAW = 6,
	SND_WAVE_FORMAT_MULAW,
	SND_WAVE_FORMAT_DIALOGIC_OKI_ADPCM = 17,
	SND_WAVE_FORMAT_CONTROL_RES_VQLPC = 34,
	SND_WAVE_FORMAT_GSM_610 = 49,
	SND_WAVE_FORMAT_MPEG3 = 85
};

#endif //__SOUNDDEFINES_H

