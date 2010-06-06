/**
 * $Id$
 *
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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"

extern "C" 
{
#include "BKE_DerivedMesh.h"
#include "BKE_fcurve.h"
#include "BKE_animsys.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "ED_keyframing.h"
}

#include "MEM_guardedalloc.h"

#include "BKE_scene.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_action.h" // pose functions
#include "BKE_armature.h"
#include "BKE_image.h"
#include "BKE_utildefines.h"
#include "BKE_object.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_listbase.h"

#include "COLLADASWAsset.h"
#include "COLLADASWLibraryVisualScenes.h"
#include "COLLADASWNode.h"
#include "COLLADASWLibraryGeometries.h"
#include "COLLADASWSource.h"
#include "COLLADASWInstanceGeometry.h"
#include "COLLADASWInputList.h"
#include "COLLADASWPrimitves.h"
#include "COLLADASWVertices.h"
#include "COLLADASWLibraryAnimations.h"
#include "COLLADASWLibraryImages.h"
#include "COLLADASWLibraryEffects.h"
#include "COLLADASWImage.h"
#include "COLLADASWEffectProfile.h"
#include "COLLADASWColorOrTexture.h"
#include "COLLADASWParamTemplate.h"
#include "COLLADASWParamBase.h"
#include "COLLADASWSurfaceInitOption.h"
#include "COLLADASWSampler.h"
#include "COLLADASWScene.h"
//#include "COLLADASWSurface.h"
#include "COLLADASWTechnique.h"
#include "COLLADASWTexture.h"
#include "COLLADASWLibraryMaterials.h"
#include "COLLADASWBindMaterial.h"
#include "COLLADASWLibraryCameras.h"
#include "COLLADASWLibraryLights.h"
#include "COLLADASWInstanceCamera.h"
#include "COLLADASWInstanceLight.h"
#include "COLLADASWCameraOptic.h"
#include "COLLADASWConstants.h"
#include "COLLADASWLibraryControllers.h"
#include "COLLADASWInstanceController.h"
#include "COLLADASWBaseInputElement.h"

#include "collada_internal.h"
#include "DocumentExporter.h"

#include <vector>
#include <algorithm> // std::find

// arithb.c now has QuatToAxisAngle too
#if 0
// This function assumes that quat is normalized.
// The following document was used as reference:
// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToAngle/index.htm
void quat_to_axis_angle( float *axis, float *angle,float *q)
{
	// quat to axis angle
	*angle = 2 * acos(q[0]);
	float divisor = sqrt(1 - q[0] * q[0]);

	// test to avoid divide by zero, divisor is always positive
	if (divisor < 0.001f ) {
		axis[0] = 1.0f;
		axis[1] = 0.0f;
		axis[2] = 0.0f;
	}
	else {
		axis[0] = q[1] / divisor;
		axis[1] = q[2] / divisor;
		axis[2] = q[3] / divisor;
	}
}
#endif

char *CustomData_get_layer_name(const struct CustomData *data, int type, int n)
{
	int layer_index = CustomData_get_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index+n].name;
}

char *CustomData_get_active_layer_name(const CustomData *data, int type)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index].name;
}

/**
Translation map.
Used to translate every COLLADA id to a valid id, no matter what "wrong" letters may be
included. Look at the IDREF XSD declaration for more.
Follows strictly the COLLADA XSD declaration which explicitly allows non-english chars,
like special chars (e.g. micro sign), umlauts and so on.
The COLLADA spec also allows additional chars for member access ('.'), these
must obviously be removed too, otherwise they would be heavily misinterpreted.
*/
const unsigned char translate_map[256] = {
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 45, 95, 95,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 95, 95, 95, 95, 95, 95,
	95, 65, 66, 67, 68, 69, 70, 71,
	72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87,
	88, 89, 90, 95, 95, 95, 95, 95,
	95, 97, 98, 99, 100, 101, 102, 103,
	104, 105, 106, 107, 108, 109, 110, 111,
	112, 113, 114, 115, 116, 117, 118, 119,
	120, 121, 122, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 183,
	95, 95, 95, 95, 95, 95, 95, 95,
	192, 193, 194, 195, 196, 197, 198, 199,
	200, 201, 202, 203, 204, 205, 206, 207,
	208, 209, 210, 211, 212, 213, 214, 95,
	216, 217, 218, 219, 220, 221, 222, 223,
	224, 225, 226, 227, 228, 229, 230, 231,
	232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 95,
	248, 249, 250, 251, 252, 253, 254, 255};

/** Look at documentation of translate_map */
static std::string translate_id(const std::string &id)
{
	std::string id_translated = id;
	for (unsigned int i=0; i < id_translated.size(); i++)
	{
		id_translated[i] = translate_map[(unsigned int)id_translated[i]];
	}
	return id_translated;
}

static std::string id_name(void *id)
{
	return ((ID*)id)->name + 2;
}

static std::string get_geometry_id(Object *ob)
{
	return translate_id(id_name(ob)) + "-mesh";
}

static std::string get_light_id(Object *ob)
{
	return translate_id(id_name(ob)) + "-light";
}

static std::string get_camera_id(Object *ob)
{
	return translate_id(id_name(ob)) + "-camera";
}

std::string get_joint_id(Bone *bone, Object *ob_arm)
{
	return translate_id(id_name(ob_arm) + "_" + bone->name);
}


/*
  Utilities to avoid code duplication.
  Definition can take some time to understand, but they should be useful.
*/

// f should have
// void operator()(Object* ob)
template<class Functor>
void forEachMeshObjectInScene(Scene *sce, Functor &f)
{
	
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
		
		if (ob->type == OB_MESH && ob->data) {
			f(ob);
		}
		base= base->next;
		
	}
}

template<class Functor>
void forEachObjectInScene(Scene *sce, Functor &f)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
			
		f(ob);

		base= base->next;
	}
}

template<class Functor>
void forEachCameraObjectInScene(Scene *sce, Functor &f)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
			
		if (ob->type == OB_CAMERA && ob->data) {
			f(ob, sce);
		}
		base= base->next;
	}
}

template<class Functor>
void forEachLampObjectInScene(Scene *sce, Functor &f)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
			
		if (ob->type == OB_LAMP && ob->data) {
			f(ob);
		}
		base= base->next;
	}
}

// used in forEachMaterialInScene
template <class MaterialFunctor>
class ForEachMaterialFunctor
{
	std::vector<std::string> mMat; // contains list of material names, to avoid duplicate calling of f
	MaterialFunctor *f;
public:
	ForEachMaterialFunctor(MaterialFunctor *f) : f(f) { }
	void operator ()(Object *ob)
	{
		int a;
		for(a = 0; a < ob->totcol; a++) {

			Material *ma = give_current_material(ob, a+1);

			if (!ma) continue;

			std::string translated_id = translate_id(id_name(ma));
			if (find(mMat.begin(), mMat.end(), translated_id) == mMat.end()) {
				(*this->f)(ma, ob);

				mMat.push_back(translated_id);
			}
		}
	}
};

// calls f for each unique material linked to each object in sce
// f should have
// void operator()(Material* ma)
template<class Functor>
void forEachMaterialInScene(Scene *sce, Functor &f)
{
	ForEachMaterialFunctor<Functor> matfunc(&f);
	forEachMeshObjectInScene(sce, matfunc);
}

