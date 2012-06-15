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

/** \file GeometryExporter.h
 *  \ingroup collada
 */

#ifndef __GEOMETRYEXPORTER_H__
#define __GEOMETRYEXPORTER_H__

#include <string>
#include <vector>
#include <set>

#include "COLLADASWStreamWriter.h"
#include "COLLADASWLibraryGeometries.h"
#include "COLLADASWInputList.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ExportSettings.h"

extern Object *bc_get_highest_selected_ancestor_or_self(Object *ob);

// TODO: optimize UV sets by making indexed list with duplicates removed
class GeometryExporter : COLLADASW::LibraryGeometries
{
	struct Face
	{
		unsigned int v1, v2, v3, v4;
	};

	struct Normal
	{
		float x, y, z;
	};

	Scene *mScene;

public:
	GeometryExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings);

	void exportGeom(Scene *sce);

	void operator()(Object *ob);

	void createLooseEdgeList(Object *ob,
						     Mesh   *me,
						     std::string& geom_id,
						     std::vector<Face>& norind);

	// powerful because it handles both cases when there is material and when there's not
	void createPolylist(short material_index,
						bool has_uvs,
						bool has_color,
						Object *ob,
						Mesh   *me,
						std::string& geom_id,
						std::vector<Face>& norind);
	
	// creates <source> for positions
	void createVertsSource(std::string geom_id, Mesh *me);

	void createVertexColorSource(std::string geom_id, Mesh *me);

	std::string makeTexcoordSourceId(std::string& geom_id, int layer_index);

	//creates <source> for texcoords
	void createTexcoordsSource(std::string geom_id, Mesh *me);

	//creates <source> for normals
	void createNormalsSource(std::string geom_id, Mesh *me, std::vector<Normal>& nor);

	void create_normals(std::vector<Normal> &nor, std::vector<Face> &ind, Mesh *me);
	
	std::string getIdBySemantics(std::string geom_id, COLLADASW::InputSemantic::Semantics type, std::string other_suffix = "");
	
	COLLADASW::URI getUrlBySemantics(std::string geom_id, COLLADASW::InputSemantic::Semantics type, std::string other_suffix = "");

	COLLADASW::URI makeUrl(std::string id);
	
	/* int getTriCount(MFace *faces, int totface);*/
private:
	std::set<std::string> exportedGeometry;
	
	const ExportSettings *export_settings;

	Mesh * get_mesh(Scene *sce, Object *ob, int apply_modifiers);
};

struct GeometryFunctor {
	// f should have
	// void operator()(Object* ob)
	template<class Functor>
	void forEachMeshObjectInExportSet(Scene *sce, Functor &f, LinkNode *export_set)
	{
		LinkNode *node;
		for (node=export_set; node; node = node->next) {
			Object *ob = (Object *)node->link;
			if (ob->type == OB_MESH)
			{
				f(ob);
			}
		}
	}
};

#endif
