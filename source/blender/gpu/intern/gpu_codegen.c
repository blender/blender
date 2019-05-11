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
#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "BLI_blenlib.h"
#include "BLI_hash_mm2a.h"
#include "BLI_link_utils.h"
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "PIL_time.h"

#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniformbuffer.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "gpu_codegen.h"

#include <string.h>
#include <stdarg.h>

extern char datatoc_gpu_shader_material_glsl[];
extern char datatoc_gpu_shader_geometry_glsl[];

static char *glsl_material_library = NULL;

/* -------------------- GPUPass Cache ------------------ */
/**
 * Internal shader cache: This prevent the shader recompilation / stall when
 * using undo/redo AND also allows for GPUPass reuse if the Shader code is the
 * same for 2 different Materials. Unused GPUPasses are free by Garbage collection.
 */

/* Only use one linklist that contains the GPUPasses grouped by hash. */
static GPUPass *pass_cache = NULL;
static SpinLock pass_cache_spin;

static uint32_t gpu_pass_hash(const char *frag_gen, const char *defs, GPUVertAttrLayers *attrs)
{
  BLI_HashMurmur2A hm2a;
  BLI_hash_mm2a_init(&hm2a, 0);
  BLI_hash_mm2a_add(&hm2a, (uchar *)frag_gen, strlen(frag_gen));
  if (attrs) {
    for (int att_idx = 0; att_idx < attrs->totlayer; att_idx++) {
      char *name = attrs->layer[att_idx].name;
      BLI_hash_mm2a_add(&hm2a, (uchar *)name, strlen(name));
    }
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

/* -------------------- GPU Codegen ------------------ */

/* type definitions and constants */

#define MAX_FUNCTION_NAME 64
#define MAX_PARAMETER 32

typedef enum {
  FUNCTION_QUAL_IN,
  FUNCTION_QUAL_OUT,
  FUNCTION_QUAL_INOUT,
} GPUFunctionQual;

typedef struct GPUFunction {
  char name[MAX_FUNCTION_NAME];
  eGPUType paramtype[MAX_PARAMETER];
  GPUFunctionQual paramqual[MAX_PARAMETER];
  int totparam;
} GPUFunction;

/* Indices match the eGPUType enum */
static const char *GPU_DATATYPE_STR[17] = {
    "",
    "float",
    "vec2",
    "vec3",
    "vec4",
    NULL,
    NULL,
    NULL,
    NULL,
    "mat3",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "mat4",
};

/* GLSL code parsing for finding function definitions.
 * These are stored in a hash for lookup when creating a material. */

static GHash *FUNCTION_HASH = NULL;
#if 0
static char *FUNCTION_PROTOTYPES = NULL;
static GPUShader *FUNCTION_LIB = NULL;
#endif

static int gpu_str_prefix(const char *str, const char *prefix)
{
  while (*str && *prefix) {
    if (*str != *prefix) {
      return 0;
    }

    str++;
    prefix++;
  }

  return (*prefix == '\0');
}

static char *gpu_str_skip_token(char *str, char *token, int max)
{
  int len = 0;

  /* skip a variable/function name */
  while (*str) {
    if (ELEM(*str, ' ', '(', ')', ',', ';', '\t', '\n', '\r')) {
      break;
    }
    else {
      if (token && len < max - 1) {
        *token = *str;
        token++;
        len++;
      }
      str++;
    }
  }

  if (token) {
    *token = '\0';
  }

  /* skip the next special characters:
   * note the missing ')' */
  while (*str) {
    if (ELEM(*str, ' ', '(', ',', ';', '\t', '\n', '\r')) {
      str++;
    }
    else {
      break;
    }
  }

  return str;
}

static void gpu_parse_functions_string(GHash *hash, char *code)
{
  GPUFunction *function;
  eGPUType type;
  GPUFunctionQual qual;
  int i;

  while ((code = strstr(code, "void "))) {
    function = MEM_callocN(sizeof(GPUFunction), "GPUFunction");

    code = gpu_str_skip_token(code, NULL, 0);
    code = gpu_str_skip_token(code, function->name, MAX_FUNCTION_NAME);

    /* get parameters */
    while (*code && *code != ')') {
      /* test if it's an input or output */
      qual = FUNCTION_QUAL_IN;
      if (gpu_str_prefix(code, "out ")) {
        qual = FUNCTION_QUAL_OUT;
      }
      if (gpu_str_prefix(code, "inout ")) {
        qual = FUNCTION_QUAL_INOUT;
      }
      if ((qual != FUNCTION_QUAL_IN) || gpu_str_prefix(code, "in ")) {
        code = gpu_str_skip_token(code, NULL, 0);
      }

      /* test for type */
      type = GPU_NONE;
      for (i = 1; i < ARRAY_SIZE(GPU_DATATYPE_STR); i++) {
        if (GPU_DATATYPE_STR[i] && gpu_str_prefix(code, GPU_DATATYPE_STR[i])) {
          type = i;
          break;
        }
      }

      if (!type && gpu_str_prefix(code, "samplerCube")) {
        type = GPU_TEXCUBE;
      }
      if (!type && gpu_str_prefix(code, "sampler2DShadow")) {
        type = GPU_SHADOW2D;
      }
      if (!type && gpu_str_prefix(code, "sampler1DArray")) {
        type = GPU_TEX1D_ARRAY;
      }
      if (!type && gpu_str_prefix(code, "sampler2D")) {
        type = GPU_TEX2D;
      }
      if (!type && gpu_str_prefix(code, "sampler3D")) {
        type = GPU_TEX3D;
      }

      if (!type && gpu_str_prefix(code, "Closure")) {
        type = GPU_CLOSURE;
      }

      if (type) {
        /* add parameter */
        code = gpu_str_skip_token(code, NULL, 0);
        code = gpu_str_skip_token(code, NULL, 0);
        function->paramqual[function->totparam] = qual;
        function->paramtype[function->totparam] = type;
        function->totparam++;
      }
      else {
        fprintf(stderr, "GPU invalid function parameter in %s.\n", function->name);
        break;
      }
    }

    if (function->name[0] == '\0' || function->totparam == 0) {
      fprintf(stderr, "GPU functions parse error.\n");
      MEM_freeN(function);
      break;
    }

    BLI_ghash_insert(hash, function->name, function);
  }
}

#if 0
static char *gpu_generate_function_prototyps(GHash *hash)
{
  DynStr *ds = BLI_dynstr_new();
  GHashIterator *ghi;
  GPUFunction *function;
  char *name, *prototypes;
  int a;

  /* automatically generate function prototypes to add to the top of the
   * generated code, to avoid have to add the actual code & recompile all */
  ghi = BLI_ghashIterator_new(hash);

  for (; !BLI_ghashIterator_done(ghi); BLI_ghashIterator_step(ghi)) {
    name = BLI_ghashIterator_getValue(ghi);
    function = BLI_ghashIterator_getValue(ghi);

    BLI_dynstr_appendf(ds, "void %s(", name);
    for (a = 0; a < function->totparam; a++) {
      if (function->paramqual[a] == FUNCTION_QUAL_OUT)
        BLI_dynstr_append(ds, "out ");
      else if (function->paramqual[a] == FUNCTION_QUAL_INOUT)
        BLI_dynstr_append(ds, "inout ");

      if (function->paramtype[a] == GPU_TEX2D)
        BLI_dynstr_append(ds, "sampler2D");
      else if (function->paramtype[a] == GPU_SHADOW2D)
        BLI_dynstr_append(ds, "sampler2DShadow");
      else
        BLI_dynstr_append(ds, GPU_DATATYPE_STR[function->paramtype[a]]);
#  if 0
      BLI_dynstr_appendf(ds, " param%d", a);
#  endif

      if (a != function->totparam - 1)
        BLI_dynstr_append(ds, ", ");
    }
    BLI_dynstr_append(ds, ");\n");
  }

  BLI_dynstr_append(ds, "\n");

  prototypes = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

  return prototypes;
}
#endif

static GPUFunction *gpu_lookup_function(const char *name)
{
  if (!FUNCTION_HASH) {
    FUNCTION_HASH = BLI_ghash_str_new("GPU_lookup_function gh");
    gpu_parse_functions_string(FUNCTION_HASH, glsl_material_library);
  }

  return BLI_ghash_lookup(FUNCTION_HASH, (const void *)name);
}

void gpu_codegen_init(void)
{
  GPU_code_generate_glsl_lib();
}

void gpu_codegen_exit(void)
{
  extern Material defmaterial; /* render module abuse... */

  if (defmaterial.gpumaterial.first) {
    GPU_material_free(&defmaterial.gpumaterial);
  }

  if (FUNCTION_HASH) {
    BLI_ghash_free(FUNCTION_HASH, NULL, MEM_freeN);
    FUNCTION_HASH = NULL;
  }

  GPU_shader_free_builtin_shaders();

  if (glsl_material_library) {
    MEM_freeN(glsl_material_library);
    glsl_material_library = NULL;
  }

#if 0
  if (FUNCTION_PROTOTYPES) {
    MEM_freeN(FUNCTION_PROTOTYPES);
    FUNCTION_PROTOTYPES = NULL;
  }
  if (FUNCTION_LIB) {
    GPU_shader_free(FUNCTION_LIB);
    FUNCTION_LIB = NULL;
  }
#endif
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
      BLI_dynstr_appendf(ds, "convert_rgba_to_float(%s)", name);
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

  BLI_dynstr_appendf(ds, "%s(", GPU_DATATYPE_STR[type]);

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

static int codegen_input_has_texture(GPUInput *input)
{
  if (input->link) {
    return 0;
  }
  else {
    return (input->source == GPU_SOURCE_TEX);
  }
}

const char *GPU_builtin_name(eGPUBuiltin builtin)
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
  else if (builtin == GPU_OBCOLOR) {
    return "unfobcolor";
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
  else if (builtin == GPU_VOLUME_DENSITY) {
    return "sampdensity";
  }
  else if (builtin == GPU_VOLUME_FLAME) {
    return "sampflame";
  }
  else if (builtin == GPU_VOLUME_TEMPERATURE) {
    return "unftemperature";
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

/* assign only one texid per buffer to avoid sampling the same texture twice */
static void codegen_set_texid(GHash *bindhash, GPUInput *input, int *texid, void *key)
{
  if (BLI_ghash_haskey(bindhash, key)) {
    /* Reuse existing texid */
    input->texid = POINTER_AS_INT(BLI_ghash_lookup(bindhash, key));
  }
  else {
    /* Allocate new texid */
    input->texid = *texid;
    (*texid)++;
    input->bindtex = true;
    BLI_ghash_insert(bindhash, key, POINTER_FROM_INT(input->texid));
  }
}

static void codegen_set_unique_ids(ListBase *nodes)
{
  GHash *bindhash;
  GPUNode *node;
  GPUInput *input;
  GPUOutput *output;
  int id = 1, texid = 0;

  bindhash = BLI_ghash_ptr_new("codegen_set_unique_ids1 gh");

  for (node = nodes->first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      /* set id for unique names of uniform variables */
      input->id = id++;

      /* set texid used for settings texture slot */
      if (codegen_input_has_texture(input)) {
        input->bindtex = false;
        if (input->ima) {
          /* input is texture from image */
          codegen_set_texid(bindhash, input, &texid, input->ima);
        }
        else if (input->coba) {
          /* input is color band texture, check coba pointer */
          codegen_set_texid(bindhash, input, &texid, input->coba);
        }
        else {
          /* Either input->ima or input->coba should be non-NULL. */
          BLI_assert(0);
        }
      }
    }

    for (output = node->outputs.first; output; output = output->next) {
      /* set id for unique names of tmp variables storing output */
      output->id = id++;
    }
  }

  BLI_ghash_free(bindhash, NULL, NULL);
}

/**
 * It will create an UBO for GPUMaterial if there is any GPU_DYNAMIC_UBO.
 */
static int codegen_process_uniforms_functions(GPUMaterial *material, DynStr *ds, ListBase *nodes)
{
  GPUNode *node;
  GPUInput *input;
  const char *name;
  int builtins = 0;
  ListBase ubo_inputs = {NULL, NULL};

  /* print uniforms */
  for (node = nodes->first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_TEX) {
        /* create exactly one sampler for each texture */
        if (codegen_input_has_texture(input) && input->bindtex) {
          BLI_dynstr_appendf(ds,
                             "uniform %s samp%d;\n",
                             (input->coba) ? "sampler1DArray" : "sampler2D",
                             input->texid);
        }
      }
      else if (input->source == GPU_SOURCE_BUILTIN) {
        /* only define each builtin uniform/varying once */
        if (!(builtins & input->builtin)) {
          builtins |= input->builtin;
          name = GPU_builtin_name(input->builtin);

          if (gpu_str_prefix(name, "samp")) {
            if ((input->builtin == GPU_VOLUME_DENSITY) || (input->builtin == GPU_VOLUME_FLAME)) {
              BLI_dynstr_appendf(ds, "uniform sampler3D %s;\n", name);
            }
          }
          else if (gpu_str_prefix(name, "unf")) {
            BLI_dynstr_appendf(ds, "uniform %s %s;\n", GPU_DATATYPE_STR[input->type], name);
          }
          else {
            BLI_dynstr_appendf(ds, "in %s %s;\n", GPU_DATATYPE_STR[input->type], name);
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
        BLI_dynstr_appendf(ds, "const %s cons%d = ", GPU_DATATYPE_STR[input->type], input->id);
        codegen_print_datatype(ds, input->type, input->vec);
        BLI_dynstr_append(ds, ";\n");
      }
      else if (input->source == GPU_SOURCE_ATTR && input->attr_first) {
        BLI_dynstr_appendf(ds, "in %s var%d;\n", GPU_DATATYPE_STR[input->type], input->attr_id);
      }
    }
  }

  /* Handle the UBO block separately. */
  if ((material != NULL) && !BLI_listbase_is_empty(&ubo_inputs)) {
    GPU_material_uniform_buffer_create(material, &ubo_inputs);

    /* Inputs are sorted */
    BLI_dynstr_appendf(ds, "\nlayout (std140) uniform %s {\n", GPU_UBO_BLOCK_NAME);

    for (LinkData *link = ubo_inputs.first; link; link = link->next) {
      input = link->data;
      BLI_dynstr_appendf(ds, "\t%s unf%d;\n", GPU_DATATYPE_STR[input->type], input->id);
    }
    BLI_dynstr_append(ds, "};\n");
    BLI_freelistN(&ubo_inputs);
  }

  BLI_dynstr_append(ds, "\n");

  return builtins;
}

static void codegen_declare_tmps(DynStr *ds, ListBase *nodes)
{
  GPUNode *node;
  GPUOutput *output;

  for (node = nodes->first; node; node = node->next) {
    /* declare temporary variables for node output storage */
    for (output = node->outputs.first; output; output = output->next) {
      if (output->type == GPU_CLOSURE) {
        BLI_dynstr_appendf(ds, "\tClosure tmp%d;\n", output->id);
      }
      else {
        BLI_dynstr_appendf(ds, "\t%s tmp%d;\n", GPU_DATATYPE_STR[output->type], output->id);
      }
    }
  }

  BLI_dynstr_append(ds, "\n");
}

static void codegen_call_functions(DynStr *ds, ListBase *nodes, GPUOutput *finaloutput)
{
  GPUNode *node;
  GPUInput *input;
  GPUOutput *output;

  for (node = nodes->first; node; node = node->next) {
    BLI_dynstr_appendf(ds, "\t%s(", node->name);

    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_TEX) {
        BLI_dynstr_appendf(ds, "samp%d", input->texid);
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
          BLI_dynstr_append(ds, GPU_builtin_name(input->builtin));
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
        BLI_dynstr_appendf(ds, "var%d", input->attr_id);
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

static char *code_generate_fragment(GPUMaterial *material,
                                    ListBase *nodes,
                                    GPUOutput *output,
                                    int *rbuiltins)
{
  DynStr *ds = BLI_dynstr_new();
  char *code;
  int builtins;

#if 0
  BLI_dynstr_append(ds, FUNCTION_PROTOTYPES);
#endif

  codegen_set_unique_ids(nodes);
  *rbuiltins = builtins = codegen_process_uniforms_functions(material, ds, nodes);

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

  codegen_declare_tmps(ds, nodes);
  codegen_call_functions(ds, nodes, output);

  BLI_dynstr_append(ds, "}\n");

  /* XXX This cannot go into gpu_shader_material.glsl because main()
   * would be parsed and generate error */
  /* Old glsl mode compat. */
  BLI_dynstr_append(ds, "#ifndef NODETREE_EXEC\n");
  BLI_dynstr_append(ds, "out vec4 fragColor;\n");
  BLI_dynstr_append(ds, "void main()\n");
  BLI_dynstr_append(ds, "{\n");
  BLI_dynstr_append(ds, "\tClosure cl = nodetree_exec();\n");
  BLI_dynstr_append(ds, "\tfragColor = vec4(cl.radiance, cl.opacity);\n");
  BLI_dynstr_append(ds, "}\n");
  BLI_dynstr_append(ds, "#endif\n\n");

  /* create shader */
  code = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

#if 0
  if (G.debug & G_DEBUG)
    printf("%s\n", code);
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

static char *code_generate_vertex(ListBase *nodes, const char *vert_code, bool use_geom)
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

  for (node = nodes->first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_BUILTIN) {
        builtins |= input->builtin;
      }
      if (input->source == GPU_SOURCE_ATTR && input->attr_first) {
        /* XXX FIXME : see notes in mesh_render_data_create() */
        /* NOTE : Replicate changes to mesh_render_data_create() in draw_cache_impl_mesh.c */
        if (input->attr_type == CD_ORCO) {
          /* OPTI : orco is computed from local positions, but only if no modifier is present. */
          BLI_dynstr_append(ds, "uniform vec3 OrcoTexCoFactors[2];\n");
          BLI_dynstr_append(ds, "DEFINE_ATTR(vec4, orco);\n");
        }
        else if (input->attr_name[0] == '\0') {
          BLI_dynstr_appendf(ds,
                             "DEFINE_ATTR(%s, %s);\n",
                             GPU_DATATYPE_STR[input->type],
                             attr_prefix_get(input->attr_type));
          BLI_dynstr_appendf(
              ds, "#define att%d %s\n", input->attr_id, attr_prefix_get(input->attr_type));
        }
        else {
          uint hash = BLI_ghashutil_strhash_p(input->attr_name);
          BLI_dynstr_appendf(ds,
                             "DEFINE_ATTR(%s, %s%u);\n",
                             GPU_DATATYPE_STR[input->type],
                             attr_prefix_get(input->attr_type),
                             hash);
          BLI_dynstr_appendf(
              ds, "#define att%d %s%u\n", input->attr_id, attr_prefix_get(input->attr_type), hash);
          /* Auto attribute can be vertex color byte buffer.
           * We need to know and convert them to linear space in VS. */
          if (input->attr_type == CD_AUTO_FROM_NAME) {
            BLI_dynstr_appendf(ds, "uniform bool ba%u;\n", hash);
            BLI_dynstr_appendf(ds, "#define att%d_is_srgb ba%u\n", input->attr_id, hash);
          }
        }
        BLI_dynstr_appendf(ds,
                           "out %s var%d%s;\n",
                           GPU_DATATYPE_STR[input->type],
                           input->attr_id,
                           use_geom ? "g" : "");
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

  BLI_dynstr_append(ds, "\n");

  BLI_dynstr_append(ds,
                    "#define USE_ATTR\n"
                    "uniform mat4 ModelMatrixInverse;\n"
                    "uniform mat4 ModelMatrix;\n"
                    "vec3 srgb_to_linear_attr(vec3 c) {\n"
                    "\tc = max(c, vec3(0.0));\n"
                    "\tvec3 c1 = c * (1.0 / 12.92);\n"
                    "\tvec3 c2 = pow((c + 0.055) * (1.0 / 1.055), vec3(2.4));\n"
                    "\treturn mix(c1, c2, step(vec3(0.04045), c));\n"
                    "}\n\n");

  /* Prototype because defined later. */
  BLI_dynstr_append(ds,
                    "vec2 hair_get_customdata_vec2(const samplerBuffer);\n"
                    "vec3 hair_get_customdata_vec3(const samplerBuffer);\n"
                    "vec4 hair_get_customdata_vec4(const samplerBuffer);\n"
                    "vec3 hair_get_strand_pos(void);\n"
                    "int hair_get_base_id(void);\n"
                    "\n");

  BLI_dynstr_append(ds, "void pass_attr(in vec3 position) {\n");

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

  for (node = nodes->first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_ATTR && input->attr_first) {
        if (input->attr_type == CD_TANGENT) {
          /* Not supported by hairs */
          BLI_dynstr_appendf(ds, "\tvar%d%s = vec4(0.0);\n", input->attr_id, use_geom ? "g" : "");
        }
        else if (input->attr_type == CD_ORCO) {
          BLI_dynstr_appendf(ds,
                             "\tvar%d%s = OrcoTexCoFactors[0] + (ModelMatrixInverse * "
                             "vec4(hair_get_strand_pos(), 1.0)).xyz * OrcoTexCoFactors[1];\n",
                             input->attr_id,
                             use_geom ? "g" : "");
          /* TODO: fix ORCO with modifiers. */
        }
        else {
          BLI_dynstr_appendf(ds,
                             "\tvar%d%s = hair_get_customdata_%s(att%d);\n",
                             input->attr_id,
                             use_geom ? "g" : "",
                             GPU_DATATYPE_STR[input->type],
                             input->attr_id);
        }
      }
    }
  }

  BLI_dynstr_append(ds, "#else /* MESH_SHADER */\n");

  /* GPU_BARYCENTRIC_TEXCO cannot be computed based on gl_VertexID
   * for MESH_SHADER because of indexed drawing. In this case a
   * geometry shader is needed. */

  if (builtins & GPU_BARYCENTRIC_DIST) {
    BLI_dynstr_append(ds, "\tbarycentricPosg = (ModelMatrix * vec4(position, 1.0)).xyz;\n");
  }

  for (node = nodes->first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_ATTR && input->attr_first) {
        if (input->attr_type == CD_TANGENT) { /* silly exception */
          BLI_dynstr_appendf(ds,
                             "\tvar%d%s.xyz = transpose(mat3(ModelMatrixInverse)) * att%d.xyz;\n",
                             input->attr_id,
                             use_geom ? "g" : "",
                             input->attr_id);
          BLI_dynstr_appendf(
              ds, "\tvar%d%s.w = att%d.w;\n", input->attr_id, use_geom ? "g" : "", input->attr_id);
          /* Normalize only if vector is not null. */
          BLI_dynstr_appendf(ds,
                             "\tfloat lvar%d = dot(var%d%s.xyz, var%d%s.xyz);\n",
                             input->attr_id,
                             input->attr_id,
                             use_geom ? "g" : "",
                             input->attr_id,
                             use_geom ? "g" : "");
          BLI_dynstr_appendf(ds,
                             "\tvar%d%s.xyz *= (lvar%d > 0.0) ? inversesqrt(lvar%d) : 1.0;\n",
                             input->attr_id,
                             use_geom ? "g" : "",
                             input->attr_id,
                             input->attr_id);
        }
        else if (input->attr_type == CD_ORCO) {
          BLI_dynstr_appendf(ds,
                             "\tvar%d%s = OrcoTexCoFactors[0] + position * OrcoTexCoFactors[1];\n",
                             input->attr_id,
                             use_geom ? "g" : "");
          /* See mesh_create_loop_orco() for explanation. */
          BLI_dynstr_appendf(ds,
                             "\tif (orco.w == 0.0) { var%d%s = orco.xyz * 0.5 + 0.5; }\n",
                             input->attr_id,
                             use_geom ? "g" : "");
        }
        else if (input->attr_type == CD_MCOL) {
          BLI_dynstr_appendf(ds,
                             "\tvar%d%s = srgb_to_linear_attr(att%d);\n",
                             input->attr_id,
                             use_geom ? "g" : "",
                             input->attr_id);
        }
        else if (input->attr_type == CD_AUTO_FROM_NAME) {
          BLI_dynstr_appendf(ds,
                             "\tvar%d%s = (att%d_is_srgb) ? srgb_to_linear_attr(att%d) : att%d;\n",
                             input->attr_id,
                             use_geom ? "g" : "",
                             input->attr_id,
                             input->attr_id,
                             input->attr_id);
        }
        else {
          BLI_dynstr_appendf(
              ds, "\tvar%d%s = att%d;\n", input->attr_id, use_geom ? "g" : "", input->attr_id);
        }
      }
    }
  }
  BLI_dynstr_append(ds, "#endif /* HAIR_SHADER */\n");

  BLI_dynstr_append(ds, "}\n");

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

  code = BLI_dynstr_get_cstring(ds);

  BLI_dynstr_free(ds);

#if 0
  if (G.debug & G_DEBUG)
    printf("%s\n", code);
#endif

  return code;
}

static char *code_generate_geometry(ListBase *nodes, const char *geom_code, const char *defines)
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
  for (node = nodes->first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_BUILTIN) {
        builtins |= input->builtin;
      }
      if (input->source == GPU_SOURCE_ATTR && input->attr_first) {
        BLI_dynstr_appendf(ds, "in %s var%dg[];\n", GPU_DATATYPE_STR[input->type], input->attr_id);
        BLI_dynstr_appendf(ds, "out %s var%d;\n", GPU_DATATYPE_STR[input->type], input->attr_id);
      }
    }
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

      BLI_dynstr_append(ds, "void main(){\n");

      if (builtins & GPU_BARYCENTRIC_DIST) {
        BLI_dynstr_append(ds,
                          "\tcalc_barycentric_distances(barycentricPosg[0], barycentricPosg[1], "
                          "barycentricPosg[2]);\n");
      }

      BLI_dynstr_append(ds, "\tgl_Position = gl_in[0].gl_Position;\n");
      BLI_dynstr_append(ds, "\tpass_attr(0);\n");
      BLI_dynstr_append(ds, "\tEmitVertex();\n");

      BLI_dynstr_append(ds, "\tgl_Position = gl_in[1].gl_Position;\n");
      BLI_dynstr_append(ds, "\tpass_attr(1);\n");
      BLI_dynstr_append(ds, "\tEmitVertex();\n");

      BLI_dynstr_append(ds, "\tgl_Position = gl_in[2].gl_Position;\n");
      BLI_dynstr_append(ds, "\tpass_attr(2);\n");
      BLI_dynstr_append(ds, "\tEmitVertex();\n");
      BLI_dynstr_append(ds, "};\n");
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

  /* Generate varying assignments. */
  BLI_dynstr_append(ds, "void pass_attr(in int vert) {\n");

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

  for (node = nodes->first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_ATTR && input->attr_first) {
        /* TODO let shader choose what to do depending on what the attribute is. */
        BLI_dynstr_appendf(ds, "\tvar%d = var%dg[vert];\n", input->attr_id, input->attr_id);
      }
    }
  }
  BLI_dynstr_append(ds, "}\n");

  code = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

  return code;
}

