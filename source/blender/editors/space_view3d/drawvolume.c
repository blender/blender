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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"




#include "DNA_armature_types.h"
#include "DNA_boid_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_constraint_types.h" // for drawing constraint
#include "DNA_effect_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_object_fluidsim.h"
#include "DNA_particle_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_smoke_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_edgehash.h"
#include "BLI_rand.h"

#include "BKE_anim.h"			//for the where_on_path function
#include "BKE_curve.h"
#include "BKE_constraint.h" // for the get_constraint_target function
#include "BKE_DerivedMesh.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_property.h"
#include "BKE_smoke.h"
#include "BKE_unit.h"
#include "BKE_utildefines.h"
#include "smoke_API.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"
#include "GPU_material.h"
#include "GPU_extensions.h"

#include "ED_mesh.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "WM_api.h"
#include "BLF_api.h"

#include "GPU_extensions.h"

#include "view3d_intern.h"	// own include


#ifdef _WIN32
#include <time.h>
#include <stdio.h>
#include <conio.h>
#include <windows.h>

static LARGE_INTEGER liFrequency;
static LARGE_INTEGER liStartTime;
static LARGE_INTEGER liCurrentTime;

static void tstart ( void )
{
	QueryPerformanceFrequency ( &liFrequency );
	QueryPerformanceCounter ( &liStartTime );
}
static void tend ( void )
{
	QueryPerformanceCounter ( &liCurrentTime );
}
static double tval()
{
	return ((double)( (liCurrentTime.QuadPart - liStartTime.QuadPart)* (double)1000.0/(double)liFrequency.QuadPart ));
}
#else
#include <sys/time.h>
static struct timeval _tstart, _tend;
static struct timezone tz;
static void tstart ( void )
{
	gettimeofday ( &_tstart, &tz );
}
static void tend ( void )
{
	gettimeofday ( &_tend,&tz );
}
static double tval()
{
	double t1, t2;
	t1 = ( double ) _tstart.tv_sec*1000 + ( double ) _tstart.tv_usec/ ( 1000 );
	t2 = ( double ) _tend.tv_sec*1000 + ( double ) _tend.tv_usec/ ( 1000 );
	return t2-t1;
}
#endif

struct GPUTexture;

int intersect_edges(float *points, float a, float b, float c, float d, float edges[12][2][3])
{
	int i;
	float t;
	int numpoints = 0;
	
	for (i=0; i<12; i++) {
		t = -(a*edges[i][0][0] + b*edges[i][0][1] + c*edges[i][0][2] + d)
			/ (a*edges[i][1][0] + b*edges[i][1][1] + c*edges[i][1][2]);
		if ((t>0)&&(t<1)) {
			points[numpoints * 3 + 0] = edges[i][0][0] + edges[i][1][0]*t;
			points[numpoints * 3 + 1] = edges[i][0][1] + edges[i][1][1]*t;
			points[numpoints * 3 + 2] = edges[i][0][2] + edges[i][1][2]*t;
			numpoints++;
		}
	}
	return numpoints;
}

static int convex(float *p0, float *up, float *a, float *b)
{
	// Vec3 va = a-p0, vb = b-p0;
	float va[3], vb[3], tmp[3];
	VECSUB(va, a, p0);
	VECSUB(vb, b, p0);
	Crossf(tmp, va, vb);
	return INPR(up, tmp) >= 0;
}

// copied from gpu_extension.c
static int is_pow2(int n)
{
	return ((n)&(n-1))==0;
}

static int larger_pow2(int n)
{
	if (is_pow2(n))
		return n;

	while(!is_pow2(n))
		n= n&(n-1);

	return n*2;
}

