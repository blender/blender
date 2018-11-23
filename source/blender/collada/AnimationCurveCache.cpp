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
* Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed.
*
* ***** END GPL LICENSE BLOCK *****
*/

#include "AnimationCurveCache.h"

extern "C" {
#include "BKE_action.h"
#include "BLI_listbase.h"
}

AnimationCurveCache::AnimationCurveCache(bContext *C):
	mContext(C)
{
}

AnimationCurveCache::~AnimationCurveCache()
{
	clear_cache();
}

void AnimationCurveCache::clear_cache()
{

}

void AnimationCurveCache::clear_cache(Object *ob)
{

}

void AnimationCurveCache::create_curves(Object *ob)
{

}

void AnimationCurveCache::addObject(Object *ob)
{
	cached_objects.push_back(ob);
}

bool AnimationCurveCache::bone_matrix_local_get(Object *ob, Bone *bone, float(&mat)[4][4], bool for_opensim)
{

	/* Ok, lets be super cautious and check if the bone exists */
	bPose *pose = ob->pose;
	bPoseChannel *pchan = BKE_pose_channel_find_name(pose, bone->name);
	if (!pchan) {
		return false;
	}

	bAction *action = bc_getSceneObjectAction(ob);
	bPoseChannel *parchan = pchan->parent;
	enable_fcurves(action, bone->name);
	float ipar[4][4];

	if (bone->parent) {
		invert_m4_m4(ipar, parchan->pose_mat);
		mul_m4_m4m4(mat, ipar, pchan->pose_mat);
	}
	else
		copy_m4_m4(mat, pchan->pose_mat);

	/* OPEN_SIM_COMPATIBILITY
	* AFAIK animation to second life is via BVH, but no
	* reason to not have the collada-animation be correct
	*/
	if (for_opensim) {
		float temp[4][4];
		copy_m4_m4(temp, bone->arm_mat);
		temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;
		invert_m4(temp);

		mul_m4_m4m4(mat, mat, temp);

		if (bone->parent) {
			copy_m4_m4(temp, bone->parent->arm_mat);
			temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;

			mul_m4_m4m4(mat, temp, mat);
		}
	}

	return true;
}

void AnimationCurveCache::sampleMain(Scene *scene, BC_export_transformation_type atm_type, bool for_opensim)
{
	std::map<int, std::vector<SamplePoint>>::iterator frame;
	for (frame = sample_frames.begin(); frame != sample_frames.end(); frame++) {
		int frame_index = frame->first;
		std::vector<SamplePoint> sample_points = frame->second;

		bc_update_scene(mContext, scene, frame_index);

		for (int spi = 0; spi < sample_points.size(); spi++) {
			SamplePoint &point = sample_points[spi];
			Object *ob = point.get_object();
			float mat[4][4];

			if (ob->type == OB_ARMATURE) {
				/* For Armatures we need to check if this maybe is a pose sample point*/
				Bone *bone = point.get_bone();
				if (bone) {
					if (bone_matrix_local_get(ob, bone, mat, for_opensim)) {
						point.set_matrix(mat);
					}
					continue;
				}
			}

			/* When this SamplePoint is not for a Bone, 
			 * then we just store the Object local matrix here
			 */

			BKE_object_matrix_local_get(ob, mat);
			point.set_matrix(mat);

		}
	}
}

/*
* enable fcurves driving a specific bone, disable all the rest
* if bone_name = NULL enable all fcurves
*/
void AnimationCurveCache::enable_fcurves(bAction *act, char *bone_name)
{
	FCurve *fcu;
	char prefix[200];

	if (bone_name)
		BLI_snprintf(prefix, sizeof(prefix), "pose.bones[\"%s\"]", bone_name);

	for (fcu = (FCurve *)act->curves.first; fcu; fcu = fcu->next) {
		if (bone_name) {
			if (STREQLEN(fcu->rna_path, prefix, strlen(prefix)))
				fcu->flag &= ~FCURVE_DISABLED;
			else
				fcu->flag |= FCURVE_DISABLED;
		}
		else {
			fcu->flag &= ~FCURVE_DISABLED;
		}
	}
}
/*
* Sample the scene at frames where object fcurves
* have defined keys.
*/
void AnimationCurveCache::sampleScene(Scene *scene, BC_export_transformation_type atm_type, bool for_opensim, bool keyframe_at_end)
{
	create_sample_frames_from_keyframes();
	sampleMain(scene, atm_type, for_opensim);
}

void AnimationCurveCache::sampleScene(Scene *scene, BC_export_transformation_type atm_type, int sampling_rate, bool for_opensim, bool keyframe_at_end)
{
	create_sample_frames_generated(scene->r.sfra, scene->r.efra, sampling_rate, keyframe_at_end);
	sampleMain(scene, atm_type, for_opensim);
}

std::vector<FCurve *> *AnimationCurveCache::getSampledCurves(Object *ob)
{
	std::map<Object *, std::vector<FCurve *>>::iterator fcurves;
	fcurves = cached_curves.find(ob);
	return (fcurves == cached_curves.end()) ? NULL : &fcurves->second;
}

