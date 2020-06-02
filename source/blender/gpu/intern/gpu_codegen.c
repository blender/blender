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
 * \ingroup gpu
 *
 * Convert material node-trees to GLSL.
 */

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_image_types.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_hash_mm2a.h"
#include "BLI_link_utils.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BKE_material.h"

#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_uniformbuffer.h"
#include "GPU_vertex_format.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "gpu_codegen.h"
#include "gpu_material_library.h"
#include "gpu_node_graph.h"

#include <stdarg.h>
#include <string.h>

extern char datatoc_gpu_shader_common_obinfos_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];

/* -------------------- GPUPass Cache ------------------ */
/**
 * Internal shader cache: This prevent the shader recompilation / stall when
 * using undo/redo AND also allows for GPUPass reuse if the Shader code is the
 * same for 2 different Materials. Unused GPUPasses are free by Garbage collection.
 */

/* Only use one linklist that contains the GPUPasses grouped by hash. */
static GPUPass *pass_cache = NULL;
static SpinLock pass_cache_spin;

static uint32_t gpu_pass_hash(const char *frag_gen, const char *defs, ListBase *attributes)
{
  BLI_HashMurmur2A hm2a;
  BLI_hash_mm2a_init(&hm2a, 0);
  BLI_hash_mm2a_add(&hm2a, (uchar *)frag_gen, strlen(frag_gen));
  LISTBASE_FOREACH (GPUMaterialAttribute *, attr, attributes) {
    BLI_hash_mm2a_add(&hm2a, (uchar *)attr->name, strlen(attr->name));
  }
  if (defs) {
    BLI_hash_mm2a_add(&hm2a, (uchar *)defs, strlen(defs));
  }

  return BLI_hash_mm2a_end(&hm2a);
}

/* Search by hash only. Return first pass with the same hash.
 * There is hash collision if (pass->next && pass->next->hash == hash) */
static GPUPass *gpu_pass_cache_lookup(uint32_t hash)
{
  BLI_spin_lock(&pass_cache_spin);
  /* Could be optimized with a Lookup table. */
  for (GPUPass *pass = pass_cache; pass; pass = pass->next) {
    if (pass->hash == hash) {
      BLI_spin_unlock(&pass_cache_spin);
      return pass;
    }
  }
  BLI_spin_unlock(&pass_cache_spin);
  return NULL;
}

/* Check all possible passes with the same hash. */
static GPUPass *gpu_pass_cache_resolve_collision(GPUPass *pass,
                                                 const char *vert,
                                                 const char *geom,
                                                 const char *frag,
                                                 const char *defs,
                                                 uint32_t hash)
{
  BLI_spin_lock(&pass_cache_spin);
  /* Collision, need to strcmp the whole shader. */
  for (; pass && (pass->hash == hash); pass = pass->next) {
    if ((defs != NULL) && (strcmp(pass->defines, defs) != 0)) { /* Pass */
    }
    else if ((geom != NULL) && (strcmp(pass->geometrycode, geom) != 0)) { /* Pass */
    }
    else if ((strcmp(pass->fragmentcode, frag) == 0) && (strcmp(pass->vertexcode, vert) == 0)) {
      BLI_spin_unlock(&pass_cache_spin);
      return pass;
    }
  }
  BLI_spin_unlock(&pass_cache_spin);
  return NULL;
}

/* GLSL code generation */

static void codegen_convert_datatype(DynStr *ds, int from, int to, const char *tmp, int id)
{
  char name[1024];

  BLI_snprintf(name, sizeof(name), "%s%d", tmp, id);

  if (from == to) {
    BLI_dynstr_append(ds, name);
  }
  else if (to == GPU_FLOAT) {
    if (from == GPU_VEC4) {
      BLI_dynstr_appendf(ds, "dot(%s.rgb, vec3(0.2126, 0.7152, 0.0722))", name);
    }
    else if (from == GPU_VEC3) {
      BLI_dynstr_appendf(ds, "(%s.r + %s.g + %s.b) / 3.0", name, name, name);
    }
    else if (from == GPU_VEC2) {
      BLI_dynstr_appendf(ds, "%s.r", name);
    }
  }
  else if (to == GPU_VEC2) {
    if (from == GPU_VEC4) {
      BLI_dynstr_appendf(ds, "vec2((%s.r + %s.g + %s.b) / 3.0, %s.a)", name, name, name, name);
    }
    else if (from == GPU_VEC3) {
      BLI_dynstr_appendf(ds, "vec2((%s.r + %s.g + %s.b) / 3.0, 1.0)", name, name, name);
    }
    else if (from == GPU_FLOAT) {
      BLI_dynstr_appendf(ds, "vec2(%s, 1.0)", name);
    }
  }
  else if (to == GPU_VEC3) {
    if (from == GPU_VEC4) {
      BLI_dynstr_appendf(ds, "%s.rgb", name);
    }
    else if (from == GPU_VEC2) {
      BLI_dynstr_appendf(ds, "vec3(%s.r, %s.r, %s.r)", name, name, name);
    }
    else if (from == GPU_FLOAT) {
      BLI_dynstr_appendf(ds, "vec3(%s, %s, %s)", name, name, name);
    }
  }
  else if (to == GPU_VEC4) {
    if (from == GPU_VEC3) {
      BLI_dynstr_appendf(ds, "vec4(%s, 1.0)", name);
    }
    else if (from == GPU_VEC2) {
      BLI_dynstr_appendf(ds, "vec4(%s.r, %s.r, %s.r, %s.g)", name, name, name, name);
    }
    else if (from == GPU_FLOAT) {
      BLI_dynstr_appendf(ds, "vec4(%s, %s, %s, 1.0)", name, name, name);
    }
  }
  else if (to == GPU_CLOSURE) {
    if (from == GPU_VEC4) {
      BLI_dynstr_appendf(ds, "closure_emission(%s.rgb)", name);
    }
    else if (from == GPU_VEC3) {
      BLI_dynstr_appendf(ds, "closure_emission(%s.rgb)", name);
    }
    else if (from == GPU_VEC2) {
      BLI_dynstr_appendf(ds, "closure_emission(%s.rrr)", name);
    }
    else if (from == GPU_FLOAT) {
      BLI_dynstr_appendf(ds, "closure_emission(vec3(%s, %s, %s))", name, name, name);
    }
  }
  else {
    BLI_dynstr_append(ds, name);
  }
}