void GPU_code_generate_glsl_lib(void)
{
  DynStr *ds;

  /* only initialize the library once */
  if (glsl_material_library) {
    return;
  }

  ds = BLI_dynstr_new();

  BLI_dynstr_append(ds, datatoc_gpu_shader_material_glsl);

  glsl_material_library = BLI_dynstr_get_cstring(ds);

  BLI_dynstr_free(ds);
}

/* GPU pass binding/unbinding */

GPUShader *GPU_pass_shader_get(GPUPass *pass)
{
  return pass->shader;
}

void GPU_nodes_extract_dynamic_inputs(GPUShader *shader, ListBase *inputs, ListBase *nodes)
{
  GPUNode *node;
  GPUInput *next, *input;

  BLI_listbase_clear(inputs);

  if (!shader) {
    return;
  }

  for (node = nodes->first; node; node = node->next) {
    int z = 0;
    for (input = node->inputs.first; input; input = next, z++) {
      next = input->next;

      /* attributes don't need to be bound, they already have
       * an id that the drawing functions will use. Builtins have
       * constant names. */
      if (ELEM(input->source, GPU_SOURCE_ATTR, GPU_SOURCE_BUILTIN)) {
        continue;
      }

      if (input->source == GPU_SOURCE_TEX) {
        BLI_snprintf(input->shadername, sizeof(input->shadername), "samp%d", input->texid);
      }
      else {
        BLI_snprintf(input->shadername, sizeof(input->shadername), "unf%d", input->id);
      }

      if (input->source == GPU_SOURCE_TEX) {
        if (input->bindtex) {
          input->shaderloc = GPU_shader_get_uniform_ensure(shader, input->shadername);
          /* extract nodes */
          BLI_remlink(&node->inputs, input);
          BLI_addtail(inputs, input);
        }
      }
    }
  }
}

