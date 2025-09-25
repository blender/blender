/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Generate shader code from the intermediate node graph.
 */

#pragma once

#include "BLI_hash_mm2a.hh"
#include "BLI_listbase.h"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "GPU_material.hh"
#include "GPU_vertex_format.hh"
#include "gpu_node_graph.hh"
#include "gpu_shader_create_info.hh"

#include <sstream>
#include <string>

namespace blender::gpu::shader {

struct GPUCodegenCreateInfo : ShaderCreateInfo {
  struct NameBuffer {
    using NameEntry = std::array<char, 32>;

    /** Duplicate attribute names to avoid reference the GPUNodeGraph directly. */
    char attr_names[16][GPU_MAX_SAFE_ATTR_NAME + 1];
    char var_names[16][8];
    Vector<std::unique_ptr<NameEntry>, 16> sampler_names;

    /* Returns the appended name memory location */
    const char *append_sampler_name(const char name[32]);
  };

  /** Optional generated interface. */
  StageInterfaceInfo *interface_generated = nullptr;
  /** Optional name buffer containing names referenced by StringRefNull. */
  NameBuffer name_buffer;
  /** Copy of the GPUMaterial name, to prevent dangling pointers. */
  std::string info_name_;

  GPUCodegenCreateInfo(const char *name) : ShaderCreateInfo(name), info_name_(name)
  {
    /* Base class is always initialized first, so we need to update the name_ pointer here. */
    name_ = info_name_.c_str();
  };
  ~GPUCodegenCreateInfo()
  {
    MEM_delete(interface_generated);
  }
};

class GPUCodegen {
 public:
  GPUMaterial &mat;
  GPUNodeGraph &graph;
  GPUCodegenOutput output = {};
  GPUCodegenCreateInfo *create_info = nullptr;

 private:
  uint32_t hash_ = 0;
  BLI_HashMurmur2A hm2a_;
  ListBase ubo_inputs_ = {nullptr, nullptr};
  GPUInput *cryptomatte_input_ = nullptr;

  /** Cache parameters for complexity heuristic. */
  uint nodes_total_ = 0;
  uint textures_total_ = 0;
  uint uniforms_total_ = 0;

 public:
  GPUCodegen(GPUMaterial *mat_, GPUNodeGraph *graph_, const char *debug_name);
  ~GPUCodegen();

  void generate_graphs();
  void generate_cryptomatte();
  void generate_uniform_buffer();
  void generate_attribs();
  void generate_resources();

  uint32_t hash_get() const
  {
    return hash_;
  }

  /* Heuristic determined during pass codegen for whether a
   * more optimal variant of this material should be compiled. */
  bool should_optimize_heuristic() const;

 private:
  void set_unique_ids();

  void node_serialize(blender::Set<blender::StringRefNull> &used_libraries,
                      std::stringstream &eval_ss,
                      const GPUNode *node);
  GPUGraphOutput graph_serialize(GPUNodeTag tree_tag,
                                 GPUNodeLink *output_link,
                                 const char *output_default = nullptr);
  GPUGraphOutput graph_serialize(GPUNodeTag tree_tag);
};

}  // namespace blender::gpu::shader
