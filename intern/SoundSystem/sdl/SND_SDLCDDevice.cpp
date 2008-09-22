/*
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
 * SND_SDLCDDevice
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#include "MT_Scalar.h"

#include "SND_SDLCDDevice.h"
#include "SoundDefines.h"

#include <SDL.h>

SND_SDLCDDevice::SND_SDLCDDevice() :
	m_cdrom(NULL),
	m_cdplaying(false),
	m_cdtrack(0),
	m_cdplaymode(SND_CD_TRACK),
	m_frame(0)
{
	init();
}

void SND_SDLCDDevice::init()
{
#ifdef DISABLE_SDL
	fprintf(stderr, "Blender compiled without SDL, no CDROM support\n");
	return;
#else
	if (SDL_InitSubSystem(SDL_INIT_CDROM))
	{
		fprintf(stderr, "Error initializing CDROM\n");
		return;
	}
	
	/* Check for CD drives */
	if(!SDL_CDNumDrives())
	{
		/* None found */
		fprintf(stderr, "No CDROM devices available\n");
		return;
	}

	/* Open the default drive */
	m_cdrom = SDL_CDOpen(0);

	/* Did if open? Check if cdrom is NULL */
	if(!m_cdrom)
	{
		fprintf(stderr, "Couldn't open drive: %s\n", SDL_GetError());
		return;
	}
#endif
}

SND_SDLCDDevice::~SND_SDLCDDevice()
{
#ifndef DISABLE_SDL
	StopCD();
	SDL_CDClose(m_cdrom);
#endif
}

void SND_SDLCDDevice::NextFrame()
{
#ifndef DISABLE_SDL
	m_frame++;
	m_frame &= 127;
	
	if (!m_frame && m_cdrom && m_cdplaying && SDL_CDStatus(m_cdrom) == CD_STOPPED)
	{
		switch (m_cdplaymode)
		{
			case SND_CD_ALL:
				if (m_cdtrack < m_cdrom->numtracks)
					PlayCD(m_cdtrack + 1);
				else
					m_cdplaying = false;
				break;
			default:
			case SND_CD_TRACK:
				m_cdplaying = false;
				break;
			case SND_CD_TRACKLOOP:
				PlayCD(m_cdtrack);
				break;
		}
	
	}
#endif
}
	
void SND_SDLCDDevice::PlayCD(int track)
{
#ifndef DISABLE_SDL
	if ( m_cdrom && CD_INDRIVE(SDL_CDStatus(m_cdrom)) ) {
		SDL_CDPlayTracks(m_cdrom, track-1, 0, track, 0);
		m_cdplaying = true;
		m_cdtrack = track;
	}
#endif
}


void SND_SDLCDDevice::PauseCD(bool pause)
{
#ifndef DISABLE_SDL
	if (!m_cdrom)
		return;
		
	if (pause)
		SDL_CDPause(m_cdrom);
	else
		SDL_CDResume(m_cdrom);
#endif
}

void SND_SDLCDDevice::StopCD()
{
#ifndef DISABLE_SDL
	if (m_cdrom)
		SDL_CDStop(m_cdrom);
	m_cdplaying = false;
#endif
}

void SND_SDLCDDevice::SetCDPlaymode(int playmode)
{
	m_cdplaymode = playmode;
}

void SND_SDLCDDevice::SetCDGain(MT_Scalar gain)
{

}