/* Node Link Functions */

static GPUNodeLink *GPU_node_link_create(void)
{
  GPUNodeLink *link = MEM_callocN(sizeof(GPUNodeLink), "GPUNodeLink");
  link->users++;

  return link;
}

static void gpu_node_link_free(GPUNodeLink *link)
{
  link->users--;

  if (link->users < 0) {
    fprintf(stderr, "GPU_node_link_free: negative refcount\n");
  }

  if (link->users == 0) {
    if (link->output) {
      link->output->link = NULL;
    }
    MEM_freeN(link);
  }
}

/* Node Functions */

static GPUNode *GPU_node_begin(const char *name)
{
  GPUNode *node = MEM_callocN(sizeof(GPUNode), "GPUNode");

  node->name = name;

  return node;
}

static void gpu_node_input_link(GPUNode *node, GPUNodeLink *link, const eGPUType type)
{
  GPUInput *input;
  GPUNode *outnode;
  const char *name;

  if (link->link_type == GPU_NODE_LINK_OUTPUT) {
    outnode = link->output->node;
    name = outnode->name;
    input = outnode->inputs.first;

    if ((STR_ELEM(name, "set_value", "set_rgb", "set_rgba")) && (input->type == type)) {
      input = MEM_dupallocN(outnode->inputs.first);
      if (input->link) {
        input->link->users++;
      }
      BLI_addtail(&node->inputs, input);
      return;
    }
  }

  input = MEM_callocN(sizeof(GPUInput), "GPUInput");
  input->node = node;
  input->type = type;

  switch (link->link_type) {
    case GPU_NODE_LINK_BUILTIN:
      input->source = GPU_SOURCE_BUILTIN;
      input->builtin = link->builtin;
      break;
    case GPU_NODE_LINK_OUTPUT:
      input->source = GPU_SOURCE_OUTPUT;
      input->link = link;
      link->users++;
      break;
    case GPU_NODE_LINK_COLORBAND:
      input->source = GPU_SOURCE_TEX;
      input->coba = link->coba;
      break;
    case GPU_NODE_LINK_IMAGE_BLENDER:
      input->source = GPU_SOURCE_TEX;
      input->ima = link->ima;
      input->iuser = link->iuser;
      break;
    case GPU_NODE_LINK_ATTR:
      input->source = GPU_SOURCE_ATTR;
      input->attr_type = link->attr_type;
      BLI_strncpy(input->attr_name, link->attr_name, sizeof(input->attr_name));
      break;
    case GPU_NODE_LINK_CONSTANT:
      input->source = (type == GPU_CLOSURE) ? GPU_SOURCE_STRUCT : GPU_SOURCE_CONSTANT;
      break;
    case GPU_NODE_LINK_UNIFORM:
      input->source = GPU_SOURCE_UNIFORM;
      break;
    default:
      break;
  }

  if (ELEM(input->source, GPU_SOURCE_CONSTANT, GPU_SOURCE_UNIFORM)) {
    memcpy(input->vec, link->data, type * sizeof(float));
  }

  if (link->link_type != GPU_NODE_LINK_OUTPUT) {
    MEM_freeN(link);
  }
  BLI_addtail(&node->inputs, input);
}

