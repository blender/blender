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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_SHRINKWRAP_H__
#define __BKE_SHRINKWRAP_H__

/** \file BKE_shrinkwrap.h
 *  \ingroup bke
 */

/* Shrinkwrap stuff */
#include "BKE_bvhutils.h"

/*
 * Shrinkwrap is composed by a set of functions and options that define the type of shrink.
 *
 * 3 modes are available:
 *    - Nearest vertex
 *	  - Nearest surface
 *    - Normal projection
 *
 * ShrinkwrapCalcData encapsulates all needed data for shrinkwrap functions.
 * (So that you don't have to pass an enormous amount of arguments to functions)
 */

struct Object;
struct Mesh;
struct MVert;
struct MDeformVert;
struct ModifierEvalContext;
struct ShrinkwrapModifierData;
struct BVHTree;
struct SpaceTransform;

typedef struct ShrinkwrapTreeData {
	Mesh *mesh;

	BVHTree *bvh;
	BVHTreeFromMesh treeData;

	float (*clnors)[3];
} ShrinkwrapTreeData;

/* Checks if the modifier needs target normals with these settings. */
bool BKE_shrinkwrap_needs_normals(int shrinkType, int shrinkMode);

/* Initializes the mesh data structure from the given mesh and settings. */
bool BKE_shrinkwrap_init_tree(struct ShrinkwrapTreeData *data, Mesh *mesh, int shrinkType, int shrinkMode, bool force_normals);

/* Frees the tree data if necessary. */
void BKE_shrinkwrap_free_tree(struct ShrinkwrapTreeData *data);

/* Implementation of the Shrinkwrap modifier */
void shrinkwrapModifier_deform(struct ShrinkwrapModifierData *smd, struct Scene *scene, struct Object *ob, struct Mesh *mesh,
                               float (*vertexCos)[3], int numVerts);

/*
 * This function casts a ray in the given BVHTree.. but it takes into consideration the space_transform, that is:
 *
 * if transf was configured with "SPACE_TRANSFORM_SETUP( &transf,  ob1, ob2 )"
 * then the input (vert, dir, BVHTreeRayHit) must be defined in ob1 coordinates space
 * and the BVHTree must be built in ob2 coordinate space.
 *
 * Thus it provides an easy way to cast the same ray across several trees
 * (where each tree was built on its own coords space)
 */
bool BKE_shrinkwrap_project_normal(
        char options, const float vert[3], const float dir[3], const float ray_radius,
        const struct SpaceTransform *transf, struct ShrinkwrapTreeData *tree, BVHTreeRayHit *hit);

/* Computes a smooth normal of the target (if applicable) at the hit location. */
void BKE_shrinkwrap_compute_smooth_normal(
        const struct ShrinkwrapTreeData *tree, const struct SpaceTransform *transform,
        int looptri_idx, const float hit_co[3], const float hit_no[3], float r_no[3]);

/* Apply the shrink to surface modes to the given original coordinates and nearest point. */
void BKE_shrinkwrap_snap_point_to_surface(
        const struct ShrinkwrapTreeData *tree, const struct SpaceTransform *transform,
        int mode, int hit_idx, const float hit_co[3], const float hit_no[3], float goal_dist,
        const float point_co[3], float r_point_co[3]);

/*
 * NULL initializers to local data
 */
#define NULL_ShrinkwrapCalcData {NULL, }
#define NULL_BVHTreeFromMesh    {NULL, }
#define NULL_BVHTreeRayHit      {NULL, }
#define NULL_BVHTreeNearest     {0, }

#endif  /* __BKE_SHRINKWRAP_H__ */
