/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <stdint.h>

#include "BLI_implicit_sharing.h"

#ifdef __cplusplus

namespace blender::bke {
class AttributeStorage;
class AttributeStorageRuntime;
}  // namespace blender::bke

using AttributeStorageRuntimeHandle = blender::bke::AttributeStorageRuntime;
#else
typedef struct AttributeStorageRuntimeHandle AttributeStorageRuntimeHandle;
#endif

/** DNA data for bke::Attribute::ArrayData. */
struct AttributeArray {
  void *data;
  const ImplicitSharingInfoHandle *sharing_info;
  /* The number of elements in the array. */
  int64_t size;
};

/** DNA data for bke::Attribute::SingleData. */
struct AttributeSingle {
  void *data;
  const ImplicitSharingInfoHandle *sharing_info;
};

/** DNA data for bke::Attribute. */
struct Attribute {
  const char *name;
  /* bke::AttrType. */
  int16_t data_type;
  /* bke::AttrDomain. */
  int8_t domain;
  /* bke::AttrStorageType */
  int8_t storage_type;
  char _pad[4];

  /** Type depends on storage type. */
  void *data;
};

/**
 * The DNA type for storing attribute values. Logic at runtime is handled by the runtime struct.
 * The DNA data here is created just for file reading and writing.
 */
struct AttributeStorage {
  /* Array only used in files, otherwise #AttributeStorageRuntime::attributes is used. */
  struct Attribute *dna_attributes;
  int dna_attributes_num;

  char _pad[4];

  AttributeStorageRuntimeHandle *runtime;

#ifdef __cplusplus
  blender::bke::AttributeStorage &wrap();
  const blender::bke::AttributeStorage &wrap() const;
#endif
};