static const char *gpu_uniform_set_function_from_type(eNodeSocketDatatype type)
{
  switch (type) {
    /* For now INT is supported as float. */
    case SOCK_INT:
    case SOCK_FLOAT:
      return "set_value";
    case SOCK_VECTOR:
      return "set_rgb";
    case SOCK_RGBA:
      return "set_rgba";
    default:
      BLI_assert(!"No gpu function for non-supported eNodeSocketDatatype");
      return NULL;
  }
}

/**
 * Link stack uniform buffer.
 * This is called for the input/output sockets that are note connected.
 */
static GPUNodeLink *gpu_uniformbuffer_link(GPUMaterial *mat,
                                           bNode *node,
                                           GPUNodeStack *stack,
                                           const int index,
                                           const eNodeSocketInOut in_out)
{
  bNodeSocket *socket;

  if (in_out == SOCK_IN) {
    socket = BLI_findlink(&node->inputs, index);
  }
  else {
    socket = BLI_findlink(&node->outputs, index);
  }

  BLI_assert(socket != NULL);
  BLI_assert(socket->in_out == in_out);

  if ((socket->flag & SOCK_HIDE_VALUE) == 0) {
    GPUNodeLink *link;
    switch (socket->type) {
      case SOCK_FLOAT: {
        bNodeSocketValueFloat *socket_data = socket->default_value;
        link = GPU_uniform(&socket_data->value);
        break;
      }
      case SOCK_VECTOR: {
        bNodeSocketValueVector *socket_data = socket->default_value;
        link = GPU_uniform(socket_data->value);
        break;
      }
      case SOCK_RGBA: {
        bNodeSocketValueRGBA *socket_data = socket->default_value;
        link = GPU_uniform(socket_data->value);
        break;
      }
      default:
        return NULL;
        break;
    }

    if (in_out == SOCK_IN) {
      GPU_link(mat, gpu_uniform_set_function_from_type(socket->type), link, &stack->link);
    }
    return link;
  }
  return NULL;
}

