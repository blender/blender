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

// needed for INT64_C
#define __STDC_CONSTANT_MACROS

#include "AUD_FFMPEGReader.h"
#include "AUD_Buffer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int AUD_FFMPEGReader::decode(AVPacket* packet, AUD_Buffer* buffer)
{
	// save packet parameters
	uint8_t *audio_pkg_data = packet->data;
	int audio_pkg_size = packet->size;

	int buf_size = buffer->getSize();
	int buf_pos = 0;

	int read_length, data_size;

	// as long as there is still data in the package
	while(audio_pkg_size > 0)
	{
		// resize buffer if needed
		if(buf_size - buf_pos < AVCODEC_MAX_AUDIO_FRAME_SIZE)
		{
			buffer->resize(buf_size + AVCODEC_MAX_AUDIO_FRAME_SIZE, true);
			buf_size += AVCODEC_MAX_AUDIO_FRAME_SIZE;
		}

		// read samples from the packet
		data_size = buf_size - buf_pos;
		/*read_length = avcodec_decode_audio3(m_codecCtx,
			(int16_t*)(((data_t*)buffer->getBuffer())+buf_pos),
			&data_size,
			packet);*/
		read_length = avcodec_decode_audio2(m_codecCtx,
			(int16_t*)(((data_t*)buffer->getBuffer())+buf_pos),
			&data_size,
			audio_pkg_data,
			audio_pkg_size);

		// read error, next packet!
		if(read_length < 0)
			break;

		buf_pos += data_size;

		// move packet parameters
		audio_pkg_data += read_length;
		audio_pkg_size -= read_length;
	}

	return buf_pos;
}

void AUD_FFMPEGReader::init()
{
	m_position = 0;
	m_pkgbuf_left = 0;

	if(av_find_stream_info(m_formatCtx)<0)
		AUD_THROW(AUD_ERROR_FFMPEG);

	// find audio stream and codec
	m_stream = -1;

	for(unsigned int i = 0; i < m_formatCtx->nb_streams; i++)
		if((m_formatCtx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO)
			&& (m_stream < 0))
		{
			m_stream=i;
			break;
		}
	if(m_stream == -1)
		AUD_THROW(AUD_ERROR_FFMPEG);

	m_codecCtx = m_formatCtx->streams[m_stream]->codec;

	// get a decoder and open it
	AVCodec *aCodec = avcodec_find_decoder(m_codecCtx->codec_id);
	if(!aCodec)
		AUD_THROW(AUD_ERROR_FFMPEG);

	if(avcodec_open(m_codecCtx, aCodec)<0)
		AUD_THROW(AUD_ERROR_FFMPEG);

	// XXX this prints file information to stdout:
	//dump_format(m_formatCtx, 0, NULL, 0);

	m_specs.channels = (AUD_Channels) m_codecCtx->channels;

	switch(m_codecCtx->sample_fmt)
	{
	case SAMPLE_FMT_U8:
		m_convert = AUD_convert_u8_float;
		m_specs.format = AUD_FORMAT_U8;
		break;
	case SAMPLE_FMT_S16:
		m_convert = AUD_convert_s16_float;
		m_specs.format = AUD_FORMAT_S16;
		break;
	case SAMPLE_FMT_S32:
		m_convert = AUD_convert_s32_float;
		m_specs.format = AUD_FORMAT_S32;
		break;
	case SAMPLE_FMT_FLT:
		m_convert = AUD_convert_copy<float>;
		m_specs.format = AUD_FORMAT_FLOAT32;
		break;
	case SAMPLE_FMT_DBL:
		m_convert = AUD_convert_double_float;
		m_specs.format = AUD_FORMAT_FLOAT64;
		break;
	default:
		AUD_THROW(AUD_ERROR_FILE);
	}

	m_specs.rate = (AUD_SampleRate) m_codecCtx->sample_rate;

	// last but not least if there hasn't been any error, create the buffers
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
	m_pkgbuf = new AUD_Buffer(AVCODEC_MAX_AUDIO_FRAME_SIZE<<1);
	AUD_NEW("buffer")
}

AUD_FFMPEGReader::AUD_FFMPEGReader(const char* filename)
{
	m_byteiocontext = NULL;

	// open file
	if(av_open_input_file(&m_formatCtx, filename, NULL, 0, NULL)!=0)
		AUD_THROW(AUD_ERROR_FILE);

	try
	{
		init();
	}
	catch(AUD_Exception)
	{
		av_close_input_file(m_formatCtx);
		throw;
	}
}

AUD_FFMPEGReader::AUD_FFMPEGReader(AUD_Reference<AUD_Buffer> buffer)
{
	m_byteiocontext = (ByteIOContext*)av_mallocz(sizeof(ByteIOContext));
	AUD_NEW("byteiocontext")
	m_membuffer = buffer;

	if(init_put_byte(m_byteiocontext, (data_t*)buffer.get()->getBuffer(),
					 buffer.get()->getSize(), 0, NULL, NULL, NULL, NULL) != 0)
		AUD_THROW(AUD_ERROR_FILE);

	AVProbeData probe_data;
	probe_data.filename = "";
	probe_data.buf = (data_t*)buffer.get()->getBuffer();
	probe_data.buf_size = buffer.get()->getSize();
	AVInputFormat* fmt = av_probe_input_format(&probe_data, 1);

	// open stream
	if(av_open_input_stream(&m_formatCtx, m_byteiocontext, "", fmt, NULL)!=0)
		AUD_THROW(AUD_ERROR_FILE);

	try
	{
		init();
	}
	catch(AUD_Exception)
	{
		av_close_input_stream(m_formatCtx);
		av_free(m_byteiocontext); AUD_DELETE("byteiocontext")
		throw;
	}
}

