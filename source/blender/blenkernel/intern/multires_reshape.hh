/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_virtual_array.hh"

#include "BKE_multires.hh"

struct Depsgraph;
struct GridPaintMask;
struct MDisps;
struct Mesh;
struct MultiresModifierData;
struct Object;
namespace blender::bke::subdiv {
struct Subdiv;
}
struct SubdivCCG;

struct MultiresReshapeContext {
  /* NOTE: Only available when context is initialized from object. */
  Depsgraph *depsgraph;
  Object *object;

  MultiresModifierData *mmd;

  /* Base mesh from original object.
   * NOTE: Does NOT include any leading modifiers in it. */
  Mesh *base_mesh;
  blender::Span<blender::float3> base_positions;
  blender::Span<blender::int2> base_edges;
  blender::OffsetIndices<int> base_faces;
  blender::Span<int> base_corner_verts;
  blender::Span<int> base_corner_edges;

  /* Subdivision surface created for multires modifier.
   *
   * The coarse mesh of this subdivision surface is a base mesh with all deformation modifiers
   * leading multires applied on it. */
  blender::bke::subdiv::Subdiv *subdiv;
  bool need_free_subdiv;

  struct {
    /* Level at which displacement is being assigned to.
     * It will be propagated up from this level to top.level. */
    int level;

    /* Grid size for reshape.level. */
    int grid_size;
  } reshape;

  struct {
    /* Top level of the displacement grids.
     * The displacement will be propagated up to this level. */
    int level;

    /* Grid size for top.level. */
    int grid_size;
  } top;

  struct {
    /* Copy of original displacement and painting masks. */
    MDisps *mdisps;
    GridPaintMask *grid_paint_masks;
  } orig;

  /* Number of grids which are required for base_mesh. */
  int num_grids;

  /* Destination displacement and mask.
   * Points to a custom data on a destination mesh. */
  MDisps *mdisps;
  GridPaintMask *grid_paint_masks;

  /* Indexed by face index, gives first grid index of the face. */
  blender::Array<int> face_start_grid_index;

  /* Indexed by grid index, contains face index in the base mesh from which the grid has
   * been created (in other words, index of a face which contains loop corresponding to the grid
   * index). */
  blender::Array<int> grid_to_face_index;

  /* Indexed by ptex face index, gives first grid index of the ptex face.
   *
   * For non-quad base faces ptex face is created for every face corner, so it's similar to a
   * grid in this case. In this case start grid index will be the only one for this ptex face.
   *
   * For quad base faces there is a single ptex face but 4 grids. So in this case there will be
   * 4 grids for the ptex, starting at a value stored in this mapping. */
  blender::Array<int> ptex_start_grid_index;

  /* Indexed by base face index, returns first ptex face index corresponding
   * to that base face. */
  blender::Span<int> face_ptex_offset;

  /* Vertex crease custom data layer, empty if none is present. */
  blender::VArraySpan<float> cd_vert_crease;
  /* Edge crease custom data layer, empty if none is present. */
  blender::VArraySpan<float> cd_edge_crease;
};

/**
 * Coordinate which identifies element of a grid.
 * This is directly related on how #CD_MDISPS stores displacement.
 */
struct GridCoord {
  int grid_index;
  float u, v;
};

/**
 * Coordinate within ptex, which is what OpenSubdiv API operates on.
 */
struct PTexCoord {
  int ptex_face_index;
  float u, v;
};

/**
 * Element of a grid data stored in the destination mesh.
 * This is where reshaped coordinates and mask values will be written to.
 */
struct ReshapeGridElement {
  blender::float3 *displacement;
  float *mask;
};

struct ReshapeConstGridElement {
  blender::float3 displacement;
  float mask;
};

/* --------------------------------------------------------------------
 * Construct/destruct reshape context.
 */

/**
 * Create subdivision surface descriptor which is configured for surface evaluation at a given
 * multi-res modifier.
 */
blender::bke::subdiv::Subdiv *multires_reshape_create_subdiv(Depsgraph *depsgraph,
                                                             Object *object,
                                                             const MultiresModifierData *mmd);

