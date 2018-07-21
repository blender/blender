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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BPY_INTERN_STRING_H__
#define __BPY_INTERN_STRING_H__

/** \file blender/python/intern/bpy_intern_string.h
 *  \ingroup pythonintern
 */

void bpy_intern_string_init(void);
void bpy_intern_string_exit(void);

extern PyObject *bpy_intern_str___annotations__;
extern PyObject *bpy_intern_str___doc__;
extern PyObject *bpy_intern_str___main__;
extern PyObject *bpy_intern_str___module__;
extern PyObject *bpy_intern_str___name__;
extern PyObject *bpy_intern_str___slots__;
extern PyObject *bpy_intern_str_attr;
extern PyObject *bpy_intern_str_bl_property;
extern PyObject *bpy_intern_str_bl_rna;
extern PyObject *bpy_intern_str_bl_target_properties;
extern PyObject *bpy_intern_str_bpy_types;
extern PyObject *bpy_intern_str_frame;
extern PyObject *bpy_intern_str_properties;
extern PyObject *bpy_intern_str_register;
extern PyObject *bpy_intern_str_self;
extern PyObject *bpy_intern_str_unregister;

#endif  /* __BPY_INTERN_STRING_H__ */
