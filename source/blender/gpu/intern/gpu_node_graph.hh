/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Intermediate node graph for generating GLSL shaders.
 */

#pragma once

#include "DNA_listBase.h"

#include "BLI_enum_flags.hh"
#include "BLI_ghash.h"

#include "GPU_material.hh"

struct GPUNode;
struct GPUOutput;
struct ListBase;

enum GPUDataSource {
  GPU_SOURCE_OUTPUT,
  GPU_SOURCE_CONSTANT,
  GPU_SOURCE_UNIFORM,
  GPU_SOURCE_ATTR,
  GPU_SOURCE_UNIFORM_ATTR,
  GPU_SOURCE_LAYER_ATTR,
  GPU_SOURCE_STRUCT,
  GPU_SOURCE_TEX,
  GPU_SOURCE_TEX_TILED_MAPPING,
  GPU_SOURCE_FUNCTION_CALL,
  GPU_SOURCE_CRYPTOMATTE,
};

enum GPUNodeLinkType {
  GPU_NODE_LINK_NONE = 0,
  GPU_NODE_LINK_ATTR,
  GPU_NODE_LINK_UNIFORM_ATTR,
  GPU_NODE_LINK_LAYER_ATTR,
  GPU_NODE_LINK_COLORBAND,
  GPU_NODE_LINK_CONSTANT,
  GPU_NODE_LINK_IMAGE,
  GPU_NODE_LINK_IMAGE_TILED,
  GPU_NODE_LINK_IMAGE_TILED_MAPPING,
  GPU_NODE_LINK_IMAGE_SKY,
  GPU_NODE_LINK_OUTPUT,
  GPU_NODE_LINK_UNIFORM,
  GPU_NODE_LINK_DIFFERENTIATE_FLOAT_FN,
};

enum GPUNodeTag {
  GPU_NODE_TAG_NONE = 0,
  GPU_NODE_TAG_SURFACE = (1 << 0),
  GPU_NODE_TAG_VOLUME = (1 << 1),
  GPU_NODE_TAG_DISPLACEMENT = (1 << 2),
  GPU_NODE_TAG_THICKNESS = (1 << 3),
  GPU_NODE_TAG_AOV = (1 << 4),
  GPU_NODE_TAG_FUNCTION = (1 << 5),
  GPU_NODE_TAG_COMPOSITOR = (1 << 6),
};

ENUM_OPERATORS(GPUNodeTag)

struct GPUNode {
  GPUNode *next, *prev;

  const char *name;

  /* Internal flag to mark nodes during pruning */
  GPUNodeTag tag;

  ListBase inputs;
  ListBase outputs;

  /* Zones. */
  int zone_index;
  bool is_zone_end;
};

struct GPUNodeLink {
  GPUNodeStack *socket;

  GPUNodeLinkType link_type;
  int users; /* Refcount */

  union {
    /* GPU_NODE_LINK_CONSTANT | GPU_NODE_LINK_UNIFORM */
    const float *data;
    /* GPU_NODE_LINK_COLORBAND */
    blender::gpu::Texture **colorband;
    /* GPU_NODE_LINK_OUTPUT */
    GPUOutput *output;
    /* GPU_NODE_LINK_ATTR */
    GPUMaterialAttribute *attr;
    /* GPU_NODE_LINK_UNIFORM_ATTR */
    GPUUniformAttr *uniform_attr;
    /* GPU_NODE_LINK_LAYER_ATTR */
    GPULayerAttr *layer_attr;
    /* GPU_NODE_LINK_IMAGE_BLENDER */
    GPUMaterialTexture *texture;
    /* GPU_NODE_LINK_DIFFERENTIATE_FLOAT_FN */
    struct {
      const char *function_name;
      float filter_width;
    } differentiate_float;
  };
};

struct GPUOutput {
  GPUOutput *next, *prev;

  GPUNode *node;
  GPUType type;      /* data type = length of vector/matrix */
  GPUNodeLink *link; /* output link */
  int id;            /* unique id as created by code generator */

