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

#include "collada.h"

struct ExportSettings {
public:
	bool apply_modifiers;
	BC_export_mesh_type export_mesh_type;

	bool selected;
	bool include_children;
	bool include_armatures;
	bool deform_bones_only;

	bool active_uv_only;
	bool include_uv_textures;
	bool use_texture_copies;

	bool use_object_instantiation;
	bool sort_by_name;
	bool second_life;

	char *filepath;
	LinkNode *export_set;
};

#endif
