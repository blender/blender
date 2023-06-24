/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

struct ID;

typedef enum ViewerPathElemType {
  VIEWER_PATH_ELEM_TYPE_ID = 0,
  VIEWER_PATH_ELEM_TYPE_MODIFIER = 1,
  VIEWER_PATH_ELEM_TYPE_GROUP_NODE = 2,
  VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE = 3,
  VIEWER_PATH_ELEM_TYPE_VIEWER_NODE = 4,
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
  char *modifier_name;
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

typedef struct ViewerNodeViewerPathElem {
  ViewerPathElem base;

  int32_t node_id;
  char _pad1[4];
} ViewerNodeViewerPathElem;

typedef struct ViewerPath {
  /** List of #ViewerPathElem. */
  ListBase path;
} ViewerPath;
