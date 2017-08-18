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

#pragma once

/**
 * @file ConverterFunctions.h
 * @ingroup respec
 * Defines several conversion functions between different sample formats.
 */

#include "Audaspace.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

/**
 * The function template for functions converting from one sample format
 * to another, having the same parameter order as std::memcpy.
 */
typedef void (*convert_f)(data_t* target, data_t* source, int length);

/**
 * The copy conversion function simply calls std::memcpy.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
template <class T>
void convert_copy(data_t* target, data_t* source, int length)
{
	std::memcpy(target, source, length*sizeof(T));
}

/**
 * @brief Converts from FORMAT_U8 to FORMAT_S16.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_u8_s16(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_U8 to FORMAT_S24 big endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_u8_s24_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_U8 to FORMAT_S24 little endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_u8_s24_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_U8 to FORMAT_S32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_u8_s32(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_U8 to FORMAT_FLOAT32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_u8_float(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_U8 to FORMAT_FLOAT64.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_u8_double(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S16 to FORMAT_U8.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s16_u8(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S16 to FORMAT_S24 big endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s16_s24_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S16 to FORMAT_S24 little endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s16_s24_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S16 to FORMAT_S32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s16_s32(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S16 to FORMAT_FLOAT32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s16_float(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S16 to FORMAT_FLOAT64.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s16_double(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 big endian to FORMAT_U8.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_u8_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 little endian to FORMAT_U8.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_u8_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 big endian to FORMAT_S16.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_s16_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 little endian to FORMAT_S16.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_s16_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 to FORMAT_S24 simply using std::memcpy.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_s24(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 big endian to FORMAT_S32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_s32_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 little endian to FORMAT_S32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_s32_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 big endian to FORMAT_FLOAT32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_float_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 little endian to FORMAT_FLOAT32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_float_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 big endian to FORMAT_FLOAT64.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_double_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S24 little endian to FORMAT_FLOAT64.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s24_double_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S32 to FORMAT_U8.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s32_u8(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S32 to FORMAT_S16.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s32_s16(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S32 to FORMAT_S24 big endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s32_s24_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S32 to FORMAT_S24 little endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s32_s24_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S32 to FORMAT_FLOAT32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s32_float(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_S32 to FORMAT_FLOAT64.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_s32_double(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT32 to FORMAT_U8.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_float_u8(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT32 to FORMAT_S16.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_float_s16(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT32 to FORMAT_S24 big endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_float_s24_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT32 to FORMAT_S24 little endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_float_s24_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT32 to FORMAT_S32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_float_s32(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT32 to FORMAT_FLOAT64.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_float_double(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT64 to FORMAT_U8.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_double_u8(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT64 to FORMAT_S16.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_double_s16(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT64 to FORMAT_S24 big endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_double_s24_be(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT64 to FORMAT_S24 little endian.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_double_s24_le(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT64 to FORMAT_S32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_double_s32(data_t* target, data_t* source, int length);

/**
 * @brief Converts from FORMAT_FLOAT64 to FORMAT_FLOAT32.
 * @param target The target buffer.
 * @param source The source buffer.
 * @param length The amount of samples to be converted.
 */
void AUD_API convert_double_float(data_t* target, data_t* source, int length);

AUD_NAMESPACE_END
