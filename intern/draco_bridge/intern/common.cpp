/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "common.h"

#include <cstring>

size_t getNumberOfComponents(char *dataType)
{
  if (!strcmp(dataType, "SCALAR")) {
    return 1;
  }
  if (!strcmp(dataType, "VEC2")) {
    return 2;
  }
  if (!strcmp(dataType, "VEC3")) {
    return 3;
  }
  if (!strcmp(dataType, "VEC4")) {
    return 4;
  }
  if (!strcmp(dataType, "MAT2")) {
    return 4;
  }
  if (!strcmp(dataType, "MAT3")) {
    return 9;
  }
  if (!strcmp(dataType, "MAT4")) {
    return 16;
  }

  return 0;
}

size_t getComponentByteLength(size_t componentType)
{
  switch (componentType) {
    case ComponentType::Byte:
    case ComponentType::UnsignedByte:
      return 1;

    case ComponentType::Short:
    case ComponentType::UnsignedShort:
      return 2;

    case ComponentType::UnsignedInt:
    case ComponentType::Float:
      return 4;

    default:
      return 0;
  }
}

size_t getAttributeStride(size_t componentType, char *dataType)
{
  return getComponentByteLength(componentType) * getNumberOfComponents(dataType);
}
