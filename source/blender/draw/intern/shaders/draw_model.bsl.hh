/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "draw_shader_shared.hh"

namespace draw {

struct ID {
  uint raw_id;

  template<int view_count> uint view_id() const
  {
    /** \note This is simply log2(DRW_VIEW_LEN) but making sure it is optimized out. */
    return raw_id & ~(0xFFFFFFFFu << ((view_count > 32) ? 6 :
                                      (view_count > 16) ? 5 :
                                      (view_count > 8)  ? 4 :
                                      (view_count > 4)  ? 3 :
                                      (view_count > 2)  ? 2 :
                                      (view_count > 1)  ? 1 :
                                                          0));
  }

  template<int view_count> uint resource_id() const
  {
    return raw_id >> ((view_count > 32) ? 6 :
                      (view_count > 16) ? 5 :
                      (view_count > 8)  ? 4 :
                      (view_count > 4)  ? 3 :
                      (view_count > 2)  ? 2 :
                      (view_count > 1)  ? 1 :
                                          0);
  }
};

template uint ID::view_id<1>() const;
template uint ID::view_id<64>() const;
template uint ID::resource_id<1>() const;
template uint ID::resource_id<64>() const;

struct Resource {
  [[storage(DRW_RESOURCE_ID_SLOT, read)]] const uint (&res_id_buf)[];

  /* `instance_index` must be the `[[instance_index]]` entry point parameter. */
  ID get(int instance_index) const
  {
    uint raw = res_id_buf[instance_index];
    return {raw};
  }
};

struct ResourceCustomID {
  [[storage(DRW_RESOURCE_ID_SLOT, read)]] const uint2 (&res_id_with_custom_id_buf)[];

  /* `instance_index` must be the `[[instance_index]]` entry point parameter. */
  ID get(int instance_index) const
  {
    return {res_id_with_custom_id_buf[instance_index].x};
  }

  /* `instance_index` must be the `[[instance_index]]` entry point parameter. */
  uint get_custom_id(int instance_index) const
  {
    return res_id_with_custom_id_buf[instance_index].y;
  }
};

struct Model {
  [[storage(DRW_OBJ_MAT_SLOT, read)]] const ObjectMatrices (&drw_matrix_buf)[];

  /* `resource_id` should be the result of `draw::ID::resource_id()` or manually indexed. */
  ObjectMatrices get(uint resource_id) const
  {
    return drw_matrix_buf[resource_id];
  }
};

struct Infos {
  [[storage(DRW_OBJ_INFOS_SLOT, read)]] const ObjectInfos (&drw_infos)[];

  /* `resource_id` should be the result of `draw::ID::resource_id()` or manually indexed. */
  ObjectInfos get(uint resource_id) const
  {
    return drw_infos[resource_id];
  }
};

}  // namespace draw
