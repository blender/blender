/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_implicit_sharing.h"

namespace blender {

namespace bke {
class AttributeStorage;
class AttributeStorageRuntime;
}  // namespace bke

/** DNA data for bke::Attribute::ArrayData. */
struct AttributeArray {
  void *data = nullptr;
  const ImplicitSharingInfoHandle *sharing_info = nullptr;
  /* The number of elements in the array. */
  int64_t size = 0;
  /**
   * Blender 5.0 (the first version to read #AttributeStorage) does not fully support single value
   * storage at runtime, even though it supports reading it from the file. The purpose of this
   * field is to let versions 5.1 and later detect that the storage is a single value, while still
   * being compatible with 5.0, which doesn't have this field and will just use array storage.
   */
  int8_t is_single = 0;
  char _pad[7] = {};
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

  bke::AttributeStorageRuntime *runtime = nullptr;

#ifdef __cplusplus
  bke::AttributeStorage &wrap();
  const bke::AttributeStorage &wrap() const;
#endif
};

}  // namespace blender
