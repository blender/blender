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
#include <float.h>

#if defined(_WIN32)
#define isnan(n) _isnan(n)
#endif

static void veccopy(float dst[3], const float src[3])
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

#define GET_TRI(_mesh, _n) \
	(*(DualConTri)(((char *)(_mesh)->looptri) + ((_n) * (_mesh)->tri_stride)))

#define GET_CO(_mesh, _n) \
	(*(DualConCo)(((char *)(_mesh)->co) + ((_n) * (_mesh)->co_stride)))

#define GET_LOOP(_mesh, _n) \
	(*(DualConLoop)(((char *)(_mesh)->mloop) + ((_n) * (_mesh)->loop_stride)))

class DualConInputReader : public ModelReader
{
private:
const DualConInput *input_mesh;
int tottri, curtri;
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
	curtri = 0;
	maxsize = 0;
	tottri = input_mesh->tottri;

	veccopy(min, input_mesh->min);
	veccopy(max, input_mesh->max);

	/* initialize maxsize */
	for (int i = 0; i < 3; i++) {
		float d = max[i] - min[i];
		if (d > maxsize)
			maxsize = d;
	}

	/* redo the bounds */
	for (int i = 0; i < 3; i++)
	{
		min[i] = (max[i] + min[i]) / 2 - maxsize / 2;
		max[i] = (max[i] + min[i]) / 2 + maxsize / 2;
	}

	for (int i = 0; i < 3; i++)
		min[i] -= maxsize * (1 / scale - 1) / 2;
	maxsize *= 1 / scale;
}

Triangle *getNextTriangle()
{
	if (curtri == input_mesh->tottri)
		return NULL;

	Triangle *t = new Triangle();

	unsigned int *tr = GET_TRI(input_mesh, curtri);
	veccopy(t->vt[0], GET_CO(input_mesh, GET_LOOP(input_mesh, tr[0])));
	veccopy(t->vt[1], GET_CO(input_mesh, GET_LOOP(input_mesh, tr[1])));
	veccopy(t->vt[2], GET_CO(input_mesh, GET_LOOP(input_mesh, tr[2])));

	curtri++;

	/* remove triangle if it contains invalid coords */
	for (int i = 0; i < 3; i++) {
		const float *co = t->vt[i];
		if (isnan(co[0]) || isnan(co[1]) || isnan(co[2])) {
			delete t;
			return getNextTriangle();
		}
	}

	return t;
}

int getNextTriangle(int t[3])
{
	if (curtri == input_mesh->tottri)
		return 0;

	unsigned int *tr = GET_TRI(input_mesh, curtri);
	t[0] = tr[0];
	t[1] = tr[1];
	t[2] = tr[2];

	curtri++;

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
	return maxsize;
}

/* output */
void getNextVertex(float /*v*/[3])
{
	/* not used */
}

/* stubs */
void printInfo() {
}
int getMemory() {
	return sizeof(DualConInputReader);
}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("DUALCON:DualConInputReader")
#endif

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
