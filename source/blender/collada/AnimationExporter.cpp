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

#include "GeometryExporter.h"
#include "AnimationExporter.h"
#include "MaterialExporter.h"

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

void AnimationExporter::exportAnimations(Scene *sce)
{
	if (hasAnimations(sce)) {
		this->scene = sce;

		openLibrary();

		forEachObjectInScene(sce, *this);

		closeLibrary();
	}
}

// called for each exported object
void AnimationExporter::operator() (Object *ob) 
{
	FCurve *fcu;
	char *transformName;
	/* bool isMatAnim = false; */ /* UNUSED */

	//Export transform animations
	if (ob->adt && ob->adt->action)
	{
		fcu = (FCurve*)ob->adt->action->curves.first;

		//transform matrix export for bones are temporarily disabled here.
		if ( ob->type == OB_ARMATURE )
		{
			bArmature *arm = (bArmature*)ob->data;
			for (Bone *bone = (Bone*)arm->bonebase.first; bone; bone = bone->next)
				write_bone_animation_matrix(ob, bone);
		}

		while (fcu) {
			//for armature animations as objects
			if ( ob->type == OB_ARMATURE )
				transformName =  fcu->rna_path;
			else 
				transformName = extract_transform_name( fcu->rna_path );

			if ((!strcmp(transformName, "location") || !strcmp(transformName, "scale")) ||
				(!strcmp(transformName, "rotation_euler") && ob->rotmode == ROT_MODE_EUL)||
				(!strcmp(transformName, "rotation_quaternion"))) 
				dae_animation(ob ,fcu, transformName, false);
			fcu = fcu->next;
		}

	}

	//Export Lamp parameter animations
	if ( (ob->type == OB_LAMP ) && ((Lamp*)ob ->data)->adt && ((Lamp*)ob ->data)->adt->action )
	{
		fcu = (FCurve*)(((Lamp*)ob ->data)->adt->action->curves.first);
		while (fcu) {
			transformName = extract_transform_name( fcu->rna_path );

			if ((!strcmp(transformName, "color")) || (!strcmp(transformName, "spot_size"))|| (!strcmp(transformName, "spot_blend"))||
				(!strcmp(transformName, "distance")) ) 
				dae_animation(ob , fcu, transformName, true );
			fcu = fcu->next;
		}
	}

	//Export Camera parameter animations
	if ( (ob->type == OB_CAMERA ) && ((Camera*)ob ->data)->adt && ((Camera*)ob ->data)->adt->action )
	{		
		fcu = (FCurve*)(((Camera*)ob ->data)->adt->action->curves.first);
		while (fcu) {
			transformName = extract_transform_name( fcu->rna_path );

			if ((!strcmp(transformName, "lens"))||
				(!strcmp(transformName, "ortho_scale"))||
				(!strcmp(transformName, "clip_end"))||(!strcmp(transformName, "clip_start"))) 
				dae_animation(ob , fcu, transformName, true );
			fcu = fcu->next;
		}
	}

	//Export Material parameter animations.
	for (int a = 0; a < ob->totcol; a++)
	{
		Material *ma = give_current_material(ob, a+1);
		if (!ma) continue;
		if (ma->adt && ma->adt->action)
		{
			/* isMatAnim = true; */
			fcu = (FCurve*)ma->adt->action->curves.first;
			while (fcu) {
				transformName = extract_transform_name( fcu->rna_path );

				if ((!strcmp(transformName, "specular_hardness"))||(!strcmp(transformName, "specular_color"))
					||(!strcmp(transformName, "diffuse_color"))||(!strcmp(transformName, "alpha"))||
					(!strcmp(transformName, "ior"))) 
					dae_animation(ob ,fcu, transformName, true, ma );
				fcu = fcu->next;
			}
		}

	}
}

//euler sources from quternion sources
float * AnimationExporter::get_eul_source_for_quat(Object *ob )
{
	FCurve *fcu = (FCurve*)ob->adt->action->curves.first;
	const int keys = fcu->totvert;  
	float *quat = (float*)MEM_callocN(sizeof(float) * fcu->totvert * 4, "quat output source values");  
	float *eul = (float*)MEM_callocN(sizeof(float) * fcu->totvert * 3, "quat output source values");
	float temp_quat[4];
	float temp_eul[3];
	while(fcu)
	{
		char * transformName = extract_transform_name( fcu->rna_path );

		if ( !strcmp(transformName, "rotation_quaternion") )	{ 
			for ( int i = 0 ; i < fcu->totvert ; i++){
				*(quat + ( i * 4 ) + fcu->array_index) = fcu->bezt[i].vec[1][1];
			}
		}
		fcu = fcu->next;
	}

	for ( int i = 0 ; i < keys ; i++){
		for ( int j = 0;j<4;j++)
			temp_quat[j] = quat[(i*4)+j];

		quat_to_eul(temp_eul,temp_quat);

		for (int k = 0;k<3;k++)
			eul[i*3 + k] = temp_eul[k];

	}
	MEM_freeN(quat);
	return eul;

}

