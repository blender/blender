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
#include "BLI_util.h"
}
#include "BKE_scene.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_action.h" // pose functions

#include "BLI_arithb.h"
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
#include "COLLADASWSurface.h"
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
#include "COLLADASWBaseInputElement.h"

#include "collada_internal.h"
#include "DocumentExporter.h"

#include <vector>
#include <algorithm> // std::find

// TODO: this can handy in BLI_arith.b
// This function assumes that quat is normalized.
// The following document was used as reference:
// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToAngle/index.htm


void QuatToAxisAngle(float *q, float *axis, float *angle)
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

std::string id_name(void *id)
{
	return ((ID*)id)->name + 2;
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
			f(ob);
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

			if (find(mMat.begin(), mMat.end(), id_name(ma)) == mMat.end()) {
				(*this->f)(ma, ob);

				mMat.push_back(id_name(ma));
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
		
		DerivedMesh *dm = mesh_get_derived_final(mScene, ob, CD_MASK_BAREMESH);
		Mesh *me = (Mesh*)ob->data;
		std::string geom_name(id_name(ob));
		
		// openMesh(geoId, geoName, meshId)
		openMesh(geom_name, "", "");
		
		// writes <source> for vertex coords
		createVertsSource(geom_name, dm);
		
		// writes <source> for normal coords
		createNormalsSource(geom_name, dm);

		int has_uvs = CustomData_has_layer(&me->fdata, CD_MTFACE);
		
		// writes <source> for uv coords if mesh has uv coords
		if (has_uvs) {
			createTexcoordsSource(geom_name, dm, (Mesh*)ob->data);
		}
		// <vertices>
		COLLADASW::Vertices verts(mSW);
		verts.setId(getIdBySemantics(geom_name, COLLADASW::VERTEX));
		COLLADASW::InputList &input_list = verts.getInputList();
		COLLADASW::Input input(COLLADASW::POSITION,
							   getUrlBySemantics(geom_name, COLLADASW::POSITION));
		input_list.push_back(input);
		verts.add();

		// XXX slow		
		if (ob->totcol) {
			for(int a = 0; a < ob->totcol; a++)	{
				// account for NULL materials, this should not normally happen?
				Material *ma = give_current_material(ob, a + 1);
				createPolylist(ma != NULL, a, has_uvs, ob, dm, geom_name);
			}
		}
		else {
			createPolylist(false, 0, has_uvs, ob, dm, geom_name);
		}
		
		closeMesh();
		closeGeometry();
		
		dm->release(dm);
		
	}

	// powerful because it handles both cases when there is material and when there's not
	void createPolylist(bool has_material,
						int material_index,
						bool has_uvs,
						Object *ob,
						DerivedMesh *dm,
						std::string& geom_name)
	{
		MFace *mfaces = dm->getFaceArray(dm);
		int totfaces = dm->getNumFaces(dm);
		Mesh *me = (Mesh*)ob->data;

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
		if (has_material)
			polylist.setMaterial(id_name(ma));
				
		COLLADASW::InputList &til = polylist.getInputList();
			
		// creates <input> in <polylist> for vertices 
		COLLADASW::Input input1(COLLADASW::VERTEX, getUrlBySemantics
								(geom_name, COLLADASW::VERTEX), 0);
			
		// creates <input> in <polylist> for normals
		COLLADASW::Input input2(COLLADASW::NORMAL, getUrlBySemantics
								(geom_name, COLLADASW::NORMAL), 0);
			
		til.push_back(input1);
		til.push_back(input2);
			
		// if mesh has uv coords writes <input> for TEXCOORD
		int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);

		for (i = 0; i < num_layers; i++) {
			char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, i);
			COLLADASW::Input input3(COLLADASW::TEXCOORD,
									makeUrl(makeTexcoordSourceId(geom_name, i)),
									1, // offset always 1, this is only until we have optimized UV sets
									i  // set number equals UV layer index
									);
			til.push_back(input3);
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
				for (int j = 0; j < (f->v4 == 0 ? 3 : 4); j++) {
					polylist.appendValues(v[j]);

					if (has_uvs)
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
	void createVertsSource(std::string geom_name, DerivedMesh *dm)
	{
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
		
		
		COLLADASW::FloatSourceF source(mSW);
		source.setId(getIdBySemantics(geom_name, COLLADASW::POSITION));
		source.setArrayId(getIdBySemantics(geom_name, COLLADASW::POSITION) +
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

	std::string makeTexcoordSourceId(std::string& geom_name, int layer_index)
	{
		char suffix[20];
		sprintf(suffix, "-%d", layer_index);
		return getIdBySemantics(geom_name, COLLADASW::TEXCOORD) + suffix;
	}

	//creates <source> for texcoords
	void createTexcoordsSource(std::string geom_name, DerivedMesh *dm, Mesh *me)
	{

		int totfaces = dm->getNumFaces(dm);
		MFace *mfaces = dm->getFaceArray(dm);
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
			char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, a);
			
			COLLADASW::FloatSourceF source(mSW);
			std::string layer_id = makeTexcoordSourceId(geom_name, a);
			source.setId(layer_id);
			source.setArrayId(layer_id + ARRAY_ID_SUFFIX);
			
			source.setAccessorCount(totuv);
			source.setAccessorStride(2);
			COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
			param.push_back("X");
			param.push_back("Y");
			
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
	void createNormalsSource(std::string geom_name, DerivedMesh *dm)
	{
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
		
		COLLADASW::FloatSourceF source(mSW);
		source.setId(getIdBySemantics(geom_name, COLLADASW::NORMAL));
		source.setArrayId(getIdBySemantics(geom_name, COLLADASW::NORMAL) +
						  ARRAY_ID_SUFFIX);
		source.setAccessorCount(totverts);
		source.setAccessorStride(3);
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("X");
		param.push_back("Y");
		param.push_back("Z");
		
		source.prepareToAppendValues();
		
		int i = 0;
		
		for( i = 0; i < totverts; ++i ){
			
			source.appendValues(float(verts[i].no[0]/32767.0),
								float(verts[i].no[1]/32767.0),
								float(verts[i].no[2]/32767.0));
				
		}
		source.finish();
	}
	
	std::string getIdBySemantics(std::string geom_name, COLLADASW::Semantics type, std::string other_suffix = "") {
		return geom_name + getSuffixBySemantic(type) + other_suffix;
	}
	
	
	COLLADASW::URI getUrlBySemantics(std::string geom_name, COLLADASW::Semantics type, std::string other_suffix = "") {
		
		std::string id(getIdBySemantics(geom_name, type, other_suffix));
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

// XXX exporter assumes armatures are not shared between meshes.
class ArmatureExporter: COLLADASW::LibraryControllers
{
public:
	ArmatureExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryControllers(sw) {}

	void export_armatures(Scene *sce)
	{
		openLibrary();

		forEachMeshObjectInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Object *ob)
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

		if (ob_arm)
			export_armature(ob, ob_arm);
	}

private:

	UnitConverter converter;

	// ob should be of type OB_MESH
	// both args are required
	void export_armature(Object* ob, Object *ob_arm)
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

		std::string controller_name(ob_arm->id.name);
		std::string controller_id = controller_name + SKIN_CONTROLLER_ID_SUFFIX;

		openSkin(controller_id, controller_name, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, ob->id.name));

		add_bind_shape_mat(ob);

		std::string joints_source_id = add_joints_source(&ob->defbase, controller_id);
		std::string inv_bind_mat_source_id =
			add_inv_bind_mats_source((bArmature*)ob_arm->data, &ob->defbase, controller_id);
		std::string weights_source_id = add_weights_source(me, controller_id);

		add_joints_element(&ob->defbase, joints_source_id, inv_bind_mat_source_id);
		add_vertex_weights_element(weights_source_id, joints_source_id, me);

		closeSkin();
		closeController();
	}

	void add_joints_element(ListBase *defbase,
							const std::string& joints_source_id, const std::string& inv_bind_mat_source_id)
	{
		COLLADASW::JointsElement joints(mSW);
		COLLADASW::InputList &input = joints.getInputList();

		int offset = 0;
		input.push_back(COLLADASW::Input(COLLADASW::JOINT, // constant declared in COLLADASWInputList.h
										 COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, joints_source_id)));
        input.push_back(COLLADASW::Input(COLLADASW::BINDMATRIX,
										 COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, inv_bind_mat_source_id)));
		joints.add();
	}

	void add_bind_shape_mat(Object *ob)
	{
		float ob_bind_mat[4][4];
		double dae_mat[4][4];

		// TODO: get matrix from ob
		Mat4One(ob_bind_mat);

		converter.mat4_to_dae(dae_mat, ob_bind_mat);

		addBindShapeTransform(dae_mat);
	}

	std::string add_joints_source(ListBase *defbase, const std::string& controller_id)
	{
		std::string source_id = controller_id + JOINTS_SOURCE_ID_SUFFIX;

		COLLADASW::NameSource source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(BLI_countlist(defbase));
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("JOINT");

		source.prepareToAppendValues();

		bDeformGroup *def;

		for (def = (bDeformGroup*)defbase->first; def; def = def->next) {
			source.appendValues(def->name);
		}

		source.finish();

		return source_id;
	}

	std::string add_inv_bind_mats_source(bArmature *arm, ListBase *defbase, const std::string& controller_id)
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

		bDeformGroup *def;

		/*
		Bone *get_named_bone (struct bArmature *arm, const char *name);
		bPoseChannel *get_pose_channel(const struct bPose *pose, const char *name);
		*/

		float inv_bind_mat[4][4];
		Mat4One(inv_bind_mat);

		float dae_mat[4][4];
		converter.mat4_to_dae(dae_mat, inv_bind_mat);

		// TODO: write inverse bind matrices for each bone (name taken from defbase)
		for (def = (bDeformGroup*)defbase->first; def; def = def->next) {
			source.appendValues(dae_mat);
		}

		source.finish();

		return source_id;
	}

	std::string add_weights_source(Mesh *me, const std::string& controller_id)
	{
		std::string source_id = controller_id + WEIGHTS_SOURCE_ID_SUFFIX;

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(me->totvert);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("WEIGHT");

		source.prepareToAppendValues();

		// NOTE: COLLADA spec says weights should be normalized

		for (int i = 0; i < me->totvert; i++) {
			MDeformVert *vert = &me->dvert[i];
			for (int j = 0; j < vert->totweight; j++) {
				source.appendValues(vert->dw[j].weight);
			}
		}

		source.finish();

		return source_id;
	}

	void add_vertex_weights_element(const std::string& weights_source_id, const std::string& joints_source_id, Mesh *me)
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

		std::vector<unsigned long> indices;

		// write deformer index - weight index pairs
		int weight_index = 0;
		for (i = 0; i < me->totvert; i++) {
			MDeformVert *dvert = &me->dvert[i];

			for (int j = 0; j < dvert->totweight; j++) {
				indices.push_back(dvert->dw[j].def_nr);
				indices.push_back(weight_index++);
			}
		}
		
		weights.CloseVCountAndOpenVElement();
		weights.appendValues(indices);

		weights.finish();
	}
};

