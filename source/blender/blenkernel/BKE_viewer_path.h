/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * A #ViewerPath is a path to data that is viewed/debugged by the user. It is a list of
 * #ViewerPathElem.
 *
 * This is only used for geometry nodes currently. When the user activates a viewer node the
 * corresponding path contains the following elements:
 * - Object the viewer is activated on.
 * - Modifier that contains the corresponding geometry node group.
 * - Node tree path in case the viewer node is in a nested node group.
 * - Viewer node name.
 *
 * The entire path is necessary (instead of just the combination of node group and viewer name),
 * because the same node group may be used in many different places.
 *
 * This file contains basic functions for creating/deleting a #ViewerPath. For more use-case
 * specific functions look in `ED_viewer_path.hh`.
 */

#include "DNA_viewer_path_types.h"

struct BlendWriter;
struct BlendDataReader;
struct BlendLibReader;
struct LibraryForeachIDData;
struct Library;
struct IDRemapper;

#ifdef __cplusplus
extern "C" {
#endif

void BKE_viewer_path_init(ViewerPath *viewer_path);
void BKE_viewer_path_clear(ViewerPath *viewer_path);
void BKE_viewer_path_copy(ViewerPath *dst, const ViewerPath *src);
bool BKE_viewer_path_equal(const ViewerPath *a, const ViewerPath *b);
void BKE_viewer_path_blend_write(struct BlendWriter *writer, const ViewerPath *viewer_path);
void BKE_viewer_path_blend_read_data(struct BlendDataReader *reader, ViewerPath *viewer_path);
void BKE_viewer_path_blend_read_lib(struct BlendLibReader *reader,
                                    struct ID *self_id,
                                    ViewerPath *viewer_path);
void BKE_viewer_path_foreach_id(struct LibraryForeachIDData *data, ViewerPath *viewer_path);
void BKE_viewer_path_id_remap(ViewerPath *viewer_path, const struct IDRemapper *mappings);

ViewerPathElem *BKE_viewer_path_elem_new(ViewerPathElemType type);
IDViewerPathElem *BKE_viewer_path_elem_new_id(void);
ModifierViewerPathElem *BKE_viewer_path_elem_new_modifier(void);
GroupNodeViewerPathElem *BKE_viewer_path_elem_new_group_node(void);
SimulationZoneViewerPathElem *BKE_viewer_path_elem_new_simulation_zone(void);
ViewerNodeViewerPathElem *BKE_viewer_path_elem_new_viewer_node(void);
RepeatZoneViewerPathElem *BKE_viewer_path_elem_new_repeat_zone(void);
ViewerPathElem *BKE_viewer_path_elem_copy(const ViewerPathElem *src);
bool BKE_viewer_path_elem_equal(const ViewerPathElem *a, const ViewerPathElem *b);
void BKE_viewer_path_elem_free(ViewerPathElem *elem);

#ifdef __cplusplus
}
#endif
