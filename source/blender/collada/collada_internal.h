/**
 * $Id$
 *
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
#ifndef BLENDER_COLLADA_H
#define BLENDER_COLLADA_H

#include "COLLADAFWFileInfo.h"
#include "Math/COLLADABUMathMatrix4.h"

class UnitConverter
{
private:
	COLLADAFW::FileInfo::Unit unit;
	COLLADAFW::FileInfo::UpAxisType up_axis;

public:

	UnitConverter() : unit(), up_axis(COLLADAFW::FileInfo::Z_UP) {}

	void read_asset(const COLLADAFW::FileInfo* asset)
	{
	}

	// TODO
	// convert vector vec from COLLADA format to Blender
	void convertVec3(float *vec)
	{
	}
		
	// TODO need also for angle conversion, time conversion...

	void dae_matrix_to_mat4(float out[][4], const COLLADABU::Math::Matrix4& in)
	{
		// in DAE, matrices use columns vectors, (see comments in COLLADABUMathMatrix4.h)
		// so here, to make a blender matrix, we swap columns and rows
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				out[i][j] = in[j][i];
			}
		}
	}

	void mat4_to_dae(float out[][4], float in[][4])
	{
		copy_m4_m4(out, in);
		transpose_m4(out);
	}

	void mat4_to_dae_double(double out[][4], float in[][4])
	{
		float mat[4][4];

		mat4_to_dae(mat, in);

		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
				out[i][j] = mat[i][j];
	}
};

class TransformBase
{
public:
	void decompose(float mat[][4], float *loc, float eul[3], float quat[4], float *size)
	{
		mat4_to_size(size, mat);
		if (eul)
			mat4_to_eul(eul, mat);
		if (quat)
			mat4_to_quat(quat, mat);
		copy_v3_v3(loc, mat[3]);
	}
};

#endif
