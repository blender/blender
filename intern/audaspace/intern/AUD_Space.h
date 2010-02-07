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

#ifndef AUD_SPACE
#define AUD_SPACE

/// The size of a format in bytes.
#define AUD_FORMAT_SIZE(format) (format & 0x0F)
/// The size of a sample in the specified device format in bytes.
#define AUD_DEVICE_SAMPLE_SIZE(specs) (specs.channels * (specs.format & 0x0F))
/// The size of a sample in the specified format in bytes.
#define AUD_SAMPLE_SIZE(specs) (specs.channels * sizeof(sample_t))
/// Throws a AUD_Exception with the provided error code.
#define AUD_THROW(exception) { AUD_Exception e; e.error = exception; throw e; }

/// Returns the smaller of the two values.
#define AUD_MIN(a, b) (((a) < (b)) ? (a) : (b))
/// Returns the bigger of the two values.
#define AUD_MAX(a, b) (((a) > (b)) ? (a) : (b))

// 5 sec * 44100 samples/sec * 4 bytes/sample * 6 channels
/// The size by which a buffer should be resized if the final extent is unknown.
#define AUD_BUFFER_RESIZE_BYTES 5292000

/// The default playback buffer size of a device.
#define AUD_DEFAULT_BUFFER_SIZE 1024

// Capability defines

/// This capability checks whether a device is a 3D device. See AUD_I3DDevice.h.
#define AUD_CAPS_3D_DEVICE			0x0001

/**
 * This capability checks whether a device is a software device. See
 * AUD_SoftwareDevice.
 */
#define AUD_CAPS_SOFTWARE_DEVICE	0x0002

/**
 * This capability enables the user to set the overall volume of the device.
 * You can set and get it with the pointer pointing to a float value between
 * 0.0 (muted) and 1.0 (maximum volume).
 */
#define AUD_CAPS_VOLUME				0x0101

/**
 * This capability enables the user to set the volume of a source.
 * You can set and get it with the pointer pointing to a AUD_SourceValue
 * structure defined in AUD_SourceCaps.h.
 */
#define AUD_CAPS_SOURCE_VOLUME		0x1001

/**
 * This capability enables the user to set the pitch of a source.
 * You can set and get it with the pointer pointing to a AUD_SourceValue
 * structure defined in AUD_SourceCaps.h.
 */
#define AUD_CAPS_SOURCE_PITCH		0x1002

/**
 * This capability enables the user to buffer a factory into the device.
 * Setting with the factory as pointer loads the factory into a device internal
 * buffer. Play function calls with the buffered factory as argument result in
 * the internal buffer being played back, so there's no reader created, what
 * also results in not being able to send messages to that handle.
 * A repeated call with the same factory doesn't do anything.
 * A set call with a NULL pointer results in all buffered factories being
 * deleted.
 * \note This is only possible with factories that create readers of the buffer
 *       type.
 */
#define AUD_CAPS_BUFFERED_FACTORY	0x2001

// Used for debugging memory leaks.
//#define AUD_DEBUG_MEMORY

#ifdef AUD_DEBUG_MEMORY
int AUD_References(int count = 0, const char* text = "");
#define AUD_NEW(text) AUD_References(1, text);
#define AUD_DELETE(text) AUD_References(-1, text);
#else
#define AUD_NEW(text)
#define AUD_DELETE(text)
#endif

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
	AUD_CHANNELS_SURROUND71 = 8,	/// 7.1 surround sound.
	AUD_CHANNELS_SURROUND72 = 9		/// 7.2 surround sound.
} AUD_Channels;

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
} AUD_SampleRate;

/**
 * Type of a reader.
 * @see AUD_IReader for details.
 */
typedef enum
{
	AUD_TYPE_INVALID = 0,			/// Invalid reader type.
	AUD_TYPE_BUFFER,				/// Reader reads from a buffer.
	AUD_TYPE_STREAM					/// Reader reads from a stream.
} AUD_ReaderType;

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
	AUD_ERROR_READER,
	AUD_ERROR_FACTORY,
	AUD_ERROR_FILE,
	AUD_ERROR_FFMPEG,
	AUD_ERROR_SDL,
	AUD_ERROR_OPENAL,
	AUD_ERROR_JACK
} AUD_Error;

/// Message codes.
typedef enum
{
	AUD_MSG_INVALID = 0,			/// Invalid message.
	AUD_MSG_LOOP,					/// Loop reader message.
	AUD_MSG_VOLUME					/// Volume reader message.
} AUD_MessageType;

/// Fading types.
typedef enum
{
	AUD_FADE_IN,
	AUD_FADE_OUT
} AUD_FadeType;

/// 3D device settings.
typedef enum
{
	AUD_3DS_NONE,					/// No setting.
	AUD_3DS_SPEED_OF_SOUND,			/// Speed of sound.
	AUD_3DS_DOPPLER_FACTOR,			/// Doppler factor.
	AUD_3DS_DISTANCE_MODEL			/// Distance model.
} AUD_3DSetting;

/// Possible distance models for the 3D device.
#define AUD_DISTANCE_MODEL_NONE					0.0f
#define AUD_DISTANCE_MODEL_INVERSE				1.0f
#define AUD_DISTANCE_MODEL_INVERSE_CLAMPED		2.0f
#define AUD_DISTANCE_MODEL_LINEAR				3.0f
#define AUD_DISTANCE_MODEL_LINEAR_CLAMPED		4.0f
#define AUD_DISTANCE_MODEL_EXPONENT				5.0f
#define AUD_DISTANCE_MODEL_EXPONENT_CLAMPED		6.0f

/// 3D source settings.
typedef enum
{
	AUD_3DSS_NONE,					/// No setting.
	AUD_3DSS_IS_RELATIVE,			/// > 0 tells that the sound source is
									/// relative to the listener
	AUD_3DSS_MIN_GAIN,				/// Minimum gain.
	AUD_3DSS_MAX_GAIN,				/// Maximum gain.
	AUD_3DSS_REFERENCE_DISTANCE,	/// Reference distance.
	AUD_3DSS_MAX_DISTANCE,			/// Maximum distance.
	AUD_3DSS_ROLLOFF_FACTOR,		/// Rolloff factor.
	AUD_3DSS_CONE_INNER_ANGLE,		/// Cone inner angle.
	AUD_3DSS_CONE_OUTER_ANGLE,		/// Cone outer angle.
	AUD_3DSS_CONE_OUTER_GAIN		/// Cone outer gain.
} AUD_3DSourceSetting;

/// Sample type.(float samples)
typedef float sample_t;

/// Sample data type (format samples)
typedef unsigned char data_t;

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

	// void* userData; - for the case it is needed someday
} AUD_Exception;

/// Message structure.
typedef struct
{
	/**
	 * The message type.
	 */
	AUD_MessageType type;

	union
	{
		// loop reader
		struct
		{
			int loopcount;
			float time;
		};

		// volume reader
		float volume;
	};
} AUD_Message;

/// Handle structure, for inherition.
typedef struct
{
	/// x, y and z coordinates of the object.
	float position[3];

	/// x, y and z coordinates telling the velocity and direction of the object.
	float velocity[3];

	/// 3x3 matrix telling the orientation of the object.
	float orientation[9];
} AUD_3DData;

#endif //AUD_SPACE
