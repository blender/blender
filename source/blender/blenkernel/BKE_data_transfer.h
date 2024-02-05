/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_compiler_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct Object;
struct ReportList;
struct Scene;
struct SpaceTransform;

/* Warning, those def are stored in files (TransferData modifier), *DO NOT* modify those values. */
enum {
  DT_TYPE_MDEFORMVERT = 1 << 0,
  DT_TYPE_SHAPEKEY = 1 << 1,
  DT_TYPE_SKIN = 1 << 2,
  DT_TYPE_BWEIGHT_VERT = 1 << 3,

  DT_TYPE_SHARP_EDGE = 1 << 8,
  DT_TYPE_SEAM = 1 << 9,
  DT_TYPE_CREASE = 1 << 10,
  DT_TYPE_BWEIGHT_EDGE = 1 << 11,
  DT_TYPE_FREESTYLE_EDGE = 1 << 12,

  DT_TYPE_MPROPCOL_VERT = 1 << 16,
  DT_TYPE_LNOR = 1 << 17,

  DT_TYPE_UV = 1 << 24,
  DT_TYPE_SHARP_FACE = 1 << 25,
  DT_TYPE_FREESTYLE_FACE = 1 << 26,
  DT_TYPE_MLOOPCOL_VERT = 1 << 27,
  DT_TYPE_MPROPCOL_LOOP = 1 << 28,
  DT_TYPE_MLOOPCOL_LOOP = 1 << 29,
  DT_TYPE_VCOL_ALL = (1 << 16) | (1 << 27) | (1 << 28) | (1 << 29),
#define DT_TYPE_MAX 30

  DT_TYPE_VERT_ALL = DT_TYPE_MDEFORMVERT | DT_TYPE_SHAPEKEY | DT_TYPE_SKIN | DT_TYPE_BWEIGHT_VERT |
                     DT_TYPE_MPROPCOL_VERT | DT_TYPE_MLOOPCOL_VERT,
  DT_TYPE_EDGE_ALL = DT_TYPE_SHARP_EDGE | DT_TYPE_SEAM | DT_TYPE_CREASE | DT_TYPE_BWEIGHT_EDGE |
                     DT_TYPE_FREESTYLE_EDGE,
  DT_TYPE_LOOP_ALL = DT_TYPE_LNOR | DT_TYPE_UV | DT_TYPE_MPROPCOL_LOOP | DT_TYPE_MLOOPCOL_LOOP,
  DT_TYPE_POLY_ALL = DT_TYPE_UV | DT_TYPE_SHARP_FACE | DT_TYPE_FREESTYLE_FACE,
};

void BKE_object_data_transfer_dttypes_to_cdmask(int dtdata_types,
                                                struct CustomData_MeshMasks *r_data_masks);
/**
 * Check what can do each layer type
 * (if it is actually handled by transfer-data, if it supports advanced mixing.
 */
bool BKE_object_data_transfer_get_dttypes_capacity(int dtdata_types,
                                                   bool *r_advanced_mixing,
                                                   bool *r_threshold);
int BKE_object_data_transfer_get_dttypes_item_types(int dtdata_types);

int BKE_object_data_transfer_dttype_to_cdtype(int dtdata_type);
int BKE_object_data_transfer_dttype_to_srcdst_index(int dtdata_type);

#define DT_DATATYPE_IS_VERT(_dt) \
  ELEM(_dt, \
       DT_TYPE_MDEFORMVERT, \
       DT_TYPE_SHAPEKEY, \
       DT_TYPE_SKIN, \
       DT_TYPE_BWEIGHT_VERT, \
       DT_TYPE_MLOOPCOL_VERT, \
       DT_TYPE_MPROPCOL_VERT)
#define DT_DATATYPE_IS_EDGE(_dt) \
  ELEM(_dt, \
       DT_TYPE_CREASE, \
       DT_TYPE_SHARP_EDGE, \
       DT_TYPE_SEAM, \
       DT_TYPE_BWEIGHT_EDGE, \
       DT_TYPE_FREESTYLE_EDGE)
#define DT_DATATYPE_IS_LOOP(_dt) \
  ELEM(_dt, DT_TYPE_UV, DT_TYPE_LNOR, DT_TYPE_MLOOPCOL_LOOP, DT_TYPE_MPROPCOL_LOOP)
#define DT_DATATYPE_IS_FACE(_dt) ELEM(_dt, DT_TYPE_UV, DT_TYPE_SHARP_FACE, DT_TYPE_FREESTYLE_FACE)

