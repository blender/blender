/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup nodes
 */

#pragma once

#include <cfloat>
#include <cmath>
#include <cstring>

#include "BLI_blenlib.h"
#include "BLI_color.hh"
#include "BLI_float3.hh"
#include "BLI_math.h"
#include "BLI_math_base_safe.h"
#include "BLI_rand.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
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
#include "node_util.h"

#include "RE_pipeline.h"
#include "RE_texture.h"

bool sh_node_poll_default(struct bNodeType *ntype,
                          struct bNodeTree *ntree,
                          const char **r_disabled_hint);
void sh_node_type_base(struct bNodeType *ntype, int type, const char *name, short nclass);
void sh_fn_node_type_base(struct bNodeType *ntype, int type, const char *name, short nclass);

/* ********* exec data struct, remains internal *********** */

struct XYZ_to_RGB /* Transposed #imbuf_xyz_to_rgb, passed as 3x vec3. */
{
  float r[3], g[3], b[3];
};

void node_gpu_stack_from_data(struct GPUNodeStack *gs, int type, struct bNodeStack *ns);
void node_data_from_gpu_stack(struct bNodeStack *ns, struct GPUNodeStack *gs);
void node_shader_gpu_bump_tex_coord(struct GPUMaterial *mat,
                                    struct bNode *node,
                                    struct GPUNodeLink **link);
void node_shader_gpu_default_tex_coord(struct GPUMaterial *mat,
                                       struct bNode *node,
                                       struct GPUNodeLink **link);
void node_shader_gpu_tex_mapping(struct GPUMaterial *mat,
                                 struct bNode *node,
                                 struct GPUNodeStack *in,
                                 struct GPUNodeStack *out);

void ntreeExecGPUNodes(struct bNodeTreeExec *exec,
                       struct GPUMaterial *mat,
                       struct bNode *output_node);
void get_XYZ_to_RGB_for_gpu(XYZ_to_RGB *data);
