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
#include "BKE_customdata.h"
#include "BKE_material.h"

#include "collada_internal.h"

// TODO: optimize UV sets by making indexed list with duplicates removed
GeometryExporter::GeometryExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings) : COLLADASW::LibraryGeometries(sw), export_settings(export_settings) {}


void GeometryExporter::exportGeom(Scene *sce)
{
	openLibrary();

	mScene = sce;
	GeometryFunctor gf;
	gf.forEachMeshObjectInScene<GeometryExporter>(sce, *this, this->export_settings->selected);

	closeLibrary();
}

void GeometryExporter::operator()(Object *ob)
{
	// XXX don't use DerivedMesh, Mesh instead?

#if 0		
	DerivedMesh *dm = mesh_get_derived_final(mScene, ob, CD_MASK_BAREMESH);
#endif
	Mesh *me = (Mesh*)ob->data;
	std::string geom_id = get_geometry_id(ob);
	std::string geom_name = id_name(ob->data);
	std::vector<Normal> nor;
	std::vector<Face> norind;

	// Skip if linked geometry was already exported from another reference
	if (exportedGeometry.find(geom_id) != exportedGeometry.end())
		return;
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
	if (has_uvs)
		createTexcoordsSource(geom_id, me);

	if (has_color)
		createVertexColorSource(geom_id, me);

	// <vertices>
	COLLADASW::Vertices verts(mSW);
	verts.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::VERTEX));
	COLLADASW::InputList &input_list = verts.getInputList();
	COLLADASW::Input input(COLLADASW::InputSemantic::POSITION, getUrlBySemantics(geom_id, COLLADASW::InputSemantic::POSITION));
	input_list.push_back(input);
	verts.add();

	// XXX slow		
	if (ob->totcol) {
		for(int a = 0; a < ob->totcol; a++)	{
			createPolylist(a, has_uvs, has_color, ob, geom_id, norind);
		}
	}
	else {
		createPolylist(0, has_uvs, has_color, ob, geom_id, norind);
	}
	
	closeMesh();
	
	if (me->flag & ME_TWOSIDED) {
		mSW->appendTextBlock("<extra><technique profile=\"MAYA\"><double_sided>1</double_sided></technique></extra>");
	}
	
	closeGeometry();
	
#if 0
	dm->release(dm);
#endif
}

