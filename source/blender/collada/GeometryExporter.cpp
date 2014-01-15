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

/** \file blender/collada/GeometryExporter.cpp
 *  \ingroup collada
 */


#include <sstream>

#include "COLLADASWPrimitves.h"
#include "COLLADASWSource.h"
#include "COLLADASWVertices.h"
#include "COLLADABUUtils.h"

#include "GeometryExporter.h"

#include "DNA_meshdata_types.h"

extern "C" {
	#include "BLI_utildefines.h"

	#include "BKE_DerivedMesh.h"
	#include "BKE_main.h"
	#include "BKE_global.h"
	#include "BKE_library.h"
	#include "BKE_customdata.h"
	#include "BKE_material.h"
	#include "BKE_mesh.h"
}

#include "collada_internal.h"
#include "collada_utils.h"

// TODO: optimize UV sets by making indexed list with duplicates removed
GeometryExporter::GeometryExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings) : COLLADASW::LibraryGeometries(sw), export_settings(export_settings)
{
}

void GeometryExporter::exportGeom(Scene *sce)
{
	openLibrary();

	mScene = sce;
	GeometryFunctor gf;
	gf.forEachMeshObjectInExportSet<GeometryExporter>(sce, *this, this->export_settings->export_set);

	closeLibrary();
}

void GeometryExporter::operator()(Object *ob)
{ 
	// XXX don't use DerivedMesh, Mesh instead?
#if 0		
	DerivedMesh *dm = mesh_get_derived_final(mScene, ob, CD_MASK_BAREMESH);
#endif

	bool use_instantiation = this->export_settings->use_object_instantiation;
	Mesh *me = bc_get_mesh_copy( mScene, 
					ob,
					this->export_settings->export_mesh_type,
					this->export_settings->apply_modifiers,
					this->export_settings->triangulate);

	std::string geom_id = get_geometry_id(ob, use_instantiation);
	std::vector<Normal> nor;
	std::vector<BCPolygonNormalsIndices> norind;

	// Skip if linked geometry was already exported from another reference
	if (use_instantiation && 
	    exportedGeometry.find(geom_id) != exportedGeometry.end())
	{
		return;
	}

	std::string geom_name = (use_instantiation) ? id_name(ob->data) : id_name(ob);

	exportedGeometry.insert(geom_id);

	bool has_color = (bool)CustomData_has_layer(&me->fdata, CD_MCOL);

	create_normals(nor, norind, me);

	// openMesh(geoId, geoName, meshId)
	openMesh(geom_id, geom_name);
	
	// writes <source> for vertex coords
	createVertsSource(geom_id, me);
	
	// writes <source> for normal coords
	createNormalsSource(geom_id, me, nor);

	bool has_uvs = (bool)CustomData_has_layer(&me->fdata, CD_MTFACE);
	
	// writes <source> for uv coords if mesh has uv coords
	if (has_uvs) {
		createTexcoordsSource(geom_id, me);
	}

	if (has_color) {
		createVertexColorSource(geom_id, me);
	}
	// <vertices>

	COLLADASW::Vertices verts(mSW);
	verts.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::VERTEX));
	COLLADASW::InputList &input_list = verts.getInputList();
	COLLADASW::Input input(COLLADASW::InputSemantic::POSITION, getUrlBySemantics(geom_id, COLLADASW::InputSemantic::POSITION));
	input_list.push_back(input);
	verts.add();

	createLooseEdgeList(ob, me, geom_id);

	// Only create Polylists if number of faces > 0
	if (me->totface > 0) {
		// XXX slow
		if (ob->totcol) {
			for (int a = 0; a < ob->totcol; a++) {
				createPolylist(a, has_uvs, has_color, ob, me, geom_id, norind);
			}
		}
		else {
			createPolylist(0, has_uvs, has_color, ob, me, geom_id, norind);
		}
	}
	
	closeMesh();
	
	if (me->flag & ME_TWOSIDED) {
		mSW->appendTextBlock("<extra><technique profile=\"MAYA\"><double_sided>1</double_sided></technique></extra>");
	}

	closeGeometry();

	if (this->export_settings->include_shapekeys) {
		Key * key = BKE_key_from_object(ob);
		if (key) {
			KeyBlock * kb = (KeyBlock *)key->block.first;
			//skip the basis
			kb = kb->next;
			for (; kb; kb = kb->next) {
				BKE_key_convert_to_mesh(kb, me);
				export_key_mesh(ob, me, kb);
			}
		}
	}

	BKE_libblock_free_us(G.main, me);

}

