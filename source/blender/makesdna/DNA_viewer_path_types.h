/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_listBase.h"

#include <cstdint>

struct ID;
struct bNodeTree;

enum ViewerPathElemType {
  VIEWER_PATH_ELEM_TYPE_ID = 0,
  VIEWER_PATH_ELEM_TYPE_MODIFIER = 1,
  VIEWER_PATH_ELEM_TYPE_GROUP_NODE = 2,
  VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE = 3,
  VIEWER_PATH_ELEM_TYPE_VIEWER_NODE = 4,
  VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE = 5,
  VIEWER_PATH_ELEM_TYPE_FOREACH_GEOMETRY_ELEMENT_ZONE = 6,
  VIEWER_PATH_ELEM_TYPE_EVALUATE_CLOSURE = 7,
};

struct ViewerPathElem {
  struct ViewerPathElem *next = nullptr, *prev = nullptr;
  int type = 0;
  char _pad[4] = {};
  char *ui_name = nullptr;
};

struct IDViewerPathElem {
  ViewerPathElem base;
  struct ID *id = nullptr;
};

struct ModifierViewerPathElem {
  ViewerPathElem base;
  /** #ModifierData.persistent_uid. */
  int modifier_uid = 0;
  char _pad[4] = {};
};

struct GroupNodeViewerPathElem {
  ViewerPathElem base;

  int32_t node_id = 0;
  char _pad1[4] = {};
};

struct SimulationZoneViewerPathElem {
  ViewerPathElem base;

  int32_t sim_output_node_id = 0;
  char _pad1[4] = {};
};

struct RepeatZoneViewerPathElem {
  ViewerPathElem base;

  int repeat_output_node_id = 0;
  int iteration = 0;
};

struct ForeachGeometryElementZoneViewerPathElem {
  ViewerPathElem base;

  int zone_output_node_id = 0;
  int index = 0;
};

struct ViewerNodeViewerPathElem {
  ViewerPathElem base;

  int32_t node_id = 0;
  char _pad1[4] = {};
};

struct EvaluateClosureNodeViewerPathElem {
  ViewerPathElem base;

  /** The identifier of the node that evaluates the closure. */
  int32_t evaluate_node_id = 0;
  /** The identifier of the output node of the closure zone that is evaluated. */
  int32_t source_output_node_id = 0;
  /** The node tree that contains the closure zone that is evaluated. */
  struct bNodeTree *source_node_tree = nullptr;
};

struct ViewerPath {
  ListBaseT<ViewerPathElem> path = {nullptr, nullptr};
};
