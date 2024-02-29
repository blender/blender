/*******************************************************************************
 * Copyright 2009-2021 Jörg Müller
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

#include "CoreAudioDevice.h"
#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"
#include "Exception.h"
#include "IReader.h"

AUD_NAMESPACE_BEGIN

OSStatus CoreAudioDevice::CoreAudio_mix(void* data, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* time_stamp, UInt32 bus_number, UInt32 number_frames, AudioBufferList* buffer_list)
{
	CoreAudioDevice* device = (CoreAudioDevice*)data;

	for(int i = 0; i < buffer_list->mNumberBuffers; i++)
	{
		auto& buffer = buffer_list->mBuffers[i];

		device->mix((data_t*)buffer.mData, buffer.mDataByteSize / AUD_DEVICE_SAMPLE_SIZE(device->m_specs));
	}

	return noErr;
}

void CoreAudioDevice::playing(bool playing)
{
	if(m_playback != playing)
	{
		if(playing)
			AudioOutputUnitStart(m_audio_unit);
		else
			AudioOutputUnitStop(m_audio_unit);
	}

	m_playback = playing;
}

CoreAudioDevice::CoreAudioDevice(DeviceSpecs specs, int buffersize) :
m_playback(false),
m_audio_unit(nullptr)
{
	AudioComponentDescription component_description = {};

	component_description.componentType = kAudioUnitType_Output;
	component_description.componentSubType = kAudioUnitSubType_DefaultOutput;
	component_description.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent component = AudioComponentFindNext(nullptr, &component_description);

	if(!component)
		AUD_THROW(DeviceException, "The audio device couldn't be opened with CoreAudio.");

	OSStatus status = AudioComponentInstanceNew(component, &m_audio_unit);

	if(status != noErr)
		AUD_THROW(DeviceException, "The audio device couldn't be opened with CoreAudio.");

	AudioStreamBasicDescription stream_basic_description = {};

	if(specs.channels == CHANNELS_INVALID)
		specs.channels = CHANNELS_STEREO;
	if(specs.format == FORMAT_INVALID)
		specs.format = FORMAT_FLOAT32;
	if(specs.rate == RATE_INVALID)
		specs.rate = RATE_48000;

	switch(specs.format)
	{
	case FORMAT_U8:
		stream_basic_description.mFormatFlags = 0;
		stream_basic_description.mBitsPerChannel = 8;
		break;
	case FORMAT_S16:
		stream_basic_description.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
		stream_basic_description.mBitsPerChannel = 16;
		break;
	case FORMAT_S24:
		stream_basic_description.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
		stream_basic_description.mBitsPerChannel = 24;
		break;
	case FORMAT_S32:
		stream_basic_description.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
		stream_basic_description.mBitsPerChannel = 32;
		break;
	case FORMAT_FLOAT32:
		stream_basic_description.mFormatFlags = kLinearPCMFormatFlagIsFloat;
		stream_basic_description.mBitsPerChannel = 32;
		break;
	case FORMAT_FLOAT64:
		stream_basic_description.mFormatFlags = kLinearPCMFormatFlagIsFloat;
		stream_basic_description.mBitsPerChannel = 64;
		break;
	default:
		specs.format = FORMAT_FLOAT32;
		stream_basic_description.mFormatFlags = kLinearPCMFormatFlagIsFloat;
		stream_basic_description.mBitsPerChannel = 32;
		break;
	}

	stream_basic_description.mSampleRate = specs.rate;
	stream_basic_description.mFormatID = kAudioFormatLinearPCM;
	stream_basic_description.mFormatFlags |= kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsPacked;
	stream_basic_description.mBytesPerPacket = stream_basic_description.mBytesPerFrame = AUD_DEVICE_SAMPLE_SIZE(specs);
	stream_basic_description.mFramesPerPacket = 1;
	stream_basic_description.mChannelsPerFrame = specs.channels;

	status = AudioUnitSetProperty(m_audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &stream_basic_description, sizeof(stream_basic_description));

	if(status != noErr)
	{
		AudioComponentInstanceDispose(m_audio_unit);
		AUD_THROW(DeviceException, "The audio device couldn't be opened with CoreAudio.");
	}

	m_specs = specs;

	AURenderCallbackStruct render_callback_struct;
	render_callback_struct.inputProc = CoreAudioDevice::CoreAudio_mix;
	render_callback_struct.inputProcRefCon = this;

	status = AudioUnitSetProperty(m_audio_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &render_callback_struct, sizeof(render_callback_struct));

	if(status != noErr)
	{
		AudioComponentInstanceDispose(m_audio_unit);
		AUD_THROW(DeviceException, "The audio device couldn't be opened with CoreAudio.");
	}

	status = AudioUnitInitialize(m_audio_unit);

	if(status != noErr)
	{
		AudioComponentInstanceDispose(m_audio_unit);
		AUD_THROW(DeviceException, "The audio device couldn't be opened with CoreAudio.");
	}

	try
	{
		m_synchronizer = std::unique_ptr<CoreAudioSynchronizer>(new CoreAudioSynchronizer(m_audio_unit));
	}
	catch(Exception&)
	{
		AudioComponentInstanceDispose(m_audio_unit);
		throw;
	}

	create();
}

CoreAudioDevice::~CoreAudioDevice()
{
	AudioOutputUnitStop(m_audio_unit);
	AudioUnitUninitialize(m_audio_unit);
	AudioComponentInstanceDispose(m_audio_unit);

	destroy();
}

ISynchronizer* CoreAudioDevice::getSynchronizer()
{
	return m_synchronizer.get();
}

class CoreAudioDeviceFactory : public IDeviceFactory
{
private:
	DeviceSpecs m_specs;
	int m_buffersize;

public:
	CoreAudioDeviceFactory() :
	m_buffersize(AUD_DEFAULT_BUFFER_SIZE)
	{
		m_specs.format = FORMAT_FLOAT32;
		m_specs.channels = CHANNELS_STEREO;
		m_specs.rate = RATE_48000;
	}

	virtual std::shared_ptr<IDevice> openDevice()
	{
		return std::shared_ptr<IDevice>(new CoreAudioDevice(m_specs, m_buffersize));
	}

	virtual int getPriority()
	{
		return 1 << 15;
	}

	virtual void setSpecs(DeviceSpecs specs)
	{
		m_specs = specs;
	}

	virtual void setBufferSize(int buffersize)
	{
		m_buffersize = buffersize;
	}

	virtual void setName(const std::string &name)
	{
	}
};

void CoreAudioDevice::registerPlugin()
{
	DeviceManager::registerDevice("CoreAudio", std::shared_ptr<IDeviceFactory>(new CoreAudioDeviceFactory));
}

#ifdef COREAUDIO_PLUGIN
extern "C" AUD_PLUGIN_API void registerPlugin()
{
	CoreAudioDevice::registerPlugin();
}

extern "C" AUD_PLUGIN_API const char* getName()
{
	return "CoreAudio";
}
#endif

AUD_NAMESPACE_END
