/*
 * SND_WaveSlot.cpp
 *
 * class for storing sample related information
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

#ifndef __SND_WAVESLOT_H
#define __SND_WAVESLOT_H

#include "STR_String.h"

class SND_WaveSlot
{
	STR_String		m_samplename;
	bool			m_loaded;
	void*			m_data;
	unsigned int	m_buffer;
	unsigned int	m_sampleformat;
	unsigned int	m_numberofchannels;
	unsigned int	m_samplerate;
	unsigned int	m_bitrate;
	unsigned int	m_numberofsamples;
	unsigned int	m_filesize;

public:

	SND_WaveSlot(): m_loaded(false),
					m_data(NULL),
					m_buffer(0),
					m_sampleformat(0),
					m_numberofchannels(0),
					m_samplerate(0),
					m_bitrate(0),
					m_numberofsamples(0),
					m_filesize(0)
					{};
	~SND_WaveSlot();

	void SetSampleName(STR_String samplename);
	void SetLoaded(bool loaded);
	void SetData(void* data);
	void SetBuffer(unsigned int buffer);
	void SetSampleFormat(unsigned int sampleformat);
	void SetNumberOfChannels(unsigned int numberofchannels);
	void SetSampleRate(unsigned int samplerate);
	void SetBitRate(unsigned int bitrate);
	void SetNumberOfSamples(unsigned int numberofsamples);
	void SetFileSize(unsigned int filesize);
	

	const STR_String&	GetSampleName();
	bool				IsLoaded() const;
	void*				GetData();
	unsigned int		GetBuffer() const;
	unsigned int		GetSampleFormat() const;
	unsigned int		GetNumberOfChannels() const;
	unsigned int		GetSampleRate() const;
	unsigned int		GetBitRate() const;
	unsigned int		GetNumberOfSamples() const;
	unsigned int		GetFileSize() const;

};

#endif //__SND_WAVESLOT_H

