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

/** \file blender/collada/AnimationImporter.cpp
 *  \ingroup collada
 */

#include <stddef.h>

/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "DNA_armature_types.h"

#include "ED_keyframing.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_fcurve.h"
#include "BKE_object.h"

#include "MEM_guardedalloc.h"

#include "collada_utils.h"
#include "AnimationImporter.h"
#include "ArmatureImporter.h"
#include "MaterialExporter.h"

#include <algorithm>

// first try node name, if not available (since is optional), fall back to original id
template<class T>
static const char *bc_get_joint_name(T *node)
{
	const std::string& id = node->getName();
	return id.size() ? id.c_str() : node->getOriginalId().c_str();
}

FCurve *AnimationImporter::create_fcurve(int array_index, const char *rna_path)
{
	FCurve *fcu = (FCurve*)MEM_callocN(sizeof(FCurve), "FCurve");
	fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
	fcu->rna_path = BLI_strdupn(rna_path, strlen(rna_path));
	fcu->array_index = array_index;
	return fcu;
}
	
void AnimationImporter::create_bezt(FCurve *fcu, float frame, float output)
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

// create one or several fcurves depending on the number of parameters being animated
void AnimationImporter::animation_to_fcurves(COLLADAFW::AnimationCurve *curve)
{
	COLLADAFW::FloatOrDoubleArray& input = curve->getInputValues();
	COLLADAFW::FloatOrDoubleArray& output = curve->getOutputValues();

	float fps = (float)FPS;
	size_t dim = curve->getOutDimension();
	unsigned int i;

	std::vector<FCurve*>& fcurves = curve_map[curve->getUniqueId()];

	switch (dim) {
	case 1: // X, Y, Z or angle
	case 3: // XYZ
	case 4:
	case 16: // matrix
		{
			for (i = 0; i < dim; i++ ) {
				FCurve *fcu = (FCurve*)MEM_callocN(sizeof(FCurve), "FCurve");

				fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
				// fcu->rna_path = BLI_strdupn(path, strlen(path));
				fcu->array_index = 0;
				fcu->totvert = curve->getKeyCount();

				// create beztriple for each key
				for (unsigned int j = 0; j < curve->getKeyCount(); j++) {
					BezTriple bez;
					memset(&bez, 0, sizeof(BezTriple));


					// input, output
					bez.vec[1][0] = bc_get_float_value(input, j) * fps; 
					bez.vec[1][1] = bc_get_float_value(output, j * dim + i);


					if ( curve->getInterpolationType() == COLLADAFW::AnimationCurve::INTERPOLATION_BEZIER ||
						curve->getInterpolationType() == COLLADAFW::AnimationCurve::INTERPOLATION_STEP) 
					{
						COLLADAFW::FloatOrDoubleArray& intan = curve->getInTangentValues();
						COLLADAFW::FloatOrDoubleArray& outtan = curve->getOutTangentValues();

						// intangent
						bez.vec[0][0] = bc_get_float_value(intan, (j * 2 * dim ) + (2 * i)) * fps;
						bez.vec[0][1] = bc_get_float_value(intan, (j * 2 * dim )+ (2 * i) + 1);

						// outtangent
						bez.vec[2][0] = bc_get_float_value(outtan, (j * 2 * dim ) + (2 * i)) * fps;
						bez.vec[2][1] = bc_get_float_value(outtan, (j * 2 * dim )+ (2 * i) + 1);
						if (curve->getInterpolationType() == COLLADAFW::AnimationCurve::INTERPOLATION_BEZIER) 
							bez.ipo = BEZT_IPO_BEZ;
						else 
							bez.ipo = BEZT_IPO_CONST;
						//bez.h1 = bez.h2 = HD_AUTO; 	
					}
					else {
						bez.h1 = bez.h2 = HD_AUTO; 
						bez.ipo = BEZT_IPO_LIN;
					}
					// bez.ipo = U.ipo_new; /* use default interpolation mode here... */
					bez.f1 = bez.f2 = bez.f3 = SELECT;

					insert_bezt_fcurve(fcu, &bez, 0);
				}

				calchandles_fcurve(fcu);

				fcurves.push_back(fcu);
			}
		}
		break;
	default:
		fprintf(stderr, "Output dimension of %d is not yet supported (animation id = %s)\n", (int)dim, curve->getOriginalId().c_str());
	}

	for (std::vector<FCurve*>::iterator it = fcurves.begin(); it != fcurves.end(); it++)
		unused_curves.push_back(*it);
}


void AnimationImporter::fcurve_deg_to_rad(FCurve *cu)
{
	for (unsigned int i = 0; i < cu->totvert; i++) {
		// TODO convert handles too
		cu->bezt[i].vec[1][1] *= DEG2RADF(1.0f);
		cu->bezt[i].vec[0][1] *= DEG2RADF(1.0f);
		cu->bezt[i].vec[2][1] *= DEG2RADF(1.0f);
	}
}


void AnimationImporter::add_fcurves_to_object(Object *ob, std::vector<FCurve*>& curves, char *rna_path, int array_index, Animation *animated)
{
	bAction *act;
	
	if (!ob->adt || !ob->adt->action) act = verify_adt_action((ID*)&ob->id, 1);
	else act = ob->adt->action;
	
	std::vector<FCurve*>::iterator it;
	int i;

#if 0
	char *p = strstr(rna_path, "rotation_euler");
	bool is_rotation = p && *(p + strlen("rotation_euler")) == '\0';

	// convert degrees to radians for rotation
	if (is_rotation)
		fcurve_deg_to_rad(fcu);
#endif
	
	for (it = curves.begin(), i = 0; it != curves.end(); it++, i++) {
		FCurve *fcu = *it;
		fcu->rna_path = BLI_strdupn(rna_path, strlen(rna_path));
		
		if (array_index == -1) fcu->array_index = i;
		else fcu->array_index = array_index;
	
		if (ob->type == OB_ARMATURE) {
			bActionGroup *grp = NULL;
			const char *bone_name = bc_get_joint_name(animated->node);
			
			if (bone_name) {
				/* try to find group */
				grp = action_groups_find_named(act, bone_name);
				
				/* no matching groups, so add one */
				if (grp == NULL) {
					/* Add a new group, and make it active */
					grp = (bActionGroup*)MEM_callocN(sizeof(bActionGroup), "bActionGroup");
					
					grp->flag = AGRP_SELECTED;
					BLI_strncpy(grp->name, bone_name, sizeof(grp->name));
					
					BLI_addtail(&act->groups, grp);
					BLI_uniquename(&act->groups, grp, "Group", '.', offsetof(bActionGroup, name), 64);
				}
				
				/* add F-Curve to group */
				action_groups_add_channel(act, grp, fcu);
				
			}
#if 0
			if (is_rotation) {
				fcurves_actionGroup_map[grp].push_back(fcu);
			}
#endif
		}
		else {
			BLI_addtail(&act->curves, fcu);
		}

		// curve is used, so remove it from unused_curves
		unused_curves.erase(std::remove(unused_curves.begin(), unused_curves.end(), fcu), unused_curves.end());
	}
}

AnimationImporter::AnimationImporter(UnitConverter *conv, ArmatureImporter *arm, Scene *scene) :
		TransformReader(conv), armature_importer(arm), scene(scene) { }

AnimationImporter::~AnimationImporter()
{
	// free unused FCurves
	for (std::vector<FCurve*>::iterator it = unused_curves.begin(); it != unused_curves.end(); it++)
		free_fcurve(*it);

	if (unused_curves.size())
		fprintf(stderr, "removed %d unused curves\n", (int)unused_curves.size());
}

