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

#ifndef __DUALCON_H__
#define __DUALCON_H__

#ifdef __cplusplus
extern "C" { 
#endif

typedef float (*DualConCo)[3];
typedef unsigned int (*DualConFaces)[4];
struct DerivedMesh;

typedef struct DualConInput {
	DualConCo co;
	int co_stride;
	int totco;
	
	DualConFaces faces;
	int face_stride;
	int totface;
	
	float min[3], max[3];
} DualConInput;

/* callback for allocating memory for output */
typedef void *(*DualConAllocOutput)(int totvert, int totquad);
/* callback for adding a new vertex to the output */
typedef void (*DualConAddVert)(void *output, const float co[3]);
/* callback for adding a new quad to the output */
typedef void (*DualConAddQuad)(void *output, const int vert_indices[4]);

typedef enum {
	DUALCON_FLOOD_FILL = 1,
} DualConFlags;

typedef enum {
	/* blocky */
	DUALCON_CENTROID,
	/* smooth */
	DUALCON_MASS_POINT,
	/* keeps sharp edges */
	DUALCON_SHARP_FEATURES,
} DualConMode;

/* Usage:
   
   The three callback arguments are used for creating the output
   mesh. The alloc_output callback takes the total number of vertices
   and faces (quads) that will be in the output. It should allocate
   and return a structure to hold the output mesh. The add_vert and
   add_quad callbacks will then be called for each new vertex and
   quad, and the callback should add the new mesh elements to the
   structure.
*/
void *dualcon(const DualConInput *input_mesh,
			  /* callbacks for output */
			  DualConAllocOutput alloc_output,
			  DualConAddVert add_vert,
			  DualConAddQuad add_quad,

			  /* flags and settings to control the remeshing
				 algorithm */
			  DualConFlags flags,
			  DualConMode mode,
			  float threshold,
			  float hermite_num,
			  float scale,
			  int depth);

#ifdef __cplusplus
} 
#endif

#endif /* __DUALCON_H__ */
