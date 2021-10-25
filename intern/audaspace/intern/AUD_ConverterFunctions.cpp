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

/** \file audaspace/intern/AUD_ConverterFunctions.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_ConverterFunctions.h"
#include "AUD_Buffer.h"

#define AUD_U8_0		0x80
#define AUD_S16_MAX		((int16_t)0x7FFF)
#define AUD_S16_MIN		((int16_t)0x8000)
#define AUD_S16_FLT		32767.0f
#define AUD_S32_MAX		((int32_t)0x7FFFFFFF)
#define AUD_S32_MIN		((int32_t)0x80000000)
#define AUD_S32_FLT		2147483647.0f
#define AUD_FLT_MAX		1.0f
#define AUD_FLT_MIN		-1.0f

void AUD_convert_u8_s16(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = (((int16_t)source[i]) - AUD_U8_0) << 8;
}

void AUD_convert_u8_s24_be(data_t* target, data_t* source, int length)
{
	for(int i = length - 1; i >= 0; i--)
	{
		target[i*3] = source[i] - AUD_U8_0;
		target[i*3+1] = 0;
		target[i*3+2] = 0;
	}
}

void AUD_convert_u8_s24_le(data_t* target, data_t* source, int length)
{
	for(int i = length - 1; i >= 0; i--)
	{
		target[i*3+2] = source[i] - AUD_U8_0;
		target[i*3+1] = 0;
		target[i*3] = 0;
	}
}

void AUD_convert_u8_s32(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = (((int32_t)source[i]) - AUD_U8_0) << 24;
}

void AUD_convert_u8_float(data_t* target, data_t* source, int length)
{
	float* t = (float*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = (((int32_t)source[i]) - AUD_U8_0) / ((float)AUD_U8_0);
}

void AUD_convert_u8_double(data_t* target, data_t* source, int length)
{
	double* t = (double*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = (((int32_t)source[i]) - AUD_U8_0) / ((double)AUD_U8_0);
}

void AUD_convert_s16_u8(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	for(int i = 0; i < length; i++)
		target[i] = (unsigned char)((s[i] >> 8) + AUD_U8_0);
}

void AUD_convert_s16_s24_be(data_t* target, data_t* source, int length)
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

void AUD_convert_s16_s24_le(data_t* target, data_t* source, int length)
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

void AUD_convert_s16_s32(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	int32_t* t = (int32_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = ((int32_t)s[i]) << 16;
}

void AUD_convert_s16_float(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	float* t = (float*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = s[i] / AUD_S16_FLT;
}

void AUD_convert_s16_double(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	double* t = (double*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = s[i] / AUD_S16_FLT;
}

void AUD_convert_s24_u8_be(data_t* target, data_t* source, int length)
{
	for(int i = 0; i < length; i++)
		target[i] = source[i*3] ^ AUD_U8_0;
}

void AUD_convert_s24_u8_le(data_t* target, data_t* source, int length)
{
	for(int i = 0; i < length; i++)
		target[i] = source[i*3+2] ^ AUD_U8_0;
}

void AUD_convert_s24_s16_be(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	for(int i = 0; i < length; i++)
		t[i] = source[i*3] << 8 | source[i*3+1];
}

void AUD_convert_s24_s16_le(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	for(int i = 0; i < length; i++)
		t[i] = source[i*3+2] << 8 | source[i*3+1];
}

void AUD_convert_s24_s24(data_t* target, data_t* source, int length)
{
	memcpy(target, source, length * 3);
}

void AUD_convert_s24_s32_be(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = source[i*3] << 24 | source[i*3+1] << 16 | source[i*3+2] << 8;
}

void AUD_convert_s24_s32_le(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = source[i*3+2] << 24 | source[i*3+1] << 16 | source[i*3] << 8;
}

void AUD_convert_s24_float_be(data_t* target, data_t* source, int length)
{
	float* t = (float*) target;
	int32_t s;
	for(int i = length - 1; i >= 0; i--)
	{
		s = source[i*3] << 24 | source[i*3+1] << 16 | source[i*3+2] << 8;
		t[i] = s / AUD_S32_FLT;
	}
}

void AUD_convert_s24_float_le(data_t* target, data_t* source, int length)
{
	float* t = (float*) target;
	int32_t s;
	for(int i = length - 1; i >= 0; i--)
	{
		s = source[i*3+2] << 24 | source[i*3+1] << 16 | source[i*3] << 8;
		t[i] = s / AUD_S32_FLT;
	}
}

void AUD_convert_s24_double_be(data_t* target, data_t* source, int length)
{
	double* t = (double*) target;
	int32_t s;
	for(int i = length - 1; i >= 0; i--)
	{
		s = source[i*3] << 24 | source[i*3+1] << 16 | source[i*3+2] << 8;
		t[i] = s / AUD_S32_FLT;
	}
}

void AUD_convert_s24_double_le(data_t* target, data_t* source, int length)
{
	double* t = (double*) target;
	int32_t s;
	for(int i = length - 1; i >= 0; i--)
	{
		s = source[i*3+2] << 24 | source[i*3+1] << 16 | source[i*3] << 8;
		t[i] = s / AUD_S32_FLT;
	}
}

void AUD_convert_s32_u8(data_t* target, data_t* source, int length)
{
	int16_t* s = (int16_t*) source;
	for(int i = 0; i < length; i++)
		target[i] = (unsigned char)((s[i] >> 24) + AUD_U8_0);
}

void AUD_convert_s32_s16(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	int32_t* s = (int32_t*) source;
	for(int i = 0; i < length; i++)
		t[i] = s[i] >> 16;
}

void AUD_convert_s32_s24_be(data_t* target, data_t* source, int length)
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

void AUD_convert_s32_s24_le(data_t* target, data_t* source, int length)
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

void AUD_convert_s32_float(data_t* target, data_t* source, int length)
{
	int32_t* s = (int32_t*) source;
	float* t = (float*) target;
	for(int i = 0; i < length; i++)
		t[i] = s[i] / AUD_S32_FLT;
}

void AUD_convert_s32_double(data_t* target, data_t* source, int length)
{
	int32_t* s = (int32_t*) source;
	double* t = (double*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = s[i] / AUD_S32_FLT;
}

void AUD_convert_float_u8(data_t* target, data_t* source, int length)
{
	float* s = (float*) source;
	float t;
	for(int i = 0; i < length; i++)
	{
		t = s[i] + AUD_FLT_MAX;
		if(t <= 0.0f)
			target[i] = 0;
		else if(t >= 2.0f)
			target[i] = 255;
		else
			target[i] = (unsigned char)(t*127);
	}
}

void AUD_convert_float_s16(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	float* s = (float*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= AUD_FLT_MIN)
			t[i] = AUD_S16_MIN;
		else if(s[i] >= AUD_FLT_MAX)
			t[i] = AUD_S16_MAX;
		else
			t[i] = (int16_t)(s[i] * AUD_S16_MAX);
	}
}

void AUD_convert_float_s24_be(data_t* target, data_t* source, int length)
{
	int32_t t;
	float* s = (float*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= AUD_FLT_MIN)
			t = AUD_S32_MIN;
		else if(s[i] >= AUD_FLT_MAX)
			t = AUD_S32_MAX;
		else
			t = (int32_t)(s[i]*AUD_S32_MAX);
		target[i*3] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3+2] = t >> 8 & 0xFF;
	}
}

void AUD_convert_float_s24_le(data_t* target, data_t* source, int length)
{
	int32_t t;
	float* s = (float*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= AUD_FLT_MIN)
			t = AUD_S32_MIN;
		else if(s[i] >= AUD_FLT_MAX)
			t = AUD_S32_MAX;
		else
			t = (int32_t)(s[i]*AUD_S32_MAX);
		target[i*3+2] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3] = t >> 8 & 0xFF;
	}
}

void AUD_convert_float_s32(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	float* s = (float*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= AUD_FLT_MIN)
			t[i] = AUD_S32_MIN;
		else if(s[i] >= AUD_FLT_MAX)
			t[i] = AUD_S32_MAX;
		else
			t[i] = (int32_t)(s[i]*AUD_S32_MAX);
	}
}

void AUD_convert_float_double(data_t* target, data_t* source, int length)
{
	float* s = (float*) source;
	double* t = (double*) target;
	for(int i = length - 1; i >= 0; i--)
		t[i] = s[i];
}

void AUD_convert_double_u8(data_t* target, data_t* source, int length)
{
	double* s = (double*) source;
	double t;
	for(int i = 0; i < length; i++)
	{
		t = s[i] + AUD_FLT_MAX;
		if(t <= 0.0)
			target[i] = 0;
		else if(t >= 2.0)
			target[i] = 255;
		else
			target[i] = (unsigned char)(t*127);
	}
}

void AUD_convert_double_s16(data_t* target, data_t* source, int length)
{
	int16_t* t = (int16_t*) target;
	double* s = (double*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= AUD_FLT_MIN)
			t[i] = AUD_S16_MIN;
		else if(s[i] >= AUD_FLT_MAX)
			t[i] = AUD_S16_MAX;
		else
			t[i] = (int16_t)(s[i]*AUD_S16_MAX);
	}
}

void AUD_convert_double_s24_be(data_t* target, data_t* source, int length)
{
	int32_t t;
	double* s = (double*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= AUD_FLT_MIN)
			t = AUD_S32_MIN;
		else if(s[i] >= AUD_FLT_MAX)
			t = AUD_S32_MAX;
		else
			t = (int32_t)(s[i]*AUD_S32_MAX);
		target[i*3] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3+2] = t >> 8 & 0xFF;
	}
}

void AUD_convert_double_s24_le(data_t* target, data_t* source, int length)
{
	int32_t t;
	double* s = (double*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= AUD_FLT_MIN)
			t = AUD_S32_MIN;
		else if(s[i] >= AUD_FLT_MAX)
			t = AUD_S32_MAX;
		else
			t = (int32_t)(s[i]*AUD_S32_MAX);
		target[i*3+2] = t >> 24 & 0xFF;
		target[i*3+1] = t >> 16 & 0xFF;
		target[i*3] = t >> 8 & 0xFF;
	}
}

void AUD_convert_double_s32(data_t* target, data_t* source, int length)
{
	int32_t* t = (int32_t*) target;
	double* s = (double*) source;
	for(int i = 0; i < length; i++)
	{
		if(s[i] <= AUD_FLT_MIN)
			t[i] = AUD_S32_MIN;
		else if(s[i] >= AUD_FLT_MAX)
			t[i] = AUD_S32_MAX;
		else
			t[i] = (int32_t)(s[i]*AUD_S32_MAX);
	}
}

void AUD_convert_double_float(data_t* target, data_t* source, int length)
{
	double* s = (double*) source;
	float* t = (float*) target;
	for(int i = 0; i < length; i++)
		t[i] = s[i];
}
