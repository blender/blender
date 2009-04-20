/*
 * SND_WaveCache.cpp
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning (disable:4786) // Get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#include "SND_WaveCache.h"
#include <stdio.h>

#ifdef __APPLE__
# include <sys/malloc.h>
#else
# ifdef __FreeBSD__
#  include <stdlib.h>
# else
#  include <malloc.h>
# endif
#endif

SND_WaveCache::SND_WaveCache()
{
	// do the buffer administration
	for (int i = 0; i < NUM_BUFFERS; i++)
		m_bufferList[i] = NULL;
}



SND_WaveCache::~SND_WaveCache()
{
	// clean up the mess
	FreeSamples();
	RemoveAllSamples();
}



SND_WaveSlot* SND_WaveCache::GetWaveSlot(const STR_String& samplename)
{
	SND_WaveSlot* waveslot = NULL;

	std::map<STR_String, SND_WaveSlot*>::iterator find_result = m_samplecache.find(samplename);
		
	// let's see if we have already loaded this sample
	if (find_result != m_samplecache.end())
	{
		waveslot = (*find_result).second;
	}
	else
	{
		// so the sample wasn't loaded, so do it here
		for (int bufnum = 0; bufnum < NUM_BUFFERS; bufnum++)
		{
			// find an empty buffer
			if (m_bufferList[bufnum] == NULL)
			{
				waveslot = new SND_WaveSlot();
				waveslot->SetSampleName(samplename);
				waveslot->SetBuffer(bufnum);
				m_bufferList[bufnum] = waveslot;
				break;
			}
		}
		m_samplecache.insert(std::pair<STR_String, SND_WaveSlot*>(samplename, waveslot));
	}

	return waveslot;
}



void SND_WaveCache::RemoveAllSamples()
{
	// remove all samples
	m_samplecache.clear();

	// reset the list of buffers
	for (int i = 0; i < NUM_BUFFERS; i++)
		m_bufferList[i] = NULL;
}



void SND_WaveCache::RemoveSample(const STR_String& samplename, int buffer)
{
	m_samplecache.erase(samplename);
	m_bufferList[buffer] = NULL;
}



void SND_WaveCache::FreeSamples()
{
	// iterate through the bufferlist and delete the waveslot if present
	for (int i = 0; i < NUM_BUFFERS; i++)
	{
		if (m_bufferList[i])
		{
			delete m_bufferList[i];
			m_bufferList[i] = NULL;
		}
	}
}
