/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This shader is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This shader is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this shader; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef __GPU_MATERIAL__
#define __GPU_MATERIAL__

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct ImageUser;
struct Material;
struct Object;
struct Lamp;
struct bNode;
struct LinkNode;
struct Scene;
struct GPUVertexAttribs;
struct GPUNode;
struct GPUNodeLink;
struct GPUNodeStack;
struct GPUMaterial;
struct GPUTexture;
struct GPULamp;

typedef struct GPUNode GPUNode;
typedef struct GPUNodeLink GPUNodeLink;
typedef struct GPUMaterial GPUMaterial;
typedef struct GPULamp GPULamp;

/* Functions to create GPU Materials nodes */

typedef enum GPUType {
	GPU_NONE = 0,
	GPU_FLOAT = 1,
	GPU_VEC2 = 2,
	GPU_VEC3 = 3,
	GPU_VEC4 = 4,
	GPU_MAT3 = 9,
	GPU_MAT4 = 16,
	GPU_TEX1D = 1001,
	GPU_TEX2D = 1002,
	GPU_SHADOW2D = 1003,
	GPU_ATTRIB = 3001
} GPUType;

typedef enum GPUBuiltin {
	GPU_VIEW_MATRIX = 1,
	GPU_OBJECT_MATRIX = 2,
	GPU_INVERSE_VIEW_MATRIX = 4,
	GPU_INVERSE_OBJECT_MATRIX = 8,
	GPU_VIEW_POSITION = 16,
	GPU_VIEW_NORMAL = 32,
	GPU_OBCOLOR = 64
} GPUBuiltin;

typedef enum GPUBlendMode {
	GPU_BLEND_SOLID = 0,
	GPU_BLEND_ADD = 1,
	GPU_BLEND_ALPHA = 2,
	GPU_BLEND_CLIP = 4
} GPUBlendMode;

typedef struct GPUNodeStack {
	GPUType type;
	char *name;
	float vec[4];
	struct GPUNodeLink *link;
	short hasinput;
	short hasoutput;
	short sockettype;
} GPUNodeStack;

GPUNodeLink *GPU_attribute(int type, char *name);
GPUNodeLink *GPU_uniform(float *num);
GPUNodeLink *GPU_dynamic_uniform(float *num);
GPUNodeLink *GPU_image(struct Image *ima, struct ImageUser *iuser);
GPUNodeLink *GPU_texture(int size, float *pixels);
GPUNodeLink *GPU_dynamic_texture(struct GPUTexture *tex);
GPUNodeLink *GPU_socket(GPUNodeStack *sock);
GPUNodeLink *GPU_builtin(GPUBuiltin builtin);

int GPU_link(GPUMaterial *mat, char *name, ...);
int GPU_stack_link(GPUMaterial *mat, char *name, GPUNodeStack *in, GPUNodeStack *out, ...);

void GPU_material_output_link(GPUMaterial *material, GPUNodeLink *link);
void GPU_material_enable_alpha(GPUMaterial *material);
GPUBlendMode GPU_material_blend_mode(GPUMaterial *material, float obcol[4]);

/* High level functions to create and use GPU materials */

GPUMaterial *GPU_material_from_blender(struct Scene *scene, struct Material *ma);
void GPU_material_free(struct Material *ma);

void GPU_materials_free();

void GPU_material_bind(GPUMaterial *material, int oblay, int viewlay, double time, int mipmap);
void GPU_material_bind_uniforms(GPUMaterial *material, float obmat[][4], float viewmat[][4], float viewinv[][4], float obcol[4]);
void GPU_material_unbind(GPUMaterial *material);
int GPU_material_bound(GPUMaterial *material);

void GPU_material_vertex_attributes(GPUMaterial *material,
	struct GPUVertexAttribs *attrib);

/* Exported shading */

typedef struct GPUShadeInput {
	GPUMaterial *gpumat;
	struct Material *mat;

	GPUNodeLink *rgb, *specrgb, *vn, *view, *vcol, *ref;
	GPUNodeLink *alpha, *refl, *spec, *emit, *har, *amb;
} GPUShadeInput;

typedef struct GPUShadeResult {
	GPUNodeLink *diff, *spec, *combined, *alpha;
} GPUShadeResult;

void GPU_shadeinput_set(GPUMaterial *mat, struct Material *ma, GPUShadeInput *shi);
void GPU_shaderesult_set(GPUShadeInput *shi, GPUShadeResult *shr);

/* Lamps */

GPULamp *GPU_lamp_from_blender(struct Scene *scene, struct Object *ob, struct Object *par);
void GPU_lamp_free(struct Object *ob);

int GPU_lamp_has_shadow_buffer(GPULamp *lamp);
void GPU_lamp_shadow_buffer_bind(GPULamp *lamp, float viewmat[][4], int *winsize, float winmat[][4]);
void GPU_lamp_shadow_buffer_unbind(GPULamp *lamp);

void GPU_lamp_update(GPULamp *lamp, int lay, float obmat[][4]);
void GPU_lamp_update_colors(GPULamp *lamp, float r, float g, float b, float energy);
int GPU_lamp_shadow_layer(GPULamp *lamp);

#ifdef __cplusplus
}
#endif

#endif /*__GPU_MATERIAL__*/

