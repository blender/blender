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

#include "respec/ChannelMapperReader.h"

#include <cmath>
#include <limits>

AUD_NAMESPACE_BEGIN

ChannelMapperReader::ChannelMapperReader(std::shared_ptr<IReader> reader,
												 Channels channels) :
		EffectReader(reader), m_target_channels(channels),
	m_source_channels(CHANNELS_INVALID), m_mapping(nullptr), m_map_size(0), m_mono_angle(0)
{
}

ChannelMapperReader::~ChannelMapperReader()
{
	delete[] m_mapping;
}

Channels ChannelMapperReader::getSourceChannels() const
{
	return m_reader->getSpecs().channels;
}

Channels ChannelMapperReader::getChannels() const
{
	return m_target_channels;
}

void ChannelMapperReader::setChannels(Channels channels)
{
	m_target_channels = channels;
	calculateMapping();
}

float ChannelMapperReader::getMapping(int source, int target)
{
	Channels source_channels = m_reader->getSpecs().channels;
	if(source_channels != m_source_channels)
	{
		m_source_channels = source_channels;
		calculateMapping();
	}

	if(source < 0 || source >= source_channels || target < 0 || target >= m_target_channels)
		return std::numeric_limits<float>::quiet_NaN();

	return m_mapping[target * source_channels + source];
}

void ChannelMapperReader::setMonoAngle(float angle)
{
	if(angle != angle)
		angle = 0;
	m_mono_angle = angle;
	if(m_source_channels == CHANNELS_MONO)
		calculateMapping();
}

float ChannelMapperReader::angleDistance(float alpha, float beta)
{
	alpha = beta - alpha;

	if(alpha > M_PI)
		alpha -= 2 * M_PI;
	if(alpha < -M_PI)
		alpha += 2 * M_PI;

	return alpha;
}

void ChannelMapperReader::calculateMapping()
{
	if(m_map_size < m_source_channels * m_target_channels)
	{
		delete[] m_mapping;
		m_mapping = new float[m_source_channels * m_target_channels];
		m_map_size = m_source_channels * m_target_channels;
	}

	for(int i = 0; i < m_source_channels * m_target_channels; i++)
		m_mapping[i] = 0;

	const Channel* source_channels = CHANNEL_MAPS[m_source_channels - 1];
	const Channel* target_channels = CHANNEL_MAPS[m_target_channels - 1];

	int lfe = -1;

	for(int i = 0; i < m_target_channels; i++)
	{
		if(target_channels[i] == CHANNEL_LFE)
		{
			lfe = i;
			break;
		}
	}

	const float* source_angles = CHANNEL_ANGLES[m_source_channels - 1];
	const float* target_angles = CHANNEL_ANGLES[m_target_channels - 1];

	if(m_source_channels == CHANNELS_MONO)
		source_angles = &m_mono_angle;

	int channel_left, channel_right;
	float angle_left, angle_right, angle;

	for(int i = 0; i < m_source_channels; i++)
	{
		if(source_channels[i] == CHANNEL_LFE)
		{
			if(lfe != -1)
				m_mapping[lfe * m_source_channels + i] = 1;

			continue;
		}

		channel_left = channel_right = -1;
		angle_left = -2 * M_PI;
		angle_right = 2 * M_PI;

		for(int j = 0; j < m_target_channels; j++)
		{
			if(j == lfe)
				continue;
			angle = angleDistance(source_angles[i], target_angles[j]);
			if(angle < 0)
			{
				if(angle > angle_left)
				{
					angle_left = angle;
					channel_left = j;
				}
			}
			else
			{
				if(angle < angle_right)
				{
					angle_right = angle;
					channel_right = j;
				}
			}
		}

		angle = angle_right - angle_left;
		if(channel_right == -1 || angle == 0)
		{
			m_mapping[channel_left * m_source_channels + i] = 1;
		}
		else if(channel_left == -1)
		{
			m_mapping[channel_right * m_source_channels + i] = 1;
		}
		else
		{
			m_mapping[channel_left * m_source_channels + i] = std::cos(M_PI_2 * angle_left / angle);
			m_mapping[channel_right * m_source_channels + i] = std::cos(M_PI_2 * angle_right / angle);
		}
	}
}

Specs ChannelMapperReader::getSpecs() const
{
	Specs specs = m_reader->getSpecs();
	specs.channels = m_target_channels;
	return specs;
}

void ChannelMapperReader::read(int& length, bool& eos, sample_t* buffer)
{
	Channels channels = m_reader->getSpecs().channels;
	if(channels != m_source_channels)
	{
		m_source_channels = channels;
		calculateMapping();
	}

	if(m_source_channels == m_target_channels)
	{
		m_reader->read(length, eos, buffer);
		return;
	}

	m_buffer.assureSize(length * channels * sizeof(sample_t));

	sample_t* in = m_buffer.getBuffer();

	m_reader->read(length, eos, in);

	sample_t sum;

	for(int i = 0; i < length; i++)
	{
		for(int j = 0; j < m_target_channels; j++)
		{
			sum = 0;
			for(int k = 0; k < m_source_channels; k++)
				sum += m_mapping[j * m_source_channels + k] * in[i * m_source_channels + k];
			buffer[i * m_target_channels + j] = sum;
		}
	}
}

const Channel ChannelMapperReader::MONO_MAP[] =
{
	CHANNEL_FRONT_CENTER
};

const Channel ChannelMapperReader::STEREO_MAP[] =
{
	CHANNEL_FRONT_LEFT,
	CHANNEL_FRONT_RIGHT
};

