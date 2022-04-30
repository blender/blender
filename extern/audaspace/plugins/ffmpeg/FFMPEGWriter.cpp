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

#include "FFMPEGWriter.h"
#include "Exception.h"

#include <algorithm>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#if LIBAVCODEC_VERSION_MAJOR >= 59
#include <libavutil/channel_layout.h>
#endif
}

AUD_NAMESPACE_BEGIN

#if LIBAVCODEC_VERSION_MAJOR < 58
#define FFMPEG_OLD_CODE
#endif

void FFMPEGWriter::encode()
{
	sample_t* data = m_input_buffer.getBuffer();

	if(m_deinterleave)
	{
		m_deinterleave_buffer.assureSize(m_input_buffer.getSize());

		sample_t* dbuf = m_deinterleave_buffer.getBuffer();
		// deinterleave
		int single_size = sizeof(sample_t);
		for(int channel = 0; channel < m_specs.channels; channel++)
		{
			for(int i = 0; i < m_input_buffer.getSize() / AUD_SAMPLE_SIZE(m_specs); i++)
			{
				std::memcpy(((data_t*)dbuf) + (m_input_samples * channel + i) * single_size,
							((data_t*)data) + ((m_specs.channels * i) + channel) * single_size, single_size);
			}
		}

		// convert first
		if(m_input_size)
			m_convert(reinterpret_cast<data_t*>(data), reinterpret_cast<data_t*>(dbuf), m_input_samples * m_specs.channels);
		else
			std::memcpy(data, dbuf, m_input_buffer.getSize());
	}
	else
		// convert first
		if(m_input_size)
			m_convert(reinterpret_cast<data_t*>(data), reinterpret_cast<data_t*>(data), m_input_samples * m_specs.channels);

#ifdef FFMPEG_OLD_CODE
	m_packet->data = nullptr;
	m_packet->size = 0;

	av_init_packet(m_packet);

	av_frame_unref(m_frame);
	int got_packet;
#endif

	m_frame->nb_samples = m_input_samples;
	m_frame->format = m_codecCtx->sample_fmt;
	m_frame->channel_layout = m_codecCtx->channel_layout;
	m_frame->channels = m_specs.channels;

	if(avcodec_fill_audio_frame(m_frame, m_specs.channels, m_codecCtx->sample_fmt, reinterpret_cast<data_t*>(data), m_input_buffer.getSize(), 0) < 0)
		AUD_THROW(FileException, "File couldn't be written, filling the audio frame failed with ffmpeg.");

	AVRational sample_time = { 1, static_cast<int>(m_specs.rate) };
	m_frame->pts = av_rescale_q(m_position - m_input_samples, m_codecCtx->time_base, sample_time);

#ifdef FFMPEG_OLD_CODE
	if(avcodec_encode_audio2(m_codecCtx, m_packet, m_frame, &got_packet))
	{
		AUD_THROW(FileException, "File couldn't be written, audio encoding failed with ffmpeg.");
	}

	if(got_packet)
	{
		m_packet->flags |= AV_PKT_FLAG_KEY;
		m_packet->stream_index = m_stream->index;
		if(av_write_frame(m_formatCtx, m_packet) < 0)
		{
			av_free_packet(m_packet);
			AUD_THROW(FileException, "Frame couldn't be writen to the file with ffmpeg.");
		}
		av_free_packet(m_packet);
	}
#else
	if(avcodec_send_frame(m_codecCtx, m_frame) < 0)
		AUD_THROW(FileException, "File couldn't be written, audio encoding failed with ffmpeg.");

	while(avcodec_receive_packet(m_codecCtx, m_packet) == 0)
	{
		m_packet->stream_index = m_stream->index;

		if(av_write_frame(m_formatCtx, m_packet) < 0)
			AUD_THROW(FileException, "Frame couldn't be writen to the file with ffmpeg.");
	}
#endif
}

