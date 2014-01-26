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

/** \file audaspace/ffmpeg/AUD_FFMPEGWriter.cpp
 *  \ingroup audffmpeg
 */


// needed for INT64_C
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include "AUD_FFMPEGWriter.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include "ffmpeg_compat.h"
}

static const char* context_error = "AUD_FFMPEGWriter: Couldn't allocate context.";
static const char* codec_error = "AUD_FFMPEGWriter: Invalid codec or codec not found.";
static const char* stream_error = "AUD_FFMPEGWriter: Couldn't allocate stream.";
static const char* format_error = "AUD_FFMPEGWriter: Unsupported sample format.";
static const char* file_error = "AUD_FFMPEGWriter: File couldn't be written.";
static const char* write_error = "AUD_FFMPEGWriter: Error writing packet.";

AUD_FFMPEGWriter::AUD_FFMPEGWriter(std::string filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate) :
	m_position(0),
	m_specs(specs),
	m_input_samples(0)
{
	static const char* formats[] = { NULL, "ac3", "flac", "matroska", "mp2", "mp3", "ogg", "wav" };

	m_formatCtx = avformat_alloc_context();
	if (!m_formatCtx) AUD_THROW(AUD_ERROR_FFMPEG, context_error);

	strcpy(m_formatCtx->filename, filename.c_str());
	m_outputFmt = m_formatCtx->oformat = av_guess_format(formats[format], filename.c_str(), NULL);
	if (!m_outputFmt) {
		avformat_free_context(m_formatCtx);
		AUD_THROW(AUD_ERROR_FFMPEG, context_error);
	}

	switch(codec)
	{
	case AUD_CODEC_AAC:
		m_outputFmt->audio_codec = AV_CODEC_ID_AAC;
		break;
	case AUD_CODEC_AC3:
		m_outputFmt->audio_codec = AV_CODEC_ID_AC3;
		break;
	case AUD_CODEC_FLAC:
		m_outputFmt->audio_codec = AV_CODEC_ID_FLAC;
		break;
	case AUD_CODEC_MP2:
		m_outputFmt->audio_codec = AV_CODEC_ID_MP2;
		break;
	case AUD_CODEC_MP3:
		m_outputFmt->audio_codec = AV_CODEC_ID_MP3;
		break;
	case AUD_CODEC_PCM:
		switch(specs.format)
		{
		case AUD_FORMAT_U8:
			m_outputFmt->audio_codec = AV_CODEC_ID_PCM_U8;
			break;
		case AUD_FORMAT_S16:
			m_outputFmt->audio_codec = AV_CODEC_ID_PCM_S16LE;
			break;
		case AUD_FORMAT_S24:
			m_outputFmt->audio_codec = AV_CODEC_ID_PCM_S24LE;
			break;
		case AUD_FORMAT_S32:
			m_outputFmt->audio_codec = AV_CODEC_ID_PCM_S32LE;
			break;
		case AUD_FORMAT_FLOAT32:
			m_outputFmt->audio_codec = AV_CODEC_ID_PCM_F32LE;
			break;
		case AUD_FORMAT_FLOAT64:
			m_outputFmt->audio_codec = AV_CODEC_ID_PCM_F64LE;
			break;
		default:
			m_outputFmt->audio_codec = AV_CODEC_ID_NONE;
			break;
		}
		break;
	case AUD_CODEC_VORBIS:
		m_outputFmt->audio_codec = AV_CODEC_ID_VORBIS;
		break;
	default:
		m_outputFmt->audio_codec = AV_CODEC_ID_NONE;
		break;
	}

	try
	{
		if(m_outputFmt->audio_codec == AV_CODEC_ID_NONE)
			AUD_THROW(AUD_ERROR_SPECS, codec_error);

		m_stream = avformat_new_stream(m_formatCtx, NULL);
		if(!m_stream)
			AUD_THROW(AUD_ERROR_FFMPEG, stream_error);

		m_codecCtx = m_stream->codec;
		m_codecCtx->codec_id = m_outputFmt->audio_codec;
		m_codecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
		m_codecCtx->bit_rate = bitrate;
		m_codecCtx->sample_rate = int(m_specs.rate);
		m_codecCtx->channels = m_specs.channels;
		m_codecCtx->time_base.num = 1;
		m_codecCtx->time_base.den = m_codecCtx->sample_rate;

		switch(m_specs.format)
		{
		case AUD_FORMAT_U8:
			m_convert = AUD_convert_float_u8;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_U8;
			break;
		case AUD_FORMAT_S16:
			m_convert = AUD_convert_float_s16;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
			break;
		case AUD_FORMAT_S32:
			m_convert = AUD_convert_float_s32;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_S32;
			break;
		case AUD_FORMAT_FLOAT32:
			m_convert = AUD_convert_copy<float>;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_FLT;
			break;
		case AUD_FORMAT_FLOAT64:
			m_convert = AUD_convert_float_double;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_DBL;
			break;
		default:
			AUD_THROW(AUD_ERROR_FFMPEG, format_error);
		}

		try
		{
			if(m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER)
				m_codecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

			AVCodec* codec = avcodec_find_encoder(m_codecCtx->codec_id);
			if(!codec)
				AUD_THROW(AUD_ERROR_FFMPEG, codec_error);

			if(avcodec_open2(m_codecCtx, codec, NULL))
				AUD_THROW(AUD_ERROR_FFMPEG, codec_error);

			m_output_buffer.resize(FF_MIN_BUFFER_SIZE);
			int samplesize = AUD_MAX(AUD_SAMPLE_SIZE(m_specs), AUD_DEVICE_SAMPLE_SIZE(m_specs));

			if(m_codecCtx->frame_size <= 1)
				m_input_size = 0;
			else
			{
				m_input_buffer.resize(m_codecCtx->frame_size * samplesize);
				m_input_size = m_codecCtx->frame_size;
			}

#ifdef FFMPEG_HAVE_ENCODE_AUDIO2
			m_frame = av_frame_alloc();
			if (!m_frame)
				AUD_THROW(AUD_ERROR_FFMPEG, codec_error);
			m_frame->linesize[0]    = m_input_size * samplesize;
			m_frame->format         = m_codecCtx->sample_fmt;
#  ifdef FFMPEG_HAVE_AVFRAME_SAMPLE_RATE
			m_frame->sample_rate    = m_codecCtx->sample_rate;
#  endif
#  ifdef FFMPEG_HAVE_FRAME_CHANNEL_LAYOUT
			m_frame->channel_layout = m_codecCtx->channel_layout;
#  endif
#endif

			try
			{
				if(avio_open(&m_formatCtx->pb, filename.c_str(), AVIO_FLAG_WRITE))
					AUD_THROW(AUD_ERROR_FILE, file_error);

				avformat_write_header(m_formatCtx, NULL);
			}
			catch(AUD_Exception&)
			{
				avcodec_close(m_codecCtx);
				av_freep(&m_formatCtx->streams[0]->codec);
				throw;
			}
		}
		catch(AUD_Exception&)
		{
			av_freep(&m_formatCtx->streams[0]);
			throw;
		}
	}
	catch(AUD_Exception&)
	{
		av_free(m_formatCtx);
		throw;
	}
}

