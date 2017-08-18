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

#include "respec/ConverterFunctions.h"

#include <stdint.h>

#define U8_0		0x80
#define S16_MAX		((int16_t)0x7FFF)
#define S16_MIN		((int16_t)0x8000)
#define S16_FLT		32767.0f
#define S32_MAX		((int32_t)0x7FFFFFFF)
#define S32_MIN		((int32_t)0x80000000)
#define S32_FLT		2147483647.0f
#define FLT_MAX		1.0f
#define FLT_MIN		-1.0f

AUD_NAMESPACE_BEGIN

void convert_u8_s16(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = (((int16_t)source[i]) - U8_0) << 8;
}

void convert_u8_s24_be(data_t* target, data_t* source, int length)
{
	for(int i = length - 1; i >= 0; i--)
	{
		target[i*3] = source[i] - U8_0;
		target[i*3+1] = 0;
		target[i*3+2] = 0;
	}
}

void convert_u8_s24_le(data_t* target, data_t* source, int length)
{
	for(int i = length - 1; i >= 0; i--)
	{
		target[i*3+2] = source[i] - U8_0;
		target[i*3+1] = 0;
		target[i*3] = 0;
	}
}

void convert_u8_s32(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = (((int32_t)source[i]) - U8_0) << 24;
}

void convert_u8_float(data_t* target, data_t* source, int length)
{
	float* t = (float*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = (((int32_t)source[i]) - U8_0) / ((float)U8_0);
}

void convert_u8_double(data_t* target, data_t* source, int length)
{
	double* t = (double*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = (((int32_t)source[i]) - U8_0) / ((double)U8_0);
}

void convert_s16_u8(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	for(int i = 0; i < length; i++)
		target[i] = (unsigned char)((s[i] >> 8) + U8_0);
}

void convert_s16_s24_be(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	int16_t t;
	for(int i = length - 1; i >= 0; i--)
	{
		t = s[i];
		target[i*3] = t >> 8 & 0xFF;
		target[i*3+1] = t & 0xFF;
		target[i*3+2] = 0;
	}
}

void convert_s16_s24_le(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	int16_t t;
	for(int i = length - 1; i >= 0; i--)
	{
		t = s[i];
		target[i*3+2] = t >> 8 & 0xFF;
		target[i*3+1] = t & 0xFF;
		target[i*3] = 0;
	}
}

void convert_s16_s32(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	int32_t* t = (int32_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = ((int32_t)s[i]) << 16;
}

void convert_s16_float(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	float* t = (float*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = s[i] / S16_FLT;
}

void convert_s16_double(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	double* t = (double*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = s[i] / S16_FLT;
}

void convert_s24_u8_be(data_t* target, data_t* source, int length)
{
	for(int i = 0; i < length; i++)
		target[i] = source[i*3] ^ U8_0;
}

void convert_s24_u8_le(data_t* target, data_t* source, int length)
{
	for(int i = 0; i < length; i++)
		target[i] = source[i*3+2] ^ U8_0;
}

void convert_s24_s16_be(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	for(int i = 0; i < length; i++)
		t[i] = source[i*3] << 8 | source[i*3+1];
}

void convert_s24_s16_le(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	for(int i = 0; i < length; i++)
		t[i] = source[i*3+2] << 8 | source[i*3+1];
}

void convert_s24_s24(data_t* target, data_t* source, int length)
{
	std::memcpy(target, source, length * 3);
}

void convert_s24_s32_be(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = source[i*3] << 24 | source[i*3+1] << 16 | source[i*3+2] << 8;
}

void convert_s24_s32_le(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = source[i*3+2] << 24 | source[i*3+1] << 16 | source[i*3] << 8;
}

void convert_s24_float_be(data_t* target, data_t* source, int length)
{
	float* t = (float*) target;
	int32_t s;
	for(int i = length - 1; i >= 0; i--)
	{
		s = source[i*3] << 24 | source[i*3+1] << 16 | source[i*3+2] << 8;
		t[i] = s / S32_FLT;
	}
}

void convert_s24_float_le(data_t* target, data_t* source, int length)
{
	float* t = (float*) target;
	int32_t s;
	for(int i = length - 1; i >= 0; i--)
	{
		s = source[i*3+2] << 24 | source[i*3+1] << 16 | source[i*3] << 8;
		t[i] = s / S32_FLT;
	}
}

void convert_s24_double_be(data_t* target, data_t* source, int length)
{
	double* t = (double*) target;
	int32_t s;
	for(int i = length - 1; i >= 0; i--)
	{
		s = source[i*3] << 24 | source[i*3+1] << 16 | source[i*3+2] << 8;
		t[i] = s / S32_FLT;
	}
}

void convert_s24_double_le(data_t* target, data_t* source, int length)
{
	double* t = (double*) target;
	int32_t s;
	for(int i = length - 1; i >= 0; i--)
	{
		s = source[i*3+2] << 24 | source[i*3+1] << 16 | source[i*3] << 8;
		t[i] = s / S32_FLT;
	}
}

void convert_s32_u8(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	for(int i = 0; i < length; i++)
		target[i] = (unsigned char)((s[i] >> 24) + U8_0);
}

void convert_s32_s16(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	int32_t* s = (int32_t*) source;
	for(int i = 0; i < length; i++)
		t[i] = s[i] >> 16;
}

void convert_s32_s24_be(data_t* target, data_t* source, int length)
{
	int32_t* s = (int32_t*) source;
	int32_t t;
	for(int i = 0; i < length; i++)
	{
		t = s[i];
		target[i*3] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3+2] = t >> 8 & 0xFF;
	}
}

void convert_s32_s24_le(data_t* target, data_t* source, int length)
{
	int32_t* s = (int32_t*) source;
	int32_t t;
	for(int i = 0; i < length; i++)
	{
		t = s[i];
		target[i*3+2] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3] = t >> 8 & 0xFF;
	}
}

void convert_s32_float(data_t* target, data_t* source, int length)
{
	int32_t* s = (int32_t*) source;
	float* t = (float*) target;
	for(int i = 0; i < length; i++)
		t[i] = s[i] / S32_FLT;
}

void convert_s32_double(data_t* target, data_t* source, int length)
{
	int32_t* s = (int32_t*) source;
	double* t = (double*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = s[i] / S32_FLT;
}

void convert_float_u8(data_t* target, data_t* source, int length)
{
	float* s = (float*) source;
	float t;
	for(int i = 0; i < length; i++)
	{
		t = s[i] + FLT_MAX;
		if(t <= 0.0f)
			target[i] = 0;
		else if(t >= 2.0f)
			target[i] = 255;
		else
			target[i] = (unsigned char)(t*127);
	}
}

void convert_float_s16(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	float* s = (float*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= FLT_MIN)
			t[i] = S16_MIN;
		else if(s[i] >= FLT_MAX)
			t[i] = S16_MAX;
		else
			t[i] = (int16_t)(s[i] * S16_MAX);
	}
}

void convert_float_s24_be(data_t* target, data_t* source, int length)
{
	int32_t t;
	float* s = (float*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= FLT_MIN)
			t = S32_MIN;
		else if(s[i] >= FLT_MAX)
			t = S32_MAX;
		else
			t = (int32_t)(s[i]*S32_MAX);
		target[i*3] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3+2] = t >> 8 & 0xFF;
	}
}

void convert_float_s24_le(data_t* target, data_t* source, int length)
{
	int32_t t;
	float* s = (float*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= FLT_MIN)
			t = S32_MIN;
		else if(s[i] >= FLT_MAX)
			t = S32_MAX;
		else
			t = (int32_t)(s[i]*S32_MAX);
		target[i*3+2] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3] = t >> 8 & 0xFF;
	}
}

