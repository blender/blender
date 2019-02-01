/*
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

#ifndef __BC_SAMPLE_H__
#define __BC_SAMPLE_H__

#include <string>
#include <map>
#include <algorithm>

extern "C"
{
#include "BKE_object.h"
#include "BLI_math_rotation.h"
#include "DNA_object_types.h"
#include "DNA_armature_types.h"
#include "DNA_material_types.h"
#include "DNA_lamp_types.h"
#include "DNA_camera_types.h"
}

typedef float(Matrix)[4][4];

class BCMatrix {

private:
	mutable float matrix[4][4];
	mutable float size[3];
	mutable float rot[3];
	mutable float loc[3];
	mutable float q[4];

	void unit();
	void copy(Matrix &r, Matrix &a);
	void set_transform(Object *ob);
	void set_transform(Matrix &mat);

public:

	float(&location() const)[3];
	float(&rotation() const)[3];
	float(&scale() const)[3];
	float(&quat() const)[4];

	BCMatrix(Matrix &mat);
	BCMatrix(Object *ob);

	void get_matrix(double(&mat)[4][4], const bool transposed = false, const int precision = -1) const;

	const bool in_range(const BCMatrix &other, float distance) const;
	static void sanitize(Matrix &matrix, int precision);
	static void transpose(Matrix &matrix);

};

typedef std::map<Bone *, BCMatrix *> BCBoneMatrixMap;

class BCSample{
private:

	BCMatrix obmat;
	BCBoneMatrixMap bonemats; /* For Armature animation */

public:
	BCSample(Object *ob);
	~BCSample();

	void add_bone_matrix(Bone *bone, Matrix &mat);

	const bool get_value(std::string channel_target, const int array_index, float *val) const;
	const BCMatrix &get_matrix() const;
	const BCMatrix *get_matrix(Bone *bone) const; // returns NULL if bone is not animated

};

typedef std::map<Object *, BCSample *> BCSampleMap;
typedef std::map<int, const BCSample *> BCFrameSampleMap;
typedef std::map<int, const BCMatrix *> BCMatrixSampleMap;

#endif