// OB_MESH is assumed
std::string getActiveUVLayerName(Object *ob)
{
	Mesh *me = (Mesh*)ob->data;

	int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
	if (num_layers)
		return std::string(CustomData_get_active_layer_name(&me->fdata, CD_MTFACE));
		
	return "";
}

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
	GeometryExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryGeometries(sw) {}

	void exportGeom(Scene *sce)
	{
		openLibrary();

		mScene = sce;
		forEachMeshObjectInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Object *ob)
	{
		// XXX don't use DerivedMesh, Mesh instead?

#if 0		
		DerivedMesh *dm = mesh_get_derived_final(mScene, ob, CD_MASK_BAREMESH);
#endif
		Mesh *me = (Mesh*)ob->data;
		std::string geom_id = get_geometry_id(ob);
		std::vector<Normal> nor;
		std::vector<Face> norind;

		bool has_color = (bool)CustomData_has_layer(&me->fdata, CD_MCOL);

		create_normals(nor, norind, me);

		// openMesh(geoId, geoName, meshId)
		openMesh(geom_id);
		
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
		verts.setId(getIdBySemantics(geom_id, COLLADASW::VERTEX));
		COLLADASW::InputList &input_list = verts.getInputList();
		COLLADASW::Input input(COLLADASW::POSITION, getUrlBySemantics(geom_id, COLLADASW::POSITION));
		input_list.push_back(input);
		verts.add();

		// XXX slow		
		if (ob->totcol) {
			for(int a = 0; a < ob->totcol; a++)	{
				// account for NULL materials, this should not normally happen?
				Material *ma = give_current_material(ob, a + 1);
				createPolylist(ma != NULL, a, has_uvs, has_color, ob, geom_id, norind);
			}
		}
		else {
			createPolylist(false, 0, has_uvs, has_color, ob, geom_id, norind);
		}
		
		closeMesh();
		closeGeometry();
		
#if 0
		dm->release(dm);
#endif
	}

	// powerful because it handles both cases when there is material and when there's not
	void createPolylist(bool has_material,
						int material_index,
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
			
			if ((has_material && f->mat_nr == material_index) || !has_material) {
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
			return;
		}
			
		Material *ma = has_material ? give_current_material(ob, material_index + 1) : NULL;
		COLLADASW::Polylist polylist(mSW);
			
		// sets count attribute in <polylist>
		polylist.setCount(faces_in_polylist);
			
		// sets material name
		if (has_material) {
			polylist.setMaterial(translate_id(id_name(ma)));
		}
				
		COLLADASW::InputList &til = polylist.getInputList();
			
		// creates <input> in <polylist> for vertices 
		COLLADASW::Input input1(COLLADASW::VERTEX, getUrlBySemantics(geom_id, COLLADASW::VERTEX), 0);
			
		// creates <input> in <polylist> for normals
		COLLADASW::Input input2(COLLADASW::NORMAL, getUrlBySemantics(geom_id, COLLADASW::NORMAL), 1);
			
		til.push_back(input1);
		til.push_back(input2);
			
		// if mesh has uv coords writes <input> for TEXCOORD
		int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);

		for (i = 0; i < num_layers; i++) {
			// char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, i);
			COLLADASW::Input input3(COLLADASW::TEXCOORD,
									makeUrl(makeTexcoordSourceId(geom_id, i)),
									2, // offset always 2, this is only until we have optimized UV sets
									i  // set number equals UV layer index
									);
			til.push_back(input3);
		}

		if (has_color) {
			COLLADASW::Input input4(COLLADASW::COLOR, getUrlBySemantics(geom_id, COLLADASW::COLOR), has_uvs ? 3 : 2);
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

			if ((has_material && f->mat_nr == material_index) || !has_material) {

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
	void createVertsSource(std::string geom_id, Mesh *me)
	{
#if 0
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
#endif
		int totverts = me->totvert;
		MVert *verts = me->mvert;
		
		COLLADASW::FloatSourceF source(mSW);
		source.setId(getIdBySemantics(geom_id, COLLADASW::POSITION));
		source.setArrayId(getIdBySemantics(geom_id, COLLADASW::POSITION) +
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

	void createVertexColorSource(std::string geom_id, Mesh *me)
	{
		if (!CustomData_has_layer(&me->fdata, CD_MCOL))
			return;

		MFace *f;
		int totcolor = 0, i, j;

		for (i = 0, f = me->mface; i < me->totface; i++, f++)
			totcolor += f->v4 ? 4 : 3;

		COLLADASW::FloatSourceF source(mSW);
		source.setId(getIdBySemantics(geom_id, COLLADASW::COLOR));
		source.setArrayId(getIdBySemantics(geom_id, COLLADASW::COLOR) + ARRAY_ID_SUFFIX);
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

	std::string makeTexcoordSourceId(std::string& geom_id, int layer_index)
	{
		char suffix[20];
		sprintf(suffix, "-%d", layer_index);
		return getIdBySemantics(geom_id, COLLADASW::TEXCOORD) + suffix;
	}

	//creates <source> for texcoords
	void createTexcoordsSource(std::string geom_id, Mesh *me)
	{
#if 0
		int totfaces = dm->getNumFaces(dm);
		MFace *mfaces = dm->getFaceArray(dm);
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
	void createNormalsSource(std::string geom_id, Mesh *me, std::vector<Normal>& nor)
	{
#if 0
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
#endif

		COLLADASW::FloatSourceF source(mSW);
		source.setId(getIdBySemantics(geom_id, COLLADASW::NORMAL));
		source.setArrayId(getIdBySemantics(geom_id, COLLADASW::NORMAL) +
						  ARRAY_ID_SUFFIX);
		source.setAccessorCount(nor.size());
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

	void create_normals(std::vector<Normal> &nor, std::vector<Face> &ind, Mesh *me)
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
						*nn = nor.size() - 1;
						nshar[*vv] = *nn;
					}
					vv++;
				}
				else {
					*nn = nor.size() - 1;
				}
				nn++;
			}

			ind.push_back(f);
		}
	}
	
	std::string getIdBySemantics(std::string geom_id, COLLADASW::Semantics type, std::string other_suffix = "") {
		return geom_id + getSuffixBySemantic(type) + other_suffix;
	}
	
	
	COLLADASW::URI getUrlBySemantics(std::string geom_id, COLLADASW::Semantics type, std::string other_suffix = "") {
		
		std::string id(getIdBySemantics(geom_id, type, other_suffix));
		return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
		
	}

	COLLADASW::URI makeUrl(std::string id)
	{
		return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
	}
	

	/*	int getTriCount(MFace *faces, int totface) {
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
};

class TransformWriter : protected TransformBase
{
protected:
	void add_node_transform(COLLADASW::Node& node, float mat[][4], float parent_mat[][4])
	{
		float loc[3], rot[3], scale[3];
		float local[4][4];

		if (parent_mat) {
			float invpar[4][4];
			invert_m4_m4(invpar, parent_mat);
			mul_m4_m4m4(local, mat, invpar);
		}
		else {
			copy_m4_m4(local, mat);
		}

		TransformBase::decompose(local, loc, rot, NULL, scale);
		
		add_transform(node, loc, rot, scale);
	}

	void add_node_transform_ob(COLLADASW::Node& node, Object *ob)
	{
		float rot[3], loc[3], scale[3];

		if (ob->parent) {
			float C[4][4], tmat[4][4], imat[4][4], mat[4][4];

			// factor out scale from obmat

			copy_v3_v3(scale, ob->size);

			ob->size[0] = ob->size[1] = ob->size[2] = 1.0f;
			object_to_mat4(ob, C);
			copy_v3_v3(ob->size, scale);

			mul_serie_m4(tmat, ob->parent->obmat, ob->parentinv, C, NULL, NULL, NULL, NULL, NULL);

			// calculate local mat

			invert_m4_m4(imat, ob->parent->obmat);
			mul_m4_m4m4(mat, tmat, imat);

			// done

			mat4_to_eul(rot, mat);
			copy_v3_v3(loc, mat[3]);
		}
		else {
			copy_v3_v3(loc, ob->loc);
			copy_v3_v3(rot, ob->rot);
			copy_v3_v3(scale, ob->size);
		}

		add_transform(node, loc, rot, scale);
	}

	void add_node_transform_identity(COLLADASW::Node& node)
	{
		float loc[] = {0.0f, 0.0f, 0.0f}, scale[] = {1.0f, 1.0f, 1.0f}, rot[] = {0.0f, 0.0f, 0.0f};
		add_transform(node, loc, rot, scale);
	}

private:
	void add_transform(COLLADASW::Node& node, float loc[3], float rot[3], float scale[3])
	{
		node.addTranslate("location", loc[0], loc[1], loc[2]);
		node.addRotateZ("rotationZ", COLLADABU::Math::Utils::radToDegF(rot[2]));
		node.addRotateY("rotationY", COLLADABU::Math::Utils::radToDegF(rot[1]));
		node.addRotateX("rotationX", COLLADABU::Math::Utils::radToDegF(rot[0]));
		node.addScale("scale", scale[0], scale[1], scale[2]);
	}
};

class InstanceWriter
{
protected:
	void add_material_bindings(COLLADASW::BindMaterial& bind_material, Object *ob)
	{
		for(int a = 0; a < ob->totcol; a++)	{
			Material *ma = give_current_material(ob, a+1);
				
			COLLADASW::InstanceMaterialList& iml = bind_material.getInstanceMaterialList();

			if (ma) {
				std::string matid(id_name(ma));
				matid = translate_id(matid);
				COLLADASW::InstanceMaterial im(matid, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, matid));
				
				// create <bind_vertex_input> for each uv layer
				Mesh *me = (Mesh*)ob->data;
				int totlayer = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
				
				for (int b = 0; b < totlayer; b++) {
					char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, b);
					im.push_back(COLLADASW::BindVertexInput(name, "TEXCOORD", b));
				}
				
				iml.push_back(im);
			}
		}
	}
};

// XXX exporter writes wrong data for shared armatures.  A separate
// controller should be written for each armature-mesh binding how do
// we make controller ids then?
class ArmatureExporter: public COLLADASW::LibraryControllers, protected TransformWriter, protected InstanceWriter
{
private:
	Scene *scene;

public:
	ArmatureExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryControllers(sw) {}

	// write bone nodes
	void add_armature_bones(Object *ob_arm, Scene *sce)
	{
		// write bone nodes
		bArmature *arm = (bArmature*)ob_arm->data;
		for (Bone *bone = (Bone*)arm->bonebase.first; bone; bone = bone->next) {
			// start from root bones
			if (!bone->parent)
				add_bone_node(bone, ob_arm);
		}
	}

	bool is_skinned_mesh(Object *ob)
	{
		return get_assigned_armature(ob) != NULL;
	}

	void add_instance_controller(Object *ob)
	{
		Object *ob_arm = get_assigned_armature(ob);
		bArmature *arm = (bArmature*)ob_arm->data;

		const std::string& controller_id = get_controller_id(ob_arm, ob);

		COLLADASW::InstanceController ins(mSW);
		ins.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, controller_id));

		// write root bone URLs
		Bone *bone;
		for (bone = (Bone*)arm->bonebase.first; bone; bone = bone->next) {
			if (!bone->parent)
				ins.addSkeleton(COLLADABU::URI(COLLADABU::Utils::EMPTY_STRING, get_joint_id(bone, ob_arm)));
		}

		InstanceWriter::add_material_bindings(ins.getBindMaterial(), ob);
			
		ins.add();
	}

	void export_controllers(Scene *sce)
	{
		scene = sce;

		openLibrary();

		forEachMeshObjectInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Object *ob)
	{
		Object *ob_arm = get_assigned_armature(ob);

		if (ob_arm /*&& !already_written(ob_arm)*/)
			export_controller(ob, ob_arm);
	}

