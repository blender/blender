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

/** \file audaspace/ffmpeg/AUD_FFMPEGReader.h
 *  \ingroup audffmpeg
 */


#ifndef __AUD_FFMPEGREADER_H__
#define __AUD_FFMPEGREADER_H__

#include "AUD_ConverterFunctions.h"
#include "AUD_IReader.h"
#include "AUD_Buffer.h"

#include <string>
#include <boost/shared_ptr.hpp>

struct AVCodecContext;
extern "C" {
#include <libavformat/avformat.h>
}

/**
 * This class reads a sound file via ffmpeg.
 * \warning Seeking may not be accurate! Moreover the position is updated after
 *          a buffer reading call. So calling getPosition right after seek
 *          normally results in a wrong value.
 */
class AUD_FFMPEGReader : public AUD_IReader
{
private:
	/**
	 * The current position in samples.
	 */
	int m_position;

	/**
	 * The specification of the audio data.
	 */
	AUD_DeviceSpecs m_specs;

	/**
	 * The buffer for package reading.
	 */
	AUD_Buffer m_pkgbuf;

	/**
	 * The count of samples still available from the last read package.
	 */
	int m_pkgbuf_left;

	/**
	 * The AVFormatContext structure for using ffmpeg.
	 */
	AVFormatContext* m_formatCtx;

	/**
	 * The AVCodecContext structure for using ffmpeg.
	 */
	AVCodecContext* m_codecCtx;

	/**
	 * The AVIOContext to read the data from.
	 */
	AVIOContext* m_aviocontext;

	/**
	 * The stream ID in the file.
	 */
	int m_stream;

	/**
	 * Converter function.
	 */
	AUD_convert_f m_convert;

	/**
	 * The memory file to read from.
	 */
	boost::shared_ptr<AUD_Buffer> m_membuffer;

	/**
	 * The buffer to read with.
	 */
	data_t* m_membuf;

	/**
	 * Reading position of the buffer.
	 */
	int64_t m_membufferpos;

	/**
	 * Whether the audio data has to be interleaved after reading.
	 */
	bool m_tointerleave;

	/**
	 * Decodes a packet into the given buffer.
	 * \param packet The AVPacket to decode.
	 * \param buffer The target buffer.
	 * \return The count of read bytes.
	 */
	int decode(AVPacket& packet, AUD_Buffer& buffer);

	/**
	 * Initializes the object.
	 */
	void init();

	// hide copy constructor and operator=
	AUD_FFMPEGReader(const AUD_FFMPEGReader&);
	AUD_FFMPEGReader& operator=(const AUD_FFMPEGReader&);

public:
	/**
	 * Creates a new reader.
	 * \param filename The path to the file to be read.
	 * \exception AUD_Exception Thrown if the file specified does not exist or
	 *            cannot be read with ffmpeg.
	 */
	AUD_FFMPEGReader(std::string filename);

	/**
	 * Creates a new reader.
	 * \param buffer The buffer to read from.
	 * \exception AUD_Exception Thrown if the buffer specified cannot be read
	 *                          with ffmpeg.
	 */
	AUD_FFMPEGReader(boost::shared_ptr<AUD_Buffer> buffer);

	/**
	 * Destroys the reader and closes the file.
	 */
	virtual ~AUD_FFMPEGReader();

	static int read_packet(void* opaque, uint8_t* buf, int buf_size);
	static int64_t seek_packet(void* opaque, int64_t offset, int whence);

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_FFMPEGREADER_H__
