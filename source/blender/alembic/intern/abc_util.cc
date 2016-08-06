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
 * Contributor(s): Esteban Tovagliari, Cedric Paille, Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "abc_util.h"

#include <algorithm>

extern "C" {
#include "DNA_object_types.h"

#include "BLI_math.h"
}

std::string get_id_name(Object *ob)
{
	if (!ob) {
		return "";
	}

	return get_id_name(&ob->id);
}

std::string get_id_name(ID *id)
{
	std::string name(id->name + 2);
	std::replace(name.begin(), name.end(), ' ', '_');
	std::replace(name.begin(), name.end(), '.', '_');
	std::replace(name.begin(), name.end(), ':', '_');

	return name;
}

std::string get_object_dag_path_name(Object *ob, Object *dupli_parent)
{
	std::string name = get_id_name(ob);

	Object *p = ob->parent;

	while (p) {
		name = get_id_name(p) + "/" + name;
		p = p->parent;
	}

	if (dupli_parent && (ob != dupli_parent)) {
		name = get_id_name(dupli_parent) + "/" + name;
	}

	return name;
}

bool object_selected(Object *ob)
{
	return ob->flag & SELECT;
}

bool parent_selected(Object *ob)
{
	if (object_selected(ob)) {
		return true;
	}

	bool do_export = false;

	Object *parent = ob->parent;

	while (parent != NULL) {
		if (object_selected(parent)) {
			do_export = true;
			break;
		}

		parent = parent->parent;
	}

	return do_export;
}

Imath::M44d convert_matrix(float mat[4][4])
{
	Imath::M44d m;

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			m[i][j] = mat[i][j];
		}
	}

	return m;
}

void split(const std::string &s, const char delim, std::vector<std::string> &tokens)
{
	tokens.clear();

	std::stringstream ss(s);
	std::string item;

	while (std::getline(ss, item, delim)) {
		if (!item.empty()) {
			tokens.push_back(item);
		}
	}
}

/* Create a rotation matrix for each axis from euler angles.
 * Euler angles are swaped to change coordinate system. */
static void create_rotation_matrix(
        float rot_x_mat[3][3], float rot_y_mat[3][3],
        float rot_z_mat[3][3], const float euler[3], const bool to_yup)
{
	const float rx = euler[0];
	const float ry = (to_yup) ?  euler[2] : -euler[2];
	const float rz = (to_yup) ? -euler[1] :  euler[1];

	unit_m3(rot_x_mat);
	unit_m3(rot_y_mat);
	unit_m3(rot_z_mat);

	rot_x_mat[1][1] = cos(rx);
	rot_x_mat[2][1] = -sin(rx);
	rot_x_mat[1][2] = sin(rx);
	rot_x_mat[2][2] = cos(rx);

	rot_y_mat[2][2] = cos(ry);
	rot_y_mat[0][2] = -sin(ry);
	rot_y_mat[2][0] = sin(ry);
	rot_y_mat[0][0] = cos(ry);

	rot_z_mat[0][0] = cos(rz);
	rot_z_mat[1][0] = -sin(rz);
	rot_z_mat[0][1] = sin(rz);
	rot_z_mat[1][1] = cos(rz);
}

/* Recompute transform matrix of object in new coordinate system
 * (from Y-Up to Z-Up). */
void create_transform_matrix(float r_mat[4][4])
{
	float rot_mat[3][3], rot[3][3], scale_mat[4][4], invmat[4][4], transform_mat[4][4];
	float rot_x_mat[3][3], rot_y_mat[3][3], rot_z_mat[3][3];
	float loc[3], scale[3], euler[3];

	zero_v3(loc);
	zero_v3(scale);
	zero_v3(euler);
	unit_m3(rot);
	unit_m3(rot_mat);
	unit_m4(scale_mat);
	unit_m4(transform_mat);
	unit_m4(invmat);

	/* Compute rotation matrix. */

	/* Extract location, rotation, and scale from matrix. */
	mat4_to_loc_rot_size(loc, rot, scale, r_mat);

	/* Get euler angles from rotation matrix. */
	mat3_to_eulO(euler, ROT_MODE_XYZ, rot);

	/* Create X, Y, Z rotation matrices from euler angles. */
	create_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, false);

	/* Concatenate rotation matrices. */
	mul_m3_m3m3(rot_mat, rot_mat, rot_y_mat);
	mul_m3_m3m3(rot_mat, rot_mat, rot_z_mat);
	mul_m3_m3m3(rot_mat, rot_mat, rot_x_mat);

	/* Add rotation matrix to transformation matrix. */
	copy_m4_m3(transform_mat, rot_mat);

	/* Add translation to transformation matrix. */
	copy_yup_zup(transform_mat[3], loc);

	/* Create scale matrix. */
	scale_mat[0][0] = scale[0];
	scale_mat[1][1] = scale[2];
	scale_mat[2][2] = scale[1];

	/* Add scale to transformation matrix. */
	mul_m4_m4m4(transform_mat, transform_mat, scale_mat);

	copy_m4_m4(r_mat, transform_mat);
}

