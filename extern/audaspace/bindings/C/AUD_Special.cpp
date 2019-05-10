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

#include "Exception.h"
#include "IReader.h"
#include "file/File.h"
#include "respec/ChannelMapper.h"
#include "fx/Lowpass.h"
#include "fx/Highpass.h"
#include "fx/Envelope.h"
#include "respec/LinearResample.h"
#include "fx/Threshold.h"
#include "fx/Accumulator.h"
#include "fx/Sum.h"
#include "generator/Silence.h"
#include "fx/Limiter.h"
#include "devices/DeviceManager.h"
#include "sequence/Sequence.h"
#include "file/FileWriter.h"
#include "devices/ReadDevice.h"
#include "plugin/PluginManager.h"
#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"
#include "devices/NULLDevice.h"

#include <cassert>
#include <cstring>
#include <cmath>
#include <sstream>

using namespace aud;

#define AUD_CAPI_IMPLEMENTATION
#include "AUD_Special.h"

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

static inline AUD_DeviceSpecs convDSpecToC(aud::DeviceSpecs specs)
{
	AUD_DeviceSpecs s;
	s.specs = convSpecToC(specs.specs);
	s.format = static_cast<AUD_SampleFormat>(specs.format);
	return s;
}

static inline aud::DeviceSpecs convCToDSpec(AUD_DeviceSpecs specs)
{
	aud::DeviceSpecs s;
	s.specs = convCToSpec(specs.specs);
	s.format = static_cast<SampleFormat>(specs.format);
	return s;
}

AUD_API AUD_SoundInfo AUD_getInfo(AUD_Sound* sound)
{
	assert(sound);

	AUD_SoundInfo info;
	info.specs.channels = AUD_CHANNELS_INVALID;
	info.specs.rate = AUD_RATE_INVALID;
	info.length = 0.0f;

	try
	{
		std::shared_ptr<IReader> reader = (*sound)->createReader();

		if(reader.get())
		{
			info.specs = convSpecToC(reader->getSpecs());
			info.length = reader->getLength() / (float) info.specs.rate;
		}
	}
	catch(Exception&)
	{
	}

	return info;
}

AUD_API float* AUD_readSoundBuffer(const char* filename, float low, float high,
						   float attack, float release, float threshold,
						   int accumulate, int additive, int square,
						   float sthreshold, double samplerate, int* length)
{
	Buffer buffer;
	DeviceSpecs specs;
	specs.channels = CHANNELS_MONO;
	specs.rate = (SampleRate)samplerate;
	std::shared_ptr<ISound> sound;

	std::shared_ptr<ISound> file = std::shared_ptr<ISound>(new File(filename));

	int position = 0;

	try
	{
		std::shared_ptr<IReader> reader = file->createReader();

		SampleRate rate = reader->getSpecs().rate;

		sound = std::shared_ptr<ISound>(new ChannelMapper(file, specs));

		if(high < rate)
			sound = std::shared_ptr<ISound>(new Lowpass(sound, high));
		if(low > 0)
			sound = std::shared_ptr<ISound>(new Highpass(sound, low));

		sound = std::shared_ptr<ISound>(new Envelope(sound, attack, release, threshold, 0.1f));
		sound = std::shared_ptr<ISound>(new LinearResample(sound, specs));

		if(square)
			sound = std::shared_ptr<ISound>(new Threshold(sound, sthreshold));

		if(accumulate)
			sound = std::shared_ptr<ISound>(new Accumulator(sound, additive));
		else if(additive)
			sound = std::shared_ptr<ISound>(new Sum(sound));

		reader = sound->createReader();

		if(!reader.get())
			return nullptr;

		int len;
		bool eos;
		do
		{
			len = samplerate;
			buffer.resize((position + len) * sizeof(float), true);
			reader->read(len, eos, buffer.getBuffer() + position);
			position += len;
		} while(!eos);
	}
	catch(Exception&)
	{
		return nullptr;
	}

	float * result = (float *)malloc(position * sizeof(float));
	std::memcpy(result, buffer.getBuffer(), position * sizeof(float));
	*length = position;
	return result;
}

static void pauseSound(AUD_Handle* handle)
{
	assert(handle);
	(*handle)->pause();
}

AUD_API AUD_Handle* AUD_pauseAfter(AUD_Handle* handle, float seconds)
{
	auto device = DeviceManager::getDevice();

	std::shared_ptr<ISound> silence = std::shared_ptr<ISound>(new Silence(device->getSpecs().rate));
	std::shared_ptr<ISound> limiter = std::shared_ptr<ISound>(new Limiter(silence, 0, seconds));

	std::lock_guard<ILockable> lock(*device);

	try
	{
		AUD_Handle handle2 = device->play(limiter);
		if(handle2.get())
		{
			handle2->setStopCallback((stopCallback)pauseSound, handle);
			return new AUD_Handle(handle2);
		}
	}
	catch(Exception&)
	{
	}

	return nullptr;
}

