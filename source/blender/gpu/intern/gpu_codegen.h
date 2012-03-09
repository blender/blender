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

#include "DNA_listBase.h"
#include "GPU_material.h"
#include "GL/glew.h"

struct ListBase;
struct GPUShader;
struct GPUOutput;
struct GPUNode;
struct GPUVertexAttribs;
struct GPUFrameBuffer;

#define MAX_FUNCTION_NAME	64
#define MAX_PARAMETER		32

#define FUNCTION_QUAL_IN	0
#define FUNCTION_QUAL_OUT	1
#define FUNCTION_QUAL_INOUT	2

typedef struct GPUFunction {
	char name[MAX_FUNCTION_NAME];
	int paramtype[MAX_PARAMETER];
	int paramqual[MAX_PARAMETER];
	int totparam;
} GPUFunction;

GPUFunction *GPU_lookup_function(const char *name);

/* Pass Generation
 *  - Takes a list of nodes and a desired output, and makes a pass. This
 *    will take ownership of the nodes and free them early if unused or
 *    at the end if used.
 */

typedef enum GPUDataSource {
	GPU_SOURCE_VEC_UNIFORM,
	GPU_SOURCE_BUILTIN,
	GPU_SOURCE_TEX_PIXEL,
	GPU_SOURCE_TEX,
	GPU_SOURCE_ATTRIB
} GPUDataSource;

struct GPUNode {
	struct GPUNode *next, *prev;

	const char *name;
	int tag;

	ListBase inputs;
	ListBase outputs;
};

struct GPUNodeLink {
	GPUNodeStack *socket;

	int attribtype;
	const char *attribname;

	int image;

	int texture;
	int texturesize;

	void *ptr1, *ptr2;

	int dynamic;
	int dynamictype;	

	int type;
	int users;

	GPUTexture *dynamictex;

	GPUBuiltin builtin;

	struct GPUOutput *output;
};

typedef struct GPUOutput {
	struct GPUOutput *next, *prev;

	GPUNode *node;
	int type;				/* data type = length of vector/matrix */
	GPUNodeLink *link;		/* output link */
	int id;					/* unique id as created by code generator */
} GPUOutput;

typedef struct GPUInput {
	struct GPUInput *next, *prev;

	GPUNode *node;

	int type;				/* datatype */
	int source;				/* data source */

	int id;					/* unique id as created by code generator */
	int texid;				/* number for multitexture */
	int attribid;			/* id for vertex attributes */
	int bindtex;			/* input is responsible for binding the texture? */
	int definetex;			/* input is responsible for defining the pixel? */
	int textarget;			/* GL_TEXTURE_* */
	int textype;			/* datatype */

	struct Image *ima;		/* image */
	struct ImageUser *iuser;/* image user */
	float *dynamicvec;		/* vector data in case it is dynamic */
	int dynamictype;		/* origin of the dynamic uniform (GPUDynamicType) */
	void *dynamicdata;		/* data source of the dynamic uniform */
	GPUTexture *tex;		/* input texture, only set at runtime */
	int shaderloc;			/* id from opengl */
	char shadername[32];	/* name in shader */

	float vec[16];			/* vector data */
	GPUNodeLink *link;
	int dynamictex;			/* dynamic? */
	int attribtype;			/* attribute type */
	char attribname[32];	/* attribute name */
	int attribfirst;		/* this is the first one that is bound */
	GPUBuiltin builtin;		/* builtin uniform */
} GPUInput;

struct GPUPass {
	struct GPUPass *next, *prev;

	ListBase inputs;
	struct GPUOutput *output;
	struct GPUShader *shader;
	char *fragmentcode;
	char *vertexcode;
	const char *libcode;
};


typedef struct GPUPass GPUPass;

GPUPass *GPU_generate_pass(ListBase *nodes, struct GPUNodeLink *outlink,
	struct GPUVertexAttribs *attribs, int *builtin, const char *name);

struct GPUShader *GPU_pass_shader(GPUPass *pass);

void GPU_pass_bind(GPUPass *pass, double time, int mipmap);
void GPU_pass_update_uniforms(GPUPass *pass);
void GPU_pass_unbind(GPUPass *pass);

void GPU_pass_free(GPUPass *pass);

void GPU_codegen_init(void);
void GPU_codegen_exit(void);

/* Material calls */

const char *GPU_builtin_name(GPUBuiltin builtin);
void gpu_material_add_node(struct GPUMaterial *material, struct GPUNode *node);
int GPU_link_changed(struct GPUNodeLink *link);

#endif