void GeometryExporter::export_key_mesh(Object *ob, Mesh *me, KeyBlock *kb)
{
	std::string geom_id = get_geometry_id(ob, false) + "_morph_" + translate_id(kb->name);
	std::vector<Normal> nor;
	std::vector<BCPolygonNormalsIndices> norind;
	
	if (exportedGeometry.find(geom_id) != exportedGeometry.end())
	{
		return;
	}

	std::string geom_name = kb->name;

	exportedGeometry.insert(geom_id);

	bool has_color = (bool)CustomData_has_layer(&me->fdata, CD_MCOL);

	create_normals(nor, norind, me);

	// openMesh(geoId, geoName, meshId)
	openMesh(geom_id, geom_name);
	
	// writes <source> for vertex coords
	createVertsSource(geom_id, me);
	
	// writes <source> for normal coords
	createNormalsSource(geom_id, me, nor);

	bool has_uvs = (bool)CustomData_has_layer(&me->fdata, CD_MTFACE);
	
	// writes <source> for uv coords if mesh has uv coords
	if (has_uvs) {
		createTexcoordsSource(geom_id, me);
	}

	if (has_color) {
		createVertexColorSource(geom_id, me);
	}

	// <vertices>

	COLLADASW::Vertices verts(mSW);
	verts.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::VERTEX));
	COLLADASW::InputList &input_list = verts.getInputList();
	COLLADASW::Input input(COLLADASW::InputSemantic::POSITION, getUrlBySemantics(geom_id, COLLADASW::InputSemantic::POSITION));
	input_list.push_back(input);
	verts.add();

	//createLooseEdgeList(ob, me, geom_id, norind);

	// XXX slow		
	if (ob->totcol) {
		for (int a = 0; a < ob->totcol; a++) {
			createPolylist(a, has_uvs, has_color, ob, me, geom_id, norind);
		}
	}
	else {
		createPolylist(0, has_uvs, has_color, ob, me, geom_id, norind);
	}
	
	closeMesh();
	
	if (me->flag & ME_TWOSIDED) {
		mSW->appendTextBlock("<extra><technique profile=\"MAYA\"><double_sided>1</double_sided></technique></extra>");
	}

	closeGeometry();
}

void GeometryExporter::createLooseEdgeList(Object *ob,
                                           Mesh   *me,
                                           std::string& geom_id)
{

	MEdge *medges = me->medge;
	int totedges  = me->totedge;
	int edges_in_linelist = 0;
	std::vector<unsigned int> edge_list;
	int index;

	// Find all loose edges in Mesh 
	// and save vertex indices in edge_list
	for (index = 0; index < totedges; index++) 
	{
		MEdge *edge = &medges[index];

		if (edge->flag & ME_LOOSEEDGE)
		{
			edges_in_linelist += 1;
			edge_list.push_back(edge->v1);
			edge_list.push_back(edge->v2);
		}
	}

	if (edges_in_linelist > 0)
	{
		// Create the list of loose edges
		COLLADASW::Lines lines(mSW);

		lines.setCount(edges_in_linelist);


		COLLADASW::InputList &til = lines.getInputList();
			
		// creates <input> in <lines> for vertices 
		COLLADASW::Input input1(COLLADASW::InputSemantic::VERTEX, getUrlBySemantics(geom_id, COLLADASW::InputSemantic::VERTEX), 0);
		til.push_back(input1);

		lines.prepareToAppendValues();

		for (index = 0; index < edges_in_linelist; index++) 
		{
			lines.appendValues(edge_list[2 * index + 1]);
			lines.appendValues(edge_list[2 * index]);
		}
		lines.finish();
	}

}

