/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include "BKE_context.h"
#include "BKE_main.h"
#include "BLI_linklist.h"
#include "BlenderTypes.h"
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"

#ifdef __cplusplus
extern "C" {
#endif

static const BC_global_forward_axis BC_DEFAULT_FORWARD = BC_GLOBAL_FORWARD_Y;
static const BC_global_up_axis BC_DEFAULT_UP = BC_GLOBAL_UP_Z;

bool bc_is_in_Export_set(LinkNode *export_set,
                         Object *ob,
                         const Scene *scene,
                         ViewLayer *view_layer);
bool bc_is_base_node(LinkNode *export_set, Object *ob, const Scene *scene, ViewLayer *view_layer);
/**
 * Returns the highest selected ancestor
 * returns NULL if no ancestor is selected
 * IMPORTANT: This function expects that all exported objects have set:
 * `ob->id.tag & LIB_TAG_DOIT`
 */
Object *bc_get_highest_exported_ancestor_or_self(LinkNode *export_set,
                                                 Object *ob,
                                                 const Scene *scene,
                                                 ViewLayer *view_layer);
int bc_is_marked(Object *ob);
void bc_remove_mark(Object *ob);
void bc_set_mark(Object *ob);

#ifdef __cplusplus
}

class BlenderContext {
 private:
  bContext *context;
  Depsgraph *depsgraph;
  Scene *scene;
  ViewLayer *view_layer;
  Main *main;

 public:
  BlenderContext(bContext *C);
  bContext *get_context();
  Depsgraph *get_depsgraph();
  Scene *get_scene();
  Scene *get_evaluated_scene();
  Object *get_evaluated_object(Object *ob);
  ViewLayer *get_view_layer();
  Main *get_main();
};
#endif
