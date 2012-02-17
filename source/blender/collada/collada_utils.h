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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file collada_utils.h
 *  \ingroup collada
 */

#ifndef __COLLADA_UTILS_H__
#define __COLLADA_UTILS_H__

#include "COLLADAFWMeshPrimitive.h"
#include "COLLADAFWGeometry.h"
#include "COLLADAFWFloatOrDoubleArray.h"
#include "COLLADAFWTypes.h"

#include <vector>
#include <map>

#include "DNA_object_types.h"
#include "DNA_customdata_types.h"
#include "DNA_texture_types.h"
#include "BKE_context.h"

typedef std::map<COLLADAFW::TextureMapId, std::vector<MTex*> > TexIndexTextureArrayMap;

extern float bc_get_float_value(const COLLADAFW::FloatOrDoubleArray& array, unsigned int index);

extern int bc_test_parent_loop(Object *par, Object *ob);
extern int bc_set_parent(Object *ob, Object *par, bContext *C, bool is_parent_space=true);
extern char *bc_CustomData_get_layer_name(const CustomData *data, int type, int n);
extern char *bc_CustomData_get_active_layer_name(const CustomData *data, int type);

#endif
