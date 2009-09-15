/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_FFMPEGREADER
#define AUD_FFMPEGREADER

#include "AUD_IReader.h"
#include "AUD_Reference.h"
class AUD_Buffer;

struct AVCodecContext;
extern "C" {
#include <libavformat/avformat.h>
}

/**
 * This class reads a sound file via ffmpeg.
 * \warning Seeking may not be accurate! Moreover the position is updated after
 *          a buffer reading call. So calling getPosition right after seek
 *          normally results in a wrong value.
 * \warning Playback of an ogg with some outdated ffmpeg versions results in a
 *          segfault on windows.
 */
class AUD_FFMPEGReader : public AUD_IReader
{
private:
	/**
	 * The current position in samples.
	 */
	int m_position;

	/**
	 * The playback buffer.
	 */
	AUD_Buffer *m_buffer;

	/**
	 * The specification of the audio data.
	 */
	AUD_Specs m_specs;

	/**
	 * The buffer for package reading.
	 */
	AUD_Buffer *m_pkgbuf;

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
	 * The ByteIOContext to read the data from.
	 */
	ByteIOContext* m_byteiocontext;

	/**
	 * The stream ID in the file.
	 */
	int m_stream;

	/**
	 * The memory file to read from, only saved to keep the buffer alive.
	 */
	AUD_Reference<AUD_Buffer> m_membuffer;

	/**
	 * Decodes a packet into the given buffer.
	 * \param packet The AVPacket to decode.
	 * \param buffer The target buffer.
	 * \return The count of read bytes.
	 */
	int decode(AVPacket* packet, AUD_Buffer* buffer);

public:
	/**
	 * Creates a new reader.
	 * \param filename The path to the file to be read.
	 * \exception AUD_Exception Thrown if the file specified does not exist or
	 *            cannot be read with ffmpeg.
	 */
	AUD_FFMPEGReader(const char* filename);

	/**
	 * Creates a new reader.
	 * \param buffer The buffer to read from.
	 * \exception AUD_Exception Thrown if the buffer specified cannot be read
	 *                          with ffmpeg.
	 */
	AUD_FFMPEGReader(AUD_Reference<AUD_Buffer> buffer);

	/**
	 * Destroys the reader and closes the file.
	 */
	virtual ~AUD_FFMPEGReader();

	virtual bool isSeekable();
	virtual void seek(int position);
	virtual int getLength();
	virtual int getPosition();
	virtual AUD_Specs getSpecs();
	virtual AUD_ReaderType getType();
	virtual bool notify(AUD_Message &message);
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_FFMPEGREADER
