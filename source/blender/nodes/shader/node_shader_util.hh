/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include <cfloat>
#include <cmath>
#include <cstring>

#include "BLI_blenlib.h"
#include "BLI_color.hh"
#include "BLI_math_base_safe.h"
#include "BLI_math_vector.hh"
#include "BLI_rand.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.hh"
#include "BKE_texture.h"

#include "DNA_ID.h"
#include "DNA_color_types.h"
#include "DNA_customdata_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "FN_multi_function_builder.hh"

#include "GPU_material.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

#include "IMB_colormanagement.h"

#include "MEM_guardedalloc.h"

#include "NOD_multi_function.hh"
#include "NOD_shader.h"
#include "NOD_socket_declarations.hh"

#include "node_shader_register.hh"
#include "node_util.hh"

#include "RE_pipeline.h"
#include "RE_texture.h"

#include "RNA_access.h"

bool sh_node_poll_default(const bNodeType *ntype,
                          const bNodeTree *ntree,
                          const char **r_disabled_hint);
void sh_node_type_base(bNodeType *ntype, int type, const char *name, short nclass);
void sh_fn_node_type_base(bNodeType *ntype, int type, const char *name, short nclass);
bool line_style_shader_nodes_poll(const bContext *C);
bool world_shader_nodes_poll(const bContext *C);
bool object_shader_nodes_poll(const bContext *C);
bool object_cycles_shader_nodes_poll(const bContext *C);
bool object_eevee_shader_nodes_poll(const bContext *C);

/* ********* exec data struct, remains internal *********** */

struct XYZ_to_RGB /* Transposed #imbuf_xyz_to_rgb, passed as 3x vec3. */
{
  float r[3], g[3], b[3];
};

void node_gpu_stack_from_data(GPUNodeStack *gs, int type, bNodeStack *ns);
void node_data_from_gpu_stack(bNodeStack *ns, GPUNodeStack *gs);
void node_shader_gpu_bump_tex_coord(GPUMaterial *mat, bNode *node, GPUNodeLink **link);
void node_shader_gpu_default_tex_coord(GPUMaterial *mat, bNode *node, GPUNodeLink **link);
void node_shader_gpu_tex_mapping(GPUMaterial *mat,
                                 bNode *node,
                                 GPUNodeStack *in,
                                 GPUNodeStack *out);

bNodeTreeExec *ntreeShaderBeginExecTree_internal(bNodeExecContext *context,
                                                 bNodeTree *ntree,
                                                 bNodeInstanceKey parent_key);
void ntreeShaderEndExecTree_internal(bNodeTreeExec *exec);

void ntreeExecGPUNodes(bNodeTreeExec *exec, GPUMaterial *mat, bNode *output_node);
void get_XYZ_to_RGB_for_gpu(XYZ_to_RGB *data);
