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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ExportSettings.h
 *  \ingroup collada
 */

#ifndef __EXPORTSETTINGS_H__
#define __EXPORTSETTINGS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_linklist.h"

typedef enum BC_export_mesh_type {
	BC_MESH_TYPE_VIEW,
	BC_MESH_TYPE_RENDER
} BC_export_mesh_type;

typedef enum BC_export_transformation_type {
	BC_TRANSFORMATION_TYPE_MATRIX,
	BC_TRANSFORMATION_TYPE_TRANSROTLOC
} BC_export_transformation_type;


typedef enum BC_export_animation_type {
	BC_ANIMATION_EXPORT_SAMPLES,
	BC_ANIMATION_EXPORT_KEYS
} BC_export_animation_type;

typedef enum BC_ui_export_section {
	BC_UI_SECTION_MAIN,
	BC_UI_SECTION_GEOMETRY,
	BC_UI_SECTION_ARMATURE,
	BC_UI_SECTION_ANIMATION,
	BC_UI_SECTION_COLLADA
} BC_ui_export_section;

typedef struct ExportSettings {
	bool apply_modifiers;
	BC_export_mesh_type export_mesh_type;

	bool selected;
	bool include_children;
	bool include_armatures;
	bool include_shapekeys;
	bool deform_bones_only;
	bool include_animations;
	bool include_all_actions;
	int sampling_rate;
	bool keep_smooth_curves;
	bool keep_keyframes;
	bool keep_flat_curves;

	bool active_uv_only;
	BC_export_animation_type export_animation_type;
	bool use_texture_copies;

	bool triangulate;
	bool use_object_instantiation;
	bool use_blender_profile;
	bool sort_by_name;
	BC_export_transformation_type export_transformation_type;

	bool open_sim;
	bool limit_precision;
	bool keep_bind_info;

	char *filepath;
	LinkNode *export_set;
} ExportSettings;

#ifdef __cplusplus
}
#endif

#endif
