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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/TransformReader.cpp
 *  \ingroup collada
 */

/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "TransformReader.h"

TransformReader::TransformReader(UnitConverter* conv) : unit_converter(conv) {}

void TransformReader::get_node_mat(float mat[][4], COLLADAFW::Node *node, std::map<COLLADAFW::UniqueId, Animation> *animation_map, Object *ob)
{
	float cur[4][4];
	float copy[4][4];

	unit_m4(mat);
	
	for (unsigned int i = 0; i < node->getTransformations().getCount(); i++) {

		COLLADAFW::Transformation *tm = node->getTransformations()[i];
		COLLADAFW::Transformation::TransformationType type = tm->getTransformationType();

			switch (type) {
			case COLLADAFW::Transformation::TRANSLATE:
				dae_translate_to_mat4(tm, cur);
				break;
			case COLLADAFW::Transformation::ROTATE:
				dae_rotate_to_mat4(tm, cur);
				break;
			case COLLADAFW::Transformation::SCALE:
				dae_scale_to_mat4(tm, cur);
				break;
			case COLLADAFW::Transformation::MATRIX:
				dae_matrix_to_mat4(tm, cur);
				break;
			case COLLADAFW::Transformation::LOOKAT:
			case COLLADAFW::Transformation::SKEW:
				fprintf(stderr, "LOOKAT and SKEW transformations are not supported yet.\n");
				break;
			}

		copy_m4_m4(copy, mat);
		mult_m4_m4m4(mat, copy, cur);

		if (animation_map) {
			// AnimationList that drives this Transformation
			const COLLADAFW::UniqueId& anim_list_id = tm->getAnimationList();
		
			// store this so later we can link animation data with ob
			Animation anim = {ob, node, tm};
			(*animation_map)[anim_list_id] = anim;
		}
	}
}

void TransformReader::dae_rotate_to_mat4(COLLADAFW::Transformation *tm, float m[][4])
{
	COLLADAFW::Rotate *ro = (COLLADAFW::Rotate*)tm;
	COLLADABU::Math::Vector3& axis = ro->getRotationAxis();
	const float angle = (float)DEG2RAD(ro->getRotationAngle());
	const float ax[] = {axis[0], axis[1], axis[2]};
	// float quat[4];
	// axis_angle_to_quat(quat, axis, angle);
	// quat_to_mat4(m, quat);
	axis_angle_to_mat4(m, ax, angle);
}

void TransformReader::dae_translate_to_mat4(COLLADAFW::Transformation *tm, float m[][4])
{
	COLLADAFW::Translate *tra = (COLLADAFW::Translate*)tm;
	COLLADABU::Math::Vector3& t = tra->getTranslation();

	unit_m4(m);

	m[3][0] = (float)t[0];
	m[3][1] = (float)t[1];
	m[3][2] = (float)t[2];
}

void TransformReader::dae_scale_to_mat4(COLLADAFW::Transformation *tm, float m[][4])
{
	COLLADABU::Math::Vector3& s = ((COLLADAFW::Scale*)tm)->getScale();
	float size[3] = {(float)s[0], (float)s[1], (float)s[2]};
	size_to_mat4(m, size);
}

void TransformReader::dae_matrix_to_mat4(COLLADAFW::Transformation *tm, float m[][4])
{
	unit_converter->dae_matrix_to_mat4_(m, ((COLLADAFW::Matrix*)tm)->getMatrix());
}

void TransformReader::dae_translate_to_v3(COLLADAFW::Transformation *tm, float v[3])
{
	dae_vector3_to_v3(((COLLADAFW::Translate*)tm)->getTranslation(), v);
}

void TransformReader::dae_scale_to_v3(COLLADAFW::Transformation *tm, float v[3])
{
	dae_vector3_to_v3(((COLLADAFW::Scale*)tm)->getScale(), v);
}

void TransformReader::dae_vector3_to_v3(const COLLADABU::Math::Vector3 &v3, float v[3])
{
	v[0] = v3.x;
	v[1] = v3.y;
	v[2] = v3.z;
}
