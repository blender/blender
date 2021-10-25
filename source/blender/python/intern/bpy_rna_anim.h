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

#ifndef __BPY_RNA_ANIM_H__
#define __BPY_RNA_ANIM_H__

/** \file blender/python/intern/bpy_rna_anim.h
 *  \ingroup pythonintern
 */

extern char pyrna_struct_keyframe_insert_doc[];
extern char pyrna_struct_keyframe_delete_doc[];
extern char pyrna_struct_driver_add_doc[];
extern char pyrna_struct_driver_remove_doc[];

PyObject *pyrna_struct_keyframe_insert(BPy_StructRNA *self, PyObject *args, PyObject *kw);
PyObject *pyrna_struct_keyframe_delete(BPy_StructRNA *self, PyObject *args, PyObject *kw);
PyObject *pyrna_struct_driver_add(BPy_StructRNA *self, PyObject *args);
PyObject *pyrna_struct_driver_remove(BPy_StructRNA *self, PyObject *args);

#endif  /* __BPY_RNA_ANIM_H__ */
