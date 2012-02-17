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

/** \file MeshImporter.h
 *  \ingroup collada
 */

#ifndef __MESHIMPORTER_H__
#define __MESHIMPORTER_H__

#include <map>
#include <vector>

#include "COLLADAFWIndexList.h"
#include "COLLADAFWInstanceGeometry.h"
#include "COLLADAFWMaterialBinding.h"
#include "COLLADAFWMesh.h"
#include "COLLADAFWMeshVertexData.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWTextureCoordinateBinding.h"
#include "COLLADAFWTypes.h"
#include "COLLADAFWUniqueId.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "ArmatureImporter.h"
#include "collada_utils.h"

// only for ArmatureImporter to "see" MeshImporter::get_object_by_geom_uid
class MeshImporterBase
{
public:
	virtual Object *get_object_by_geom_uid(const COLLADAFW::UniqueId& geom_uid) = 0;
};

class UVDataWrapper
{
	COLLADAFW::MeshVertexData *mVData;
public:
	UVDataWrapper(COLLADAFW::MeshVertexData& vdata);

#ifdef COLLADA_DEBUG
	void print();
#endif

	void getUV(int uv_index, float *uv);
};

class MeshImporter : public MeshImporterBase
{
private:

	UnitConverter *unitconverter;

	Scene *scene;
	ArmatureImporter *armature_importer;

	std::map<COLLADAFW::UniqueId, Mesh*> uid_mesh_map; // geometry unique id-to-mesh map
	std::map<COLLADAFW::UniqueId, Object*> uid_object_map; // geom uid-to-object
	// this structure is used to assign material indices to faces
	// it holds a portion of Mesh faces and corresponds to a DAE primitive list (<triangles>, <polylist>, etc.)
	struct Primitive {
		MFace *mface;
		unsigned int totface;
	};
	typedef std::map<COLLADAFW::MaterialId, std::vector<Primitive> > MaterialIdPrimitiveArrayMap;
	std::map<COLLADAFW::UniqueId, MaterialIdPrimitiveArrayMap> geom_uid_mat_mapping_map; // crazy name!
	std::multimap<COLLADAFW::UniqueId, COLLADAFW::UniqueId> materials_mapped_to_geom; //< materials that have already been mapped to a geometry. A pair of geom uid and mat uid, one geometry can have several materials
	

	void set_face_indices(MFace *mface, unsigned int *indices, bool quad);

	// not used anymore, test_index_face from blenkernel is better
#if 0
	// change face indices order so that v4 is not 0
	void rotate_face_indices(MFace *mface);
#endif
	
	void set_face_uv(MTFace *mtface, UVDataWrapper &uvs,
					 COLLADAFW::IndexList& index_list, unsigned int *tris_indices);

	void set_face_uv(MTFace *mtface, UVDataWrapper &uvs,
					COLLADAFW::IndexList& index_list, int index, bool quad);

#ifdef COLLADA_DEBUG
	void print_index_list(COLLADAFW::IndexList& index_list);
#endif

	bool is_nice_mesh(COLLADAFW::Mesh *mesh);

	void read_vertices(COLLADAFW::Mesh *mesh, Mesh *me);
	
	int triangulate_poly(unsigned int *indices, int totvert, MVert *verts, std::vector<unsigned int>& tri);
	
	int count_new_tris(COLLADAFW::Mesh *mesh, Mesh *me);
	
	// TODO: import uv set names
	void read_faces(COLLADAFW::Mesh *mesh, Mesh *me, int new_tris);

	void get_vector(float v[3], COLLADAFW::MeshVertexData& arr, int i, int stride);

	bool flat_face(unsigned int *nind, COLLADAFW::MeshVertexData& nor, int count);
	
public:

	MeshImporter(UnitConverter *unitconv, ArmatureImporter *arm, Scene *sce);

	virtual Object *get_object_by_geom_uid(const COLLADAFW::UniqueId& geom_uid);
	
	MTex *assign_textures_to_uvlayer(COLLADAFW::TextureCoordinateBinding &ctexture,
									 Mesh *me, TexIndexTextureArrayMap& texindex_texarray_map,
									 MTex *color_texture);
	
	MTFace *assign_material_to_geom(COLLADAFW::MaterialBinding cmaterial,
									std::map<COLLADAFW::UniqueId, Material*>& uid_material_map,
									Object *ob, const COLLADAFW::UniqueId *geom_uid, 
									MTex **color_texture, char *layername, MTFace *texture_face,
									std::map<Material*, TexIndexTextureArrayMap>& material_texture_mapping_map, short mat_index);
	
	
	Object *create_mesh_object(COLLADAFW::Node *node, COLLADAFW::InstanceGeometry *geom,
							   bool isController,
							   std::map<COLLADAFW::UniqueId, Material*>& uid_material_map,
							   std::map<Material*, TexIndexTextureArrayMap>& material_texture_mapping_map);

	// create a mesh storing a pointer in a map so it can be retrieved later by geometry UID
	bool write_geometry(const COLLADAFW::Geometry* geom);

};

#endif