void create_input_transform(const Alembic::AbcGeom::ISampleSelector &sample_sel,
                            const Alembic::AbcGeom::IXform &ixform, Object *ob,
                            float r_mat[4][4], float scale, bool has_alembic_parent)
{

	const Alembic::AbcGeom::IXformSchema &ixform_schema = ixform.getSchema();
	Alembic::AbcGeom::XformSample xs;
	ixform_schema.get(xs, sample_sel);
	const Imath::M44d &xform = xs.getMatrix();

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			r_mat[i][j] = xform[i][j];
		}
	}

	if (ob->type == OB_CAMERA) {
		float cam_to_yup[4][4];
		unit_m4(cam_to_yup);
		rotate_m4(cam_to_yup, 'X', M_PI_2);
		mul_m4_m4m4(r_mat, r_mat, cam_to_yup);
	}

	create_transform_matrix(r_mat);

	if (ob->parent) {
		mul_m4_m4m4(r_mat, ob->parent->obmat, r_mat);
	}
	/* TODO(kevin) */
	else if (!has_alembic_parent) {
		/* Only apply scaling to root objects, parenting will propagate it. */
		float scale_mat[4][4];
		scale_m4_fl(scale_mat, scale);
		mul_m4_m4m4(r_mat, r_mat, scale_mat);
		mul_v3_fl(r_mat[3], scale);
	}
}