/**
 * \note Initialized base mesh to object's mesh, the Subdivision is created from the deformed
 * mesh prior to the multi-res modifier if depsgraph is not NULL. If the depsgraph is NULL
 * then Subdivision is created from base mesh (without any deformation applied).
 */
bool multires_reshape_context_create_from_object(MultiresReshapeContext *reshape_context,
                                                 Depsgraph *depsgraph,
                                                 Object *object,
                                                 MultiresModifierData *mmd);

bool multires_reshape_context_create_from_base_mesh(MultiresReshapeContext *reshape_context,
                                                    Depsgraph *depsgraph,
                                                    Object *object,
                                                    MultiresModifierData *mmd);

bool multires_reshape_context_create_from_ccg(MultiresReshapeContext *reshape_context,
                                              SubdivCCG *subdiv_ccg,
                                              Mesh *base_mesh,
                                              int top_level);

bool multires_reshape_context_create_from_modifier(MultiresReshapeContext *reshape_context,
                                                   Object *object,
                                                   MultiresModifierData *mmd,
                                                   int top_level);

bool multires_reshape_context_create_from_subdiv(MultiresReshapeContext *reshape_context,
                                                 Object *object,
                                                 MultiresModifierData *mmd,
                                                 blender::bke::subdiv::Subdiv *subdiv,
                                                 int top_level);

void multires_reshape_free_original_grids(MultiresReshapeContext *reshape_context);
void multires_reshape_context_free(MultiresReshapeContext *reshape_context);

/* --------------------------------------------------------------------
 * Helper accessors.
 */

/**
 * For the given grid index get index of face it was created for.
 */
int multires_reshape_grid_to_face_index(const MultiresReshapeContext *reshape_context,
                                        int grid_index);

/**
 * For the given grid index get corner of a face it was created for.
 */
int multires_reshape_grid_to_corner(const MultiresReshapeContext *reshape_context, int grid_index);

bool multires_reshape_is_quad_face(const MultiresReshapeContext *reshape_context, int face_index);

/**
 * For the given grid index get index of corresponding PTEX face.
 */
int multires_reshape_grid_to_ptex_index(const MultiresReshapeContext *reshape_context,
                                        int grid_index);

/**
 * Convert normalized coordinate within a grid to a normalized coordinate within a PTEX face.
 */
PTexCoord multires_reshape_grid_coord_to_ptex(const MultiresReshapeContext *reshape_context,
                                              const GridCoord *grid_coord);

/**
 * Convert a normalized coordinate within a PTEX face to a normalized coordinate within a grid.
 */
GridCoord multires_reshape_ptex_coord_to_grid(const MultiresReshapeContext *reshape_context,
                                              const PTexCoord *ptex_coord);

/**
 * Calculate tangent matrix which converts displacement to an object vector.
 * Is calculated for the given surface derivatives at a given base face corner.
 */
void multires_reshape_tangent_matrix_for_corner(const MultiresReshapeContext *reshape_context,
                                                int face_index,
                                                int corner,
                                                const blender::float3 &dPdu,
                                                const blender::float3 &dPdv,
                                                blender::float3x3 &r_tangent_matrix);

/**
 * Get grid elements which are to be reshaped at a given or PTEX coordinate.
 * The data is coming from final custom mdata layers.
 */
ReshapeGridElement multires_reshape_grid_element_for_grid_coord(
    const MultiresReshapeContext *reshape_context, const GridCoord *grid_coord);
ReshapeGridElement multires_reshape_grid_element_for_ptex_coord(
    const MultiresReshapeContext *reshape_context, const PTexCoord *ptex_coord);

/**
 * Get original grid element for the given coordinate.
 */
ReshapeConstGridElement multires_reshape_orig_grid_element_for_grid_coord(
    const MultiresReshapeContext *reshape_context, const GridCoord *grid_coord);

/* --------------------------------------------------------------------
 * Sample limit surface of the base mesh.
 */

/**
 * Evaluate limit surface created from base mesh.
 * This is the limit surface which defines tangent space for MDisps.
 */
void multires_reshape_evaluate_base_mesh_limit_at_grid(
    const MultiresReshapeContext *reshape_context,
    const GridCoord *grid_coord,
    blender::float3 &r_P,
    blender::float3x3 &r_tangent_matrix);