static void codegen_print_datatype(DynStr *ds, const eGPUType type, float *data)
{
  int i;

  BLI_dynstr_appendf(ds, "%s(", gpu_data_type_to_string(type));

  for (i = 0; i < type; i++) {
    BLI_dynstr_appendf(ds, "%.12f", data[i]);
    if (i == type - 1) {
      BLI_dynstr_append(ds, ")");
    }
    else {
      BLI_dynstr_append(ds, ", ");
    }
  }
}

static const char *gpu_builtin_name(eGPUBuiltin builtin)
{
  if (builtin == GPU_VIEW_MATRIX) {
    return "unfviewmat";
  }
  else if (builtin == GPU_OBJECT_MATRIX) {
    return "unfobmat";
  }
  else if (builtin == GPU_INVERSE_VIEW_MATRIX) {
    return "unfinvviewmat";
  }
  else if (builtin == GPU_INVERSE_OBJECT_MATRIX) {
    return "unfinvobmat";
  }
  else if (builtin == GPU_LOC_TO_VIEW_MATRIX) {
    return "unflocaltoviewmat";
  }
  else if (builtin == GPU_INVERSE_LOC_TO_VIEW_MATRIX) {
    return "unfinvlocaltoviewmat";
  }
  else if (builtin == GPU_VIEW_POSITION) {
    return "varposition";
  }
  else if (builtin == GPU_WORLD_NORMAL) {
    return "varwnormal";
  }
  else if (builtin == GPU_VIEW_NORMAL) {
    return "varnormal";
  }
  else if (builtin == GPU_OBJECT_COLOR) {
    return "unfobjectcolor";
  }
  else if (builtin == GPU_AUTO_BUMPSCALE) {
    return "unfobautobumpscale";
  }
  else if (builtin == GPU_CAMERA_TEXCO_FACTORS) {
    return "unfcameratexfactors";
  }
  else if (builtin == GPU_PARTICLE_SCALAR_PROPS) {
    return "unfparticlescalarprops";
  }
  else if (builtin == GPU_PARTICLE_LOCATION) {
    return "unfparticleco";
  }
  else if (builtin == GPU_PARTICLE_VELOCITY) {
    return "unfparticlevel";
  }
  else if (builtin == GPU_PARTICLE_ANG_VELOCITY) {
    return "unfparticleangvel";
  }
  else if (builtin == GPU_OBJECT_INFO) {
    return "unfobjectinfo";
  }
  else if (builtin == GPU_BARYCENTRIC_TEXCO) {
    return "unfbarycentrictex";
  }
  else if (builtin == GPU_BARYCENTRIC_DIST) {
    return "unfbarycentricdist";
  }
  else {
    return "";
  }
}

static void codegen_set_unique_ids(GPUNodeGraph *graph)
{
  GPUNode *node;
  GPUInput *input;
  GPUOutput *output;
  int id = 1;

  for (node = graph->nodes.first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      /* set id for unique names of uniform variables */
      input->id = id++;
    }

    for (output = node->outputs.first; output; output = output->next) {
      /* set id for unique names of tmp variables storing output */
      output->id = id++;
    }
  }
}

/**
 * It will create an UBO for GPUMaterial if there is any GPU_DYNAMIC_UBO.
 */
static int codegen_process_uniforms_functions(GPUMaterial *material,
                                              DynStr *ds,
                                              GPUNodeGraph *graph)
{
  GPUNode *node;
  GPUInput *input;
  const char *name;
  int builtins = 0;
  ListBase ubo_inputs = {NULL, NULL};

  /* Attributes */
  LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &graph->attributes) {
    BLI_dynstr_appendf(ds, "in %s var%d;\n", gpu_data_type_to_string(attr->gputype), attr->id);
  }

  /* Textures */
  LISTBASE_FOREACH (GPUMaterialTexture *, tex, &graph->textures) {
    if (tex->colorband) {
      BLI_dynstr_appendf(ds, "uniform sampler1DArray %s;\n", tex->sampler_name);
    }
    else if (tex->tiled_mapping_name[0]) {
      BLI_dynstr_appendf(ds, "uniform sampler2DArray %s;\n", tex->sampler_name);
      BLI_dynstr_appendf(ds, "uniform sampler1DArray %s;\n", tex->tiled_mapping_name);
    }
    else {
      BLI_dynstr_appendf(ds, "uniform sampler2D %s;\n", tex->sampler_name);
    }
  }

  /* Volume Grids */
  LISTBASE_FOREACH (GPUMaterialVolumeGrid *, grid, &graph->volume_grids) {
    BLI_dynstr_appendf(ds, "uniform sampler3D %s;\n", grid->sampler_name);
    BLI_dynstr_appendf(ds, "uniform mat4 %s = mat4(0.0);\n", grid->transform_name);
  }

  /* Print other uniforms */
  for (node = graph->nodes.first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_BUILTIN) {
        /* only define each builtin uniform/varying once */
        if (!(builtins & input->builtin)) {
          builtins |= input->builtin;
          name = gpu_builtin_name(input->builtin);

          if (BLI_str_startswith(name, "unf")) {
            BLI_dynstr_appendf(ds, "uniform %s %s;\n", gpu_data_type_to_string(input->type), name);
          }
          else {
            BLI_dynstr_appendf(ds, "in %s %s;\n", gpu_data_type_to_string(input->type), name);
          }
        }
      }
      else if (input->source == GPU_SOURCE_STRUCT) {
        /* Add other struct here if needed. */
        BLI_dynstr_appendf(ds, "Closure strct%d = CLOSURE_DEFAULT;\n", input->id);
      }
      else if (input->source == GPU_SOURCE_UNIFORM) {
        if (!input->link) {
          /* We handle the UBOuniforms separately. */
          BLI_addtail(&ubo_inputs, BLI_genericNodeN(input));
        }
      }
      else if (input->source == GPU_SOURCE_CONSTANT) {
        BLI_dynstr_appendf(
            ds, "const %s cons%d = ", gpu_data_type_to_string(input->type), input->id);
        codegen_print_datatype(ds, input->type, input->vec);
        BLI_dynstr_append(ds, ";\n");
      }
    }
  }

  /* Handle the UBO block separately. */
  if ((material != NULL) && !BLI_listbase_is_empty(&ubo_inputs)) {
    GPU_material_uniform_buffer_create(material, &ubo_inputs);

    /* Inputs are sorted */
    BLI_dynstr_appendf(ds, "\nlayout (std140) uniform %s {\n", GPU_UBO_BLOCK_NAME);

    LISTBASE_FOREACH (LinkData *, link, &ubo_inputs) {
      input = link->data;
      BLI_dynstr_appendf(ds, "\t%s unf%d;\n", gpu_data_type_to_string(input->type), input->id);
    }
    BLI_dynstr_append(ds, "};\n");
    BLI_freelistN(&ubo_inputs);
  }

  BLI_dynstr_append(ds, "\n");

  return builtins;
}

