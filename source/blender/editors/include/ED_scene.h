/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_scene.h
 *  \ingroup editors
 */

#ifndef __ED_SCENE_H__
#define __ED_SCENE_H__

#include "BLI_compiler_attrs.h"

enum eSceneCopyMethod;

struct Scene *ED_scene_add(struct Main *bmain, struct bContext *C, struct wmWindow *win, enum eSceneCopyMethod method) ATTR_NONNULL();
bool ED_scene_delete(struct bContext *C, struct Main *bmain, struct wmWindow *win, struct Scene *scene) ATTR_NONNULL();
void ED_scene_change_update(struct Main *bmain, struct Scene *scene, struct ViewLayer *layer) ATTR_NONNULL();
bool ED_scene_view_layer_delete(
        struct Main *bmain, struct Scene *scene, struct ViewLayer *layer,
        struct ReportList *reports) ATTR_NONNULL(1, 2, 3);

void ED_operatortypes_scene(void);

#endif /* __ED_SCENE_H__ */
