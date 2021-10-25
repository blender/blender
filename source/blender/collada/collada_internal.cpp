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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/collada_internal.cpp
 *  \ingroup collada
 */


/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"
#include "collada_utils.h"

#include "BLI_linklist.h"

UnitConverter::UnitConverter() : unit(), up_axis(COLLADAFW::FileInfo::Z_UP)
{
	axis_angle_to_mat4_single(x_up_mat4, 'Y', -0.5 * M_PI);
	axis_angle_to_mat4_single(y_up_mat4, 'X', 0.5 * M_PI);

	unit_m4(z_up_mat4);
	unit_m4(scale_mat4);
}

void UnitConverter::read_asset(const COLLADAFW::FileInfo *asset)
{
	unit = asset->getUnit();
	up_axis = asset->getUpAxisType();
}

UnitConverter::UnitSystem UnitConverter::isMetricSystem()
{
	switch (unit.getLinearUnitUnit()) {
		case COLLADAFW::FileInfo::Unit::MILLIMETER:
		case COLLADAFW::FileInfo::Unit::CENTIMETER:
		case COLLADAFW::FileInfo::Unit::DECIMETER:
		case COLLADAFW::FileInfo::Unit::METER:
		case COLLADAFW::FileInfo::Unit::KILOMETER:
			return UnitConverter::Metric;
		case COLLADAFW::FileInfo::Unit::INCH:
		case COLLADAFW::FileInfo::Unit::FOOT:
		case COLLADAFW::FileInfo::Unit::YARD:
			return UnitConverter::Imperial;
		default:
			return UnitConverter::None;
	}
}

float UnitConverter::getLinearMeter()
{
	return (float)unit.getLinearUnitMeter();
}

void UnitConverter::convertVector3(COLLADABU::Math::Vector3 &vec, float *v)
{
	v[0] = vec.x;
	v[1] = vec.y;
	v[2] = vec.z;
}

// TODO need also for angle conversion, time conversion...

void UnitConverter::dae_matrix_to_mat4_(float out[4][4], const COLLADABU::Math::Matrix4& in)
{
	// in DAE, matrices use columns vectors, (see comments in COLLADABUMathMatrix4.h)
	// so here, to make a blender matrix, we swap columns and rows
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			out[i][j] = in[j][i];
		}
	}
}

void UnitConverter::mat4_to_dae(float out[4][4], float in[4][4])
{
	transpose_m4_m4(out, in);
}

void UnitConverter::mat4_to_dae_double(double out[4][4], float in[4][4])
{
	float mat[4][4];

	mat4_to_dae(mat, in);

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			out[i][j] = mat[i][j];
}

float(&UnitConverter::get_rotation())[4][4]
{
	switch (up_axis) {
		case COLLADAFW::FileInfo::X_UP:
			return x_up_mat4;
			break;
		case COLLADAFW::FileInfo::Y_UP:
			return y_up_mat4;
			break;
		default:
			return z_up_mat4;
			break;
	}
}


float(&UnitConverter::get_scale())[4][4]
{
	return scale_mat4;
}

void UnitConverter::calculate_scale(Scene &sce)
{
	PointerRNA scene_ptr, unit_settings;
	PropertyRNA *system_ptr, *scale_ptr;
	RNA_id_pointer_create(&sce.id, &scene_ptr);

	unit_settings = RNA_pointer_get(&scene_ptr, "unit_settings");
	system_ptr    = RNA_struct_find_property(&unit_settings, "system");
	scale_ptr     = RNA_struct_find_property(&unit_settings, "scale_length");

	int   type    = RNA_property_enum_get(&unit_settings, system_ptr);

	float bl_scale;

	switch (type) {
		case USER_UNIT_NONE:
			bl_scale = 1.0; // map 1 Blender unit to 1 Meter
			break;

		case USER_UNIT_METRIC:
			bl_scale = RNA_property_float_get(&unit_settings, scale_ptr);
			break;

		default :
			bl_scale = RNA_property_float_get(&unit_settings, scale_ptr);
			// it looks like the conversion to Imperial is done implicitly.
			// So nothing to do here.
			break;
	}

	float rescale[3];
	rescale[0] = rescale[1] = rescale[2] = getLinearMeter() / bl_scale;

	size_to_mat4(scale_mat4, rescale);
}

/**
 * Translation map.
 * Used to translate every COLLADA id to a valid id, no matter what "wrong" letters may be
 * included. Look at the IDREF XSD declaration for more.
 * Follows strictly the COLLADA XSD declaration which explicitly allows non-english chars,
 * like special chars (e.g. micro sign), umlauts and so on.
 * The COLLADA spec also allows additional chars for member access ('.'), these
 * must obviously be removed too, otherwise they would be heavily misinterpreted.
 */
