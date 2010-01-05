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

typedef void (*AUD_convert_f)(data_t* target, data_t* source, int length);

template <class T>
void AUD_convert_copy(data_t* target, data_t* source, int length)
{
	memcpy(target, source, length*sizeof(T));
}

void AUD_convert_u8_s16(data_t* target, data_t* source, int length);

void AUD_convert_u8_s24_be(data_t* target, data_t* source, int length);

void AUD_convert_u8_s24_le(data_t* target, data_t* source, int length);

void AUD_convert_u8_s32(data_t* target, data_t* source, int length);

void AUD_convert_u8_float(data_t* target, data_t* source, int length);

void AUD_convert_u8_double(data_t* target, data_t* source, int length);

void AUD_convert_s16_u8(data_t* target, data_t* source, int length);

void AUD_convert_s16_s24_be(data_t* target, data_t* source, int length);

void AUD_convert_s16_s24_le(data_t* target, data_t* source, int length);

void AUD_convert_s16_s32(data_t* target, data_t* source, int length);

void AUD_convert_s16_float(data_t* target, data_t* source, int length);

void AUD_convert_s16_double(data_t* target, data_t* source, int length);

void AUD_convert_s24_u8_be(data_t* target, data_t* source, int length);

void AUD_convert_s24_u8_le(data_t* target, data_t* source, int length);

void AUD_convert_s24_s16_be(data_t* target, data_t* source, int length);

void AUD_convert_s24_s16_le(data_t* target, data_t* source, int length);

void AUD_convert_s24_s24(data_t* target, data_t* source, int length);

void AUD_convert_s24_s32_be(data_t* target, data_t* source, int length);

void AUD_convert_s24_s32_le(data_t* target, data_t* source, int length);

void AUD_convert_s24_float_be(data_t* target, data_t* source, int length);

void AUD_convert_s24_float_le(data_t* target, data_t* source, int length);

void AUD_convert_s24_double_be(data_t* target, data_t* source, int length);

void AUD_convert_s24_double_le(data_t* target, data_t* source, int length);

void AUD_convert_s32_u8(data_t* target, data_t* source, int length);

void AUD_convert_s32_s16(data_t* target, data_t* source, int length);

void AUD_convert_s32_s24_be(data_t* target, data_t* source, int length);

void AUD_convert_s32_s24_le(data_t* target, data_t* source, int length);

void AUD_convert_s32_float(data_t* target, data_t* source, int length);

void AUD_convert_s32_double(data_t* target, data_t* source, int length);

void AUD_convert_float_u8(data_t* target, data_t* source, int length);

void AUD_convert_float_s16(data_t* target, data_t* source, int length);

void AUD_convert_float_s24_be(data_t* target, data_t* source, int length);

void AUD_convert_float_s24_le(data_t* target, data_t* source, int length);

void AUD_convert_float_s32(data_t* target, data_t* source, int length);

void AUD_convert_float_double(data_t* target, data_t* source, int length);

void AUD_convert_double_u8(data_t* target, data_t* source, int length);

void AUD_convert_double_s16(data_t* target, data_t* source, int length);

void AUD_convert_double_s24_be(data_t* target, data_t* source, int length);

void AUD_convert_double_s24_le(data_t* target, data_t* source, int length);

void AUD_convert_double_s32(data_t* target, data_t* source, int length);

void AUD_convert_double_float(data_t* target, data_t* source, int length);

#endif //AUD_CONVERTERFUNCTIONS
