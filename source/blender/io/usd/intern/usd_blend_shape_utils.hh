/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <pxr/usd/usd/prim.h>

struct Key;
struct Mesh;
struct Object;

namespace blender::io::usd {

/* Name of the temporary USD primvar for storing blend shape
 * weight time samples on the mesh before they are copied
 * to the bound skeleton. */
extern pxr::TfToken TempBlendShapeWeightsPrimvarName;

struct ImportSettings;

/**
 * Return the shape key on the given mesh object.
 *
 * \param obj: The mesh object
 * \return The shape key on the given object's mesh data, or
 *         null if the object isn't a mesh.
 */
const Key *get_mesh_shape_key(const Object *obj);

/**
 * Query whether the given object is a mesh with relative
 * shape keys.
 *
 * \param obj: The mesh object
 * \return True if the object is a mesh with shape keys, false otherwise
 */
bool is_mesh_with_shape_keys(const Object *obj);

/**
 * Convert shape keys on the given object to USD blend shapes. The blend-shapes
 * will be added to the stage as children of the given USD mesh prim. The blend-shape
 * names and targets will also be set as properties on the primitive.
 *
 * \param stage: The stage
 * \param obj: The mesh object whose shape keys will be converted to blend shapes
 * \param mesh_prim: The USD mesh that will be assigned the blend shape targets
 */
void create_blend_shapes(pxr::UsdStageRefPtr stage,
                         const Object *obj,
                         const pxr::UsdPrim &mesh_prim);

/**
 * Return the current weight values of the given key.
 *
 * \param key: The key whose values will be queried
 * \return The array of key values.
 */
pxr::VtFloatArray get_blendshape_weights(const Key *key);

/**
 * USD implementations expect that a mesh with blend shape targets
 * be bound to a skeleton with an animation that provides the blend
 * shape weights. If the given mesh is not already bound to a skeleton
 * this function will create a dummy skeleton with a single joint and
 * will bind it to the mesh. This is typically required if the source
 * Blender mesh has shape keys but not an armature deformer.
 *
 * This function will also create a skel animation prim as a child of
 * the skeleton and will copy the weight time samples from a temporary
 * primvar on the mesh to the animation prim.
 *
 * \param stage: The stage
 * \param mesh_prim: The USD mesh to which the skeleton will be bound
 */
void ensure_blend_shape_skeleton(pxr::UsdStageRefPtr stage, pxr::UsdPrim &mesh_prim);

/**
 * Query whether the object is a mesh with animated shape keys.
 *
 * \param obj: The mesh object
 * \return True if the object has animated keys, false otherwise.
 */
bool has_animated_mesh_shape_key(const Object *obj);

/**
 * Return the block names of the given shape key.
 *
 * \param key: The key to query
 * \return The list of key block names.
 */
pxr::VtTokenArray get_blend_shape_names(const Key *key);

/**
 * Return the list of blend shape names given by the mesh
 * prim's 'blendShapes' attribute value.
 *
 * \param mesh_prim: The prim to query
 * \return The list of blend shape names.
 */
pxr::VtTokenArray get_blend_shapes_attr_value(const pxr::UsdPrim &mesh_prim);

/**
 * When multiple meshes with blend shape animations are bound to one skeleton, USD implementations
 * typically expect these animations to be combined in a single animation on the skeleton.  This
 * function creates an animation prim as a child of the skeleton and merges the blend shape time
 * samples from multiple meshes in a single attribute on the animation.  Merging the weight samples
 * requires handling blend shape name collisions by generating unique names for the combined
 * result.
 *
 * \param stage: The stage
 * \param skel_path: Path to the skeleton
 * \param mesh_paths: Paths to one or more mesh prims bound to the skeleton
 */
void remap_blend_shape_anim(pxr::UsdStageRefPtr stage,
                            const pxr::SdfPath &skel_path,
                            const pxr::SdfPathSet &mesh_paths);

/**
 * If the given object is a mesh with shape keys, return a copy of the object's pre-modified mesh
 * with its verts in the shape key basis positions. The returned mesh must be freed by the caller.
 *
 * \param obj: The mesh object with shape keys
 * \return A new mesh corresponding to the shape key basis shape, or null if the object
 *         isn't a mesh or has no shape keys.
 */
Mesh *get_shape_key_basis_mesh(Object *obj);

}  // namespace blender::io::usd