static void codegen_declare_tmps(DynStr *ds, GPUNodeGraph *graph)
{
  GPUNode *node;
  GPUOutput *output;

  for (node = graph->nodes.first; node; node = node->next) {
    /* declare temporary variables for node output storage */
    for (output = node->outputs.first; output; output = output->next) {
      if (output->type == GPU_CLOSURE) {
        BLI_dynstr_appendf(ds, "\tClosure tmp%d;\n", output->id);
      }
      else {
        BLI_dynstr_appendf(ds, "\t%s tmp%d;\n", gpu_data_type_to_string(output->type), output->id);
      }
    }
  }

  BLI_dynstr_append(ds, "\n");
}

static void codegen_call_functions(DynStr *ds, GPUNodeGraph *graph, GPUOutput *finaloutput)
{
  GPUNode *node;
  GPUInput *input;
  GPUOutput *output;

  for (node = graph->nodes.first; node; node = node->next) {
    BLI_dynstr_appendf(ds, "\t%s(", node->name);

    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_TEX) {
        BLI_dynstr_append(ds, input->texture->sampler_name);
      }
      else if (input->source == GPU_SOURCE_TEX_TILED_MAPPING) {
        BLI_dynstr_append(ds, input->texture->tiled_mapping_name);
      }
      else if (input->source == GPU_SOURCE_VOLUME_GRID) {
        BLI_dynstr_append(ds, input->volume_grid->sampler_name);
      }
      else if (input->source == GPU_SOURCE_VOLUME_GRID_TRANSFORM) {
        BLI_dynstr_append(ds, input->volume_grid->transform_name);
      }
      else if (input->source == GPU_SOURCE_OUTPUT) {
        codegen_convert_datatype(
            ds, input->link->output->type, input->type, "tmp", input->link->output->id);
      }
      else if (input->source == GPU_SOURCE_BUILTIN) {
        /* TODO(fclem) get rid of that. */
        if (input->builtin == GPU_INVERSE_VIEW_MATRIX) {
          BLI_dynstr_append(ds, "viewinv");
        }
        else if (input->builtin == GPU_VIEW_MATRIX) {
          BLI_dynstr_append(ds, "viewmat");
        }
        else if (input->builtin == GPU_CAMERA_TEXCO_FACTORS) {
          BLI_dynstr_append(ds, "camtexfac");
        }
        else if (input->builtin == GPU_LOC_TO_VIEW_MATRIX) {
          BLI_dynstr_append(ds, "localtoviewmat");
        }
        else if (input->builtin == GPU_INVERSE_LOC_TO_VIEW_MATRIX) {
          BLI_dynstr_append(ds, "invlocaltoviewmat");
        }
        else if (input->builtin == GPU_BARYCENTRIC_DIST) {
          BLI_dynstr_append(ds, "barycentricDist");
        }
        else if (input->builtin == GPU_BARYCENTRIC_TEXCO) {
          BLI_dynstr_append(ds, "barytexco");
        }
        else if (input->builtin == GPU_OBJECT_MATRIX) {
          BLI_dynstr_append(ds, "objmat");
        }
        else if (input->builtin == GPU_OBJECT_INFO) {
          BLI_dynstr_append(ds, "ObjectInfo");
        }
        else if (input->builtin == GPU_OBJECT_COLOR) {
          BLI_dynstr_append(ds, "ObjectColor");
        }
        else if (input->builtin == GPU_INVERSE_OBJECT_MATRIX) {
          BLI_dynstr_append(ds, "objinv");
        }
        else if (input->builtin == GPU_VIEW_POSITION) {
          BLI_dynstr_append(ds, "viewposition");
        }
        else if (input->builtin == GPU_VIEW_NORMAL) {
          BLI_dynstr_append(ds, "facingnormal");
        }
        else if (input->builtin == GPU_WORLD_NORMAL) {
          BLI_dynstr_append(ds, "facingwnormal");
        }
        else {
          BLI_dynstr_append(ds, gpu_builtin_name(input->builtin));
        }
      }
      else if (input->source == GPU_SOURCE_STRUCT) {
        BLI_dynstr_appendf(ds, "strct%d", input->id);
      }
      else if (input->source == GPU_SOURCE_UNIFORM) {
        BLI_dynstr_appendf(ds, "unf%d", input->id);
      }
      else if (input->source == GPU_SOURCE_CONSTANT) {
        BLI_dynstr_appendf(ds, "cons%d", input->id);
      }
      else if (input->source == GPU_SOURCE_ATTR) {
        BLI_dynstr_appendf(ds, "var%d", input->attr->id);
      }

      BLI_dynstr_append(ds, ", ");
    }

    for (output = node->outputs.first; output; output = output->next) {
      BLI_dynstr_appendf(ds, "tmp%d", output->id);
      if (output->next) {
        BLI_dynstr_append(ds, ", ");
      }
    }

    BLI_dynstr_append(ds, ");\n");
  }

  BLI_dynstr_appendf(ds, "\n\treturn tmp%d", finaloutput->id);
  BLI_dynstr_append(ds, ";\n");
}