class SceneExporter: COLLADASW::LibraryVisualScenes
{
public:
	SceneExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryVisualScenes(sw) {}
	
	void exportScene(Scene *sce) {
 		// <library_visual_scenes> <visual_scene>
		openVisualScene(id_name(sce), "");

		// write <node>s
		//forEachMeshObjectInScene(sce, *this);
		//forEachCameraObjectInScene(sce, *this);
		//forEachLampObjectInScene(sce, *this);
		exportHierarchy(sce);

		// </visual_scene> </library_visual_scenes>
		closeVisualScene();

		closeLibrary();
	}

	// called for each object
	//void operator()(Object *ob) {
	void writeNodes(Object *ob, Scene *sce) {
		
		COLLADASW::Node node(mSW);
		node.setNodeId(ob->id.name);
		node.setType(COLLADASW::Node::NODE);

		std::string ob_name(id_name(ob));

		node.start();
		node.addTranslate("location", ob->loc[0], ob->loc[1], ob->loc[2]);
		
		// this code used to create a single <rotate> representing object rotation
		// float quat[4];
		// float axis[3];
		// float angle;
		// double angle_deg;
		// EulToQuat(ob->rot, quat);
		// NormalQuat(quat);
		// QuatToAxisAngle(quat, axis, &angle);
		// angle_deg = angle * 180.0f / M_PI;
		// node.addRotate(axis[0], axis[1], axis[2], angle_deg);

		float *rot = ob->rot;
		node.addRotateX("rotationX", COLLADABU::Math::Utils::radToDegF(rot[0]));
		node.addRotateY("rotationY", COLLADABU::Math::Utils::radToDegF(rot[1]));
		node.addRotateZ("rotationZ", COLLADABU::Math::Utils::radToDegF(rot[2]));

		node.addScale("scale", ob->size[0], ob->size[1], ob->size[2]);
		
		// <instance_geometry>
		if (ob->type == OB_MESH) {
			COLLADASW::InstanceGeometry instGeom(mSW);
			instGeom.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, ob_name));
			