void FFMPEGWriter::close()
{
#ifdef FFMPEG_OLD_CODE
	int got_packet = true;

	while(got_packet)
	{
		m_packet->data = nullptr;
		m_packet->size = 0;

		av_init_packet(m_packet);

		if(avcodec_encode_audio2(m_codecCtx, m_packet, nullptr, &got_packet))
			AUD_THROW(FileException, "File end couldn't be written, audio encoding failed with ffmpeg.");

		if(got_packet)
		{
			m_packet->flags |= AV_PKT_FLAG_KEY;
			m_packet->stream_index = m_stream->index;
			if(av_write_frame(m_formatCtx, m_packet))
			{
				av_free_packet(m_packet);
				AUD_THROW(FileException, "Final frames couldn't be writen to the file with ffmpeg.");
			}
			av_free_packet(m_packet);
		}
	}
#else
	if(avcodec_send_frame(m_codecCtx, nullptr) < 0)
		AUD_THROW(FileException, "File couldn't be written, audio encoding failed with ffmpeg.");

	while(avcodec_receive_packet(m_codecCtx, m_packet) == 0)
	{
		m_packet->stream_index = m_stream->index;

		if(av_write_frame(m_formatCtx, m_packet) < 0)
			AUD_THROW(FileException, "Frame couldn't be writen to the file with ffmpeg.");
	}
#endif
}