// powerful because it handles both cases when there is material and when there's not
void GeometryExporter::createPolylist(short material_index,
                                      bool has_uvs,
                                      bool has_color,
                                      Object *ob,
                                      Mesh *me,
                                      std::string& geom_id,
                                      std::vector<BCPolygonNormalsIndices>& norind)
{

	MPoly *mpolys = me->mpoly;
	MLoop *mloops = me->mloop;
	int totpolys  = me->totpoly;

	// <vcount>
	int i;
	int faces_in_polylist = 0;
	std::vector<unsigned long> vcount_list;

	// count faces with this material
	for (i = 0; i < totpolys; i++) {
		MPoly *p = &mpolys[i];
		
		if (p->mat_nr == material_index) {
			faces_in_polylist++;
			vcount_list.push_back(p->totloop);
		}
	}

	// no faces using this material
	if (faces_in_polylist == 0) {
		fprintf(stderr, "%s: material with index %d is not used.\n", id_name(ob).c_str(), material_index);
		return;
	}
		
	Material *ma = ob->totcol ? give_current_material(ob, material_index + 1) : NULL;
	COLLADASW::Polylist polylist(mSW);
		
	// sets count attribute in <polylist>
	polylist.setCount(faces_in_polylist);
		
	// sets material name
	if (ma) {
		std::string material_id = get_material_id(ma);
		std::ostringstream ostr;
		ostr << translate_id(material_id);
		polylist.setMaterial(ostr.str());
	}
			
	COLLADASW::InputList &til = polylist.getInputList();
		
	// creates <input> in <polylist> for vertices 
	COLLADASW::Input input1(COLLADASW::InputSemantic::VERTEX, getUrlBySemantics(geom_id, COLLADASW::InputSemantic::VERTEX), 0);
		
	// creates <input> in <polylist> for normals
	COLLADASW::Input input2(COLLADASW::InputSemantic::NORMAL, getUrlBySemantics(geom_id, COLLADASW::InputSemantic::NORMAL), 1);
		
	til.push_back(input1);
	til.push_back(input2);
		
	// if mesh has uv coords writes <input> for TEXCOORD
	int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
	int active_uv_index = CustomData_get_active_layer_index(&me->fdata, CD_MTFACE)-1;
	for (i = 0; i < num_layers; i++) {
		if (!this->export_settings->active_uv_only || i == active_uv_index) {

			// char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, i);
			COLLADASW::Input input3(COLLADASW::InputSemantic::TEXCOORD,
									makeUrl(makeTexcoordSourceId(geom_id, i, this->export_settings->active_uv_only)),
									2, // this is only until we have optimized UV sets
									(this->export_settings->active_uv_only) ? 0 : i  // only_active_uv exported -> we have only one set
									);
			til.push_back(input3);
		}
	}

	if (has_color) {
		COLLADASW::Input input4(COLLADASW::InputSemantic::COLOR, getUrlBySemantics(geom_id, COLLADASW::InputSemantic::COLOR), has_uvs ? 3 : 2);
		til.push_back(input4);
	}
		
	// sets <vcount>
	polylist.setVCountList(vcount_list);
		
	// performs the actual writing
	polylist.prepareToAppendValues();
	
	// <p>
	int texindex = 0;
	for (i = 0; i < totpolys; i++) {
		MPoly *p = &mpolys[i];
		int loop_count = p->totloop;

		if (p->mat_nr == material_index) {
			MLoop *l = &mloops[p->loopstart];
			BCPolygonNormalsIndices normal_indices = norind[i];

			for (int j = 0; j < loop_count; j++) {
				polylist.appendValues(l[j].v);
				polylist.appendValues(normal_indices[j]);
				if (has_uvs)
					polylist.appendValues(texindex + j);

				if (has_color)
					polylist.appendValues(texindex + j);
			}
		}

		texindex += loop_count;
	}
		
	polylist.finish();
}


