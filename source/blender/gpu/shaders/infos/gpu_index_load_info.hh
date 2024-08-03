/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_index_load)
    .push_constant(Type::BOOL, "gpu_index_no_buffer")
    .push_constant(Type::BOOL, "gpu_index_16bit")
    .push_constant(Type::INT, "gpu_index_base_index")
    .storage_buf(
        GPU_SSBO_INDEX_BUF_SLOT, Qualifier::READ, "uint", "gpu_index_buf[]", Frequency::GEOMETRY)
    .define("GPU_INDEX_LOAD");
