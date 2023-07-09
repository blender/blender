/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"

#include "BLI_generic_virtual_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

#include "BLI_mempool.h"

#include "bmesh.h"

namespace blender::ed::spreadsheet {

eSpreadsheetColumnValueType cpp_type_to_column_type(const CPPType &type);

/**
 * This represents a column in a spreadsheet. It has a name and provides a value for all the cells
 * in the column.
 */
class ColumnValues final {
 protected:
  std::string name_;

  GVArray data_;
  void *bmesh_data_ = nullptr;

 public:
  ColumnValues(std::string name, GVArray data) : name_(std::move(name)), data_(std::move(data))
  {
    /* The array should not be empty. */
    BLI_assert(data_);
  }

  ColumnValues(const ColumnValues &b) = delete;

  ColumnValues(std::string name, BLI_mempool *pool, int type, int offset, int htype)
      : name_(std::move(name))
  {
    using namespace blender;

    auto create_array = [&](auto typevar) {
      using T = decltype(typevar);

      int count = BLI_mempool_len(pool);

      T *array = MEM_cnew_array<T>(count, "ColumnValues::bmesh_data");
      bmesh_data_ = static_cast<void *>(array);

      BLI_mempool_iter iter;
      BLI_mempool_iternew(pool, &iter);
      void *velem;
      int i = 0;

      for (velem = BLI_mempool_iterstep(&iter); velem; velem = BLI_mempool_iterstep(&iter), i++) {
        BMElem *elem = static_cast<BMElem *>(velem);
        T *value = static_cast<T *>(POINTER_OFFSET(elem->head.data, offset));

        array[i] = *value;
      }

      data_ = VArray<T>::ForSpan({array, count});
    };

    switch (type) {
      case CD_PROP_FLOAT:
        create_array(float(0));
        break;
      case CD_PROP_FLOAT2:
        create_array(float2());
        break;
      case CD_PROP_FLOAT3:
        create_array(float3());
        break;
      case CD_PROP_COLOR:
        create_array(float4());
        break;
      case CD_PROP_BYTE_COLOR:
        create_array(uchar4());
        break;
      case CD_PROP_INT32:
        create_array(int(0));
        break;
      case CD_PROP_INT8:
        create_array(int8_t(0));
      case CD_PROP_BOOL:
        create_array(bool(0));
        break;
    }
  }

  ~ColumnValues() {
    if (bmesh_data_) {
      MEM_freeN(bmesh_data_);
    }
  }

  eSpreadsheetColumnValueType type() const
  {
    return cpp_type_to_column_type(data_.type());
  }

  StringRefNull name() const
  {
    return name_;
  }

  int size() const
  {
    return data_.size();
  }

  const GVArray &data() const
  {
    return data_;
  }

  /* The default width of newly created columns, in UI units. */
  float default_width = 0.0f;
};

}  // namespace blender::ed::spreadsheet
