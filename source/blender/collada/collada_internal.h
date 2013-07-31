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

/** \file collada_internal.h
 *  \ingroup collada
 */

#ifndef __COLLADA_INTERNAL_H__
#define __COLLADA_INTERNAL_H__

#include <string>
#include <vector>
#include <map>

#include "COLLADAFWFileInfo.h"
#include "Math/COLLADABUMathMatrix4.h"

#include "DNA_armature_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "BLI_math.h"
#include "BLI_linklist.h"

class UnitConverter
{
private:
	COLLADAFW::FileInfo::Unit unit;
	COLLADAFW::FileInfo::UpAxisType up_axis;

	float x_up_mat4[4][4];
	float y_up_mat4[4][4];
	float z_up_mat4[4][4];
	float scale_mat4[4][4];
	
public:

	enum UnitSystem {
		None,
		Metric,
		Imperial
	};

	// Initialize with Z_UP, since Blender uses right-handed, z-up
	UnitConverter();

	void read_asset(const COLLADAFW::FileInfo *asset);

	void convertVector3(COLLADABU::Math::Vector3 &vec, float *v);
	
	UnitConverter::UnitSystem isMetricSystem(void);
	
	float getLinearMeter(void);
		
	// TODO need also for angle conversion, time conversion...

	void dae_matrix_to_mat4_(float out[4][4], const COLLADABU::Math::Matrix4& in);

	void mat4_to_dae(float out[4][4], float in[4][4]);

	void mat4_to_dae_double(double out[4][4], float in[4][4]);

	float(&get_rotation())[4][4];
	float(&get_scale())[4][4];
	void calculate_scale(Scene &sce);

};

class TransformBase
{
public:
	void decompose(float mat[4][4], float *loc, float eul[3], float quat[4], float *size);
};

extern void clear_global_id_map();
/** Look at documentation of translate_map */
extern std::string translate_id(const std::string &id);
extern std::string translate_id(const char *idString);

extern std::string id_name(void *id);

extern std::string get_geometry_id(Object *ob);
extern std::string get_geometry_id(Object *ob, bool use_instantiation);

extern std::string get_light_id(Object *ob);

extern std::string get_joint_id(Bone *bone, Object *ob_arm);

extern std::string get_camera_id(Object *ob);

extern std::string get_material_id(Material *mat);

extern std::string get_morph_id(Object *ob);

#endif /* __COLLADA_INTERNAL_H__ */