FFMPEGWriter::FFMPEGWriter(std::string filename, DeviceSpecs specs, Container format, Codec codec, unsigned int bitrate) :
	m_position(0),
	m_specs(specs),
	m_formatCtx(nullptr),
	m_codecCtx(nullptr),
	m_stream(nullptr),
	m_packet(nullptr),
	m_frame(nullptr),
	m_input_samples(0),
	m_deinterleave(false)
{
	static const char* formats[] = { nullptr, "ac3", "flac", "matroska", "mp2", "mp3", "ogg", "wav" };

	if(avformat_alloc_output_context2(&m_formatCtx, nullptr, formats[format], filename.c_str()) < 0)
		AUD_THROW(FileException, "File couldn't be written, format couldn't be found with ffmpeg.");

	const AVOutputFormat* outputFmt = m_formatCtx->oformat;

	if(!outputFmt) {
		avformat_free_context(m_formatCtx);
		AUD_THROW(FileException, "File couldn't be written, output format couldn't be found with ffmpeg.");
	}

	AVCodecID audio_codec = AV_CODEC_ID_NONE;

	switch(codec)
	{
	case CODEC_AAC:
		audio_codec = AV_CODEC_ID_AAC;
		break;
	case CODEC_AC3:
		audio_codec = AV_CODEC_ID_AC3;
		break;
	case CODEC_FLAC:
		audio_codec = AV_CODEC_ID_FLAC;
		break;
	case CODEC_MP2:
		audio_codec = AV_CODEC_ID_MP2;
		break;
	case CODEC_MP3:
		audio_codec = AV_CODEC_ID_MP3;
		break;
	case CODEC_OPUS:
		audio_codec = AV_CODEC_ID_OPUS;
		break;
	case CODEC_PCM:
		switch(specs.format)
		{
		case FORMAT_U8:
			audio_codec = AV_CODEC_ID_PCM_U8;
			break;
		case FORMAT_S16:
			audio_codec = AV_CODEC_ID_PCM_S16LE;
			break;
		case FORMAT_S24:
			audio_codec = AV_CODEC_ID_PCM_S24LE;
			break;
		case FORMAT_S32:
			audio_codec = AV_CODEC_ID_PCM_S32LE;
			break;
		case FORMAT_FLOAT32:
			audio_codec = AV_CODEC_ID_PCM_F32LE;
			break;
		case FORMAT_FLOAT64:
			audio_codec = AV_CODEC_ID_PCM_F64LE;
			break;
		default:
			audio_codec = AV_CODEC_ID_NONE;
			break;
		}
		break;
	case CODEC_VORBIS:
		audio_codec = AV_CODEC_ID_VORBIS;
		break;
	default:
		audio_codec = AV_CODEC_ID_NONE;
		break;
	}

	uint64_t channel_layout = 0;

	switch(m_specs.channels)
	{
	case CHANNELS_MONO:
		channel_layout = AV_CH_LAYOUT_MONO;
		break;
	case CHANNELS_STEREO:
		channel_layout = AV_CH_LAYOUT_STEREO;
		break;
	case CHANNELS_STEREO_LFE:
		channel_layout = AV_CH_LAYOUT_2POINT1;
		break;
	case CHANNELS_SURROUND4:
		channel_layout = AV_CH_LAYOUT_QUAD;
		break;
	case CHANNELS_SURROUND5:
		channel_layout = AV_CH_LAYOUT_5POINT0_BACK;
		break;
	case CHANNELS_SURROUND51:
		channel_layout = AV_CH_LAYOUT_5POINT1_BACK;
		break;
	case CHANNELS_SURROUND61:
		channel_layout = AV_CH_LAYOUT_6POINT1_BACK;
		break;
	case CHANNELS_SURROUND71:
		channel_layout = AV_CH_LAYOUT_7POINT1;
		break;
	default:
		AUD_THROW(FileException, "File couldn't be written, channel layout not supported.");
	}

	try
	{
		if(audio_codec == AV_CODEC_ID_NONE)
			AUD_THROW(FileException, "File couldn't be written, audio codec not found with ffmpeg.");

		const AVCodec* codec = avcodec_find_encoder(audio_codec);
		if(!codec)
			AUD_THROW(FileException, "File couldn't be written, audio encoder couldn't be found with ffmpeg.");

		m_stream = avformat_new_stream(m_formatCtx, codec);
		if(!m_stream)
			AUD_THROW(FileException, "File couldn't be written, stream creation failed with ffmpeg.");

		m_stream->id = m_formatCtx->nb_streams - 1;

#ifdef FFMPEG_OLD_CODE
		m_codecCtx = m_stream->codec;
#else
		m_codecCtx = avcodec_alloc_context3(codec);
#endif

		if(!m_codecCtx)
			AUD_THROW(FileException, "File couldn't be written, context creation failed with ffmpeg.");

		switch(m_specs.format)
		{
		case FORMAT_U8:
			m_convert = convert_float_u8;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_U8;
			break;
		case FORMAT_S16:
			m_convert = convert_float_s16;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
			break;
		case FORMAT_S32:
			m_convert = convert_float_s32;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_S32;
			break;
		case FORMAT_FLOAT64:
			m_convert = convert_float_double;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_DBL;
			break;
		default:
			m_convert = convert_copy<sample_t>;
			m_codecCtx->sample_fmt = AV_SAMPLE_FMT_FLT;
			break;
		}

		if(m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER)
			m_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		bool format_supported = false;

		for(int i = 0; codec->sample_fmts[i] != -1; i++)
		{
			if(av_get_alt_sample_fmt(codec->sample_fmts[i], false) == m_codecCtx->sample_fmt)
			{
				m_deinterleave = av_sample_fmt_is_planar(codec->sample_fmts[i]);
				m_codecCtx->sample_fmt = codec->sample_fmts[i];
				format_supported = true;
			}
		}

		if(!format_supported)
		{
			int chosen_index = 0;
			auto chosen = av_get_alt_sample_fmt(codec->sample_fmts[chosen_index], false);
			for(int i = 1; codec->sample_fmts[i] != -1; i++)
			{
				auto fmt = av_get_alt_sample_fmt(codec->sample_fmts[i], false);
				if((fmt > chosen && chosen < m_codecCtx->sample_fmt) || (fmt > m_codecCtx->sample_fmt && fmt < chosen))
				{
					chosen = fmt;
					chosen_index = i;
				}
			}

			m_codecCtx->sample_fmt = codec->sample_fmts[chosen_index];
			m_deinterleave = av_sample_fmt_is_planar(m_codecCtx->sample_fmt);
			switch(av_get_alt_sample_fmt(m_codecCtx->sample_fmt, false))
			{
			case AV_SAMPLE_FMT_U8:
				specs.format = FORMAT_U8;
				m_convert = convert_float_u8;
				break;
			case AV_SAMPLE_FMT_S16:
				specs.format = FORMAT_S16;
				m_convert = convert_float_s16;
				break;
			case AV_SAMPLE_FMT_S32:
				specs.format = FORMAT_S32;
				m_convert = convert_float_s32;
				break;
			case AV_SAMPLE_FMT_FLT:
				specs.format = FORMAT_FLOAT32;
				m_convert = convert_copy<sample_t>;
				break;
			case AV_SAMPLE_FMT_DBL:
				specs.format = FORMAT_FLOAT64;
				m_convert = convert_float_double;
				break;
			default:
				AUD_THROW(FileException, "File couldn't be written, sample format not supported with ffmpeg.");
			}
		}

		m_codecCtx->sample_rate = 0;

		if(codec->supported_samplerates)
		{
			for(int i = 0; codec->supported_samplerates[i]; i++)
			{
				if(codec->supported_samplerates[i] == m_specs.rate)
				{
					m_codecCtx->sample_rate = codec->supported_samplerates[i];
					break;
				}
				else if((codec->supported_samplerates[i] > m_codecCtx->sample_rate && m_specs.rate > m_codecCtx->sample_rate) ||
						(codec->supported_samplerates[i] < m_codecCtx->sample_rate && m_specs.rate < codec->supported_samplerates[i]))
				{
					m_codecCtx->sample_rate = codec->supported_samplerates[i];
				}
			}
		}

		if(m_codecCtx->sample_rate == 0)
			m_codecCtx->sample_rate = m_specs.rate;

		m_specs.rate = m_codecCtx->sample_rate;

#ifdef FFMPEG_OLD_CODE
		m_codecCtx->codec_id = audio_codec;
#endif

		m_codecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
		m_codecCtx->bit_rate = bitrate;
		m_codecCtx->channel_layout = channel_layout;
		m_codecCtx->channels = m_specs.channels;
		m_stream->time_base.num = m_codecCtx->time_base.num = 1;
		m_stream->time_base.den = m_codecCtx->time_base.den = m_codecCtx->sample_rate;

		if(avcodec_open2(m_codecCtx, codec, nullptr) < 0)
			AUD_THROW(FileException, "File couldn't be written, encoder couldn't be opened with ffmpeg.");

#ifndef FFMPEG_OLD_CODE
		if(avcodec_parameters_from_context(m_stream->codecpar, m_codecCtx) < 0)
			AUD_THROW(FileException, "File couldn't be written, codec parameters couldn't be copied to the context.");
#endif

		int samplesize = std::max(int(AUD_SAMPLE_SIZE(m_specs)), AUD_DEVICE_SAMPLE_SIZE(m_specs));

		if((m_input_size = m_codecCtx->frame_size))
			m_input_buffer.resize(m_input_size * samplesize);

		if(avio_open(&m_formatCtx->pb, filename.c_str(), AVIO_FLAG_WRITE))
			AUD_THROW(FileException, "File couldn't be written, file opening failed with ffmpeg.");

		if(avformat_write_header(m_formatCtx, nullptr) < 0)
			AUD_THROW(FileException, "File couldn't be written, writing the header failed.");
	}
	catch(Exception&)
	{
#ifndef FFMPEG_OLD_CODE
		if(m_codecCtx)
			avcodec_free_context(&m_codecCtx);
#endif
		avformat_free_context(m_formatCtx);
		throw;
	}

#ifdef FFMPEG_OLD_CODE
	m_packet = new AVPacket({});
#else
	m_packet = av_packet_alloc();
#endif

	m_frame = av_frame_alloc();
}