bool AnimationImporter::write_animation(const COLLADAFW::Animation* anim) 
{
	if (anim->getAnimationType() == COLLADAFW::Animation::ANIMATION_CURVE) {
		COLLADAFW::AnimationCurve *curve = (COLLADAFW::AnimationCurve*)anim;
		
		// XXX Don't know if it's necessary
		// Should we check outPhysicalDimension?
		if (curve->getInPhysicalDimension() != COLLADAFW::PHYSICAL_DIMENSION_TIME) {
			fprintf(stderr, "Inputs physical dimension is not time.\n");
			return true;
		}

		// a curve can have mixed interpolation type,
		// in this case curve->getInterpolationTypes returns a list of interpolation types per key
		COLLADAFW::AnimationCurve::InterpolationType interp = curve->getInterpolationType();

		if (interp != COLLADAFW::AnimationCurve::INTERPOLATION_MIXED) {
			switch (interp) {
			case COLLADAFW::AnimationCurve::INTERPOLATION_LINEAR:
			case COLLADAFW::AnimationCurve::INTERPOLATION_BEZIER:
			case COLLADAFW::AnimationCurve::INTERPOLATION_STEP:
				animation_to_fcurves(curve);
				break;
			default:
				// TODO there're also CARDINAL, HERMITE, BSPLINE and STEP types
				fprintf(stderr, "CARDINAL, HERMITE and BSPLINE anim interpolation types not supported yet.\n");
				break;
			}
		}
		else {
			// not supported yet
			fprintf(stderr, "MIXED anim interpolation type is not supported yet.\n");
		}
	}
	else {
		fprintf(stderr, "FORMULA animation type is not supported yet.\n");
	}
	
	return true;
}
	
// called on post-process stage after writeVisualScenes
bool AnimationImporter::write_animation_list(const COLLADAFW::AnimationList* animlist) 
{
	const COLLADAFW::UniqueId& animlist_id = animlist->getUniqueId();

	animlist_map[animlist_id] = animlist;

#if 0

	// should not happen
	if (uid_animated_map.find(animlist_id) == uid_animated_map.end()) {
		return true;
	}

	// for bones rna_path is like: pose.bones["bone-name"].rotation


#endif

	return true;
}

// \todo refactor read_node_transform to not automatically apply anything,
// but rather return the transform matrix, so caller can do with it what is
// necessary. Same for \ref get_node_mat
void AnimationImporter::read_node_transform(COLLADAFW::Node *node, Object *ob)
{
	float mat[4][4];
	TransformReader::get_node_mat(mat, node, &uid_animated_map, ob);
	if (ob) {
		copy_m4_m4(ob->obmat, mat);
		object_apply_mat4(ob, ob->obmat, 0, 0);
	}
}

#if 0
virtual void AnimationImporter::change_eul_to_quat(Object *ob, bAction *act)
{
	bActionGroup *grp;
	int i;
	
	for (grp = (bActionGroup*)act->groups.first; grp; grp = grp->next) {

		FCurve *eulcu[3] = {NULL, NULL, NULL};
		
		if (fcurves_actionGroup_map.find(grp) == fcurves_actionGroup_map.end())
			continue;

		std::vector<FCurve*> &rot_fcurves = fcurves_actionGroup_map[grp];
		
		if (rot_fcurves.size() > 3) continue;

		for (i = 0; i < rot_fcurves.size(); i++)
			eulcu[rot_fcurves[i]->array_index] = rot_fcurves[i];

		char joint_path[100];
		char rna_path[100];

		BLI_snprintf(joint_path, sizeof(joint_path), "pose.bones[\"%s\"]", grp->name);
		BLI_snprintf(rna_path, sizeof(rna_path), "%s.rotation_quaternion", joint_path);

		FCurve *quatcu[4] = {
			create_fcurve(0, rna_path),
			create_fcurve(1, rna_path),
			create_fcurve(2, rna_path),
			create_fcurve(3, rna_path)
		};

		bPoseChannel *chan = get_pose_channel(ob->pose, grp->name);

		float m4[4][4], irest[3][3];
		invert_m4_m4(m4, chan->bone->arm_mat);
		copy_m3_m4(irest, m4);

		for (i = 0; i < 3; i++) {

			FCurve *cu = eulcu[i];

			if (!cu) continue;

			for (int j = 0; j < cu->totvert; j++) {
				float frame = cu->bezt[j].vec[1][0];

				float eul[3] = {
					eulcu[0] ? evaluate_fcurve(eulcu[0], frame) : 0.0f,
					eulcu[1] ? evaluate_fcurve(eulcu[1], frame) : 0.0f,
					eulcu[2] ? evaluate_fcurve(eulcu[2], frame) : 0.0f
				};

				// make eul relative to bone rest pose
				float rot[3][3], rel[3][3], quat[4];

				/*eul_to_mat3(rot, eul);

				mul_m3_m3m3(rel, irest, rot);

				mat3_to_quat(quat, rel);
				*/

				eul_to_quat(quat, eul);

				for (int k = 0; k < 4; k++)
					create_bezt(quatcu[k], frame, quat[k]);
			}
		}

		// now replace old Euler curves

		for (i = 0; i < 3; i++) {
			if (!eulcu[i]) continue;

			action_groups_remove_channel(act, eulcu[i]);
			free_fcurve(eulcu[i]);
		}

		chan->rotmode = ROT_MODE_QUAT;

		for (i = 0; i < 4; i++)
			action_groups_add_channel(act, grp, quatcu[i]);
	}

	bPoseChannel *pchan;
	for (pchan = (bPoseChannel*)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		pchan->rotmode = ROT_MODE_QUAT;
	}
}
#endif


//sets the rna_path and array index to curve
void AnimationImporter::modify_fcurve(std::vector<FCurve*>* curves, const char* rna_path, int array_index )
{
	std::vector<FCurve*>::iterator it;
	int i;
	for (it = curves->begin(), i = 0; it != curves->end(); it++, i++) {
		FCurve *fcu = *it;
		fcu->rna_path = BLI_strdup(rna_path);
		
		if (array_index == -1) fcu->array_index = i;
		else fcu->array_index = array_index;

		unused_curves.erase(std::remove(unused_curves.begin(), unused_curves.end(), fcu), unused_curves.end());
	}
}

void AnimationImporter::find_frames( std::vector<float>* frames, std::vector<FCurve*>* curves)
{
	std::vector<FCurve*>::iterator iter;
	for (iter = curves->begin(); iter != curves->end(); iter++) {
		FCurve *fcu = *iter;

		for (unsigned int k = 0; k < fcu->totvert; k++) {
			//get frame value from bezTriple
			float fra = fcu->bezt[k].vec[1][0];
			//if frame already not added add frame to frames
			if (std::find(frames->begin(), frames->end(), fra) == frames->end())
				frames->push_back(fra);

		}
	}
}

//creates the rna_paths and array indices of fcurves from animations using transformation and bound animation class of each animation.
void AnimationImporter:: Assign_transform_animations(COLLADAFW::Transformation * transform,
													 const COLLADAFW::AnimationList::AnimationBinding * binding,
													 std::vector<FCurve*>* curves, bool is_joint, char * joint_path)
{
	COLLADAFW::Transformation::TransformationType tm_type = transform->getTransformationType();
	bool is_matrix = tm_type == COLLADAFW::Transformation::MATRIX;
	bool is_rotation = tm_type  == COLLADAFW::Transformation::ROTATE;

	//to check if the no of curves are valid
	bool xyz = ((tm_type == COLLADAFW::Transformation::TRANSLATE ||tm_type  == COLLADAFW::Transformation::SCALE) && binding->animationClass == COLLADAFW::AnimationList::POSITION_XYZ);


	if (!((!xyz && curves->size() == 1) || (xyz && curves->size() == 3) || is_matrix)) {
		fprintf(stderr, "expected %d curves, got %d\n", xyz ? 3 : 1, (int)curves->size());
		return;
	}

	char rna_path[100];

	switch (tm_type) {
		case COLLADAFW::Transformation::TRANSLATE:
		case COLLADAFW::Transformation::SCALE:
			{
				bool loc = tm_type == COLLADAFW::Transformation::TRANSLATE;
				if (is_joint)
					BLI_snprintf(rna_path, sizeof(rna_path), "%s.%s", joint_path, loc ? "location" : "scale");
				else
					BLI_strncpy(rna_path, loc ? "location" : "scale", sizeof(rna_path));

				switch (binding->animationClass) {
		case COLLADAFW::AnimationList::POSITION_X:
			modify_fcurve(curves, rna_path, 0 );
			break;
		case COLLADAFW::AnimationList::POSITION_Y:
			modify_fcurve(curves, rna_path, 1 );
			break;
		case COLLADAFW::AnimationList::POSITION_Z:
			modify_fcurve(curves, rna_path, 2 );
			break;
		case COLLADAFW::AnimationList::POSITION_XYZ:
			modify_fcurve(curves, rna_path, -1 );
			break;
		default:
			fprintf(stderr, "AnimationClass %d is not supported for %s.\n",
				binding->animationClass, loc ? "TRANSLATE" : "SCALE");
				}
				break;
			}


		case COLLADAFW::Transformation::ROTATE:
			{
				if (is_joint)
					BLI_snprintf(rna_path, sizeof(rna_path), "%s.rotation_euler", joint_path);
				else
					BLI_strncpy(rna_path, "rotation_euler", sizeof(rna_path));
				std::vector<FCurve*>::iterator iter;
				for (iter = curves->begin(); iter != curves->end(); iter++) {
					FCurve* fcu = *iter;

					//if transform is rotation the fcurves values must be turned in to radian.
					if (is_rotation)
						fcurve_deg_to_rad(fcu);		 
				}					
				COLLADAFW::Rotate* rot = (COLLADAFW::Rotate*)transform;
				COLLADABU::Math::Vector3& axis = rot->getRotationAxis();

				switch (binding->animationClass) {
		case COLLADAFW::AnimationList::ANGLE:
			if (COLLADABU::Math::Vector3::UNIT_X == axis) {
				modify_fcurve(curves, rna_path, 0 );
			}
			else if (COLLADABU::Math::Vector3::UNIT_Y == axis) {
				modify_fcurve(curves, rna_path, 1 );
			}
			else if (COLLADABU::Math::Vector3::UNIT_Z == axis) {
				modify_fcurve(curves, rna_path, 2 );
			}
			break;
		case COLLADAFW::AnimationList::AXISANGLE:
			// TODO convert axis-angle to quat? or XYZ?
		default:
			fprintf(stderr, "AnimationClass %d is not supported for ROTATE transformation.\n",
				binding->animationClass);
				}
				break;
			}

		case COLLADAFW::Transformation::MATRIX:
			/*{
			COLLADAFW::Matrix* mat = (COLLADAFW::Matrix*)transform;
			COLLADABU::Math::Matrix4 mat4 = mat->getMatrix();
			switch (binding->animationClass) {
			case COLLADAFW::AnimationList::TRANSFORM:

			}
			}*/
			break;
		case COLLADAFW::Transformation::SKEW:
		case COLLADAFW::Transformation::LOOKAT:
			fprintf(stderr, "Animation of SKEW and LOOKAT transformations is not supported yet.\n");
			break;
	}

}

