/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/ffmpeg/AUD_FFMPEGFactory.h
 *  \ingroup audffmpeg
 */


#ifndef __AUD_FFMPEGFACTORY_H__
#define __AUD_FFMPEGFACTORY_H__

#include "AUD_IFactory.h"
#include "AUD_Reference.h"
#include "AUD_Buffer.h"

#include <string>

/**
 * This factory reads a sound file via ffmpeg.
 * \warning Notice that the needed formats and codecs have to be registered
 * for ffmpeg before this class can be used.
 */
class AUD_FFMPEGFactory : public AUD_IFactory
{
private:
	/**
	 * The filename of the sound source file.
	 */
	const std::string m_filename;

	/**
	 * The buffer to read from.
	 */
	AUD_Reference<AUD_Buffer> m_buffer;

	// hide copy constructor and operator=
	AUD_FFMPEGFactory(const AUD_FFMPEGFactory&);
	AUD_FFMPEGFactory& operator=(const AUD_FFMPEGFactory&);

public:
	/**
	 * Creates a new factory.
	 * \param filename The sound file path.
	 */
	AUD_FFMPEGFactory(std::string filename);

	/**
	 * Creates a new factory.
	 * \param buffer The buffer to read from.
	 * \param size The size of the buffer.
	 */
	AUD_FFMPEGFactory(const data_t* buffer, int size);

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //__AUD_FFMPEGFACTORY_H__
