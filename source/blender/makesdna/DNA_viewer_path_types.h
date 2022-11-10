/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

struct ID;

typedef enum ViewerPathElemType {
  VIEWER_PATH_ELEM_TYPE_ID = 0,
  VIEWER_PATH_ELEM_TYPE_MODIFIER = 1,
  VIEWER_PATH_ELEM_TYPE_NODE = 2,
} ViewerPathElemType;

typedef struct ViewerPathElem {
  struct ViewerPathElem *next, *prev;
  int type;
  char _pad[4];
} ViewerPathElem;

typedef struct IDViewerPathElem {
  ViewerPathElem base;
  struct ID *id;
} IDViewerPathElem;

typedef struct ModifierViewerPathElem {
  ViewerPathElem base;
  char *modifier_name;
} ModifierViewerPathElem;

typedef struct NodeViewerPathElem {
  ViewerPathElem base;
  char *node_name;
} NodeViewerPathElem;

typedef struct ViewerPath {
  /** List of #ViewerPathElem. */
  ListBase path;
} ViewerPath;