static char *code_generate_fragment(GPUMaterial *material, GPUNodeGraph *graph)
{
  DynStr *ds = BLI_dynstr_new();
  char *code;
  int builtins;

#if 0
  BLI_dynstr_append(ds, FUNCTION_PROTOTYPES);
#endif

  codegen_set_unique_ids(graph);
  builtins = codegen_process_uniforms_functions(material, ds, graph);

  if (builtins & (GPU_OBJECT_INFO | GPU_OBJECT_COLOR)) {
    BLI_dynstr_append(ds, datatoc_gpu_shader_common_obinfos_lib_glsl);
  }

  if (builtins & GPU_BARYCENTRIC_TEXCO) {
    BLI_dynstr_append(ds, "in vec2 barycentricTexCo;\n");
  }

  if (builtins & GPU_BARYCENTRIC_DIST) {
    BLI_dynstr_append(ds, "flat in vec3 barycentricDist;\n");
  }

  BLI_dynstr_append(ds, "Closure nodetree_exec(void)\n{\n");

  if (builtins & GPU_BARYCENTRIC_TEXCO) {
    BLI_dynstr_append(ds, "#ifdef HAIR_SHADER\n");
    BLI_dynstr_append(ds,
                      "\tvec2 barytexco = vec2((fract(barycentricTexCo.y) != 0.0)\n"
                      "\t                      ? barycentricTexCo.x\n"
                      "\t                      : 1.0 - barycentricTexCo.x,\n"
                      "\t                      0.0);\n");
    BLI_dynstr_append(ds, "#else\n");
    BLI_dynstr_append(ds, "\tvec2 barytexco = barycentricTexCo;\n");
    BLI_dynstr_append(ds, "#endif\n");
  }
  /* TODO(fclem) get rid of that. */
  if (builtins & GPU_VIEW_MATRIX) {
    BLI_dynstr_append(ds, "\t#define viewmat ViewMatrix\n");
  }
  if (builtins & GPU_CAMERA_TEXCO_FACTORS) {
    BLI_dynstr_append(ds, "\t#define camtexfac CameraTexCoFactors\n");
  }
  if (builtins & GPU_OBJECT_MATRIX) {
    BLI_dynstr_append(ds, "\t#define objmat ModelMatrix\n");
  }
  if (builtins & GPU_INVERSE_OBJECT_MATRIX) {
    BLI_dynstr_append(ds, "\t#define objinv ModelMatrixInverse\n");
  }
  if (builtins & GPU_INVERSE_VIEW_MATRIX) {
    BLI_dynstr_append(ds, "\t#define viewinv ViewMatrixInverse\n");
  }
  if (builtins & GPU_LOC_TO_VIEW_MATRIX) {
    BLI_dynstr_append(ds, "\t#define localtoviewmat (ViewMatrix * ModelMatrix)\n");
  }
  if (builtins & GPU_INVERSE_LOC_TO_VIEW_MATRIX) {
    BLI_dynstr_append(ds,
                      "\t#define invlocaltoviewmat (ModelMatrixInverse * ViewMatrixInverse)\n");
  }
  if (builtins & GPU_VIEW_NORMAL) {
    BLI_dynstr_append(ds, "#ifdef HAIR_SHADER\n");
    BLI_dynstr_append(ds, "\tvec3 n;\n");
    BLI_dynstr_append(ds, "\tworld_normals_get(n);\n");
    BLI_dynstr_append(ds, "\tvec3 facingnormal = transform_direction(ViewMatrix, n);\n");
    BLI_dynstr_append(ds, "#else\n");
    BLI_dynstr_append(ds, "\tvec3 facingnormal = gl_FrontFacing ? viewNormal: -viewNormal;\n");
    BLI_dynstr_append(ds, "#endif\n");
  }
  if (builtins & GPU_WORLD_NORMAL) {
    BLI_dynstr_append(ds, "\tvec3 facingwnormal;\n");
    if (builtins & GPU_VIEW_NORMAL) {
      BLI_dynstr_append(ds, "#ifdef HAIR_SHADER\n");
      BLI_dynstr_append(ds, "\tfacingwnormal = n;\n");
      BLI_dynstr_append(ds, "#else\n");
      BLI_dynstr_append(ds, "\tworld_normals_get(facingwnormal);\n");
      BLI_dynstr_append(ds, "#endif\n");
    }
    else {
      BLI_dynstr_append(ds, "\tworld_normals_get(facingwnormal);\n");
    }
  }
  if (builtins & GPU_VIEW_POSITION) {
    BLI_dynstr_append(ds, "\t#define viewposition viewPosition\n");
  }

  codegen_declare_tmps(ds, graph);
  codegen_call_functions(ds, graph, graph->outlink->output);

  BLI_dynstr_append(ds, "}\n");

  /* XXX This cannot go into gpu_shader_material.glsl because main()
   * would be parsed and generate error */
  /* Old glsl mode compat. */
  /* TODO(fclem) This is only used by world shader now. get rid of it? */
  BLI_dynstr_append(ds, "#ifndef NODETREE_EXEC\n");
  BLI_dynstr_append(ds, "out vec4 fragColor;\n");
  BLI_dynstr_append(ds, "void main()\n");
  BLI_dynstr_append(ds, "{\n");
  BLI_dynstr_append(ds, "\tClosure cl = nodetree_exec();\n");
  BLI_dynstr_append(ds,
                    "\tfragColor = vec4(cl.radiance, "
                    "saturate(1.0 - avg(cl.transmittance)));\n");
  BLI_dynstr_append(ds, "}\n");
  BLI_dynstr_append(ds, "#endif\n\n");

  /* create shader */
  code = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

#if 0
  if (G.debug & G_DEBUG) {
    printf("%s\n", code);
  }
#endif

  return code;
}

static const char *attr_prefix_get(CustomDataType type)
{
  switch (type) {
    case CD_ORCO:
      return "orco";
    case CD_MTFACE:
      return "u";
    case CD_TANGENT:
      return "t";
    case CD_MCOL:
      return "c";
    case CD_AUTO_FROM_NAME:
      return "a";
    default:
      BLI_assert(false && "GPUVertAttr Prefix type not found : This should not happen!");
      return "";
  }
}