std::vector<SamplePoint> &AnimationCurveCache::getFrameInfos(int frame_index)
{
	std::map<int, std::vector<SamplePoint>>::iterator frames = sample_frames.find(frame_index);
	if (frames == sample_frames.end()) {
		std::vector<SamplePoint> sample_points;
		sample_frames[frame_index] = sample_points;
	}
	return sample_frames[frame_index];
}


void AnimationCurveCache::add_sample_point(SamplePoint &point)
{
	int frame_index = point.get_frame();
	std::vector<SamplePoint> &frame_infos = getFrameInfos(frame_index);
	frame_infos.push_back(point);
}

/*
* loop over all cached objects
*     loop over all fcurves
*         record all keyframes
* 
* The vector sample_frames finally contains a list of vectors
* where each vector contains a list of SamplePoints which
* need to be processed when evaluating the animation.
*/
void AnimationCurveCache::create_sample_frames_from_keyframes()
{
	sample_frames.clear();
	for (int i = 0; i < cached_objects.size(); i++) {
		Object *ob = cached_objects[i];
		bAction *action = bc_getSceneObjectAction(ob);
		FCurve *fcu = (FCurve *)action->curves.first;

		for (; fcu; fcu = fcu->next) {
			for (unsigned int i = 0; i < fcu->totvert; i++) {
				float f = fcu->bezt[i].vec[1][0];
				int frame_index = int(f);
				SamplePoint sample_point(frame_index, ob, fcu, i);
				add_sample_point(sample_point);
			}
		}
	}
}

/*
* loop over all cached objects
*     loop over active action using a stesize of sampling_rate
*         record all frames
*
* The vector sample_frames finally contains a list of vectors
* where each vector contains a list of SamplePoints which
* need to be processed when evaluating the animation.
* Note: The FCurves of the objects will not be used here.
*/
void AnimationCurveCache::create_sample_frames_generated(float sfra, float efra, int sampling_rate, int keyframe_at_end)
{
	sample_frames.clear();

	for (int i = 0; i < cached_objects.size(); i++) {

		Object *ob = cached_objects[i];
		float f = sfra;

		do {		
			int frame_index = int(f);
			SamplePoint sample_point(frame_index, ob);
			add_sample_point(sample_point);

			/* Depending on the Object type add more sample points here
			*/

			if (ob && ob->type == OB_ARMATURE) {
				LISTBASE_FOREACH(bPoseChannel *, pchan, &ob->pose->chanbase) {
					SamplePoint point(frame_index, ob, pchan->bone);
					add_sample_point(sample_point);
				}
			}

			if (f == efra)
				break;
			f += sampling_rate;
			if (f > efra)
				if (keyframe_at_end)
					f = efra; // make sure the last frame is always exported
				else
					break;
		} while (true);
	}
}

Matrix::Matrix()
{
	unit_m4(matrix);
}

Matrix::Matrix(float (&mat)[4][4])
{
	set_matrix(mat);
}

void Matrix::set_matrix(float(&mat)[4][4])
{
	copy_m4_m4(matrix, mat);
}

void Matrix::set_matrix(Matrix &mat)
{
	copy_m4_m4(matrix, mat.matrix);
}

void Matrix::get_matrix(float(&mat)[4][4])
{
	copy_m4_m4(mat, matrix);
}

SamplePoint::SamplePoint(int frame, Object *ob)
{
	this->frame = frame;
	this->fcu = NULL;
	this->ob = ob;
	this->pose_bone = NULL;
	this->index = -1;
}

SamplePoint::SamplePoint(int frame, Object *ob, FCurve *fcu, int index)
{
	this->frame = frame;
	this->fcu = fcu;
	this->ob = ob;
	this->pose_bone = NULL;
	this->index = index;
	this->path = std::string(fcu->rna_path);

	/* Further elaborate on what this Fcurve is doing by checking
	 * its rna_path
     */

	if (ob && ob->type == OB_ARMATURE) {
		char *boneName = BLI_str_quoted_substrN(fcu->rna_path, "pose.bones[");
		bPose *pose = ob->pose;
		if (boneName) {
			bPoseChannel *pchan = BKE_pose_channel_find_name(pose, boneName);
			this->pose_bone = pchan->bone;
		}
	}
}


SamplePoint::SamplePoint(int frame, Object *ob, Bone *bone)
{
	this->frame = frame;
	this->fcu = NULL;
	this->ob = ob;
	this->pose_bone = bone;
	this->index = -1;
	this->path = "pose.bones[\"" + id_name(bone) + "\"].matrix";
}

Matrix &SamplePoint::get_matrix()
{
	return matrix;
}

void SamplePoint::set_matrix(Matrix &mat)
{
	this->matrix.set_matrix(mat);
}

void SamplePoint::set_matrix(float(&mat)[4][4])
{

}

Object *SamplePoint::get_object()
{
	return this->ob;
}

Bone *SamplePoint::get_bone()
{
	return this->pose_bone;
}

FCurve *SamplePoint::get_fcurve()
{
	return this->fcu;
}

int SamplePoint::get_frame()
{
	return this->frame;
}

int SamplePoint::get_fcurve_index()
{
	return this->index;
}

std::string &SamplePoint::get_path()
{
	return path;
}