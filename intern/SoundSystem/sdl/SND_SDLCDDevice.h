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
#ifndef SND_SDLCDDEVICE
#define SND_SDLCDDEVICE

typedef struct SDL_CD;

class SND_SDLCDDevice
{
public:
	SND_SDLCDDevice();
	~SND_SDLCDDevice();
	
	void NextFrame();

	void PlayCD(int track);
	void PauseCD(bool pause);
	void StopCD();
	void SetCDPlaymode(int playmode);
	void SetCDGain(MT_Scalar gain);

private:
	void init();
	/* CD Audio */
	SDL_CD*			m_cdrom;
	bool		m_cdplaying;
	int		m_cdtrack;
	unsigned char	m_cdplaymode;
	unsigned char	m_frame;
};

#endif
