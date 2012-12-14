/* FRS_freestyle.h
 *
 * $Id: FRS_freestyle.h 43652 2012-01-23 23:32:09Z kjym3 $
 *
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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef FRS_FREESTYLE_H
#define FRS_FREESTYLE_H

#ifdef __cplusplus
extern "C" {
#endif	
	
	#include "DNA_listBase.h"
	#include "DNA_scene_types.h"
	
	#include "BKE_context.h"
	#include "BKE_object.h"
	
	struct Render;
	
	extern Scene *freestyle_scene;
	extern float freestyle_viewpoint[3];
	extern float freestyle_mv[4][4];
	extern float freestyle_proj[4][4];
	extern int freestyle_viewport[4];

	// Rendering
	void FRS_initialize(void);
	void FRS_set_context(bContext* C);
	void FRS_read_file(bContext* C);
	int FRS_is_freestyle_enabled(struct SceneRenderLayer* srl);
	void FRS_init_stroke_rendering(struct Render* re);
	struct Render* FRS_do_stroke_rendering(struct Render* re, struct SceneRenderLayer* srl);
	void FRS_finish_stroke_rendering(struct Render* re);
	void FRS_composite_result(struct Render* re, struct SceneRenderLayer* srl, struct Render* freestyle_render);
	void FRS_exit(void);
	
	// Panel configuration
	void FRS_add_module(FreestyleConfig *config);
	void FRS_delete_module(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
	void FRS_move_module_up(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
	void FRS_move_module_down(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
	
	FreestyleLineSet *FRS_add_lineset(FreestyleConfig *config);
	void FRS_copy_active_lineset(FreestyleConfig *config);
	void FRS_paste_active_lineset(FreestyleConfig *config);
	void FRS_delete_active_lineset(FreestyleConfig *config);
	void FRS_move_active_lineset_up(FreestyleConfig *config);
	void FRS_move_active_lineset_down(FreestyleConfig *config);

	FreestyleLineSet *FRS_get_active_lineset(FreestyleConfig *config);
	short FRS_get_active_lineset_index(FreestyleConfig *config);
	void FRS_set_active_lineset_index(FreestyleConfig *config, short index);

	void FRS_unlink_target_object(FreestyleConfig *config, struct Object *ob);

#ifdef __cplusplus
}
#endif

#endif