void convert_float_s32(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	float* s = (float*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= FLT_MIN)
			t[i] = S32_MIN;
		else if(s[i] >= FLT_MAX)
			t[i] = S32_MAX;
		else
			t[i] = (int32_t)(s[i]*S32_MAX);
	}
}

void convert_float_double(data_t* target, data_t* source, int length)
{
	float* s = (float*) source;
	double* t = (double*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = s[i];
}

void convert_double_u8(data_t* target, data_t* source, int length)
{
	double* s = (double*) source;
	double t;
	for(int i = 0; i < length; i++)
	{
		t = s[i] + FLT_MAX;
		if(t <= 0.0)
			target[i] = 0;
		else if(t >= 2.0)
			target[i] = 255;
		else
			target[i] = (unsigned char)(t*127);
	}
}

void convert_double_s16(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	double* s = (double*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= FLT_MIN)
			t[i] = S16_MIN;
		else if(s[i] >= FLT_MAX)
			t[i] = S16_MAX;
		else
			t[i] = (int16_t)(s[i]*S16_MAX);
	}
}

void convert_double_s24_be(data_t* target, data_t* source, int length)
{
	int32_t t;
	double* s = (double*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= FLT_MIN)
			t = S32_MIN;
		else if(s[i] >= FLT_MAX)
			t = S32_MAX;
		else
			t = (int32_t)(s[i]*S32_MAX);
		target[i*3] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3+2] = t >> 8 & 0xFF;
	}
}

void convert_double_s24_le(data_t* target, data_t* source, int length)
{
	int32_t t;
	double* s = (double*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= FLT_MIN)
			t = S32_MIN;
		else if(s[i] >= FLT_MAX)
			t = S32_MAX;
		else
			t = (int32_t)(s[i]*S32_MAX);
		target[i*3+2] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3] = t >> 8 & 0xFF;
	}
}

void convert_double_s32(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	double* s = (double*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= FLT_MIN)
			t[i] = S32_MIN;
		else if(s[i] >= FLT_MAX)
			t[i] = S32_MAX;
		else
			t[i] = (int32_t)(s[i]*S32_MAX);
	}
}

void convert_double_float(data_t* target, data_t* source, int length)
{
	double* s = (double*) source;
	float* t = (float*) target;
	for(int i = 0; i < length; i++)
		t[i] = s[i];
}

AUD_NAMESPACE_END
