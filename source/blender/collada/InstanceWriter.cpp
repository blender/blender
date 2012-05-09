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

/** \file blender/collada/InstanceWriter.cpp
 *  \ingroup collada
 */


#include <string>
#include <sstream>

#include "COLLADASWInstanceMaterial.h"

#include "BKE_customdata.h"
#include "BKE_material.h"

#include "DNA_mesh_types.h"

#include "InstanceWriter.h"

#include "collada_internal.h"
#include "collada_utils.h"

void InstanceWriter::add_material_bindings(COLLADASW::BindMaterial& bind_material, Object *ob)
{
	for (int a = 0; a < ob->totcol; a++)	{
		Material *ma = give_current_material(ob, a+1);
			
		COLLADASW::InstanceMaterialList& iml = bind_material.getInstanceMaterialList();

		if (ma) {
			std::string matid(get_material_id(ma));
			matid = translate_id(matid);
			std::ostringstream ostr;
			ostr << translate_id(id_name(ma));
			COLLADASW::InstanceMaterial im(ostr.str(), COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, matid));
			
			// create <bind_vertex_input> for each uv map
			Mesh *me = (Mesh*)ob->data;
			int totlayer = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
			
			for (int b = 0; b < totlayer; b++) {
				char *name = bc_CustomData_get_layer_name(&me->fdata, CD_MTFACE, b);
				im.push_back(COLLADASW::BindVertexInput(name, "TEXCOORD", b));
			}
			
			iml.push_back(im);
		}
	}
}
