/*
 * SND_WaveCache.h
 *
 * abstract wavecache, a way to organize samples
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

#ifdef WIN32
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#ifndef __SND_WAVECACHE_H
#define __SND_WAVECACHE_H

#include "SND_WaveSlot.h"
#include "SoundDefines.h"
#include "SND_SoundObject.h"
#include <map>

class SND_WaveCache
{
public:
	SND_WaveCache();
	virtual ~SND_WaveCache();

	SND_WaveSlot*			GetWaveSlot(const STR_String& samplename);

	void					RemoveAllSamples();
	void					RemoveSample(const STR_String& samplename, int buffer);

private:
	std::map<STR_String, SND_WaveSlot*> m_samplecache;

	SND_WaveSlot*			m_bufferList[NUM_BUFFERS];

	void					FreeSamples();
};

#endif //__SND_WAVECACHE_H

