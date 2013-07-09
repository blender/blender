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

/** \file audaspace/intern/AUD_Space.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SPACE_H__
#define __AUD_SPACE_H__

/// The size of a format in bytes.
#define AUD_FORMAT_SIZE(format) (format & 0x0F)
/// The size of a sample in the specified device format in bytes.
#define AUD_DEVICE_SAMPLE_SIZE(specs) (specs.channels * (specs.format & 0x0F))
/// The size of a sample in the specified format in bytes.
#define AUD_SAMPLE_SIZE(specs) (specs.channels * sizeof(sample_t))
/// Throws a AUD_Exception with the provided error code.
#define AUD_THROW(exception, errorstr) { AUD_Exception e; e.error = exception; e.str = errorstr; throw e; }

/// Compares two audio data specifications.
#define AUD_COMPARE_SPECS(s1, s2) ((s1.rate == s2.rate) && (s1.channels == s2.channels))

/// Returns the bit for a channel mask.
#define AUD_CHANNEL_BIT(channel) (0x01 << channel)

/// Returns the smaller of the two values.
#define AUD_MIN(a, b) (((a) < (b)) ? (a) : (b))
/// Returns the bigger of the two values.
#define AUD_MAX(a, b) (((a) > (b)) ? (a) : (b))

// 5 sec * 44100 samples/sec * 4 bytes/sample * 6 channels
/// The size by which a buffer should be resized if the final extent is unknown.
#define AUD_BUFFER_RESIZE_BYTES 5292000

/// The default playback buffer size of a device.
#define AUD_DEFAULT_BUFFER_SIZE 1024

/**
 * The format of a sample.
 * The last 4 bit save the byte count of the format.
 */
typedef enum
{
	AUD_FORMAT_INVALID = 0x00,		/// Invalid sample format.
	AUD_FORMAT_U8      = 0x01,		/// 1 byte unsigned byte.
	AUD_FORMAT_S16     = 0x12,		/// 2 byte signed integer.
	AUD_FORMAT_S24     = 0x13,		/// 3 byte signed integer.
	AUD_FORMAT_S32     = 0x14,		/// 4 byte signed integer.
	AUD_FORMAT_FLOAT32 = 0x24,		/// 4 byte float.
	AUD_FORMAT_FLOAT64 = 0x28		/// 8 byte float.
} AUD_SampleFormat;

/// The channel count.
typedef enum
{
	AUD_CHANNELS_INVALID    = 0,	/// Invalid channel count.
	AUD_CHANNELS_MONO       = 1,	/// Mono.
	AUD_CHANNELS_STEREO     = 2,	/// Stereo.
	AUD_CHANNELS_STEREO_LFE = 3,	/// Stereo with LFE channel.
	AUD_CHANNELS_SURROUND4  = 4,	/// 4 channel surround sound.
	AUD_CHANNELS_SURROUND5  = 5,	/// 5 channel surround sound.
	AUD_CHANNELS_SURROUND51 = 6,	/// 5.1 surround sound.
	AUD_CHANNELS_SURROUND61 = 7,	/// 6.1 surround sound.
	AUD_CHANNELS_SURROUND71 = 8	/// 7.1 surround sound.
} AUD_Channels;

/// The channel names.
typedef enum
{
	AUD_CHANNEL_FRONT_LEFT = 0,
	AUD_CHANNEL_FRONT_RIGHT,
	AUD_CHANNEL_FRONT_CENTER,
	AUD_CHANNEL_LFE,
	AUD_CHANNEL_REAR_LEFT,
	AUD_CHANNEL_REAR_RIGHT,
	AUD_CHANNEL_REAR_CENTER,
	AUD_CHANNEL_SIDE_LEFT,
	AUD_CHANNEL_SIDE_RIGHT,
	AUD_CHANNEL_MAX
} AUD_Channel;

/**
 * The sample rate tells how many samples are played back within one second.
 * Some exotic formats may use other sample rates than provided here.
 */
