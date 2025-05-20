/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "DNA_modifier_enums.h"

struct Depsgraph;
struct Object;
struct ReportList;
struct SpaceTransform;

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
