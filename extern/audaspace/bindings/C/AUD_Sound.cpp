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

#include "generator/Sawtooth.h"
#include "generator/Sine.h"
#include "generator/Silence.h"
#include "generator/Square.h"
#include "generator/Triangle.h"
#include "file/File.h"
#include "file/FileWriter.h"
#include "util/StreamBuffer.h"
#include "fx/Accumulator.h"
#include "fx/ADSR.h"
#include "fx/Delay.h"
#include "fx/Envelope.h"
#include "fx/Fader.h"
#include "fx/Highpass.h"
#include "fx/IIRFilter.h"
#include "fx/Limiter.h"
#include "fx/Loop.h"
#include "fx/Lowpass.h"
#include "fx/Modulator.h"
#include "fx/Pitch.h"
#include "fx/Reverse.h"
#include "fx/Sum.h"
#include "fx/Threshold.h"
#include "fx/Volume.h"
#include "fx/SoundList.h"
#include "fx/MutableSound.h"
#include "sequence/Double.h"
#include "sequence/Superpose.h"
#include "sequence/PingPong.h"
#include "respec/LinearResample.h"
#include "respec/JOSResample.h"
#include "respec/JOSResampleReader.h"
#include "respec/ChannelMapper.h"
#include "respec/ChannelMapperReader.h"
#include "util/Buffer.h"
#include "Exception.h"

#ifdef WITH_CONVOLUTION
#include "fx/BinauralSound.h"
#include "fx/ConvolverSound.h"
#include "fx/Equalizer.h"
#endif

#ifdef WITH_RUBBERBAND
#include "fx/TimeStretchPitchScale.h"
#include "fx/AnimateableTimeStretchPitchScale.h"
#endif

#include <cassert>
#include <cstring>

using namespace aud;

#define AUD_CAPI_IMPLEMENTATION
#include "AUD_Sound.h"

static inline AUD_Specs convSpecToC(aud::Specs specs)
{
	AUD_Specs s;
	s.channels = static_cast<AUD_Channels>(specs.channels);
	s.rate = static_cast<AUD_SampleRate>(specs.rate);
	return s;
}

static inline aud::Specs convCToSpec(AUD_Specs specs)
{
	aud::Specs s;
	s.channels = static_cast<Channels>(specs.channels);
	s.rate = static_cast<SampleRate>(specs.rate);
	return s;
}

AUD_API AUD_Specs AUD_Sound_getSpecs(AUD_Sound* sound)
{
	assert(sound);

	return convSpecToC((*sound)->createReader()->getSpecs());
}

AUD_API int AUD_Sound_getLength(AUD_Sound* sound)
{
	assert(sound);

	return (*sound)->createReader()->getLength();
}

AUD_API int AUD_Sound_getFileStreams(AUD_Sound* sound, AUD_StreamInfo **stream_infos)
{
	assert(sound);

	std::shared_ptr<File> file = std::dynamic_pointer_cast<File>(*sound);

	if(file)
	{
		try
		{
			auto streams = file->queryStreams();

			size_t size = sizeof(AUD_StreamInfo) * streams.size();

			if(!size)
			{
				*stream_infos = nullptr;
				return 0;
			}

			*stream_infos = reinterpret_cast<AUD_StreamInfo*>(std::malloc(size));
			std::memcpy(*stream_infos, streams.data(), size);

			return streams.size();
		}
		catch(Exception&)
		{
		}
	}

	*stream_infos = nullptr;
	return 0;
}

AUD_API sample_t* AUD_Sound_data(AUD_Sound* sound, int* length, AUD_Specs* specs)
{
	assert(sound);
	assert(length);
	assert(specs);

	auto stream_buffer = std::dynamic_pointer_cast<StreamBuffer>(*sound);
	if(!stream_buffer)
		stream_buffer = std::make_shared<StreamBuffer>(*sound);
	*specs = convSpecToC(stream_buffer->getSpecs());
	auto buffer = stream_buffer->getBuffer();

	*length = buffer->getSize() / AUD_SAMPLE_SIZE((*specs));

	sample_t* data = new sample_t[buffer->getSize()];

	std::memcpy(data, buffer->getBuffer(), buffer->getSize());

	return data;
}

AUD_API void AUD_Sound_freeData(sample_t* data)
{
	delete[] data;
}