FFMPEGWriter::~FFMPEGWriter()
{
	// writte missing data
	if(m_input_samples)
		encode();

	close();

	av_write_trailer(m_formatCtx);

	if(m_frame)
		av_frame_free(&m_frame);

	if(m_packet)
	{
#ifdef FFMPEG_OLD_CODE
		delete m_packet;
#else
		av_packet_free(&m_packet);
#endif
	}

#ifdef FFMPEG_OLD_CODE
	avcodec_close(m_codecCtx);
#else
	if(m_codecCtx)
		avcodec_free_context(&m_codecCtx);
#endif

	avio_closep(&m_formatCtx->pb);
	avformat_free_context(m_formatCtx);
}

int FFMPEGWriter::getPosition() const
{
	return m_position;
}

DeviceSpecs FFMPEGWriter::getSpecs() const
{
	return m_specs;
}

void FFMPEGWriter::write(unsigned int length, sample_t* buffer)
{
	unsigned int samplesize = AUD_SAMPLE_SIZE(m_specs);

	if(m_input_size)
	{
		sample_t* inbuf = m_input_buffer.getBuffer();

		while(length)
		{
			unsigned int len = std::min(m_input_size - m_input_samples, length);

			std::memcpy(inbuf + m_input_samples * m_specs.channels, buffer, len * samplesize);

			buffer += len * m_specs.channels;
			m_input_samples += len;
			m_position += len;
			length -= len;

			if(m_input_samples == m_input_size)
			{
				encode();

				m_input_samples = 0;
			}
		}
	}
	else // PCM data, can write directly!
	{
		int samplesize = AUD_SAMPLE_SIZE(m_specs);
		m_input_buffer.assureSize(length * std::max(AUD_DEVICE_SAMPLE_SIZE(m_specs), samplesize));

		sample_t* buf = m_input_buffer.getBuffer();
		m_convert(reinterpret_cast<data_t*>(buf), reinterpret_cast<data_t*>(buffer), length * m_specs.channels);

		m_input_samples = length;

		m_position += length;

		encode();
	}
}

AUD_NAMESPACE_END