AUD_API int AUD_readSound(AUD_Sound* sound, float* buffer, int length, int samples_per_second, short* interrupt)
{
	DeviceSpecs specs;
	float* buf;
	Buffer aBuffer;

	specs.rate = RATE_INVALID;
	specs.channels = CHANNELS_MONO;
	specs.format = FORMAT_INVALID;

	std::shared_ptr<IReader> reader = ChannelMapper(*sound, specs).createReader();

	specs.specs = reader->getSpecs();
	int len;
	float samplejump = specs.rate / samples_per_second;
	float min, max, power, overallmax;
	bool eos;

	overallmax = 0;

	for(int i = 0; i < length; i++)
	{
		len = floor(samplejump * (i+1)) - floor(samplejump * i);

		if(*interrupt)
			return 0;

		aBuffer.assureSize(len * AUD_SAMPLE_SIZE(specs));
		buf = aBuffer.getBuffer();

		reader->read(len, eos, buf);

		max = min = *buf;
		power = *buf * *buf;
		for(int j = 1; j < len; j++)
		{
			if(buf[j] < min)
				min = buf[j];
			if(buf[j] > max)
				max = buf[j];
			power += buf[j] * buf[j];
		}

		buffer[i * 3] = min;
		buffer[i * 3 + 1] = max;
		buffer[i * 3 + 2] = sqrt(power) / len;

		if(overallmax < max)
			overallmax = max;
		if(overallmax < -min)
			overallmax = -min;

		if(eos)
		{
			length = i;
			break;
		}
	}

	if(overallmax > 1.0f)
	{
		for(int i = 0; i < length * 3; i++)
		{
			buffer[i] /= overallmax;
		}
	}

	return length;
}

AUD_API const char* AUD_mixdown(AUD_Sound* sound, unsigned int start, unsigned int length, unsigned int buffersize, const char* filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate)
{
	try
	{
		Sequence* f = dynamic_cast<Sequence *>(sound->get());

		f->setSpecs(convCToSpec(specs.specs));
		std::shared_ptr<IReader> reader = f->createQualityReader();
		reader->seek(start);
		std::shared_ptr<IWriter> writer = FileWriter::createWriter(filename, convCToDSpec(specs), static_cast<Container>(format), static_cast<Codec>(codec), bitrate);
		FileWriter::writeReader(reader, writer, length, buffersize);

		return nullptr;
	}
	catch(Exception& e)
	{
		return e.getMessage().c_str();
	}
}

AUD_API const char* AUD_mixdown_per_channel(AUD_Sound* sound, unsigned int start, unsigned int length, unsigned int buffersize, const char* filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate)
{
	try
	{
		Sequence* f = dynamic_cast<Sequence *>(sound->get());

		f->setSpecs(convCToSpec(specs.specs));

		std::vector<std::shared_ptr<IWriter> > writers;

		int channels = specs.channels;
		specs.channels = AUD_CHANNELS_MONO;

		for(int i = 0; i < channels; i++)
		{
			std::stringstream stream;
			std::string fn = filename;
			size_t index = fn.find_last_of('.');
			size_t index_slash = fn.find_last_of('/');
			size_t index_backslash = fn.find_last_of('\\');

			if((index == std::string::npos) ||
				((index < index_slash) && (index_slash != std::string::npos)) ||
				((index < index_backslash) && (index_backslash != std::string::npos)))
			{
				stream << filename << "_" << (i + 1);
			}
			else
			{
				stream << fn.substr(0, index) << "_" << (i + 1) << fn.substr(index);
			}
			writers.push_back(FileWriter::createWriter(stream.str(), convCToDSpec(specs), static_cast<Container>(format), static_cast<Codec>(codec), bitrate));
		}

		std::shared_ptr<IReader> reader = f->createQualityReader();
		reader->seek(start);
		FileWriter::writeReader(reader, writers, length, buffersize);

		return nullptr;
	}
	catch(Exception& e)
	{
		return e.getMessage().c_str();
	}
}

AUD_API AUD_Device* AUD_openMixdownDevice(AUD_DeviceSpecs specs, AUD_Sound* sequencer, float volume, float start)
{
	try
	{
		ReadDevice* device = new ReadDevice(convCToDSpec(specs));
		device->setQuality(true);
		device->setVolume(volume);

		Sequence* f = dynamic_cast<Sequence*>(sequencer->get());

		f->setSpecs(convCToSpec(specs.specs));

		AUD_Handle handle = device->play(f->createQualityReader());
		if(handle.get())
		{
			handle->seek(start);
		}

		return new AUD_Device(device);
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API void AUD_initOnce()
{
	PluginManager::loadPlugins();
	NULLDevice::registerPlugin();
}

AUD_API void AUD_exitOnce()
{
}

AUD_API AUD_Device* AUD_init(const char* device, AUD_DeviceSpecs specs, int buffersize, const char* name)
{
	try
	{
		std::shared_ptr<IDeviceFactory> factory = DeviceManager::getDeviceFactory(device);

		if(factory)
		{
			factory->setName(name);
			factory->setBufferSize(buffersize);
			factory->setSpecs(convCToDSpec(specs));
			auto device = factory->openDevice();
			DeviceManager::setDevice(device);

			return new AUD_Device(device);
		}
	}
	catch(Exception&)
	{
	}
	return nullptr;
}

AUD_API void AUD_exit(AUD_Device* device)
{
	delete device;
	DeviceManager::releaseDevice();
}


AUD_API char** AUD_getDeviceNames()
{
	std::vector<std::string> v_names = DeviceManager::getAvailableDeviceNames();
	char** names = (char**) malloc(sizeof(char*) * (v_names.size() + 1));

	for(int i = 0; i < v_names.size(); i++)
	{
		std::string name = v_names[i];
		names[i] = (char*) malloc(sizeof(char) * (name.length() + 1));
		strcpy(names[i], name.c_str());
	}

	names[v_names.size()] = nullptr;

	return names;
}