// powerful because it handles both cases when there is material and when there's not
void GeometryExporter::createPolylist(short material_index,
					bool has_uvs,
					bool has_color,
					Object *ob,
					std::string& geom_id,
					std::vector<Face>& norind)
{
	Mesh *me = (Mesh*)ob->data;
	MFace *mfaces = me->mface;
	int totfaces = me->totface;

	// <vcount>
	int i;
	int faces_in_polylist = 0;
	std::vector<unsigned long> vcount_list;

	// count faces with this material
	for (i = 0; i < totfaces; i++) {
		MFace *f = &mfaces[i];
		
		if (f->mat_nr == material_index) {
			faces_in_polylist++;
			if (f->v4 == 0) {
				vcount_list.push_back(3);
			}
			else {
				vcount_list.push_back(4);
			}
		}
	}

	// no faces using this material
	if (faces_in_polylist == 0) {
		fprintf(stderr, "%s: no faces use material %d\n", id_name(ob).c_str(), material_index);
		return;
	}
		
	Material *ma = ob->totcol ? give_current_material(ob, material_index + 1) : NULL;
	COLLADASW::Polylist polylist(mSW);
		
	// sets count attribute in <polylist>
	polylist.setCount(faces_in_polylist);
		
	// sets material name
	if (ma) {
		std::ostringstream ostr;
		ostr << translate_id(id_name(ma)) << material_index+1;
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

	for (i = 0; i < num_layers; i++) {
		// char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, i);
		COLLADASW::Input input3(COLLADASW::InputSemantic::TEXCOORD,
								makeUrl(makeTexcoordSourceId(geom_id, i)),
								2, // offset always 2, this is only until we have optimized UV sets
								i  // set number equals UV map index
								);
		til.push_back(input3);
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
	for (i = 0; i < totfaces; i++) {
		MFace *f = &mfaces[i];

		if (f->mat_nr == material_index) {

			unsigned int *v = &f->v1;
			unsigned int *n = &norind[i].v1;
			for (int j = 0; j < (f->v4 == 0 ? 3 : 4); j++) {
				polylist.appendValues(v[j]);
				polylist.appendValues(n[j]);

				if (has_uvs)
					polylist.appendValues(texindex + j);

				if (has_color)
					polylist.appendValues(texindex + j);
			}
		}

		texindex += 3;
		if (f->v4 != 0)
			texindex++;
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
	/*main function, it creates <source id = "">, <float_array id = ""
	  count = ""> */
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
	if (!CustomData_has_layer(&me->fdata, CD_MCOL))
		return;

	MFace *f;
	int totcolor = 0, i, j;

	for (i = 0, f = me->mface; i < me->totface; i++, f++)
		totcolor += f->v4 ? 4 : 3;

	COLLADASW::FloatSourceF source(mSW);
	source.setId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::COLOR));
	source.setArrayId(getIdBySemantics(geom_id, COLLADASW::InputSemantic::COLOR) + ARRAY_ID_SUFFIX);
	source.setAccessorCount(totcolor);
	source.setAccessorStride(3);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("R");
	param.push_back("G");
	param.push_back("B");

	source.prepareToAppendValues();

	int index = CustomData_get_active_layer_index(&me->fdata, CD_MCOL);

	MCol *mcol = (MCol*)me->fdata.layers[index].data;
	MCol *c = mcol;

	for (i = 0, f = me->mface; i < me->totface; i++, c += 4, f++)
		for (j = 0; j < (f->v4 ? 4 : 3); j++)
			source.appendValues(c[j].b / 255.0f, c[j].g / 255.0f, c[j].r / 255.0f);
	
	source.finish();
}

std::string GeometryExporter::makeTexcoordSourceId(std::string& geom_id, int layer_index)
{
	char suffix[20];
	sprintf(suffix, "-%d", layer_index);
	return getIdBySemantics(geom_id, COLLADASW::InputSemantic::TEXCOORD) + suffix;
}

//creates <source> for texcoords
void GeometryExporter::createTexcoordsSource(std::string geom_id, Mesh *me)
{
#if 0
	int totfaces = dm->getNumTessFaces(dm);
	MFace *mfaces = dm->getTessFaceArray(dm);
#endif
	int totfaces = me->totface;
	MFace *mfaces = me->mface;

	int totuv = 0;
	int i;

	// count totuv
	for (i = 0; i < totfaces; i++) {
		MFace *f = &mfaces[i];
		if (f->v4 == 0) {
			totuv+=3;
		}
		else {
			totuv+=4;
		}
	}

	int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);

	// write <source> for each layer
	// each <source> will get id like meshName + "map-channel-1"
	for (int a = 0; a < num_layers; a++) {
		MTFace *tface = (MTFace*)CustomData_get_layer_n(&me->fdata, CD_MTFACE, a);
		// char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, a);
		
		COLLADASW::FloatSourceF source(mSW);
		std::string layer_id = makeTexcoordSourceId(geom_id, a);
		source.setId(layer_id);
		source.setArrayId(layer_id + ARRAY_ID_SUFFIX);
		
		source.setAccessorCount(totuv);
		source.setAccessorStride(2);
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("S");
		param.push_back("T");
		
		source.prepareToAppendValues();
		
		for (i = 0; i < totfaces; i++) {
			MFace *f = &mfaces[i];
			
			for (int j = 0; j < (f->v4 == 0 ? 3 : 4); j++) {
				source.appendValues(tface[i].uv[j][0],
									tface[i].uv[j][1]);
			}
		}
		
		source.finish();
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

void GeometryExporter::create_normals(std::vector<Normal> &nor, std::vector<Face> &ind, Mesh *me)
{
	int i, j, v;
	MVert *vert = me->mvert;
	std::map<unsigned int, unsigned int> nshar;

	for (i = 0; i < me->totface; i++) {
		MFace *fa = &me->mface[i];
		Face f;
		unsigned int *nn = &f.v1;
		unsigned int *vv = &fa->v1;

		memset(&f, 0, sizeof(f));
		v = fa->v4 == 0 ? 3 : 4;

		if (!(fa->flag & ME_SMOOTH)) {
			Normal n;
			if (v == 4)
				normal_quad_v3(&n.x, vert[fa->v1].co, vert[fa->v2].co, vert[fa->v3].co, vert[fa->v4].co);
			else
				normal_tri_v3(&n.x, vert[fa->v1].co, vert[fa->v2].co, vert[fa->v3].co);
			nor.push_back(n);
		}

		for (j = 0; j < v; j++) {
			if (fa->flag & ME_SMOOTH) {
				if (nshar.find(*vv) != nshar.end())
					*nn = nshar[*vv];
				else {
					Normal n = {
						vert[*vv].no[0]/32767.0,
						vert[*vv].no[1]/32767.0,
						vert[*vv].no[2]/32767.0
					};
					nor.push_back(n);
					*nn = (unsigned int)nor.size() - 1;
					nshar[*vv] = *nn;
				}
				vv++;
			}
			else {
				*nn = (unsigned int)nor.size() - 1;
			}
			nn++;
		}

		ind.push_back(f);
	}
}

std::string GeometryExporter::getIdBySemantics(std::string geom_id, COLLADASW::InputSemantic::Semantics type, std::string other_suffix) {
	return geom_id + getSuffixBySemantic(type) + other_suffix;
}


COLLADASW::URI GeometryExporter::getUrlBySemantics(std::string geom_id, COLLADASW::InputSemantic::Semantics type, std::string other_suffix) {
	
	std::string id(getIdBySemantics(geom_id, type, other_suffix));
	return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
	
}

COLLADASW::URI GeometryExporter::makeUrl(std::string id)
{
	return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
}


/* int GeometryExporter::getTriCount(MFace *faces, int totface) {
	int i;
	int tris = 0;
	for (i = 0; i < totface; i++) {
		// if quad
		if (faces[i].v4 != 0)
			tris += 2;
		else
			tris++;
	}

	return tris;
	}*/
