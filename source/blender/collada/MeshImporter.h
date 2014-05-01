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
#include "COLLADAFWPolygons.h"
#include "COLLADAFWInstanceGeometry.h"
#include "COLLADAFWMaterialBinding.h"
#include "COLLADAFWMesh.h"
#include "COLLADAFWMeshVertexData.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWTextureCoordinateBinding.h"
#include "COLLADAFWTypes.h"
#include "COLLADAFWUniqueId.h"

#include "ArmatureImporter.h"
#include "collada_utils.h"

extern "C" {
#include "BLI_edgehash.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

}

// only for ArmatureImporter to "see" MeshImporter::get_object_by_geom_uid
class MeshImporterBase
{
public:
	virtual Object *get_object_by_geom_uid(const COLLADAFW::UniqueId& geom_uid) = 0;
	virtual Mesh *get_mesh_by_geom_uid(const COLLADAFW::UniqueId& mesh_uid) = 0;
	virtual std::string *get_geometry_name(const std::string &mesh_name) = 0;
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

class VCOLDataWrapper
{
	COLLADAFW::MeshVertexData *mVData;
public:
	VCOLDataWrapper(COLLADAFW::MeshVertexData& vdata);
	void get_vcol(int v_index, MLoopCol *mloopcol);
};

class MeshImporter : public MeshImporterBase
{
private:

	UnitConverter *unitconverter;

	Scene *scene;
	ArmatureImporter *armature_importer;

	std::map<std::string, std::string> mesh_geom_map; // needed for correct shape key naming
	std::map<COLLADAFW::UniqueId, Mesh*> uid_mesh_map; // geometry unique id-to-mesh map
	std::map<COLLADAFW::UniqueId, Object*> uid_object_map; // geom uid-to-object
	std::vector<Object*> imported_objects; // list of imported objects

	// this structure is used to assign material indices to polygons
	// it holds a portion of Mesh faces and corresponds to a DAE primitive list (<triangles>, <polylist>, etc.)
	struct Primitive {
		MPoly *mpoly;
		unsigned int totpoly;
	};
	typedef std::map<COLLADAFW::MaterialId, std::vector<Primitive> > MaterialIdPrimitiveArrayMap;
	std::map<COLLADAFW::UniqueId, MaterialIdPrimitiveArrayMap> geom_uid_mat_mapping_map; // crazy name!
	std::multimap<COLLADAFW::UniqueId, COLLADAFW::UniqueId> materials_mapped_to_geom; //< materials that have already been mapped to a geometry. A pair of geom uid and mat uid, one geometry can have several materials
	
	void set_poly_indices(MPoly *mpoly,
						  MLoop *mloop,
						  int loop_index,
						  unsigned int *indices,
						  int loop_count);

	void set_face_uv(MLoopUV *mloopuv,
					 UVDataWrapper &uvs,
					 int loop_index,
					 COLLADAFW::IndexList& index_list,
					 int count);

	void set_vcol(MLoopCol *mloopcol,
		          VCOLDataWrapper &vob,
		          int loop_index,
		          COLLADAFW::IndexList& index_list,
		          int count);

#ifdef COLLADA_DEBUG
	void print_index_list(COLLADAFW::IndexList& index_list);
#endif

	bool is_nice_mesh(COLLADAFW::Mesh *mesh);

	void read_vertices(COLLADAFW::Mesh *mesh, Mesh *me);
			
	bool primitive_has_useable_normals(COLLADAFW::MeshPrimitive *mp);
	bool primitive_has_faces(COLLADAFW::MeshPrimitive *mp);

	static void mesh_add_edges(Mesh *mesh, int len);

	unsigned int get_loose_edge_count(COLLADAFW::Mesh *mesh);

	CustomData create_edge_custom_data(EdgeHash *eh);

	void allocate_poly_data(COLLADAFW::Mesh *collada_mesh, Mesh *me);

	// TODO: import uv set names
	void read_polys(COLLADAFW::Mesh *mesh, Mesh *me);
	void read_lines(COLLADAFW::Mesh *mesh, Mesh *me);
	unsigned int get_vertex_count(COLLADAFW::Polygons *mp, int index);

	void get_vector(float v[3], COLLADAFW::MeshVertexData& arr, int i, int stride);

	bool is_flat_face(unsigned int *nind, COLLADAFW::MeshVertexData& nor, int count);

	std::vector<Object *> get_all_users_of(Mesh *reference_mesh);

public:

	MeshImporter(UnitConverter *unitconv, ArmatureImporter *arm, Scene *sce);

	void bmeshConversion();

	virtual Object *get_object_by_geom_uid(const COLLADAFW::UniqueId& geom_uid);

	virtual Mesh *get_mesh_by_geom_uid(const COLLADAFW::UniqueId& geom_uid);
	
	MTex *assign_textures_to_uvlayer(COLLADAFW::TextureCoordinateBinding &ctexture,
	                                 Mesh *me, TexIndexTextureArrayMap& texindex_texarray_map,
	                                 MTex *color_texture);

	void optimize_material_assignements();

	MTFace *assign_material_to_geom(COLLADAFW::MaterialBinding cmaterial,
	                                std::map<COLLADAFW::UniqueId, Material*>& uid_material_map,
	                                Object *ob, const COLLADAFW::UniqueId *geom_uid,
	                                char *layername, MTFace *texture_face,
	                                std::map<Material*, TexIndexTextureArrayMap>& material_texture_mapping_map, short mat_index);
	
	
	Object *create_mesh_object(COLLADAFW::Node *node, COLLADAFW::InstanceGeometry *geom,
	                           bool isController,
	                           std::map<COLLADAFW::UniqueId, Material*>& uid_material_map,
	                           std::map<Material*, TexIndexTextureArrayMap>& material_texture_mapping_map);

	// create a mesh storing a pointer in a map so it can be retrieved later by geometry UID
	bool write_geometry(const COLLADAFW::Geometry* geom);
	std::string *get_geometry_name(const std::string &mesh_name);
};

#endif
