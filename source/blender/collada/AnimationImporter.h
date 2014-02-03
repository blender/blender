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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory, Sukhitha Jayathilake.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
/** \file AnimationImporter.h
 *  \ingroup collada
 */

#ifndef __ANIMATIONIMPORTER_H__
#define __ANIMATIONIMPORTER_H__

#include <map>
#include <vector>

#include "COLLADAFWAnimation.h"
#include "COLLADAFWAnimationCurve.h"
#include "COLLADAFWAnimationList.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWUniqueId.h"
#include "COLLADAFWLight.h"
#include "COLLADAFWCamera.h"
#include "COLLADAFWMaterial.h"
#include "COLLADAFWEffect.h"
#include "COLLADAFWInstanceGeometry.h"

extern "C" {
#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_lamp_types.h"
#include "DNA_camera_types.h"
}

//#include "ArmatureImporter.h"
#include "TransformReader.h"

#include "collada_internal.h"

class ArmatureImporter;

class AnimationImporterBase
{
public:
	// virtual void change_eul_to_quat(Object *ob, bAction *act) = 0;
};

class AnimationImporter : private TransformReader, public AnimationImporterBase
{
private:

	ArmatureImporter *armature_importer;
	Scene *scene;

	std::map<COLLADAFW::UniqueId, std::vector<FCurve*> > curve_map;
	std::map<COLLADAFW::UniqueId, TransformReader::Animation> uid_animated_map;
	// std::map<bActionGroup*, std::vector<FCurve*> > fcurves_actionGroup_map;
	std::map<COLLADAFW::UniqueId, const COLLADAFW::AnimationList*> animlist_map;
	std::vector<FCurve*> unused_curves;
	std::map<COLLADAFW::UniqueId, Object*> joint_objects;
	
	FCurve *create_fcurve(int array_index, const char *rna_path);
	
	void create_bezt(FCurve *fcu, float frame, float output);

	// create one or several fcurves depending on the number of parameters being animated
	void animation_to_fcurves(COLLADAFW::AnimationCurve *curve);

	void fcurve_deg_to_rad(FCurve *cu);

	void add_fcurves_to_object(Object *ob, std::vector<FCurve*>& curves, char *rna_path, int array_index, Animation *animated);
	
	int typeFlag;

	std::string import_from_version;

	enum lightAnim
	{
//		INANIMATE = 0,
		LIGHT_COLOR	= 2,
		LIGHT_FOA = 4,
		LIGHT_FOE = 8
	};

	enum cameraAnim
	{
//		INANIMATE = 0,
		CAMERA_XFOV = 2,
		CAMERA_XMAG = 4,
		CAMERA_YFOV = 8,
		CAMERA_YMAG = 16,
		CAMERA_ZFAR = 32,
		CAMERA_ZNEAR = 64
	};

	enum matAnim
	{
		MATERIAL_SHININESS = 2,
		MATERIAL_SPEC_COLOR = 4,
		MATERIAL_DIFF_COLOR = 1 << 3,
		MATERIAL_TRANSPARENCY = 1 << 4,
		MATERIAL_IOR = 1 << 5
	};
	
	enum AnimationType
		{
			INANIMATE = 0,
			NODE_TRANSFORM = 1,
		};

	struct AnimMix
	{
		int transform;
		int light;
		int camera;
		int material;
		int texture;
	};
public:

	AnimationImporter(UnitConverter *conv, ArmatureImporter *arm, Scene *scene);

	~AnimationImporter();

	void set_import_from_version(std::string import_from_version);
	bool write_animation(const COLLADAFW::Animation* anim);
	
	// called on post-process stage after writeVisualScenes
	bool write_animation_list(const COLLADAFW::AnimationList* animlist);

	void read_node_transform(COLLADAFW::Node *node, Object *ob);
#if 0
	virtual void change_eul_to_quat(Object *ob, bAction *act);
#endif

