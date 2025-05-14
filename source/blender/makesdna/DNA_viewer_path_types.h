/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_listBase.h"

#include <stdint.h>

struct ID;
struct bNodeTree;

typedef enum ViewerPathElemType {
  VIEWER_PATH_ELEM_TYPE_ID = 0,
  VIEWER_PATH_ELEM_TYPE_MODIFIER = 1,
  VIEWER_PATH_ELEM_TYPE_GROUP_NODE = 2,
  VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE = 3,
  VIEWER_PATH_ELEM_TYPE_VIEWER_NODE = 4,
  VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE = 5,
  VIEWER_PATH_ELEM_TYPE_FOREACH_GEOMETRY_ELEMENT_ZONE = 6,
  VIEWER_PATH_ELEM_TYPE_EVALUATE_CLOSURE = 7,
} ViewerPathElemType;

typedef struct ViewerPathElem {
  struct ViewerPathElem *next, *prev;
  int type;
  char _pad[4];
  char *ui_name;
} ViewerPathElem;

typedef struct IDViewerPathElem {
  ViewerPathElem base;
  struct ID *id;
} IDViewerPathElem;

typedef struct ModifierViewerPathElem {
  ViewerPathElem base;
  /** #ModifierData.persistent_uid. */
  int modifier_uid;
  char _pad[4];
} ModifierViewerPathElem;

typedef struct GroupNodeViewerPathElem {
  ViewerPathElem base;

  int32_t node_id;
  char _pad1[4];
} GroupNodeViewerPathElem;

typedef struct SimulationZoneViewerPathElem {
  ViewerPathElem base;

  int32_t sim_output_node_id;
  char _pad1[4];
} SimulationZoneViewerPathElem;

typedef struct RepeatZoneViewerPathElem {
  ViewerPathElem base;

  int repeat_output_node_id;
  int iteration;
} RepeatZoneViewerPathElem;

typedef struct ForeachGeometryElementZoneViewerPathElem {
  ViewerPathElem base;

  int zone_output_node_id;
  int index;
} ForeachGeometryElementZoneViewerPathElem;

typedef struct ViewerNodeViewerPathElem {
  ViewerPathElem base;

  int32_t node_id;
  char _pad1[4];
} ViewerNodeViewerPathElem;

typedef struct EvaluateClosureNodeViewerPathElem {
  ViewerPathElem base;

  /** The identifier of the node that evaluates the closure. */
  int32_t evaluate_node_id;
  /** The identifier of the output node of the closure zone that is evaluated. */
  int32_t source_output_node_id;
  /** The node tree that contains the closure zone that is evaluated. */
  struct bNodeTree *source_node_tree;
} EvaluateClosureNodeViewerPathElem;

typedef struct ViewerPath {
  /** List of #ViewerPathElem. */
  ListBase path;
} ViewerPath;