void draw_volume(Scene *scene, ARegion *ar, View3D *v3d, Base *base, GPUTexture *tex, float *min, float *max, int res[3], float dx, GPUTexture *tex_shadow)
{
	RegionView3D *rv3d= ar->regiondata;

	float viewnormal[3];
	int i, j, n, good_index;
	float d, d0, dd, ds;
	float *points = NULL;
	int numpoints = 0;
	float cor[3] = {1.,1.,1.};
	int gl_depth = 0, gl_blend = 0;

	/* draw slices of smoke is adapted from c++ code authored by: Johannes Schmid and Ingemar Rask, 2006, johnny@grob.org */
	float cv[][3] = {
		{1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
		{1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}
	};

	// edges have the form edges[n][0][xyz] + t*edges[n][1][xyz]
	float edges[12][2][3] = {
		{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 2.0f}},
		{{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 2.0f}},
		{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 2.0f}},
		{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 2.0f}},

		{{1.0f, -1.0f, 1.0f}, {0.0f, 2.0f, 0.0f}},
		{{-1.0f, -1.0f, 1.0f}, {0.0f, 2.0f, 0.0f}},
		{{-1.0f, -1.0f, -1.0f}, {0.0f, 2.0f, 0.0f}},
		{{1.0f, -1.0f, -1.0f}, {0.0f, 2.0f, 0.0f}},

		{{-1.0f, 1.0f, 1.0f}, {2.0f, 0.0f, 0.0f}},
		{{-1.0f, -1.0f, 1.0f}, {2.0f, 0.0f, 0.0f}},
		{{-1.0f, -1.0f, -1.0f}, {2.0f, 0.0f, 0.0f}},
		{{-1.0f, 1.0f, -1.0f}, {2.0f, 0.0f, 0.0f}}
	};

	/* Fragment program to calculate the 3dview of smoke */
	/* using 2 textures, density and shadow */
	const char *text = "!!ARBfp1.0\n"
					"PARAM dx = program.local[0];\n"
					"PARAM darkness = program.local[1];\n"
					"PARAM f = {1.442695041, 1.442695041, 1.442695041, 0.01};\n"
					"TEMP temp, shadow, value;\n"
					"TEX temp, fragment.texcoord[0], texture[0], 3D;\n"
					"TEX shadow, fragment.texcoord[0], texture[1], 3D;\n"
					"MUL value, temp, darkness;\n"
					"MUL value, value, dx;\n"
					"MUL value, value, f;\n"
					"EX2 temp, -value.r;\n"
					"SUB temp.a, 1.0, temp.r;\n"
					"MUL temp.r, temp.r, shadow.r;\n"
					"MUL temp.g, temp.g, shadow.r;\n"
					"MUL temp.b, temp.b, shadow.r;\n"
					"MOV result.color, temp;\n"
					"END\n";
	GLuint prog;

	
	float size[3];

	tstart();

	VECSUB(size, max, min);

	// maxx, maxy, maxz
	cv[0][0] = max[0];
	cv[0][1] = max[1];
	cv[0][2] = max[2];
	// minx, maxy, maxz
	cv[1][0] = min[0];
	cv[1][1] = max[1];
	cv[1][2] = max[2];
	// minx, miny, maxz
	cv[2][0] = min[0];
	cv[2][1] = min[1];
	cv[2][2] = max[2];
	// maxx, miny, maxz
	cv[3][0] = max[0];
	cv[3][1] = min[1];
	cv[3][2] = max[2];

	// maxx, maxy, minz
	cv[4][0] = max[0];
	cv[4][1] = max[1];
	cv[4][2] = min[2];
	// minx, maxy, minz
	cv[5][0] = min[0];
	cv[5][1] = max[1];
	cv[5][2] = min[2];
	// minx, miny, minz
	cv[6][0] = min[0];
	cv[6][1] = min[1];
	cv[6][2] = min[2];
	// maxx, miny, minz
	cv[7][0] = max[0];
	cv[7][1] = min[1];
	cv[7][2] = min[2];

	VECCOPY(edges[0][0], cv[4]); // maxx, maxy, minz
	VECCOPY(edges[1][0], cv[5]); // minx, maxy, minz
	VECCOPY(edges[2][0], cv[6]); // minx, miny, minz
	VECCOPY(edges[3][0], cv[7]); // maxx, miny, minz

	VECCOPY(edges[4][0], cv[3]); // maxx, miny, maxz
	VECCOPY(edges[5][0], cv[2]); // minx, miny, maxz
	VECCOPY(edges[6][0], cv[6]); // minx, miny, minz
	VECCOPY(edges[7][0], cv[7]); // maxx, miny, minz

	VECCOPY(edges[8][0], cv[1]); // minx, maxy, maxz
	VECCOPY(edges[9][0], cv[2]); // minx, miny, maxz
	VECCOPY(edges[10][0], cv[6]); // minx, miny, minz
	VECCOPY(edges[11][0], cv[5]); // minx, maxy, minz

	// printf("size x: %f, y: %f, z: %f\n", size[0], size[1], size[2]);
	// printf("min[2]: %f, max[2]: %f\n", min[2], max[2]);

	edges[0][1][2] = size[2];
	edges[1][1][2] = size[2];
	edges[2][1][2] = size[2];
	edges[3][1][2] = size[2];

	edges[4][1][1] = size[1];
	edges[5][1][1] = size[1];
	edges[6][1][1] = size[1];
	edges[7][1][1] = size[1];

	edges[8][1][0] = size[0];
	edges[9][1][0] = size[0];
	edges[10][1][0] = size[0];
	edges[11][1][0] = size[0];

	glGetBooleanv(GL_BLEND, (GLboolean *)&gl_blend);
	glGetBooleanv(GL_DEPTH_TEST, (GLboolean *)&gl_depth);

	wmLoadMatrix(rv3d->viewmat);
	// wmMultMatrix(ob->obmat);	

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/*
	printf("Viewinv:\n");
	printf("%f, %f, %f\n", rv3d->viewinv[0][0], rv3d->viewinv[0][1], rv3d->viewinv[0][2]);
	printf("%f, %f, %f\n", rv3d->viewinv[1][0], rv3d->viewinv[1][1], rv3d->viewinv[1][2]);
	printf("%f, %f, %f\n", rv3d->viewinv[2][0], rv3d->viewinv[2][1], rv3d->viewinv[2][2]);
	*/

	// get view vector
	VECCOPY(viewnormal, rv3d->viewinv[2]);
	Normalize(viewnormal);

	// find cube vertex that is closest to the viewer
	for (i=0; i<8; i++) {
		float x,y,z;

		x = cv[i][0] - viewnormal[0];
		y = cv[i][1] - viewnormal[1];
		z = cv[i][2] - viewnormal[2];

		if ((x>=min[0])&&(x<=max[0])
			&&(y>=min[1])&&(y<=max[1])
			&&(z>=min[2])&&(z<=max[2])) {
			break;
		}
	}

	// printf("i: %d\n", i);
	// printf("point %f, %f, %f\n", cv[i][0], cv[i][1], cv[i][2]);

	if (GL_TRUE == glewIsSupported("GL_ARB_fragment_program"))
	{
		glEnable(GL_FRAGMENT_PROGRAM_ARB);
		glGenProgramsARB(1, &prog);

		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, prog);
		glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(text), text);

		// cell spacing
		glProgramLocalParameter4fARB (GL_FRAGMENT_PROGRAM_ARB, 0, dx, dx, dx, 1.0);
		// custom parameter for smoke style (higher = thicker)
		glProgramLocalParameter4fARB (GL_FRAGMENT_PROGRAM_ARB, 1, 7.0, 7.0, 7.0, 1.0);
	}
	else
		printf("Your gfx card does not support 3dview smoke drawing.\n");

	GPU_texture_bind(tex, 0);
	if(tex_shadow)
		GPU_texture_bind(tex_shadow, 1);
	else
		printf("No volume shadow\n");

	if (!GPU_non_power_of_two_support()) {
		cor[0] = (float)res[0]/(float)larger_pow2(res[0]);
		cor[1] = (float)res[1]/(float)larger_pow2(res[1]);
		cor[2] = (float)res[2]/(float)larger_pow2(res[2]);
	}

	// our slices are defined by the plane equation a*x + b*y +c*z + d = 0
	// (a,b,c), the plane normal, are given by viewdir
	// d is the parameter along the view direction. the first d is given by
	// inserting previously found vertex into the plane equation
	d0 = (viewnormal[0]*cv[i][0] + viewnormal[1]*cv[i][1] + viewnormal[2]*cv[i][2]);
	ds = (ABS(viewnormal[0])*size[0] + ABS(viewnormal[1])*size[1] + ABS(viewnormal[2])*size[2]);
	dd = 0.05; // ds/512.0f;
	n = 0;
	good_index = i;

	// printf("d0: %f, dd: %f, ds: %f\n\n", d0, dd, ds);

	points = MEM_callocN(sizeof(float)*12*3, "smoke_points_preview");

	while(1) {
		float p0[3];
		float tmp_point[3], tmp_point2[3];

		if(dd*(float)n > ds)
			break;

		VECCOPY(tmp_point, viewnormal);
		VecMulf(tmp_point, -dd*((ds/dd)-(float)n));
		VECADD(tmp_point2, cv[good_index], tmp_point);
		d = INPR(tmp_point2, viewnormal);

		// printf("my d: %f\n", d);

		// intersect_edges returns the intersection points of all cube edges with
		// the given plane that lie within the cube
		numpoints = intersect_edges(points, viewnormal[0], viewnormal[1], viewnormal[2], -d, edges);

		// printf("points: %d\n", numpoints);

		if (numpoints > 2) {
			VECCOPY(p0, points);

			// sort points to get a convex polygon
			for(i = 1; i < numpoints - 1; i++)
			{
				for(j = i + 1; j < numpoints; j++)
				{
					if(!convex(p0, viewnormal, &points[j * 3], &points[i * 3]))
					{
						float tmp2[3];
						VECCOPY(tmp2, &points[j * 3]);
						VECCOPY(&points[j * 3], &points[i * 3]);
						VECCOPY(&points[i * 3], tmp2);
					}
				}
			}

			// printf("numpoints: %d\n", numpoints);
			glBegin(GL_POLYGON);
			glColor3f(1.0, 1.0, 1.0);
			for (i = 0; i < numpoints; i++) {
				glTexCoord3d((points[i * 3 + 0] - min[0] )*cor[0]/size[0], (points[i * 3 + 1] - min[1])*cor[1]/size[1], (points[i * 3 + 2] - min[2])*cor[2]/size[2]);
				glVertex3f(points[i * 3 + 0], points[i * 3 + 1], points[i * 3 + 2]);
			}
			glEnd();
		}
		n++;
	}

	tend();
	printf ( "Draw Time: %f\n",( float ) tval() );

	if(tex_shadow)
		GPU_texture_unbind(tex_shadow);
	GPU_texture_unbind(tex);

	if(GLEW_ARB_fragment_program)
	{
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
		glDeleteProgramsARB(1, &prog);
	}


	MEM_freeN(points);

	if(!gl_blend)
		glDisable(GL_BLEND);
	if(gl_depth)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);	
	}
}