	void translate_Animations(COLLADAFW::Node * Node,
	                          std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& root_map,
	                          std::multimap<COLLADAFW::UniqueId, Object*>& object_map,
	                          std::map<COLLADAFW::UniqueId, const COLLADAFW::Object*> FW_object_map);

	AnimMix* get_animation_type( const COLLADAFW::Node * node, std::map<COLLADAFW::UniqueId, const COLLADAFW::Object*> FW_object_map );

	void apply_matrix_curves(Object *ob, std::vector<FCurve*>& animcurves, COLLADAFW::Node* root, COLLADAFW::Node* node,
	                         COLLADAFW::Transformation * tm );
	
	void add_bone_animation_sampled(Object *ob, std::vector<FCurve*>& animcurves, COLLADAFW::Node* root, COLLADAFW::Node* node, COLLADAFW::Transformation * tm);

	void Assign_transform_animations(COLLADAFW::Transformation* transform,
	                                 const COLLADAFW::AnimationList::AnimationBinding *binding,
	                                 std::vector<FCurve*>* curves, bool is_joint, char *joint_path);

	void Assign_color_animations(const COLLADAFW::UniqueId& listid, ListBase *AnimCurves, const char * anim_type);
	void Assign_float_animations(const COLLADAFW::UniqueId& listid, ListBase *AnimCurves, const char * anim_type);
	void Assign_lens_animations(const COLLADAFW::UniqueId& listid, ListBase *AnimCurves, const double aspect, Camera *cam, const char *anim_type, int fov_type);

	int setAnimType ( const COLLADAFW::Animatable * prop, int type, int addition);
	
	void modify_fcurve(std::vector<FCurve*>* curves, const char *rna_path, int array_index );
	void unused_fcurve(std::vector<FCurve*>* curves );
	// prerequisites:
	// animlist_map - map animlist id -> animlist
	// curve_map - map anim id -> curve(s)
	Object * translate_animation_OLD(COLLADAFW::Node *node,
	                                 std::map<COLLADAFW::UniqueId, Object*>& object_map,
	                                 std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& root_map,
	                                 COLLADAFW::Transformation::TransformationType tm_type,
	                                 Object *par_job = NULL);
	
	void find_frames( std::vector<float>* frames, std::vector<FCurve*>* curves );
	void find_frames_old( std::vector<float>* frames, COLLADAFW::Node * node, COLLADAFW::Transformation::TransformationType tm_type );
	// internal, better make it private
	// warning: evaluates only rotation
	// prerequisites: animlist_map, curve_map
	void evaluate_transform_at_frame(float mat[4][4], COLLADAFW::Node *node, float fra);

	// return true to indicate that mat contains a sane value
	bool evaluate_animation(COLLADAFW::Transformation *tm, float mat[4][4], float fra, const char *node_id);

	// gives a world-space mat of joint at rest position
	void get_joint_rest_mat(float mat[4][4], COLLADAFW::Node *root, COLLADAFW::Node *node);

	// gives a world-space mat, end's mat not included
	bool calc_joint_parent_mat_rest(float mat[4][4], float par[4][4], COLLADAFW::Node *node, COLLADAFW::Node *end);

#ifdef ARMATURE_TEST
	Object *get_joint_object(COLLADAFW::Node *root, COLLADAFW::Node *node, Object *par_job);
#endif

#if 0
	// recursively evaluates joint tree until end is found, mat then is world-space matrix of end
	// mat must be identity on enter, node must be root
	bool evaluate_joint_world_transform_at_frame(float mat[4][4], float par[4][4], COLLADAFW::Node *node, COLLADAFW::Node *end, float fra);
#endif

	void add_bone_fcurve(Object *ob, COLLADAFW::Node *node, FCurve *fcu);

	void add_bezt(FCurve *fcu, float fra, float value);

	void extra_data_importer(std::string elementName);
};

#endif
