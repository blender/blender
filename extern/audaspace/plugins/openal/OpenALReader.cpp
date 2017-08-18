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

#include "OpenALReader.h"
#include "respec/ConverterFunctions.h"
#include "Exception.h"

#include <algorithm>
#include <al.h>

AUD_NAMESPACE_BEGIN

OpenALReader::OpenALReader(Specs specs, int buffersize) :
	m_specs(specs),
	m_position(0),
	m_device(nullptr)
{
	if((specs.channels != CHANNELS_MONO) && (specs.channels != CHANNELS_STEREO))
		specs.channels = CHANNELS_MONO;

	m_device = alcCaptureOpenDevice(nullptr, specs.rate,
									specs.channels == CHANNELS_MONO ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16,
									buffersize * specs.channels * 2);

	if(!m_device)
		AUD_THROW(DeviceException, "The capture device couldn't be opened with OpenAL.");

	alcCaptureStart(m_device);
}

OpenALReader::~OpenALReader()
{
	if(m_device)
	{
		//alcCaptureStop(m_device);
		alcCaptureCloseDevice(m_device);
	}
}

bool OpenALReader::isSeekable() const
{
	return false;
}

void OpenALReader::seek(int position)
{
	m_position = position;
}

int OpenALReader::getLength() const
{
	int length;
	alcGetIntegerv(m_device, ALC_CAPTURE_SAMPLES, 1, &length);
	return length;
}

int OpenALReader::getPosition() const
{
	return m_position;
}

Specs OpenALReader::getSpecs() const
{
	return m_specs;
}

void OpenALReader::read(int & length, bool& eos, sample_t* buffer)
{
	int len = getLength();
	length = std::min(length, len);

	if(length > 0)
	{
		alcCaptureSamples(m_device, buffer, length);
		convert_s16_float((data_t*)buffer, (data_t*)buffer, length * m_specs.channels);
	}

	eos = false;

	m_position += length;
}

AUD_NAMESPACE_END
