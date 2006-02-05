/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * Contributor(s): Peter Schlaile <peter@schlaile.de> 2005
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WITH_FFMPEG
#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>
#if LIBAVFORMAT_VERSION_INT < (49 << 16)
#define FFMPEG_OLD_FRAME_RATE 1
#else
#define FFMPEG_CODEC_IS_POINTER 1
#endif
#endif

#include "MEM_guardedalloc.h"

#include "BIF_editsound.h"

#include "blendef.h"

extern void do_init_ffmpeg();

struct hdaudio {
	int sample_rate;
	int channels;
	int audioStream;

#ifdef WITH_FFMPEG
	char * filename;
	AVCodec *pCodec;
	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;
	int frame_position;
	int frame_duration;
	int frame_size;
	short * decode_cache;
	int decode_cache_size;
	int target_channels;
	int target_rate;
	ReSampleContext *resampler;
#else
	

#endif
};

struct hdaudio * sound_open_hdaudio(char * filename)
{
#ifdef WITH_FFMPEG
	struct hdaudio * rval;
	int            i, audioStream;

	AVCodec *pCodec;
	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;

	do_init_ffmpeg();

	if(av_open_input_file(&pFormatCtx, filename, NULL, 0, NULL)!=0) {
		return 0;
	}

	if(av_find_stream_info(pFormatCtx)<0) {
		av_close_input_file(pFormatCtx);
		return 0;
	}

	dump_format(pFormatCtx, 0, filename, 0);


        /* Find the first audio stream */
	audioStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++)
#ifdef FFMPEG_CODEC_IS_POINTER
		if(pFormatCtx->streams[i]->codec->codec_type==CODEC_TYPE_AUDIO)
#else
		if(pFormatCtx->streams[i]->codec.codec_type==CODEC_TYPE_AUDIO)
#endif
		{
			audioStream=i;
			break;
		}

	if(audioStream == -1) {
		av_close_input_file(pFormatCtx);
		return 0;
	}

#ifdef FFMPEG_CODEC_IS_POINTER
	pCodecCtx=pFormatCtx->streams[audioStream]->codec;
#else
	pCodecCtx=&pFormatCtx->streams[audioStream]->codec;
#endif

        /* Find the decoder for the audio stream */
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec == NULL) {
		avcodec_close(pCodecCtx);
		av_close_input_file(pFormatCtx);
		return 0;
	}

#if 0
	if(pCodec->capabilities & CODEC_CAP_TRUNCATED)
		pCodecCtx->flags|=CODEC_FLAG_TRUNCATED;
#endif

	if(avcodec_open(pCodecCtx, pCodec)<0) {
		avcodec_close(pCodecCtx);
		av_close_input_file(pFormatCtx);
		return 0;
	}

	rval = (struct hdaudio *)MEM_mallocN(sizeof(struct hdaudio), 
					     "hdaudio struct");

	rval->filename = strdup(filename);
	rval->sample_rate = pCodecCtx->sample_rate;
	rval->channels = pCodecCtx->channels;

	rval->pFormatCtx = pFormatCtx;
	rval->pCodecCtx = pCodecCtx;
	rval->pCodec = pCodec;
	rval->audioStream = audioStream;
	rval->frame_position = -1;

	/* FIXME: This only works with integer frame rates ... */
	rval->frame_duration = AV_TIME_BASE;
	rval->decode_cache_size = 
		(long long) rval->sample_rate * rval->channels
		* rval->frame_duration / AV_TIME_BASE
		* 2;

	rval->decode_cache = (short*) MEM_mallocN(
		rval->decode_cache_size * sizeof(short), 
		"hdaudio decode cache");

	rval->target_channels = -1;
	rval->target_rate = -1;
	rval->resampler = 0;
	return rval;
#else
	return 0;
#endif
}

struct hdaudio * sound_copy_hdaudio(struct hdaudio * c)
{
	return sound_open_hdaudio(c->filename);
}

long sound_hdaudio_get_duration(struct hdaudio * hdaudio, int frame_rate)
{
#ifdef WITH_FFMPEG
	return hdaudio->pFormatCtx->duration * frame_rate / AV_TIME_BASE;
#else
	return 0;
#endif
}