static char *code_generate_vertex(GPUNodeGraph *graph, const char *vert_code, bool use_geom)
{
  DynStr *ds = BLI_dynstr_new();
  GPUNode *node;
  GPUInput *input;
  char *code;
  int builtins = 0;

  /* Hairs uv and col attributes are passed by bufferTextures. */
  BLI_dynstr_append(ds,
                    "#ifdef HAIR_SHADER\n"
                    "#define DEFINE_ATTR(type, attr) uniform samplerBuffer attr\n"
                    "#else\n"
                    "#define DEFINE_ATTR(type, attr) in type attr\n"
                    "#endif\n");

  LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &graph->attributes) {
    /* XXX FIXME : see notes in mesh_render_data_create() */
    /* NOTE : Replicate changes to mesh_render_data_create() in draw_cache_impl_mesh.c */
    if (attr->type == CD_ORCO) {
      /* OPTI : orco is computed from local positions, but only if no modifier is present. */
      BLI_dynstr_append(ds, datatoc_gpu_shader_common_obinfos_lib_glsl);
      BLI_dynstr_append(ds, "DEFINE_ATTR(vec4, orco);\n");
    }
    else if (attr->name[0] == '\0') {
      BLI_dynstr_appendf(ds,
                         "DEFINE_ATTR(%s, %s);\n",
                         gpu_data_type_to_string(attr->gputype),
                         attr_prefix_get(attr->type));
      BLI_dynstr_appendf(ds, "#define att%d %s\n", attr->id, attr_prefix_get(attr->type));
    }
    else {
      char attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      GPU_vertformat_safe_attr_name(attr->name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
      BLI_dynstr_appendf(ds,
                         "DEFINE_ATTR(%s, %s%s);\n",
                         gpu_data_type_to_string(attr->gputype),
                         attr_prefix_get(attr->type),
                         attr_safe_name);
      BLI_dynstr_appendf(
          ds, "#define att%d %s%s\n", attr->id, attr_prefix_get(attr->type), attr_safe_name);
    }
    BLI_dynstr_appendf(ds,
                       "out %s var%d%s;\n",
                       gpu_data_type_to_string(attr->gputype),
                       attr->id,
                       use_geom ? "g" : "");
  }

  for (node = graph->nodes.first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_BUILTIN) {
        builtins |= input->builtin;
      }
    }
  }

  if (builtins & GPU_BARYCENTRIC_TEXCO) {
    BLI_dynstr_append(ds, "#ifdef HAIR_SHADER\n");
    BLI_dynstr_appendf(ds, "out vec2 barycentricTexCo%s;\n", use_geom ? "g" : "");
    BLI_dynstr_append(ds, "#endif\n");
  }

  if (builtins & GPU_BARYCENTRIC_DIST) {
    BLI_dynstr_append(ds, "out vec3 barycentricPosg;\n");
  }

  BLI_dynstr_append(ds, "\n#define USE_ATTR\n");

  /* Prototype, defined later (this is because of matrices definition). */
  BLI_dynstr_append(ds, "void pass_attr(in vec3 position);\n");

  BLI_dynstr_append(ds, "\n");

  if (use_geom) {
    /* XXX HACK: Eevee specific. */
    char *vert_new, *vert_new2;
    vert_new = BLI_str_replaceN(vert_code, "worldPosition", "worldPositiong");
    vert_new2 = vert_new;
    vert_new = BLI_str_replaceN(vert_new2, "viewPosition", "viewPositiong");
    MEM_freeN(vert_new2);
    vert_new2 = vert_new;
    vert_new = BLI_str_replaceN(vert_new2, "worldNormal", "worldNormalg");
    MEM_freeN(vert_new2);
    vert_new2 = vert_new;
    vert_new = BLI_str_replaceN(vert_new2, "viewNormal", "viewNormalg");
    MEM_freeN(vert_new2);

    BLI_dynstr_append(ds, vert_new);

    MEM_freeN(vert_new);
  }
  else {
    BLI_dynstr_append(ds, vert_code);
  }

  BLI_dynstr_append(ds, "\n");

  BLI_dynstr_append(ds, use_geom ? "RESOURCE_ID_VARYING_GEOM\n" : "RESOURCE_ID_VARYING\n");

  /* Prototype because defined later. */
  BLI_dynstr_append(ds,
                    "vec2 hair_get_customdata_vec2(const samplerBuffer);\n"
                    "vec3 hair_get_customdata_vec3(const samplerBuffer);\n"
                    "vec4 hair_get_customdata_vec4(const samplerBuffer);\n"
                    "vec3 hair_get_strand_pos(void);\n"
                    "int hair_get_base_id(void);\n"
                    "\n");

  BLI_dynstr_append(ds, "void pass_attr(in vec3 position) {\n");

  BLI_dynstr_append(ds, use_geom ? "\tPASS_RESOURCE_ID_GEOM\n" : "\tPASS_RESOURCE_ID\n");

  BLI_dynstr_append(ds, "#ifdef HAIR_SHADER\n");

  if (builtins & GPU_BARYCENTRIC_TEXCO) {
    /* To match cycles without breaking into individual segment we encode if we need to invert
     * the first component into the second component. We invert if the barycentricTexCo.y
     * is NOT 0.0 or 1.0. */
    BLI_dynstr_append(ds, "\tint _base_id = hair_get_base_id();\n");
    BLI_dynstr_appendf(
        ds, "\tbarycentricTexCo%s.x = float((_base_id %% 2) == 1);\n", use_geom ? "g" : "");
    BLI_dynstr_appendf(
        ds, "\tbarycentricTexCo%s.y = float(((_base_id %% 4) %% 3) > 0);\n", use_geom ? "g" : "");
  }

  if (builtins & GPU_BARYCENTRIC_DIST) {
    BLI_dynstr_append(ds, "\tbarycentricPosg = position;\n");
  }

  LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &graph->attributes) {
    if (attr->type == CD_TANGENT) {
      /* Not supported by hairs */
      BLI_dynstr_appendf(ds, "\tvar%d%s = vec4(0.0);\n", attr->id, use_geom ? "g" : "");
    }
    else if (attr->type == CD_ORCO) {
      BLI_dynstr_appendf(ds,
                         "\tvar%d%s = OrcoTexCoFactors[0].xyz + (ModelMatrixInverse * "
                         "vec4(hair_get_strand_pos(), 1.0)).xyz * OrcoTexCoFactors[1].xyz;\n",
                         attr->id,
                         use_geom ? "g" : "");
      /* TODO: fix ORCO with modifiers. */
    }
    else {
      BLI_dynstr_appendf(ds,
                         "\tvar%d%s = hair_get_customdata_%s(att%d);\n",
                         attr->id,
                         use_geom ? "g" : "",
                         gpu_data_type_to_string(attr->gputype),
                         attr->id);
    }
  }

  BLI_dynstr_append(ds, "#else /* MESH_SHADER */\n");

  /* GPU_BARYCENTRIC_TEXCO cannot be computed based on gl_VertexID
   * for MESH_SHADER because of indexed drawing. In this case a
   * geometry shader is needed. */

  if (builtins & GPU_BARYCENTRIC_DIST) {
    BLI_dynstr_append(ds, "\tbarycentricPosg = (ModelMatrix * vec4(position, 1.0)).xyz;\n");
  }

  LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &graph->attributes) {
    if (attr->type == CD_TANGENT) { /* silly exception */
      BLI_dynstr_appendf(ds,
                         "\tvar%d%s.xyz = transpose(mat3(ModelMatrixInverse)) * att%d.xyz;\n",
                         attr->id,
                         use_geom ? "g" : "",
                         attr->id);
      BLI_dynstr_appendf(ds, "\tvar%d%s.w = att%d.w;\n", attr->id, use_geom ? "g" : "", attr->id);
      /* Normalize only if vector is not null. */
      BLI_dynstr_appendf(ds,
                         "\tfloat lvar%d = dot(var%d%s.xyz, var%d%s.xyz);\n",
                         attr->id,
                         attr->id,
                         use_geom ? "g" : "",
                         attr->id,
                         use_geom ? "g" : "");
      BLI_dynstr_appendf(ds,
                         "\tvar%d%s.xyz *= (lvar%d > 0.0) ? inversesqrt(lvar%d) : 1.0;\n",
                         attr->id,
                         use_geom ? "g" : "",
                         attr->id,
                         attr->id);
    }
    else if (attr->type == CD_ORCO) {
      BLI_dynstr_appendf(ds,
                         "\tvar%d%s = OrcoTexCoFactors[0].xyz + position *"
                         " OrcoTexCoFactors[1].xyz;\n",
                         attr->id,
                         use_geom ? "g" : "");
      /* See mesh_create_loop_orco() for explanation. */
      BLI_dynstr_appendf(ds,
                         "\tif (orco.w == 0.0) { var%d%s = orco.xyz * 0.5 + 0.5; }\n",
                         attr->id,
                         use_geom ? "g" : "");
    }
    else {
      BLI_dynstr_appendf(ds, "\tvar%d%s = att%d;\n", attr->id, use_geom ? "g" : "", attr->id);
    }
  }
  BLI_dynstr_append(ds, "#endif /* HAIR_SHADER */\n");

  BLI_dynstr_append(ds, "}\n");

  code = BLI_dynstr_get_cstring(ds);

  BLI_dynstr_free(ds);