/* Recompute transform matrix of object in new coordinate system (from Z-Up to Y-Up). */
void create_transform_matrix(Object *obj, float transform_mat[4][4])
{
	float rot_mat[3][3], rot[3][3], scale_mat[4][4], invmat[4][4], mat[4][4];
	float rot_x_mat[3][3], rot_y_mat[3][3], rot_z_mat[3][3];
	float loc[3], scale[3], euler[3];

	zero_v3(loc);
	zero_v3(scale);
	zero_v3(euler);
	unit_m3(rot);
	unit_m3(rot_mat);
	unit_m4(scale_mat);
	unit_m4(transform_mat);
	unit_m4(invmat);
	unit_m4(mat);

	/* get local matrix. */
	if (obj->parent) {
		invert_m4_m4(invmat, obj->parent->obmat);
		mul_m4_m4m4(mat, invmat, obj->obmat);
	}
	else {
		copy_m4_m4(mat, obj->obmat);
	}

	/* Compute rotation matrix. */
	switch (obj->rotmode) {
		case ROT_MODE_AXISANGLE:
		{
			/* Get euler angles from axis angle rotation. */
			axis_angle_to_eulO(euler, ROT_MODE_XYZ, obj->rotAxis, obj->rotAngle);

			/* Create X, Y, Z rotation matrices from euler angles. */
			create_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, true);

			/* Concatenate rotation matrices. */
			mul_m3_m3m3(rot_mat, rot_mat, rot_y_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_z_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_x_mat);

			/* Extract location and scale from matrix. */
			mat4_to_loc_rot_size(loc, rot, scale, mat);

			break;
		}
		case ROT_MODE_QUAT:
		{
			float q[4];
			copy_v4_v4(q, obj->quat);

			/* Swap axis. */
			q[2] = obj->quat[3];
			q[3] = -obj->quat[2];

			/* Compute rotation matrix from quaternion. */
			quat_to_mat3(rot_mat, q);

			/* Extract location and scale from matrix. */
			mat4_to_loc_rot_size(loc, rot, scale, mat);

			break;
		}
		case ROT_MODE_XYZ:
		{
			/* Extract location, rotation, and scale form matrix. */
			mat4_to_loc_rot_size(loc, rot, scale, mat);

			/* Get euler angles from rotation matrix. */
			mat3_to_eulO(euler, ROT_MODE_XYZ, rot);

			/* Create X, Y, Z rotation matrices from euler angles. */
			create_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, true);

			/* Concatenate rotation matrices. */
			mul_m3_m3m3(rot_mat, rot_mat, rot_y_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_z_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_x_mat);

			break;
		}
		case ROT_MODE_XZY:
		{
			/* Extract location, rotation, and scale form matrix. */
			mat4_to_loc_rot_size(loc, rot, scale, mat);

			/* Get euler angles from rotation matrix. */
			mat3_to_eulO(euler, ROT_MODE_XZY, rot);

			/* Create X, Y, Z rotation matrices from euler angles. */
			create_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, true);

			/* Concatenate rotation matrices. */
			mul_m3_m3m3(rot_mat, rot_mat, rot_z_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_y_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_x_mat);

			break;
		}
		case ROT_MODE_YXZ:
		{
			/* Extract location, rotation, and scale form matrix. */
			mat4_to_loc_rot_size(loc, rot, scale, mat);

			/* Get euler angles from rotation matrix. */
			mat3_to_eulO(euler, ROT_MODE_YXZ, rot);

			/* Create X, Y, Z rotation matrices from euler angles. */
			create_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, true);

			/* Concatenate rotation matrices. */
			mul_m3_m3m3(rot_mat, rot_mat, rot_y_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_x_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_z_mat);

			break;
		}
		case ROT_MODE_YZX:
		{
			/* Extract location, rotation, and scale form matrix. */
			mat4_to_loc_rot_size(loc, rot, scale, mat);

			/* Get euler angles from rotation matrix. */
			mat3_to_eulO(euler, ROT_MODE_YZX, rot);

			/* Create X, Y, Z rotation matrices from euler angles. */
			create_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, true);

			/* Concatenate rotation matrices. */
			mul_m3_m3m3(rot_mat, rot_mat, rot_x_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_y_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_z_mat);

			break;
		}
		case ROT_MODE_ZXY:
		{
			/* Extract location, rotation, and scale form matrix. */
			mat4_to_loc_rot_size(loc, rot, scale, mat);

			/* Get euler angles from rotation matrix. */
			mat3_to_eulO(euler, ROT_MODE_ZXY, rot);

			/* Create X, Y, Z rotation matrices from euler angles. */
			create_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, true);

			/* Concatenate rotation matrices. */
			mul_m3_m3m3(rot_mat, rot_mat, rot_z_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_x_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_y_mat);

			break;
		}
		case ROT_MODE_ZYX:
		{
			/* Extract location, rotation, and scale form matrix. */
			mat4_to_loc_rot_size(loc, rot, scale, mat);

			/* Get euler angles from rotation matrix. */
			mat3_to_eulO(euler, ROT_MODE_ZYX, rot);

			/* Create X, Y, Z rotation matrices from euler angles. */
			create_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, true);

			/* Concatenate rotation matrices. */
			mul_m3_m3m3(rot_mat, rot_mat, rot_x_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_z_mat);
			mul_m3_m3m3(rot_mat, rot_mat, rot_y_mat);

			break;
		}
	}

	/* Add rotation matrix to transformation matrix. */
	copy_m4_m3(transform_mat, rot_mat);

	/* Add translation to transformation matrix. */
	copy_zup_yup(transform_mat[3], loc);

	/* Create scale matrix. */
	scale_mat[0][0] = scale[0];
	scale_mat[1][1] = scale[2];
	scale_mat[2][2] = scale[1];

	/* Add scale to transformation matrix. */
	mul_m4_m4m4(transform_mat, transform_mat, scale_mat);
}

bool has_property(const Alembic::Abc::ICompoundProperty &prop, const std::string &name)
{
	if (!prop.valid()) {
		return false;
	}

	return prop.getPropertyHeader(name) != NULL;
}
