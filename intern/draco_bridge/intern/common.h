/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/**
 * Library for the Draco encoding/decoding feature inside the glTF-Blender-IO project.
 *
 * The python script within glTF-Blender-IO uses the CTypes library to open the DLL,
 * load function pointers add pass the raw data to the encoder.
 *
 * @author Jim Eckerlein <eckerlein@ux3d.io>
 * @date   2020-11-18
 */

#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER)
#  define API(returnType) extern "C" __declspec(dllexport) returnType __cdecl
#else
#  define API(returnType) extern "C" returnType
#endif

enum ComponentType : size_t {
  Byte = 5120,
  UnsignedByte = 5121,
  Short = 5122,
  UnsignedShort = 5123,
  UnsignedInt = 5125,
  Float = 5126,
};

size_t getNumberOfComponents(char *dataType);

size_t getComponentByteLength(size_t componentType);

size_t getAttributeStride(size_t componentType, char *dataType);