const unsigned char translate_start_name_map[256] = {

	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 95, 95, 95, 95, 95, 95, 95,
	95, 65, 66, 67, 68, 69, 70, 71,
	72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87,
	88, 89, 90, 95, 95, 95, 95, 95,
	95, 97, 98, 99, 100, 101, 102, 103,
	104, 105, 106, 107, 108, 109, 110, 111,
	112, 113, 114, 115, 116, 117, 118, 119,
	120, 121, 122, 95, 95, 95, 95, 95,

	128, 129, 130, 131, 132, 133, 134, 135,
	136, 137, 138, 139, 140, 141, 142, 143,
	144, 145, 146, 147, 148, 149, 150, 151,
	152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167,
	168, 169, 170, 171, 172, 173, 174, 175,
	176, 177, 178, 179, 180, 181, 182, 183,
	184, 185, 186, 187, 188, 189, 190, 191,
	192, 193, 194, 195, 196, 197, 198, 199,
	200, 201, 202, 203, 204, 205, 206, 207,
	208, 209, 210, 211, 212, 213, 214, 215,
	216, 217, 218, 219, 220, 221, 222, 223,
	224, 225, 226, 227, 228, 229, 230, 231,
	232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 247,
	248, 249, 250, 251, 252, 253, 254, 255
};

const unsigned char translate_name_map[256] = {

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

	128, 129, 130, 131, 132, 133, 134, 135,
	136, 137, 138, 139, 140, 141, 142, 143,
	144, 145, 146, 147, 148, 149, 150, 151,
	152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167,
	168, 169, 170, 171, 172, 173, 174, 175,
	176, 177, 178, 179, 180, 181, 182, 183,
	184, 185, 186, 187, 188, 189, 190, 191,
	192, 193, 194, 195, 196, 197, 198, 199,
	200, 201, 202, 203, 204, 205, 206, 207,
	208, 209, 210, 211, 212, 213, 214, 215,
	216, 217, 218, 219, 220, 221, 222, 223,
	224, 225, 226, 227, 228, 229, 230, 231,
	232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 247,
	248, 249, 250, 251, 252, 253, 254, 255
};

typedef std::map< std::string, std::vector<std::string> > map_string_list;
map_string_list global_id_map;

void clear_global_id_map()
{
	global_id_map.clear();
}

/** Look at documentation of translate_map */
std::string translate_id(const char *idString)
{
	std::string id = std::string(idString);
	return translate_id(id);
}

std::string translate_id(const std::string &id)
{
	if (id.size() == 0) {
		return id;
	}

	std::string id_translated = id;
	id_translated[0] = translate_start_name_map[(unsigned int)id_translated[0]];
	for (unsigned int i = 1; i < id_translated.size(); i++) {
		id_translated[i] = translate_name_map[(unsigned int)id_translated[i]];
	}
	// It's so much workload now, the if () should speed up things.
	if (id_translated != id) {
		// Search duplicates
		map_string_list::iterator iter = global_id_map.find(id_translated);
		if (iter != global_id_map.end()) {
			unsigned int i = 0;
			bool found = false;
			for (i = 0; i < iter->second.size(); i++) {
				if (id == iter->second[i]) {
					found = true;
					break;
				}
			}
			bool convert = false;
			if (found) {
				if (i > 0) {
					convert = true;
				}
			}
			else {
				convert = true;
				global_id_map[id_translated].push_back(id);
			}
			if (convert) {
				std::stringstream out;
				out << ++i;
				id_translated += out.str();
			}
		}
		else { global_id_map[id_translated].push_back(id); }
	}
	return id_translated;
}

std::string id_name(void *id)
{
	return ((ID *)id)->name + 2;
}

std::string get_geometry_id(Object *ob)
{
	return translate_id(id_name(ob->data)) + "-mesh";
}

std::string get_geometry_id(Object *ob, bool use_instantiation)
{
	std::string geom_name = (use_instantiation) ? id_name(ob->data) : id_name(ob);

	return translate_id(geom_name) + "-mesh";
}

std::string get_light_id(Object *ob)
{
	return translate_id(id_name(ob)) + "-light";
}

std::string get_joint_id(Bone *bone, Object *ob_arm)
{
	return translate_id(id_name(ob_arm) + "_" + bone->name);
}

std::string get_joint_sid(Bone *bone, Object *ob_arm)
{
	return translate_id(bone->name);
}

std::string get_camera_id(Object *ob)
{
	return translate_id(id_name(ob)) + "-camera";
}

std::string get_material_id(Material *mat)
{
	std::string id = id_name(mat);
	return get_material_id_from_id(id);
}

std::string get_material_id_from_id(std::string id)
{
	return translate_id(id) + "-material";
}

std::string get_morph_id(Object *ob)
{
	return translate_id(id_name(ob)) + "-morph";
}

