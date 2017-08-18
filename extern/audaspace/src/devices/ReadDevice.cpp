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

#include "devices/ReadDevice.h"
#include "IReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

ReadDevice::ReadDevice(DeviceSpecs specs) :
	m_playing(false)
{
	m_specs = specs;

	create();
}

ReadDevice::ReadDevice(Specs specs) :
	m_playing(false)
{
	m_specs.specs = specs;
	m_specs.format = FORMAT_FLOAT32;

	create();
}

ReadDevice::~ReadDevice()
{
	destroy();
}

bool ReadDevice::read(data_t* buffer, int length)
{
	if(m_playing)
		mix(buffer, length);
	else
		if(m_specs.format == FORMAT_U8)
			std::memset(buffer, 0x80, length * AUD_DEVICE_SAMPLE_SIZE(m_specs));
		else
			std::memset(buffer, 0, length * AUD_DEVICE_SAMPLE_SIZE(m_specs));
	return m_playing;
}

void ReadDevice::changeSpecs(Specs specs)
{
	if(!AUD_COMPARE_SPECS(specs, m_specs.specs))
		setSpecs(specs);
}

void ReadDevice::playing(bool playing)
{
	m_playing = playing;
}

AUD_NAMESPACE_END