private:

	UnitConverter converter;

#if 0
	std::vector<Object*> written_armatures;

	bool already_written(Object *ob_arm)
	{
		return std::find(written_armatures.begin(), written_armatures.end(), ob_arm) != written_armatures.end();
	}

	void wrote(Object *ob_arm)
	{
		written_armatures.push_back(ob_arm);
	}

	void find_objects_using_armature(Object *ob_arm, std::vector<Object *>& objects, Scene *sce)
	{
		objects.clear();

		Base *base= (Base*) sce->base.first;
		while(base) {
			Object *ob = base->object;
			
			if (ob->type == OB_MESH && get_assigned_armature(ob) == ob_arm) {
				objects.push_back(ob);
			}

			base= base->next;
		}
	}
#endif

	Object *get_assigned_armature(Object *ob)
	{
		Object *ob_arm = NULL;

		if (ob->parent && ob->partype == PARSKEL && ob->parent->type == OB_ARMATURE) {
			ob_arm = ob->parent;
		}
		else {
			ModifierData *mod = (ModifierData*)ob->modifiers.first;
			while (mod) {
				if (mod->type == eModifierType_Armature) {
					ob_arm = ((ArmatureModifierData*)mod)->object;
				}

				mod = mod->next;
			}
		}

		return ob_arm;
	}

	std::string get_joint_sid(Bone *bone, Object *ob_arm)
	{
		return get_joint_id(bone, ob_arm);
	}

	// parent_mat is armature-space
	void add_bone_node(Bone *bone, Object *ob_arm)
	{
		std::string node_id = get_joint_id(bone, ob_arm);
		std::string node_name = std::string(bone->name);
		std::string node_sid = get_joint_sid(bone, ob_arm);

		COLLADASW::Node node(mSW);

		node.setType(COLLADASW::Node::JOINT);
		node.setNodeId(node_id);
		node.setNodeName(node_name);
		node.setNodeSid(node_sid);

		node.start();

		add_bone_transform(ob_arm, bone, node);

		for (Bone *child = (Bone*)bone->childbase.first; child; child = child->next) {
			add_bone_node(child, ob_arm);
		}

		node.end();
	}

	void add_bone_transform(Object *ob_arm, Bone *bone, COLLADASW::Node& node)
	{
		bPoseChannel *pchan = get_pose_channel(ob_arm->pose, bone->name);

		float mat[4][4];

		if (bone->parent) {
			// get bone-space matrix from armature-space
			bPoseChannel *parchan = get_pose_channel(ob_arm->pose, bone->parent->name);

			float invpar[4][4];
			invert_m4_m4(invpar, parchan->pose_mat);
			mul_m4_m4m4(mat, pchan->pose_mat, invpar);
		}
		else {
			// get world-space from armature-space
			mul_m4_m4m4(mat, pchan->pose_mat, ob_arm->obmat);
		}

		TransformWriter::add_node_transform(node, mat, NULL);
	}

	std::string get_controller_id(Object *ob_arm, Object *ob)
	{
		return translate_id(id_name(ob_arm)) + "_" + translate_id(id_name(ob)) + SKIN_CONTROLLER_ID_SUFFIX;
	}

	// ob should be of type OB_MESH
	// both args are required
	void export_controller(Object* ob, Object *ob_arm)
	{
		// joint names
		// joint inverse bind matrices
		// vertex weights

		// input:
		// joint names: ob -> vertex group names
		// vertex group weights: me->dvert -> groups -> index, weight

		/*
		me->dvert:

		typedef struct MDeformVert {
			struct MDeformWeight *dw;
			int totweight;
			int flag;	// flag only in use for weightpaint now
		} MDeformVert;

		typedef struct MDeformWeight {
			int				def_nr;
			float			weight;
		} MDeformWeight;
		*/

		Mesh *me = (Mesh*)ob->data;
		if (!me->dvert) return;

		std::string controller_name = id_name(ob_arm);
		std::string controller_id = get_controller_id(ob_arm, ob);

		openSkin(controller_id, controller_name,
				 COLLADABU::URI(COLLADABU::Utils::EMPTY_STRING, get_geometry_id(ob)));

		add_bind_shape_mat(ob);

		std::string joints_source_id = add_joints_source(ob_arm, &ob->defbase, controller_id);
		std::string inv_bind_mat_source_id = add_inv_bind_mats_source(ob_arm, &ob->defbase, controller_id);
		std::string weights_source_id = add_weights_source(me, controller_id);

		add_joints_element(&ob->defbase, joints_source_id, inv_bind_mat_source_id);
		add_vertex_weights_element(weights_source_id, joints_source_id, me, ob_arm, &ob->defbase);

		closeSkin();
		closeController();
	}

	void add_joints_element(ListBase *defbase,
							const std::string& joints_source_id, const std::string& inv_bind_mat_source_id)
	{
		COLLADASW::JointsElement joints(mSW);
		COLLADASW::InputList &input = joints.getInputList();

		input.push_back(COLLADASW::Input(COLLADASW::JOINT, // constant declared in COLLADASWInputList.h
								   COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, joints_source_id)));
		input.push_back(COLLADASW::Input(COLLADASW::BINDMATRIX,
								   COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, inv_bind_mat_source_id)));
		joints.add();
	}

	void add_bind_shape_mat(Object *ob)
	{
		double bind_mat[4][4];

		converter.mat4_to_dae_double(bind_mat, ob->obmat);

		addBindShapeTransform(bind_mat);
	}

	std::string add_joints_source(Object *ob_arm, ListBase *defbase, const std::string& controller_id)
	{
		std::string source_id = controller_id + JOINTS_SOURCE_ID_SUFFIX;

		int totjoint = 0;
		bDeformGroup *def;
		for (def = (bDeformGroup*)defbase->first; def; def = def->next) {
			if (is_bone_defgroup(ob_arm, def))
				totjoint++;
		}

		COLLADASW::NameSource source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(totjoint);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("JOINT");

		source.prepareToAppendValues();

		for (def = (bDeformGroup*)defbase->first; def; def = def->next) {
			Bone *bone = get_bone_from_defgroup(ob_arm, def);
			if (bone)
				source.appendValues(get_joint_sid(bone, ob_arm));
		}

		source.finish();

		return source_id;
	}

	std::string add_inv_bind_mats_source(Object *ob_arm, ListBase *defbase, const std::string& controller_id)
	{
		std::string source_id = controller_id + BIND_POSES_SOURCE_ID_SUFFIX;

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(BLI_countlist(defbase));
		source.setAccessorStride(16);
		
		source.setParameterTypeName(&COLLADASW::CSWC::CSW_VALUE_TYPE_FLOAT4x4);
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("TRANSFORM");

		source.prepareToAppendValues();

		bPose *pose = ob_arm->pose;
		bArmature *arm = (bArmature*)ob_arm->data;

		int flag = arm->flag;

		// put armature in rest position
		if (!(arm->flag & ARM_RESTPOS)) {
			arm->flag |= ARM_RESTPOS;
			where_is_pose(scene, ob_arm);
		}

		for (bDeformGroup *def = (bDeformGroup*)defbase->first; def; def = def->next) {
			if (is_bone_defgroup(ob_arm, def)) {

				bPoseChannel *pchan = get_pose_channel(pose, def->name);

				float mat[4][4];
				float world[4][4];
				float inv_bind_mat[4][4];

				// make world-space matrix, pose_mat is armature-space
				mul_m4_m4m4(world, pchan->pose_mat, ob_arm->obmat);
				
				invert_m4_m4(mat, world);
				converter.mat4_to_dae(inv_bind_mat, mat);

				source.appendValues(inv_bind_mat);
			}
		}

		// back from rest positon
		if (!(flag & ARM_RESTPOS)) {
			arm->flag = flag;
			where_is_pose(scene, ob_arm);
		}

		source.finish();

		return source_id;
	}

	Bone *get_bone_from_defgroup(Object *ob_arm, bDeformGroup* def)
	{
		bPoseChannel *pchan = get_pose_channel(ob_arm->pose, def->name);
		return pchan ? pchan->bone : NULL;
	}

	bool is_bone_defgroup(Object *ob_arm, bDeformGroup* def)
	{
		return get_bone_from_defgroup(ob_arm, def) != NULL;
	}

	std::string add_weights_source(Mesh *me, const std::string& controller_id)
	{
		std::string source_id = controller_id + WEIGHTS_SOURCE_ID_SUFFIX;

		int i;
		int totweight = 0;

		for (i = 0; i < me->totvert; i++) {
			totweight += me->dvert[i].totweight;
		}

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(totweight);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("WEIGHT");

		source.prepareToAppendValues();

		// NOTE: COLLADA spec says weights should be normalized

		for (i = 0; i < me->totvert; i++) {
			MDeformVert *vert = &me->dvert[i];
			for (int j = 0; j < vert->totweight; j++) {
				source.appendValues(vert->dw[j].weight);
			}
		}

		source.finish();

		return source_id;
	}

	void add_vertex_weights_element(const std::string& weights_source_id, const std::string& joints_source_id, Mesh *me,
									Object *ob_arm, ListBase *defbase)
	{
		COLLADASW::VertexWeightsElement weights(mSW);
		COLLADASW::InputList &input = weights.getInputList();

		int offset = 0;
		input.push_back(COLLADASW::Input(COLLADASW::JOINT, // constant declared in COLLADASWInputList.h
										 COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, joints_source_id), offset++));
        input.push_back(COLLADASW::Input(COLLADASW::WEIGHT,
										 COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, weights_source_id), offset++));

		weights.setCount(me->totvert);

		// write number of deformers per vertex
		COLLADASW::PrimitivesBase::VCountList vcount;
		int i;
		for (i = 0; i < me->totvert; i++) {
			vcount.push_back(me->dvert[i].totweight);
		}

		weights.prepareToAppendVCountValues();
		weights.appendVertexCount(vcount);

		// def group index -> joint index
		std::map<int, int> joint_index_by_def_index;
		bDeformGroup *def;
		int j;
		for (def = (bDeformGroup*)defbase->first, i = 0, j = 0; def; def = def->next, i++) {
			if (is_bone_defgroup(ob_arm, def))
				joint_index_by_def_index[i] = j++;
			else
				joint_index_by_def_index[i] = -1;
		}

		weights.CloseVCountAndOpenVElement();

		// write deformer index - weight index pairs
		int weight_index = 0;
		for (i = 0; i < me->totvert; i++) {
			MDeformVert *dvert = &me->dvert[i];
			for (int j = 0; j < dvert->totweight; j++) {
				weights.appendValues(joint_index_by_def_index[dvert->dw[j].def_nr]);
				weights.appendValues(weight_index++);
			}
		}

		weights.finish();
	}
};