//creates the rna_paths and array indices of fcurves from animations using color and bound animation class of each animation.
void AnimationImporter:: Assign_color_animations(const COLLADAFW::UniqueId& listid, ListBase *AnimCurves, const char * anim_type)
{
	char rna_path[100];
	BLI_strncpy(rna_path, anim_type, sizeof(rna_path));

	const COLLADAFW::AnimationList *animlist = animlist_map[listid];
	const COLLADAFW::AnimationList::AnimationBindings& bindings = animlist->getAnimationBindings();
	//all the curves belonging to the current binding
	std::vector<FCurve*> animcurves;
	for (unsigned int j = 0; j < bindings.getCount(); j++) {
		animcurves = curve_map[bindings[j].animation];

		switch (bindings[j].animationClass) {
		case COLLADAFW::AnimationList::COLOR_R:
			modify_fcurve(&animcurves, rna_path, 0 );
			break;
		case COLLADAFW::AnimationList::COLOR_G:
			modify_fcurve(&animcurves, rna_path, 1 );
			break;
		case COLLADAFW::AnimationList::COLOR_B:
			modify_fcurve(&animcurves, rna_path, 2 );
			break;
		case COLLADAFW::AnimationList::COLOR_RGB:
		case COLLADAFW::AnimationList::COLOR_RGBA: // to do-> set intensity
			modify_fcurve(&animcurves, rna_path, -1 );
			break;

		default:
			fprintf(stderr, "AnimationClass %d is not supported for %s.\n",
				bindings[j].animationClass, "COLOR" );
		}

		std::vector<FCurve*>::iterator iter;
		//Add the curves of the current animation to the object
		for (iter = animcurves.begin(); iter != animcurves.end(); iter++) {
			FCurve * fcu = *iter;
			BLI_addtail(AnimCurves, fcu);	
		}
	}


}

void AnimationImporter:: Assign_float_animations(const COLLADAFW::UniqueId& listid, ListBase *AnimCurves, const char * anim_type)
{
	char rna_path[100];
	if (animlist_map.find(listid) == animlist_map.end()) {
		return;
	}
	else {
		//anim_type has animations
		const COLLADAFW::AnimationList *animlist = animlist_map[listid];
		const COLLADAFW::AnimationList::AnimationBindings& bindings = animlist->getAnimationBindings();
		//all the curves belonging to the current binding
		std::vector<FCurve*> animcurves;
		for (unsigned int j = 0; j < bindings.getCount(); j++) {
			animcurves = curve_map[bindings[j].animation];

			BLI_strncpy(rna_path, anim_type, sizeof(rna_path));
			modify_fcurve(&animcurves, rna_path, 0 );
			std::vector<FCurve*>::iterator iter;
			//Add the curves of the current animation to the object
			for (iter = animcurves.begin(); iter != animcurves.end(); iter++) {
				FCurve * fcu = *iter;
				BLI_addtail(AnimCurves, fcu);
			}
		}
	}
	
}

void AnimationImporter::apply_matrix_curves( Object * ob, std::vector<FCurve*>& animcurves, COLLADAFW::Node* root, COLLADAFW::Node* node,
													COLLADAFW::Transformation * tm )
{
	bool is_joint = node->getType() == COLLADAFW::Node::JOINT;
	const char *bone_name = is_joint ? bc_get_joint_name(node) : NULL;
	char joint_path[200];
	if ( is_joint ) 
		armature_importer->get_rna_path_for_joint(node, joint_path, sizeof(joint_path));

	std::vector<float> frames;
	find_frames(&frames, &animcurves);

	float irest_dae[4][4];
	float rest[4][4], irest[4][4];

	if (is_joint) {
		get_joint_rest_mat(irest_dae, root, node);
		invert_m4(irest_dae);

		Bone *bone = get_named_bone((bArmature*)ob->data, bone_name);
		if (!bone) {
			fprintf(stderr, "cannot find bone \"%s\"\n", bone_name);
			return;
		}

		unit_m4(rest);
		copy_m4_m4(rest, bone->arm_mat);
		invert_m4_m4(irest, rest);
	}
	// new curves to assign matrix transform animation
	FCurve *newcu[10]; // if tm_type is matrix, then create 10 curves: 4 rot, 3 loc, 3 scale
	unsigned int totcu = 10;
	const char *tm_str = NULL;
	char rna_path[200];
	for (int i = 0; i < totcu; i++) {

		int axis = i;

		if (i < 4) {
			tm_str = "rotation_quaternion";
			axis = i;
		}
		else if (i < 7) {
			tm_str = "location";
			axis = i - 4;
		}
		else {
			tm_str = "scale";
			axis = i - 7;
		}


		if (is_joint)
			BLI_snprintf(rna_path, sizeof(rna_path), "%s.%s", joint_path, tm_str);
		else
			BLI_strncpy(rna_path, tm_str, sizeof(rna_path));
		newcu[i] = create_fcurve(axis, rna_path);
		newcu[i]->totvert = frames.size();
	}

	if (frames.size() == 0)
		return;

	std::sort(frames.begin(), frames.end());

	std::vector<float>::iterator it;

	// sample values at each frame
	for (it = frames.begin(); it != frames.end(); it++) {
		float fra = *it;

		float mat[4][4];
		float matfra[4][4];

		unit_m4(matfra);

		// calc object-space mat
		evaluate_transform_at_frame(matfra, node, fra);


		// for joints, we need a special matrix
		if (is_joint) {
			// special matrix: iR * M * iR_dae * R
			// where R, iR are bone rest and inverse rest mats in world space (Blender bones),
			// iR_dae is joint inverse rest matrix (DAE) and M is an evaluated joint world-space matrix (DAE)
			float temp[4][4], par[4][4];

			// calc M
			calc_joint_parent_mat_rest(par, NULL, root, node);
			mult_m4_m4m4(temp, par, matfra);

			// evaluate_joint_world_transform_at_frame(temp, NULL, node, fra);

			// calc special matrix
			mul_serie_m4(mat, irest, temp, irest_dae, rest, NULL, NULL, NULL, NULL);
		}
		else {
			copy_m4_m4(mat, matfra);
		}

		float  rot[4], loc[3], scale[3];

		mat4_to_quat(rot, mat);
		/*for ( int i = 0 ; i < 4  ;  i ++ )
		{
		rot[i] = RAD2DEGF(rot[i]);
		}*/
		copy_v3_v3(loc, mat[3]);
		mat4_to_size(scale, mat);

		// add keys
		for (int i = 0; i < totcu; i++) {
			if (i < 4)
				add_bezt(newcu[i], fra, rot[i]);
			else if (i < 7)
				add_bezt(newcu[i], fra, loc[i - 4]);
			else
				add_bezt(newcu[i], fra, scale[i - 7]);
		}
	}
	verify_adt_action((ID*)&ob->id, 1);

	ListBase *curves = &ob->adt->action->curves;

	// add curves
	for (int i= 0; i < totcu; i++) {
		if (is_joint)
			add_bone_fcurve(ob, node, newcu[i]);
		else
			BLI_addtail(curves, newcu[i]);
	}

	if (is_joint) {
		bPoseChannel *chan = get_pose_channel(ob->pose, bone_name);
		chan->rotmode = ROT_MODE_QUAT;
	}
	else {
		ob->rotmode = ROT_MODE_QUAT;
	}

	return;

}

