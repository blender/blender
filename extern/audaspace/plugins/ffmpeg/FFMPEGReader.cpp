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

#include "FFMPEGReader.h"
#include "Exception.h"

#include <algorithm>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
}

AUD_NAMESPACE_BEGIN

#if LIBAVCODEC_VERSION_MAJOR < 58
#define FFMPEG_OLD_CODE
#endif

SampleFormat FFMPEGReader::convertSampleFormat(AVSampleFormat format)
{
	switch(av_get_packed_sample_fmt(format))
	{
	case AV_SAMPLE_FMT_U8:
		return FORMAT_U8;
	case AV_SAMPLE_FMT_S16:
		return FORMAT_S16;
	case AV_SAMPLE_FMT_S32:
		return FORMAT_S32;
	case AV_SAMPLE_FMT_FLT:
		return FORMAT_FLOAT32;
	case AV_SAMPLE_FMT_DBL:
		return FORMAT_FLOAT64;
	default:
		AUD_THROW(FileException, "FFMPEG sample format unknown.");
	}
}

int FFMPEGReader::decode(AVPacket& packet, Buffer& buffer)
{
	int buf_size = buffer.getSize();
	int buf_pos = 0;

#ifdef FFMPEG_OLD_CODE
	int got_frame;
	int read_length;
	uint8_t* orig_data = packet.data;
	int orig_size = packet.size;

	while(packet.size > 0)
	{
		got_frame = 0;

		read_length = avcodec_decode_audio4(m_codecCtx, m_frame, &got_frame, &packet);
		if(read_length < 0)
			break;

		if(got_frame)
		{
			int data_size = av_samples_get_buffer_size(nullptr, m_codecCtx->channels, m_frame->nb_samples, m_codecCtx->sample_fmt, 1);

			if(buf_size - buf_pos < data_size)
			{
				buffer.resize(buf_size + data_size, true);
				buf_size += data_size;
			}

			if(m_tointerleave)
			{
				int single_size = data_size / m_codecCtx->channels / m_frame->nb_samples;
				for(int channel = 0; channel < m_codecCtx->channels; channel++)
				{
					for(int i = 0; i < m_frame->nb_samples; i++)
					{
						std::memcpy(((data_t*)buffer.getBuffer()) + buf_pos + ((m_codecCtx->channels * i) + channel) * single_size,
							   m_frame->data[channel] + i * single_size, single_size);
					}
				}
			}
			else
				std::memcpy(((data_t*)buffer.getBuffer()) + buf_pos, m_frame->data[0], data_size);

			buf_pos += data_size;
		}
		packet.size -= read_length;
		packet.data += read_length;
	}

	packet.data = orig_data;
	packet.size = orig_size;
#else
	avcodec_send_packet(m_codecCtx, &packet);

	while(true)
	{
		auto ret = avcodec_receive_frame(m_codecCtx, m_frame);

		if(ret != 0)
			break;

		int data_size = av_samples_get_buffer_size(nullptr, m_codecCtx->channels, m_frame->nb_samples, m_codecCtx->sample_fmt, 1);

		if(buf_size - buf_pos < data_size)
		{
			buffer.resize(buf_size + data_size, true);
			buf_size += data_size;
		}

		if(m_tointerleave)
		{
			int single_size = data_size / m_codecCtx->channels / m_frame->nb_samples;
			for(int channel = 0; channel < m_codecCtx->channels; channel++)
			{
				for(int i = 0; i < m_frame->nb_samples; i++)
				{
					std::memcpy(((data_t*)buffer.getBuffer()) + buf_pos + ((m_codecCtx->channels * i) + channel) * single_size,
						   m_frame->data[channel] + i * single_size, single_size);
				}
			}
		}
		else
			std::memcpy(((data_t*)buffer.getBuffer()) + buf_pos, m_frame->data[0], data_size);

		buf_pos += data_size;
	}
#endif

	return buf_pos;
}

