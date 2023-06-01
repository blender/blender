/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_query.hh"

namespace blender::gpu {

void VKQueryPool::init(GPUQueryType /*type*/) {}

void VKQueryPool::begin_query() {}

void VKQueryPool::end_query() {}

void VKQueryPool::get_occlusion_result(MutableSpan<uint32_t> /*r_values*/) {}

}  // namespace blender::gpu