class SceneExporter: COLLADASW::LibraryVisualScenes, protected TransformWriter, protected InstanceWriter
{
	ArmatureExporter *arm_exporter;
public:
	SceneExporter(COLLADASW::StreamWriter *sw, ArmatureExporter *arm) : COLLADASW::LibraryVisualScenes(sw),
																		arm_exporter(arm) {}
	
	void exportScene(Scene *sce) {
 		// <library_visual_scenes> <visual_scene>
		std::string id_naming = id_name(sce);
		openVisualScene(translate_id(id_naming), id_naming);

		// write <node>s
		//forEachMeshObjectInScene(sce, *this);
		//forEachCameraObjectInScene(sce, *this);
		//forEachLampObjectInScene(sce, *this);
		exportHierarchy(sce);

		// </visual_scene> </library_visual_scenes>
		closeVisualScene();

		closeLibrary();
	}

	void exportHierarchy(Scene *sce)
	{
		Base *base= (Base*) sce->base.first;
		while(base) {
			Object *ob = base->object;

			if (!ob->parent) {
				switch(ob->type) {
				case OB_MESH:
				case OB_CAMERA:
				case OB_LAMP:
				case OB_EMPTY:
				case OB_ARMATURE:
					// write nodes....
					writeNodes(ob, sce);
					break;
				}
			}

			base= base->next;
		}
	}


	// called for each object
	//void operator()(Object *ob) {
	void writeNodes(Object *ob, Scene *sce)
	{
		COLLADASW::Node node(mSW);
		node.setNodeId(translate_id(id_name(ob)));
		node.setType(COLLADASW::Node::NODE);

		node.start();

		bool is_skinned_mesh = arm_exporter->is_skinned_mesh(ob);

		if (ob->type == OB_MESH && is_skinned_mesh)
			// for skinned mesh we write obmat in <bind_shape_matrix>
			TransformWriter::add_node_transform_identity(node);
		else
			TransformWriter::add_node_transform_ob(node, ob);
		
		// <instance_geometry>
		if (ob->type == OB_MESH) {
			if (is_skinned_mesh) {
				arm_exporter->add_instance_controller(ob);
			}
			else {
				COLLADASW::InstanceGeometry instGeom(mSW);
				instGeom.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_geometry_id(ob)));

				InstanceWriter::add_material_bindings(instGeom.getBindMaterial(), ob);
			
				instGeom.add();
			}
		}

		// <instance_controller>
		else if (ob->type == OB_ARMATURE) {
			arm_exporter->add_armature_bones(ob, sce);

			// XXX this looks unstable...
			node.end();
		}
		
		// <instance_camera>
		else if (ob->type == OB_CAMERA) {
			COLLADASW::InstanceCamera instCam(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_camera_id(ob)));
			instCam.add();
		}
		
		// <instance_light>
		else if (ob->type == OB_LAMP) {
			COLLADASW::InstanceLight instLa(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_light_id(ob)));
			instLa.add();
		}

		// empty object
		else if (ob->type == OB_EMPTY) {
		}

		// write nodes for child objects
		Base *b = (Base*) sce->base.first;
		while(b) {
			// cob - child object
			Object *cob = b->object;

			if (cob->parent == ob) {
				switch(cob->type) {
				case OB_MESH:
				case OB_CAMERA:
				case OB_LAMP:
				case OB_EMPTY:
				case OB_ARMATURE:
					// write node...
					writeNodes(cob, sce);
					break;
				}
			}

			b = b->next;
		}

		if (ob->type != OB_ARMATURE)
			node.end();
	}
};