void FFMPEGReader::init(int stream)
{
	m_position = 0;
	m_pkgbuf_left = 0;

	if(avformat_find_stream_info(m_formatCtx, nullptr) < 0)
		AUD_THROW(FileException, "File couldn't be read, ffmpeg couldn't find the stream info.");

	// find audio stream and codec
	m_stream = -1;

	for(unsigned int i = 0; i < m_formatCtx->nb_streams; i++)
	{
#ifdef FFMPEG_OLD_CODE
		if((m_formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
#else
		if((m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
#endif
			&& (m_stream < 0))
		{
			if(stream == 0)
			{
				m_stream=i;
				break;
			}
			else
				stream--;
		}
	}

	if(m_stream == -1)
		AUD_THROW(FileException, "File couldn't be read, no audio stream found by ffmpeg.");

	// get a decoder and open it
#ifndef FFMPEG_OLD_CODE
	const AVCodec* aCodec = avcodec_find_decoder(m_formatCtx->streams[m_stream]->codecpar->codec_id);

	if(!aCodec)
		AUD_THROW(FileException, "File couldn't be read, no decoder found with ffmpeg.");
#endif

	m_frame = av_frame_alloc();

	if(!m_frame)
		AUD_THROW(FileException, "File couldn't be read, ffmpeg frame couldn't be allocated.");

#ifdef FFMPEG_OLD_CODE
	m_codecCtx = m_formatCtx->streams[m_stream]->codec;

	AVCodec* aCodec = avcodec_find_decoder(m_codecCtx->codec_id);
#else
	m_codecCtx = avcodec_alloc_context3(aCodec);
#endif

	if(!m_codecCtx)
		AUD_THROW(FileException, "File couldn't be read, ffmpeg context couldn't be allocated.");

#ifndef FFMPEG_OLD_CODE
	if(avcodec_parameters_to_context(m_codecCtx, m_formatCtx->streams[m_stream]->codecpar) < 0)
		AUD_THROW(FileException, "File couldn't be read, ffmpeg decoder parameters couldn't be copied to decoder context.");
#endif

	if(avcodec_open2(m_codecCtx, aCodec, nullptr) < 0)
		AUD_THROW(FileException, "File couldn't be read, ffmpeg codec couldn't be opened.");

	m_specs.channels = (Channels) m_codecCtx->channels;
	m_tointerleave = av_sample_fmt_is_planar(m_codecCtx->sample_fmt);

	switch(av_get_packed_sample_fmt(m_codecCtx->sample_fmt))
	{
	case AV_SAMPLE_FMT_U8:
		m_convert = convert_u8_float;
		m_specs.format = FORMAT_U8;
		break;
	case AV_SAMPLE_FMT_S16:
		m_convert = convert_s16_float;
		m_specs.format = FORMAT_S16;
		break;
	case AV_SAMPLE_FMT_S32:
		m_convert = convert_s32_float;
		m_specs.format = FORMAT_S32;
		break;
	case AV_SAMPLE_FMT_FLT:
		m_convert = convert_copy<float>;
		m_specs.format = FORMAT_FLOAT32;
		break;
	case AV_SAMPLE_FMT_DBL:
		m_convert = convert_double_float;
		m_specs.format = FORMAT_FLOAT64;
		break;
	default:
		AUD_THROW(FileException, "File couldn't be read, ffmpeg sample format unknown.");
	}

	m_specs.rate = (SampleRate) m_codecCtx->sample_rate;
}

FFMPEGReader::FFMPEGReader(std::string filename, int stream) :
	m_pkgbuf(),
	m_formatCtx(nullptr),
	m_codecCtx(nullptr),
	m_frame(nullptr),
	m_aviocontext(nullptr),
	m_membuf(nullptr)
{
	// open file
	if(avformat_open_input(&m_formatCtx, filename.c_str(), nullptr, nullptr)!=0)
		AUD_THROW(FileException, "File couldn't be opened with ffmpeg.");

	try
	{
		init(stream);
	}
	catch(Exception&)
	{
		avformat_close_input(&m_formatCtx);
		throw;
	}
}

FFMPEGReader::FFMPEGReader(std::shared_ptr<Buffer> buffer, int stream) :
		m_pkgbuf(),
		m_codecCtx(nullptr),
		m_frame(nullptr),
		m_membuffer(buffer),
		m_membufferpos(0)
{
	m_membuf = reinterpret_cast<data_t*>(av_malloc(AV_INPUT_BUFFER_MIN_SIZE + AV_INPUT_BUFFER_PADDING_SIZE));

	m_aviocontext = avio_alloc_context(m_membuf, AV_INPUT_BUFFER_MIN_SIZE, 0, this, read_packet, nullptr, seek_packet);

	if(!m_aviocontext)
	{
		av_free(m_aviocontext);
		AUD_THROW(FileException, "Buffer reading context couldn't be created with ffmpeg.");
	}

	m_formatCtx = avformat_alloc_context();
	m_formatCtx->pb = m_aviocontext;
	if(avformat_open_input(&m_formatCtx, "", nullptr, nullptr)!=0)
	{
		av_free(m_aviocontext);
		AUD_THROW(FileException, "Buffer couldn't be read with ffmpeg.");
	}

	try
	{
		init(stream);
	}
	catch(Exception&)
	{
		avformat_close_input(&m_formatCtx);
		av_free(m_aviocontext);
		throw;
	}
}

FFMPEGReader::~FFMPEGReader()
{
	if(m_frame)
		av_frame_free(&m_frame);
#ifdef FFMPEG_OLD_CODE
	avcodec_close(m_codecCtx);
#else
	if(m_codecCtx)
		avcodec_free_context(&m_codecCtx);
#endif
	avformat_close_input(&m_formatCtx);
}

std::vector<StreamInfo> FFMPEGReader::queryStreams()
{
	std::vector<StreamInfo> result;

	for(unsigned int i = 0; i < m_formatCtx->nb_streams; i++)
	{
#ifdef FFMPEG_OLD_CODE
		if(m_formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
#else
		if(m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
#endif
		{
			StreamInfo info;

			double time_base = av_q2d(m_formatCtx->streams[i]->time_base);

			if(m_formatCtx->streams[i]->start_time != AV_NOPTS_VALUE)
				info.start = m_formatCtx->streams[i]->start_time * time_base;
			else
				info.start = 0;

			if(m_formatCtx->streams[i]->duration != AV_NOPTS_VALUE)
				info.duration = m_formatCtx->streams[i]->duration * time_base;
			else if(m_formatCtx->duration != AV_NOPTS_VALUE)
				info.duration = double(m_formatCtx->duration) / AV_TIME_BASE - info.start;
			else
				info.duration = 0;

#ifdef FFMPEG_OLD_CODE
			info.specs.channels = Channels(m_formatCtx->streams[i]->codec->channels);
			info.specs.rate = m_formatCtx->streams[i]->codec->sample_rate;
			info.specs.format = convertSampleFormat(m_formatCtx->streams[i]->codec->sample_fmt);
#else
			info.specs.channels = Channels(m_formatCtx->streams[i]->codecpar->channels);
			info.specs.rate = m_formatCtx->streams[i]->codecpar->sample_rate;
			info.specs.format = convertSampleFormat(AVSampleFormat(m_formatCtx->streams[i]->codecpar->format));
#endif

			result.emplace_back(info);
		}
	}

	return result;
}

int FFMPEGReader::read_packet(void* opaque, uint8_t* buf, int buf_size)
{
	FFMPEGReader* reader = reinterpret_cast<FFMPEGReader*>(opaque);

	long long size = std::min(static_cast<long long>(buf_size), reader->m_membuffer->getSize() - reader->m_membufferpos);

	if(size <= 0)
		return AVERROR_EOF;

	std::memcpy(buf, ((data_t*)reader->m_membuffer->getBuffer()) + reader->m_membufferpos, size);
	reader->m_membufferpos += size;

	return size;
}

int64_t FFMPEGReader::seek_packet(void* opaque, int64_t offset, int whence)
{
	FFMPEGReader* reader = reinterpret_cast<FFMPEGReader*>(opaque);

	switch(whence)
	{
	case SEEK_SET:
		reader->m_membufferpos = 0;
		break;
	case SEEK_END:
		reader->m_membufferpos = reader->m_membuffer->getSize();
		break;
	case AVSEEK_SIZE:
		return reader->m_membuffer->getSize();
	}

	int64_t position = reader->m_membufferpos + offset;

	if(position > reader->m_membuffer->getSize())
		position = reader->m_membuffer->getSize();

	reader->m_membufferpos = int(position);

	return position;
}

bool FFMPEGReader::isSeekable() const
{
	return true;
}

void FFMPEGReader::seek(int position)
{
	if(position >= 0)
	{
		double pts_time_base = av_q2d(m_formatCtx->streams[m_stream]->time_base);

		uint64_t st_time = m_formatCtx->streams[m_stream]->start_time;
		uint64_t seek_pos = (uint64_t)(position / (pts_time_base * m_specs.rate));

		if(st_time != AV_NOPTS_VALUE)
			seek_pos += st_time;

		// a value < 0 tells us that seeking failed
		if(av_seek_frame(m_formatCtx, m_stream, seek_pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY) >= 0)
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
					m_pkgbuf_left = decode(packet, m_pkgbuf);
					search = false;

					// check position
					if(packet.pts != AV_NOPTS_VALUE)
					{
						// calculate real position, and read to frame!
						m_position = (packet.pts - (st_time != AV_NOPTS_VALUE ? st_time : 0)) * pts_time_base * m_specs.rate;

						if(m_position < position)
						{
							// read until we're at the right position
							int length = AUD_DEFAULT_BUFFER_SIZE;
							Buffer buffer(length * AUD_SAMPLE_SIZE(m_specs));
							bool eos;
							for(int len = position - m_position; len > 0; len -= AUD_DEFAULT_BUFFER_SIZE)
							{
								if(len < AUD_DEFAULT_BUFFER_SIZE)
									length = len;
								read(length, eos, buffer.getBuffer());
							}
						}
					}
				}
				av_packet_unref(&packet);
			}
		}
		else
		{
			fprintf(stderr, "seeking failed!\n");
			// Seeking failed, do nothing.
		}
	}
}

int FFMPEGReader::getLength() const
{
	auto stream = m_formatCtx->streams[m_stream];

	double time_base = av_q2d(stream->time_base);
	double duration;

	if(stream->duration != AV_NOPTS_VALUE)
		duration = stream->duration * time_base;
	else if(m_formatCtx->duration != AV_NOPTS_VALUE)
	{
		duration = float(m_formatCtx->duration) / AV_TIME_BASE;

		if(stream->start_time != AV_NOPTS_VALUE)
			duration -= stream->start_time * time_base;
	}
	else
		duration = -1;

	// return approximated remaning size
	return (int)(duration * m_codecCtx->sample_rate) - m_position;
}

int FFMPEGReader::getPosition() const
{
	return m_position;
}

Specs FFMPEGReader::getSpecs() const
{
	return m_specs.specs;
}

void FFMPEGReader::read(int& length, bool& eos, sample_t* buffer)
{
	// read packages and decode them
	AVPacket packet = {};
	int data_size = 0;
	int pkgbuf_pos;
	int left = length;
	int sample_size = AUD_DEVICE_SAMPLE_SIZE(m_specs);

	sample_t* buf = buffer;
	pkgbuf_pos = m_pkgbuf_left;
	m_pkgbuf_left = 0;

	// there may still be data in the buffer from the last call
	if(pkgbuf_pos > 0)
	{
		data_size = std::min(pkgbuf_pos, left * sample_size);
		m_convert((data_t*) buf, (data_t*) m_pkgbuf.getBuffer(), data_size / AUD_FORMAT_SIZE(m_specs.format));
		buf += data_size / AUD_FORMAT_SIZE(m_specs.format);
		left -= data_size / sample_size;
	}

	// for each frame read as long as there isn't enough data already
	while((left > 0) && (av_read_frame(m_formatCtx, &packet) >= 0))
	{
		// is it a frame from the audio stream?
		if(packet.stream_index == m_stream)
		{
			// decode the package
			pkgbuf_pos = decode(packet, m_pkgbuf);

			// copy to output buffer
			data_size = std::min(pkgbuf_pos, left * sample_size);
			m_convert((data_t*) buf, (data_t*) m_pkgbuf.getBuffer(), data_size / AUD_FORMAT_SIZE(m_specs.format));
			buf += data_size / AUD_FORMAT_SIZE(m_specs.format);
			left -= data_size / sample_size;
		}
		av_packet_unref(&packet);
	}
	// read more data than necessary?
	if(pkgbuf_pos > data_size)
	{
		m_pkgbuf_left = pkgbuf_pos-data_size;
		memmove(m_pkgbuf.getBuffer(),
				((data_t*)m_pkgbuf.getBuffer())+data_size,
				pkgbuf_pos-data_size);
	}

	if((eos = (left > 0)))
		length -= left;

	m_position += length;
}

AUD_NAMESPACE_END