#if 0
  if (G.debug & G_DEBUG) {
    printf("%s\n", code);
  }
#endif

  return code;
}

static char *code_generate_geometry(GPUNodeGraph *graph,
                                    const char *geom_code,
                                    const char *defines)
{
  DynStr *ds = BLI_dynstr_new();
  GPUNode *node;
  GPUInput *input;
  char *code;
  int builtins = 0;

  /* XXX we should not make specific eevee cases here. */
  bool is_hair_shader = (strstr(defines, "HAIR_SHADER") != NULL);

  /* Create prototype because attributes cannot be declared before layout. */
  BLI_dynstr_append(ds, "void pass_attr(in int vert);\n");
  BLI_dynstr_append(ds, "void calc_barycentric_distances(vec3 pos0, vec3 pos1, vec3 pos2);\n");
  BLI_dynstr_append(ds, "#define USE_ATTR\n");

  /* Generate varying declarations. */
  for (node = graph->nodes.first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_BUILTIN) {
        builtins |= input->builtin;
      }
    }
  }

  LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &graph->attributes) {
    BLI_dynstr_appendf(ds, "in %s var%dg[];\n", gpu_data_type_to_string(attr->gputype), attr->id);
    BLI_dynstr_appendf(ds, "out %s var%d;\n", gpu_data_type_to_string(attr->gputype), attr->id);
  }

  if (builtins & GPU_BARYCENTRIC_TEXCO) {
    BLI_dynstr_append(ds, "#ifdef HAIR_SHADER\n");
    BLI_dynstr_append(ds, "in vec2 barycentricTexCog[];\n");
    BLI_dynstr_append(ds, "#endif\n");

    BLI_dynstr_append(ds, "out vec2 barycentricTexCo;\n");
  }

  if (builtins & GPU_BARYCENTRIC_DIST) {
    BLI_dynstr_append(ds, "in vec3 barycentricPosg[];\n");
    BLI_dynstr_append(ds, "flat out vec3 barycentricDist;\n");
  }

  if (geom_code == NULL) {
    /* Force geometry usage if GPU_BARYCENTRIC_DIST or GPU_BARYCENTRIC_TEXCO are used.
     * Note: GPU_BARYCENTRIC_TEXCO only requires it if the shader is not drawing hairs. */
    if ((builtins & (GPU_BARYCENTRIC_DIST | GPU_BARYCENTRIC_TEXCO)) == 0 || is_hair_shader) {
      /* Early out */
      BLI_dynstr_free(ds);
      return NULL;
    }
    else {
      /* Force geom shader usage */
      /* TODO put in external file. */
      BLI_dynstr_append(ds, "layout(triangles) in;\n");
      BLI_dynstr_append(ds, "layout(triangle_strip, max_vertices=3) out;\n");

      BLI_dynstr_append(ds, "in vec3 worldPositiong[];\n");
      BLI_dynstr_append(ds, "in vec3 viewPositiong[];\n");
      BLI_dynstr_append(ds, "in vec3 worldNormalg[];\n");
      BLI_dynstr_append(ds, "in vec3 viewNormalg[];\n");

      BLI_dynstr_append(ds, "out vec3 worldPosition;\n");
      BLI_dynstr_append(ds, "out vec3 viewPosition;\n");
      BLI_dynstr_append(ds, "out vec3 worldNormal;\n");
      BLI_dynstr_append(ds, "out vec3 viewNormal;\n");

      BLI_dynstr_append(ds, datatoc_common_view_lib_glsl);

      BLI_dynstr_append(ds, "void main(){\n");

      if (builtins & GPU_BARYCENTRIC_DIST) {
        BLI_dynstr_append(ds,
                          "\tcalc_barycentric_distances(barycentricPosg[0], barycentricPosg[1], "
                          "barycentricPosg[2]);\n");
      }

      for (int i = 0; i < 3; i++) {
        BLI_dynstr_appendf(ds, "\tgl_Position = gl_in[%d].gl_Position;\n", i);
        BLI_dynstr_appendf(ds, "\tgl_ClipDistance[0] = gl_in[%d].gl_ClipDistance[0];\n", i);
        BLI_dynstr_appendf(ds, "\tpass_attr(%d);\n", i);
        BLI_dynstr_append(ds, "\tEmitVertex();\n");
      }
      BLI_dynstr_append(ds, "}\n");
    }
  }
  else {
    BLI_dynstr_append(ds, geom_code);
  }

  if (builtins & GPU_BARYCENTRIC_DIST) {
    BLI_dynstr_append(ds, "void calc_barycentric_distances(vec3 pos0, vec3 pos1, vec3 pos2) {\n");
    BLI_dynstr_append(ds, "\tvec3 edge21 = pos2 - pos1;\n");
    BLI_dynstr_append(ds, "\tvec3 edge10 = pos1 - pos0;\n");
    BLI_dynstr_append(ds, "\tvec3 edge02 = pos0 - pos2;\n");
    BLI_dynstr_append(ds, "\tvec3 d21 = normalize(edge21);\n");
    BLI_dynstr_append(ds, "\tvec3 d10 = normalize(edge10);\n");
    BLI_dynstr_append(ds, "\tvec3 d02 = normalize(edge02);\n");

    BLI_dynstr_append(ds, "\tfloat d = dot(d21, edge02);\n");
    BLI_dynstr_append(ds, "\tbarycentricDist.x = sqrt(dot(edge02, edge02) - d * d);\n");
    BLI_dynstr_append(ds, "\td = dot(d02, edge10);\n");
    BLI_dynstr_append(ds, "\tbarycentricDist.y = sqrt(dot(edge10, edge10) - d * d);\n");
    BLI_dynstr_append(ds, "\td = dot(d10, edge21);\n");
    BLI_dynstr_append(ds, "\tbarycentricDist.z = sqrt(dot(edge21, edge21) - d * d);\n");
    BLI_dynstr_append(ds, "}\n");
  }

  BLI_dynstr_append(ds, "RESOURCE_ID_VARYING\n");

  /* Generate varying assignments. */
  BLI_dynstr_append(ds, "void pass_attr(in int vert) {\n");

  BLI_dynstr_append(ds, "\tPASS_RESOURCE_ID(vert)\n");

  /* XXX HACK: Eevee specific. */
  if (geom_code == NULL) {
    BLI_dynstr_append(ds, "\tworldPosition = worldPositiong[vert];\n");
    BLI_dynstr_append(ds, "\tviewPosition = viewPositiong[vert];\n");
    BLI_dynstr_append(ds, "\tworldNormal = worldNormalg[vert];\n");
    BLI_dynstr_append(ds, "\tviewNormal = viewNormalg[vert];\n");
  }

  if (builtins & GPU_BARYCENTRIC_TEXCO) {
    BLI_dynstr_append(ds, "#ifdef HAIR_SHADER\n");
    BLI_dynstr_append(ds, "\tbarycentricTexCo = barycentricTexCog[vert];\n");
    BLI_dynstr_append(ds, "#else\n");
    BLI_dynstr_append(ds, "\tbarycentricTexCo.x = float((vert % 3) == 0);\n");
    BLI_dynstr_append(ds, "\tbarycentricTexCo.y = float((vert % 3) == 1);\n");
    BLI_dynstr_append(ds, "#endif\n");
  }

  LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &graph->attributes) {
    /* TODO let shader choose what to do depending on what the attribute is. */
    BLI_dynstr_appendf(ds, "\tvar%d = var%dg[vert];\n", attr->id, attr->id);
  }
  BLI_dynstr_append(ds, "}\n");

  code = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

  return code;
}

