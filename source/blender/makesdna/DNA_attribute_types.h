/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_implicit_sharing.h"

#ifdef __cplusplus

namespace blender::bke {
class AttributeStorage;
class AttributeStorageRuntime;
}  // namespace blender::bke

using AttributeStorageRuntimeHandle = blender::bke::AttributeStorageRuntime;
#else
struct AttributeStorageRuntimeHandle;
#endif

/** DNA data for bke::Attribute::ArrayData. */
struct AttributeArray {
  void *data = nullptr;
  const ImplicitSharingInfoHandle *sharing_info = nullptr;
  /* The number of elements in the array. */
  int64_t size = 0;
};

/** DNA data for bke::Attribute::SingleData. */
struct AttributeSingle {
  void *data = nullptr;
  const ImplicitSharingInfoHandle *sharing_info = nullptr;
};

/** DNA data for bke::Attribute. */
struct Attribute {
  const char *name = nullptr;
  /* bke::AttrType. */
  int16_t data_type = 0;
  /* bke::AttrDomain. */
  int8_t domain = 0;
  /* bke::AttrStorageType */
  int8_t storage_type = 0;
  char _pad[4] = {};

  /** Type depends on storage type. */
  void *data = nullptr;
};

/**
 * The DNA type for storing attribute values. Logic at runtime is handled by the runtime struct.
 * The DNA data here is created just for file reading and writing.
 */
struct AttributeStorage {
  /* Array only used in files, otherwise #AttributeStorageRuntime::attributes is used. */
  struct Attribute *dna_attributes = nullptr;
  int dna_attributes_num = 0;

  char _pad[4] = {};

  AttributeStorageRuntimeHandle *runtime = nullptr;

#ifdef __cplusplus
  blender::bke::AttributeStorage &wrap();
  const blender::bke::AttributeStorage &wrap() const;
#endif
};
