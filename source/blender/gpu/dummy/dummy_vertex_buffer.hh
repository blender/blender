/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_vertex_buffer_private.hh"

namespace blender::gpu {

class DummyVertexBuffer : public VertBuf {

 public:
  void bind_as_ssbo(uint /*binding*/) override {}
  void bind_as_texture(uint /*binding*/) override {}
  void wrap_handle(uint64_t /*handle*/) override {}

  void update_sub(uint /*start*/, uint /*len*/, const void * /*data*/) override {}
  void read(void * /*data*/) const override {}

 protected:
  void acquire_data() override
  {
    MEM_SAFE_FREE(data);
    data = (uchar *)MEM_mallocN(sizeof(uchar) * this->size_alloc_get(), __func__);
  }
  void resize_data() override {}
  void release_data() override
  {
    MEM_SAFE_FREE(data);
  }
  void upload_data() override {}
  void duplicate_data(VertBuf * /*dst*/) override {}
};

}  // namespace blender::gpu