// creates <source> for positions
void GeometryExporter::createVertsSource(std::string geom_id, Mesh *me)
{
#if 0
	int totverts = dm->getNumVerts(dm);
	MVert *verts = dm->getVertArray(dm);
#endif
	int totverts = me->totvert;
	MVert *verts = me->mvert;
	
	COLLADASW::FloatSourceF source(mSW);
	source.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::POSITION));
	source.setArrayId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::POSITION) +
	                  ARRAY_ID_SUFFIX);
	source.setAccessorCount(totverts);
	source.setAccessorStride(3);
	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("X");
	param.push_back("Y");
	param.push_back("Z");
	/* main function, it creates <source id = "">, <float_array id = ""
	 * count = ""> */
	source.prepareToAppendValues();
	//appends data to <float_array>
	int i = 0;
	for (i = 0; i < totverts; i++) {
		source.appendValues(verts[i].co[0], verts[i].co[1], verts[i].co[2]);
	}
	
	source.finish();

}

void GeometryExporter::createVertexColorSource(std::string geom_id, Mesh *me)
{
	MLoopCol *mloopcol = (MLoopCol *)CustomData_get_layer(&me->ldata, CD_MLOOPCOL);
	if (mloopcol == NULL)
		return;


	COLLADASW::FloatSourceF source(mSW);
	source.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::COLOR));
	source.setArrayId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::COLOR) + ARRAY_ID_SUFFIX);
	source.setAccessorCount(me->totloop);
	source.setAccessorStride(3);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("R");
	param.push_back("G");
	param.push_back("B");

	source.prepareToAppendValues();

	MPoly *mpoly;
	int i;
	for (i = 0, mpoly = me->mpoly; i < me->totpoly; i++, mpoly++) {
		MLoopCol *mlc = mloopcol + mpoly->loopstart;
		for (int j = 0; j < mpoly->totloop; j++, mlc++) {
			source.appendValues(
					mlc->r / 255.0f,
					mlc->g / 255.0f,
					mlc->b / 255.0f
			);
		}
	}
	
	source.finish();
}


std::string GeometryExporter::makeTexcoordSourceId(std::string& geom_id, int layer_index, bool is_single_layer)
{
	char suffix[20];
	if (is_single_layer) {
		suffix[0] = '\0';
	}
	else {
		sprintf(suffix, "-%d", layer_index);
	}
	return getIdBySemantics(geom_id, COLLADASW::InputSemantic::TEXCOORD) + suffix;
}

//creates <source> for texcoords
void GeometryExporter::createTexcoordsSource(std::string geom_id, Mesh *me)
{

	int totpoly   = me->totpoly;
	int totuv     = me->totloop;
	MPoly *mpolys = me->mpoly;

	int num_layers = CustomData_number_of_layers(&me->ldata, CD_MLOOPUV);

	// write <source> for each layer
	// each <source> will get id like meshName + "map-channel-1"
	int active_uv_index = CustomData_get_active_layer_index(&me->ldata, CD_MLOOPUV);
	for (int a = 0; a < num_layers; a++) {

		if (!this->export_settings->active_uv_only || a == active_uv_index) {
			MLoopUV *mloops = (MLoopUV *)CustomData_get_layer_n(&me->ldata, CD_MLOOPUV, a);
			
			COLLADASW::FloatSourceF source(mSW);
			std::string layer_id = makeTexcoordSourceId(geom_id, a, this->export_settings->active_uv_only);
			source.setId(layer_id);
			source.setArrayId(layer_id + ARRAY_ID_SUFFIX);
			
			source.setAccessorCount(totuv);
			source.setAccessorStride(2);
			COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
			param.push_back("S");
			param.push_back("T");
			
			source.prepareToAppendValues();
			
			for (int index = 0; index < totpoly; index++) {
				MPoly   *mpoly = mpolys+index;
				MLoopUV *mloop = mloops+mpoly->loopstart;
				for (int j = 0; j < mpoly->totloop; j++) {
					source.appendValues(mloop[j].uv[0],
										mloop[j].uv[1]);
				}
			}
			
			source.finish();
		}
	}
}


