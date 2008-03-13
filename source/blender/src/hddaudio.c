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
 * Contributor(s): Peter Schlaile <peter [at] schlaile [dot] de> 2005
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
#include <ffmpeg/rational.h>
#if LIBAVFORMAT_VERSION_INT < (49 << 16)
#define FFMPEG_OLD_FRAME_RATE 1
#else
#define FFMPEG_CODEC_IS_POINTER 1
#endif
#endif

#include "MEM_guardedalloc.h"

#include "BIF_editsound.h"

#include "blendef.h"

#ifdef WITH_FFMPEG
extern void do_init_ffmpeg();
#endif

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
	int frame_alloc_duration;
	int decode_pos;
	int frame_size;
	short * decode_cache;
	short * decode_cache_zero;
	short * resample_cache;
	int decode_cache_size;
	int target_channels;
	int target_rate;
	int resample_samples_written;
	int resample_samples_in;
	ReSampleContext *resampler;
#else
	

#endif
};

#ifdef WITH_FFMPEG
#ifdef FFMPEG_CODEC_IS_POINTER
static AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return stream->codec;
}
#else
static AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return &stream->codec;
}
#endif
#endif

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
		if(get_codec_from_stream(pFormatCtx->streams[i])
		   ->codec_type == CODEC_TYPE_AUDIO)
		{
			audioStream=i;
			break;
		}

	if(audioStream == -1) {
		av_close_input_file(pFormatCtx);
		return 0;
	}

	pCodecCtx = get_codec_from_stream(pFormatCtx->streams[audioStream]);

        /* Find the decoder for the audio stream */
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec == NULL) {
		av_close_input_file(pFormatCtx);
		return 0;
	}

	if(avcodec_open(pCodecCtx, pCodec)<0) {
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
	rval->frame_position = -10;

	rval->frame_duration = AV_TIME_BASE / 10;
	rval->frame_alloc_duration = AV_TIME_BASE;
	rval->decode_cache_size = 
		(long long) rval->sample_rate * rval->channels
		* rval->frame_alloc_duration / AV_TIME_BASE
		* 2;

	rval->decode_cache = (short*) MEM_mallocN(
		rval->decode_cache_size * sizeof(short)
		+ AVCODEC_MAX_AUDIO_FRAME_SIZE, 
		"hdaudio decode cache");
	rval->decode_cache_zero = rval->decode_cache;
	rval->decode_pos = 0;
	rval->target_channels = -1;
	rval->target_rate = -1;
	rval->resampler = 0;
	rval->resample_cache = 0;
	rval->resample_samples_written = 0;
	rval->resample_samples_in = 0;
	return rval;
#else
	return 0;
#endif
}

struct hdaudio * sound_copy_hdaudio(struct hdaudio * c)
{
#ifdef WITH_FFMPEG
	return sound_open_hdaudio(c->filename);
#else
	return 0;
#endif
}

long sound_hdaudio_get_duration(struct hdaudio * hdaudio, double frame_rate)
{
#ifdef WITH_FFMPEG
	return hdaudio->pFormatCtx->duration * frame_rate / AV_TIME_BASE;
#else
	return 0;
#endif
}

