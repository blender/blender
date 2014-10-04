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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/scene_graph/SceneHash.cpp
 *  \ingroup freestyle
 */

#include "SceneHash.h"

#include <sstream>

namespace Freestyle {

string SceneHash::toString()
{
	 stringstream ss;
	 ss << hex << _sum;
	 return ss.str();
}

void SceneHash::visitNodeCamera(NodeCamera& cam)
{
	double *proj = cam.projectionMatrix();
	for (int i = 0; i < 16; i++) {
		adler32((unsigned char *)&proj[i], sizeof(double));
	}
}

void SceneHash::visitIndexedFaceSet(IndexedFaceSet& ifs)
{
	const real *v = ifs.vertices();
	const unsigned n = ifs.vsize();

	for (unsigned i = 0; i < n; i++) {
		adler32((unsigned char *)&v[i], sizeof(v[i]));
	}
}

static const int MOD_ADLER = 65521;

void SceneHash::adler32(unsigned char *data, int size)
{
	uint32_t sum1 = _sum & 0xffff;
	uint32_t sum2 = (_sum >> 16) & 0xffff;

	for (int i = 0; i < size; i++) {
		sum1 = (sum1 + data[i]) % MOD_ADLER;
		sum2 = (sum1 + sum2) % MOD_ADLER;
	}
	_sum = sum1 | (sum2 << 16);
}

} /* namespace Freestyle */
