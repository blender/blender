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
 * Contributor(s): Nicholas Bishop
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <cassert>
#include "dualcon.h"
#include "ModelReader.h"
#include "octree.h"

#include <cstdio>

void veccopy(float dst[3], const float src[3])
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

#define GET_FACE(_mesh, _n) \
	(*(DualConFaces)(((char*)(_mesh)->faces) + ((_n) * (_mesh)->face_stride)))

#define GET_CO(_mesh, _n) \
	(*(DualConCo)(((char*)(_mesh)->co) + ((_n) * (_mesh)->co_stride)))

class DualConInputReader : public ModelReader
{
private:
	const DualConInput *input_mesh;
	int tottri, curface, offset;
	float min[3], max[3], maxsize;
	float scale;
public:
	DualConInputReader(const DualConInput *mesh, float _scale)
	: input_mesh(mesh), scale(_scale)
	{
		reset();
	}

	void reset()
	{
		tottri = 0;
		curface = 0;
		offset = 0;
		maxsize = 0;

		/* initialize tottri */
		for(int i = 0; i < input_mesh->totface; i++)
			tottri += GET_FACE(input_mesh, i)[3] ? 2 : 1;

		veccopy(min, input_mesh->min);
		veccopy(max, input_mesh->max);

		/* initialize maxsize */
		for(int i = 0; i < 3; i++) {
			float d = max[i] - min[i];
			if(d > maxsize)
				maxsize = d;
		}

		/* redo the bounds */
		for(int i = 0; i < 3; i++)
		{
			min[i] = (max[i] + min[i]) / 2 - maxsize / 2;
			max[i] = (max[i] + min[i]) / 2 + maxsize / 2;
		}

		for(int i = 0; i < 3; i++)
			min[i] -= maxsize * (1 / scale - 1) / 2;
		maxsize *= 1 / scale;
	}

	Triangle* getNextTriangle()
	{
		if(curface == input_mesh->totface)
			return 0;

		Triangle* t = new Triangle();
		
		unsigned int *f = GET_FACE(input_mesh, curface);
		if(offset == 0) {
			veccopy(t->vt[0], GET_CO(input_mesh, f[0]));
			veccopy(t->vt[1], GET_CO(input_mesh, f[1]));
			veccopy(t->vt[2], GET_CO(input_mesh, f[2]));
		}
		else {
			veccopy(t->vt[0], GET_CO(input_mesh, f[2]));
			veccopy(t->vt[1], GET_CO(input_mesh, f[3]));
			veccopy(t->vt[2], GET_CO(input_mesh, f[0]));
		}

		if(offset == 0 && f[3])
			offset++;
		else {
			offset = 0;
			curface++;
		}

		/* remove triangle if it contains invalid coords */
		for(int i = 0; i < 3; i++) {
			const float *co = t->vt[i];
			if(isnan(co[0]) || isnan(co[1]) || isnan(co[2])) {
				delete t;
				return getNextTriangle();
			}
		}

		return t;
	}

	int getNextTriangle(int t[3])
	{
		if(curface == input_mesh->totface)
			return 0;
		
		unsigned int *f = GET_FACE(input_mesh, curface);
		if(offset == 0) {
			t[0] = f[0];
			t[1] = f[1];
			t[2] = f[2];
		}
		else {
			t[0] = f[2];
			t[1] = f[3];
			t[2] = f[0];
		}

		if(offset == 0 && f[3])
			offset++;
		else {
			offset = 0;
			curface++;
		}

		return 1;
	}

	int getNumTriangles()
	{
		return tottri;
	}

	int getNumVertices()
	{
		return input_mesh->totco;
	}

	float getBoundingBox(float origin[3])
	{
		veccopy(origin, min);
		return maxsize ;
	}

	/* output */
	void getNextVertex(float v[3])
	{
		/* not used */
	}

	/* stubs */
	void printInfo() {}
	int getMemory() { return sizeof(DualConInputReader); }
};

void *dualcon(const DualConInput *input_mesh,
			  /* callbacks for output */
			  DualConAllocOutput alloc_output,
			  DualConAddVert add_vert,
			  DualConAddQuad add_quad,
					 
			  DualConFlags flags,
			  DualConMode mode,
			  float threshold,
			  float hermite_num,
			  float scale,
			  int depth)
{
	DualConInputReader r(input_mesh, scale);
	Octree o(&r, alloc_output, add_vert, add_quad,
			 flags, mode, depth, threshold, hermite_num);
	o.scanConvert();
	return o.getOutputMesh();
}