#ifdef WITH_FFMPEG
static void sound_hdaudio_extract_small_block(
	struct hdaudio * hdaudio, 
	short * target_buffer,
	int sample_position /* units of target_rate */,
	int target_rate,
	int target_channels,
	int nb_samples /* in target */)
{
	AVPacket packet;
	int frame_position;
	int frame_size = (long long) target_rate 
		* hdaudio->frame_duration / AV_TIME_BASE;
	int in_frame_size = (long long) hdaudio->sample_rate
		* hdaudio->frame_duration / AV_TIME_BASE;
	int rate_conversion = 
		(target_rate != hdaudio->sample_rate) 
		|| (target_channels != hdaudio->channels);
	int sample_ofs = target_channels * (sample_position % frame_size);

	frame_position = sample_position / frame_size; 

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
			if (hdaudio->resample_cache) {
				MEM_freeN(hdaudio->resample_cache);
			}
			

			hdaudio->resample_cache = (short*) MEM_mallocN(
				(long long) 
				hdaudio->target_channels 
				* frame_size * 2
				* sizeof(short), 
				"hdaudio resample cache");
			if (frame_position == hdaudio->frame_position) {
				hdaudio->resample_samples_in = 
					in_frame_size * 7 / 4;
				hdaudio->resample_samples_written
					= audio_resample(
						hdaudio->resampler,
						hdaudio->resample_cache,
						hdaudio->decode_cache_zero,
						in_frame_size * 7 / 4);
			}
		}
	}

	if (frame_position == hdaudio->frame_position + 1
	    && in_frame_size * hdaudio->channels <= hdaudio->decode_pos) {
		int bl_size = in_frame_size * hdaudio->channels;
		int decode_pos = hdaudio->decode_pos;
	       
		hdaudio->frame_position = frame_position;

		memmove(hdaudio->decode_cache,
			hdaudio->decode_cache + bl_size,
			(decode_pos - bl_size) * sizeof(short));
		
		decode_pos -= bl_size;

		if (decode_pos < hdaudio->decode_cache_size) {
			memset(hdaudio->decode_cache + decode_pos, 0,
			       (hdaudio->decode_cache_size - decode_pos) 
			       * sizeof(short));

			while(av_read_frame(
				      hdaudio->pFormatCtx, &packet) >= 0) {
				int data_size;
				int len;
				uint8_t *audio_pkt_data;
				int audio_pkt_size;
				
				if(packet.stream_index 
				   != hdaudio->audioStream) {
					av_free_packet(&packet);
					continue;
				}

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
					if (len <= 0) {
						audio_pkt_size = 0;
						break;
					}
					
					audio_pkt_size -= len;
					audio_pkt_data += len;
					
					if (data_size <= 0) {
						continue;
					}
					
					decode_pos += data_size / sizeof(short);
					if (decode_pos + data_size
					    / sizeof(short)
					    > hdaudio->decode_cache_size) {
						break;
					}
				}
				av_free_packet(&packet);
				
				if (decode_pos + data_size / sizeof(short)
				    > hdaudio->decode_cache_size) {
					break;
				}
			}
		}

		if (rate_conversion) {
			int written = hdaudio->resample_samples_written
				* target_channels;
			int ofs = target_channels * frame_size;
			int recycle = written - ofs;
			int next_in = in_frame_size 
				+ (3.0/4.0 
				   - (double) recycle / (double) 
				   (frame_size * target_channels)
					) * in_frame_size;

			memmove(hdaudio->resample_cache,
				hdaudio->resample_cache + ofs,
			        recycle * sizeof(short));

			hdaudio->resample_samples_written
			       = audio_resample(
				       hdaudio->resampler,
				       hdaudio->resample_cache + recycle,
				       hdaudio->decode_cache_zero
				       + hdaudio->resample_samples_in
				       * hdaudio->channels
				       - bl_size,
				       next_in)
				+ recycle / target_channels;

			hdaudio->resample_samples_in = next_in;
		}

		hdaudio->decode_pos = decode_pos;
	}

	if (frame_position != hdaudio->frame_position) { 
		long decode_pos = 0;
		long long st_time = hdaudio->pFormatCtx
			->streams[hdaudio->audioStream]->start_time;
		double time_base = 
			av_q2d(hdaudio->pFormatCtx
			       ->streams[hdaudio->audioStream]->time_base);
		long long pos = (long long) frame_position * AV_TIME_BASE
			* hdaudio->frame_duration / AV_TIME_BASE;

		long long seek_pos;

		hdaudio->frame_position = frame_position;

		if (st_time == AV_NOPTS_VALUE) {
			st_time = 0;
		}

		pos += st_time * AV_TIME_BASE * time_base;

		/* seek a little bit before the target position,
		   (ffmpeg seek algorithm doesn't seem to work always as
		   specified...)
		*/

		seek_pos = pos - (AV_TIME_BASE
				 * hdaudio->frame_duration 
				  / AV_TIME_BASE / 10);
		if (seek_pos < 0) {
			seek_pos = pos;
		}

		av_seek_frame(hdaudio->pFormatCtx, -1, 
			      seek_pos, 
			      AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD);
		avcodec_flush_buffers(hdaudio->pCodecCtx);

		memset(hdaudio->decode_cache, 0,
		       hdaudio->decode_cache_size * sizeof(short));

		hdaudio->decode_cache_zero = hdaudio->decode_cache;

		while(av_read_frame(hdaudio->pFormatCtx, &packet) >= 0) {
			int data_size;
			int len;
			uint8_t *audio_pkt_data;
			int audio_pkt_size;

			if(packet.stream_index != hdaudio->audioStream) {
				av_free_packet(&packet);
				continue;
			}

			audio_pkt_data = packet.data;
			audio_pkt_size = packet.size;

			if (!hdaudio->decode_cache_zero 
			    && audio_pkt_size > 0) { 
				long long diff;

				if (packet.pts == AV_NOPTS_VALUE) {
					fprintf(stderr,
						"hdaudio: audio "
						"pts=NULL audio "
						"distortion!\n");
					diff = 0;
				} else {
					long long pts = packet.pts;
					long long spts = (long long) (
						pos / time_base / AV_TIME_BASE 
						+ 0.5);
					diff = spts - pts;
					if (diff < 0) {
						fprintf(stderr,
							"hdaudio: "
							"negative seek: "
							"%lld < %lld "
							"(pos=%lld) "
							"audio distortion!!\n",
							spts, pts, pos);
						diff = 0;
					}
				}


				diff *= hdaudio->sample_rate * time_base;
				diff *= hdaudio->channels;

				if (diff > hdaudio->decode_cache_size / 2) {
					fprintf(stderr,
						"hdaudio: audio "
						"diff too large!!\n");
					diff = 0;
				}

				hdaudio->decode_cache_zero
					= hdaudio->decode_cache + diff;
			}

			while (audio_pkt_size > 0) {
				len = avcodec_decode_audio(
					hdaudio->pCodecCtx, 
					hdaudio->decode_cache 
					+ decode_pos, 
					&data_size, 
					audio_pkt_data, 
					audio_pkt_size);
				if (len <= 0) {
					audio_pkt_size = 0;
					break;
				}
				
				audio_pkt_size -= len;
				audio_pkt_data += len;

				if (data_size <= 0) {
					continue;
				}
				
				decode_pos += data_size / sizeof(short);
				if (decode_pos + data_size
				    / sizeof(short)
				    > hdaudio->decode_cache_size) {
					break;
				}
			}
	
			av_free_packet(&packet);

			if (decode_pos + data_size / sizeof(short)
			    > hdaudio->decode_cache_size) {
				break;
			}
		}
		if (rate_conversion) {
			hdaudio->resample_samples_written
				= audio_resample(hdaudio->resampler,
						 hdaudio->resample_cache,
						 hdaudio->decode_cache_zero,
						 in_frame_size * 7 / 4);
			hdaudio->resample_samples_in = 
				in_frame_size * 7 / 4;
		}
		hdaudio->decode_pos = decode_pos;
	}

	memcpy(target_buffer, (rate_conversion 
			       ? hdaudio->resample_cache 
			       : hdaudio->decode_cache_zero) + sample_ofs, 
	       nb_samples * target_channels * sizeof(short));
}
#endif


void sound_hdaudio_extract(struct hdaudio * hdaudio, 
			   short * target_buffer,
			   int sample_position /* units of target_rate */,
			   int target_rate,
			   int target_channels,
			   int nb_samples /* in target */)
{
#ifdef WITH_FFMPEG
	long long max_samples = (long long) target_rate 
		* hdaudio->frame_duration / AV_TIME_BASE / 4;

	while (nb_samples > max_samples) {
		sound_hdaudio_extract_small_block(hdaudio, target_buffer,
						  sample_position,
						  target_rate,
						  target_channels,
						  max_samples);
		target_buffer += max_samples * target_channels;
		sample_position += max_samples;
		nb_samples -= max_samples;
	}
	if (nb_samples > 0) {
		sound_hdaudio_extract_small_block(hdaudio, target_buffer,
						  sample_position,
						  target_rate,
						  target_channels,
						  nb_samples);
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
		if (hdaudio->resample_cache) {
			MEM_freeN(hdaudio->resample_cache);
		}
		free(hdaudio->filename);
		MEM_freeN (hdaudio);
	}
#else

#endif
}