const Channel ChannelMapperReader::STEREO_LFE_MAP[] =
{
	CHANNEL_FRONT_LEFT,
	CHANNEL_FRONT_RIGHT,
	CHANNEL_LFE
};

const Channel ChannelMapperReader::SURROUND4_MAP[] =
{
	CHANNEL_FRONT_LEFT,
	CHANNEL_FRONT_RIGHT,
	CHANNEL_REAR_LEFT,
	CHANNEL_REAR_RIGHT
};

const Channel ChannelMapperReader::SURROUND5_MAP[] =
{
	CHANNEL_FRONT_LEFT,
	CHANNEL_FRONT_RIGHT,
	CHANNEL_FRONT_CENTER,
	CHANNEL_REAR_LEFT,
	CHANNEL_REAR_RIGHT
};

const Channel ChannelMapperReader::SURROUND51_MAP[] =
{
	CHANNEL_FRONT_LEFT,
	CHANNEL_FRONT_RIGHT,
	CHANNEL_FRONT_CENTER,
	CHANNEL_LFE,
	CHANNEL_REAR_LEFT,
	CHANNEL_REAR_RIGHT
};

const Channel ChannelMapperReader::SURROUND61_MAP[] =
{
	CHANNEL_FRONT_LEFT,
	CHANNEL_FRONT_RIGHT,
	CHANNEL_FRONT_CENTER,
	CHANNEL_LFE,
	CHANNEL_REAR_CENTER,
	CHANNEL_REAR_LEFT,
	CHANNEL_REAR_RIGHT
};

const Channel ChannelMapperReader::SURROUND71_MAP[] =
{
	CHANNEL_FRONT_LEFT,
	CHANNEL_FRONT_RIGHT,
	CHANNEL_FRONT_CENTER,
	CHANNEL_LFE,
	CHANNEL_REAR_LEFT,
	CHANNEL_REAR_RIGHT,
	CHANNEL_SIDE_LEFT,
	CHANNEL_SIDE_RIGHT
};

const Channel* ChannelMapperReader::CHANNEL_MAPS[] =
{
	ChannelMapperReader::MONO_MAP,
	ChannelMapperReader::STEREO_MAP,
	ChannelMapperReader::STEREO_LFE_MAP,
	ChannelMapperReader::SURROUND4_MAP,
	ChannelMapperReader::SURROUND5_MAP,
	ChannelMapperReader::SURROUND51_MAP,
	ChannelMapperReader::SURROUND61_MAP,
	ChannelMapperReader::SURROUND71_MAP
};

const float ChannelMapperReader::MONO_ANGLES[] =
{
	0.0f * M_PI / 180.0f
};

const float ChannelMapperReader::STEREO_ANGLES[] =
{
	-90.0f * M_PI / 180.0f,
	 90.0f * M_PI / 180.0f
};

const float ChannelMapperReader::STEREO_LFE_ANGLES[] =
{
	-90.0f * M_PI / 180.0f,
	 90.0f * M_PI / 180.0f,
	  0.0f * M_PI / 180.0f
};

const float ChannelMapperReader::SURROUND4_ANGLES[] =
{
	 -45.0f * M_PI / 180.0f,
	  45.0f * M_PI / 180.0f,
	-135.0f * M_PI / 180.0f,
	 135.0f * M_PI / 180.0f
};

const float ChannelMapperReader::SURROUND5_ANGLES[] =
{
	 -30.0f * M_PI / 180.0f,
	  30.0f * M_PI / 180.0f,
	   0.0f * M_PI / 180.0f,
	-110.0f * M_PI / 180.0f,
	 110.0f * M_PI / 180.0f
};

const float ChannelMapperReader::SURROUND51_ANGLES[] =
{
	  -30.0f * M_PI / 180.0f,
	   30.0f * M_PI / 180.0f,
	   0.0f * M_PI / 180.0f,
	   0.0f * M_PI / 180.0f,
	-110.0f * M_PI / 180.0f,
	 110.0f * M_PI / 180.0f
};

const float ChannelMapperReader::SURROUND61_ANGLES[] =
{
	  -30.0f * M_PI / 180.0f,
	   30.0f * M_PI / 180.0f,
	   0.0f * M_PI / 180.0f,
	   0.0f * M_PI / 180.0f,
	 180.0f * M_PI / 180.0f,
	-110.0f * M_PI / 180.0f,
	 110.0f * M_PI / 180.0f
};

const float ChannelMapperReader::SURROUND71_ANGLES[] =
{
	  -30.0f * M_PI / 180.0f,
	   30.0f * M_PI / 180.0f,
	   0.0f * M_PI / 180.0f,
	   0.0f * M_PI / 180.0f,
	-110.0f * M_PI / 180.0f,
	 110.0f * M_PI / 180.0f,
	-150.0f * M_PI / 180.0f,
	 150.0f * M_PI / 180.0f
};

const float* ChannelMapperReader::CHANNEL_ANGLES[] =
{
	ChannelMapperReader::MONO_ANGLES,
	ChannelMapperReader::STEREO_ANGLES,
	ChannelMapperReader::STEREO_LFE_ANGLES,
	ChannelMapperReader::SURROUND4_ANGLES,
	ChannelMapperReader::SURROUND5_ANGLES,
	ChannelMapperReader::SURROUND51_ANGLES,
	ChannelMapperReader::SURROUND61_ANGLES,
	ChannelMapperReader::SURROUND71_ANGLES
};

AUD_NAMESPACE_END