static void gpu_node_input_socket(
    GPUMaterial *material, bNode *bnode, GPUNode *node, GPUNodeStack *sock, const int index)
{
  if (sock->link) {
    gpu_node_input_link(node, sock->link, sock->type);
  }
  else if ((material != NULL) &&
           (gpu_uniformbuffer_link(material, bnode, sock, index, SOCK_IN) != NULL)) {
    gpu_node_input_link(node, sock->link, sock->type);
  }
  else {
    gpu_node_input_link(node, GPU_constant(sock->vec), sock->type);
  }
}

static void gpu_node_output(GPUNode *node, const eGPUType type, GPUNodeLink **link)
{
  GPUOutput *output = MEM_callocN(sizeof(GPUOutput), "GPUOutput");

  output->type = type;
  output->node = node;

  if (link) {
    *link = output->link = GPU_node_link_create();
    output->link->link_type = GPU_NODE_LINK_OUTPUT;
    output->link->output = output;

    /* note: the caller owns the reference to the link, GPUOutput
     * merely points to it, and if the node is destroyed it will
     * set that pointer to NULL */
  }

  BLI_addtail(&node->outputs, output);
}

void GPU_inputs_free(ListBase *inputs)
{
  GPUInput *input;

  for (input = inputs->first; input; input = input->next) {
    if (input->link) {
      gpu_node_link_free(input->link);
    }
  }

  BLI_freelistN(inputs);
}

