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
#include "BLI_util.h"
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

static std::string id_name(void *id)
{
	return ((ID*)id)->name + 2;
}

static std::string get_geometry_id(Object *ob)
{
	return id_name(ob) + "-mesh";
}

static std::string get_light_id(Object *ob)
{
	return id_name(ob) + "-light";
}

static std::string get_camera_id(Object *ob)
{
	return id_name(ob) + "-camera";
}

static void replace_chars(char *str, char chars[], char with)
{
	char *ch, *p;

	for (ch = chars; *ch; ch++) {
		while ((p = strchr(str, *ch))) {
			*p = with;
		}
	}
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

#if 0		
		DerivedMesh *dm = mesh_get_derived_final(mScene, ob, CD_MASK_BAREMESH);
#endif
		Mesh *me = (Mesh*)ob->data;
		std::string geom_id = get_geometry_id(ob);
		
		// openMesh(geoId, geoName, meshId)
		openMesh(geom_id);
		
		// writes <source> for vertex coords
		createVertsSource(geom_id, me);
		
		// writes <source> for normal coords
		createNormalsSource(geom_id, me);

		int has_uvs = CustomData_has_layer(&me->fdata, CD_MTFACE);
		
		// writes <source> for uv coords if mesh has uv coords
		if (has_uvs) {
			createTexcoordsSource(geom_id, (Mesh*)ob->data);
		}
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
				createPolylist(ma != NULL, a, has_uvs, ob, geom_id);
			}
		}
		else {
			createPolylist(false, 0, has_uvs, ob, geom_id);
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
						Object *ob,
						std::string& geom_id)
	{
#if 0
		MFace *mfaces = dm->getFaceArray(dm);
		int totfaces = dm->getNumFaces(dm);
#endif
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
		if (has_material)
			polylist.setMaterial(id_name(ma));
				
		COLLADASW::InputList &til = polylist.getInputList();
			
		// creates <input> in <polylist> for vertices 
		COLLADASW::Input input1(COLLADASW::VERTEX, getUrlBySemantics
								(geom_id, COLLADASW::VERTEX), 0);
			
		// creates <input> in <polylist> for normals
		COLLADASW::Input input2(COLLADASW::NORMAL, getUrlBySemantics
								(geom_id, COLLADASW::NORMAL), 0);
			
		til.push_back(input1);
		til.push_back(input2);
			
		// if mesh has uv coords writes <input> for TEXCOORD
		int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);

		for (i = 0; i < num_layers; i++) {
			char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, i);
			COLLADASW::Input input3(COLLADASW::TEXCOORD,
									makeUrl(makeTexcoordSourceId(geom_id, i)),
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
			char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, a);
			
			COLLADASW::FloatSourceF source(mSW);
			std::string layer_id = makeTexcoordSourceId(geom_id, a);
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
	void createNormalsSource(std::string geom_id, Mesh *me)
	{
#if 0
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
#endif

		int totverts = me->totvert;
		MVert *verts = me->mvert;

		COLLADASW::FloatSourceF source(mSW);
		source.setId(getIdBySemantics(geom_id, COLLADASW::NORMAL));
		source.setArrayId(getIdBySemantics(geom_id, COLLADASW::NORMAL) +
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
		float loc[3], rot[3], size[3];
		float local[4][4];

		if (parent_mat) {
			float invpar[4][4];
			Mat4Invert(invpar, parent_mat);
			Mat4MulMat4(local, mat, invpar);
		}
		else {
			Mat4CpyMat4(local, mat);
		}

		TransformBase::decompose(local, loc, rot, size);
		
		/*
		// this code used to create a single <rotate> representing object rotation
		float quat[4];
		float axis[3];
		float angle;
		double angle_deg;
		EulToQuat(rot, quat);
		NormalQuat(quat);
		QuatToAxisAngle(quat, axis, &angle);
		angle_deg = angle * 180.0f / M_PI;
		node.addRotate(axis[0], axis[1], axis[2], angle_deg);
		*/
		node.addTranslate("location", loc[0], loc[1], loc[2]);

		node.addRotateZ("rotationZ", COLLADABU::Math::Utils::radToDegF(rot[2]));
		node.addRotateY("rotationY", COLLADABU::Math::Utils::radToDegF(rot[1]));
		node.addRotateX("rotationX", COLLADABU::Math::Utils::radToDegF(rot[0]));

		node.addScale("scale", size[0], size[1], size[2]);
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

		const std::string& controller_id = get_controller_id(ob_arm);

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

	std::string get_joint_id(Bone *bone, Object *ob_arm)
	{
		return id_name(ob_arm) + "_" + bone->name;
	}

	std::string get_joint_sid(Bone *bone)
	{
		char name[100];
		BLI_strncpy(name, bone->name, sizeof(name));

		// these chars have special meaning in SID
		replace_chars(name, ".()", '_');

		return name;
	}

	// parent_mat is armature-space
	void add_bone_node(Bone *bone, Object *ob_arm)
	{
		std::string node_id = get_joint_id(bone, ob_arm);
		std::string node_name = std::string(bone->name);
		std::string node_sid = get_joint_sid(bone);

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
		bPose *pose = ob_arm->pose;

		bPoseChannel *pchan = get_pose_channel(ob_arm->pose, bone->name);

		float mat[4][4];

		if (bone->parent) {
			// get bone-space matrix from armature-space
			bPoseChannel *parchan = get_pose_channel(ob_arm->pose, bone->parent->name);

			float invpar[4][4];
			Mat4Invert(invpar, parchan->pose_mat);
			Mat4MulMat4(mat, pchan->pose_mat, invpar);
		}
		else {
			// get world-space from armature-space
			Mat4MulMat4(mat, pchan->pose_mat, ob_arm->obmat);
		}

		TransformWriter::add_node_transform(node, mat, NULL);
	}

	std::string get_controller_id(Object *ob_arm)
	{
		return id_name(ob_arm) + SKIN_CONTROLLER_ID_SUFFIX;
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
		std::string controller_id = get_controller_id(ob_arm);

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

		int offset = 0;
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
				source.appendValues(get_joint_sid(bone));
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
				Mat4MulMat4(world, pchan->pose_mat, ob_arm->obmat);
				
				Mat4Invert(mat, world);
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
		openVisualScene(id_name(sce));

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
		node.setNodeId(id_name(ob));
		node.setType(COLLADASW::Node::NODE);

		node.start();

		bool is_skinned_mesh = arm_exporter->is_skinned_mesh(ob);

		float mat[4][4];
		
		if (ob->type == OB_MESH && is_skinned_mesh)
			// for skinned mesh we write obmat in <bind_shape_matrix>
			Mat4One(mat);
		else
			Mat4CpyMat4(mat, ob->obmat);

		TransformWriter::add_node_transform(node, mat, ob->parent ? ob->parent->obmat : NULL);
		
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
				char rel[FILE_MAX];
				char abs[FILE_MAX];
				char src[FILE_MAX];
				char dir[FILE_MAX];
				
				BLI_split_dirfile_basic(mfilename, dir, NULL);

				BKE_get_image_export_path(image, dir, abs, sizeof(abs), rel, sizeof(rel));

				if (strlen(abs)) {

					// make absolute source path
					BLI_strncpy(src, image->name, sizeof(src));
					BLI_convertstringcode(src, G.sce);

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

		openEffect(id_name(ma) + "-effect");
		
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
		// transparency
		ep.setTransparency(ma->alpha);
		// emission
		COLLADASW::ColorOrTexture cot = getcol(0.0f, 0.0f, 0.0f, 1.0f);
		ep.setEmission(cot);
		ep.setTransparent(cot);
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
			if (t->mapto & MAP_ALPHA) {
				ep.setTransparent(createTexture(ima, uvname, sampler));
			}
		}
		// performs the actual writing
		ep.addProfileElements();
		ep.closeProfile();
		closeEffect();	
	}
	
	COLLADASW::ColorOrTexture createTexture(Image *ima,
											std::string& uv_layer_name,
											COLLADASW::Sampler *sampler
											/*COLLADASW::Surface *surface*/)
	{
		
		COLLADASW::Texture texture(id_name(ima));
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
	std::map<bActionGroup*, std::vector<FCurve*> > fcurves_actionGroup_map;
	std::map<bActionGroup*, std::vector<FCurve*> > rotfcurves_actionGroup_map;
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
							   Sampler::Semantic semantic, bool rotation, const char *axis) {
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

	std::string create_source(Sampler::Semantic semantic, FCurve *fcu, std::string& anim_id, const char *axis_name)
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

	std::string create_interpolation_source(FCurve *fcu, std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(Sampler::INTERPOLATION);

		//bool is_rotation = !strcmp(fcu->rna_path, "rotation");

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

	std::string get_transform_sid(char *rna_path, const char *axis_name)
	{
		// if (!strcmp(rna_path, "rotation"))
// 			return std::string(rna_path) + axis_name;

// 		return std::string(rna_path) + "." + axis_name;
		std::string new_rna_path;
		
		if (strstr(rna_path, "rotation")) {
			new_rna_path = "rotation";
			return new_rna_path + axis_name;
		}
		else if (strstr(rna_path, "location")) {
			new_rna_path = strstr(rna_path, "location");
			return new_rna_path + "." + axis_name;
		}
		else if (strstr(rna_path, "scale")) {
			new_rna_path = strstr(rna_path, "scale");
			return new_rna_path + "." + axis_name;
		}
		return NULL;
	}

	void add_animation(FCurve *fcu, std::string ob_name)
	{
		const char *axis_names[] = {"X", "Y", "Z"};
		const char *axis_name = NULL;
		char c_anim_id[100]; // careful!
		
		if (fcu->array_index < 3)
			axis_name = axis_names[fcu->array_index];

		BLI_snprintf(c_anim_id, sizeof(c_anim_id), "%s.%s.%s", (char*)ob_name.c_str(), fcu->rna_path, axis_names[fcu->array_index]);
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

		std::string target = ob_name + "/" + get_transform_sid(fcu->rna_path, axis_name);
		addChannel(COLLADABU::URI(empty, sampler_id), target);

		closeAnimation();
	}
	
	void add_bone_animation(FCurve *fcu, std::string ob_name, std::string bone_name)
	{
		const char *axis_names[] = {"X", "Y", "Z"};
		const char *axis_name = NULL;
		char c_anim_id[100]; // careful!

		if (fcu->array_index < 3)
			axis_name = axis_names[fcu->array_index];
		
		std::string transform_sid = get_transform_sid(fcu->rna_path, axis_name);
		
		BLI_snprintf(c_anim_id, sizeof(c_anim_id), "%s.%s.%s", (char*)ob_name.c_str(), (char*)bone_name.c_str(), (char*)transform_sid.c_str());
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

		std::string target = ob_name + "_" + bone_name + "/" + transform_sid;
		addChannel(COLLADABU::URI(empty, sampler_id), target);

		closeAnimation();
	}
	
	FCurve *create_fcurve(int array_index, char *rna_path)
	{
		FCurve *fcu = (FCurve*)MEM_callocN(sizeof(FCurve), "FCurve");
		
		fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
		fcu->rna_path = BLI_strdupn(rna_path, strlen(rna_path));
		fcu->array_index = array_index;
		return fcu;
	}
	
	void create_bezt(FCurve *fcu, float frame, float output)
	{
		BezTriple bez;
		memset(&bez, 0, sizeof(BezTriple));
		bez.vec[1][0] = frame;
		bez.vec[1][1] = output;
		bez.ipo = U.ipo_new; /* use default interpolation mode here... */
		bez.f1 = bez.f2 = bez.f3 = SELECT;
		bez.h1 = bez.h2 = HD_AUTO;
		insert_bezt_fcurve(fcu, &bez, 0);
		calchandles_fcurve(fcu);
	}
	
	void change_quat_to_eul(Object *ob, bActionGroup *grp, char *grpname)
	{
		std::vector<FCurve*> &rot_fcurves = rotfcurves_actionGroup_map[grp];
		
		FCurve *quatcu[4] = {NULL, NULL, NULL, NULL};
		int i;
		
		for (i = 0; i < rot_fcurves.size(); i++)
			quatcu[rot_fcurves[i]->array_index] = rot_fcurves[i];
		
		char *rna_path = rot_fcurves[0]->rna_path;
		
		FCurve *eulcu[3] = {
			create_fcurve(0, rna_path),
			create_fcurve(1, rna_path),
			create_fcurve(2, rna_path)
		};
		
		for (i = 0; i < 4; i++) {
			
			FCurve *cu = quatcu[i];
			
			if (!cu) continue;
			
			for (int j = 0; j < cu->totvert; j++) {
				float frame = cu->bezt[j].vec[1][0];
				
				float quat[4] = {
					quatcu[0] ? evaluate_fcurve(quatcu[0], frame) : 0.0f,
					quatcu[1] ? evaluate_fcurve(quatcu[1], frame) : 0.0f,
					quatcu[2] ? evaluate_fcurve(quatcu[2], frame) : 0.0f,
					quatcu[3] ? evaluate_fcurve(quatcu[3], frame) : 0.0f
				};
				
				float eul[3];
				
				QuatToEul(quat, eul);
				
				for (int k = 0; k < 3; k++)
					create_bezt(eulcu[k], frame, eul[k]);
			}
		}
		
		for (i = 0; i < 3; i++) {
			add_bone_animation(eulcu[i], id_name(ob), std::string(grpname));
			free_fcurve(eulcu[i]);
		}
	}

	// called for each exported object
	void operator() (Object *ob) 
	{
		if (!ob->adt || !ob->adt->action) return;
		
		FCurve *fcu = (FCurve*)ob->adt->action->curves.first;
		
		if (ob->type == OB_ARMATURE) {
			
			while (fcu) {
				
				if (strstr(fcu->rna_path, ".rotation")) 
					rotfcurves_actionGroup_map[fcu->grp].push_back(fcu);
				else fcurves_actionGroup_map[fcu->grp].push_back(fcu);
				
				fcu = fcu->next;
			}
			
			for (bPoseChannel *pchan = (bPoseChannel*)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				int i;
				char *grpname = pchan->name;
				bActionGroup *grp = action_groups_find_named(ob->adt->action, grpname);
				
				if (!grp) continue;
				
				// write animation for location & scaling
				if (fcurves_actionGroup_map.find(grp) == fcurves_actionGroup_map.end()) continue;
				
				std::vector<FCurve*> &fcurves = fcurves_actionGroup_map[grp];
				for (i = 0; i < fcurves.size(); i++)
					add_bone_animation(fcurves[i], id_name(ob), std::string(grpname));
				
				// ... for rotation
				if (rotfcurves_actionGroup_map.find(grp) == rotfcurves_actionGroup_map.end())
					continue;
				
				// if rotation mode is euler - no need to convert it
				if (pchan->rotmode == ROT_MODE_EUL) {
					
					std::vector<FCurve*> &rotfcurves = rotfcurves_actionGroup_map[grp];
					
					for (i = 0; i < rotfcurves.size(); i++) 
						add_bone_animation(rotfcurves[i], id_name(ob), std::string(grpname));
				}
				
				// convert rotation to euler & write animation
				else change_quat_to_eul(ob, grp, grpname);
			}
		}
		else {
			while (fcu) {
				
				if (!strcmp(fcu->rna_path, "location") ||
					!strcmp(fcu->rna_path, "scale") ||
					!strcmp(fcu->rna_path, "rotation_euler")) {
					
					add_animation(fcu, id_name(ob));
				}
				
				fcu = fcu->next;
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
