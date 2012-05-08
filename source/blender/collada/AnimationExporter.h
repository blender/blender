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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
extern "C" 
{
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_lamp_types.h"
#include "DNA_camera_types.h"
#include "DNA_armature_types.h"
#include "DNA_material_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_fcurve.h"
#include "BKE_animsys.h"
#include "BKE_scene.h"
#ifdef NAN_BUILDINFO
extern char build_rev[];
#endif
}

#include "MEM_guardedalloc.h"

#include "BKE_action.h" // pose functions
#include "BKE_armature.h"
#include "BKE_object.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_listbase.h"

#include "RNA_access.h"

#include "COLLADASWSource.h"
#include "COLLADASWInstanceGeometry.h"
#include "COLLADASWInputList.h"
#include "COLLADASWPrimitves.h"
#include "COLLADASWVertices.h"
#include "COLLADASWLibraryAnimations.h"
#include "COLLADASWParamTemplate.h"
#include "COLLADASWParamBase.h"
#include "COLLADASWSampler.h"
#include "COLLADASWConstants.h"
#include "COLLADASWBaseInputElement.h"

#include "EffectExporter.h"

#include "collada_internal.h"

#include <vector>
#include <algorithm> // std::find

class AnimationExporter: COLLADASW::LibraryAnimations
{
private:
	Scene *scene;
	COLLADASW::StreamWriter *sw;

public:

	AnimationExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings):
			COLLADASW::LibraryAnimations(sw), export_settings(export_settings)
			{ this->sw = sw; }
	

	void exportAnimations(Scene *sce);

	// called for each exported object
	void operator() (Object *ob); 
	
protected:
	const ExportSettings *export_settings;

	void dae_animation(Object* ob, FCurve *fcu, char* transformName , bool is_param, Material *ma = NULL);

	void write_bone_animation_matrix(Object *ob_arm, Bone *bone);

	void write_bone_animation(Object *ob_arm, Bone *bone);

	void sample_and_write_bone_animation(Object *ob_arm, Bone *bone, int transform_type);

	bool is_bone_deform_group(Bone * bone);

	void sample_and_write_bone_animation_matrix(Object *ob_arm, Bone *bone);

	void sample_animation(float *v, std::vector<float> &frames, int type, Bone *bone, Object *ob_arm, bPoseChannel *pChan);

	void sample_animation(std::vector<float[4][4]> &mats, std::vector<float> &frames, Bone *bone, Object *ob_arm, bPoseChannel *pChan);

	// dae_bone_animation -> add_bone_animation
	// (blend this into dae_bone_animation)
	void dae_bone_animation(std::vector<float> &fra, float *v, int tm_type, int axis, std::string ob_name, std::string bone_name);
	
	void dae_baked_animation(std::vector<float> &fra, Object *ob_arm , Bone *bone);

	float convert_time(float frame);

	float convert_angle(float angle);

	std::string get_semantic_suffix(COLLADASW::InputSemantic::Semantics semantic);	

	void add_source_parameters(COLLADASW::SourceBase::ParameterNameList& param,
							   COLLADASW::InputSemantic::Semantics semantic, bool is_rot, const char *axis , bool transform);
	
	void get_source_values(BezTriple *bezt, COLLADASW::InputSemantic::Semantics semantic, bool rotation, float *values, int *length);
	
	float * get_eul_source_for_quat(Object *ob );

	std::string create_source_from_fcurve(COLLADASW::InputSemantic::Semantics semantic, FCurve *fcu, const std::string& anim_id, const char *axis_name);

	std::string create_source_from_array(COLLADASW::InputSemantic::Semantics semantic, float *v, int tot, bool is_rot, const std::string& anim_id, const char *axis_name);

	std::string create_source_from_vector(COLLADASW::InputSemantic::Semantics semantic, std::vector<float> &fra, bool is_rot, const std::string& anim_id, const char *axis_name);

	std::string create_xyz_source(float *v, int tot, const std::string& anim_id);

	std::string create_4x4_source(std::vector<float> &frames , Object * ob_arm, Bone *bone , const std::string& anim_id);

	std::string create_interpolation_source(FCurve *fcu, const std::string& anim_id, const char *axis_name, bool *has_tangents);

	std::string fake_interpolation_source(int tot, const std::string& anim_id, const char *axis_name);
	// for rotation, axis name is always appended and the value of append_axis is ignored
	std::string get_transform_sid(char *rna_path, int tm_type, const char *axis_name, bool append_axis);
	std::string get_light_param_sid(char *rna_path, int tm_type, const char *axis_name, bool append_axis);
	std::string get_camera_param_sid(char *rna_path, int tm_type, const char *axis_name, bool append_axis);
	void find_frames(Object *ob, std::vector<float> &fra, const char *prefix, const char *tm_name);
	void find_frames(Object *ob, std::vector<float> &fra);

	void find_rotation_frames(Object *ob, std::vector<float> &fra, const char *prefix, int rotmode);
	
	// enable fcurves driving a specific bone, disable all the rest
	// if bone_name = NULL enable all fcurves
	void enable_fcurves(bAction *act, char *bone_name);
	
	bool hasAnimations(Scene *sce);
	
	char* extract_transform_name(char *rna_path);

	std::string getObjectBoneName ( Object *ob,const FCurve * fcu);
};