GPUShader *GPU_pass_shader_get(GPUPass *pass)
{
  return pass->shader;
}

/* Pass create/free */

static bool gpu_pass_is_valid(GPUPass *pass)
{
  /* Shader is not null if compilation is successful. */
  return (pass->compiled == false || pass->shader != NULL);
}

GPUPass *GPU_generate_pass(GPUMaterial *material,
                           GPUNodeGraph *graph,
                           const char *vert_code,
                           const char *geom_code,
                           const char *frag_lib,
                           const char *defines)
{
  /* Prune the unused nodes and extract attributes before compiling so the
   * generated VBOs are ready to accept the future shader. */
  gpu_node_graph_prune_unused(graph);

  /* generate code */
  char *fragmentgen = code_generate_fragment(material, graph);

  /* Cache lookup: Reuse shaders already compiled */
  uint32_t hash = gpu_pass_hash(fragmentgen, defines, &graph->attributes);
  GPUPass *pass_hash = gpu_pass_cache_lookup(hash);

  if (pass_hash && (pass_hash->next == NULL || pass_hash->next->hash != hash)) {
    /* No collision, just return the pass. */
    MEM_freeN(fragmentgen);
    if (!gpu_pass_is_valid(pass_hash)) {
      /* Shader has already been created but failed to compile. */
      return NULL;
    }
    pass_hash->refcount += 1;
    return pass_hash;
  }

  /* Either the shader is not compiled or there is a hash collision...
   * continue generating the shader strings. */
  GSet *used_libraries = gpu_material_used_libraries(material);
  char *tmp = gpu_material_library_generate_code(used_libraries, frag_lib);

  char *geometrycode = code_generate_geometry(graph, geom_code, defines);
  char *vertexcode = code_generate_vertex(graph, vert_code, (geometrycode != NULL));
  char *fragmentcode = BLI_strdupcat(tmp, fragmentgen);

  MEM_freeN(fragmentgen);
  MEM_freeN(tmp);

  GPUPass *pass = NULL;
  if (pass_hash) {
    /* Cache lookup: Reuse shaders already compiled */
    pass = gpu_pass_cache_resolve_collision(
        pass_hash, vertexcode, geometrycode, fragmentcode, defines, hash);
  }

  if (pass) {
    MEM_SAFE_FREE(vertexcode);
    MEM_SAFE_FREE(fragmentcode);
    MEM_SAFE_FREE(geometrycode);

    /* Cache hit. Reuse the same GPUPass and GPUShader. */
    if (!gpu_pass_is_valid(pass)) {
      /* Shader has already been created but failed to compile. */
      return NULL;
    }

    pass->refcount += 1;
  }
  else {
    /* We still create a pass even if shader compilation
     * fails to avoid trying to compile again and again. */
    pass = MEM_callocN(sizeof(GPUPass), "GPUPass");
    pass->shader = NULL;
    pass->refcount = 1;
    pass->hash = hash;
    pass->vertexcode = vertexcode;
    pass->fragmentcode = fragmentcode;
    pass->geometrycode = geometrycode;
    pass->defines = (defines) ? BLI_strdup(defines) : NULL;
    pass->compiled = false;

    BLI_spin_lock(&pass_cache_spin);
    if (pass_hash != NULL) {
      /* Add after the first pass having the same hash. */
      pass->next = pass_hash->next;
      pass_hash->next = pass;
    }
    else {
      /* No other pass have same hash, just prepend to the list. */
      BLI_LINKS_PREPEND(pass_cache, pass);
    }
    BLI_spin_unlock(&pass_cache_spin);
  }

  return pass;
}