#define DT_DATATYPE_IS_MULTILAYERS(_dt) \
  ELEM(_dt, \
       DT_TYPE_MDEFORMVERT, \
       DT_TYPE_SHAPEKEY, \
       DT_TYPE_MPROPCOL_VERT, \
       DT_TYPE_MLOOPCOL_VERT, \
       DT_TYPE_MPROPCOL_VERT | DT_TYPE_MLOOPCOL_VERT, \
       DT_TYPE_MPROPCOL_LOOP, \
       DT_TYPE_MLOOPCOL_LOOP, \
       DT_TYPE_MPROPCOL_LOOP | DT_TYPE_MLOOPCOL_LOOP, \
       DT_TYPE_UV)

enum {
  DT_MULTILAYER_INDEX_INVALID = -1,
  DT_MULTILAYER_INDEX_MDEFORMVERT = 0,
  DT_MULTILAYER_INDEX_SHAPEKEY = 1,
  DT_MULTILAYER_INDEX_VCOL_LOOP = 2,
  DT_MULTILAYER_INDEX_UV = 3,
  DT_MULTILAYER_INDEX_VCOL_VERT = 4,
  DT_MULTILAYER_INDEX_MAX = 5,
};

/* Below we keep positive values for real layers idx (generated dynamically). */

/* How to select data layers, for types supporting multi-layers.
 * Here too, some options are highly dependent on type of transferred data! */
enum {
  DT_LAYERS_ACTIVE_SRC = -1,
  DT_LAYERS_ALL_SRC = -2,
  /* Datatype-specific. */
  DT_LAYERS_VGROUP_SRC = 1 << 8,
  DT_LAYERS_VGROUP_SRC_BONE_SELECT = -(DT_LAYERS_VGROUP_SRC | 1),
  DT_LAYERS_VGROUP_SRC_BONE_DEFORM = -(DT_LAYERS_VGROUP_SRC | 2),
  /* Other types-related modes... */
};

/* How to map a source layer to a destination layer, for types supporting multi-layers.
 * NOTE: if no matching layer can be found, it will be created. */
enum {
  DT_LAYERS_ACTIVE_DST = -1, /* Only for DT_LAYERS_FROMSEL_ACTIVE. */
  DT_LAYERS_NAME_DST = -2,
  DT_LAYERS_INDEX_DST = -3,
#if 0 /* TODO */
  DT_LAYERS_CREATE_DST = -4, /* Never replace existing data in dst, always create new layers. */
#endif
};

/**
 * Transfer data *layout* of selected types from source to destination object.
 * By default, it only creates new data layers if needed on \a ob_dst.
 * If \a use_delete is true, it will also delete data layers on \a ob_dst that do not match those
 * from \a ob_src, to get (as much as possible) exact copy of source data layout.
 */
void BKE_object_data_transfer_layout(struct Depsgraph *depsgraph,
                                     struct Object *ob_src,
                                     struct Object *ob_dst,
                                     int data_types,
                                     bool use_delete,
                                     const int fromlayers_select[DT_MULTILAYER_INDEX_MAX],
                                     const int tolayers_select[DT_MULTILAYER_INDEX_MAX]);

bool BKE_object_data_transfer_mesh(struct Depsgraph *depsgraph,
                                   struct Object *ob_src,
                                   struct Object *ob_dst,
                                   int data_types,
                                   bool use_create,
                                   int map_vert_mode,
                                   int map_edge_mode,
                                   int map_loop_mode,
                                   int map_face_mode,
                                   struct SpaceTransform *space_transform,
                                   bool auto_transform,
                                   float max_distance,
                                   float ray_radius,
                                   float islands_handling_precision,
                                   const int fromlayers_select[DT_MULTILAYER_INDEX_MAX],
                                   const int tolayers_select[DT_MULTILAYER_INDEX_MAX],
                                   int mix_mode,
                                   float mix_factor,
                                   const char *vgroup_name,
                                   bool invert_vgroup,
                                   struct ReportList *reports);
bool BKE_object_data_transfer_ex(struct Depsgraph *depsgraph,
                                 struct Object *ob_src,
                                 struct Object *ob_dst,
                                 struct Mesh *me_dst,
                                 int data_types,
                                 bool use_create,
                                 int map_vert_mode,
                                 int map_edge_mode,
                                 int map_loop_mode,
                                 int map_face_mode,
                                 struct SpaceTransform *space_transform,
                                 bool auto_transform,
                                 float max_distance,
                                 float ray_radius,
                                 float islands_handling_precision,
                                 const int fromlayers_select[DT_MULTILAYER_INDEX_MAX],
                                 const int tolayers_select[DT_MULTILAYER_INDEX_MAX],
                                 int mix_mode,
                                 float mix_factor,
                                 const char *vgroup_name,
                                 bool invert_vgroup,
                                 struct ReportList *reports);

#ifdef __cplusplus
}
#endif