AUD_API const char* AUD_Sound_write(AUD_Sound* sound, const char* filename, AUD_SampleRate rate, AUD_Channels channels, AUD_SampleFormat format, AUD_Container container, AUD_Codec codec, int bitrate, int buffersize)
{
	assert(sound);
	assert(filename);

	try
	{
		std::shared_ptr<IReader> reader = (*sound)->createReader();

		DeviceSpecs specs;
		specs.specs = reader->getSpecs();

		if((rate != RATE_INVALID) && (specs.rate != rate))
		{
			specs.rate = rate;
			reader = std::make_shared<JOSResampleReader>(reader, rate);
		}

		if((channels != AUD_CHANNELS_INVALID) && (specs.channels != static_cast<Channels>(channels)))
		{
			specs.channels = static_cast<Channels>(channels);
			reader = std::make_shared<ChannelMapperReader>(reader, specs.channels);
		}

		if(format == AUD_FORMAT_INVALID)
			format = AUD_FORMAT_S16;
		specs.format = static_cast<SampleFormat>(format);

		const char* invalid_container_error = "Container could not be determined from filename.";

		if(container == AUD_CONTAINER_INVALID)
		{
			std::string path = filename;

			if(path.length() < 4)
				return invalid_container_error;

			std::string extension = path.substr(path.length() - 4);

			if(extension == ".ac3")
				container = AUD_CONTAINER_AC3;
			else if(extension == "flac")
				container = AUD_CONTAINER_FLAC;
			else if(extension == ".mkv")
				container = AUD_CONTAINER_MATROSKA;
			else if(extension == ".mp2")
				container = AUD_CONTAINER_MP2;
			else if(extension == ".mp3")
				container = AUD_CONTAINER_MP3;
			else if(extension == ".ogg")
				container = AUD_CONTAINER_OGG;
			else if(extension == ".wav")
				container = AUD_CONTAINER_WAV;
			else if(extension == ".aac")
				container = AUD_CONTAINER_AAC;
			else
				return invalid_container_error;
		}

		if(codec == AUD_CODEC_INVALID)
		{
			switch(container)
			{
			case AUD_CONTAINER_AC3:
				codec = AUD_CODEC_AC3;
				break;
			case AUD_CONTAINER_FLAC:
				codec = AUD_CODEC_FLAC;
				break;
			case AUD_CONTAINER_MATROSKA:
				codec = AUD_CODEC_OPUS;
				break;
			case AUD_CONTAINER_MP2:
				codec = AUD_CODEC_MP2;
				break;
			case AUD_CONTAINER_MP3:
				codec = AUD_CODEC_MP3;
				break;
			case AUD_CONTAINER_OGG:
				codec = AUD_CODEC_VORBIS;
				break;
			case AUD_CONTAINER_WAV:
				codec = AUD_CODEC_PCM;
				break;
			case AUD_CONTAINER_AAC:
				codec = AUD_CODEC_AAC;
				break;
			default:
				return "Unknown container, cannot select default codec.";
			}
		}

		if(buffersize <= 0)
			buffersize = AUD_DEFAULT_BUFFER_SIZE;

		std::shared_ptr<IWriter> writer = FileWriter::createWriter(filename, specs, static_cast<Container>(container), static_cast<Codec>(codec), bitrate);
		FileWriter::writeReader(reader, writer, 0, buffersize);
	}
	catch(Exception&)
	{
		return "An exception occured while writing.";
	}

	return nullptr;
}