			for(int a = 0; a < ob->totcol; a++)	{
				Material *ma = give_current_material(ob, a+1);
				
				COLLADASW::BindMaterial& bm = instGeom.getBindMaterial();
				COLLADASW::InstanceMaterialList& iml = bm.getInstanceMaterialList();

				if (ma) {
					std::string matid(id_name(ma));
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
			
			instGeom.add();
		}
		
		// <instance_camera>
		else if (ob->type == OB_CAMERA) {
			COLLADASW::InstanceCamera instCam(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, ob_name));
			instCam.add();
		}
		
		// <instance_light>
		else if (ob->type == OB_LAMP) {
			COLLADASW::InstanceLight instLa(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, ob_name));
			instLa.add();
		}
		// empty object
		else if (ob->type == OB_EMPTY) {
		}
		
		// write node for child object
		Base *b = (Base*) sce->base.first;
		while(b) {
			
			Object *cob = b->object;
			
			if ((cob->type == OB_MESH || cob->type == OB_CAMERA || cob->type == OB_LAMP || cob->type == OB_EMPTY) && cob->parent == ob) {
				// write node...
				writeNodes(cob, sce);
			}
			b = b->next;
		}
		
		node.end();
	}

	void exportHierarchy(Scene *sce)
	{
		Base *base= (Base*) sce->base.first;
		while(base) {
			Object *ob = base->object;
			
			if ((ob->type == OB_MESH || ob->type == OB_CAMERA || ob->type == OB_LAMP || ob->type == OB_EMPTY) && !ob->parent) {
				// write nodes....
				writeNodes(ob, sce);
				
			}
			base= base->next;
		}
	}

};

