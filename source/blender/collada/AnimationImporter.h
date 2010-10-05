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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef __BC_ANIMATIONIMPORTER_H__
#define __BC_ANIMATIONIMPORTER_H__

#include <map>
#include <vector>

#include "COLLADAFWAnimation.h"
#include "COLLADAFWAnimationCurve.h"
#include "COLLADAFWAnimationList.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWUniqueId.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

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
public:

	AnimationImporter(UnitConverter *conv, ArmatureImporter *arm, Scene *scene);

	~AnimationImporter();

	bool write_animation(const COLLADAFW::Animation* anim);
	
	// called on post-process stage after writeVisualScenes
	bool write_animation_list(const COLLADAFW::AnimationList* animlist);

	void read_node_transform(COLLADAFW::Node *node, Object *ob);
#if 0
	virtual void change_eul_to_quat(Object *ob, bAction *act);
#endif

	// prerequisites:
	// animlist_map - map animlist id -> animlist
	// curve_map - map anim id -> curve(s)
	Object *translate_animation(COLLADAFW::Node *node,
								std::map<COLLADAFW::UniqueId, Object*>& object_map,
								std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& root_map,
								COLLADAFW::Transformation::TransformationType tm_type,
								Object *par_job = NULL);

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
};
 
 #endif