AUD_API AUD_Sound* AUD_Sound_buffer(sample_t* data, int length, AUD_Specs specs)
{
	assert(data);

	if(length <= 0 || specs.rate <= 0 || specs.channels <= 0)
	{
		return nullptr;
	}

	int size = length * AUD_SAMPLE_SIZE(specs);

	std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>(size);

	std::memcpy(buffer->getBuffer(), data, size);

	try
	{
		return new AUD_Sound(new StreamBuffer(buffer, convCToSpec(specs)));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_bufferFile(unsigned char* buffer, int size)
{
	assert(buffer);
	return new AUD_Sound(new File(buffer, size));
}

AUD_API AUD_Sound* AUD_Sound_bufferFileStream(unsigned char* buffer, int size, int stream)
{
	assert(buffer);
	return new AUD_Sound(new File(buffer, size, stream));
}

AUD_API AUD_Sound* AUD_Sound_cache(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new StreamBuffer(*sound));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_file(const char* filename)
{
	assert(filename);
	return new AUD_Sound(new File(filename));
}

AUD_API AUD_Sound* AUD_Sound_fileStream(const char* filename, int stream)
{
	assert(filename);
	return new AUD_Sound(new File(filename, stream));
}

AUD_API AUD_Sound* AUD_Sound_sawtooth(float frequency, AUD_SampleRate rate)
{
	return new AUD_Sound(new Sawtooth(frequency, rate));
}

AUD_API AUD_Sound* AUD_Sound_silence(AUD_SampleRate rate)
{
	return new AUD_Sound(new Silence(rate));
}

AUD_API AUD_Sound* AUD_Sound_sine(float frequency, AUD_SampleRate rate)
{
	return new AUD_Sound(new Sine(frequency, rate));
}

AUD_API AUD_Sound* AUD_Sound_square(float frequency, AUD_SampleRate rate)
{
	return new AUD_Sound(new Square(frequency, rate));
}

AUD_API AUD_Sound* AUD_Sound_triangle(float frequency, AUD_SampleRate rate)
{
	return new AUD_Sound(new Triangle(frequency, rate));
}

AUD_API AUD_Sound* AUD_Sound_accumulate(AUD_Sound* sound, int additive)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Accumulator(*sound, additive));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_ADSR(AUD_Sound* sound, float attack, float decay, float sustain, float release)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new ADSR(*sound, attack, decay, sustain, release));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_delay(AUD_Sound* sound, float delay)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Delay(*sound, delay));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_envelope(AUD_Sound* sound, float attack, float release, float threshold, float arthreshold)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Envelope(*sound, attack, release, threshold, arthreshold));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_fadein(AUD_Sound* sound, float start, float length)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Fader(*sound, FADE_IN, start, length));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_fadeout(AUD_Sound* sound, float start, float length)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Fader(*sound, FADE_OUT, start, length));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_filter(AUD_Sound* sound, float* b, int b_length, float* a, int a_length)
{
	assert(sound);

	try
	{
		std::vector<float> a_coeff, b_coeff;

		if(b)
			for(int i = 0; i < b_length; i++)
				b_coeff.push_back(b[i]);

		if(a)
		{
			for(int i = 0; i < a_length; i++)
				a_coeff.push_back(a[i]);

			if(*a == 0.0f)
				a_coeff[0] = 1.0f;
		}

		return new AUD_Sound(new IIRFilter(*sound, b_coeff, a_coeff));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_highpass(AUD_Sound* sound, float frequency, float Q)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Highpass(*sound, frequency, Q));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_limit(AUD_Sound* sound, float start, float end)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Limiter(*sound, start, end));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_loop(AUD_Sound* sound, int count)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Loop(*sound, count));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_lowpass(AUD_Sound* sound, float frequency, float Q)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Lowpass(*sound, frequency, Q));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_modulate(AUD_Sound* first, AUD_Sound* second)
{
	assert(first);
	assert(second);

	try
	{
		return new AUD_Sound(new Modulator(*first, *second));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_pitch(AUD_Sound* sound, float factor)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Pitch(*sound, factor));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_rechannel(AUD_Sound* sound, AUD_Channels channels)
{
	assert(sound);

	try
	{
		DeviceSpecs specs;
		specs.channels = static_cast<Channels>(channels);
		specs.rate = RATE_INVALID;
		specs.format = FORMAT_INVALID;
		return new AUD_Sound(new ChannelMapper(*sound, specs));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_resample(AUD_Sound* sound, AUD_SampleRate rate, AUD_ResampleQuality quality)
{
	assert(sound);

	try
	{
		DeviceSpecs specs;
		specs.channels = CHANNELS_INVALID;
		specs.rate = rate;
		specs.format = FORMAT_INVALID;
		if (quality == AUD_RESAMPLE_QUALITY_FASTEST)
		{
			return new AUD_Sound(new LinearResample(*sound, specs));
		}
		else
		{
			return new AUD_Sound(new JOSResample(*sound, specs, static_cast<ResampleQuality>(quality)));
		}
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_reverse(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Reverse(*sound));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_sum(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Sum(*sound));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_threshold(AUD_Sound* sound, float threshold)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Threshold(*sound, threshold));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_volume(AUD_Sound* sound, float volume)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new Volume(*sound, volume));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_join(AUD_Sound* first, AUD_Sound* second)
{
	assert(first);
	assert(second);

	try
	{
		return new AUD_Sound(new Double(*first, *second));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_mix(AUD_Sound* first, AUD_Sound* second)
{
	assert(first);
	assert(second);

	try
	{
		return new AUD_Sound(new Superpose(*first, *second));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_pingpong(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new PingPong(*sound));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API void AUD_Sound_free(AUD_Sound* sound)
{
	assert(sound);
	delete sound;
}

AUD_API AUD_Sound* AUD_Sound_copy(AUD_Sound* sound)
{
	return new std::shared_ptr<ISound>(*sound);
}

AUD_API AUD_Sound* AUD_Sound_list(int random)
{
	try
	{
		return new AUD_Sound(new SoundList(random));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API int AUD_SoundList_addSound(AUD_Sound* list, AUD_Sound* sound)
{
	assert(sound);
	assert(list);

	std::shared_ptr<SoundList> s = std::dynamic_pointer_cast<SoundList>(*list);
	if(s.get())
	{
		s->addSound(*sound);
		return 1;
	}
	else
		return 0;

}

AUD_API AUD_Sound* AUD_Sound_mutable(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new MutableSound(*sound));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

#ifdef WITH_CONVOLUTION

AUD_API AUD_Sound* AUD_Sound_Convolver(AUD_Sound* sound, AUD_ImpulseResponse* filter, AUD_ThreadPool* threadPool)
{
	assert(sound);
	assert(filter);
	assert(threadPool);

	try
	{
		return new AUD_Sound(new ConvolverSound(*sound, *filter, *threadPool));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_Binaural(AUD_Sound* sound, AUD_HRTF* hrtfs, AUD_Source* source, AUD_ThreadPool* threadPool)
{
	assert(sound);
	assert(hrtfs);
	assert(source);
	assert(threadPool);

	try
	{
		return new AUD_Sound(new BinauralSound(*sound, *hrtfs, *source, *threadPool));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_equalize(AUD_Sound* sound, float *definition, int size, float maxFreqEq, int sizeConversion)
{
	assert(sound);

	std::shared_ptr<Buffer> buf = std::shared_ptr<Buffer>(new Buffer(sizeof(float)*size));
	std::memcpy(buf->getBuffer(), definition, sizeof(float)*size);
	AUD_Sound *equalizer=new AUD_Sound(new Equalizer(*sound, buf, size, maxFreqEq, sizeConversion));
	return equalizer;
}

#endif

#ifdef WITH_RUBBERBAND
AUD_API AUD_Sound* AUD_Sound_timeStretchPitchScale(AUD_Sound* sound, double timeRatio, double pitchScale, AUD_StretcherQuality quality, char preserveFormant)
{
	assert(sound);
	try
	{
		return new AUD_Sound(new TimeStretchPitchScale(*sound, timeRatio, pitchScale, static_cast<StretcherQuality>(quality), preserveFormant));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API AUD_Sound* AUD_Sound_animateableTimeStretchPitchScale(AUD_Sound* sound, float fps, double timeRatio, double pitchScale, AUD_StretcherQuality quality, char preserveFormant)
{
	assert(sound);
	try
	{
		return new AUD_Sound(new AnimateableTimeStretchPitchScale(*sound, fps, timeRatio, pitchScale, static_cast<StretcherQuality>(quality), preserveFormant));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API void AUD_Sound_animateableTimeStretchPitchScale_setConstantRangeAnimationData(AUD_Sound* sound, AUD_AnimateablePropertyType type, int frame_start, int frame_end,
                                                                                      float* data)
{
	std::shared_ptr<AnimateableProperty> prop = std::dynamic_pointer_cast<AnimateableTimeStretchPitchScale>(*sound)->getAnimProperty(static_cast<AnimateablePropertyType>(type));
	prop->writeConstantRange(data, frame_start, frame_end);
}

AUD_API void AUD_Sound_animateableTimeStretchPitchScale_setAnimationData(AUD_Sound* sound, AUD_AnimateablePropertyType type, int frame, float* data, char animated)
{
	std::shared_ptr<AnimateableProperty> prop = std::dynamic_pointer_cast<AnimateableTimeStretchPitchScale>(*sound)->getAnimProperty(static_cast<AnimateablePropertyType>(type));
	if(animated)
	{
		if(frame >= 0)
			prop->write(data, frame, 1);
	}
	else
	{
		prop->write(data);
	}
}

AUD_API float AUD_Sound_animateableTimeStretchPitchScale_getFPS(AUD_Sound* sound)
{
	assert(sound);
	return dynamic_cast<AnimateableTimeStretchPitchScale*>(sound->get())->getFPS();
}

AUD_API void AUD_Sound_animateableTimeStretchPitchScale_setFPS(AUD_Sound* sound, float value)
{
	assert(sound);
	dynamic_cast<AnimateableTimeStretchPitchScale*>(sound->get())->setFPS(value);
}

AUD_API bool AUD_Sound_isAnimateableTimeStretchPitchScale(AUD_Sound* sound)
{
	assert(sound);
	return dynamic_cast<AnimateableTimeStretchPitchScale*>(sound->get()) != nullptr;
}

#endif
