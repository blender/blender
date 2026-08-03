/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/usd/sdf/path.h>

#include "BLI_hash.hh"
#include "BLI_vector.hh"

namespace blender {

template<> struct DefaultHash<pxr::SdfPath> {
  uint64_t operator()(const pxr::SdfPath &value) const
  {
    return uint64_t(value.GetHash());
  }
};

template<> struct DefaultHash<pxr::TfToken> {
  uint64_t operator()(const pxr::TfToken &value) const
  {
    return uint64_t(value.Hash());
  }
};

namespace io::hydra {

inline pxr::GfMatrix4d gf_matrix_from_transform(const float m[4][4])
{
  pxr::GfMatrix4d ret;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      ret[i][j] = m[i][j];
    }
  }
  return ret;
}

/** Resize without value-initializing new elements; caller must fill them. */
template<typename T> inline void resize_uninitialized(pxr::VtArray<T> &array, const int new_size)
{
  static_assert(std::is_trivial_v<T>);
  array.resize(new_size, [](auto /*begin*/, auto /*end*/) {});
}

/* Helper for building flat name -> value Hydra container data sources. */
struct HdContainerBuilder {
  Vector<pxr::TfToken> names;
  Vector<pxr::HdDataSourceBaseHandle> values;

  void reserve(int64_t n)
  {
    names.reserve(n);
    values.reserve(n);
  }

  /* Append a value, wrapping raw scalar/vector values in a typed sampled
   * data source. Pre-built data source handles are appended as-is. */
  template<typename T> void add(const pxr::TfToken &name, const T &value)
  {
    using U = std::decay_t<T>;
    names.append(name);
    if constexpr (std::is_convertible_v<U, pxr::HdDataSourceBaseHandle>) {
      values.append(value);
    }
    else {
      values.append(pxr::HdRetainedTypedSampledDataSource<U>::New(value));
    }
  }

  pxr::HdContainerDataSourceHandle build() const
  {
    return pxr::HdRetainedContainerDataSource::New(names.size(), names.data(), values.data());
  }
};

}  // namespace io::hydra
}  // namespace blender