typedef enum
{
	AUD_RATE_INVALID = 0,			/// Invalid sample rate.
	AUD_RATE_8000    = 8000,		/// 8000 Hz.
	AUD_RATE_16000   = 16000,		/// 16000 Hz.
	AUD_RATE_11025   = 11025,		/// 11025 Hz.
	AUD_RATE_22050   = 22050,		/// 22050 Hz.
	AUD_RATE_32000   = 32000,		/// 32000 Hz.
	AUD_RATE_44100   = 44100,		/// 44100 Hz.
	AUD_RATE_48000   = 48000,		/// 48000 Hz.
	AUD_RATE_88200   = 88200,		/// 88200 Hz.
	AUD_RATE_96000   = 96000,		/// 96000 Hz.
	AUD_RATE_192000  = 192000		/// 192000 Hz.
} AUD_DefaultSampleRate;

/// Status of a playback handle.
typedef enum
{
	AUD_STATUS_INVALID = 0,			/// Invalid handle. Maybe due to stopping.
	AUD_STATUS_PLAYING,				/// Sound is playing.
	AUD_STATUS_PAUSED				/// Sound is being paused.
} AUD_Status;

/// Error codes for exceptions (C++ library) or for return values (C API).
typedef enum
{
	AUD_NO_ERROR = 0,
	AUD_ERROR_SPECS,
	AUD_ERROR_PROPS,
	AUD_ERROR_FILE,
	AUD_ERROR_SRC,
	AUD_ERROR_FFMPEG,
	AUD_ERROR_OPENAL,
	AUD_ERROR_SDL,
	AUD_ERROR_JACK
} AUD_Error;

/// Fading types.
typedef enum
{
	AUD_FADE_IN,
	AUD_FADE_OUT
} AUD_FadeType;

/// Possible distance models for the 3D device.
typedef enum
{
	AUD_DISTANCE_MODEL_INVALID = 0,
	AUD_DISTANCE_MODEL_INVERSE,
	AUD_DISTANCE_MODEL_INVERSE_CLAMPED,
	AUD_DISTANCE_MODEL_LINEAR,
	AUD_DISTANCE_MODEL_LINEAR_CLAMPED,
	AUD_DISTANCE_MODEL_EXPONENT,
	AUD_DISTANCE_MODEL_EXPONENT_CLAMPED
} AUD_DistanceModel;

/// Possible animatable properties for Sequencer Factories and Entries.
typedef enum
{
	AUD_AP_VOLUME,
	AUD_AP_PANNING,
	AUD_AP_PITCH,
	AUD_AP_LOCATION,
	AUD_AP_ORIENTATION
} AUD_AnimateablePropertyType;

/// Container formats for writers.
typedef enum
{
	AUD_CONTAINER_INVALID = 0,
	AUD_CONTAINER_AC3,
	AUD_CONTAINER_FLAC,
	AUD_CONTAINER_MATROSKA,
	AUD_CONTAINER_MP2,
	AUD_CONTAINER_MP3,
	AUD_CONTAINER_OGG,
	AUD_CONTAINER_WAV
} AUD_Container;

/// Audio codecs for writers.
typedef enum
{
	AUD_CODEC_INVALID = 0,
	AUD_CODEC_AAC,
	AUD_CODEC_AC3,
	AUD_CODEC_FLAC,
	AUD_CODEC_MP2,
	AUD_CODEC_MP3,
	AUD_CODEC_PCM,
	AUD_CODEC_VORBIS
} AUD_Codec;

/// Sample type.(float samples)
typedef float sample_t;

/// Sample data type (format samples)
typedef unsigned char data_t;

/// Sample rate type.
typedef double AUD_SampleRate;

/// Specification of a sound source.
typedef struct
{
	/// Sample rate in Hz.
	AUD_SampleRate rate;

	/// Channel count.
	AUD_Channels channels;
} AUD_Specs;

/// Specification of a sound device.
typedef struct
{
	/// Sample format.
	AUD_SampleFormat format;

	union
	{
		struct
		{
			/// Sample rate in Hz.
			AUD_SampleRate rate;

			/// Channel count.
			AUD_Channels channels;
		};
		AUD_Specs specs;
	};
} AUD_DeviceSpecs;

/// Exception structure.
typedef struct
{
	/**
	 * Error code.
	 * \see AUD_Error
	 */
	AUD_Error error;

	/**
	 * Error string.
	 */
	const char* str;

	// void* userData; - for the case it is needed someday
} AUD_Exception;

#endif //__AUD_SPACE_H__