class ImagesExporter: COLLADASW::LibraryImages
{
	const char *mfilename;
	std::vector<std::string> mImages; // contains list of written images, to avoid duplicates
public:
	ImagesExporter(COLLADASW::StreamWriter *sw, const char* filename) : COLLADASW::LibraryImages(sw), mfilename(filename)
	{}
	
	void exportImages(Scene *sce)
	{
		openLibrary();

		forEachMaterialInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Material *ma, Object *ob)
	{
		int a;
		for (a = 0; a < MAX_MTEX; a++) {
			MTex *mtex = ma->mtex[a];
			if (mtex && mtex->tex && mtex->tex->ima) {

				Image *image = mtex->tex->ima;
				std::string name(id_name(image));
				name = translate_id(name);
				char rel[FILE_MAX];
				char abs[FILE_MAX];
				char src[FILE_MAX];
				char dir[FILE_MAX];
				
				BLI_split_dirfile(mfilename, dir, NULL);

				BKE_rebase_path(abs, sizeof(abs), rel, sizeof(rel), G.sce, image->name, dir);

				if (abs[0] != '\0') {

					// make absolute source path
					BLI_strncpy(src, image->name, sizeof(src));
					BLI_path_abs(src, G.sce);

					// make dest directory if it doesn't exist
					BLI_make_existing_file(abs);
				
					if (BLI_copy_fileops(src, abs) != 0) {
						fprintf(stderr, "Cannot copy image to file's directory. \n");
					}
				} 
				
				if (find(mImages.begin(), mImages.end(), name) == mImages.end()) {
					COLLADASW::Image img(COLLADABU::URI(COLLADABU::URI::nativePathToUri(rel)), name);
					img.add(mSW);

					mImages.push_back(name);
				}
			}
		}
	}
};

class EffectsExporter: COLLADASW::LibraryEffects
{
public:
	EffectsExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryEffects(sw){}
	void exportEffects(Scene *sce)
	{
		openLibrary();

		forEachMaterialInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Material *ma, Object *ob)
	{
		// create a list of indices to textures of type TEX_IMAGE
		std::vector<int> tex_indices;
		createTextureIndices(ma, tex_indices);

		openEffect(translate_id(id_name(ma)) + "-effect");
		
		COLLADASW::EffectProfile ep(mSW);
		ep.setProfileType(COLLADASW::EffectProfile::COMMON);
		ep.openProfile();
		// set shader type - one of three blinn, phong or lambert
		if (ma->spec_shader == MA_SPEC_BLINN) {
			ep.setShaderType(COLLADASW::EffectProfile::BLINN);
			// shininess
			ep.setShininess(ma->spec);
		}
		else if (ma->spec_shader == MA_SPEC_PHONG) {
			ep.setShaderType(COLLADASW::EffectProfile::PHONG);
			// shininess
			// XXX not sure, stolen this from previous Collada plugin
			ep.setShininess(ma->har / 4);
		}
		else {
			// XXX write warning "Current shader type is not supported" 
			ep.setShaderType(COLLADASW::EffectProfile::LAMBERT);
		}
		// index of refraction
		if (ma->mode & MA_RAYTRANSP) {
			ep.setIndexOfRefraction(ma->ang);
		}
		else {
			ep.setIndexOfRefraction(1.0f);
		}
	
		COLLADASW::ColorOrTexture cot;

		// transparency
		// Tod: because we are in A_ONE mode transparency is calculated like this:
		ep.setTransparency(1.0f);
		cot = getcol(0.0f, 0.0f, 0.0f, ma->alpha);
		ep.setTransparent(cot);

		// emission
		cot=getcol(ma->emit, ma->emit, ma->emit, 1.0f);
		ep.setEmission(cot);

		// diffuse 
		cot = getcol(ma->r, ma->g, ma->b, 1.0f);
		ep.setDiffuse(cot);

		// ambient
		cot = getcol(ma->ambr, ma->ambg, ma->ambb, 1.0f);
		ep.setAmbient(cot);

		// reflective, reflectivity
		if (ma->mode & MA_RAYMIRROR) {
			cot = getcol(ma->mirr, ma->mirg, ma->mirb, 1.0f);
			ep.setReflective(cot);
			ep.setReflectivity(ma->ray_mirror);
		}
		else {
			cot = getcol(ma->specr, ma->specg, ma->specb, 1.0f);
			ep.setReflective(cot);
			ep.setReflectivity(ma->spec);
		}

		// specular
		if (ep.getShaderType() != COLLADASW::EffectProfile::LAMBERT) {
			cot = getcol(ma->specr, ma->specg, ma->specb, 1.0f);
			ep.setSpecular(cot);
		}	

		// XXX make this more readable if possible

		// create <sampler> and <surface> for each image
		COLLADASW::Sampler samplers[MAX_MTEX];
		//COLLADASW::Surface surfaces[MAX_MTEX];
		//void *samp_surf[MAX_MTEX][2];
		void *samp_surf[MAX_MTEX][1];
		
		// image to index to samp_surf map
		// samp_surf[index] stores 2 pointers, sampler and surface
		std::map<std::string, int> im_samp_map;

		unsigned int a, b;
		for (a = 0, b = 0; a < tex_indices.size(); a++) {
			MTex *t = ma->mtex[tex_indices[a]];
			Image *ima = t->tex->ima;
			
			std::string key(id_name(ima));
			key = translate_id(key);

			// create only one <sampler>/<surface> pair for each unique image
			if (im_samp_map.find(key) == im_samp_map.end()) {
				//<newparam> <surface> <init_from>
			// 	COLLADASW::Surface surface(COLLADASW::Surface::SURFACE_TYPE_2D,
// 										   key + COLLADASW::Surface::SURFACE_SID_SUFFIX);
// 				COLLADASW::SurfaceInitOption sio(COLLADASW::SurfaceInitOption::INIT_FROM);
// 				sio.setImageReference(key);
// 				surface.setInitOption(sio);
				
				//<newparam> <sampler> <source>
				COLLADASW::Sampler sampler(COLLADASW::Sampler::SAMPLER_TYPE_2D,
										   key + COLLADASW::Sampler::SAMPLER_SID_SUFFIX,
										   key + COLLADASW::Sampler::SURFACE_SID_SUFFIX);
				sampler.setImageId(key);
				// copy values to arrays since they will live longer
				samplers[a] = sampler;
				//surfaces[a] = surface;
				
				// store pointers so they can be used later when we create <texture>s
				samp_surf[b][0] = &samplers[a];
				//samp_surf[b][1] = &surfaces[a];
				
				im_samp_map[key] = b;
				b++;
			}
		}

		// used as fallback when MTex->uvname is "" (this is pretty common)
		// it is indeed the correct value to use in that case
		std::string active_uv(getActiveUVLayerName(ob));

		// write textures
		// XXX very slow
		for (a = 0; a < tex_indices.size(); a++) {
			MTex *t = ma->mtex[tex_indices[a]];
			Image *ima = t->tex->ima;

			// we assume map input is always TEXCO_UV

			std::string key(id_name(ima));
			key = translate_id(key);
			int i = im_samp_map[key];
			COLLADASW::Sampler *sampler = (COLLADASW::Sampler*)samp_surf[i][0];
			//COLLADASW::Surface *surface = (COLLADASW::Surface*)samp_surf[i][1];

			std::string uvname = strlen(t->uvname) ? t->uvname : active_uv;

			// color
			if (t->mapto & MAP_COL) {
				ep.setDiffuse(createTexture(ima, uvname, sampler));
			}
			// ambient
			if (t->mapto & MAP_AMB) {
				ep.setAmbient(createTexture(ima, uvname, sampler));
			}
			// specular
			if (t->mapto & MAP_SPEC) {
				ep.setSpecular(createTexture(ima, uvname, sampler));
			}
			// emission
			if (t->mapto & MAP_EMIT) {
				ep.setEmission(createTexture(ima, uvname, sampler));
			}
			// reflective
			if (t->mapto & MAP_REF) {
				ep.setReflective(createTexture(ima, uvname, sampler));
			}
			// alpha
			if (t->mapto & MAP_ALPHA) {
				ep.setTransparent(createTexture(ima, uvname, sampler));
			}
			// extension:
			// Normal map --> Must be stored with <extra> tag as different technique, 
			// since COLLADA doesn't support normal maps, even in current COLLADA 1.5.
			if (t->mapto & MAP_NORM) {
				COLLADASW::Texture texture(key);
				texture.setTexcoord(uvname);
				texture.setSampler(*sampler);
				// technique FCOLLADA, with the <bump> tag, is most likely the best understood,
				// most widespread de-facto standard.
				texture.setProfileName("FCOLLADA");
				texture.setChildElementName("bump");				
				ep.addExtraTechniqueColorOrTexture(COLLADASW::ColorOrTexture(texture));
			}
		}
		// performs the actual writing
		ep.addProfileElements();
		bool twoSided = false;
		if (ob->type == OB_MESH && ob->data) {
			Mesh *me = (Mesh*)ob->data;
			if (me->flag & ME_TWOSIDED)
				twoSided = true;
		}
		if (twoSided)
			ep.addExtraTechniqueParameter("GOOGLEEARTH", "double_sided", 1);
		ep.addExtraTechniques(mSW);

		ep.closeProfile();
		if (twoSided)
			mSW->appendTextBlock("<extra><technique profile=\"MAX3D\"><double_sided>1</double_sided></technique></extra>");
		closeEffect();	
	}
	