static void gpu_node_free(GPUNode *node)
{
  GPUOutput *output;

  GPU_inputs_free(&node->inputs);

  for (output = node->outputs.first; output; output = output->next) {
    if (output->link) {
      output->link->output = NULL;
      gpu_node_link_free(output->link);
    }
  }

  BLI_freelistN(&node->outputs);
  MEM_freeN(node);
}

static void gpu_nodes_free(ListBase *nodes)
{
  GPUNode *node;

  while ((node = BLI_pophead(nodes))) {
    gpu_node_free(node);
  }
}

/* vertex attributes */

void GPU_nodes_get_vertex_attrs(ListBase *nodes, GPUVertAttrLayers *attrs)
{
  GPUNode *node;
  GPUInput *input;
  int a;

  /* convert attributes requested by node inputs to an array of layers,
   * checking for duplicates and assigning id's starting from zero. */

  memset(attrs, 0, sizeof(*attrs));

  for (node = nodes->first; node; node = node->next) {
    for (input = node->inputs.first; input; input = input->next) {
      if (input->source == GPU_SOURCE_ATTR) {
        for (a = 0; a < attrs->totlayer; a++) {
          if (attrs->layer[a].type == input->attr_type &&
              STREQ(attrs->layer[a].name, input->attr_name)) {
            break;
          }
        }

        if (a < GPU_MAX_ATTR) {
          if (a == attrs->totlayer) {
            input->attr_id = attrs->totlayer++;
            input->attr_first = true;

            attrs->layer[a].type = input->attr_type;
            attrs->layer[a].attr_id = input->attr_id;
            BLI_strncpy(attrs->layer[a].name, input->attr_name, sizeof(attrs->layer[a].name));
          }
          else {
            input->attr_id = attrs->layer[a].attr_id;
          }
        }
      }
    }
  }
}