//Get proper name for bones
std::string AnimationExporter::getObjectBoneName( Object* ob,const FCurve* fcu ) 
{
	//hard-way to derive the bone name from rna_path. Must find more compact method
	std::string rna_path = std::string(fcu->rna_path);

	char* boneName = strtok((char *)rna_path.c_str(), "\"");
	boneName = strtok(NULL,"\"");

	if ( boneName != NULL )
		return /*id_name(ob) + "_" +*/ std::string(boneName);
	else		
		return id_name(ob);
}

//convert f-curves to animation curves and write
void AnimationExporter::dae_animation(Object* ob, FCurve *fcu, char* transformName , bool is_param, Material * ma )
{
	const char *axis_name = NULL;
	char anim_id[200];

	bool has_tangents = false;
	bool quatRotation = false;

	if ( !strcmp(transformName, "rotation_quaternion") )
	{
		fprintf(stderr, "quaternion rotation curves are not supported. rotation curve will not be exported\n");
		quatRotation = true;
		return;
	}

	//axis names for colors
	else if ( !strcmp(transformName, "color")||!strcmp(transformName, "specular_color")||!strcmp(transformName, "diffuse_color")||
		(!strcmp(transformName, "alpha")))
	{
		const char *axis_names[] = {"R", "G", "B"};
		if (fcu->array_index < 3)
			axis_name = axis_names[fcu->array_index];
	}

	//axis names for transforms
	else if ((!strcmp(transformName, "location") || !strcmp(transformName, "scale")) ||
		(!strcmp(transformName, "rotation_euler"))||(!strcmp(transformName, "rotation_quaternion")))
	{
		const char *axis_names[] = {"X", "Y", "Z"};
		if (fcu->array_index < 3)
			axis_name = axis_names[fcu->array_index];
	}

	//no axis name. single parameter.
	else{
		axis_name = "";
	}

	std::string ob_name = std::string("null");

	//Create anim Id
	if (ob->type == OB_ARMATURE) 
	{   
		ob_name =  getObjectBoneName( ob , fcu);
		BLI_snprintf(anim_id, sizeof(anim_id), "%s_%s.%s", (char*)translate_id(ob_name).c_str(),
			transformName, axis_name);
	}
	else 
	{
		if (ma)
			ob_name = id_name(ob) + "_material";
		else
			ob_name = id_name(ob);
		BLI_snprintf(anim_id, sizeof(anim_id), "%s_%s_%s", (char*)translate_id(ob_name).c_str(),
			fcu->rna_path, axis_name);
	}

	openAnimation(anim_id, COLLADABU::Utils::EMPTY_STRING);

	// create input source
	std::string input_id = create_source_from_fcurve(COLLADASW::InputSemantic::INPUT, fcu, anim_id, axis_name);

	// create output source
	std::string output_id;

	//quat rotations are skipped for now, because of complications with determining axis.
	if (quatRotation) 
	{
		float * eul  = get_eul_source_for_quat(ob);
		float * eul_axis = (float*)MEM_callocN(sizeof(float) * fcu->totvert, "quat output source values");
		for ( int i = 0 ; i< fcu->totvert ; i++)
			eul_axis[i] = eul[i*3 + fcu->array_index];
		output_id= create_source_from_array(COLLADASW::InputSemantic::OUTPUT, eul_axis , fcu->totvert, quatRotation, anim_id, axis_name);
		MEM_freeN(eul);
		MEM_freeN(eul_axis);
	}
	else 
	{
		output_id= create_source_from_fcurve(COLLADASW::InputSemantic::OUTPUT, fcu, anim_id, axis_name);
	}
	// create interpolations source
	std::string interpolation_id = create_interpolation_source(fcu, anim_id, axis_name, &has_tangents);

	// handle tangents (if required)
	std::string intangent_id;
	std::string outtangent_id;

	if (has_tangents) {
		// create in_tangent source
		intangent_id = create_source_from_fcurve(COLLADASW::InputSemantic::IN_TANGENT, fcu, anim_id, axis_name);

		// create out_tangent source
		outtangent_id = create_source_from_fcurve(COLLADASW::InputSemantic::OUT_TANGENT, fcu, anim_id, axis_name);
	}

	std::string sampler_id = std::string(anim_id) + SAMPLER_ID_SUFFIX;
	COLLADASW::LibraryAnimations::Sampler sampler(sw, sampler_id);
	std::string empty;
	sampler.addInput(COLLADASW::InputSemantic::INPUT, COLLADABU::URI(empty, input_id));
	sampler.addInput(COLLADASW::InputSemantic::OUTPUT, COLLADABU::URI(empty, output_id));

	// this input is required
	sampler.addInput(COLLADASW::InputSemantic::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

	if (has_tangents) {
		sampler.addInput(COLLADASW::InputSemantic::IN_TANGENT, COLLADABU::URI(empty, intangent_id));
		sampler.addInput(COLLADASW::InputSemantic::OUT_TANGENT, COLLADABU::URI(empty, outtangent_id));
	}

	addSampler(sampler);

	std::string target;

	if ( !is_param )
		target = translate_id(ob_name)
		+ "/" + get_transform_sid(fcu->rna_path, -1, axis_name, true);
	else 
	{
		if ( ob->type == OB_LAMP )
			target = get_light_id(ob)
			+ "/" + get_light_param_sid(fcu->rna_path, -1, axis_name, true);

		if ( ob->type == OB_CAMERA )
			target = get_camera_id(ob)
			+ "/" + get_camera_param_sid(fcu->rna_path, -1, axis_name, true);

		if ( ma ) 
			target = translate_id(id_name(ma)) + "-effect"
			+"/common/" /*profile common is only supported */ + get_transform_sid(fcu->rna_path, -1, axis_name, true);
	}
	addChannel(COLLADABU::URI(empty, sampler_id), target);

	closeAnimation();
}



//write bone animations in transform matrix sources
void AnimationExporter::write_bone_animation_matrix(Object *ob_arm, Bone *bone)
{
	if (!ob_arm->adt)
		return;

	//This will only export animations of bones in deform group.
	/*if(!is_bone_deform_group(bone))
	return;*/

	sample_and_write_bone_animation_matrix(ob_arm, bone);

	for (Bone *child = (Bone*)bone->childbase.first; child; child = child->next)
		write_bone_animation_matrix(ob_arm, child);
}

bool AnimationExporter::is_bone_deform_group(Bone * bone)
{   
	bool is_def;
	//Check if current bone is deform
	if ((bone->flag & BONE_NO_DEFORM) == 0 ) return true;
	//Check child bones
	else 
	{   
		for (Bone *child = (Bone*)bone->childbase.first; child; child = child->next){
			//loop through all the children until deform bone is found, and then return
			is_def = is_bone_deform_group(child);
			if (is_def) return true;
		}
	}
	//no deform bone found in children also
	return false;
}

void AnimationExporter::sample_and_write_bone_animation_matrix(Object *ob_arm, Bone *bone)
{
	bArmature *arm = (bArmature*)ob_arm->data;
	int flag = arm->flag;
	std::vector<float> fra;
	//char prefix[256];

	FCurve* fcu = (FCurve*)ob_arm->adt->action->curves.first;
	while(fcu)
	{
		std::string bone_name = getObjectBoneName(ob_arm,fcu);
		int val = BLI_strcasecmp((char*)bone_name.c_str(),bone->name);
		if (val==0) break;
		fcu = fcu->next;
	}

	if (!(fcu)) return; 
	bPoseChannel *pchan = get_pose_channel(ob_arm->pose, bone->name);
	if (!pchan)
		return;

	find_frames(ob_arm, fra);

	if (flag & ARM_RESTPOS) {
		arm->flag &= ~ARM_RESTPOS;
		where_is_pose(scene, ob_arm);
	}

	if (fra.size()) {
		dae_baked_animation(fra ,ob_arm, bone );
	}

	if (flag & ARM_RESTPOS) 
		arm->flag = flag;
	where_is_pose(scene, ob_arm);
}

void AnimationExporter::dae_baked_animation(std::vector<float> &fra, Object *ob_arm , Bone *bone)
{
	std::string ob_name = id_name(ob_arm);
	std::string bone_name = bone->name;
	char anim_id[200];

	if (!fra.size())
		return;

	BLI_snprintf(anim_id, sizeof(anim_id), "%s_%s_%s", (char*)translate_id(ob_name).c_str(),
		(char*)translate_id(bone_name).c_str(), "pose_matrix");

	openAnimation(anim_id, COLLADABU::Utils::EMPTY_STRING);

	// create input source
	std::string input_id = create_source_from_vector(COLLADASW::InputSemantic::INPUT, fra, false, anim_id, "");

	// create output source
	std::string output_id;
	output_id = create_4x4_source( fra, ob_arm , bone ,  anim_id);

	// create interpolations source
	std::string interpolation_id = fake_interpolation_source(fra.size(), anim_id, "");

	std::string sampler_id = std::string(anim_id) + SAMPLER_ID_SUFFIX;
	COLLADASW::LibraryAnimations::Sampler sampler(sw, sampler_id);
	std::string empty;
	sampler.addInput(COLLADASW::InputSemantic::INPUT, COLLADABU::URI(empty, input_id));
	sampler.addInput(COLLADASW::InputSemantic::OUTPUT, COLLADABU::URI(empty, output_id));

	// TODO create in/out tangents source

	// this input is required
	sampler.addInput(COLLADASW::InputSemantic::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

	addSampler(sampler);

	std::string target = translate_id(bone_name) + "/transform";
	addChannel(COLLADABU::URI(empty, sampler_id), target);

	closeAnimation();
}

// dae_bone_animation -> add_bone_animation
// (blend this into dae_bone_animation)
void AnimationExporter::dae_bone_animation(std::vector<float> &fra, float *values, int tm_type, int axis, std::string ob_name, std::string bone_name)
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
	std::string input_id = create_source_from_vector(COLLADASW::InputSemantic::INPUT, fra, is_rot, anim_id, axis_name);

	// create output source
	std::string output_id;
	if (axis == -1)
		output_id = create_xyz_source(values, fra.size(), anim_id);
	else
		output_id = create_source_from_array(COLLADASW::InputSemantic::OUTPUT, values, fra.size(), is_rot, anim_id, axis_name);

	// create interpolations source
	std::string interpolation_id = fake_interpolation_source(fra.size(), anim_id, axis_name);

	std::string sampler_id = std::string(anim_id) + SAMPLER_ID_SUFFIX;
	COLLADASW::LibraryAnimations::Sampler sampler(sw, sampler_id);
	std::string empty;
	sampler.addInput(COLLADASW::InputSemantic::INPUT, COLLADABU::URI(empty, input_id));
	sampler.addInput(COLLADASW::InputSemantic::OUTPUT, COLLADABU::URI(empty, output_id));

	// TODO create in/out tangents source

	// this input is required
	sampler.addInput(COLLADASW::InputSemantic::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

	addSampler(sampler);

	std::string target = translate_id(ob_name + "_" + bone_name) + "/" + transform_sid;
	addChannel(COLLADABU::URI(empty, sampler_id), target);

	closeAnimation();
}

float AnimationExporter::convert_time(float frame)
{
	return FRA2TIME(frame);
}

float AnimationExporter::convert_angle(float angle)
{
	return COLLADABU::Math::Utils::radToDegF(angle);
}

std::string AnimationExporter::get_semantic_suffix(COLLADASW::InputSemantic::Semantics semantic)
{
	switch(semantic) {
		case COLLADASW::InputSemantic::INPUT:
			return INPUT_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::OUTPUT:
			return OUTPUT_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::INTERPOLATION:
			return INTERPOLATION_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::IN_TANGENT:
			return INTANGENT_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::OUT_TANGENT:
			return OUTTANGENT_SOURCE_ID_SUFFIX;
		default:
			break;
	}
	return "";
}

void AnimationExporter::add_source_parameters(COLLADASW::SourceBase::ParameterNameList& param,
											  COLLADASW::InputSemantic::Semantics semantic, bool is_rot, const char *axis, bool transform)
{
	switch(semantic) {
		case COLLADASW::InputSemantic::INPUT:
			param.push_back("TIME");
			break;
		case COLLADASW::InputSemantic::OUTPUT:
			if (is_rot) {
				param.push_back("ANGLE");
			}
			else {
				if (axis) {
					param.push_back(axis);
				}
				else 
					if ( transform )
					{
						param.push_back("TRANSFORM");  
					}
					else { //assumes if axis isn't specified all axises are added
						param.push_back("X");
						param.push_back("Y");
						param.push_back("Z");
					}
			}
			break;
		case COLLADASW::InputSemantic::IN_TANGENT:
		case COLLADASW::InputSemantic::OUT_TANGENT:
			param.push_back("X");
			param.push_back("Y");
			break;
		default:
			break;
	}
}

void AnimationExporter::get_source_values(BezTriple *bezt, COLLADASW::InputSemantic::Semantics semantic, bool rotation, float *values, int *length)
{
	switch (semantic) {
		case COLLADASW::InputSemantic::INPUT:
			*length = 1;
			values[0] = convert_time(bezt->vec[1][0]);
			break;
		case COLLADASW::InputSemantic::OUTPUT:
			*length = 1;
			if (rotation) {
				values[0] = RAD2DEGF(bezt->vec[1][1]);
			}
			else {
				values[0] = bezt->vec[1][1];
			}
			break;

		case COLLADASW::InputSemantic::IN_TANGENT:
			*length = 2;
			values[0] = convert_time(bezt->vec[0][0]);
			if (bezt->ipo != BEZT_IPO_BEZ) {
				// We're in a mixed interpolation scenario, set zero as it's irrelevant but value might contain unused data
				values[0] = 0;	
				values[1] = 0; 	
			}
			else if (rotation) {
				values[1] = RAD2DEGF(bezt->vec[0][1]);
			}
			else {
				values[1] = bezt->vec[0][1];
			}
			break;

		case COLLADASW::InputSemantic::OUT_TANGENT:
			*length = 2;
			values[0] = convert_time(bezt->vec[2][0]);
			if (bezt->ipo != BEZT_IPO_BEZ) {
				// We're in a mixed interpolation scenario, set zero as it's irrelevant but value might contain unused data
				values[0] = 0;	
				values[1] = 0;	
			}
			else if (rotation) {
				values[1] = RAD2DEGF(bezt->vec[2][1]);
			}
			else {
				values[1] = bezt->vec[2][1];
			}
			break;
			break;
		default:
			*length = 0;
			break;
	}
}

std::string AnimationExporter::create_source_from_fcurve(COLLADASW::InputSemantic::Semantics semantic, FCurve *fcu, const std::string& anim_id, const char *axis_name)
{
	std::string source_id = anim_id + get_semantic_suffix(semantic);

	//bool is_rotation = !strcmp(fcu->rna_path, "rotation");
	bool is_angle = false;

	if (strstr(fcu->rna_path, "rotation")) is_angle = true;

	COLLADASW::FloatSourceF source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(fcu->totvert);

	switch (semantic) {
		case COLLADASW::InputSemantic::INPUT:
		case COLLADASW::InputSemantic::OUTPUT:
			source.setAccessorStride(1);			
			break;
		case COLLADASW::InputSemantic::IN_TANGENT:
		case COLLADASW::InputSemantic::OUT_TANGENT:
			source.setAccessorStride(2);			
			break;
		default:
			break;
	}


	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	add_source_parameters(param, semantic, is_angle, axis_name, false);

	source.prepareToAppendValues();

	for (unsigned int i = 0; i < fcu->totvert; i++) {
		float values[3]; // be careful!
		int length = 0;
		get_source_values(&fcu->bezt[i], semantic, is_angle, values, &length);
		for (int j = 0; j < length; j++)
			source.appendValues(values[j]);
	}

	source.finish();

	return source_id;
}

//Currently called only to get OUTPUT source values ( if rotation and hence the axis is also specified )
std::string AnimationExporter::create_source_from_array(COLLADASW::InputSemantic::Semantics semantic, float *v, int tot, bool is_rot, const std::string& anim_id, const char *axis_name)
{
	std::string source_id = anim_id + get_semantic_suffix(semantic);

	COLLADASW::FloatSourceF source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(tot);
	source.setAccessorStride(1);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	add_source_parameters(param, semantic, is_rot, axis_name,  false);

	source.prepareToAppendValues();

	for (int i = 0; i < tot; i++) {
		float val = v[i];
		////if (semantic == COLLADASW::InputSemantic::INPUT)
		//	val = convert_time(val);
		//else
		if (is_rot)
			val = RAD2DEGF(val);
		source.appendValues(val);
	}

	source.finish();

	return source_id;
}
// only used for sources with INPUT semantic
std::string AnimationExporter::create_source_from_vector(COLLADASW::InputSemantic::Semantics semantic, std::vector<float> &fra, bool is_rot, const std::string& anim_id, const char *axis_name)
{
	std::string source_id = anim_id + get_semantic_suffix(semantic);

	COLLADASW::FloatSourceF source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(fra.size());
	source.setAccessorStride(1);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	add_source_parameters(param, semantic, is_rot, axis_name, false);

	source.prepareToAppendValues();

	std::vector<float>::iterator it;
	for (it = fra.begin(); it != fra.end(); it++) {
		float val = *it;
		//if (semantic == COLLADASW::InputSemantic::INPUT)
		val = convert_time(val);
		/*else if (is_rot)
		val = convert_angle(val);*/
		source.appendValues(val);
	}

	source.finish();

	return source_id;
}

std::string AnimationExporter::create_4x4_source(std::vector<float> &frames , Object * ob_arm, Bone *bone , const std::string& anim_id)
{
	COLLADASW::InputSemantic::Semantics semantic = COLLADASW::InputSemantic::OUTPUT;
	std::string source_id = anim_id + get_semantic_suffix(semantic);

	COLLADASW::Float4x4Source source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(frames.size());
	source.setAccessorStride(16);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	add_source_parameters(param, semantic, false, NULL, true);

	source.prepareToAppendValues();

	bPoseChannel *parchan = NULL;
	bPoseChannel *pchan = NULL;
	bPose *pose = ob_arm->pose;

	pchan = get_pose_channel(pose, bone->name);

	if (!pchan)
		return "";

	parchan = pchan->parent;

	enable_fcurves(ob_arm->adt->action, bone->name);

	std::vector<float>::iterator it;
	int j = 0;
	for (it = frames.begin(); it != frames.end(); it++) {
		float mat[4][4], ipar[4][4];

		float ctime = BKE_frame_to_ctime(scene, *it);

		BKE_animsys_evaluate_animdata(scene , &ob_arm->id, ob_arm->adt, ctime, ADT_RECALC_ANIM);
		where_is_pose_bone(scene, ob_arm, pchan, ctime, 1);

		// compute bone local mat
		if (bone->parent) {
			invert_m4_m4(ipar, parchan->pose_mat);
			mult_m4_m4m4(mat, ipar, pchan->pose_mat);
		}
		else
			copy_m4_m4(mat, pchan->pose_mat);
		UnitConverter converter;

		// SECOND_LIFE_COMPATIBILITY
		// AFAIK animation to second life is via BVH, but no
		// reason to not have the collada-animation be correct
		if (export_settings->second_life)
		{
			float temp[4][4];
			copy_m4_m4(temp, bone->arm_mat);
			temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;
			invert_m4(temp);

			mult_m4_m4m4(mat, mat, temp);

			if (bone->parent)
			{
				copy_m4_m4(temp, bone->parent->arm_mat);
				temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;

				mult_m4_m4m4(mat, temp, mat);
			}
		}

		float outmat[4][4];
		converter.mat4_to_dae(outmat,mat);


		source.appendValues(outmat);


		j++;
	}

	enable_fcurves(ob_arm->adt->action, NULL);

	source.finish();

	return source_id;
}
// only used for sources with OUTPUT semantic ( locations and scale)
std::string AnimationExporter::create_xyz_source(float *v, int tot, const std::string& anim_id)
{
	COLLADASW::InputSemantic::Semantics semantic = COLLADASW::InputSemantic::OUTPUT;
	std::string source_id = anim_id + get_semantic_suffix(semantic);

	COLLADASW::FloatSourceF source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(tot);
	source.setAccessorStride(3);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	add_source_parameters(param, semantic, false, NULL, false);

	source.prepareToAppendValues();

	for (int i = 0; i < tot; i++) {
		source.appendValues(*v, *(v + 1), *(v + 2));
		v += 3;
	}

	source.finish();

	return source_id;
}

std::string AnimationExporter::create_interpolation_source(FCurve *fcu, const std::string& anim_id, const char *axis_name, bool *has_tangents)
{
	std::string source_id = anim_id + get_semantic_suffix(COLLADASW::InputSemantic::INTERPOLATION);

	COLLADASW::NameSource source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(fcu->totvert);
	source.setAccessorStride(1);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("INTERPOLATION");

	source.prepareToAppendValues();

	*has_tangents = false;

	for (unsigned int i = 0; i < fcu->totvert; i++) {
		if (fcu->bezt[i].ipo==BEZT_IPO_BEZ) {
			source.appendValues(BEZIER_NAME);
			*has_tangents = true;
		}
		else if (fcu->bezt[i].ipo==BEZT_IPO_CONST) {
			source.appendValues(STEP_NAME);
		}
		else { // BEZT_IPO_LIN
			source.appendValues(LINEAR_NAME);
		}
	}
	// unsupported? -- HERMITE, CARDINAL, BSPLINE, NURBS

	source.finish();

	return source_id;
}

std::string AnimationExporter::fake_interpolation_source(int tot, const std::string& anim_id, const char *axis_name)
{
	std::string source_id = anim_id + get_semantic_suffix(COLLADASW::InputSemantic::INTERPOLATION);

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

std::string AnimationExporter::get_light_param_sid(char *rna_path, int tm_type, const char *axis_name, bool append_axis)
{
	std::string tm_name;
	// when given rna_path, determine tm_type from it
	if (rna_path) {
		char *name = extract_transform_name(rna_path);

		if (!strcmp(name, "color"))
			tm_type = 1;
		else if (!strcmp(name, "spot_size"))
			tm_type = 2;
		else if (!strcmp(name, "spot_blend"))
			tm_type = 3;
		else if (!strcmp(name, "distance"))
			tm_type = 4;
		else
			tm_type = -1;
	}

	switch (tm_type) {
		case 1:
			tm_name = "color";
			break;
		case 2:
			tm_name = "fall_off_angle";
			break;
		case 3:
			tm_name = "fall_off_exponent";
			break;
		case 4:
			tm_name = "blender/blender_dist";
			break;

		default:
			tm_name = "";
			break;
	}

	if (tm_name.size()) {
		if (axis_name[0])
			return tm_name + "." + std::string(axis_name);
		else 
			return tm_name;
	}

	return std::string("");
}

std::string AnimationExporter::get_camera_param_sid(char *rna_path, int tm_type, const char *axis_name, bool append_axis)
{
	std::string tm_name;
	// when given rna_path, determine tm_type from it
	if (rna_path) {
		char *name = extract_transform_name(rna_path);

		if (!strcmp(name, "lens"))
			tm_type = 0;
		else if (!strcmp(name, "ortho_scale"))
			tm_type = 1;
		else if (!strcmp(name, "clip_end"))
			tm_type = 2;
		else if (!strcmp(name, "clip_start"))
			tm_type = 3;

		else
			tm_type = -1;
	}

	switch (tm_type) {
		case 0:
			tm_name = "xfov";
			break;
		case 1:
			tm_name = "xmag";
			break;
		case 2:
			tm_name = "zfar";
			break;
		case 3:
			tm_name = "znear";
			break;

		default:
			tm_name = "";
			break;
	}

	if (tm_name.size()) {
		if (axis_name[0])
			return tm_name + "." + std::string(axis_name);
		else 
			return tm_name;
	}

	return std::string("");
}

// Assign sid of the animated parameter or transform 
// for rotation, axis name is always appended and the value of append_axis is ignored
std::string AnimationExporter::get_transform_sid(char *rna_path, int tm_type, const char *axis_name, bool append_axis)
{
	std::string tm_name;
	bool is_rotation =false;
	// when given rna_path, determine tm_type from it
	if (rna_path) {
		char *name = extract_transform_name(rna_path);

		if (!strcmp(name, "rotation_euler"))
			tm_type = 0;
		else if (!strcmp(name, "rotation_quaternion"))
			tm_type = 1;
		else if (!strcmp(name, "scale"))
			tm_type = 2;
		else if (!strcmp(name, "location"))
			tm_type = 3;
		else if (!strcmp(name, "specular_hardness"))
			tm_type = 4;
		else if (!strcmp(name, "specular_color"))
			tm_type = 5;
		else if (!strcmp(name, "diffuse_color"))
			tm_type = 6;
		else if (!strcmp(name, "alpha"))
			tm_type = 7;
		else if (!strcmp(name, "ior"))
			tm_type = 8;

		else
			tm_type = -1;
	}

	switch (tm_type) {
		case 0:
		case 1:
			tm_name = "rotation";
			is_rotation = true;
			break;
		case 2:
			tm_name = "scale";
			break;
		case 3:
			tm_name = "location";
			break;
		case 4:
			tm_name = "shininess";
			break;
		case 5:
			tm_name = "specular";
			break;
		case 6:
			tm_name = "diffuse";
			break;	
		case 7:
			tm_name = "transparency";
			break;	
		case 8:
			tm_name = "index_of_refraction";
			break;	

		default:
			tm_name = "";
			break;
	}

	if (tm_name.size()) {
		if (is_rotation)
			return tm_name + std::string(axis_name) + ".ANGLE";
		else
			if (axis_name[0])
				return tm_name + "." + std::string(axis_name);
			else 
				return tm_name;
	}

	return std::string("");
}

char* AnimationExporter::extract_transform_name(char *rna_path)
{
	char *dot = strrchr(rna_path, '.');
	return dot ? (dot + 1) : rna_path;
}

//find keyframes of all the objects animations
void AnimationExporter::find_frames(Object *ob, std::vector<float> &fra)
{
	FCurve *fcu= (FCurve*)ob->adt->action->curves.first;

	for (; fcu; fcu = fcu->next) {

		for (unsigned int i = 0; i < fcu->totvert; i++) {
			float f = fcu->bezt[i].vec[1][0];
			if (std::find(fra.begin(), fra.end(), f) == fra.end())   
				fra.push_back(f);
		}
	}

	// keep the keys in ascending order
	std::sort(fra.begin(), fra.end());
}



// enable fcurves driving a specific bone, disable all the rest
// if bone_name = NULL enable all fcurves
void AnimationExporter::enable_fcurves(bAction *act, char *bone_name)
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

bool AnimationExporter::hasAnimations(Scene *sce)
{
	Base *base= (Base*) sce->base.first;

	while(base) {
		Object *ob = base->object;

		FCurve *fcu = 0;
		//Check for object transform animations
		if (ob->adt && ob->adt->action)
			fcu = (FCurve*)ob->adt->action->curves.first;
		//Check for Lamp parameter animations
		else if ( (ob->type == OB_LAMP ) && ((Lamp*)ob ->data)->adt && ((Lamp*)ob ->data)->adt->action )
			fcu = (FCurve*)(((Lamp*)ob ->data)->adt->action->curves.first);
		//Check for Camera parameter animations
		else if ( (ob->type == OB_CAMERA ) && ((Camera*)ob ->data)->adt && ((Camera*)ob ->data)->adt->action )
			fcu = (FCurve*)(((Camera*)ob ->data)->adt->action->curves.first);

		//Check Material Effect parameter animations.
		for (int a = 0; a < ob->totcol; a++)
		{
			Material *ma = give_current_material(ob, a+1);
			if (!ma) continue;
			if (ma->adt && ma->adt->action)
			{
				fcu = (FCurve*)ma->adt->action->curves.first;	
			}
		}

		if ( fcu) 
			return true;
		base= base->next;
	}
	return false;
}

//------------------------------- Not used in the new system.--------------------------------------------------------
void AnimationExporter::find_rotation_frames(Object *ob, std::vector<float> &fra, const char *prefix, int rotmode)
{
	if (rotmode > 0)
		find_frames(ob, fra, prefix, "rotation_euler");
	else if (rotmode == ROT_MODE_QUAT)
		find_frames(ob, fra, prefix, "rotation_quaternion");
	/*else if (rotmode == ROT_MODE_AXISANGLE)
	;*/
}

void AnimationExporter::find_frames(Object *ob, std::vector<float> &fra, const char *prefix, const char *tm_name)
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

	// keep the keys in ascending order
	std::sort(fra.begin(), fra.end());
}

void AnimationExporter::write_bone_animation(Object *ob_arm, Bone *bone)
{
	if (!ob_arm->adt)
		return;

	//write bone animations for 3 transform types
	//i=0 --> rotations
	//i=1 --> scale
	//i=2 --> location
	for (int i = 0; i < 3; i++)
		sample_and_write_bone_animation(ob_arm, bone, i);

	for (Bone *child = (Bone*)bone->childbase.first; child; child = child->next)
		write_bone_animation(ob_arm, child);
}

void AnimationExporter::sample_and_write_bone_animation(Object *ob_arm, Bone *bone, int transform_type)
{
	bArmature *arm = (bArmature*)ob_arm->data;
	int flag = arm->flag;
	std::vector<float> fra;
	char prefix[256];

	BLI_snprintf(prefix, sizeof(prefix), "pose.bones[\"%s\"]", bone->name);

	bPoseChannel *pchan = get_pose_channel(ob_arm->pose, bone->name);
	if (!pchan)
		return;
	//Fill frame array with key frame values framed at \param:transform_type
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
	//v array will hold all values which will be exported. 
	if (fra.size()) {
		float *values = (float*)MEM_callocN(sizeof(float) * 3 * fra.size(), "temp. anim frames");
		sample_animation(values, fra, transform_type, bone, ob_arm, pchan);

		if (transform_type == 0) {
			// write x, y, z curves separately if it is rotation
			float *axisValues = (float*)MEM_callocN(sizeof(float) * fra.size(), "temp. anim frames");   

			for (int i = 0; i < 3; i++) {
				for (unsigned int j = 0; j < fra.size(); j++)
					axisValues[j] = values[j * 3 + i];

				dae_bone_animation(fra, axisValues, transform_type, i, id_name(ob_arm), bone->name);
			}
			MEM_freeN(axisValues);
		}
		else {
			// write xyz at once if it is location or scale
			dae_bone_animation(fra, values, transform_type, -1, id_name(ob_arm), bone->name);
		}

		MEM_freeN(values);
	}

	// restore restpos
	if (flag & ARM_RESTPOS) 
		arm->flag = flag;
	where_is_pose(scene, ob_arm);
}

void AnimationExporter::sample_animation(float *v, std::vector<float> &frames, int type, Bone *bone, Object *ob_arm, bPoseChannel *pchan)
{
	bPoseChannel *parchan = NULL;
	bPose *pose = ob_arm->pose;

	pchan = get_pose_channel(pose, bone->name);

	if (!pchan)
		return;

	parchan = pchan->parent;

	enable_fcurves(ob_arm->adt->action, bone->name);

	std::vector<float>::iterator it;
	for (it = frames.begin(); it != frames.end(); it++) {
		float mat[4][4], ipar[4][4];

		float ctime = BKE_frame_to_ctime(scene, *it);


		BKE_animsys_evaluate_animdata(scene , &ob_arm->id, ob_arm->adt, ctime, ADT_RECALC_ANIM);
		where_is_pose_bone(scene, ob_arm, pchan, ctime, 1);

		// compute bone local mat
		if (bone->parent) {
			invert_m4_m4(ipar, parchan->pose_mat);
			mult_m4_m4m4(mat, ipar, pchan->pose_mat);
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