	COLLADASW::ColorOrTexture createTexture(Image *ima,
											std::string& uv_layer_name,
											COLLADASW::Sampler *sampler
											/*COLLADASW::Surface *surface*/)
	{
		
		COLLADASW::Texture texture(translate_id(id_name(ima)));
		texture.setTexcoord(uv_layer_name);
		//texture.setSurface(*surface);
		texture.setSampler(*sampler);
		
		COLLADASW::ColorOrTexture cot(texture);
		return cot;
	}
	
	COLLADASW::ColorOrTexture getcol(float r, float g, float b, float a)
	{
		COLLADASW::Color color(r,g,b,a);
		COLLADASW::ColorOrTexture cot(color);
		return cot;
	}
	
	//returns the array of mtex indices which have image 
	//need this for exporting textures
	void createTextureIndices(Material *ma, std::vector<int> &indices)
	{
		indices.clear();

		for (int a = 0; a < MAX_MTEX; a++) {
			if (ma->mtex[a] &&
				ma->mtex[a]->tex->type == TEX_IMAGE &&
				ma->mtex[a]->texco == TEXCO_UV){
				indices.push_back(a);
			}
		}
	}
};

class MaterialsExporter: COLLADASW::LibraryMaterials
{
public:
	MaterialsExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryMaterials(sw){}
	void exportMaterials(Scene *sce)
	{
		openLibrary();

		forEachMaterialInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Material *ma, Object *ob)
	{
		std::string name(id_name(ma));

		openMaterial(translate_id(name), name);

		std::string efid = translate_id(name) + "-effect";
		addInstanceEffect(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, efid));

		closeMaterial();
	}
};

class CamerasExporter: COLLADASW::LibraryCameras
{
public:
	CamerasExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryCameras(sw){}
	void exportCameras(Scene *sce)
	{
		openLibrary();
		
		forEachCameraObjectInScene(sce, *this);
		
		closeLibrary();
	}
	void operator()(Object *ob, Scene *sce)
	{
		// XXX add other params later
		Camera *cam = (Camera*)ob->data;
		std::string cam_id(get_camera_id(ob));
		std::string cam_name(id_name(cam));
		
		if (cam->type == CAM_PERSP) {
			COLLADASW::PerspectiveOptic persp(mSW);
			persp.setXFov(1.0);
			persp.setAspectRatio(0.1);
			persp.setZFar(cam->clipend);
			persp.setZNear(cam->clipsta);
			COLLADASW::Camera ccam(mSW, &persp, cam_id, cam_name);
			addCamera(ccam);
		}
		else {
			COLLADASW::OrthographicOptic ortho(mSW);
			ortho.setXMag(1.0);
			ortho.setAspectRatio(0.1);
			ortho.setZFar(cam->clipend);
			ortho.setZNear(cam->clipsta);
			COLLADASW::Camera ccam(mSW, &ortho, cam_id, cam_name);
			addCamera(ccam);
		}
	}	
};

class LightsExporter: COLLADASW::LibraryLights
{
public:
	LightsExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryLights(sw){}
	void exportLights(Scene *sce)
	{
		openLibrary();
		
		forEachLampObjectInScene(sce, *this);
		
		closeLibrary();
	}
	void operator()(Object *ob)
	{
		Lamp *la = (Lamp*)ob->data;
		std::string la_id(get_light_id(ob));
		std::string la_name(id_name(la));
		COLLADASW::Color col(la->r, la->g, la->b);
		float e = la->energy;
		
		// sun
		if (la->type == LA_SUN) {
			COLLADASW::DirectionalLight cla(mSW, la_id, la_name, e);
			cla.setColor(col);
			addLight(cla);
		}
		// hemi
		else if (la->type == LA_HEMI) {
			COLLADASW::AmbientLight cla(mSW, la_id, la_name, e);
			cla.setColor(col);
			addLight(cla);
		}
		// spot
		else if (la->type == LA_SPOT) {
			COLLADASW::SpotLight cla(mSW, la_id, la_name, e);
			cla.setColor(col);
			cla.setFallOffAngle(la->spotsize);
			cla.setFallOffExponent(la->spotblend);
			cla.setLinearAttenuation(la->att1);
			cla.setQuadraticAttenuation(la->att2);
			addLight(cla);
		}
		// lamp
		else if (la->type == LA_LOCAL) {
			COLLADASW::PointLight cla(mSW, la_id, la_name, e);
			cla.setColor(col);
			cla.setLinearAttenuation(la->att1);
			cla.setQuadraticAttenuation(la->att2);
			addLight(cla);
		}
		// area lamp is not supported
		// it will be exported as a local lamp
		else {
			COLLADASW::PointLight cla(mSW, la_id, la_name, e);
			cla.setColor(col);
			cla.setLinearAttenuation(la->att1);
			cla.setQuadraticAttenuation(la->att2);
			addLight(cla);
		}
	}
};

// TODO: it would be better to instantiate animations rather than create a new one per object
// COLLADA allows this through multiple <channel>s in <animation>.
// For this to work, we need to know objects that use a certain action.
class AnimationExporter: COLLADASW::LibraryAnimations
{
	Scene *scene;

public:

	AnimationExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryAnimations(sw) {}

	void exportAnimations(Scene *sce)
	{
		this->scene = sce;

		openLibrary();
		
		forEachObjectInScene(sce, *this);
		
		closeLibrary();
	}

	// called for each exported object
	void operator() (Object *ob) 
	{
		if (!ob->adt || !ob->adt->action) return;
		
		FCurve *fcu = (FCurve*)ob->adt->action->curves.first;
		
		if (ob->type == OB_ARMATURE) {
			if (!ob->data) return;

			bArmature *arm = (bArmature*)ob->data;
			for (Bone *bone = (Bone*)arm->bonebase.first; bone; bone = bone->next)
				write_bone_animation(ob, bone);
		}
		else {
			while (fcu) {
				// TODO "rotation_quaternion" is also possible for objects (although euler is default)
				if ((!strcmp(fcu->rna_path, "location") || !strcmp(fcu->rna_path, "scale")) ||
					(!strcmp(fcu->rna_path, "rotation_euler") && ob->rotmode == ROT_MODE_EUL))
					dae_animation(fcu, id_name(ob));

				fcu = fcu->next;
			}
		}
	}

protected:

	void dae_animation(FCurve *fcu, std::string ob_name)
	{
		const char *axis_names[] = {"X", "Y", "Z"};
		const char *axis_name = NULL;
		char anim_id[200];
		
		if (fcu->array_index < 3)
			axis_name = axis_names[fcu->array_index];

		BLI_snprintf(anim_id, sizeof(anim_id), "%s_%s_%s", (char*)translate_id(ob_name).c_str(),
					 fcu->rna_path, axis_names[fcu->array_index]);

		// check rna_path is one of: rotation, scale, location

		openAnimation(anim_id, COLLADABU::Utils::EMPTY_STRING);

		// create input source
		std::string input_id = create_source_from_fcurve(Sampler::INPUT, fcu, anim_id, axis_name);

		// create output source
		std::string output_id = create_source_from_fcurve(Sampler::OUTPUT, fcu, anim_id, axis_name);

		// create interpolations source
		std::string interpolation_id = create_interpolation_source(fcu->totvert, anim_id, axis_name);

		std::string sampler_id = std::string(anim_id) + SAMPLER_ID_SUFFIX;
		COLLADASW::LibraryAnimations::Sampler sampler(sampler_id);
		std::string empty;
		sampler.addInput(Sampler::INPUT, COLLADABU::URI(empty, input_id));
		sampler.addInput(Sampler::OUTPUT, COLLADABU::URI(empty, output_id));

		// this input is required
		sampler.addInput(Sampler::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

		addSampler(sampler);

		std::string target = translate_id(ob_name)
			+ "/" + get_transform_sid(fcu->rna_path, -1, axis_name, true);
		addChannel(COLLADABU::URI(empty, sampler_id), target);

		closeAnimation();
	}

	void write_bone_animation(Object *ob_arm, Bone *bone)
	{
		if (!ob_arm->adt)
			return;

		for (int i = 0; i < 3; i++)
			sample_and_write_bone_animation(ob_arm, bone, i);

		for (Bone *child = (Bone*)bone->childbase.first; child; child = child->next)
			write_bone_animation(ob_arm, child);
	}

	void sample_and_write_bone_animation(Object *ob_arm, Bone *bone, int transform_type)
	{
		bArmature *arm = (bArmature*)ob_arm->data;
		int flag = arm->flag;
		std::vector<float> fra;
		char prefix[256];

		BLI_snprintf(prefix, sizeof(prefix), "pose.bones[\"%s\"]", bone->name);

		bPoseChannel *pchan = get_pose_channel(ob_arm->pose, bone->name);
		if (!pchan)
			return;

		switch (transform_type) {
		case 0:
			find_rotation_frames(ob_arm, fra, prefix, pchan->rotmode);
			break;
		case 1:
			find_frames(ob_arm, fra, prefix, "scale");
			break;
		case 2:
			find_frames(ob_arm, fra, prefix, "location");
			break;
		default:
			return;
		}

		// exit rest position
		if (flag & ARM_RESTPOS) {
			arm->flag &= ~ARM_RESTPOS;
			where_is_pose(scene, ob_arm);
		}

		if (fra.size()) {
			float *v = (float*)MEM_callocN(sizeof(float) * 3 * fra.size(), "temp. anim frames");
			sample_animation(v, fra, transform_type, bone, ob_arm);

			if (transform_type == 0) {
				// write x, y, z curves separately if it is rotation
				float *c = (float*)MEM_callocN(sizeof(float) * fra.size(), "temp. anim frames");
				for (int i = 0; i < 3; i++) {
					for (unsigned int j = 0; j < fra.size(); j++)
						c[j] = v[j * 3 + i];

					dae_bone_animation(fra, c, transform_type, i, id_name(ob_arm), bone->name);
				}
				MEM_freeN(c);
			}
			else {
				// write xyz at once if it is location or scale
				dae_bone_animation(fra, v, transform_type, -1, id_name(ob_arm), bone->name);
			}

			MEM_freeN(v);
		}

		// restore restpos
		if (flag & ARM_RESTPOS) 
			arm->flag = flag;
		where_is_pose(scene, ob_arm);
	}

	void sample_animation(float *v, std::vector<float> &frames, int type, Bone *bone, Object *ob_arm)
	{
		bPoseChannel *pchan, *parchan = NULL;
		bPose *pose = ob_arm->pose;

		pchan = get_pose_channel(pose, bone->name);

		if (!pchan)
			return;

		parchan = pchan->parent;

		enable_fcurves(ob_arm->adt->action, bone->name);

		std::vector<float>::iterator it;
		for (it = frames.begin(); it != frames.end(); it++) {
			float mat[4][4], ipar[4][4];

			float ctime = bsystem_time(scene, ob_arm, *it, 0.0f);

			BKE_animsys_evaluate_animdata(&ob_arm->id, ob_arm->adt, *it, ADT_RECALC_ANIM);
			where_is_pose_bone(scene, ob_arm, pchan, ctime, 1);

			// compute bone local mat
			if (bone->parent) {
				invert_m4_m4(ipar, parchan->pose_mat);
				mul_m4_m4m4(mat, pchan->pose_mat, ipar);
			}
			else
				copy_m4_m4(mat, pchan->pose_mat);

			switch (type) {
			case 0:
				mat4_to_eul(v, mat);
				break;
			case 1:
				mat4_to_size(v, mat);
				break;
			case 2:
				copy_v3_v3(v, mat[3]);
				break;
			}

			v += 3;
		}

		enable_fcurves(ob_arm->adt->action, NULL);
	}

	// dae_bone_animation -> add_bone_animation
	// (blend this into dae_bone_animation)
	void dae_bone_animation(std::vector<float> &fra, float *v, int tm_type, int axis, std::string ob_name, std::string bone_name)
	{
		const char *axis_names[] = {"X", "Y", "Z"};
		const char *axis_name = NULL;
		char anim_id[200];
		bool is_rot = tm_type == 0;
		
		if (!fra.size())
			return;

		char rna_path[200];
		BLI_snprintf(rna_path, sizeof(rna_path), "pose.bones[\"%s\"].%s", bone_name.c_str(),
					 tm_type == 0 ? "rotation_quaternion" : (tm_type == 1 ? "scale" : "location"));

		if (axis > -1)
			axis_name = axis_names[axis];
		
		std::string transform_sid = get_transform_sid(NULL, tm_type, axis_name, false);
		
		BLI_snprintf(anim_id, sizeof(anim_id), "%s_%s_%s", (char*)translate_id(ob_name).c_str(),
					 (char*)translate_id(bone_name).c_str(), (char*)transform_sid.c_str());

		openAnimation(anim_id, COLLADABU::Utils::EMPTY_STRING);

		// create input source
		std::string input_id = create_source_from_vector(Sampler::INPUT, fra, is_rot, anim_id, axis_name);

		// create output source
		std::string output_id;
		if (axis == -1)
			output_id = create_xyz_source(v, fra.size(), anim_id);
		else
			output_id = create_source_from_array(Sampler::OUTPUT, v, fra.size(), is_rot, anim_id, axis_name);

		// create interpolations source
		std::string interpolation_id = create_interpolation_source(fra.size(), anim_id, axis_name);

		std::string sampler_id = std::string(anim_id) + SAMPLER_ID_SUFFIX;
		COLLADASW::LibraryAnimations::Sampler sampler(sampler_id);
		std::string empty;
		sampler.addInput(Sampler::INPUT, COLLADABU::URI(empty, input_id));
		sampler.addInput(Sampler::OUTPUT, COLLADABU::URI(empty, output_id));

		// TODO create in/out tangents source

		// this input is required
		sampler.addInput(Sampler::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

		addSampler(sampler);

		std::string target = translate_id(ob_name + "_" + bone_name) + "/" + transform_sid;
		addChannel(COLLADABU::URI(empty, sampler_id), target);

		closeAnimation();
	}

	float convert_time(float frame)
	{
		return FRA2TIME(frame);
	}

	float convert_angle(float angle)
	{
		return COLLADABU::Math::Utils::radToDegF(angle);
	}

	std::string get_semantic_suffix(Sampler::Semantic semantic)
	{
		switch(semantic) {
		case Sampler::INPUT:
			return INPUT_SOURCE_ID_SUFFIX;
		case Sampler::OUTPUT:
			return OUTPUT_SOURCE_ID_SUFFIX;
		case Sampler::INTERPOLATION:
			return INTERPOLATION_SOURCE_ID_SUFFIX;
		case Sampler::IN_TANGENT:
			return INTANGENT_SOURCE_ID_SUFFIX;
		case Sampler::OUT_TANGENT:
			return OUTTANGENT_SOURCE_ID_SUFFIX;
		default:
			break;
		}
		return "";
	}

	void add_source_parameters(COLLADASW::SourceBase::ParameterNameList& param,
							   Sampler::Semantic semantic, bool is_rot, const char *axis)
	{
		switch(semantic) {
		case Sampler::INPUT:
			param.push_back("TIME");
			break;
		case Sampler::OUTPUT:
			if (is_rot) {
				param.push_back("ANGLE");
			}
			else {
				if (axis) {
					param.push_back(axis);
				}
				else {
					param.push_back("X");
					param.push_back("Y");
					param.push_back("Z");
				}
			}
			break;
		case Sampler::IN_TANGENT:
		case Sampler::OUT_TANGENT:
			param.push_back("X");
			param.push_back("Y");
			break;
		default:
			break;
		}
	}

	void get_source_values(BezTriple *bezt, Sampler::Semantic semantic, bool rotation, float *values, int *length)
	{
		switch (semantic) {
		case Sampler::INPUT:
			*length = 1;
			values[0] = convert_time(bezt->vec[1][0]);
			break;
		case Sampler::OUTPUT:
			*length = 1;
			if (rotation) {
				values[0] = convert_angle(bezt->vec[1][1]);
			}
			else {
				values[0] = bezt->vec[1][1];
			}
			break;
		case Sampler::IN_TANGENT:
		case Sampler::OUT_TANGENT:
			// XXX
			*length = 2;
			break;
		default:
			*length = 0;
			break;
		}
	}

	std::string create_source_from_fcurve(Sampler::Semantic semantic, FCurve *fcu, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		//bool is_rotation = !strcmp(fcu->rna_path, "rotation");
		bool is_rotation = false;
		
		if (strstr(fcu->rna_path, "rotation")) is_rotation = true;
		
		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(fcu->totvert);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rotation, axis_name);

		source.prepareToAppendValues();

		for (unsigned int i = 0; i < fcu->totvert; i++) {
			float values[3]; // be careful!
			int length = 0;

			get_source_values(&fcu->bezt[i], semantic, is_rotation, values, &length);
			for (int j = 0; j < length; j++)
				source.appendValues(values[j]);
		}

		source.finish();

		return source_id;
	}

	std::string create_source_from_array(Sampler::Semantic semantic, float *v, int tot, bool is_rot, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(tot);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rot, axis_name);

		source.prepareToAppendValues();

		for (int i = 0; i < tot; i++) {
			float val = v[i];
			if (semantic == Sampler::INPUT)
				val = convert_time(val);
			else if (is_rot)
				val = convert_angle(val);
			source.appendValues(val);
		}

		source.finish();

		return source_id;
	}