void AnimationImporter::translate_Animations ( COLLADAFW::Node * node,
												   std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& root_map,
												   std::map<COLLADAFW::UniqueId, Object*>& object_map,
												   std::map<COLLADAFW::UniqueId, const COLLADAFW::Object*> FW_object_map)
{
	AnimationImporter::AnimMix* animType = get_animation_type(node, FW_object_map );

	bool is_joint = node->getType() == COLLADAFW::Node::JOINT;
	COLLADAFW::Node *root = root_map.find(node->getUniqueId()) == root_map.end() ? node : root_map[node->getUniqueId()];
	Object *ob = is_joint ? armature_importer->get_armature_for_joint(root) : object_map[node->getUniqueId()];
	if (!ob) {
		fprintf(stderr, "cannot find Object for Node with id=\"%s\"\n", node->getOriginalId().c_str());
		return;
	}

	bAction * act;

	if ( (animType->transform) != 0 ) {
		/* const char *bone_name = is_joint ? bc_get_joint_name(node) : NULL; */ /* UNUSED */
		char joint_path[200];

		if ( is_joint ) 
			armature_importer->get_rna_path_for_joint(node, joint_path, sizeof(joint_path));


		if (!ob->adt || !ob->adt->action) act = verify_adt_action((ID*)&ob->id, 1);
		else act = ob->adt->action;

		//Get the list of animation curves of the object
		ListBase *AnimCurves = &(act->curves);

		const COLLADAFW::TransformationPointerArray& nodeTransforms = node->getTransformations();

		//for each transformation in node 
		for (unsigned int i = 0; i < nodeTransforms.getCount(); i++) {
			COLLADAFW::Transformation *transform = nodeTransforms[i];
			COLLADAFW::Transformation::TransformationType tm_type = transform->getTransformationType();

			bool is_rotation = tm_type == COLLADAFW::Transformation::ROTATE;
			bool is_matrix = tm_type == COLLADAFW::Transformation::MATRIX;

			const COLLADAFW::UniqueId& listid = transform->getAnimationList();

			//check if transformation has animations
			if (animlist_map.find(listid) == animlist_map.end()) {
				continue;
			}
			else {
				//transformation has animations
				const COLLADAFW::AnimationList *animlist = animlist_map[listid];
				const COLLADAFW::AnimationList::AnimationBindings& bindings = animlist->getAnimationBindings();
				//all the curves belonging to the current binding
				std::vector<FCurve*> animcurves;
				for (unsigned int j = 0; j < bindings.getCount(); j++) {
					animcurves = curve_map[bindings[j].animation];
					if ( is_matrix ) {
						apply_matrix_curves(ob, animcurves, root, node,  transform  );
					}
					else {				

						if (is_joint) {

							add_bone_animation_sampled(ob, animcurves, root, node, transform);
						}
						else {
							//calculate rnapaths and array index of fcurves according to transformation and animation class
							Assign_transform_animations(transform, &bindings[j], &animcurves, is_joint, joint_path ); 

							std::vector<FCurve*>::iterator iter;
							//Add the curves of the current animation to the object
							for (iter = animcurves.begin(); iter != animcurves.end(); iter++) {
								FCurve * fcu = *iter;
							
								BLI_addtail(AnimCurves, fcu);
							}
						}
						
					}
				}
			}
			if (is_rotation && !is_joint) {
				ob->rotmode = ROT_MODE_EUL;
			}
		}
	}

	if ((animType->light) != 0) {
		Lamp * lamp  = (Lamp*) ob->data;

		if (!lamp->adt || !lamp->adt->action) act = verify_adt_action((ID*)&lamp->id, 1);
		else act = lamp->adt->action;

		ListBase *AnimCurves = &(act->curves);
		const COLLADAFW::InstanceLightPointerArray& nodeLights = node->getInstanceLights();

		for (unsigned int i = 0; i < nodeLights.getCount(); i++) {
			const COLLADAFW::Light *light = (COLLADAFW::Light *) FW_object_map[nodeLights[i]->getInstanciatedObjectId()];

			if ((animType->light & LIGHT_COLOR) != 0) {
				const COLLADAFW::Color *col =  &(light->getColor());
				const COLLADAFW::UniqueId& listid = col->getAnimationList();

				Assign_color_animations(listid, AnimCurves, "color"); 
			}
			if ((animType->light & LIGHT_FOA) != 0 ) {
				const COLLADAFW::AnimatableFloat *foa =  &(light->getFallOffAngle());
				const COLLADAFW::UniqueId& listid = foa->getAnimationList();

				Assign_float_animations( listid, AnimCurves, "spot_size");
			}
			if ( (animType->light & LIGHT_FOE) != 0 ) {
				const COLLADAFW::AnimatableFloat *foe =  &(light->getFallOffExponent());
				const COLLADAFW::UniqueId& listid = foe->getAnimationList();

				Assign_float_animations( listid, AnimCurves, "spot_blend");

			}
		}
	}

	if ( (animType->camera) != 0)  {
		Camera * camera  = (Camera*) ob->data;

		if (!camera->adt || !camera->adt->action) act = verify_adt_action((ID*)&camera->id, 1);
		else act = camera->adt->action;

		ListBase *AnimCurves = &(act->curves);
		const COLLADAFW::InstanceCameraPointerArray& nodeCameras= node->getInstanceCameras();

		for (unsigned int i = 0; i < nodeCameras.getCount(); i++) {
			const COLLADAFW::Camera *camera = (COLLADAFW::Camera *) FW_object_map[nodeCameras[i]->getInstanciatedObjectId()];

			if ((animType->camera & CAMERA_XFOV) != 0 ) {
				const COLLADAFW::AnimatableFloat *xfov =  &(camera->getXFov());
				const COLLADAFW::UniqueId& listid = xfov->getAnimationList();
				Assign_float_animations( listid, AnimCurves, "lens");
			}

			else if ((animType->camera & CAMERA_XMAG) != 0 ) {
				const COLLADAFW::AnimatableFloat *xmag =  &(camera->getXMag());
				const COLLADAFW::UniqueId& listid = xmag->getAnimationList();
				Assign_float_animations( listid, AnimCurves, "ortho_scale");
			}

			if ((animType->camera & CAMERA_ZFAR) != 0 ) {
				const COLLADAFW::AnimatableFloat *zfar =  &(camera->getFarClippingPlane());
				const COLLADAFW::UniqueId& listid = zfar->getAnimationList();
				Assign_float_animations( listid, AnimCurves, "clip_end");
			}

			if ((animType->camera & CAMERA_ZNEAR) != 0 ) {
				const COLLADAFW::AnimatableFloat *znear =  &(camera->getNearClippingPlane());
				const COLLADAFW::UniqueId& listid = znear->getAnimationList();
				Assign_float_animations( listid, AnimCurves, "clip_start");
			}

		}
	}
	if ( animType->material != 0) {
		Material *ma = give_current_material(ob, 1);
		if (!ma->adt || !ma->adt->action) act = verify_adt_action((ID*)&ma->id, 1);
		else act = ma->adt->action;

		ListBase *AnimCurves = &(act->curves);

		const COLLADAFW::InstanceGeometryPointerArray& nodeGeoms = node->getInstanceGeometries();
		for (unsigned int i = 0; i < nodeGeoms.getCount(); i++) {
			const COLLADAFW::MaterialBindingArray& matBinds = nodeGeoms[i]->getMaterialBindings();
			for (unsigned int j = 0; j < matBinds.getCount(); j++) {
				const COLLADAFW::UniqueId & matuid = matBinds[j].getReferencedMaterial();
				const COLLADAFW::Effect *ef = (COLLADAFW::Effect *) (FW_object_map[matuid]);
				if (ef != NULL) { /* can be NULL [#28909] */
					const COLLADAFW::CommonEffectPointerArray& commonEffects  =  ef->getCommonEffects();
					COLLADAFW::EffectCommon *efc = commonEffects[0];
					if ((animType->material & MATERIAL_SHININESS) != 0) {
						const COLLADAFW::FloatOrParam *shin = &(efc->getShininess());
						const COLLADAFW::UniqueId& listid =  shin->getAnimationList();
						Assign_float_animations( listid, AnimCurves, "specular_hardness" );
					}

					if ((animType->material & MATERIAL_IOR) != 0) {
						const COLLADAFW::FloatOrParam *ior = &(efc->getIndexOfRefraction());
						const COLLADAFW::UniqueId& listid =  ior->getAnimationList();
						Assign_float_animations( listid, AnimCurves, "raytrace_transparency.ior" );
					}

					if ((animType->material & MATERIAL_SPEC_COLOR) != 0) {
						const COLLADAFW::ColorOrTexture *cot = &(efc->getSpecular());
						const COLLADAFW::UniqueId& listid =  cot->getColor().getAnimationList();
						Assign_color_animations( listid, AnimCurves, "specular_color" );
					}

					if ((animType->material & MATERIAL_DIFF_COLOR) != 0) {
						const COLLADAFW::ColorOrTexture *cot = &(efc->getDiffuse());
						const COLLADAFW::UniqueId& listid =  cot->getColor().getAnimationList();
						Assign_color_animations( listid, AnimCurves, "diffuse_color" );
					}
				}
			}
		}	
	}
}