AUD_FFMPEGWriter::~AUD_FFMPEGWriter()
{
	// writte missing data
	if(m_input_samples)
	{
		sample_t* buf = m_input_buffer.getBuffer();
		memset(buf + m_specs.channels * m_input_samples, 0,
			   (m_input_size - m_input_samples) * AUD_DEVICE_SAMPLE_SIZE(m_specs));

		encode(buf);
	}

	av_write_trailer(m_formatCtx);

	avcodec_close(m_codecCtx);

	av_freep(&m_formatCtx->streams[0]->codec);
	av_freep(&m_formatCtx->streams[0]);

#ifdef FFMPEG_HAVE_ENCODE_AUDIO2
	av_frame_free(&m_frame);
#endif

	avio_close(m_formatCtx->pb);
	av_free(m_formatCtx);
}

int AUD_FFMPEGWriter::getPosition() const
{
	return m_position;
}

AUD_DeviceSpecs AUD_FFMPEGWriter::getSpecs() const
{
	return m_specs;
}

void AUD_FFMPEGWriter::encode(sample_t* data)
{
	// convert first
	if(m_input_size)
		m_convert(reinterpret_cast<data_t*>(data), reinterpret_cast<data_t*>(data), m_input_size * m_specs.channels);

	AVPacket packet = { 0 };
	av_init_packet(&packet);

#ifdef FFMPEG_HAVE_ENCODE_AUDIO2
	int got_output, ret;

	m_frame->data[0] = reinterpret_cast<uint8_t*>(data);
	ret = avcodec_encode_audio2(m_codecCtx, &packet, m_frame, &got_output);
	if (ret < 0)
		AUD_THROW(AUD_ERROR_FFMPEG, codec_error);

	if (!got_output)
		return;
#else
	sample_t* outbuf = m_output_buffer.getBuffer();

	packet.size = avcodec_encode_audio(m_codecCtx, reinterpret_cast<uint8_t*>(outbuf), m_output_buffer.getSize(), reinterpret_cast<short*>(data));
	if(m_codecCtx->coded_frame && m_codecCtx->coded_frame->pts != AV_NOPTS_VALUE)
		packet.pts = av_rescale_q(m_codecCtx->coded_frame->pts, m_codecCtx->time_base, m_stream->time_base);
	packet.flags |= AV_PKT_FLAG_KEY;
	packet.data = reinterpret_cast<uint8_t*>(outbuf);
#endif

	packet.stream_index = m_stream->index;

	if(av_interleaved_write_frame(m_formatCtx, &packet))
		AUD_THROW(AUD_ERROR_FFMPEG, write_error);
}

void AUD_FFMPEGWriter::write(unsigned int length, sample_t* buffer)
{
	unsigned int samplesize = AUD_SAMPLE_SIZE(m_specs);

	if(m_input_size)
	{
		sample_t* inbuf = m_input_buffer.getBuffer();

		while(length)
		{
			unsigned int len = AUD_MIN(m_input_size - m_input_samples, length);

			memcpy(inbuf + m_input_samples * m_specs.channels, buffer, len * samplesize);

			buffer += len * m_specs.channels;
			m_input_samples += len;
			m_position += len;
			length -= len;

			if(m_input_samples == m_input_size)
			{
				encode(inbuf);

				m_input_samples = 0;
			}
		}
	}
	else // PCM data, can write directly!
	{
		int samplesize = AUD_SAMPLE_SIZE(m_specs);
		if(m_output_buffer.getSize() != length * m_specs.channels * m_codecCtx->bits_per_coded_sample / 8)
			m_output_buffer.resize(length * m_specs.channels * m_codecCtx->bits_per_coded_sample / 8);
		m_input_buffer.assureSize(length * AUD_MAX(AUD_DEVICE_SAMPLE_SIZE(m_specs), samplesize));

		sample_t* buf = m_input_buffer.getBuffer();
		m_convert(reinterpret_cast<data_t*>(buf), reinterpret_cast<data_t*>(buffer), length * m_specs.channels);

		encode(buf);

		m_position += length;
	}
}