//creates <source> for normals
void GeometryExporter::createNormalsSource(std::string geom_id, Mesh *me, std::vector<Normal>& nor)
{
#if 0
	int totverts = dm->getNumVerts(dm);
	MVert *verts = dm->getVertArray(dm);
#endif

	COLLADASW::FloatSourceF source(mSW);
	source.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::NORMAL));
	source.setArrayId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::NORMAL) +
	                  ARRAY_ID_SUFFIX);
	source.setAccessorCount((unsigned long)nor.size());
	source.setAccessorStride(3);
	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("X");
	param.push_back("Y");
	param.push_back("Z");
	
	source.prepareToAppendValues();

	std::vector<Normal>::iterator it;
	for (it = nor.begin(); it != nor.end(); it++) {
		Normal& n = *it;
		source.appendValues(n.x, n.y, n.z);
	}

	source.finish();
}

void GeometryExporter::create_normals(std::vector<Normal> &normals, std::vector<BCPolygonNormalsIndices> &polygons_normals, Mesh *me)
{
	std::map<unsigned int, unsigned int> shared_normal_indices;
	int last_normal_index = -1;

	MVert *verts  = me->mvert;
	MLoop *mloops = me->mloop;
	for (int poly_index = 0; poly_index < me->totpoly; poly_index++) {
		MPoly *mpoly  = &me->mpoly[poly_index];

		if (!(mpoly->flag & ME_SMOOTH)) {
			// For flat faces use face normal as vertex normal:

			float vector[3];
			BKE_mesh_calc_poly_normal(mpoly, mloops+mpoly->loopstart, verts, vector);

			Normal n = { vector[0], vector[1], vector[2] };
			normals.push_back(n);
			last_normal_index++;
		}


		MLoop *mloop = mloops + mpoly->loopstart;
		BCPolygonNormalsIndices poly_indices;
		for (int loop_index = 0; loop_index < mpoly->totloop; loop_index++) {
			unsigned int vertex_index = mloop[loop_index].v;
			if (mpoly->flag & ME_SMOOTH) {
				if (shared_normal_indices.find(vertex_index) != shared_normal_indices.end())
					poly_indices.add_index (shared_normal_indices[vertex_index]);
				else {

					float vector[3];
					normal_short_to_float_v3(vector, verts[vertex_index].no);

					Normal n = { vector[0], vector[1], vector[2] };
					normals.push_back(n);
					last_normal_index++;

					poly_indices.add_index(last_normal_index);
					shared_normal_indices[vertex_index] = last_normal_index;
				}
			}
			else {
				poly_indices.add_index(last_normal_index);
			}
		}

		polygons_normals.push_back(poly_indices);
	}
}

std::string GeometryExporter::getIdBySemantics(std::string geom_id, COLLADASW::InputSemantic::Semantics type, std::string other_suffix)
{
	return geom_id + getSuffixBySemantic(type) + other_suffix;
}


COLLADASW::URI GeometryExporter::getUrlBySemantics(std::string geom_id, COLLADASW::InputSemantic::Semantics type, std::string other_suffix)
{
	
	std::string id(getIdBySemantics(geom_id, type, other_suffix));
	return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
	
}

COLLADASW::URI GeometryExporter::makeUrl(std::string id)
{
	return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
}


