/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup collada
 */

#ifndef __BLENDERCONTEXT_H__
#define __BLENDERCONTEXT_H__

#ifdef __cplusplus

extern "C" {
#endif

#include "DNA_object_types.h"
#include "BLI_linklist.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
#include "DNA_layer_types.h"
#include "BlenderTypes.h"

static const BC_global_forward_axis BC_DEFAULT_FORWARD = BC_GLOBAL_FORWARD_Y;
static const BC_global_up_axis BC_DEFAULT_UP = BC_GLOBAL_UP_Z;

bool bc_is_in_Export_set(LinkNode *export_set, Object *ob, ViewLayer *view_layer);
bool bc_is_base_node(LinkNode *export_set, Object *ob, ViewLayer *view_layer);
Object *bc_get_highest_exported_ancestor_or_self(LinkNode *export_set,
                                                 Object *ob,
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

#endif
