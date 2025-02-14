/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include "DNA_node_types.h"

#include "GPU_material.hh"

namespace blender::compositor {

/* Returns the GPU node stack of the input with the given identifier in the given node within the
 * given inputs stack array. See the ShaderNode class for more information. */
GPUNodeStack &get_shader_node_input(const bNode &node,
                                    GPUNodeStack inputs[],
                                    StringRef identifier);

/* Returns the GPU node stack of the output with the given identifier in the given node within the
 * given output stack array. See the ShaderNode class for more information. */
GPUNodeStack &get_shader_node_output(const bNode &node,
                                     GPUNodeStack outputs[],
                                     StringRef identifier);

/* Returns the GPU node link of the input with the given identifier in the given node within the
 * given inputs stack array, if the input is not linked, a uniform link carrying the value of the
 * input will be created and returned. It is expected that the caller will use the returned link in
 * a GPU material, otherwise, the link may not be properly freed. See the ShaderNode class for more
 * information. */
GPUNodeLink *get_shader_node_input_link(const bNode &node,
                                        GPUNodeStack inputs[],
                                        StringRef identifier);

}  // namespace blender::compositor