class ImagesExporter: COLLADASW::LibraryImages
{
	std::vector<std::string> mImages; // contains list of written images, to avoid duplicates
public:
	ImagesExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryImages(sw)
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
				char *ima_name;
				BLI_split_dirfile_basic(image->name, NULL, ima_name);
				
				if (find(mImages.begin(), mImages.end(), name) == mImages.end()) {
					COLLADASW::Image img(COLLADABU::URI(COLLADABU::URI::nativePathToUri(ima_name)), name, "");
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

		openEffect(id_name(ma) + "-effect");
		
		COLLADASW::EffectProfile ep(mSW);
		ep.setProfileType(COLLADASW::EffectProfile::COMMON);
		ep.openProfile();
		// set shader type - one of three blinn, phong or lambert
		if (ma->spec_shader == MA_SPEC_BLINN) {
			ep.setShaderType(COLLADASW::EffectProfile::BLINN);
		}
		else if (ma->spec_shader == MA_SPEC_PHONG) {
			ep.setShaderType(COLLADASW::EffectProfile::PHONG);
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
		// transparency
		ep.setTransparency(ma->alpha);
		// shininess
		ep.setShininess(ma->spec);
		// emission
		COLLADASW::ColorOrTexture cot = getcol(0.0f, 0.0f, 0.0f, 1.0f);
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
			cot = getcol(0.0f, 0.0f, 0.0f, 1.0f);
			ep.setReflective(cot);
			ep.setReflectivity(0.0f);
		}
		// specular
		if (ep.getShaderType() != COLLADASW::EffectProfile::LAMBERT) {
			cot = getcol(ma->specr, ma->specg, ma->specb, 1.0f);
			ep.setSpecular(cot);
		}

		// XXX make this more readable if possible

		// create <sampler> and <surface> for each image
		COLLADASW::Sampler samplers[MAX_MTEX];
		COLLADASW::Surface surfaces[MAX_MTEX];
		void *samp_surf[MAX_MTEX][2];

		// image to index to samp_surf map
		// samp_surf[index] stores 2 pointers, sampler and surface
		std::map<std::string, int> im_samp_map;

		unsigned int a, b;
		for (a = 0, b = 0; a < tex_indices.size(); a++) {
			MTex *t = ma->mtex[tex_indices[a]];
			Image *ima = t->tex->ima;

			std::string key(id_name(ima));

			// create only one <sampler>/<surface> pair for each unique image
			if (im_samp_map.find(key) == im_samp_map.end()) {
				//<newparam> <surface> <init_from>
				COLLADASW::Surface surface(COLLADASW::Surface::SURFACE_TYPE_2D,
										   key + COLLADASW::Surface::SURFACE_SID_SUFFIX);
				COLLADASW::SurfaceInitOption sio(COLLADASW::SurfaceInitOption::INIT_FROM);
				sio.setImageReference(key);
				surface.setInitOption(sio);

				//<newparam> <sampler> <source>
				COLLADASW::Sampler sampler(COLLADASW::Sampler::SAMPLER_TYPE_2D,
										   key + COLLADASW::Surface::SURFACE_SID_SUFFIX);

				// copy values to arrays since they will live longer
				samplers[a] = sampler;
				surfaces[a] = surface;

				// store pointers so they can be used later when we create <texture>s
				samp_surf[b][0] = &samplers[a];
				samp_surf[b][1] = &surfaces[a];
				
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

			// we assume map input is always TEXTCO_UV

			std::string key(id_name(ima));
			int i = im_samp_map[key];
			COLLADASW::Sampler *sampler = (COLLADASW::Sampler*)samp_surf[i][0];
			COLLADASW::Surface *surface = (COLLADASW::Surface*)samp_surf[i][1];

			std::string uvname = strlen(t->uvname) ? t->uvname : active_uv;

			// color
			if (t->mapto & MAP_COL) {
				ep.setDiffuse(createTexture(ima, uvname, sampler, surface));
			}
			// ambient
			if (t->mapto & MAP_AMB) {
				ep.setAmbient(createTexture(ima, uvname, sampler, surface));
			}
			// specular
			if (t->mapto & MAP_SPEC) {
				ep.setSpecular(createTexture(ima, uvname, sampler, surface));
			}
			// emission
			if (t->mapto & MAP_EMIT) {
				ep.setEmission(createTexture(ima, uvname, sampler, surface));
			}
			// reflective
			if (t->mapto & MAP_REF) {
				ep.setReflective(createTexture(ima, uvname, sampler, surface));
			}
		}
		// performs the actual writing
		ep.addProfileElements();
		ep.closeProfile();
		closeEffect();	
	}
	
	COLLADASW::ColorOrTexture createTexture(Image *ima,
											std::string& uv_layer_name,
											COLLADASW::Sampler *sampler,
											COLLADASW::Surface *surface)
	{
		
		COLLADASW::Texture texture(id_name(ima));
		texture.setTexcoord(uv_layer_name);
		texture.setSurface(*surface);
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
			if (ma->mtex[a] && ma->mtex[a]->tex->type == TEX_IMAGE){
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

		openMaterial(name);

		std::string efid = name + "-effect";
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
	void operator()(Object *ob)
	{
		// XXX add other params later
		Camera *cam = (Camera*)ob->data;
		std::string cam_name(id_name(ob));
		if (cam->type == CAM_PERSP) {
			COLLADASW::PerspectiveOptic persp(mSW);
			persp.setXFov(1.0);
			//persp.setYFov(1.0);
			persp.setAspectRatio(1.0);
			persp.setZFar(cam->clipend);
			persp.setZNear(cam->clipsta);
			COLLADASW::Camera ccam(mSW, &persp, cam_name);
			addCamera(ccam);
		}
		else {
			COLLADASW::OrthographicOptic ortho(mSW);
			ortho.setXMag(1.0);
			//ortho.setYMag(1.0, true);
			ortho.setAspectRatio(1.0);
			ortho.setZFar(cam->clipend);
			ortho.setZNear(cam->clipsta);
			COLLADASW::Camera ccam(mSW, &ortho, cam_name);
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
		std::string la_name(id_name(ob));
		COLLADASW::Color col(la->r, la->g, la->b);
		
		// sun
		if (la->type == LA_SUN) {
			COLLADASW::DirectionalLight cla(mSW, la_name, la->energy);
			cla.setColor(col);
			addLight(cla);
		}
		// hemi
		else if (la->type == LA_HEMI) {
			COLLADASW::AmbientLight cla(mSW, la_name, la->energy);
			cla.setColor(col);
			addLight(cla);
		}
		// spot
		// XXX add other params later
		else if (la->type == LA_SPOT) {
			COLLADASW::SpotLight cla(mSW, la_name, la->energy);
			cla.setColor(col);
			addLight(cla);
		}
		// lamp
		else if (la->type != LA_AREA) {
			COLLADASW::PointLight cla(mSW, la_name, la->energy);
			cla.setColor(col);
			addLight(cla);
		}
		else {
			// XXX write error
			return;
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

	// create <animation> for each transform axis

	float convert_time(float frame) {
		return FRA2TIME(frame);
	}

	float convert_angle(float angle) {
		return COLLADABU::Math::Utils::radToDegF(angle);
	}

	std::string get_semantic_suffix(Sampler::Semantic semantic) {
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
		}
		return "";
	}

	void add_source_parameters(COLLADASW::SourceBase::ParameterNameList& param,
							   Sampler::Semantic semantic, bool rotation, char *axis) {
		switch(semantic) {
		case Sampler::INPUT:
			param.push_back("TIME");
			break;
		case Sampler::OUTPUT:
			if (rotation) {
				param.push_back("ANGLE");
			}
			else {
				param.push_back(axis);
			}
			break;
		case Sampler::IN_TANGENT:
		case Sampler::OUT_TANGENT:
			param.push_back("X");
			param.push_back("Y");
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
		}
	}

	std::string create_source(Sampler::Semantic semantic, FCurve *fcu, std::string& anim_id, char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		bool is_rotation = !strcmp(fcu->rna_path, "rotation");

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(fcu->totvert);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rotation, axis_name);

		source.prepareToAppendValues();

		for (int i = 0; i < fcu->totvert; i++) {
			float values[3]; // be careful!
			int length;

			get_source_values(&fcu->bezt[i], semantic, is_rotation, values, &length);
			for (int j = 0; j < length; j++)
				source.appendValues(values[j]);
		}

		source.finish();

		return source_id;
	}

	std::string create_interpolation_source(FCurve *fcu, std::string& anim_id, char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(Sampler::INTERPOLATION);

		bool is_rotation = !strcmp(fcu->rna_path, "rotation");

		COLLADASW::NameSource source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(fcu->totvert);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("INTERPOLATION");

		source.prepareToAppendValues();

		for (int i = 0; i < fcu->totvert; i++) {
			// XXX
			source.appendValues(LINEAR_NAME);
		}

		source.finish();

		return source_id;
	}

	std::string get_transform_sid(char *rna_path, char *axis_name)
	{
		if (!strcmp(rna_path, "rotation"))
			return std::string(rna_path) + axis_name;

		return std::string(rna_path) + "." + axis_name;
	}

	void add_animation(FCurve *fcu, const char *ob_name)
	{
		static char *axis_names[] = {"X", "Y", "Z"};
		char *axis_name = NULL;
		char c_anim_id[100]; // careful!

		if (fcu->array_index < 3)
			axis_name = axis_names[fcu->array_index];

		BLI_snprintf(c_anim_id, sizeof(c_anim_id), "%s.%s.%s", ob_name, fcu->rna_path, axis_names[fcu->array_index]);
		std::string anim_id(c_anim_id);

		// check rna_path is one of: rotation, scale, location

		openAnimation(anim_id);

		// create input source
		std::string input_id = create_source(Sampler::INPUT, fcu, anim_id, axis_name);

		// create output source
		std::string output_id = create_source(Sampler::OUTPUT, fcu, anim_id, axis_name);

		// create interpolations source
		std::string interpolation_id = create_interpolation_source(fcu, anim_id, axis_name);

		std::string sampler_id = anim_id + SAMPLER_ID_SUFFIX;
		COLLADASW::LibraryAnimations::Sampler sampler(sampler_id);
		std::string empty;
		sampler.addInput(Sampler::INPUT, COLLADABU::URI(empty, input_id));
		sampler.addInput(Sampler::OUTPUT, COLLADABU::URI(empty, output_id));

		// this input is required
		sampler.addInput(Sampler::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

		addSampler(sampler);

		std::string target = std::string(ob_name) + "/" + get_transform_sid(fcu->rna_path, axis_name);
		addChannel(COLLADABU::URI(empty, sampler_id), target);

		closeAnimation();
	}

	// called for each exported object
	void operator() (Object *ob) 
	{
		if (!ob->adt || !ob->adt->action) return;

		// XXX this needs to be handled differently?
		if (ob->type == OB_ARMATURE) return;

		FCurve *fcu = (FCurve*)ob->adt->action->curves.first;
		while (fcu) {

			if (!strcmp(fcu->rna_path, "location") ||
				!strcmp(fcu->rna_path, "scale") ||
				!strcmp(fcu->rna_path, "rotation")) {

				add_animation(fcu, ob->id.name);
			}

			fcu = fcu->next;
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
	asset.setUnit("meter", 1.0);
	asset.setUpAxisType(COLLADASW::Asset::Z_UP);
	asset.add();
	
	// <library_cameras>
	CamerasExporter ce(&sw);
	ce.exportCameras(sce);
	
	// <library_lights>
	LightsExporter le(&sw);
	le.exportLights(sce);
	
	// <library_images>
	ImagesExporter ie(&sw);
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
	ArmatureExporter(&sw).export_armatures(sce);

	// <library_visual_scenes>
	SceneExporter se(&sw);
	se.exportScene(sce);
	
	// <scene>
	std::string scene_name(id_name(sce));
	COLLADASW::Scene scene(&sw, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING,
											   scene_name));
	scene.add();
	
	// close <Collada>
	sw.endDocument();

}

void DocumentExporter::exportScenes(const char* filename)
{
}
