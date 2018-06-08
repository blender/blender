/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

#ifdef FFMPEG_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file FFMPEGReader.h
 * @ingroup plugin
 * The FFMPEGReader class.
 */

#include "respec/ConverterFunctions.h"
#include "IReader.h"
#include "util/Buffer.h"

#include <string>
#include <memory>

struct AVCodecContext;
extern "C" {
#include <libavformat/avformat.h>
}

AUD_NAMESPACE_BEGIN

/**
 * This class reads a sound file via ffmpeg.
 * \warning Seeking may not be accurate! Moreover the position is updated after
 *          a buffer reading call. So calling getPosition right after seek
 *          normally results in a wrong value.
 */
class AUD_PLUGIN_API FFMPEGReader : public IReader
{
private:
	/**
	 * The current position in samples.
	 */
	int m_position;

	/**
	 * The specification of the audio data.
	 */
	DeviceSpecs m_specs;

	/**
	 * The buffer for package reading.
	 */
	Buffer m_pkgbuf;

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
	 * The AVFrame structure for using ffmpeg.
	 */
	AVFrame* m_frame;

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
	convert_f m_convert;

	/**
	 * The memory file to read from.
	 */
	std::shared_ptr<Buffer> m_membuffer;

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
	AUD_LOCAL int decode(AVPacket& packet, Buffer& buffer);

	/**
	 * Initializes the object.
	 */
	AUD_LOCAL void init();

	// delete copy constructor and operator=
	FFMPEGReader(const FFMPEGReader&) = delete;
	FFMPEGReader& operator=(const FFMPEGReader&) = delete;

public:
	/**
	 * Creates a new reader.
	 * \param filename The path to the file to be read.
	 * \exception Exception Thrown if the file specified does not exist or
	 *            cannot be read with ffmpeg.
	 */
	FFMPEGReader(std::string filename);

	/**
	 * Creates a new reader.
	 * \param buffer The buffer to read from.
	 * \exception Exception Thrown if the buffer specified cannot be read
	 *                          with ffmpeg.
	 */
	FFMPEGReader(std::shared_ptr<Buffer> buffer);

	/**
	 * Destroys the reader and closes the file.
	 */
	virtual ~FFMPEGReader();

	/**
	 * Reads data to a memory buffer.
	 * This function is used for avio only.
	 * @param opaque The FFMPEGReader.
	 * @param buf The buffer to read to.
	 * @param buf_size The size of the buffer.
	 * @return How many bytes have been read.
	 */
	static int read_packet(void* opaque, uint8_t* buf, int buf_size);

	/**
	 * Seeks within data.
	 * This function is used for avio only.
	 * @param opaque The FFMPEGReader.
	 * @param offset The byte offset to seek to.
	 * @param whence The seeking action.
	 * @return The current position or the size of the data if requested.
	 */
	static int64_t seek_packet(void* opaque, int64_t offset, int whence);

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
