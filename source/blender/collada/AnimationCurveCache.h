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

#ifndef __ANIMATION_CURVE_CACHE_H__
#define __ANIMATION_CURVE_CACHE_H__

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <map>
#include <algorithm>    // std::find

#include "exportSettings.h"
#include "collada_utils.h"

extern "C"
{
#include "DNA_object_types.h"
#include "DNA_anim_types.h"
}

class Matrix {
private:
	float matrix[4][4];
public:
	Matrix();
	Matrix(float (&mat)[4][4]);
	void set_matrix(float (&mat)[4][4]);
	void set_matrix(Matrix &mat);
	void get_matrix(float (&mat)[4][4]);
};

class SamplePoint {

private:

	Object * ob;
	Bone *pose_bone;
	FCurve *fcu;
	int frame; /* frame in timeline (not sure if we actually should store a float here) */
	int index; /* Keyframe index in fcurve (makes sense only when fcu is also set) */
	std::string path; /* Do not mixup with rna_path. It is used for different purposes! */

	Matrix matrix; /* Local matrix, by default unit matrix, will be set when sampling */

public:

	SamplePoint(int frame, Object *ob);
	SamplePoint(int frame, Object *ob, FCurve *fcu, int index);
	SamplePoint(int frame, Object *ob, Bone *bone);

	Object *get_object();
	Bone *get_bone();
	FCurve *get_fcurve();
	int get_frame();
	int get_fcurve_index();
	Matrix &get_matrix();
	std::string &get_path();

	void set_matrix(Matrix &matrix);
	void set_matrix(float(&mat)[4][4]);
};


class AnimationCurveCache {
private:
	void clear_cache(); // remove all sampled FCurves
	void clear_cache(Object *ob); //remove sampled FCurves for single object
	void create_curves(Object *ob);

	std::vector<Object *> cached_objects; // list of objects for caching
	std::map<Object *, std::vector<FCurve *>> cached_curves; //map of cached FCurves
	std::map<int, std::vector<SamplePoint>> sample_frames; // list of frames where objects need to be sampled

	std::vector<SamplePoint> &getFrameInfos(int frame_index);
	void add_sample_point(SamplePoint &point);
	void enable_fcurves(bAction *act, char *bone_name);
	bool bone_matrix_local_get(Object *ob, Bone *bone, float (&mat)[4][4], bool for_opensim);

	bContext *mContext;

public:

	AnimationCurveCache(bContext *C);
	~AnimationCurveCache();

	void addObject(Object *obj);
	
	void sampleMain(Scene *scene, 
		BC_export_transformation_type atm_type, 
		bool for_opensim);
	
	void sampleScene(Scene *scene, 
		BC_export_transformation_type atm_type, 
		bool for_opensim,
		bool keyframe_at_end = true); // use keys from FCurves, use timeline boundaries

	void sampleScene(Scene *scene, 
		BC_export_transformation_type atm_type,
		int sampling_rate, bool for_opensim,
		bool keyframe_at_end = true ); // generate keyframes for frames use timeline boundaries

	std::vector<FCurve *> *getSampledCurves(Object *ob);

	void create_sample_frames_from_keyframes();
	void create_sample_frames_generated(float sfra, float efra, int sampling_rate, int keyframe_at_end);
};


#endif