void AnimationImporter::add_bone_animation_sampled(Object * ob, std::vector<FCurve*>& animcurves, COLLADAFW::Node* root, COLLADAFW::Node* node, COLLADAFW::Transformation * tm)
{
	const char *bone_name = bc_get_joint_name(node);
	char joint_path[200];
	armature_importer->get_rna_path_for_joint(node, joint_path, sizeof(joint_path));

	std::vector<float> frames;
	find_frames(&frames, &animcurves);

	// convert degrees to radians
	if (tm->getTransformationType() == COLLADAFW::Transformation::ROTATE) {

		std::vector<FCurve*>::iterator iter;
		for (iter = animcurves.begin(); iter != animcurves.end(); iter++) {
			FCurve* fcu = *iter;

			fcurve_deg_to_rad(fcu);		 
		}					
	}


	float irest_dae[4][4];
	float rest[4][4], irest[4][4];

	get_joint_rest_mat(irest_dae, root, node);
	invert_m4(irest_dae);

	Bone *bone = get_named_bone((bArmature*)ob->data, bone_name);
	if (!bone) {
		fprintf(stderr, "cannot find bone \"%s\"\n", bone_name);
		return;
	}

	unit_m4(rest);
	copy_m4_m4(rest, bone->arm_mat);
	invert_m4_m4(irest, rest);

	// new curves to assign matrix transform animation
	FCurve *newcu[10]; // if tm_type is matrix, then create 10 curves: 4 rot, 3 loc, 3 scale
	unsigned int totcu = 10;
	const char *tm_str = NULL;
	char rna_path[200];
	for (int i = 0; i < totcu; i++) {

		int axis = i;

		if (i < 4) {
			tm_str = "rotation_quaternion";
			axis = i;
		}
		else if (i < 7) {
			tm_str = "location";
			axis = i - 4;
		}
		else {
			tm_str = "scale";
			axis = i - 7;
		}


		BLI_snprintf(rna_path, sizeof(rna_path), "%s.%s", joint_path, tm_str);

		newcu[i] = create_fcurve(axis, rna_path);
		newcu[i]->totvert = frames.size();
	}

	if (frames.size() == 0)
		return;

	std::sort(frames.begin(), frames.end());

	std::vector<float>::iterator it;

	// sample values at each frame
	for (it = frames.begin(); it != frames.end(); it++) {
		float fra = *it;

		float mat[4][4];
		float matfra[4][4];

		unit_m4(matfra);

		// calc object-space mat
		evaluate_transform_at_frame(matfra, node, fra);


		// for joints, we need a special matrix
		// special matrix: iR * M * iR_dae * R
		// where R, iR are bone rest and inverse rest mats in world space (Blender bones),
		// iR_dae is joint inverse rest matrix (DAE) and M is an evaluated joint world-space matrix (DAE)
		float temp[4][4], par[4][4];


		// calc M
		calc_joint_parent_mat_rest(par, NULL, root, node);
		mult_m4_m4m4(temp, par, matfra);

		// evaluate_joint_world_transform_at_frame(temp, NULL,, node, fra);

		// calc special matrix
		mul_serie_m4(mat, irest, temp, irest_dae, rest, NULL, NULL, NULL, NULL);

		float  rot[4], loc[3], scale[3];

		mat4_to_quat(rot, mat);
		copy_v3_v3(loc, mat[3]);
		mat4_to_size(scale, mat);

		// add keys
		for (int i = 0; i < totcu; i++) {
			if (i < 4)
				add_bezt(newcu[i], fra, rot[i]);
			else if (i < 7)
				add_bezt(newcu[i], fra, loc[i - 4]);
			else
				add_bezt(newcu[i], fra, scale[i - 7]);
		}
	}
	verify_adt_action((ID*)&ob->id, 1);

	// add curves
	for (int i= 0; i < totcu; i++) {
		add_bone_fcurve(ob, node, newcu[i]);
	}

	bPoseChannel *chan = get_pose_channel(ob->pose, bone_name);
	chan->rotmode = ROT_MODE_QUAT;

}


//Check if object is animated by checking if animlist_map holds the animlist_id of node transforms
AnimationImporter::AnimMix* AnimationImporter::get_animation_type ( const COLLADAFW::Node * node,
											std::map<COLLADAFW::UniqueId, const COLLADAFW::Object*> FW_object_map) 
{
	AnimMix *types = new AnimMix();

	const COLLADAFW::TransformationPointerArray& nodeTransforms = node->getTransformations();

	//for each transformation in node 
	for (unsigned int i = 0; i < nodeTransforms.getCount(); i++) {
		COLLADAFW::Transformation *transform = nodeTransforms[i];
		const COLLADAFW::UniqueId& listid = transform->getAnimationList();

		//check if transformation has animations
		if (animlist_map.find(listid) == animlist_map.end()) {
			continue;
		}
		else {
			types->transform = types->transform|NODE_TRANSFORM;
			break;
		}
	}
	const COLLADAFW::InstanceLightPointerArray& nodeLights = node->getInstanceLights();

	for (unsigned int i = 0; i < nodeLights.getCount(); i++) {
		const COLLADAFW::Light *light = (COLLADAFW::Light *) FW_object_map[nodeLights[i]->getInstanciatedObjectId()];
		types->light = setAnimType(&(light->getColor()), (types->light), LIGHT_COLOR);
		types->light = setAnimType(&(light->getFallOffAngle()), (types->light), LIGHT_FOA);
		types->light = setAnimType(&(light->getFallOffExponent()), (types->light), LIGHT_FOE);

		if ( types->light != 0) break;

	}

	const COLLADAFW::InstanceCameraPointerArray& nodeCameras = node->getInstanceCameras();
	for (unsigned int i = 0; i < nodeCameras.getCount(); i++) {
		const COLLADAFW::Camera *camera = (COLLADAFW::Camera *) FW_object_map[nodeCameras[i]->getInstanciatedObjectId()];

		if ( camera->getCameraType() == COLLADAFW::Camera::PERSPECTIVE ) {
			types->camera = setAnimType(&(camera->getXMag()), (types->camera), CAMERA_XFOV);
		}
		else {
			types->camera = setAnimType(&(camera->getXMag()), (types->camera), CAMERA_XMAG);
		}
		types->camera = setAnimType(&(camera->getFarClippingPlane()), (types->camera), CAMERA_ZFAR);
		types->camera = setAnimType(&(camera->getNearClippingPlane()), (types->camera), CAMERA_ZNEAR);

		if ( types->camera != 0) break;

	}

	const COLLADAFW::InstanceGeometryPointerArray& nodeGeoms = node->getInstanceGeometries();
	for (unsigned int i = 0; i < nodeGeoms.getCount(); i++) {
		const COLLADAFW::MaterialBindingArray& matBinds = nodeGeoms[i]->getMaterialBindings();
		for (unsigned int j = 0; j < matBinds.getCount(); j++) {
			const COLLADAFW::UniqueId & matuid = matBinds[j].getReferencedMaterial();
			const COLLADAFW::Effect *ef = (COLLADAFW::Effect *) (FW_object_map[matuid]);
			if (ef != NULL) { /* can be NULL [#28909] */
				const COLLADAFW::CommonEffectPointerArray& commonEffects = ef->getCommonEffects();
				if (!commonEffects.empty()) {
					COLLADAFW::EffectCommon *efc = commonEffects[0];
					types->material =  setAnimType(&(efc->getShininess()), (types->material), MATERIAL_SHININESS);
					types->material =  setAnimType(&(efc->getSpecular().getColor()), (types->material), MATERIAL_SPEC_COLOR);
					types->material =  setAnimType(&(efc->getDiffuse().getColor()), (types->material), MATERIAL_DIFF_COLOR);
					// types->material =  setAnimType(&(efc->get()), (types->material), MATERIAL_TRANSPARENCY);
					types->material =  setAnimType(&(efc->getIndexOfRefraction()), (types->material), MATERIAL_IOR);
				}
			}
		}
	}
	return types;
}

