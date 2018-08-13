/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_codegen.h
 *  \ingroup gpu
 */


#ifndef __GPU_CODEGEN_H__
#define __GPU_CODEGEN_H__

#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "GPU_material.h"
#include "GPU_glew.h"

struct ListBase;
struct GPUShader;
struct GPUOutput;
struct GPUNode;
struct GPUVertexAttribs;
struct PreviewImage;

/* Pass Generation
 *  - Takes a list of nodes and a desired output, and makes a pass. This
 *    will take ownership of the nodes and free them early if unused or
 *    at the end if used.
 */

typedef enum GPUDataSource {
	GPU_SOURCE_OUTPUT,
	GPU_SOURCE_CONSTANT,
	GPU_SOURCE_UNIFORM,
	GPU_SOURCE_ATTRIB,
	GPU_SOURCE_BUILTIN,
	GPU_SOURCE_STRUCT,
	GPU_SOURCE_TEX,
} GPUDataSource;

typedef enum {
	GPU_NODE_LINK_NONE = 0,
	GPU_NODE_LINK_ATTRIB,
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
		GPUBuiltin builtin;
		/* GPU_NODE_LINK_COLORBAND */
		struct GPUTexture **coba;
		/* GPU_NODE_LINK_OUTPUT */
		struct GPUOutput *output;
		/* GPU_NODE_LINK_ATTRIB */
		struct {
			const char *attribname;
			CustomDataType attribtype;
		};
		/* GPU_NODE_LINK_IMAGE_BLENDER */
		struct {
			struct Image *ima;
			struct ImageUser *iuser;
			bool image_isdata;
		};
	};
};

typedef struct GPUOutput {
	struct GPUOutput *next, *prev;

	GPUNode *node;
	GPUType type;      /* data type = length of vector/matrix */
	GPUNodeLink *link; /* output link */
	int id;            /* unique id as created by code generator */
} GPUOutput;

typedef struct GPUInput {
	struct GPUInput *next, *prev;

	GPUNode *node;
	GPUType type;                /* datatype */
	GPUNodeLink *link;
	int id;                      /* unique id as created by code generator */

	GPUDataSource source;        /* data source */

	int shaderloc;               /* id from opengl */
	char shadername[32];         /* name in shader */

	/* Content based on GPUDataSource */
	union {
		/* GPU_SOURCE_CONSTANT | GPU_SOURCE_UNIFORM */
		float vec[16];                   /* vector data */
		/* GPU_SOURCE_BUILTIN */
		GPUBuiltin builtin;              /* builtin uniform */
		/* GPU_SOURCE_TEX */
		struct {
			struct GPUTexture **coba;    /* input texture, only set at runtime */
			struct Image *ima;           /* image */
			struct ImageUser *iuser;     /* image user */
			bool image_isdata;           /* image does not contain color data */
			bool bindtex;                /* input is responsible for binding the texture? */
			int texid;                   /* number for multitexture, starting from zero */
			GPUType textype;             /* texture type (2D, 1D Array ...) */
		};
		/* GPU_SOURCE_ATTRIB */
		struct {
			char attribname[MAX_CUSTOMDATA_LAYER_NAME]; /* attribute name */
			int attribid;                /* id for vertex attributes */
			bool attribfirst;            /* this is the first one that is bound */
			CustomDataType attribtype;   /* attribute type */
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
	unsigned int refcount;       /* Orphaned GPUPasses gets freed by the garbage collector. */
	uint32_t hash;               /* Identity hash generated from all GLSL code. */
	bool compiled;               /* Did we already tried to compile the attached GPUShader. */
};

typedef struct GPUPass GPUPass;

GPUPass *GPU_generate_pass_new(
        GPUMaterial *material,
        GPUNodeLink *frag_outlink, struct GPUVertexAttribs *attribs,
        ListBase *nodes,
        const char *vert_code, const char *geom_code,
        const char *frag_lib, const char *defines);

struct GPUShader *GPU_pass_shader_get(GPUPass *pass);

void GPU_nodes_extract_dynamic_inputs(struct GPUShader *shader, ListBase *inputs, ListBase *nodes);
void GPU_nodes_get_vertex_attributes(ListBase *nodes, struct GPUVertexAttribs *attribs);
void GPU_nodes_prune(ListBase *nodes, struct GPUNodeLink *outlink);

void GPU_pass_compile(GPUPass *pass, const char *shname);
void GPU_pass_release(GPUPass *pass);
void GPU_pass_free_nodes(ListBase *nodes);

void GPU_inputs_free(ListBase *inputs);

void gpu_codegen_init(void);
void gpu_codegen_exit(void);

/* Material calls */

const char *GPU_builtin_name(GPUBuiltin builtin);
void gpu_material_add_node(struct GPUMaterial *material, struct GPUNode *node);
struct GPUTexture **gpu_material_ramp_texture_row_set(GPUMaterial *mat, int size, float *pixels, float *row);

#endif
