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
 */

#ifndef __GPU_CODEGEN_H__
#define __GPU_CODEGEN_H__

#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "GPU_material.h"
#include "GPU_glew.h"

struct GPUNode;
struct GPUOutput;
struct GPUShader;
struct GPUVertAttrLayers;
struct ListBase;
struct PreviewImage;

/* Pass Generation
 * - Takes a list of nodes and a desired output, and makes a pass. This
 *   will take ownership of the nodes and free them early if unused or
 *   at the end if used.
 */

typedef enum eGPUDataSource {
  GPU_SOURCE_OUTPUT,
  GPU_SOURCE_CONSTANT,
  GPU_SOURCE_UNIFORM,
  GPU_SOURCE_ATTR,
  GPU_SOURCE_BUILTIN,
  GPU_SOURCE_STRUCT,
  GPU_SOURCE_TEX,
} eGPUDataSource;

typedef enum {
  GPU_NODE_LINK_NONE = 0,
  GPU_NODE_LINK_ATTR,
  GPU_NODE_LINK_BUILTIN,
  GPU_NODE_LINK_COLORBAND,
  GPU_NODE_LINK_CONSTANT,
  GPU_NODE_LINK_IMAGE_BLENDER,
  GPU_NODE_LINK_OUTPUT,
  GPU_NODE_LINK_UNIFORM,
} GPUNodeLinkType;

struct GPUNode {
  struct GPUNode *next, *prev;

  const char *name;

  /* Internal flag to mark nodes during pruning */
  bool tag;

  ListBase inputs;
  ListBase outputs;
};

struct GPUNodeLink {
  GPUNodeStack *socket;

  GPUNodeLinkType link_type;
  int users; /* Refcount */

  union {
    /* GPU_NODE_LINK_CONSTANT | GPU_NODE_LINK_UNIFORM */
    float *data;
    /* GPU_NODE_LINK_BUILTIN */
    eGPUBuiltin builtin;
    /* GPU_NODE_LINK_COLORBAND */
    struct GPUTexture **coba;
    /* GPU_NODE_LINK_OUTPUT */
    struct GPUOutput *output;
    /* GPU_NODE_LINK_ATTR */
    struct {
      const char *attr_name;
      CustomDataType attr_type;
    };
    /* GPU_NODE_LINK_IMAGE_BLENDER */
    struct {
      struct Image *ima;
      struct ImageUser *iuser;
    };
  };
};

typedef struct GPUOutput {
  struct GPUOutput *next, *prev;

  GPUNode *node;
  eGPUType type;     /* data type = length of vector/matrix */
  GPUNodeLink *link; /* output link */
  int id;            /* unique id as created by code generator */
} GPUOutput;

typedef struct GPUInput {
  struct GPUInput *next, *prev;

  GPUNode *node;
  eGPUType type; /* datatype */
  GPUNodeLink *link;
  int id; /* unique id as created by code generator */

  eGPUDataSource source; /* data source */

  int shaderloc;       /* id from opengl */
  char shadername[32]; /* name in shader */

  /* Content based on eGPUDataSource */
  union {
    /* GPU_SOURCE_CONSTANT | GPU_SOURCE_UNIFORM */
    float vec[16]; /* vector data */
    /* GPU_SOURCE_BUILTIN */
    eGPUBuiltin builtin; /* builtin uniform */
    /* GPU_SOURCE_TEX */
    struct {
      struct GPUTexture **coba; /* input texture, only set at runtime */
      struct Image *ima;        /* image */
      struct ImageUser *iuser;  /* image user */
      bool bindtex;             /* input is responsible for binding the texture? */
      int texid;                /* number for multitexture, starting from zero */
      eGPUType textype;         /* texture type (2D, 1D Array ...) */
    };
    /* GPU_SOURCE_ATTR */
    struct {
      /** Attribute name. */
      char attr_name[MAX_CUSTOMDATA_LAYER_NAME];
      /** ID for vertex attributes. */
      int attr_id;
      /** This is the first one that is bound. */
      bool attr_first;
      /** Attribute type. */
      CustomDataType attr_type;
    };
  };
} GPUInput;

struct GPUPass {
  struct GPUPass *next;

  struct GPUShader *shader;
  char *fragmentcode;
  char *geometrycode;
  char *vertexcode;
  char *defines;
  uint refcount; /* Orphaned GPUPasses gets freed by the garbage collector. */
  uint32_t hash; /* Identity hash generated from all GLSL code. */
  bool compiled; /* Did we already tried to compile the attached GPUShader. */
};

typedef struct GPUPass GPUPass;

GPUPass *GPU_generate_pass(GPUMaterial *material,
                           GPUNodeLink *frag_outlink,
                           struct GPUVertAttrLayers *attrs,
                           ListBase *nodes,
                           int *builtins,
                           const char *vert_code,
                           const char *geom_code,
                           const char *frag_lib,
                           const char *defines);

struct GPUShader *GPU_pass_shader_get(GPUPass *pass);

void GPU_nodes_extract_dynamic_inputs(struct GPUShader *shader, ListBase *inputs, ListBase *nodes);
void GPU_nodes_get_vertex_attrs(ListBase *nodes, struct GPUVertAttrLayers *attrs);
void GPU_nodes_prune(ListBase *nodes, struct GPUNodeLink *outlink);

void GPU_pass_compile(GPUPass *pass, const char *shname);
void GPU_pass_release(GPUPass *pass);
void GPU_pass_free_nodes(ListBase *nodes);

void GPU_inputs_free(ListBase *inputs);

void gpu_codegen_init(void);
void gpu_codegen_exit(void);

/* Material calls */

const char *GPU_builtin_name(eGPUBuiltin builtin);
void gpu_material_add_node(struct GPUMaterial *material, struct GPUNode *node);
struct GPUTexture **gpu_material_ramp_texture_row_set(GPUMaterial *mat,
                                                      int size,
                                                      float *pixels,
                                                      float *row);

#endif
