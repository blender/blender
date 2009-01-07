/**
 * $Id$
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

#include "SND_WaveSlot.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SND_WaveSlot::~SND_WaveSlot()
{
#ifdef ONTKEVER
	printf("neeeeeee...\n");
#endif
}



void SND_WaveSlot::SetSampleName(STR_String samplename)
{
	m_samplename = samplename;
}



void SND_WaveSlot::SetLoaded(bool loaded)
{
	m_loaded = loaded;
}



void SND_WaveSlot::SetData(void* data)
{
	m_data = data;
}



void SND_WaveSlot::SetBuffer(unsigned int buffer)
{
	m_buffer = buffer;
}



void SND_WaveSlot::SetSampleFormat(unsigned int sampleformat)
{
	m_sampleformat = sampleformat;
}



void SND_WaveSlot::SetNumberOfChannels(unsigned int numberofchannels)
{
	m_numberofchannels = numberofchannels;
}



void SND_WaveSlot::SetSampleRate(unsigned int samplerate)
{
	m_samplerate = samplerate;
}



void SND_WaveSlot::SetBitRate(unsigned int bitrate)
{
	m_bitrate = bitrate;
}



void SND_WaveSlot::SetNumberOfSamples(unsigned int numberofsamples)
{
	m_numberofsamples = numberofsamples;
}



void SND_WaveSlot::SetFileSize(unsigned int filesize)
{
	m_filesize = filesize;
}



const STR_String& SND_WaveSlot::GetSampleName()
{
	return m_samplename;
}



bool SND_WaveSlot::IsLoaded() const
{
	return m_loaded;
}



void* SND_WaveSlot::GetData()
{
	return m_data;
}



unsigned int SND_WaveSlot::GetBuffer() const
{
	return m_buffer;
}



unsigned int SND_WaveSlot::GetSampleFormat() const
{
	return m_sampleformat;
}



unsigned int SND_WaveSlot::GetNumberOfChannels() const
{
	return m_numberofchannels;
}



unsigned int SND_WaveSlot::GetSampleRate() const
{
	return m_samplerate;
}



unsigned int SND_WaveSlot::GetBitRate() const
{
	return m_bitrate;
}



unsigned int SND_WaveSlot::GetNumberOfSamples() const
{
	return m_numberofsamples;
}



unsigned int SND_WaveSlot::GetFileSize() const
{
	return m_filesize;
}