/* varargs linking  */

GPUNodeLink *GPU_attribute(const CustomDataType type, const char *name)
{
  GPUNodeLink *link = GPU_node_link_create();
  link->link_type = GPU_NODE_LINK_ATTR;
  link->attr_name = name;
  /* Fall back to the UV layer, which matches old behavior. */
  if (type == CD_AUTO_FROM_NAME && name[0] == '\0') {
    link->attr_type = CD_MTFACE;
  }
  else {
    link->attr_type = type;
  }
  return link;
}

GPUNodeLink *GPU_constant(float *num)
{
  GPUNodeLink *link = GPU_node_link_create();
  link->link_type = GPU_NODE_LINK_CONSTANT;
  link->data = num;
  return link;
}

GPUNodeLink *GPU_uniform(float *num)
{
  GPUNodeLink *link = GPU_node_link_create();
  link->link_type = GPU_NODE_LINK_UNIFORM;
  link->data = num;
  return link;
}

GPUNodeLink *GPU_image(Image *ima, ImageUser *iuser)
{
  GPUNodeLink *link = GPU_node_link_create();
  link->link_type = GPU_NODE_LINK_IMAGE_BLENDER;
  link->ima = ima;
  link->iuser = iuser;
  return link;
}

GPUNodeLink *GPU_color_band(GPUMaterial *mat, int size, float *pixels, float *row)
{
  GPUNodeLink *link = GPU_node_link_create();
  link->link_type = GPU_NODE_LINK_COLORBAND;
  link->coba = gpu_material_ramp_texture_row_set(mat, size, pixels, row);
  MEM_freeN(pixels);
  return link;
}

GPUNodeLink *GPU_builtin(eGPUBuiltin builtin)
{
  GPUNodeLink *link = GPU_node_link_create();
  link->link_type = GPU_NODE_LINK_BUILTIN;
  link->builtin = builtin;
  return link;
}

bool GPU_link(GPUMaterial *mat, const char *name, ...)
{
  GPUNode *node;
  GPUFunction *function;
  GPUNodeLink *link, **linkptr;
  va_list params;
  int i;

  function = gpu_lookup_function(name);
  if (!function) {
    fprintf(stderr, "GPU failed to find function %s\n", name);
    return false;
  }

  node = GPU_node_begin(name);

  va_start(params, name);
  for (i = 0; i < function->totparam; i++) {
    if (function->paramqual[i] != FUNCTION_QUAL_IN) {
      linkptr = va_arg(params, GPUNodeLink **);
      gpu_node_output(node, function->paramtype[i], linkptr);
    }
    else {
      link = va_arg(params, GPUNodeLink *);
      gpu_node_input_link(node, link, function->paramtype[i]);
    }
  }
  va_end(params);

  gpu_material_add_node(mat, node);

  return true;
}

bool GPU_stack_link(GPUMaterial *material,
                    bNode *bnode,
                    const char *name,
                    GPUNodeStack *in,
                    GPUNodeStack *out,
                    ...)
{
  GPUNode *node;
  GPUFunction *function;
  GPUNodeLink *link, **linkptr;
  va_list params;
  int i, totin, totout;

  function = gpu_lookup_function(name);
  if (!function) {
    fprintf(stderr, "GPU failed to find function %s\n", name);
    return false;
  }

  node = GPU_node_begin(name);
  totin = 0;
  totout = 0;

  if (in) {
    for (i = 0; !in[i].end; i++) {
      if (in[i].type != GPU_NONE) {
        gpu_node_input_socket(material, bnode, node, &in[i], i);
        totin++;
      }
    }
  }

  if (out) {
    for (i = 0; !out[i].end; i++) {
      if (out[i].type != GPU_NONE) {
        gpu_node_output(node, out[i].type, &out[i].link);
        totout++;
      }
    }
  }

  va_start(params, out);
  for (i = 0; i < function->totparam; i++) {
    if (function->paramqual[i] != FUNCTION_QUAL_IN) {
      if (totout == 0) {
        linkptr = va_arg(params, GPUNodeLink **);
        gpu_node_output(node, function->paramtype[i], linkptr);
      }
      else {
        totout--;
      }
    }
    else {
      if (totin == 0) {
        link = va_arg(params, GPUNodeLink *);
        if (link->socket) {
          gpu_node_input_socket(NULL, NULL, node, link->socket, -1);
        }
        else {
          gpu_node_input_link(node, link, function->paramtype[i]);
        }
      }
      else {
        totin--;
      }
    }
  }
  va_end(params);

  gpu_material_add_node(material, node);

  return true;
}