int AnimationImporter::setAnimType ( const COLLADAFW::Animatable * prop, int types, int addition)
{
	const COLLADAFW::UniqueId& listid =  prop->getAnimationList();
	if (animlist_map.find(listid) != animlist_map.end())
		return types|addition;
	else return types;
}		

// Is not used anymore.
void AnimationImporter::find_frames_old(std::vector<float> * frames, COLLADAFW::Node * node, COLLADAFW::Transformation::TransformationType tm_type)
{
	bool is_matrix = tm_type == COLLADAFW::Transformation::MATRIX;
	bool is_rotation = tm_type == COLLADAFW::Transformation::ROTATE;
	// for each <rotate>, <translate>, etc. there is a separate Transformation
	const COLLADAFW::TransformationPointerArray& nodeTransforms = node->getTransformations();

	unsigned int i;
	// find frames at which to sample plus convert all rotation keys to radians
	for (i = 0; i < nodeTransforms.getCount(); i++) {
		COLLADAFW::Transformation *transform = nodeTransforms[i];
		COLLADAFW::Transformation::TransformationType nodeTmType = transform->getTransformationType();


		if (nodeTmType == tm_type) {
			//get animation bindings for the current transformation
			const COLLADAFW::UniqueId& listid = transform->getAnimationList();
			//if transform is animated its animlist must exist.
			if (animlist_map.find(listid) != animlist_map.end()) {
				
				const COLLADAFW::AnimationList *animlist = animlist_map[listid];
				const COLLADAFW::AnimationList::AnimationBindings& bindings = animlist->getAnimationBindings();

				if (bindings.getCount()) {
					//for each AnimationBinding get the fcurves which animate the transform
					for (unsigned int j = 0; j < bindings.getCount(); j++) {
						std::vector<FCurve*>& curves = curve_map[bindings[j].animation];
						bool xyz = ((nodeTmType == COLLADAFW::Transformation::TRANSLATE || nodeTmType == COLLADAFW::Transformation::SCALE) && bindings[j].animationClass == COLLADAFW::AnimationList::POSITION_XYZ);

						if ((!xyz && curves.size() == 1) || (xyz && curves.size() == 3) || is_matrix) {
							std::vector<FCurve*>::iterator iter;

							for (iter = curves.begin(); iter != curves.end(); iter++) {
								FCurve *fcu = *iter;

								//if transform is rotation the fcurves values must be turned in to radian.
								if (is_rotation)
									fcurve_deg_to_rad(fcu);

								for (unsigned int k = 0; k < fcu->totvert; k++) {
									//get frame value from bezTriple
									float fra = fcu->bezt[k].vec[1][0];
									//if frame already not added add frame to frames
									if (std::find(frames->begin(), frames->end(), fra) == frames->end())
										frames->push_back(fra);
								}
							}
						}
						else {
							fprintf(stderr, "expected %d curves, got %d\n", xyz ? 3 : 1, (int)curves.size());
						}
					}
				}
			}
		}
	}
}