	std::string create_source_from_vector(Sampler::Semantic semantic, std::vector<float> &fra, bool is_rot, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(fra.size());
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rot, axis_name);

		source.prepareToAppendValues();

		std::vector<float>::iterator it;
		for (it = fra.begin(); it != fra.end(); it++) {
			float val = *it;
			if (semantic == Sampler::INPUT)
				val = convert_time(val);
			else if (is_rot)
				val = convert_angle(val);
			source.appendValues(val);
		}

		source.finish();

		return source_id;
	}

	// only used for sources with OUTPUT semantic
	std::string create_xyz_source(float *v, int tot, const std::string& anim_id)
	{
		Sampler::Semantic semantic = Sampler::OUTPUT;
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(tot);
		source.setAccessorStride(3);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, false, NULL);

		source.prepareToAppendValues();

		for (int i = 0; i < tot; i++) {
			source.appendValues(*v, *(v + 1), *(v + 2));
			v += 3;
		}

		source.finish();

		return source_id;
	}

	std::string create_interpolation_source(int tot, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(Sampler::INTERPOLATION);

		COLLADASW::NameSource source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(tot);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("INTERPOLATION");

		source.prepareToAppendValues();

		for (int i = 0; i < tot; i++) {
			source.appendValues(LINEAR_NAME);
		}

		source.finish();

		return source_id;
	}

	// for rotation, axis name is always appended and the value of append_axis is ignored
	std::string get_transform_sid(char *rna_path, int tm_type, const char *axis_name, bool append_axis)
	{
		std::string tm_name;

		// when given rna_path, determine tm_type from it
		if (rna_path) {
			char *name = extract_transform_name(rna_path);

			if (strstr(name, "rotation"))
				tm_type = 0;
			else if (!strcmp(name, "scale"))
				tm_type = 1;
			else if (!strcmp(name, "location"))
				tm_type = 2;
			else
				tm_type = -1;
		}

		switch (tm_type) {
		case 0:
			return std::string("rotation") + std::string(axis_name) + ".ANGLE";
		case 1:
			tm_name = "scale";
			break;
		case 2:
			tm_name = "location";
			break;
		default:
			tm_name = "";
			break;
		}

		if (tm_name.size()) {
			if (append_axis)
				return tm_name + std::string(".") + std::string(axis_name);
			else
				return tm_name;
		}

		return std::string("");
	}

	char *extract_transform_name(char *rna_path)
	{
		char *dot = strrchr(rna_path, '.');
		return dot ? (dot + 1) : rna_path;
	}

	void find_frames(Object *ob, std::vector<float> &fra, const char *prefix, const char *tm_name)
	{
		FCurve *fcu= (FCurve*)ob->adt->action->curves.first;

		for (; fcu; fcu = fcu->next) {
			if (prefix && strncmp(prefix, fcu->rna_path, strlen(prefix)))
				continue;

			char *name = extract_transform_name(fcu->rna_path);
			if (!strcmp(name, tm_name)) {
				for (unsigned int i = 0; i < fcu->totvert; i++) {
					float f = fcu->bezt[i].vec[1][0];
					if (std::find(fra.begin(), fra.end(), f) == fra.end())
						fra.push_back(f);
				}
			}
		}
	}

	void find_rotation_frames(Object *ob, std::vector<float> &fra, const char *prefix, int rotmode)
	{
		if (rotmode > 0)
			find_frames(ob, fra, prefix, "rotation_euler");
		else if (rotmode == ROT_MODE_QUAT)
			find_frames(ob, fra, prefix, "rotation_quaternion");
		else if (rotmode == ROT_MODE_AXISANGLE)
			;
	}

	// enable fcurves driving a specific bone, disable all the rest
	// if bone_name = NULL enable all fcurves
	void enable_fcurves(bAction *act, char *bone_name)
	{
		FCurve *fcu;
		char prefix[200];

		if (bone_name)
			BLI_snprintf(prefix, sizeof(prefix), "pose.bones[\"%s\"]", bone_name);

		for (fcu = (FCurve*)act->curves.first; fcu; fcu = fcu->next) {
			if (bone_name) {
				if (!strncmp(fcu->rna_path, prefix, strlen(prefix)))
					fcu->flag &= ~FCURVE_DISABLED;
				else
					fcu->flag |= FCURVE_DISABLED;
			}
			else {
				fcu->flag &= ~FCURVE_DISABLED;
			}
		}
	}
};

void DocumentExporter::exportCurrentScene(Scene *sce, const char* filename)
{
	COLLADABU::NativeString native_filename =
		COLLADABU::NativeString(std::string(filename));
	COLLADASW::StreamWriter sw(native_filename);

	// open <Collada>
	sw.startDocument();

	// <asset>
	COLLADASW::Asset asset(&sw);
	// XXX ask blender devs about this?
	asset.setUnit("decimetre", 0.1);
	asset.setUpAxisType(COLLADASW::Asset::Z_UP);
	asset.add();
	
	// <library_cameras>
	CamerasExporter ce(&sw);
	ce.exportCameras(sce);
	
	// <library_lights>
	LightsExporter le(&sw);
	le.exportLights(sce);

	// <library_images>
	ImagesExporter ie(&sw, filename);
	ie.exportImages(sce);
	
	// <library_effects>
	EffectsExporter ee(&sw);
	ee.exportEffects(sce);
	
	// <library_materials>
	MaterialsExporter me(&sw);
	me.exportMaterials(sce);

	// <library_geometries>
	GeometryExporter ge(&sw);
	ge.exportGeom(sce);

	// <library_animations>
	AnimationExporter ae(&sw);
	ae.exportAnimations(sce);

	// <library_controllers>
	ArmatureExporter arm_exporter(&sw);
	arm_exporter.export_controllers(sce);

	// <library_visual_scenes>
	SceneExporter se(&sw, &arm_exporter);
	se.exportScene(sce);
	
	// <scene>
	std::string scene_name(translate_id(id_name(sce)));
	COLLADASW::Scene scene(&sw, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING,
											   scene_name));
	scene.add();
	
	// close <Collada>
	sw.endDocument();

}

void DocumentExporter::exportScenes(const char* filename)
{
}

/*

NOTES:

* AnimationExporter::sample_animation enables all curves on armature, this is undesirable for a user

 */

