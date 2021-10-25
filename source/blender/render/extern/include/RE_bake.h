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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file RE_bake.h
 *  \ingroup render
 */

#ifndef __RE_BAKE_H__
#define __RE_BAKE_H__

struct ImBuf;
struct Render;
struct Mesh;

typedef struct BakeImage {
	struct Image *image;
	int width;
	int height;
	size_t offset;
} BakeImage;

typedef struct BakeImages {
	BakeImage *data; /* all the images of an object */
	int *lookup;     /* lookup table from Material to BakeImage */
	int size;
} BakeImages;

typedef struct BakePixel {
	int primitive_id, object_id;
	float uv[2];
	float du_dx, du_dy;
	float dv_dx, dv_dy;
} BakePixel;

typedef struct BakeHighPolyData {
	struct Object *ob;
	struct ModifierData *tri_mod;
	struct Mesh *me;
	char restrict_flag;
	bool is_flip_object;

	float obmat[4][4];
	float imat[4][4];
} BakeHighPolyData;

/* external_engine.c */
bool RE_bake_has_engine(struct Render *re);

bool RE_bake_engine(
        struct Render *re, struct Object *object, const int object_id, const BakePixel pixel_array[],
        const size_t num_pixels, const int depth, const ScenePassType pass_type, const int pass_filter, float result[]);

/* bake.c */
int RE_pass_depth(const ScenePassType pass_type);
bool RE_bake_internal(
        struct Render *re, struct Object *object, const BakePixel pixel_array[],
        const size_t num_pixels, const int depth, const ScenePassType pass_type, float result[]);

bool RE_bake_pixels_populate_from_objects(
        struct Mesh *me_low, BakePixel pixel_array_from[], BakePixel pixel_array_to[],
        BakeHighPolyData highpoly[], const int tot_highpoly, const size_t num_pixels, const bool is_custom_cage,
        const float cage_extrusion, float mat_low[4][4], float mat_cage[4][4], struct Mesh *me_cage);

void RE_bake_pixels_populate(
        struct Mesh *me, struct BakePixel *pixel_array,
        const size_t num_pixels, const struct BakeImages *bake_images, const char *uv_layer);

void RE_bake_mask_fill(const BakePixel pixel_array[], const size_t num_pixels, char *mask);

void RE_bake_margin(struct ImBuf *ibuf, char *mask, const int margin);

void RE_bake_normal_world_to_object(
        const BakePixel pixel_array[], const size_t num_pixels, const int depth, float result[],
        struct Object *ob, const BakeNormalSwizzle normal_swizzle[3]);
void RE_bake_normal_world_to_tangent(
        const BakePixel pixel_array[], const size_t num_pixels, const int depth, float result[],
        struct Mesh *me, const BakeNormalSwizzle normal_swizzle[3], float mat[4][4]);
void RE_bake_normal_world_to_world(
        const BakePixel pixel_array[], const size_t num_pixels, const int depth, float result[],
        const BakeNormalSwizzle normal_swizzle[3]);

void RE_bake_ibuf_clear(struct Image *image, const bool is_tangent);

#endif  /* __RE_BAKE_H__ */