// prerequisites:
// animlist_map - map animlist id -> animlist
// curve_map - map anim id -> curve(s)
Object *AnimationImporter::translate_animation_OLD(COLLADAFW::Node *node,
							std::map<COLLADAFW::UniqueId, Object*>& object_map,
							std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& root_map,
							COLLADAFW::Transformation::TransformationType tm_type,
							Object *par_job)
{
	
	bool is_rotation = tm_type == COLLADAFW::Transformation::ROTATE;
	bool is_matrix = tm_type == COLLADAFW::Transformation::MATRIX;
	bool is_joint = node->getType() == COLLADAFW::Node::JOINT;
	
	COLLADAFW::Node *root = root_map.find(node->getUniqueId()) == root_map.end() ? node : root_map[node->getUniqueId()];
	Object *ob = is_joint ? armature_importer->get_armature_for_joint(node) : object_map[node->getUniqueId()];
	const char *bone_name = is_joint ? bc_get_joint_name(node) : NULL;
	if (!ob) {
		fprintf(stderr, "cannot find Object for Node with id=\"%s\"\n", node->getOriginalId().c_str());
		return NULL;
	}

	// frames at which to sample
	std::vector<float> frames;
	
	find_frames_old(&frames, node, tm_type);
	
	unsigned int i;
	
	float irest_dae[4][4];
	float rest[4][4], irest[4][4];

	if (is_joint) {
		get_joint_rest_mat(irest_dae, root, node);
		invert_m4(irest_dae);

		Bone *bone = get_named_bone((bArmature*)ob->data, bone_name);
		if (!bone) {
			fprintf(stderr, "cannot find bone \"%s\"\n", bone_name);
			return NULL;
		}

		unit_m4(rest);
		copy_m4_m4(rest, bone->arm_mat);
		invert_m4_m4(irest, rest);
	}

	Object *job = NULL;

#ifdef ARMATURE_TEST
	FCurve *job_curves[10];
	job = get_joint_object(root, node, par_job);
#endif

	if (frames.size() == 0)
		return job;

	std::sort(frames.begin(), frames.end());

	const char *tm_str = NULL;
	switch (tm_type) {
	case COLLADAFW::Transformation::ROTATE:
		tm_str = "rotation_quaternion";
		break;
	case COLLADAFW::Transformation::SCALE:
		tm_str = "scale";
		break;
	case COLLADAFW::Transformation::TRANSLATE:
		tm_str = "location";
		break;
	case COLLADAFW::Transformation::MATRIX:
		break;
	default:
		return job;
	}

	char rna_path[200];
	char joint_path[200];

	if (is_joint)
		armature_importer->get_rna_path_for_joint(node, joint_path, sizeof(joint_path));

	// new curves
	FCurve *newcu[10]; // if tm_type is matrix, then create 10 curves: 4 rot, 3 loc, 3 scale
	unsigned int totcu = is_matrix ? 10 : (is_rotation ? 4 : 3);

	for (i = 0; i < totcu; i++) {

		int axis = i;

		if (is_matrix) {
			if (i < 4) {
				tm_str = "rotation_quaternion";
				axis = i;
			}
			else if (i < 7) {
				tm_str = "location";
				axis = i - 4;
			}
			else {
				tm_str = "scale";
				axis = i - 7;
			}
		}

		if (is_joint)
			BLI_snprintf(rna_path, sizeof(rna_path), "%s.%s", joint_path, tm_str);
		else
			BLI_strncpy(rna_path, tm_str, sizeof(rna_path));
		newcu[i] = create_fcurve(axis, rna_path);

#ifdef ARMATURE_TEST
		if (is_joint)
			job_curves[i] = create_fcurve(axis, tm_str);
#endif
	}

	std::vector<float>::iterator it;

	// sample values at each frame
	for (it = frames.begin(); it != frames.end(); it++) {
		float fra = *it;

		float mat[4][4];
		float matfra[4][4];

		unit_m4(matfra);

		// calc object-space mat
		evaluate_transform_at_frame(matfra, node, fra);

		// for joints, we need a special matrix
		if (is_joint) {
			// special matrix: iR * M * iR_dae * R
			// where R, iR are bone rest and inverse rest mats in world space (Blender bones),
			// iR_dae is joint inverse rest matrix (DAE) and M is an evaluated joint world-space matrix (DAE)
			float temp[4][4], par[4][4];

			// calc M
			calc_joint_parent_mat_rest(par, NULL, root, node);
			mult_m4_m4m4(temp, par, matfra);

			// evaluate_joint_world_transform_at_frame(temp, NULL,, node, fra);

			// calc special matrix
			mul_serie_m4(mat, irest, temp, irest_dae, rest, NULL, NULL, NULL, NULL);
		}
		else {
			copy_m4_m4(mat, matfra);
		}

		float val[4], rot[4], loc[3], scale[3];

		switch (tm_type) {
		case COLLADAFW::Transformation::ROTATE:
			mat4_to_quat(val, mat);
			break;
		case COLLADAFW::Transformation::SCALE:
			mat4_to_size(val, mat);
			break;
		case COLLADAFW::Transformation::TRANSLATE:
			copy_v3_v3(val, mat[3]);
			break;
		case COLLADAFW::Transformation::MATRIX:
			mat4_to_quat(rot, mat);
			copy_v3_v3(loc, mat[3]);
			mat4_to_size(scale, mat);
			break;
		default:
			break;
		}

		// add keys
		for (i = 0; i < totcu; i++) {
			if (is_matrix) {
				if (i < 4)
					add_bezt(newcu[i], fra, rot[i]);
				else if (i < 7)
					add_bezt(newcu[i], fra, loc[i - 4]);
				else
					add_bezt(newcu[i], fra, scale[i - 7]);
			}
			else {
				add_bezt(newcu[i], fra, val[i]);
			}
		}

#ifdef ARMATURE_TEST
		if (is_joint) {
			switch (tm_type) {
			case COLLADAFW::Transformation::ROTATE:
				mat4_to_quat(val, matfra);
				break;
			case COLLADAFW::Transformation::SCALE:
				mat4_to_size(val, matfra);
				break;
			case COLLADAFW::Transformation::TRANSLATE:
				copy_v3_v3(val, matfra[3]);
				break;
			case MATRIX:
				mat4_to_quat(rot, matfra);
				copy_v3_v3(loc, matfra[3]);
				mat4_to_size(scale, matfra);
				break;
			default:
				break;
			}

			for (i = 0; i < totcu; i++) {
				if (is_matrix) {
					if (i < 4)
						add_bezt(job_curves[i], fra, rot[i]);
					else if (i < 7)
						add_bezt(job_curves[i], fra, loc[i - 4]);
					else
						add_bezt(job_curves[i], fra, scale[i - 7]);
				}
				else {
					add_bezt(job_curves[i], fra, val[i]);
				}
			}
		}
#endif
	}

	verify_adt_action((ID*)&ob->id, 1);

	ListBase *curves = &ob->adt->action->curves;

	// add curves
	for (i = 0; i < totcu; i++) {
		if (is_joint)
			add_bone_fcurve(ob, node, newcu[i]);
		else
			BLI_addtail(curves, newcu[i]);

#ifdef ARMATURE_TEST
		if (is_joint)
			BLI_addtail(&job->adt->action->curves, job_curves[i]);
#endif
	}

	if (is_rotation || is_matrix) {
		if (is_joint) {
			bPoseChannel *chan = get_pose_channel(ob->pose, bone_name);
			chan->rotmode = ROT_MODE_QUAT;
		}
		else {
			ob->rotmode = ROT_MODE_QUAT;
		}
	}

	return job;
}

// internal, better make it private
// warning: evaluates only rotation and only assigns matrix transforms now
// prerequisites: animlist_map, curve_map
void AnimationImporter::evaluate_transform_at_frame(float mat[4][4], COLLADAFW::Node *node, float fra)
{
	const COLLADAFW::TransformationPointerArray& tms = node->getTransformations();

	unit_m4(mat);

	for (unsigned int i = 0; i < tms.getCount(); i++) {
		COLLADAFW::Transformation *tm = tms[i];
		COLLADAFW::Transformation::TransformationType type = tm->getTransformationType();
		float m[4][4];

		unit_m4(m);

		std::string nodename = node->getName().size() ? node->getName() : node->getOriginalId();
		if (!evaluate_animation(tm, m, fra, nodename.c_str())) {
			switch (type) {
			case COLLADAFW::Transformation::ROTATE:
				dae_rotate_to_mat4(tm, m);
				break;
			case COLLADAFW::Transformation::TRANSLATE:
				dae_translate_to_mat4(tm, m);
				break;
			case COLLADAFW::Transformation::SCALE:
				dae_scale_to_mat4(tm, m);
				break;
			case COLLADAFW::Transformation::MATRIX:
				dae_matrix_to_mat4(tm, m);
				break;
			default:
				fprintf(stderr, "unsupported transformation type %d\n", type);
			}
			// dae_matrix_to_mat4(tm, m);
			
		}

		float temp[4][4];
		copy_m4_m4(temp, mat);

		mult_m4_m4m4(mat, temp, m);
	}
}