void sound_hdaudio_extract(struct hdaudio * hdaudio, 
			   short * target_buffer,
			   int sample_position /* units of target_rate */,
			   int target_rate,
			   int target_channels,
			   int nb_samples /* in target */)
{
#ifdef WITH_FFMPEG
	AVPacket packet;
	int frame_position;
	int frame_size = (long long) target_rate 
		* hdaudio->frame_duration / AV_TIME_BASE;
	int rate_conversion = 
		(target_rate != hdaudio->sample_rate) 
		|| (target_channels != hdaudio->channels);

	if (hdaudio == 0) return;

	if (rate_conversion) {
		if (hdaudio->resampler && 
		    (hdaudio->target_rate != target_rate
		     || hdaudio->target_channels != target_channels)) {
			audio_resample_close(hdaudio->resampler);
			hdaudio->resampler = 0;
		}
		if (!hdaudio->resampler) {
			hdaudio->resampler = audio_resample_init(
				target_channels, hdaudio->channels,
				target_rate, hdaudio->sample_rate);
			hdaudio->target_rate = target_rate;
			hdaudio->target_channels = target_channels;
		}
	}

	frame_position = sample_position / frame_size; 

	if (frame_position != hdaudio->frame_position) { 
		long decode_pos = 0;

		hdaudio->frame_position = frame_position;

		av_seek_frame(hdaudio->pFormatCtx, -1, 
			      (long long) frame_position * AV_TIME_BASE, 
			      AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD);

		while(av_read_frame(hdaudio->pFormatCtx, &packet) >= 0) {
			if(packet.stream_index == hdaudio->audioStream) {
				int data_size;
				int len;
				uint8_t *audio_pkt_data;
				int audio_pkt_size;
	
				audio_pkt_data = packet.data;
				audio_pkt_size = packet.size;

				while (audio_pkt_size > 0) {
					len = avcodec_decode_audio(
						hdaudio->pCodecCtx, 
						hdaudio->decode_cache 
						+ decode_pos, 
						&data_size, 
						audio_pkt_data, 
						audio_pkt_size);
					if (data_size <= 0) {
						continue;
					}
					if (len < 0) {
					        audio_pkt_size = 0;
						break;
					}

					audio_pkt_size -= len;
					audio_pkt_data += len;

					decode_pos += 
						data_size / sizeof(short);
					if (decode_pos + data_size
					    / sizeof(short)
					    > hdaudio->decode_cache_size) {
						av_free_packet(&packet);
						break;
					}
				}
				if (decode_pos + data_size / sizeof(short)
				    > hdaudio->decode_cache_size) {
					break;
				}
			}
			av_free_packet(&packet);
		}
	}

	if (!rate_conversion) {
		int ofs = target_channels * (sample_position % frame_size);
		memcpy(target_buffer,
		       hdaudio->decode_cache + ofs,
		       nb_samples * target_channels * sizeof(short));
	} else {
		double ratio = (double) hdaudio->sample_rate / target_rate;
		long in_samples = (long) ((nb_samples + 16) * ratio);
		short temp_buffer[target_channels * (nb_samples + 64)];

		int s = audio_resample(hdaudio->resampler,
				       temp_buffer,
				       hdaudio->decode_cache
				       + target_channels * 
				       (long) 
				       (ratio*(sample_position % frame_size)),
				       in_samples);
		if (s < nb_samples || s > nb_samples + 63) {
			fprintf(stderr, "resample ouch: %d != %d\n",
				s, nb_samples);
		}
		memcpy(target_buffer, temp_buffer,
		       nb_samples * target_channels * sizeof(short));
	}
#else

#endif
}
			   
void sound_close_hdaudio(struct hdaudio * hdaudio)
{
#ifdef WITH_FFMPEG

	if (hdaudio) {
		avcodec_close(hdaudio->pCodecCtx);
		av_close_input_file(hdaudio->pFormatCtx);
		MEM_freeN (hdaudio->decode_cache);
		free(hdaudio->filename);
		MEM_freeN (hdaudio);
	}
#else

#endif
}