AUD_FFMPEGReader::~AUD_FFMPEGReader()
{
	avcodec_close(m_codecCtx);

	if(m_byteiocontext)
	{
		av_close_input_stream(m_formatCtx);
		av_free(m_byteiocontext); AUD_DELETE("byteiocontext")
	}
	else
		av_close_input_file(m_formatCtx);

	delete m_buffer; AUD_DELETE("buffer")
	delete m_pkgbuf; AUD_DELETE("buffer")
}

bool AUD_FFMPEGReader::isSeekable()
{
	return true;
}

void AUD_FFMPEGReader::seek(int position)
{
	if(position >= 0)
	{
		// a value < 0 tells us that seeking failed
		if(av_seek_frame(m_formatCtx,
						 -1,
						 (uint64_t)(((uint64_t)position *
									 (uint64_t)AV_TIME_BASE) /
									(uint64_t)m_specs.rate),
						 AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY) >= 0)
		{
			avcodec_flush_buffers(m_codecCtx);
			m_position = position;

			AVPacket packet;
			bool search = true;

			while(search && av_read_frame(m_formatCtx, &packet) >= 0)
			{
				// is it a frame from the audio stream?
				if(packet.stream_index == m_stream)
				{
					// decode the package
					m_pkgbuf_left = decode(&packet, m_pkgbuf);
					search = false;

					// check position
					if(packet.pts != AV_NOPTS_VALUE)
					{
						// calculate real position, and read to frame!
						m_position = packet.pts *
							av_q2d(m_formatCtx->streams[m_stream]->time_base) *
							m_specs.rate;

						if(m_position < position)
						{
							sample_t* buf;
							int length = position - m_position;
							read(length, buf);
						}
					}
				}
				av_free_packet(&packet);
			}
		}
		else
		{
			// Seeking failed, do nothing.
		}
	}
}

int AUD_FFMPEGReader::getLength()
{
	// return approximated remaning size
	return (int)((m_formatCtx->duration * m_codecCtx->sample_rate)
				 / AV_TIME_BASE)-m_position;
}

int AUD_FFMPEGReader::getPosition()
{
	return m_position;
}

AUD_Specs AUD_FFMPEGReader::getSpecs()
{
	return m_specs.specs;
}

AUD_ReaderType AUD_FFMPEGReader::getType()
{
	return AUD_TYPE_STREAM;
}

bool AUD_FFMPEGReader::notify(AUD_Message &message)
{
	return false;
}

void AUD_FFMPEGReader::read(int & length, sample_t* & buffer)
{
	// read packages and decode them
	AVPacket packet;
	int data_size = 0;
	int pkgbuf_pos;
	int left = length;
	int sample_size = AUD_DEVICE_SAMPLE_SIZE(m_specs);

	// resize output buffer if necessary
	if(m_buffer->getSize() < length * AUD_SAMPLE_SIZE(m_specs))
		m_buffer->resize(length * AUD_SAMPLE_SIZE(m_specs));

	buffer = m_buffer->getBuffer();
	pkgbuf_pos = m_pkgbuf_left;
	m_pkgbuf_left = 0;

	// there may still be data in the buffer from the last call
	if(pkgbuf_pos > 0)
	{
		data_size = AUD_MIN(pkgbuf_pos, left * sample_size);
		m_convert((data_t*) buffer, (data_t*) m_pkgbuf->getBuffer(),
				  data_size / AUD_FORMAT_SIZE(m_specs.format));
		buffer += data_size / AUD_FORMAT_SIZE(m_specs.format);
		left -= data_size/sample_size;
	}

	// for each frame read as long as there isn't enough data already
	while((left > 0) && (av_read_frame(m_formatCtx, &packet) >= 0))
	{
		// is it a frame from the audio stream?
		if(packet.stream_index == m_stream)
		{
			// decode the package
			pkgbuf_pos = decode(&packet, m_pkgbuf);

			// copy to output buffer
			data_size = AUD_MIN(pkgbuf_pos, left * sample_size);
			m_convert((data_t*) buffer, (data_t*) m_pkgbuf->getBuffer(),
					  data_size / AUD_FORMAT_SIZE(m_specs.format));
			buffer += data_size / AUD_FORMAT_SIZE(m_specs.format);
			left -= data_size/sample_size;
		}
		av_free_packet(&packet);
	}
	// read more data than necessary?
	if(pkgbuf_pos > data_size)
	{
		m_pkgbuf_left = pkgbuf_pos-data_size;
		memmove(m_pkgbuf->getBuffer(),
				((data_t*)m_pkgbuf->getBuffer())+data_size,
				pkgbuf_pos-data_size);
	}

	buffer = m_buffer->getBuffer();

	if(left > 0)
		length -= left;
	m_position += length;
}
