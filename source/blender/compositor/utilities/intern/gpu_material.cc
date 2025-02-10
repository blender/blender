/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"

#include "DNA_node_types.h"

#include "BKE_node_runtime.hh"

#include "GPU_material.hh"

#include "COM_utilities_gpu_material.hh"

namespace blender::compositor {

GPUNodeStack &get_shader_node_input(const bNode &node,
                                    GPUNodeStack inputs[],
                                    const StringRef identifier)
{
  return inputs[node.input_by_identifier(identifier).index()];
}

GPUNodeStack &get_shader_node_output(const bNode &node,
                                     GPUNodeStack outputs[],
                                     const StringRef identifier)
{
  return outputs[node.output_by_identifier(identifier).index()];
}

GPUNodeLink *get_shader_node_input_link(const bNode &node,
                                        GPUNodeStack inputs[],
                                        const StringRef identifier)
{
  GPUNodeStack &input = get_shader_node_input(node, inputs, identifier);
  if (input.link) {
    return input.link;
  }
  return GPU_uniform(input.vec);
}

}  // namespace blender::compositor