// return true to indicate that mat contains a sane value
bool AnimationImporter::evaluate_animation(COLLADAFW::Transformation *tm, float mat[4][4], float fra, const char *node_id)
{
	const COLLADAFW::UniqueId& listid = tm->getAnimationList();
	COLLADAFW::Transformation::TransformationType type = tm->getTransformationType();

	if (type != COLLADAFW::Transformation::ROTATE &&
		type != COLLADAFW::Transformation::SCALE &&
		type != COLLADAFW::Transformation::TRANSLATE &&
		type != COLLADAFW::Transformation::MATRIX) {
		fprintf(stderr, "animation of transformation %d is not supported yet\n", type);
		return false;
	}

	if (animlist_map.find(listid) == animlist_map.end())
		return false;

	const COLLADAFW::AnimationList *animlist = animlist_map[listid];
	const COLLADAFW::AnimationList::AnimationBindings& bindings = animlist->getAnimationBindings();

	if (bindings.getCount()) {
		float vec[3];

		bool is_scale = (type == COLLADAFW::Transformation::SCALE);
		bool is_translate = (type == COLLADAFW::Transformation::TRANSLATE);

		if (is_scale)
			dae_scale_to_v3(tm, vec);
		else if (is_translate)
			dae_translate_to_v3(tm, vec);

		for (unsigned int j = 0; j < bindings.getCount(); j++) {
			const COLLADAFW::AnimationList::AnimationBinding& binding = bindings[j];
			std::vector<FCurve*>& curves = curve_map[binding.animation];
			COLLADAFW::AnimationList::AnimationClass animclass = binding.animationClass;
			char path[100];

			switch (type) {
			case COLLADAFW::Transformation::ROTATE:
				BLI_snprintf(path, sizeof(path), "%s.rotate (binding %u)", node_id, j);
				break;
			case COLLADAFW::Transformation::SCALE:
				BLI_snprintf(path, sizeof(path), "%s.scale (binding %u)", node_id, j);
				break;
			case COLLADAFW::Transformation::TRANSLATE:
				BLI_snprintf(path, sizeof(path), "%s.translate (binding %u)", node_id, j);
				break;
			case COLLADAFW::Transformation::MATRIX:
				BLI_snprintf(path, sizeof(path), "%s.matrix (binding %u)", node_id, j);
				break;
			default:
				break;
			}

			if (animclass == COLLADAFW::AnimationList::UNKNOWN_CLASS) {
				fprintf(stderr, "%s: UNKNOWN animation class\n", path);
				//continue;
			}

			if (type == COLLADAFW::Transformation::ROTATE) {
				if (curves.size() != 1) {
					fprintf(stderr, "expected 1 curve, got %d\n", (int)curves.size());
					return false;
				}

				// TODO support other animclasses
				if (animclass != COLLADAFW::AnimationList::ANGLE) {
					fprintf(stderr, "%s: animation class %d is not supported yet\n", path, animclass);
					return false;
				}

				COLLADABU::Math::Vector3& axis = ((COLLADAFW::Rotate*)tm)->getRotationAxis();

				float ax[3] = {(float)axis[0], (float)axis[1], (float)axis[2]};
				float angle = evaluate_fcurve(curves[0], fra);
				axis_angle_to_mat4(mat, ax, angle);

				return true;
			}
			else if (is_scale || is_translate) {
				bool is_xyz = animclass == COLLADAFW::AnimationList::POSITION_XYZ;

				if ((!is_xyz && curves.size() != 1) || (is_xyz && curves.size() != 3)) {
					if (is_xyz)
						fprintf(stderr, "%s: expected 3 curves, got %d\n", path, (int)curves.size());
					else
						fprintf(stderr, "%s: expected 1 curve, got %d\n", path, (int)curves.size());
					return false;
				}
				
				switch (animclass) {
				case COLLADAFW::AnimationList::POSITION_X:
					vec[0] = evaluate_fcurve(curves[0], fra);
					break;
				case COLLADAFW::AnimationList::POSITION_Y:
					vec[1] = evaluate_fcurve(curves[0], fra);
					break;
				case COLLADAFW::AnimationList::POSITION_Z:
					vec[2] = evaluate_fcurve(curves[0], fra);
					break;
				case COLLADAFW::AnimationList::POSITION_XYZ:
					vec[0] = evaluate_fcurve(curves[0], fra);
					vec[1] = evaluate_fcurve(curves[1], fra);
					vec[2] = evaluate_fcurve(curves[2], fra);
					break;
				default:
					fprintf(stderr, "%s: animation class %d is not supported yet\n", path, animclass);
					break;
				}
			}
			else if (type == COLLADAFW::Transformation::MATRIX) {
				// for now, of matrix animation, support only the case when all values are packed into one animation
				if (curves.size() != 16) {
					fprintf(stderr, "%s: expected 16 curves, got %d\n", path, (int)curves.size());
					return false;
				}

				COLLADABU::Math::Matrix4 matrix;
				int i = 0, j = 0;

				for (std::vector<FCurve*>::iterator it = curves.begin(); it != curves.end(); it++) {
					matrix.setElement(i, j, evaluate_fcurve(*it, fra));
					j++;
					if (j == 4) {
						i++;
						j = 0;
					}
					unused_curves.erase(std::remove(unused_curves.begin(), unused_curves.end(), *it), unused_curves.end());
				}

				COLLADAFW::Matrix tm(matrix);
				dae_matrix_to_mat4(&tm, mat);

				std::vector<FCurve*>::iterator it;

				return true;
			}
		}

		if (is_scale)
			size_to_mat4(mat, vec);
		else
			copy_v3_v3(mat[3], vec);

		return is_scale || is_translate;
	}

	return false;
}

// gives a world-space mat of joint at rest position
void AnimationImporter::get_joint_rest_mat(float mat[4][4], COLLADAFW::Node *root, COLLADAFW::Node *node)
{
	// if bind mat is not available,
	// use "current" node transform, i.e. all those tms listed inside <node>
	if (!armature_importer->get_joint_bind_mat(mat, node)) {
		float par[4][4], m[4][4];

		calc_joint_parent_mat_rest(par, NULL, root, node);
		get_node_mat(m, node, NULL, NULL);
		mult_m4_m4m4(mat, par, m);
	}
}

// gives a world-space mat, end's mat not included
bool AnimationImporter::calc_joint_parent_mat_rest(float mat[4][4], float par[4][4], COLLADAFW::Node *node, COLLADAFW::Node *end)
{
	float m[4][4];

	if (node == end) {
		par ? copy_m4_m4(mat, par) : unit_m4(mat);
		return true;
	}

	// use bind matrix if available or calc "current" world mat
	if (!armature_importer->get_joint_bind_mat(m, node)) {
		if (par) {
			float temp[4][4];
			get_node_mat(temp, node, NULL, NULL);
			mult_m4_m4m4(m, par, temp);
		}
		else {
			get_node_mat(m, node, NULL, NULL);
		}
	}

	COLLADAFW::NodePointerArray& children = node->getChildNodes();
	for (unsigned int i = 0; i < children.getCount(); i++) {
		if (calc_joint_parent_mat_rest(mat, m, children[i], end))
			return true;
	}

	return false;
}

#ifdef ARMATURE_TEST
Object *AnimationImporter::get_joint_object(COLLADAFW::Node *root, COLLADAFW::Node *node, Object *par_job)
{
	if (joint_objects.find(node->getUniqueId()) == joint_objects.end()) {
		Object *job = add_object(scene, OB_EMPTY);

		rename_id((ID*)&job->id, (char*)get_joint_name(node));

		job->lay = object_in_scene(job, scene)->lay = 2;

		mul_v3_fl(job->size, 0.5f);
		job->recalc |= OB_RECALC_OB;

		verify_adt_action((ID*)&job->id, 1);

		job->rotmode = ROT_MODE_QUAT;

		float mat[4][4];
		get_joint_rest_mat(mat, root, node);

		if (par_job) {
			float temp[4][4], ipar[4][4];
			invert_m4_m4(ipar, par_job->obmat);
			copy_m4_m4(temp, mat);
			mult_m4_m4m4(mat, ipar, temp);
		}

		TransformBase::decompose(mat, job->loc, NULL, job->quat, job->size);

		if (par_job) {
			job->parent = par_job;

			par_job->recalc |= OB_RECALC_OB;
			job->parsubstr[0] = 0;
		}

		where_is_object(scene, job);

		// after parenting and layer change
		DAG_scene_sort(CTX_data_main(C), scene);

		joint_objects[node->getUniqueId()] = job;
	}

	return joint_objects[node->getUniqueId()];
}
#endif

#if 0
// recursively evaluates joint tree until end is found, mat then is world-space matrix of end
// mat must be identity on enter, node must be root
bool AnimationImporter::evaluate_joint_world_transform_at_frame(float mat[4][4], float par[4][4], COLLADAFW::Node *node, COLLADAFW::Node *end, float fra)
{
	float m[4][4];
	if (par) {
		float temp[4][4];
		evaluate_transform_at_frame(temp, node, node == end ? fra : 0.0f);
		mult_m4_m4m4(m, par, temp);
	}
	else {
		evaluate_transform_at_frame(m, node, node == end ? fra : 0.0f);
	}

	if (node == end) {
		copy_m4_m4(mat, m);
		return true;
	}
	else {
		COLLADAFW::NodePointerArray& children = node->getChildNodes();
		for (int i = 0; i < children.getCount(); i++) {
			if (evaluate_joint_world_transform_at_frame(mat, m, children[i], end, fra))
				return true;
		}
	}

	return false;
}
#endif

void AnimationImporter::add_bone_fcurve(Object *ob, COLLADAFW::Node *node, FCurve *fcu)
{
	const char *bone_name = bc_get_joint_name(node);
	bAction *act = ob->adt->action;
			
	/* try to find group */
	bActionGroup *grp = action_groups_find_named(act, bone_name);

	/* no matching groups, so add one */
	if (grp == NULL) {
		/* Add a new group, and make it active */
		grp = (bActionGroup*)MEM_callocN(sizeof(bActionGroup), "bActionGroup");
					
		grp->flag = AGRP_SELECTED;
		BLI_strncpy(grp->name, bone_name, sizeof(grp->name));
					
		BLI_addtail(&act->groups, grp);
		BLI_uniquename(&act->groups, grp, "Group", '.', offsetof(bActionGroup, name), 64);
	}
				
	/* add F-Curve to group */
	action_groups_add_channel(act, grp, fcu);
}

void AnimationImporter::add_bezt(FCurve *fcu, float fra, float value)
{
	//float fps = (float)FPS;
	BezTriple bez;
	memset(&bez, 0, sizeof(BezTriple));
	bez.vec[1][0] = fra;
	bez.vec[1][1] = value;
	bez.ipo = BEZT_IPO_LIN ;/* use default interpolation mode here... */
	bez.f1 = bez.f2 = bez.f3 = SELECT;
	bez.h1 = bez.h2 = HD_AUTO;
	insert_bezt_fcurve(fcu, &bez, 0);
	calchandles_fcurve(fcu);
}