  /* True for Zone Items. */
  bool is_zone_io;
  /* This variable is shared with other socket/s and doesn't need to be declared. */
  bool is_duplicate;
};

struct GPUInput {
  GPUInput *next, *prev;

  GPUNode *node;
  GPUType type; /* data-type. */
  GPUNodeLink *link;
  int id; /* unique id as created by code generator */

  GPUDataSource source; /* data source */

  /* Content based on GPUDataSource */
  union {
    /* GPU_SOURCE_CONSTANT | GPU_SOURCE_UNIFORM */
    float vec[16]; /* vector data */
                   /* GPU_SOURCE_TEX | GPU_SOURCE_TEX_TILED_MAPPING */
    GPUMaterialTexture *texture;
    /* GPU_SOURCE_ATTR */
    GPUMaterialAttribute *attr;
    /* GPU_SOURCE_UNIFORM_ATTR */
    GPUUniformAttr *uniform_attr;
    /* GPU_SOURCE_LAYER_ATTR */
    GPULayerAttr *layer_attr;
    /* GPU_SOURCE_FUNCTION_CALL */
    char function_call[64];
  };

  /* True for Zone Items. */
  bool is_zone_io;
  /* This variable is shared with other socket/s and doesn't need to be declared. */
  bool is_duplicate;
};

struct GPUNodeGraphOutputLink {
  GPUNodeGraphOutputLink *next, *prev;
  int hash;
  GPUNodeLink *outlink;
};

struct GPUNodeGraphFunctionLink {
  GPUNodeGraphFunctionLink *next, *prev;
  char name[16];
  GPUNodeLink *outlink;
};

struct GPUNodeGraph {
  /* Nodes */
  ListBase nodes;

  /* Main Outputs. */
  GPUNodeLink *outlink_surface;
  GPUNodeLink *outlink_volume;
  GPUNodeLink *outlink_displacement;
  GPUNodeLink *outlink_thickness;
  /* List of GPUNodeGraphOutputLink */
  ListBase outlink_aovs;
  /* List of GPUNodeGraphFunctionLink */
  ListBase material_functions;
  /* List of GPUNodeGraphOutputLink */
  ListBase outlink_compositor;

  /* Requested attributes and textures. */
  ListBase attributes;
  ListBase textures;

  /* The list of uniform attributes. */
  GPUUniformAttrList uniform_attrs;

  /* The list of layer attributes. */
  ListBase layer_attrs;
};

/* Node Graph */

void gpu_nodes_tag(GPUNodeGraph *graph, GPUNodeLink *link_start, GPUNodeTag tag);
void gpu_node_graph_prune_unused(GPUNodeGraph *graph);
void gpu_node_graph_finalize_uniform_attrs(GPUNodeGraph *graph);

/**
 * Optimize node graph for optimized material shader path.
 * Once the base material has been generated, we can modify the shader
 * node graph to create one which will produce an optimally performing shader.
 * This currently involves baking uniform data into constant data to enable
 * aggressive constant folding by the compiler in order to reduce complexity and
 * shader core memory pressure.
 *
 * NOTE: Graph optimizations will produce a shader which needs to be re-compiled
 * more frequently, however, the default material pass will always exist to fall
 * back on. */
void gpu_node_graph_optimize(GPUNodeGraph *graph);

/**
 * Free intermediate node graph.
 */
void gpu_node_graph_free_nodes(GPUNodeGraph *graph);
/**
 * Free both node graph and requested attributes and textures.
 */
void gpu_node_graph_free(GPUNodeGraph *graph);

/* Material calls */

GPUNodeGraph *gpu_material_node_graph(GPUMaterial *material);
/**
 * Returns the address of the future pointer to coba_tex.
 */
blender::gpu::Texture **gpu_material_ramp_texture_row_set(GPUMaterial *mat,
                                                          int size,
                                                          const float *pixels,
                                                          float *r_row);
/**
 * Returns the address of the future pointer to sky_tex
 */
blender::gpu::Texture **gpu_material_sky_texture_layer_set(
    GPUMaterial *mat, int width, int height, const float *pixels, float *row);