GPUNodeLink *GPU_uniformbuffer_link_out(GPUMaterial *mat,
                                        bNode *node,
                                        GPUNodeStack *stack,
                                        const int index)
{
  return gpu_uniformbuffer_link(mat, node, stack, index, SOCK_OUT);
}

/* Pass create/free */

static void gpu_nodes_tag(GPUNodeLink *link)
{
  GPUNode *node;
  GPUInput *input;

  if (!link->output) {
    return;
  }

  node = link->output->node;
  if (node->tag) {
    return;
  }

  node->tag = true;
  for (input = node->inputs.first; input; input = input->next) {
    if (input->link) {
      gpu_nodes_tag(input->link);
    }
  }
}

void GPU_nodes_prune(ListBase *nodes, GPUNodeLink *outlink)
{
  GPUNode *node, *next;

  for (node = nodes->first; node; node = node->next) {
    node->tag = false;
  }

  gpu_nodes_tag(outlink);

  for (node = nodes->first; node; node = next) {
    next = node->next;

    if (!node->tag) {
      BLI_remlink(nodes, node);
      gpu_node_free(node);
    }
  }
}

static bool gpu_pass_is_valid(GPUPass *pass)
{
  /* Shader is not null if compilation is successful. */
  return (pass->compiled == false || pass->shader != NULL);
}

GPUPass *GPU_generate_pass(GPUMaterial *material,
                           GPUNodeLink *frag_outlink,
                           struct GPUVertAttrLayers *attrs,
                           ListBase *nodes,
                           int *builtins,
                           const char *vert_code,
                           const char *geom_code,
                           const char *frag_lib,
                           const char *defines)
{
  char *vertexcode, *geometrycode, *fragmentcode;
  GPUPass *pass = NULL, *pass_hash = NULL;

  /* prune unused nodes */
  GPU_nodes_prune(nodes, frag_outlink);

  GPU_nodes_get_vertex_attrs(nodes, attrs);

  /* generate code */
  char *fragmentgen = code_generate_fragment(material, nodes, frag_outlink->output, builtins);

  /* Cache lookup: Reuse shaders already compiled */
  uint32_t hash = gpu_pass_hash(fragmentgen, defines, attrs);
  pass_hash = gpu_pass_cache_lookup(hash);

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
  char *tmp = BLI_strdupcat(frag_lib, glsl_material_library);

  geometrycode = code_generate_geometry(nodes, geom_code, defines);
  vertexcode = code_generate_vertex(nodes, vert_code, (geometrycode != NULL));
  fragmentcode = BLI_strdupcat(tmp, fragmentgen);

  MEM_freeN(fragmentgen);
  MEM_freeN(tmp);

  if (pass_hash) {
    /* Cache lookup: Reuse shaders already compiled */
    pass = gpu_pass_cache_resolve_collision(
        pass_hash, vertexcode, geometrycode, fragmentcode, defines, hash);
  }

  if (pass) {
    /* Cache hit. Reuse the same GPUPass and GPUShader. */
    if (!gpu_pass_is_valid(pass)) {
      /* Shader has already been created but failed to compile. */
      return NULL;
    }

    MEM_SAFE_FREE(vertexcode);
    MEM_SAFE_FREE(fragmentcode);
    MEM_SAFE_FREE(geometrycode);

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
  int samplers_id[64]; /* Remember this is per stage. */
  int sampler_len = 0;

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
    if (gpu_str_prefix(code, "sampler")) {
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
        int id = GPU_shader_get_uniform_ensure(shader, sampler_name);

        if (id == -1) {
          continue;
        }
        /* Catch duplicates. */
        bool is_duplicate = false;
        for (int i = 0; i < sampler_len; ++i) {
          if (samplers_id[i] == id) {
            is_duplicate = true;
          }
        }

        if (!is_duplicate) {
          samplers_id[sampler_len] = id;
          sampler_len++;
        }
      }
    }
  }

  return sampler_len;
}

static bool gpu_pass_shader_validate(GPUPass *pass)
{
  if (pass->shader == NULL) {
    return false;
  }

  /* NOTE: The only drawback of this method is that it will count a sampler
   * used in the fragment shader and only declared (but not used) in the vertex
   * shader as used by both. But this corner case is not happening for now. */
  int vert_samplers_len = count_active_texture_sampler(pass->shader, pass->vertexcode);
  int frag_samplers_len = count_active_texture_sampler(pass->shader, pass->fragmentcode);

  int total_samplers_len = vert_samplers_len + frag_samplers_len;

  /* Validate against opengl limit. */
  if ((frag_samplers_len > GPU_max_textures_frag()) ||
      (vert_samplers_len > GPU_max_textures_vert())) {
    return false;
  }

  if (pass->geometrycode) {
    int geom_samplers_len = count_active_texture_sampler(pass->shader, pass->geometrycode);
    total_samplers_len += geom_samplers_len;
    if (geom_samplers_len > GPU_max_textures_geom()) {
      return false;
    }
  }

  return (total_samplers_len <= GPU_max_textures());
}

void GPU_pass_compile(GPUPass *pass, const char *shname)
{
  if (!pass->compiled) {
    pass->shader = GPU_shader_create(
        pass->vertexcode, pass->fragmentcode, pass->geometrycode, NULL, pass->defines, shname);

    /* NOTE: Some drivers / gpu allows more active samplers than the opengl limit.
     * We need to make sure to count active samplers to avoid undefined behavior. */
    if (!gpu_pass_shader_validate(pass)) {
      if (pass->shader != NULL) {
        fprintf(stderr, "GPUShader: error: too many samplers in shader.\n");
        GPU_shader_free(pass->shader);
      }
      pass->shader = NULL;
    }
    else if (!BLI_thread_is_main()) {
      /* For some Intel drivers, you must use the program at least once
       * in the rendering context that it is linked. */
      glUseProgram(GPU_shader_get_program(pass->shader));
      glUseProgram(0);
    }

    pass->compiled = true;
  }
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
  MEM_freeN(pass);
}

void GPU_pass_free_nodes(ListBase *nodes)
{
  gpu_nodes_free(nodes);
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