/* --------------------------------------------------------------------
 * Custom data preparation.
 */

/**
 * Make sure custom data is allocated for the given level.
 */
void multires_reshape_ensure_grids(Mesh *mesh, int level);

/* --------------------------------------------------------------------
 * Functions specific to reshaping from a set of vertices in an object position.
 */

/**
 * Set displacement grids values at a reshape level to an object coordinates of the given source.
 *
 * \returns true if all coordinates were assigned.
 *
 * False will be returned if the number of vertex coordinates did not match required number of
 * vertices at a reshape level.
 */
bool multires_reshape_assign_final_coords_from_vertcos(
    const MultiresReshapeContext *reshape_context, blender::Span<blender::float3> positions);

/* --------------------------------------------------------------------
 * Functions specific to reshaping from CCG.
 */

/**
 * Store final object-space coordinates in the displacement grids.
 * The reason why displacement grids are used for storage is based on memory
 * footprint optimization.
 *
 * \note Displacement grids to be at least at a reshape level.
 *
 * \return true if all coordinates have been updated.
 */
bool multires_reshape_assign_final_coords_from_ccg(const MultiresReshapeContext *reshape_context,
                                                   SubdivCCG *subdiv_ccg);

/* --------------------------------------------------------------------
 * Functions specific to reshaping from MDISPS.
 */

/**
 * Reads and writes to the current mesh #CD_MDISPS.
 */
void multires_reshape_assign_final_coords_from_mdisps(
    const MultiresReshapeContext *reshape_context);

/**
 * Reads from original #CD_MIDSPS, writes to the current mesh #CD_MDISPS.
 */
void multires_reshape_assign_final_elements_from_orig_mdisps(
    const MultiresReshapeContext *reshape_context);

/* --------------------------------------------------------------------
 * Displacement smooth.
 */

/**
 * Operates on a displacement grids (CD_MDISPS) which contains object space coordinates stored for
 * the reshape level.
 *
 * The result is grids which are defining mesh with a smooth surface and details starting from
 * reshape level up to top level added back from original displacement grids.
 */
void multires_reshape_smooth_object_grids_with_details(
    const MultiresReshapeContext *reshape_context);

/**
 * Operates on a displacement grids (CD_MDISPS) which contains object space-coordinates stored for
 * the reshape level.
 *
 * Makes it so surface on top level looks smooth. Details are not preserved
 */
void multires_reshape_smooth_object_grids(const MultiresReshapeContext *reshape_context,
                                          MultiresSubdivideModeType mode);

/* --------------------------------------------------------------------
 * Displacement, space conversion.
 */

/**
 * Store original grid data, so then it's possible to calculate delta from it and add
 * high-frequency content on top of reshaped grids.
 */
void multires_reshape_store_original_grids(MultiresReshapeContext *reshape_context);

void multires_reshape_object_grids_to_tangent_displacement(
    const MultiresReshapeContext *reshape_context);

/* --------------------------------------------------------------------
 * Apply base.
 */

/**
 * Update mesh coordinates to the final positions of displacement in object space.
 * This is effectively desired position of base mesh vertices after canceling out displacement.
 *
 * \note Expects that mesh's CD_MDISPS has been set to object space positions.
 */
void multires_reshape_apply_base_update_mesh_coords(MultiresReshapeContext *reshape_context);

/**
 * Perform better fitting of the base mesh so its subdivided version brings vertices to their
 * desired locations.
 */
void multires_reshape_apply_base_refit_base_mesh(MultiresReshapeContext *reshape_context);

/**
 * Refine subdivision surface to the new positions of the base mesh.
 */
void multires_reshape_apply_base_refine_from_base(MultiresReshapeContext *reshape_context);

/**
 * Refine subdivision surface to the new positions of the deformed mesh (base mesh with all
 * modifiers leading the multi-res applied).
 *
 * \note Will re-evaluate all leading modifiers, so it's not cheap.
 */
void multires_reshape_apply_base_refine_from_deform(MultiresReshapeContext *reshape_context);