static int count_active_texture_sampler(GPUShader *shader, char *source)
{
  char *code = source;

  /* Remember this is per stage. */
  GSet *sampler_ids = BLI_gset_int_new(__func__);
  int num_samplers = 0;

  while ((code = strstr(code, "uniform "))) {
    /* Move past "uniform". */
    code += 7;
    /* Skip following spaces. */
    while (*code == ' ') {
      code++;
    }
    /* Skip "i" from potential isamplers. */
    if (*code == 'i') {
      code++;
    }
    /* Skip following spaces. */
    if (BLI_str_startswith(code, "sampler")) {
      /* Move past "uniform". */
      code += 7;
      /* Skip sampler type suffix. */
      while (*code != ' ' && *code != '\0') {
        code++;
      }
      /* Skip following spaces. */
      while (*code == ' ') {
        code++;
      }

      if (*code != '\0') {
        char sampler_name[64];
        code = gpu_str_skip_token(code, sampler_name, sizeof(sampler_name));
        int id = GPU_shader_get_uniform(shader, sampler_name);

        if (id == -1) {
          continue;
        }
        /* Catch duplicates. */
        if (BLI_gset_add(sampler_ids, POINTER_FROM_INT(id))) {
          num_samplers++;
        }
      }
    }
  }

  BLI_gset_free(sampler_ids, NULL);

  return num_samplers;
}

static bool gpu_pass_shader_validate(GPUPass *pass, GPUShader *shader)
{
  if (shader == NULL) {
    return false;
  }

  /* NOTE: The only drawback of this method is that it will count a sampler
   * used in the fragment shader and only declared (but not used) in the vertex
   * shader as used by both. But this corner case is not happening for now. */
  int vert_samplers_len = count_active_texture_sampler(shader, pass->vertexcode);
  int frag_samplers_len = count_active_texture_sampler(shader, pass->fragmentcode);

  int total_samplers_len = vert_samplers_len + frag_samplers_len;

  /* Validate against opengl limit. */
  if ((frag_samplers_len > GPU_max_textures_frag()) ||
      (vert_samplers_len > GPU_max_textures_vert())) {
    return false;
  }

  if (pass->geometrycode) {
    int geom_samplers_len = count_active_texture_sampler(shader, pass->geometrycode);
    total_samplers_len += geom_samplers_len;
    if (geom_samplers_len > GPU_max_textures_geom()) {
      return false;
    }
  }

  return (total_samplers_len <= GPU_max_textures());
}

bool GPU_pass_compile(GPUPass *pass, const char *shname)
{
  bool success = true;
  if (!pass->compiled) {
    GPUShader *shader = GPU_shader_create(
        pass->vertexcode, pass->fragmentcode, pass->geometrycode, NULL, pass->defines, shname);

    /* NOTE: Some drivers / gpu allows more active samplers than the opengl limit.
     * We need to make sure to count active samplers to avoid undefined behavior. */
    if (!gpu_pass_shader_validate(pass, shader)) {
      success = false;
      if (shader != NULL) {
        fprintf(stderr, "GPUShader: error: too many samplers in shader.\n");
        GPU_shader_free(shader);
        shader = NULL;
      }
    }
    else if (!BLI_thread_is_main() && GPU_context_local_shaders_workaround()) {
      pass->binary.content = GPU_shader_get_binary(
          shader, &pass->binary.format, &pass->binary.len);
      GPU_shader_free(shader);
      shader = NULL;
    }

    pass->shader = shader;
    pass->compiled = true;
  }
  else if (pass->binary.content && BLI_thread_is_main()) {
    pass->shader = GPU_shader_load_from_binary(
        pass->binary.content, pass->binary.format, pass->binary.len, shname);
    MEM_SAFE_FREE(pass->binary.content);
  }

  return success;
}

void GPU_pass_release(GPUPass *pass)
{
  BLI_assert(pass->refcount > 0);
  pass->refcount--;
}

static void gpu_pass_free(GPUPass *pass)
{
  BLI_assert(pass->refcount == 0);
  if (pass->shader) {
    GPU_shader_free(pass->shader);
  }
  MEM_SAFE_FREE(pass->fragmentcode);
  MEM_SAFE_FREE(pass->geometrycode);
  MEM_SAFE_FREE(pass->vertexcode);
  MEM_SAFE_FREE(pass->defines);
  if (pass->binary.content) {
    MEM_freeN(pass->binary.content);
  }
  MEM_freeN(pass);
}

void GPU_pass_cache_garbage_collect(void)
{
  static int lasttime = 0;
  const int shadercollectrate = 60; /* hardcoded for now. */
  int ctime = (int)PIL_check_seconds_timer();

  if (ctime < shadercollectrate + lasttime) {
    return;
  }

  lasttime = ctime;

  BLI_spin_lock(&pass_cache_spin);
  GPUPass *next, **prev_pass = &pass_cache;
  for (GPUPass *pass = pass_cache; pass; pass = next) {
    next = pass->next;
    if (pass->refcount == 0) {
      /* Remove from list */
      *prev_pass = next;
      gpu_pass_free(pass);
    }
    else {
      prev_pass = &pass->next;
    }
  }
  BLI_spin_unlock(&pass_cache_spin);
}

void GPU_pass_cache_init(void)
{
  BLI_spin_init(&pass_cache_spin);
}

void GPU_pass_cache_free(void)
{
  BLI_spin_lock(&pass_cache_spin);
  while (pass_cache) {
    GPUPass *next = pass_cache->next;
    gpu_pass_free(pass_cache);
    pass_cache = next;
  }
  BLI_spin_unlock(&pass_cache_spin);

  BLI_spin_end(&pass_cache_spin);
}

/* Module */

void gpu_codegen_init(void)
{
}

void gpu_codegen_exit(void)
{
  BKE_material_defaults_free_gpu();
  GPU_shader_free_builtin_shaders();
}
