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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed,
 *                 Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file TransformWriter.h
 *  \ingroup collada
 */

#ifndef __TRANSFORMWRITER_H__
#define __TRANSFORMWRITER_H__

#include "COLLADASWNode.h"

#include "DNA_object_types.h"

#include "collada_internal.h"
#include "collada.h"

class TransformWriter : protected TransformBase
{
protected:
	void add_node_transform(COLLADASW::Node& node, float mat[4][4], float parent_mat[4][4]);

	void add_node_transform_ob(COLLADASW::Node& node, Object *ob, BC_export_transformation_type transformation_type);

	void add_node_transform_identity(COLLADASW::Node& node);

private:
	void add_transform(COLLADASW::Node& node, float loc[3], float rot[3], float scale[3]);
};

#endif
