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

#ifndef AUD_CONVERTERFUNCTIONS
#define AUD_CONVERTERFUNCTIONS

#include "AUD_Space.h"

#include <cstring>
#ifdef _MSC_VER
#if (_MSC_VER < 1300)
   typedef short             int16_t;
   typedef int               int32_t;
#else
   typedef __int16           int16_t;
   typedef __int32           int32_t;
#endif
#else
#include <stdint.h>
#endif

typedef void (*AUD_convert_f)(sample_t* target, sample_t* source, int length);

typedef void (*AUD_volume_adjust_f)(sample_t* target, sample_t* source,
									int count, float volume);

typedef void (*AUD_rectify_f)(sample_t* target, sample_t* source, int count);

template <class T>
void AUD_convert_copy(sample_t* target, sample_t* source, int length)
{
	memcpy(target, source, length*sizeof(T));
}

void AUD_convert_u8_s16(sample_t* target, sample_t* source, int length);

void AUD_convert_u8_s24_be(sample_t* target, sample_t* source, int length);

void AUD_convert_u8_s24_le(sample_t* target, sample_t* source, int length);

void AUD_convert_u8_s32(sample_t* target, sample_t* source, int length);

void AUD_convert_u8_float(sample_t* target, sample_t* source, int length);

void AUD_convert_u8_double(sample_t* target, sample_t* source, int length);

void AUD_convert_s16_u8(sample_t* target, sample_t* source, int length);

void AUD_convert_s16_s24_be(sample_t* target, sample_t* source, int length);

void AUD_convert_s16_s24_le(sample_t* target, sample_t* source, int length);

void AUD_convert_s16_s32(sample_t* target, sample_t* source, int length);

void AUD_convert_s16_float(sample_t* target, sample_t* source, int length);

void AUD_convert_s16_double(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_u8_be(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_u8_le(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_s16_be(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_s16_le(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_s24(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_s32_be(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_s32_le(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_float_be(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_float_le(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_double_be(sample_t* target, sample_t* source, int length);

void AUD_convert_s24_double_le(sample_t* target, sample_t* source, int length);

void AUD_convert_s32_u8(sample_t* target, sample_t* source, int length);

void AUD_convert_s32_s16(sample_t* target, sample_t* source, int length);

void AUD_convert_s32_s24_be(sample_t* target, sample_t* source, int length);

void AUD_convert_s32_s24_le(sample_t* target, sample_t* source, int length);

void AUD_convert_s32_float(sample_t* target, sample_t* source, int length);

void AUD_convert_s32_double(sample_t* target, sample_t* source, int length);

void AUD_convert_float_u8(sample_t* target, sample_t* source, int length);

void AUD_convert_float_s16(sample_t* target, sample_t* source, int length);

void AUD_convert_float_s24_be(sample_t* target, sample_t* source, int length);

void AUD_convert_float_s24_le(sample_t* target, sample_t* source, int length);

void AUD_convert_float_s32(sample_t* target, sample_t* source, int length);

void AUD_convert_float_double(sample_t* target, sample_t* source, int length);

void AUD_convert_double_u8(sample_t* target, sample_t* source, int length);

void AUD_convert_double_s16(sample_t* target, sample_t* source, int length);

void AUD_convert_double_s24_be(sample_t* target, sample_t* source, int length);

void AUD_convert_double_s24_le(sample_t* target, sample_t* source, int length);

void AUD_convert_double_s32(sample_t* target, sample_t* source, int length);

void AUD_convert_double_float(sample_t* target, sample_t* source, int length);

template <class T>
void AUD_volume_adjust(sample_t* target, sample_t* source,
					   int count, float volume)
{
	T* t = (T*)target;
	T* s = (T*)source;
	for(int i=0; i < count; i++)
		t[i] = (T)(s[i] * volume);
}

void AUD_volume_adjust_u8(sample_t* target, sample_t* source,
						  int count, float volume);

void AUD_volume_adjust_s24_le(sample_t* target, sample_t* source,
							  int count, float volume);

void AUD_volume_adjust_s24_be(sample_t* target, sample_t* source,
							  int count, float volume);

template <class T>
void AUD_rectify(sample_t* target, sample_t* source, int count)
{
	T* t = (T*)target;
	T* s = (T*)source;
	for(int i=0; i < count; i++)
		t[i] = s[i] < 0 ? -s[i] : s[i];
}

void AUD_rectify_u8(sample_t* target, sample_t* source, int count);

void AUD_rectify_s24_le(sample_t* target, sample_t* source, int count);

void AUD_rectify_s24_be(sample_t* target, sample_t* source, int count);

#endif //AUD_CONVERTERFUNCTIONS
