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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, full recode and added functions
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawobject.c
 *  \ingroup spview3d
 */


#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_constraint_types.h" // for drawing constraint
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"
#include "DNA_speaker_types.h"
#include "DNA_world_types.h"
#include "DNA_armature_types.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_anim.h"			//for the where_on_path function
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_constraint.h" // for the get_constraint_target function
#include "BKE_curve.h"
#include "BKE_DerivedMesh.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
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
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_unit.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "BKE_tessmesh.h"

#include "smoke_API.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"

#include "ED_mesh.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_types.h"
#include "ED_curve.h" /* for curve_editnurbs */

#include "UI_resources.h"

#include "WM_api.h"
#include "wm_subwindow.h"
#include "BLF_api.h"

#include "view3d_intern.h"	// own include


/* this condition has been made more complex since editmode can draw textures */
#define CHECK_OB_DRAWTEXTURE(vd, dt)                                          \
	((ELEM(vd->drawtype, OB_TEXTURE, OB_MATERIAL) && dt>OB_SOLID) ||          \
	(vd->drawtype==OB_SOLID && vd->flag2 & V3D_SOLID_TEX))

typedef enum eWireDrawMode {
	OBDRAW_WIRE_OFF= 0,
	OBDRAW_WIRE_ON= 1,
	OBDRAW_WIRE_ON_DEPTH= 2
} eWireDrawMode;

/* user data structures for derived mesh callbacks */
typedef struct foreachScreenVert_userData {
	void (*func)(void *userData, BMVert *eve, int x, int y, int index);
	void *userData;
	ViewContext vc;
	eV3DClipTest clipVerts;
} foreachScreenVert_userData;

typedef struct foreachScreenEdge_userData {
	void (*func)(void *userData, BMEdge *eed, int x0, int y0, int x1, int y1, int index);
	void *userData;
	ViewContext vc;
	eV3DClipTest clipVerts;
} foreachScreenEdge_userData;

typedef struct foreachScreenFace_userData {
	void (*func)(void *userData, BMFace *efa, int x, int y, int index);
	void *userData;
	ViewContext vc;
} foreachScreenFace_userData;

typedef struct drawDMVerts_userData {
	BMEditMesh *em; /* BMESH BRANCH ONLY */

	int sel;
	BMVert *eve_act;
} drawDMVerts_userData;

typedef struct drawDMEdgesSel_userData {
	BMEditMesh *em; /* BMESH BRANCH ONLY */

	unsigned char *baseCol, *selCol, *actCol;
	BMEdge *eed_act;
} drawDMEdgesSel_userData;

typedef struct drawDMFacesSel_userData {
	unsigned char *cols[3];

	DerivedMesh *dm; /* BMESH BRANCH ONLY */
	BMEditMesh *em;  /* BMESH BRANCH ONLY */

	BMFace *efa_act;
	int *orig_index;
} drawDMFacesSel_userData;

typedef struct drawDMNormal_userData {
	BMEditMesh *em;
	float normalsize;
} drawDMNormal_userData;

typedef struct bbsObmodeMeshVerts_userData {
	void *offset;
	MVert *mvert;
} bbsObmodeMeshVerts_userData;

static void draw_bounding_volume(Scene *scene, Object *ob, char type);

static void drawcube_size(float size);
static void drawcircle_size(float size);
static void draw_empty_sphere(float size);
static void draw_empty_cone(float size);

static int check_ob_drawface_dot(Scene *sce, View3D *vd, char dt)
{
	if ((sce->toolsettings->selectmode & SCE_SELECT_FACE) == 0)
		return 0;

	if (G.f & G_BACKBUFSEL)
		return 0;

	if ((vd->flag & V3D_ZBUF_SELECT) == 0)
		return 1;

	/* if its drawing textures with zbuf sel, then don't draw dots */
	if (dt==OB_TEXTURE && vd->drawtype==OB_TEXTURE)
		return 0;

	if ((vd->drawtype >= OB_SOLID) && (vd->flag2 & V3D_SOLID_TEX))
		return 0;

	return 1;
}

/* ************* only use while object drawing **************
 * or after running ED_view3d_init_mats_rv3d
 * */
static void view3d_project_short_clip(ARegion *ar, const float vec[3], short adr[2], int is_local)
{
	RegionView3D *rv3d= ar->regiondata;
	float fx, fy, vec4[4];
	
	adr[0]= IS_CLIPPED;
	
	/* clipplanes in eye space */
	if (rv3d->rflag & RV3D_CLIPPING) {
		if (ED_view3d_clipping_test(rv3d, vec, is_local))
			return;
	}
	
	copy_v3_v3(vec4, vec);
	vec4[3]= 1.0;
	
	mul_m4_v4(rv3d->persmatob, vec4);
	
	/* clipplanes in window space */
	if ( vec4[3] > (float)BL_NEAR_CLIP ) {	/* is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if ( fx>0 && fx<ar->winx) {
			
			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if (fy > 0.0f && fy < (float)ar->winy) {
				adr[0]= (short)floorf(fx);
				adr[1]= (short)floorf(fy);
			}
		}
	}
}

/* BMESH NOTE: this function is unused in bmesh only */

/* only use while object drawing */
static void UNUSED_FUNCTION(view3d_project_short_noclip)(ARegion *ar, const float vec[3], short adr[2])
{
	RegionView3D *rv3d= ar->regiondata;
	float fx, fy, vec4[4];
	
	adr[0]= IS_CLIPPED;
	
	copy_v3_v3(vec4, vec);
	vec4[3]= 1.0;
	
	mul_m4_v4(rv3d->persmatob, vec4);
	
	if ( vec4[3] > (float)BL_NEAR_CLIP ) {	/* is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if ( fx>-32700 && fx<32700) {
			
			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if (fy > -32700.0f && fy < 32700.0f) {
				adr[0]= (short)floorf(fx);
				adr[1]= (short)floorf(fy);
			}
		}
	}
}

/* same as view3d_project_short_clip but use persmat instead of persmatob for projection */
static void view3d_project_short_clip_persmat(ARegion *ar, const float vec[3], short adr[2], int is_local)
{
	RegionView3D *rv3d= ar->regiondata;
	float fx, fy, vec4[4];

	adr[0]= IS_CLIPPED;

	/* clipplanes in eye space */
	if (rv3d->rflag & RV3D_CLIPPING) {
		if (ED_view3d_clipping_test(rv3d, vec, is_local))
			return;
	}

	copy_v3_v3(vec4, vec);
	vec4[3]= 1.0;

	mul_m4_v4(rv3d->persmat, vec4);

	/* clipplanes in window space */
	if ( vec4[3] > (float)BL_NEAR_CLIP ) {	/* is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);

		if ( fx>0 && fx<ar->winx) {

			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);

			if (fy > 0.0f && fy < (float)ar->winy) {
				adr[0]= (short)floorf(fx);
				adr[1]= (short)floorf(fy);
			}
		}
	}
}
/* ************************ */

/* check for glsl drawing */

int draw_glsl_material(Scene *scene, Object *ob, View3D *v3d, int dt)
{
	if (!GPU_glsl_support())
		return 0;
	if (G.f & G_PICKSEL)
		return 0;
	if (!CHECK_OB_DRAWTEXTURE(v3d, dt))
		return 0;
	if (ob==OBACT && (ob && ob->mode & OB_MODE_WEIGHT_PAINT))
		return 0;
	if (scene_use_new_shading_nodes(scene))
		return 0;
	
	return (scene->gm.matmode == GAME_MAT_GLSL) && (dt > OB_SOLID);
}

static int check_alpha_pass(Base *base)
{
	if (base->flag & OB_FROMDUPLI)
		return 0;

	if (G.f & G_PICKSEL)
		return 0;
	
	return (base->object->dtx & OB_DRAWTRANSP);
}

	/***/
static unsigned int colortab[24]=
	{0x0,		0xFF88FF, 0xFFBBFF, 
	 0x403000,	0xFFFF88, 0xFFFFBB, 
	 0x104040,	0x66CCCC, 0x77CCCC, 
	 0x104010,	0x55BB55, 0x66FF66, 
	 0xFFFFFF
};


static float cube[8][3] = {
	{-1.0, -1.0, -1.0},
	{-1.0, -1.0,  1.0},
	{-1.0,  1.0,  1.0},
	{-1.0,  1.0, -1.0},
	{ 1.0, -1.0, -1.0},
	{ 1.0, -1.0,  1.0},
	{ 1.0,  1.0,  1.0},
	{ 1.0,  1.0, -1.0},
};

/* ----------------- OpenGL Circle Drawing - Tables for Optimized Drawing Speed ------------------ */
/* 32 values of sin function (still same result!) */
#define CIRCLE_RESOL 32

static const float sinval[CIRCLE_RESOL] = {
	0.00000000,
	0.20129852,
	0.39435585,
	0.57126821,
	0.72479278,
	0.84864425,
	0.93775213,
	0.98846832,
	0.99871650,
	0.96807711,
	0.89780453,
	0.79077573,
	0.65137248,
	0.48530196,
	0.29936312,
	0.10116832,
	-0.10116832,
	-0.29936312,
	-0.48530196,
	-0.65137248,
	-0.79077573,
	-0.89780453,
	-0.96807711,
	-0.99871650,
	-0.98846832,
	-0.93775213,
	-0.84864425,
	-0.72479278,
	-0.57126821,
	-0.39435585,
	-0.20129852,
	0.00000000
};

/* 32 values of cos function (still same result!) */
static const float cosval[CIRCLE_RESOL] = {
	1.00000000,
	0.97952994,
	0.91895781,
	0.82076344,
	0.68896691,
	0.52896401,
	0.34730525,
	0.15142777,
	-0.05064916,
	-0.25065253,
	-0.44039415,
	-0.61210598,
	-0.75875812,
	-0.87434661,
	-0.95413925,
	-0.99486932,
	-0.99486932,
	-0.95413925,
	-0.87434661,
	-0.75875812,
	-0.61210598,
	-0.44039415,
	-0.25065253,
	-0.05064916,
	0.15142777,
	0.34730525,
	0.52896401,
	0.68896691,
	0.82076344,
	0.91895781,
	0.97952994,
	1.00000000
};

static void draw_xyz_wire(const float c[3], float size, int axis)
{
	float v1[3]= {0.f, 0.f, 0.f}, v2[3] = {0.f, 0.f, 0.f};
	float dim = size * 0.1f;
	float dx[3], dy[3], dz[3];

	dx[0]=dim; dx[1]=0.f; dx[2]=0.f;
	dy[0]=0.f; dy[1]=dim; dy[2]=0.f;
	dz[0]=0.f; dz[1]=0.f; dz[2]=dim;

	switch(axis) {
		case 0:		/* x axis */
			glBegin(GL_LINES);
			
			/* bottom left to top right */
			sub_v3_v3v3(v1, c, dx);
			sub_v3_v3(v1, dy);
			add_v3_v3v3(v2, c, dx);
			add_v3_v3(v2, dy);
			
			glVertex3fv(v1);
			glVertex3fv(v2);
			
			/* top left to bottom right */
			mul_v3_fl(dy, 2.f);
			add_v3_v3(v1, dy);
			sub_v3_v3(v2, dy);
			
			glVertex3fv(v1);
			glVertex3fv(v2);
			
			glEnd();
			break;
		case 1:		/* y axis */
			glBegin(GL_LINES);
			
			/* bottom left to top right */
			mul_v3_fl(dx, 0.75f);
			sub_v3_v3v3(v1, c, dx);
			sub_v3_v3(v1, dy);
			add_v3_v3v3(v2, c, dx);
			add_v3_v3(v2, dy);
			
			glVertex3fv(v1);
			glVertex3fv(v2);
			
			/* top left to center */
			mul_v3_fl(dy, 2.f);
			add_v3_v3(v1, dy);
			copy_v3_v3(v2, c);
			
			glVertex3fv(v1);
			glVertex3fv(v2);
			
			glEnd();
			break;
		case 2:		/* z axis */
			glBegin(GL_LINE_STRIP);
			
			/* start at top left */
			sub_v3_v3v3(v1, c, dx);
			add_v3_v3v3(v1, c, dz);
			
			glVertex3fv(v1);
			
			mul_v3_fl(dx, 2.f);
			add_v3_v3(v1, dx);

			glVertex3fv(v1);
			
			mul_v3_fl(dz, 2.f);
			sub_v3_v3(v1, dx);
			sub_v3_v3(v1, dz);
			
			glVertex3fv(v1);
			
			add_v3_v3(v1, dx);
		
			glVertex3fv(v1);
			
			glEnd();
			break;
	}
	
}

void drawaxes(float size, char drawtype)
{
	int axis;
	float v1[3]= {0.0, 0.0, 0.0};
	float v2[3]= {0.0, 0.0, 0.0};
	float v3[3]= {0.0, 0.0, 0.0};
	
	switch(drawtype) {

		case OB_PLAINAXES:
			for (axis=0; axis<3; axis++) {
				glBegin(GL_LINES);

				v1[axis]= size;
				v2[axis]= -size;
				glVertex3fv(v1);
				glVertex3fv(v2);

				/* reset v1 & v2 to zero */
				v1[axis]= v2[axis]= 0.0f;

				glEnd();
			}
			break;
		case OB_SINGLE_ARROW:

			glBegin(GL_LINES);
			/* in positive z direction only */
			v1[2]= size;
			glVertex3fv(v1);
			glVertex3fv(v2);
			glEnd();

			/* square pyramid */
			glBegin(GL_TRIANGLES);

			v2[0]= size * 0.035f; v2[1] = size * 0.035f;
			v3[0]= size * -0.035f; v3[1] = size * 0.035f;
			v2[2]= v3[2]= size * 0.75f;

			for (axis=0; axis<4; axis++) {
				if (axis % 2 == 1) {
					v2[0] = -v2[0];
					v3[1] = -v3[1];
				}
				else {
					v2[1] = -v2[1];
					v3[0] = -v3[0];
				}

				glVertex3fv(v1);
				glVertex3fv(v2);
				glVertex3fv(v3);

			}
			glEnd();

			break;
		case OB_CUBE:
			drawcube_size(size);
			break;

		case OB_CIRCLE:
			drawcircle_size(size);
			break;

		case OB_EMPTY_SPHERE:
			draw_empty_sphere(size);
			break;

		case OB_EMPTY_CONE:
			draw_empty_cone(size);
			break;

		case OB_ARROWS:
		default:
		{
			for (axis=0; axis<3; axis++) {
				const int arrow_axis= (axis==0) ? 1:0;

				glBegin(GL_LINES);

				v2[axis]= size;
				glVertex3fv(v1);
				glVertex3fv(v2);
				
				v1[axis]= size*0.85f;
				v1[arrow_axis]= -size*0.08f;
				glVertex3fv(v1);
				glVertex3fv(v2);
				
				v1[arrow_axis]= size*0.08f;
				glVertex3fv(v1);
				glVertex3fv(v2);

				glEnd();
				
				v2[axis]+= size*0.125f;

				draw_xyz_wire(v2, size, axis);


				/* reset v1 & v2 to zero */
				v1[arrow_axis]= v1[axis]= v2[axis]= 0.0f;
			}
			break;
		}
	}
}


/* Function to draw an Image on a empty Object */
static void draw_empty_image(Object *ob)
{
	Image *ima = (Image*)ob->data;
	ImBuf *ibuf = ima ? BKE_image_get_ibuf(ima, NULL) : NULL;

	float scale, ofs_x, ofs_y, sca_x, sca_y;
	int ima_x, ima_y;

	if (ibuf && (ibuf->rect == NULL) && (ibuf->rect_float != NULL)) {
		IMB_rect_from_float(ibuf);
	}

	/* Get the buffer dimensions so we can fallback to fake ones */
	if (ibuf && ibuf->rect) {
		ima_x= ibuf->x;
		ima_y= ibuf->y;
	}
	else {
		ima_x= 1;
		ima_y= 1;
	}

	/* Get the image aspect even if the buffer is invalid */
	if (ima) {
		if (ima->aspx > ima->aspy) {
			sca_x= 1.0f;
			sca_y= ima->aspy / ima->aspx;
		}
		else if (ima->aspx < ima->aspy) {
			sca_x= ima->aspx / ima->aspy;
			sca_y= 1.0f;
		}
		else {
			sca_x= 1.0f;
			sca_y= 1.0f;
		}
	}
	else {
		sca_x= 1.0f;
		sca_y= 1.0f;
	}

	/* Calculate the scale center based on objects origin */
	ofs_x= ob->ima_ofs[0] * ima_x;
	ofs_y= ob->ima_ofs[1] * ima_y;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	/* Make sure we are drawing at the origin */
	glTranslatef(0.0f,  0.0f,  0.0f);

	/* Calculate Image scale */
	scale= (ob->empty_drawsize / (float)MAX2(ima_x * sca_x, ima_y * sca_y));

	/* Set the object scale */
	glScalef(scale * sca_x, scale * sca_y, 1.0f);

	if (ibuf && ibuf->rect) {
		/* Setup GL params */
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);

		/* Use the object color and alpha */
		glColor4fv(ob->col);

		/* Draw the Image on the screen */
		glaDrawPixelsTex(ofs_x, ofs_y, ima_x, ima_y, GL_UNSIGNED_BYTE, ibuf->rect);
		glPixelTransferf(GL_ALPHA_SCALE, 1.0f);

		glDisable(GL_BLEND);
	}

	UI_ThemeColor((ob->flag & SELECT) ? TH_SELECT : TH_WIRE);

	/* Calculate the outline vertex positions */
	glBegin(GL_LINE_LOOP);
	glVertex2f(ofs_x, ofs_y);
	glVertex2f(ofs_x + ima_x, ofs_y);
	glVertex2f(ofs_x + ima_x, ofs_y + ima_y);
	glVertex2f(ofs_x, ofs_y + ima_y);
	glEnd();

	/* Reset GL settings */
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

static void circball_array_fill(float verts[CIRCLE_RESOL][3], const float cent[3], float rad, float tmat[][4])
{
	float vx[3], vy[3];
	float *viter= (float *)verts;
	unsigned int a;

	mul_v3_v3fl(vx, tmat[0], rad);
	mul_v3_v3fl(vy, tmat[1], rad);

	for (a=0; a < CIRCLE_RESOL; a++, viter += 3) {
		viter[0]= cent[0] + sinval[a] * vx[0] + cosval[a] * vy[0];
		viter[1]= cent[1] + sinval[a] * vx[1] + cosval[a] * vy[1];
		viter[2]= cent[2] + sinval[a] * vx[2] + cosval[a] * vy[2];
	}
}

void drawcircball(int mode, const float cent[3], float rad, float tmat[][4])
{
	float verts[CIRCLE_RESOL][3];

	circball_array_fill(verts, cent, rad, tmat);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(mode, 0, CIRCLE_RESOL);
	glDisableClientState(GL_VERTEX_ARRAY);
}

/* circle for object centers, special_color is for library or ob users */
static void drawcentercircle(View3D *v3d, RegionView3D *rv3d, const float co[3], int selstate, int special_color)
{
	const float size= ED_view3d_pixel_size(rv3d, co) * (float)U.obcenter_dia * 0.5f;
	float verts[CIRCLE_RESOL][3];

	/* using gldepthfunc guarantees that it does write z values,
	 * but not checks for it, so centers remain visible independent order of drawing */
	if (v3d->zbuf)  glDepthFunc(GL_ALWAYS);
	glEnable(GL_BLEND);
	
	if (special_color) {
		if (selstate==ACTIVE || selstate==SELECT) glColor4ub(0x88, 0xFF, 0xFF, 155);

		else glColor4ub(0x55, 0xCC, 0xCC, 155);
	}
	else {
		if (selstate == ACTIVE) UI_ThemeColorShadeAlpha(TH_ACTIVE, 0, -80);
		else if (selstate == SELECT) UI_ThemeColorShadeAlpha(TH_SELECT, 0, -80);
		else if (selstate == DESELECT) UI_ThemeColorShadeAlpha(TH_TRANSFORM, 0, -80);
	}

	circball_array_fill(verts, co, size, rv3d->viewinv);

	/* enable vertex array */
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, verts);

	/* 1. draw filled, blended polygon */
	glDrawArrays(GL_POLYGON, 0, CIRCLE_RESOL);

	/* 2. draw outline */
	UI_ThemeColorShadeAlpha(TH_WIRE, 0, -30);
	glDrawArrays(GL_LINE_LOOP, 0, CIRCLE_RESOL);

	/* finish up */
	glDisableClientState(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);

	if (v3d->zbuf)  glDepthFunc(GL_LEQUAL);
}

/* *********** text drawing for object/particles/armature ************* */
static ListBase CachedText[3];
static int CachedTextLevel= 0;

typedef struct ViewCachedString {
	struct ViewCachedString *next, *prev;
	float vec[3];
	union {
		unsigned char ub[4];
		int pack;
	} col;
	short sco[2];
	short xoffs;
	short flag;
	int str_len, pad;
	/* str is allocated past the end */
} ViewCachedString;

void view3d_cached_text_draw_begin(void)
{
	ListBase *strings= &CachedText[CachedTextLevel];
	strings->first= strings->last= NULL;
	CachedTextLevel++;
}

void view3d_cached_text_draw_add(const float co[3],
                                 const char *str,
                                 short xoffs, short flag,
                                 const unsigned char col[4])
{
	int alloc_len= strlen(str) + 1;
	ListBase *strings= &CachedText[CachedTextLevel-1];
	/* TODO, replace with more efficient malloc, perhaps memarena per draw? */
	ViewCachedString *vos= MEM_callocN(sizeof(ViewCachedString) + alloc_len, "ViewCachedString");

	BLI_addtail(strings, vos);
	copy_v3_v3(vos->vec, co);
	vos->col.pack= *((int *)col);
	vos->xoffs= xoffs;
	vos->flag= flag;
	vos->str_len= alloc_len-1;

	/* allocate past the end */
	memcpy(++vos, str, alloc_len);
}

void view3d_cached_text_draw_end(View3D *v3d, ARegion *ar, int depth_write, float mat[][4])
{
	RegionView3D *rv3d= ar->regiondata;
	ListBase *strings= &CachedText[CachedTextLevel-1];
	ViewCachedString *vos;
	int tot= 0;
	
	/* project first and test */
	for (vos= strings->first; vos; vos= vos->next) {
		if (mat && !(vos->flag & V3D_CACHE_TEXT_WORLDSPACE))
			mul_m4_v3(mat, vos->vec);

		if (vos->flag & V3D_CACHE_TEXT_GLOBALSPACE)
			view3d_project_short_clip_persmat(ar, vos->vec, vos->sco, (vos->flag & V3D_CACHE_TEXT_LOCALCLIP) != 0);
		else
			view3d_project_short_clip(ar, vos->vec, vos->sco, (vos->flag & V3D_CACHE_TEXT_LOCALCLIP) != 0);

		if (vos->sco[0]!=IS_CLIPPED)
			tot++;
	}

	if (tot) {
		int col_pack_prev= 0;

#if 0
		bglMats mats; /* ZBuffer depth vars */
		double ux, uy, uz;
		float depth;

		if (v3d->zbuf)
			bgl_get_mats(&mats);
#endif
		if (rv3d->rflag & RV3D_CLIPPING) {
			ED_view3d_clipping_disable();
		}

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		ED_region_pixelspace(ar);
		
		if (depth_write) {
			if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
		}
		else {
			glDepthMask(0);
		}
		
		for (vos= strings->first; vos; vos= vos->next) {
			/* too slow, reading opengl info while drawing is very bad,
			 * better to see if we can use the zbuffer while in pixel space - campbell */
#if 0
			if (v3d->zbuf && (vos->flag & V3D_CACHE_TEXT_ZBUF)) {
				gluProject(vos->vec[0], vos->vec[1], vos->vec[2], mats.modelview, mats.projection, (GLint *)mats.viewport, &ux, &uy, &uz);
				glReadPixels(ar->winrct.xmin+vos->mval[0]+vos->xoffs, ar->winrct.ymin+vos->mval[1], 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

				if (uz > depth)
					continue;
			}
#endif
			if (vos->sco[0]!=IS_CLIPPED) {
				const char *str= (char *)(vos+1);

				if (col_pack_prev != vos->col.pack) {
					glColor3ubv(vos->col.ub);
					col_pack_prev= vos->col.pack;
				}

				((vos->flag & V3D_CACHE_TEXT_ASCII) ?
				            BLF_draw_default_ascii :
				            BLF_draw_default
				            ) ( (float)vos->sco[0] + vos->xoffs,
				                (float)vos->sco[1],
				                (depth_write) ? 0.0f: 2.0f,
				                str,
				                vos->str_len);
			}
		}
		
		if (depth_write) {
			if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
		}
		else glDepthMask(1);
		
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();

		if (rv3d->rflag & RV3D_CLIPPING) {
			ED_view3d_clipping_enable();
		}
	}
	
	if (strings->first)
		BLI_freelistN(strings);
	
	CachedTextLevel--;
}

/* ******************** primitive drawing ******************* */

static void drawcube(void)
{

	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[0]); glVertex3fv(cube[1]);glVertex3fv(cube[2]); glVertex3fv(cube[3]);
	glVertex3fv(cube[0]); glVertex3fv(cube[4]);glVertex3fv(cube[5]); glVertex3fv(cube[6]);
	glVertex3fv(cube[7]); glVertex3fv(cube[4]);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[1]); glVertex3fv(cube[5]);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[2]); glVertex3fv(cube[6]);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[3]); glVertex3fv(cube[7]);
	glEnd();
}

/* draws a cube on given the scaling of the cube, assuming that 
 * all required matrices have been set (used for drawing empties)
 */
static void drawcube_size(float size)
{
	glBegin(GL_LINE_STRIP);
	glVertex3f(-size,-size,-size); glVertex3f(-size,-size,size);
	glVertex3f(-size,size,size); glVertex3f(-size,size,-size);

	glVertex3f(-size,-size,-size); glVertex3f(size,-size,-size);
	glVertex3f(size,-size,size); glVertex3f(size,size,size);

	glVertex3f(size,size,-size); glVertex3f(size,-size,-size);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3f(-size,-size,size); glVertex3f(size,-size,size);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3f(-size,size,size); glVertex3f(size,size,size);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3f(-size,size,-size); glVertex3f(size,size,-size);
	glEnd();
}

/* this is an unused (old) cube-drawing function based on a given size */
#if 0
static void drawcube_size(const float size[3])
{

	glPushMatrix();
	glScalef(size[0],  size[1],  size[2]);
	

	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[0]); glVertex3fv(cube[1]);glVertex3fv(cube[2]); glVertex3fv(cube[3]);
	glVertex3fv(cube[0]); glVertex3fv(cube[4]);glVertex3fv(cube[5]); glVertex3fv(cube[6]);
	glVertex3fv(cube[7]); glVertex3fv(cube[4]);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[1]); glVertex3fv(cube[5]);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[2]); glVertex3fv(cube[6]);
	glEnd();

	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[3]); glVertex3fv(cube[7]);
	glEnd();
	
	glPopMatrix();
}
#endif

static void drawshadbuflimits(Lamp *la, float mat[][4])
{
	float sta[3], end[3], lavec[3];

	negate_v3_v3(lavec, mat[2]);
	normalize_v3(lavec);

	madd_v3_v3v3fl(sta, mat[3], lavec, la->clipsta);
	madd_v3_v3v3fl(end, mat[3], lavec, la->clipend);

	glBegin(GL_LINE_STRIP);
	glVertex3fv(sta);
	glVertex3fv(end);
	glEnd();

	glPointSize(3.0);
	bglBegin(GL_POINTS);
	bglVertex3fv(sta);
	bglVertex3fv(end);
	bglEnd();
	glPointSize(1.0);
}



static void spotvolume(float lvec[3], float vvec[3], const float inp)
{
	/* camera is at 0,0,0 */
	float temp[3],plane[3],mat1[3][3],mat2[3][3],mat3[3][3],mat4[3][3],q[4],co,si,angle;

	normalize_v3(lvec);
	normalize_v3(vvec);				/* is this the correct vector ? */

	cross_v3_v3v3(temp,vvec,lvec);		/* equation for a plane through vvec en lvec */
	cross_v3_v3v3(plane,lvec,temp);		/* a plane perpendicular to this, parrallel with lvec */

	/* vectors are exactly aligned, use the X axis, this is arbitrary */
	if (normalize_v3(plane) == 0.0f)
		plane[1]= 1.0f;

	/* now we've got two equations: one of a cone and one of a plane, but we have
	 * three unknowns. We remove one unknown by rotating the plane to z=0 (the plane normal) */

	/* rotate around cross product vector of (0,0,1) and plane normal, dot product degrees */
	/* according definition, we derive cross product is (plane[1],-plane[0],0), en cos = plane[2]);*/

	/* translating this comment to english didnt really help me understanding the math! :-) (ton) */
	
	q[1] =  plane[1];
	q[2] = -plane[0];
	q[3] =  0;
	normalize_v3(&q[1]);

	angle = saacos(plane[2])/2.0f;
	co = cosf(angle);
	si = sqrtf(1-co*co);

	q[0] =  co;
	q[1] *= si;
	q[2] *= si;
	q[3] =  0;

	quat_to_mat3(mat1,q);

	/* rotate lamp vector now over acos(inp) degrees */
	copy_v3_v3(vvec, lvec);

	unit_m3(mat2);
	co = inp;
	si = sqrtf(1.0f-inp*inp);

	mat2[0][0] =  co;
	mat2[1][0] = -si;
	mat2[0][1] =  si;
	mat2[1][1] =  co;
	mul_m3_m3m3(mat3,mat2,mat1);

	mat2[1][0] =  si;
	mat2[0][1] = -si;
	mul_m3_m3m3(mat4,mat2,mat1);
	transpose_m3(mat1);

	mul_m3_m3m3(mat2,mat1,mat3);
	mul_m3_v3(mat2,lvec);
	mul_m3_m3m3(mat2,mat1,mat4);
	mul_m3_v3(mat2,vvec);

	return;
}

static void draw_spot_cone(Lamp *la, float x, float z)
{
	z= fabs(z);

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0f, 0.0f, -x);

	if (la->mode & LA_SQUARE) {
		glVertex3f(z, z, 0);
		glVertex3f(-z, z, 0);
		glVertex3f(-z, -z, 0);
		glVertex3f(z, -z, 0);
		glVertex3f(z, z, 0);
	}
	else {
		float angle;
		int a;

		for (a=0; a<33; a++) {
			angle= a*M_PI*2/(33-1);
			glVertex3f(z*cosf(angle), z*sinf(angle), 0);
		}
	}

	glEnd();
}

static void draw_transp_spot_volume(Lamp *la, float x, float z)
{
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glDepthMask(0);

	/* draw backside darkening */
	glCullFace(GL_FRONT);

	glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
	glColor4f(0.0f, 0.0f, 0.0f, 0.4f);

	draw_spot_cone(la, x, z);

	/* draw front side lighting */
	glCullFace(GL_BACK);

	glBlendFunc(GL_ONE, GL_ONE);
	glColor4f(0.2f, 0.2f, 0.2f, 1.0f);

	draw_spot_cone(la, x, z);

	/* restore state */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	glDepthMask(1);
	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

static void drawlamp(Scene *scene, View3D *v3d, RegionView3D *rv3d, Base *base, int dt, int flag)
{
	Object *ob= base->object;
	const float pixsize= ED_view3d_pixel_size(rv3d, ob->obmat[3]);
	Lamp *la= ob->data;
	float vec[3], lvec[3], vvec[3], circrad, x,y,z;
	float lampsize;
	float imat[4][4], curcol[4];
	unsigned char col[4];
	/* cone can't be drawn for duplicated lamps, because duplilist would be freed to */
	/* the moment of view3d_draw_transp() call */
	const short is_view= (rv3d->persp==RV3D_CAMOB && v3d->camera == base->object);
	const short drawcone= ((dt > OB_WIRE) &&
	                       !(G.f & G_PICKSEL) &&
	                       (la->type == LA_SPOT) &&
	                       (la->mode & LA_SHOW_CONE) &&
	                       !(base->flag & OB_FROMDUPLI) &&
	                       !is_view);

	if (drawcone && !v3d->transp) {
		/* in this case we need to draw delayed */
		add_view3d_after(&v3d->afterdraw_transp, base, flag);
		return;
	}
	
	/* we first draw only the screen aligned & fixed scale stuff */
	glPushMatrix();
	glLoadMatrixf(rv3d->viewmat);

	/* lets calculate the scale: */
	lampsize= pixsize*((float)U.obcenter_dia*0.5f);

	/* and view aligned matrix: */
	copy_m4_m4(imat, rv3d->viewinv);
	normalize_v3(imat[0]);
	normalize_v3(imat[1]);

	/* lamp center */
	copy_v3_v3(vec, ob->obmat[3]);
	
	/* for AA effects */
	glGetFloatv(GL_CURRENT_COLOR, curcol);
	curcol[3]= 0.6;
	glColor4fv(curcol);
	
	if (lampsize > 0.0f) {

		if (ob->id.us>1) {
			if (ob==OBACT || (ob->flag & SELECT)) glColor4ub(0x88, 0xFF, 0xFF, 155);
			else glColor4ub(0x77, 0xCC, 0xCC, 155);
		}
		
		/* Inner Circle */
		glEnable(GL_BLEND);
		drawcircball(GL_LINE_LOOP, vec, lampsize, imat);
		glDisable(GL_BLEND);
		drawcircball(GL_POLYGON, vec, lampsize, imat);
		
		/* restore */
		if (ob->id.us>1)
			glColor4fv(curcol);

		/* Outer circle */
		circrad = 3.0f*lampsize;
		setlinestyle(3);

		drawcircball(GL_LINE_LOOP, vec, circrad, imat);

		/* draw dashed outer circle if shadow is on. remember some lamps can't have certain shadows! */
		if (la->type!=LA_HEMI) {
			if ((la->mode & LA_SHAD_RAY) || ((la->mode & LA_SHAD_BUF) && (la->type == LA_SPOT))) {
				drawcircball(GL_LINE_LOOP, vec, circrad + 3.0f*pixsize, imat);
			}
		}
	}
	else {
		setlinestyle(3);
		circrad = 0.0f;
	}
	
	/* draw the pretty sun rays */
	if (la->type==LA_SUN) {
		float v1[3], v2[3], mat[3][3];
		short axis;
		
		/* setup a 45 degree rotation matrix */
		vec_rot_to_mat3(mat, imat[2], (float)M_PI/4.0f);
		
		/* vectors */
		mul_v3_v3fl(v1, imat[0], circrad * 1.2f);
		mul_v3_v3fl(v2, imat[0], circrad * 2.5f);
		
		/* center */
		glTranslatef(vec[0], vec[1], vec[2]);
		
		setlinestyle(3);
		
		glBegin(GL_LINES);
		for (axis=0; axis<8; axis++) {
			glVertex3fv(v1);
			glVertex3fv(v2);
			mul_m3_v3(mat, v1);
			mul_m3_v3(mat, v2);
		}
		glEnd();
		
		glTranslatef(-vec[0], -vec[1], -vec[2]);

	}
	
	if (la->type==LA_LOCAL) {
		if (la->mode & LA_SPHERE) {
			drawcircball(GL_LINE_LOOP, vec, la->dist, imat);
		}
		/* yafray: for photonlight also draw lightcone as for spot */
	}
	
	glPopMatrix();	/* back in object space */
	zero_v3(vec);
	
	if (is_view) {
		/* skip drawing extra info */
	}
	else if ((la->type==LA_SPOT) || (la->type==LA_YF_PHOTON)) {
		lvec[0]=lvec[1]= 0.0;
		lvec[2] = 1.0;
		x = rv3d->persmat[0][2];
		y = rv3d->persmat[1][2];
		z = rv3d->persmat[2][2];
		vvec[0]= x*ob->obmat[0][0] + y*ob->obmat[0][1] + z*ob->obmat[0][2];
		vvec[1]= x*ob->obmat[1][0] + y*ob->obmat[1][1] + z*ob->obmat[1][2];
		vvec[2]= x*ob->obmat[2][0] + y*ob->obmat[2][1] + z*ob->obmat[2][2];

		y = cosf(la->spotsize*(float)(M_PI/360.0));
		spotvolume(lvec, vvec, y);
		x = -la->dist;
		mul_v3_fl(lvec, x);
		mul_v3_fl(vvec, x);

		/* draw the angled sides of the cone */
		glBegin(GL_LINE_STRIP);
		glVertex3fv(vvec);
		glVertex3fv(vec);
		glVertex3fv(lvec);
		glEnd();
		
		z = x*sqrtf(1.0f - y*y);
		x *= y;

		/* draw the circle/square at the end of the cone */
		glTranslatef(0.0, 0.0 ,  x);
		if (la->mode & LA_SQUARE) {
			float tvec[3];
			float z_abs= fabs(z);

			tvec[0]= tvec[1]= z_abs;
			tvec[2]= 0.0;

			glBegin(GL_LINE_LOOP);
			glVertex3fv(tvec);
			tvec[1]= -z_abs; /* neg */
			glVertex3fv(tvec);
			tvec[0]= -z_abs; /* neg */
			glVertex3fv(tvec);
			tvec[1]= z_abs; /* pos */
			glVertex3fv(tvec);
			glEnd();
		}
		else circ(0.0, 0.0, fabsf(z));
		
		/* draw the circle/square representing spotbl */
		if (la->type==LA_SPOT) {
			float spotblcirc = fabs(z)*(1 - pow(la->spotblend, 2));
			/* hide line if it is zero size or overlaps with outer border,
			 * previously it adjusted to always to show it but that seems
			 * confusing because it doesn't show the actual blend size */
			if (spotblcirc != 0 && spotblcirc != fabsf(z))
				circ(0.0, 0.0, spotblcirc);
		}

		if (drawcone)
			draw_transp_spot_volume(la, x, z);

		/* draw clip start, useful for wide cones where its not obvious where the start is */
		glTranslatef(0.0, 0.0 , -x); /* reverse translation above */
		if (la->type==LA_SPOT && (la->mode & LA_SHAD_BUF) ) {
			float lvec_clip[3];
			float vvec_clip[3];
			float clipsta_fac= la->clipsta / -x;

			interp_v3_v3v3(lvec_clip, vec, lvec, clipsta_fac);
			interp_v3_v3v3(vvec_clip, vec, vvec, clipsta_fac);

			glBegin(GL_LINE_STRIP);
			glVertex3fv(lvec_clip);
			glVertex3fv(vvec_clip);
			glEnd();
		}
	}
	else if (ELEM(la->type, LA_HEMI, LA_SUN)) {
		
		/* draw the line from the circle along the dist */
		glBegin(GL_LINE_STRIP);
		vec[2] = -circrad;
		glVertex3fv(vec);
		vec[2]= -la->dist;
		glVertex3fv(vec);
		glEnd();
		
		if (la->type==LA_HEMI) {
			/* draw the hemisphere curves */
			short axis, steps, dir;
			float outdist, zdist, mul;
			zero_v3(vec);
			outdist = 0.14; mul = 1.4; dir = 1;
			
			setlinestyle(4);
			/* loop over the 4 compass points, and draw each arc as a LINE_STRIP */
			for (axis=0; axis<4; axis++) {
				float v[3]= {0.0, 0.0, 0.0};
				zdist = 0.02;
				
				glBegin(GL_LINE_STRIP);
				
				for (steps=0; steps<6; steps++) {
					if (axis == 0 || axis == 1) { 		/* x axis up, x axis down */
						/* make the arcs start at the edge of the energy circle */
						if (steps == 0) v[0] = dir*circrad;
						else v[0] = v[0] + dir*(steps*outdist);
					}
					else if (axis == 2 || axis == 3) { 		/* y axis up, y axis down */
						/* make the arcs start at the edge of the energy circle */
						v[1] = (steps == 0) ? (dir * circrad) : (v[1] + dir * (steps * outdist));
					}

					v[2] = v[2] - steps*zdist;
					
					glVertex3fv(v);
					
					zdist = zdist * mul;
				}
				
				glEnd();
				/* flip the direction */
				dir = -dir;
			}
		}
	}
	else if (la->type==LA_AREA) {
		setlinestyle(3);
		if (la->area_shape==LA_AREA_SQUARE)
			fdrawbox(-la->area_size*0.5f, -la->area_size*0.5f, la->area_size*0.5f, la->area_size*0.5f);
		else if (la->area_shape==LA_AREA_RECT)
			fdrawbox(-la->area_size*0.5f, -la->area_sizey*0.5f, la->area_size*0.5f, la->area_sizey*0.5f);

		glBegin(GL_LINE_STRIP);
		glVertex3f(0.0,0.0,-circrad);
		glVertex3f(0.0,0.0,-la->dist);
		glEnd();
	}
	
	/* and back to viewspace */
	glLoadMatrixf(rv3d->viewmat);
	copy_v3_v3(vec, ob->obmat[3]);

	setlinestyle(0);
	
	if ((la->type == LA_SPOT) && (la->mode & LA_SHAD_BUF) && (is_view == FALSE)) {
		drawshadbuflimits(la, ob->obmat);
	}
	
	UI_GetThemeColor4ubv(TH_LAMP, col);
	glColor4ubv(col);

	glEnable(GL_BLEND);
	
	if (vec[2]>0) vec[2] -= circrad;
	else vec[2] += circrad;
	
	glBegin(GL_LINE_STRIP);
	glVertex3fv(vec);
	vec[2]= 0;
	glVertex3fv(vec);
	glEnd();
	
	glPointSize(2.0);
	glBegin(GL_POINTS);
	glVertex3fv(vec);
	glEnd();
	glPointSize(1.0);
	
	glDisable(GL_BLEND);
	
	/* restore for drawing extra stuff */
	glColor3fv(curcol);

}

static void draw_limit_line(float sta, float end, unsigned int col)
{
	glBegin(GL_LINES);
	glVertex3f(0.0, 0.0, -sta);
	glVertex3f(0.0, 0.0, -end);
	glEnd();

	glPointSize(3.0);
	glBegin(GL_POINTS);
	cpack(col);
	glVertex3f(0.0, 0.0, -sta);
	glVertex3f(0.0, 0.0, -end);
	glEnd();
	glPointSize(1.0);
}		


/* yafray: draw camera focus point (cross, similar to aqsis code in tuhopuu) */
/* qdn: now also enabled for Blender to set focus point for defocus composite node */
static void draw_focus_cross(float dist, float size)
{
	glBegin(GL_LINES);
	glVertex3f(-size, 0.f, -dist);
	glVertex3f(size, 0.f, -dist);
	glVertex3f(0.f, -size, -dist);
	glVertex3f(0.f, size, -dist);
	glEnd();
}

#ifdef VIEW3D_CAMERA_BORDER_HACK
float view3d_camera_border_hack_col[4];
short view3d_camera_border_hack_test= FALSE;
#endif

/* ****************** draw clip data *************** */

static void draw_bundle_sphere(void)
{
	static GLuint displist= 0;

	if (displist == 0) {
		GLUquadricObj *qobj;

		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE);

		qobj= gluNewQuadric();
		gluQuadricDrawStyle(qobj, GLU_FILL);
		glShadeModel(GL_SMOOTH);
		gluSphere(qobj, 0.05, 8, 8);
		glShadeModel(GL_FLAT);
		gluDeleteQuadric(qobj);

		glEndList();
	}

	glCallList(displist);
}

static void draw_viewport_object_reconstruction(Scene *scene, Base *base, View3D *v3d,
                                                MovieClip *clip, MovieTrackingObject *tracking_object, int flag,
                                                int *global_track_index, int draw_selected)
{
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingTrack *track;
	float mat[4][4], imat[4][4];
	unsigned char col[4], scol[4];
	int tracknr= *global_track_index;
	ListBase *tracksbase= BKE_tracking_object_tracks(tracking, tracking_object);

	UI_GetThemeColor4ubv(TH_TEXT, col);
	UI_GetThemeColor4ubv(TH_SELECT, scol);

	BKE_get_tracking_mat(scene, base->object, mat);

	glPushMatrix();

	if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
		/* current ogl matrix is translated in camera space, bundles should
		 * be rendered in world space, so camera matrix should be "removed"
		 * from current ogl matrix */
		invert_m4_m4(imat, base->object->obmat);

		glMultMatrixf(imat);
		glMultMatrixf(mat);
	}
	else {
		float obmat[4][4];

		BKE_tracking_get_interpolated_camera(tracking, tracking_object, scene->r.cfra, obmat);

		invert_m4_m4(imat, obmat);
		glMultMatrixf(imat);
	}

	for (track= tracksbase->first; track; track= track->next) {
		int selected= TRACK_SELECTED(track);

		if (draw_selected && !selected)
			continue;

		if ((track->flag&TRACK_HAS_BUNDLE)==0)
			continue;

		if (flag&DRAW_PICKING)
			glLoadName(base->selcol + (tracknr<<16));

		glPushMatrix();
		glTranslatef(track->bundle_pos[0], track->bundle_pos[1], track->bundle_pos[2]);
		glScalef(v3d->bundle_size/0.05f, v3d->bundle_size/0.05f, v3d->bundle_size/0.05f);

		if (v3d->drawtype==OB_WIRE) {
			glDisable(GL_LIGHTING);

			if (selected) {
				if (base==BASACT) UI_ThemeColor(TH_ACTIVE);
				else UI_ThemeColor(TH_SELECT);
			}
			else {
				if (track->flag&TRACK_CUSTOMCOLOR) glColor3fv(track->color);
				else UI_ThemeColor(TH_WIRE);
			}

			drawaxes(0.05f, v3d->bundle_drawtype);

			glEnable(GL_LIGHTING);
		}
		else if (v3d->drawtype>OB_WIRE) {
			if (v3d->bundle_drawtype==OB_EMPTY_SPHERE) {
				/* selection outline */
				if (selected) {
					if (base==BASACT) UI_ThemeColor(TH_ACTIVE);
					else UI_ThemeColor(TH_SELECT);

					glLineWidth(2.f);
					glDisable(GL_LIGHTING);
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

					draw_bundle_sphere();

					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
					glEnable(GL_LIGHTING);
					glLineWidth(1.f);
				}

				if (track->flag&TRACK_CUSTOMCOLOR) glColor3fv(track->color);
				else UI_ThemeColor(TH_BUNDLE_SOLID);

				draw_bundle_sphere();
			}
			else {
				glDisable(GL_LIGHTING);

				if (selected) {
					if (base==BASACT) UI_ThemeColor(TH_ACTIVE);
					else UI_ThemeColor(TH_SELECT);
				}
				else {
					if (track->flag&TRACK_CUSTOMCOLOR) glColor3fv(track->color);
					else UI_ThemeColor(TH_WIRE);
				}

				drawaxes(0.05f, v3d->bundle_drawtype);

				glEnable(GL_LIGHTING);
			}
		}

		glPopMatrix();

		if ((flag & DRAW_PICKING)==0 && (v3d->flag2&V3D_SHOW_BUNDLENAME)) {
			float pos[3];
			unsigned char tcol[4];

			if (selected) memcpy(tcol, scol, sizeof(tcol));
			else memcpy(tcol, col, sizeof(tcol));

			mul_v3_m4v3(pos, mat, track->bundle_pos);
			view3d_cached_text_draw_add(pos, track->name, 10, V3D_CACHE_TEXT_GLOBALSPACE, tcol);
		}

		tracknr++;
	}

	if ((flag & DRAW_PICKING)==0) {
		if ((v3d->flag2&V3D_SHOW_CAMERAPATH) && (tracking_object->flag&TRACKING_OBJECT_CAMERA)) {
			MovieTrackingReconstruction *reconstruction;
			reconstruction= BKE_tracking_object_reconstruction(tracking, tracking_object);

			if (reconstruction->camnr) {
				MovieReconstructedCamera *camera= reconstruction->cameras;
				int a= 0;

				glDisable(GL_LIGHTING);
				UI_ThemeColor(TH_CAMERA_PATH);
				glLineWidth(2.0f);

				glBegin(GL_LINE_STRIP);
				for (a= 0; a<reconstruction->camnr; a++, camera++) {
					glVertex3fv(camera->mat[3]);
				}
				glEnd();

				glLineWidth(1.0f);
				glEnable(GL_LIGHTING);
			}
		}
	}

	glPopMatrix();

	*global_track_index= tracknr;
}

static void draw_viewport_reconstruction(Scene *scene, Base *base, View3D *v3d, MovieClip *clip,
                                         int flag, int draw_selected)
{
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingObject *tracking_object;
	float curcol[4];
	int global_track_index= 1;

	if ((v3d->flag2&V3D_SHOW_RECONSTRUCTION)==0)
		return;

	if (v3d->flag2&V3D_RENDER_OVERRIDE)
		return;

	glGetFloatv(GL_CURRENT_COLOR, curcol);

	glEnable(GL_LIGHTING);
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);

	tracking_object= tracking->objects.first;
	while (tracking_object) {
		draw_viewport_object_reconstruction(scene, base, v3d, clip, tracking_object,
		                                    flag, &global_track_index, draw_selected);

		tracking_object= tracking_object->next;
	}

	/* restore */
	glShadeModel(GL_FLAT);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);

	glColor4fv(curcol);

	if (flag&DRAW_PICKING)
		glLoadName(base->selcol);
}

/* flag similar to draw_object() */
static void drawcamera(Scene *scene, View3D *v3d, RegionView3D *rv3d, Base *base, int flag)
{
	/* a standing up pyramid with (0,0,0) as top */
	Camera *cam;
	Object *ob= base->object;
	float tvec[3];
	float vec[4][3], asp[2], shift[2], scale[3];
	int i;
	float drawsize;
	const short is_view= (rv3d->persp==RV3D_CAMOB && ob==v3d->camera);
	MovieClip *clip= object_get_movieclip(scene, base->object, 0);

	/* draw data for movie clip set as active for scene */
	if (clip) {
		draw_viewport_reconstruction(scene, base, v3d, clip, flag, FALSE);
		draw_viewport_reconstruction(scene, base, v3d, clip, flag, TRUE);
	}

#ifdef VIEW3D_CAMERA_BORDER_HACK
	if (is_view && !(G.f & G_PICKSEL)) {
		glGetFloatv(GL_CURRENT_COLOR, view3d_camera_border_hack_col);
		view3d_camera_border_hack_test= TRUE;
		return;
	}
#endif

	cam= ob->data;

	scale[0]= 1.0f / len_v3(ob->obmat[0]);
	scale[1]= 1.0f / len_v3(ob->obmat[1]);
	scale[2]= 1.0f / len_v3(ob->obmat[2]);

	camera_view_frame_ex(scene, cam, cam->drawsize, is_view, scale,
	                     asp, shift, &drawsize, vec);

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);

	/* camera frame */
	glBegin(GL_LINE_LOOP);
	glVertex3fv(vec[0]);
	glVertex3fv(vec[1]);
	glVertex3fv(vec[2]);
	glVertex3fv(vec[3]);
	glEnd();

	if (is_view)
		return;

	zero_v3(tvec);

	/* center point to camera frame */
	glBegin(GL_LINE_STRIP);
	glVertex3fv(vec[1]);
	glVertex3fv(tvec);
	glVertex3fv(vec[0]);
	glVertex3fv(vec[3]);
	glVertex3fv(tvec);
	glVertex3fv(vec[2]);
	glEnd();


	/* arrow on top */
	tvec[2]= vec[1][2]; /* copy the depth */


	/* draw an outline arrow for inactive cameras and filled
	 * for active cameras. We actually draw both outline+filled
	 * for active cameras so the wire can be seen side-on */
	for (i=0;i<2;i++) {
		if (i==0) glBegin(GL_LINE_LOOP);
		else if (i==1 && (ob == v3d->camera)) glBegin(GL_TRIANGLES);
		else break;

		tvec[0]= shift[0] + ((-0.7f * drawsize) * scale[0]);
		tvec[1]= shift[1] + ((drawsize * (asp[1] + 0.1f)) * scale[1]);
		glVertex3fv(tvec); /* left */
		
		tvec[0]= shift[0] + ((0.7f * drawsize) * scale[0]);
		glVertex3fv(tvec); /* right */
		
		tvec[0]= shift[0];
		tvec[1]= shift[1] + ((1.1f * drawsize * (asp[1] + 0.7f)) * scale[1]);
		glVertex3fv(tvec); /* top */

		glEnd();
	}

	if (flag==0) {
		if (cam->flag & (CAM_SHOWLIMITS+CAM_SHOWMIST)) {
			float nobmat[4][4];
			World *wrld;

			/* draw in normalized object matrix space */
			copy_m4_m4(nobmat, ob->obmat);
			normalize_m4(nobmat);

			glPushMatrix();
			glLoadMatrixf(rv3d->viewmat);
			glMultMatrixf(nobmat);

			if (cam->flag & CAM_SHOWLIMITS) {
				draw_limit_line(cam->clipsta, cam->clipend, 0x77FFFF);
				/* qdn: was yafray only, now also enabled for Blender to be used with defocus composite node */
				draw_focus_cross(object_camera_dof_distance(ob), cam->drawsize);
			}

			wrld= scene->world;
			if (cam->flag & CAM_SHOWMIST)
				if (wrld) draw_limit_line(wrld->miststa, wrld->miststa+wrld->mistdist, 0xFFFFFF);

			glPopMatrix();
		}
	}
}

/* flag similar to draw_object() */
static void drawspeaker(Scene *UNUSED(scene), View3D *UNUSED(v3d), RegionView3D *UNUSED(rv3d),
                        Object *UNUSED(ob), int UNUSED(flag))
{
	//Speaker *spk = ob->data;

	float vec[3];
	int i, j;

	glEnable(GL_BLEND);

	for (j = 0; j < 3; j++) {
		vec[2] = 0.25f * j -0.125f;

		glBegin(GL_LINE_LOOP);
		for (i = 0; i < 16; i++) {
			vec[0] = cosf((float)M_PI * i / 8.0f) * (j == 0 ? 0.5f : 0.25f);
			vec[1] = sinf((float)M_PI * i / 8.0f) * (j == 0 ? 0.5f : 0.25f);
			glVertex3fv(vec);
		}
		glEnd();
	}

	for (j = 0; j < 4; j++) {
		vec[0] = (((j + 1) % 2) * (j - 1)) * 0.5f;
		vec[1] = ((j % 2) * (j - 2)) * 0.5f;
		glBegin(GL_LINE_STRIP);
		for (i = 0; i < 3; i++) {
			if (i == 1) {
				vec[0] *= 0.5f;
				vec[1] *= 0.5f;
			}

			vec[2] = 0.25f * i -0.125f;
			glVertex3fv(vec);
		}
		glEnd();
	}

	glDisable(GL_BLEND);
}

static void lattice_draw_verts(Lattice *lt, DispList *dl, short sel)
{
	BPoint *bp = lt->def;
	float *co = dl?dl->verts:NULL;
	int u, v, w;

	UI_ThemeColor(sel?TH_VERTEX_SELECT:TH_VERTEX);
	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));
	bglBegin(GL_POINTS);

	for (w=0; w<lt->pntsw; w++) {
		int wxt = (w==0 || w==lt->pntsw-1);
		for (v=0; v<lt->pntsv; v++) {
			int vxt = (v==0 || v==lt->pntsv-1);
			for (u=0; u<lt->pntsu; u++, bp++, co+=3) {
				int uxt = (u==0 || u==lt->pntsu-1);
				if (!(lt->flag & LT_OUTSIDE) || uxt || vxt || wxt) {
					if (bp->hide==0) {
						if ((bp->f1 & SELECT)==sel) {
							bglVertex3fv(dl?co:bp->vec);
						}
					}
				}
			}
		}
	}
	
	glPointSize(1.0);
	bglEnd();
}

void lattice_foreachScreenVert(ViewContext *vc, void (*func)(void *userData, BPoint *bp, int x, int y), void *userData)
{
	Object *obedit= vc->obedit;
	Lattice *lt= obedit->data;
	BPoint *bp = lt->editlatt->latt->def;
	DispList *dl = find_displist(&obedit->disp, DL_VERTS);
	float *co = dl?dl->verts:NULL;
	int i, N = lt->editlatt->latt->pntsu*lt->editlatt->latt->pntsv*lt->editlatt->latt->pntsw;
	short s[2] = {IS_CLIPPED, 0};

	ED_view3d_clipping_local(vc->rv3d, obedit->obmat); /* for local clipping lookups */

	for (i=0; i<N; i++, bp++, co+=3) {
		if (bp->hide==0) {
			view3d_project_short_clip(vc->ar, dl?co:bp->vec, s, TRUE);
			if (s[0] != IS_CLIPPED)
				func(userData, bp, s[0], s[1]);
		}
	}
}

static void drawlattice__point(Lattice *lt, DispList *dl, int u, int v, int w, int use_wcol)
{
	int index = ((w*lt->pntsv + v)*lt->pntsu) + u;

	if (use_wcol) {
		float col[3];
		MDeformWeight *mdw= defvert_find_index (lt->dvert+index, use_wcol-1);
		
		weight_to_rgb(col, mdw?mdw->weight:0.0f);
		glColor3fv(col);

	}
	
	if (dl) {
		glVertex3fv(&dl->verts[index*3]);
	}
	else {
		glVertex3fv(lt->def[index].vec);
	}
}

/* lattice color is hardcoded, now also shows weightgroup values in edit mode */
static void drawlattice(Scene *scene, View3D *v3d, Object *ob)
{
	Lattice *lt= ob->data;
	DispList *dl;
	int u, v, w;
	int use_wcol= 0, is_edit= (lt->editlatt != NULL);

	/* now we default make displist, this will modifiers work for non animated case */
	if (ob->disp.first==NULL)
		lattice_calc_modifiers(scene, ob);
	dl= find_displist(&ob->disp, DL_VERTS);
	
	if (is_edit) {
		lt= lt->editlatt->latt;

		cpack(0x004000);
		
		if (ob->defbase.first && lt->dvert) {
			use_wcol= ob->actdef;
			glShadeModel(GL_SMOOTH);
		}
	}
	
	glBegin(GL_LINES);
	for (w=0; w<lt->pntsw; w++) {
		int wxt = (w==0 || w==lt->pntsw-1);
		for (v=0; v<lt->pntsv; v++) {
			int vxt = (v==0 || v==lt->pntsv-1);
			for (u=0; u<lt->pntsu; u++) {
				int uxt = (u==0 || u==lt->pntsu-1);

				if (w && ((uxt || vxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u, v, w-1, use_wcol);
					drawlattice__point(lt, dl, u, v, w, use_wcol);
				}
				if (v && ((uxt || wxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u, v-1, w, use_wcol);
					drawlattice__point(lt, dl, u, v, w, use_wcol);
				}
				if (u && ((vxt || wxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u-1, v, w, use_wcol);
					drawlattice__point(lt, dl, u, v, w, use_wcol);
				}
			}
		}
	}
	glEnd();
	
	/* restoration for weight colors */
	if (use_wcol)
		glShadeModel(GL_FLAT);

	if (is_edit) {
		if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
		
		lattice_draw_verts(lt, dl, 0);
		lattice_draw_verts(lt, dl, 1);
		
		if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	}
}

/* ***************** ******************** */

/* Note! - foreach funcs should be called while drawing or directly after
 * if not, ED_view3d_init_mats_rv3d() can be used for selection tools
 * but would not give correct results with dupli's for eg. which don't
 * use the object matrix in the usual way */
static void mesh_foreachScreenVert__mapFunc(void *userData, int index, float *co, float *UNUSED(no_f), short *UNUSED(no_s))
{
	foreachScreenVert_userData *data = userData;
	BMVert *eve = EDBM_get_vert_for_index(data->vc.em, index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
		short s[2]= {IS_CLIPPED, 0};

		if (data->clipVerts != V3D_CLIP_TEST_OFF) {
			view3d_project_short_clip(data->vc.ar, co, s, TRUE);
		}
		else {
			float co2[2];
			mul_v3_m4v3(co2, data->vc.obedit->obmat, co);
			project_short_noclip(data->vc.ar, co2, s);
		}

		if (s[0]!=IS_CLIPPED)
			data->func(data->userData, eve, s[0], s[1], index);
	}
}

void mesh_foreachScreenVert(
        ViewContext *vc,
        void (*func)(void *userData, BMVert *eve, int x, int y, int index),
        void *userData, eV3DClipTest clipVerts)
{
	foreachScreenVert_userData data;
	DerivedMesh *dm = editbmesh_get_derived_cage(vc->scene, vc->obedit, vc->em, CD_MASK_BAREMESH);
	
	data.vc= *vc;
	data.func = func;
	data.userData = userData;
	data.clipVerts = clipVerts;

	if (clipVerts != V3D_CLIP_TEST_OFF)
		ED_view3d_clipping_local(vc->rv3d, vc->obedit->obmat); /* for local clipping lookups */

	EDBM_init_index_arrays(vc->em, 1, 0, 0);
	dm->foreachMappedVert(dm, mesh_foreachScreenVert__mapFunc, &data);
	EDBM_free_index_arrays(vc->em);

	dm->release(dm);
}

/*  draw callback */
static void drawSelectedVertices__mapFunc(void *userData, int index, float *co, float *UNUSED(no_f), short *UNUSED(no_s))
{
	MVert *mv = &((MVert *)userData)[index];

	if (!(mv->flag & ME_HIDE)) {
		const char sel= mv->flag & SELECT;

		// TODO define selected color
		if (sel) {
			glColor3f(1.0f, 1.0f, 0.0f);
		}
		else {
			glColor3f(0.0f, 0.0f, 0.0f);
		}

		glVertex3fv(co);
	}
}

static void drawSelectedVertices(DerivedMesh *dm, Mesh *me)
{
	glBegin(GL_POINTS);
	dm->foreachMappedVert(dm, drawSelectedVertices__mapFunc, me->mvert);
	glEnd();
}
static int is_co_in_region(ARegion *ar, const short co[2])
{
	return ((co[0] != IS_CLIPPED) && /* may be the only initialized value, check first */
	        (co[0] >= 0)          &&
	        (co[0] <  ar->winx)   &&
	        (co[1] >= 0)          &&
	        (co[1] <  ar->winy));
}
static void mesh_foreachScreenEdge__mapFunc(void *userData, int index, float *v0co, float *v1co)
{
	foreachScreenEdge_userData *data = userData;
	BMEdge *eed = EDBM_get_edge_for_index(data->vc.em, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
		short s[2][2];

		if (data->clipVerts == V3D_CLIP_TEST_RV3D_CLIPPING) {
			view3d_project_short_clip(data->vc.ar, v0co, s[0], TRUE);
			view3d_project_short_clip(data->vc.ar, v1co, s[1], TRUE);
		}
		else {
			float v1_co[3], v2_co[3];

			mul_v3_m4v3(v1_co, data->vc.obedit->obmat, v0co);
			mul_v3_m4v3(v2_co, data->vc.obedit->obmat, v1co);

			project_short_noclip(data->vc.ar, v1_co, s[0]);
			project_short_noclip(data->vc.ar, v2_co, s[1]);

			if (data->clipVerts == V3D_CLIP_TEST_REGION) {
				if ( !is_co_in_region(data->vc.ar, s[0]) &&
				     !is_co_in_region(data->vc.ar, s[1]))
				{
					return;
				}
			}
		}

		data->func(data->userData, eed, s[0][0], s[0][1], s[1][0], s[1][1], index);
	}
}

void mesh_foreachScreenEdge(
        ViewContext *vc,
        void (*func)(void *userData, BMEdge *eed, int x0, int y0, int x1, int y1, int index),
        void *userData, eV3DClipTest clipVerts)
{
	foreachScreenEdge_userData data;
	DerivedMesh *dm = editbmesh_get_derived_cage(vc->scene, vc->obedit, vc->em, CD_MASK_BAREMESH);

	data.vc= *vc;
	data.func = func;
	data.userData = userData;
	data.clipVerts = clipVerts;

	if (clipVerts != V3D_CLIP_TEST_OFF)
		ED_view3d_clipping_local(vc->rv3d, vc->obedit->obmat); /* for local clipping lookups */

	EDBM_init_index_arrays(vc->em, 0, 1, 0);
	dm->foreachMappedEdge(dm, mesh_foreachScreenEdge__mapFunc, &data);
	EDBM_free_index_arrays(vc->em);

	dm->release(dm);
}

static void mesh_foreachScreenFace__mapFunc(void *userData, int index, float *cent, float *UNUSED(no))
{
	foreachScreenFace_userData *data = userData;
	BMFace *efa = EDBM_get_face_for_index(data->vc.em, index);

	if (efa && !BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		float cent2[3];
		short s[2];

		mul_v3_m4v3(cent2, data->vc.obedit->obmat, cent);
		project_short(data->vc.ar, cent2, s);

		if (s[0] != IS_CLIPPED) {
			data->func(data->userData, efa, s[0], s[1], index);
		}
	}
}

void mesh_foreachScreenFace(
        ViewContext *vc,
        void (*func)(void *userData, BMFace *efa, int x, int y, int index),
        void *userData)
{
	foreachScreenFace_userData data;
	DerivedMesh *dm = editbmesh_get_derived_cage(vc->scene, vc->obedit, vc->em, CD_MASK_BAREMESH);

	data.vc= *vc;
	data.func = func;
	data.userData = userData;

	//if (clipVerts)
	ED_view3d_clipping_local(vc->rv3d, vc->obedit->obmat); /* for local clipping lookups */

	EDBM_init_index_arrays(vc->em, 0, 0, 1);
	dm->foreachMappedFaceCenter(dm, mesh_foreachScreenFace__mapFunc, &data);
	EDBM_free_index_arrays(vc->em);

	dm->release(dm);
}

void nurbs_foreachScreenVert(
        ViewContext *vc,
        void (*func)(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y),
        void *userData)
{
	Curve *cu= vc->obedit->data;
	short s[2] = {IS_CLIPPED, 0};
	Nurb *nu;
	int i;
	ListBase *nurbs= curve_editnurbs(cu);

	ED_view3d_clipping_local(vc->rv3d, vc->obedit->obmat); /* for local clipping lookups */

	for (nu= nurbs->first; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER) {
			for (i=0; i<nu->pntsu; i++) {
				BezTriple *bezt = &nu->bezt[i];

				if (bezt->hide==0) {
					
					if (cu->drawflag & CU_HIDE_HANDLES) {
						view3d_project_short_clip(vc->ar, bezt->vec[1], s, TRUE);
						if (s[0] != IS_CLIPPED)
							func(userData, nu, NULL, bezt, 1, s[0], s[1]);
					}
					else {
						view3d_project_short_clip(vc->ar, bezt->vec[0], s, TRUE);
						if (s[0] != IS_CLIPPED)
							func(userData, nu, NULL, bezt, 0, s[0], s[1]);
						view3d_project_short_clip(vc->ar, bezt->vec[1], s, TRUE);
						if (s[0] != IS_CLIPPED)
							func(userData, nu, NULL, bezt, 1, s[0], s[1]);
						view3d_project_short_clip(vc->ar, bezt->vec[2], s, TRUE);
						if (s[0] != IS_CLIPPED)
							func(userData, nu, NULL, bezt, 2, s[0], s[1]);
					}
				}
			}
		}
		else {
			for (i=0; i<nu->pntsu*nu->pntsv; i++) {
				BPoint *bp = &nu->bp[i];

				if (bp->hide==0) {
					view3d_project_short_clip(vc->ar, bp->vec, s, TRUE);
					if (s[0] != IS_CLIPPED)
						func(userData, nu, bp, NULL, -1, s[0], s[1]);
				}
			}
		}
	}
}

/* ************** DRAW MESH ****************** */

/* First section is all the "simple" draw routines, 
 * ones that just pass some sort of primitive to GL,
 * with perhaps various options to control lighting,
 * color, etc.
 *
 * These routines should not have user interface related
 * logic!!!
 */

static void draw_dm_face_normals__mapFunc(void *userData, int index, float *cent, float *no)
{
	drawDMNormal_userData *data = userData;
	BMFace *efa = EDBM_get_face_for_index(data->em, index);

	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		glVertex3fv(cent);
		glVertex3f(cent[0] + no[0] * data->normalsize,
		           cent[1] + no[1] * data->normalsize,
		           cent[2] + no[2] * data->normalsize);
	}
}
static void draw_dm_face_normals(BMEditMesh *em, Scene *scene, DerivedMesh *dm) 
{
	drawDMNormal_userData data;

	data.em = em;
	data.normalsize = scene->toolsettings->normalsize;

	glBegin(GL_LINES);
	dm->foreachMappedFaceCenter(dm, draw_dm_face_normals__mapFunc, &data);
	glEnd();
}

static void draw_dm_face_centers__mapFunc(void *userData, int index, float *cent, float *UNUSED(no))
{
	BMFace *efa = EDBM_get_face_for_index(((void **)userData)[0], index);
	int sel = *(((int **)userData)[1]);
	
	if (efa && !BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && BM_elem_flag_test(efa, BM_ELEM_SELECT)==sel) {
		bglVertex3fv(cent);
	}
}
static void draw_dm_face_centers(BMEditMesh *em, DerivedMesh *dm, int sel)
{
	void *ptrs[2] = {em, &sel};

	bglBegin(GL_POINTS);
	dm->foreachMappedFaceCenter(dm, draw_dm_face_centers__mapFunc, ptrs);
	bglEnd();
}

static void draw_dm_vert_normals__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	drawDMNormal_userData *data = userData;
	BMVert *eve = EDBM_get_vert_for_index(data->em, index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
		glVertex3fv(co);

		if (no_f) {
			glVertex3f(co[0] + no_f[0] * data->normalsize,
			           co[1] + no_f[1] * data->normalsize,
			           co[2] + no_f[2] * data->normalsize);
		}
		else {
			glVertex3f(co[0] + no_s[0] * (data->normalsize / 32767.0f),
			           co[1] + no_s[1] * (data->normalsize / 32767.0f),
			           co[2] + no_s[2] * (data->normalsize / 32767.0f));
		}
	}
}
static void draw_dm_vert_normals(BMEditMesh *em, Scene *scene, DerivedMesh *dm) 
{
	drawDMNormal_userData data;

	data.em = em;
	data.normalsize = scene->toolsettings->normalsize;

	glBegin(GL_LINES);
	dm->foreachMappedVert(dm, draw_dm_vert_normals__mapFunc, &data);
	glEnd();
}

/* Draw verts with color set based on selection */
static void draw_dm_verts__mapFunc(void *userData, int index, float *co, float *UNUSED(no_f), short *UNUSED(no_s))
{
	drawDMVerts_userData * data = userData;
	BMVert *eve = EDBM_get_vert_for_index(data->em, index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && BM_elem_flag_test(eve, BM_ELEM_SELECT)==data->sel) {
		/* draw active larger - need to stop/start point drawing for this :/ */
		if (eve==data->eve_act) {
			float size = UI_GetThemeValuef(TH_VERTEX_SIZE);
			UI_ThemeColor4(TH_EDITMESH_ACTIVE);
			
			bglEnd();
			
			glPointSize(size);
			bglBegin(GL_POINTS);
			bglVertex3fv(co);
			bglEnd();
			
			UI_ThemeColor4(data->sel?TH_VERTEX_SELECT:TH_VERTEX);
			glPointSize(size);
			bglBegin(GL_POINTS);
		}
		else {
			bglVertex3fv(co);
		}
	}
}

static void draw_dm_verts(BMEditMesh *em, DerivedMesh *dm, int sel, BMVert *eve_act)
{
	drawDMVerts_userData data;
	data.sel = sel;
	data.eve_act = eve_act;
	data.em = em;

	bglBegin(GL_POINTS);
	dm->foreachMappedVert(dm, draw_dm_verts__mapFunc, &data);
	bglEnd();
}

/* Draw edges with color set based on selection */
static DMDrawOption draw_dm_edges_sel__setDrawOptions(void *userData, int index)
{
	BMEdge *eed;
	//unsigned char **cols = userData, *col;
	drawDMEdgesSel_userData * data = userData;
	unsigned char *col;

	eed = EDBM_get_edge_for_index(data->em, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
		if (eed==data->eed_act) {
			glColor4ubv(data->actCol);
		}
		else {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
				col = data->selCol;
			}
			else {
				col = data->baseCol;
			}
			/* no alpha, this is used so a transparent color can disable drawing unselected edges in editmode  */
			if (col[3]==0)
				return DM_DRAW_OPTION_SKIP;
			
			glColor4ubv(col);
		}
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}
static void draw_dm_edges_sel(BMEditMesh *em, DerivedMesh *dm, unsigned char *baseCol, 
                              unsigned char *selCol, unsigned char *actCol, BMEdge *eed_act)
{
	drawDMEdgesSel_userData data;
	
	data.baseCol = baseCol;
	data.selCol = selCol;
	data.actCol = actCol;
	data.em = em;
	data.eed_act = eed_act;
	dm->drawMappedEdges(dm, draw_dm_edges_sel__setDrawOptions, &data);
}

/* Draw edges */
static DMDrawOption draw_dm_edges__setDrawOptions(void *userData, int index)
{
	if (BM_elem_flag_test(EDBM_get_edge_for_index(userData, index), BM_ELEM_HIDDEN))
		return DM_DRAW_OPTION_SKIP;
	else
		return DM_DRAW_OPTION_NORMAL;
}

static void draw_dm_edges(BMEditMesh *em, DerivedMesh *dm) 
{
	dm->drawMappedEdges(dm, draw_dm_edges__setDrawOptions, em);
}

/* Draw edges with color interpolated based on selection */
static DMDrawOption draw_dm_edges_sel_interp__setDrawOptions(void *userData, int index)
{
	if (BM_elem_flag_test(EDBM_get_edge_for_index(((void**)userData)[0], index), BM_ELEM_HIDDEN))
		return DM_DRAW_OPTION_SKIP;
	else
		return DM_DRAW_OPTION_NORMAL;
}
static void draw_dm_edges_sel_interp__setDrawInterpOptions(void *userData, int index, float t)
{
	BMEdge *eed = EDBM_get_edge_for_index(((void**)userData)[0], index);
	unsigned char **cols = userData;
	unsigned char *col0 = cols[(BM_elem_flag_test(eed->v1, BM_ELEM_SELECT))?2:1];
	unsigned char *col1 = cols[(BM_elem_flag_test(eed->v2, BM_ELEM_SELECT))?2:1];

	glColor4ub(col0[0] + (col1[0] - col0[0]) * t,
	           col0[1] + (col1[1] - col0[1]) * t,
	           col0[2] + (col1[2] - col0[2]) * t,
	           col0[3] + (col1[3] - col0[3]) * t);
}

static void draw_dm_edges_sel_interp(BMEditMesh *em, DerivedMesh *dm, unsigned char *baseCol, unsigned char *selCol)
{
	void *cols[3] = {em, baseCol, selCol};

	dm->drawMappedEdgesInterp(dm, draw_dm_edges_sel_interp__setDrawOptions, draw_dm_edges_sel_interp__setDrawInterpOptions, cols);
}

/* Draw only seam edges */
static DMDrawOption draw_dm_edges_seams__setDrawOptions(void *userData, int index)
{
	BMEdge *eed = EDBM_get_edge_for_index(userData, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && BM_elem_flag_test(eed, BM_ELEM_SEAM))
		return DM_DRAW_OPTION_NORMAL;
	else
		return DM_DRAW_OPTION_SKIP;
}

static void draw_dm_edges_seams(BMEditMesh *em, DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges_seams__setDrawOptions, em);
}

/* Draw only sharp edges */
static DMDrawOption draw_dm_edges_sharp__setDrawOptions(void *userData, int index)
{
	BMEdge *eed = EDBM_get_edge_for_index(userData, index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && !BM_elem_flag_test(eed, BM_ELEM_SMOOTH))
		return DM_DRAW_OPTION_NORMAL;
	else
		return DM_DRAW_OPTION_SKIP;
}

static void draw_dm_edges_sharp(BMEditMesh *em, DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges_sharp__setDrawOptions, em);
}


/* Draw faces with color set based on selection
	 * return 2 for the active face so it renders with stipple enabled */
static DMDrawOption draw_dm_faces_sel__setDrawOptions(void *userData, int index)
{
	drawDMFacesSel_userData * data = userData;
	BMFace *efa = EDBM_get_face_for_index(data->em, index);
	unsigned char *col;
	
	if (!efa)
		return DM_DRAW_OPTION_SKIP;
	
	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		if (efa == data->efa_act) {
			glColor4ubv(data->cols[2]);
			return DM_DRAW_OPTION_STIPPLE;
		}
		else {
			col = data->cols[BM_elem_flag_test(efa, BM_ELEM_SELECT)?1:0];
			if (col[3]==0)
				return DM_DRAW_OPTION_SKIP;
			glColor4ubv(col);
			return DM_DRAW_OPTION_NORMAL;
		}
	}
	return DM_DRAW_OPTION_SKIP;
}

static int draw_dm_faces_sel__compareDrawOptions(void *userData, int index, int next_index)
{

	drawDMFacesSel_userData *data = userData;
	BMFace *efa;
	BMFace *next_efa;

	unsigned char *col, *next_col;

	if (!data->orig_index)
		return 0;

	efa= EDBM_get_face_for_index(data->em, data->orig_index[index]);
	next_efa= EDBM_get_face_for_index(data->em, data->orig_index[next_index]);

	if (efa == next_efa)
		return 1;

	if (efa == data->efa_act || next_efa == data->efa_act)
		return 0;

	col = data->cols[BM_elem_flag_test(efa, BM_ELEM_SELECT)?1:0];
	next_col = data->cols[BM_elem_flag_test(next_efa, BM_ELEM_SELECT)?1:0];

	if (col[3]==0 || next_col[3]==0)
		return 0;

	return col == next_col;
}

/* also draws the active face */
static void draw_dm_faces_sel(BMEditMesh *em, DerivedMesh *dm, unsigned char *baseCol, 
                              unsigned char *selCol, unsigned char *actCol, BMFace *efa_act)
{
	drawDMFacesSel_userData data;
	data.dm= dm;
	data.cols[0] = baseCol;
	data.em = em;
	data.cols[1] = selCol;
	data.cols[2] = actCol;
	data.efa_act = efa_act;
	data.orig_index = DM_get_tessface_data_layer(dm, CD_ORIGINDEX);

	dm->drawMappedFaces(dm, draw_dm_faces_sel__setDrawOptions, GPU_enable_material, draw_dm_faces_sel__compareDrawOptions, &data, 0);
}

static DMDrawOption draw_dm_creases__setDrawOptions(void *userData, int index)
{
	BMEditMesh *em = userData;
	BMEdge *eed = EDBM_get_edge_for_index(userData, index);
	float *crease = eed ? (float *)CustomData_bmesh_get(&em->bm->edata, eed->head.data, CD_CREASE) : NULL;
	
	if (!crease)
		return DM_DRAW_OPTION_SKIP;
	
	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && *crease!=0.0f) {
		UI_ThemeColorBlend(TH_WIRE, TH_EDGE_CREASE, *crease);
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}
static void draw_dm_creases(BMEditMesh *em, DerivedMesh *dm)
{
	glLineWidth(3.0);
	dm->drawMappedEdges(dm, draw_dm_creases__setDrawOptions, em);
	glLineWidth(1.0);
}

static DMDrawOption draw_dm_bweights__setDrawOptions(void *userData, int index)
{
	BMEditMesh *em = userData;
	BMEdge *eed = EDBM_get_edge_for_index(userData, index);
	float *bweight = (float *)CustomData_bmesh_get(&em->bm->edata, eed->head.data, CD_BWEIGHT);

	if (!bweight)
		return DM_DRAW_OPTION_SKIP;
	
	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && *bweight!=0.0f) {
		UI_ThemeColorBlend(TH_WIRE, TH_EDGE_SELECT, *bweight);
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}
static void draw_dm_bweights__mapFunc(void *userData, int index, float *co, float *UNUSED(no_f), short *UNUSED(no_s))
{
	BMEditMesh *em = userData;
	BMVert *eve = EDBM_get_vert_for_index(userData, index);
	float *bweight = (float *)CustomData_bmesh_get(&em->bm->vdata, eve->head.data, CD_BWEIGHT);
	
	if (!bweight)
		return;
	
	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && *bweight!=0.0f) {
		UI_ThemeColorBlend(TH_VERTEX, TH_VERTEX_SELECT, *bweight);
		bglVertex3fv(co);
	}
}
static void draw_dm_bweights(BMEditMesh *em, Scene *scene, DerivedMesh *dm)
{
	ToolSettings *ts= scene->toolsettings;

	if (ts->selectmode & SCE_SELECT_VERTEX) {
		glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE) + 2);
		bglBegin(GL_POINTS);
		dm->foreachMappedVert(dm, draw_dm_bweights__mapFunc, em);
		bglEnd();
	}
	else {
		glLineWidth(3.0);
		dm->drawMappedEdges(dm, draw_dm_bweights__setDrawOptions, em);
		glLineWidth(1.0);
	}
}

/* Second section of routines: Combine first sets to form fancy
 * drawing routines (for example rendering twice to get overlays).
 *
 * Also includes routines that are basic drawing but are too
 * specialized to be split out (like drawing creases or measurements).
 */

/* EditMesh drawing routines*/

static void draw_em_fancy_verts(Scene *scene, View3D *v3d, Object *obedit, 
                                BMEditMesh *em, DerivedMesh *cageDM, BMVert *eve_act)
{
	ToolSettings *ts= scene->toolsettings;
	int sel;

	if (v3d->zbuf) glDepthMask(0);		// disable write in zbuffer, zbuf select

	for (sel=0; sel<2; sel++) {
		unsigned char col[4], fcol[4];
		int pass;

		UI_GetThemeColor3ubv(sel?TH_VERTEX_SELECT:TH_VERTEX, col);
		UI_GetThemeColor3ubv(sel?TH_FACE_DOT:TH_WIRE, fcol);

		for (pass=0; pass<2; pass++) {
			float size = UI_GetThemeValuef(TH_VERTEX_SIZE);
			float fsize = UI_GetThemeValuef(TH_FACEDOT_SIZE);

			if (pass==0) {
				if (v3d->zbuf && !(v3d->flag&V3D_ZBUF_SELECT)) {
					glDisable(GL_DEPTH_TEST);

					glEnable(GL_BLEND);
				}
				else {
					continue;
				}

				size = (size > 2.1f ? size/2.0f:size);
				fsize = (fsize > 2.1f ? fsize/2.0f:fsize);
				col[3] = fcol[3] = 100;
			}
			else {
				col[3] = fcol[3] = 255;
			}

			if (ts->selectmode & SCE_SELECT_VERTEX) {
				glPointSize(size);
				glColor4ubv(col);
				draw_dm_verts(em, cageDM, sel, eve_act);
			}
			
			if (check_ob_drawface_dot(scene, v3d, obedit->dt)) {
				glPointSize(fsize);
				glColor4ubv(fcol);
				draw_dm_face_centers(em, cageDM, sel);
			}
			
			if (pass==0) {
				glDisable(GL_BLEND);
				glEnable(GL_DEPTH_TEST);
			}
		}
	}

	if (v3d->zbuf) glDepthMask(1);
	glPointSize(1.0);
}

static void draw_em_fancy_edges(BMEditMesh *em, Scene *scene, View3D *v3d,
                                Mesh *me, DerivedMesh *cageDM, short sel_only,
                                BMEdge *eed_act)
{
	ToolSettings *ts= scene->toolsettings;
	int pass;
	unsigned char wireCol[4], selCol[4], actCol[4];

	/* since this function does transparant... */
	UI_GetThemeColor4ubv(TH_EDGE_SELECT, selCol);
	UI_GetThemeColor4ubv(TH_WIRE, wireCol);
	UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, actCol);
	
	/* when sel only is used, don't render wire, only selected, this is used for
	 * textured draw mode when the 'edges' option is disabled */
	if (sel_only)
		wireCol[3] = 0;

	for (pass=0; pass<2; pass++) {
		/* show wires in transparant when no zbuf clipping for select */
		if (pass==0) {
			if (v3d->zbuf && (v3d->flag & V3D_ZBUF_SELECT)==0) {
				glEnable(GL_BLEND);
				glDisable(GL_DEPTH_TEST);
				selCol[3] = 85;
				if (!sel_only) wireCol[3] = 85;
			}
			else {
				continue;
			}
		}
		else {
			selCol[3] = 255;
			if (!sel_only) wireCol[3] = 255;
		}

		if (ts->selectmode == SCE_SELECT_FACE) {
			draw_dm_edges_sel(em, cageDM, wireCol, selCol, actCol, eed_act);
		}
		else if ( (me->drawflag & ME_DRAWEDGES) || (ts->selectmode & SCE_SELECT_EDGE) ) {
			if (cageDM->drawMappedEdgesInterp && (ts->selectmode & SCE_SELECT_VERTEX)) {
				glShadeModel(GL_SMOOTH);
				draw_dm_edges_sel_interp(em, cageDM, wireCol, selCol);
				glShadeModel(GL_FLAT);
			}
			else {
				draw_dm_edges_sel(em, cageDM, wireCol, selCol, actCol, eed_act);
			}
		}
		else {
			if (!sel_only) {
				glColor4ubv(wireCol);
				draw_dm_edges(em, cageDM);
			}
		}

		if (pass==0) {
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
		}
	}
}	

static void draw_em_measure_stats(View3D *v3d, Object *ob, BMEditMesh *em, UnitSettings *unit)
{
	const short txt_flag = V3D_CACHE_TEXT_ASCII | V3D_CACHE_TEXT_LOCALCLIP;
	Mesh *me= ob->data;
	float v1[3], v2[3], v3[3], vmid[3], fvec[3];
	char numstr[32]; /* Stores the measurement display text here */
	const char *conv_float; /* Use a float conversion matching the grid size */
	unsigned char col[4]= {0, 0, 0, 255}; /* color of the text to draw */
	float area; /* area of the face */
	float grid= unit->system ? unit->scale_length : v3d->grid;
	const int do_split= unit->flag & USER_UNIT_OPT_SPLIT;
	const int do_global= v3d->flag & V3D_GLOBAL_STATS;
	const int do_moving= G.moving;

	BMIter iter;
	int i;

	/* make the precision of the pronted value proportionate to the gridsize */

	if (grid < 0.01f)		conv_float= "%.6g";
	else if (grid < 0.1f)	conv_float= "%.5g";
	else if (grid < 1.0f)	conv_float= "%.4g";
	else if (grid < 10.0f)	conv_float= "%.3g";
	else					conv_float= "%.2g";
	
	if (me->drawflag & ME_DRAWEXTRA_EDGELEN) {
		BMEdge *eed;

		UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGELEN, col);

		eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for (; eed; eed=BM_iter_step(&iter)) {
			/* draw selected edges, or edges next to selected verts while draging */
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT) ||
			    (do_moving && (BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) ||
			                   BM_elem_flag_test(eed->v2, BM_ELEM_SELECT))))
			{

				copy_v3_v3(v1, eed->v1->co);
				copy_v3_v3(v2, eed->v2->co);

				mid_v3_v3v3(vmid, v1, v2);

				if (do_global) {
					mul_mat3_m4_v3(ob->obmat, v1);
					mul_mat3_m4_v3(ob->obmat, v2);
				}

				if (unit->system) {
					bUnit_AsString(numstr, sizeof(numstr), len_v3v3(v1, v2) * unit->scale_length, 3,
					               unit->system, B_UNIT_LENGTH, do_split, FALSE);
				}
				else {
					sprintf(numstr, conv_float, len_v3v3(v1, v2));
				}

				view3d_cached_text_draw_add(vmid, numstr, 0, txt_flag, col);
			}
		}
	}

	if (me->drawflag & ME_DRAWEXTRA_FACEAREA) {
		/* would be nice to use BM_face_area_calc, but that is for 2d faces
		 * so instead add up tessellation triangle areas */
		BMFace *f;
		int n;

#define DRAW_EM_MEASURE_STATS_FACEAREA()                                             \
		if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {                                  \
			mul_v3_fl(vmid, 1.0/n);                                                  \
			if (unit->system)                                                        \
				bUnit_AsString(numstr, sizeof(numstr), area*unit->scale_length,      \
					3, unit->system, B_UNIT_LENGTH, do_split, FALSE);                \
			else                                                                     \
				BLI_snprintf(numstr, sizeof(numstr), conv_float, area);              \
			view3d_cached_text_draw_add(vmid, numstr, 0, txt_flag, col);             \
		}

		UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEAREA, col);
		
		f = NULL;
		area = 0.0;
		zero_v3(vmid);
		n = 0;
		for (i = 0; i < em->tottri; i++) {
			BMLoop **l = em->looptris[i];
			if (f && l[0]->f != f) {
				DRAW_EM_MEASURE_STATS_FACEAREA();
				zero_v3(vmid);
				area = 0.0;
				n = 0;
			}

			f = l[0]->f;
			copy_v3_v3(v1, l[0]->v->co);
			copy_v3_v3(v2, l[1]->v->co);
			copy_v3_v3(v3, l[2]->v->co);
			if (do_global) {
				mul_mat3_m4_v3(ob->obmat, v1);
				mul_mat3_m4_v3(ob->obmat, v2);
				mul_mat3_m4_v3(ob->obmat, v3);
			}
			area += area_tri_v3(v1, v2, v3);
			add_v3_v3(vmid, v1);
			add_v3_v3(vmid, v2);
			add_v3_v3(vmid, v3);
			n += 3;
		}

		if (f) {
			DRAW_EM_MEASURE_STATS_FACEAREA();
		}
#undef DRAW_EM_MEASURE_STATS_FACEAREA
	}

	if (me->drawflag & ME_DRAWEXTRA_FACEANG) {
		BMFace *efa;

		UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEANG, col);


		for (efa = BM_iter_new(&iter, em->bm, BM_FACES_OF_MESH, NULL);
		     efa; efa=BM_iter_step(&iter))
		{
			BMIter liter;
			BMLoop *loop;

			BM_face_center_bounds_calc(em->bm, efa, vmid);

			for (loop = BM_iter_new(&liter, em->bm, BM_LOOPS_OF_FACE, efa);
			     loop; loop = BM_iter_step(&liter))
			{
				float v1[3], v2[3], v3[3];

				copy_v3_v3(v1, loop->prev->v->co);
				copy_v3_v3(v2, loop->v->co);
				copy_v3_v3(v3, loop->next->v->co);

				if (do_global) {
					mul_mat3_m4_v3(ob->obmat, v1);
					mul_mat3_m4_v3(ob->obmat, v2);
					mul_mat3_m4_v3(ob->obmat, v3);
				}

				if ( (BM_elem_flag_test(efa, BM_ELEM_SELECT)) ||
				     (do_moving && BM_elem_flag_test(loop->v, BM_ELEM_SELECT)))
				{
					BLI_snprintf(numstr, sizeof(numstr), "%.3g", RAD2DEGF(angle_v3v3v3(v1, v2, v3)));
					interp_v3_v3v3(fvec, vmid, v2, 0.8f);
					view3d_cached_text_draw_add(fvec, numstr, 0, txt_flag, col);
				}
			}
		}
	}
}

static void draw_em_indices(BMEditMesh *em)
{
	const short txt_flag = V3D_CACHE_TEXT_ASCII | V3D_CACHE_TEXT_LOCALCLIP;
	BMEdge *e;
	BMFace *f;
	BMVert *v;
	int i;
	char numstr[32];
	float pos[3];
	unsigned char col[4];

	BMIter iter;
	BMesh *bm= em->bm;

	/* For now, reuse appropriate theme colors from stats text colors */
	i= 0;
	if (em->selectmode & SCE_SELECT_VERTEX) {
		UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEANG, col);
		BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				sprintf(numstr, "%d", i);
				view3d_cached_text_draw_add(v->co, numstr, 0, txt_flag, col);
			}
			i++;
		}
	}

	if (em->selectmode & SCE_SELECT_EDGE) {
		i= 0;
		UI_GetThemeColor3ubv(TH_DRAWEXTRA_EDGELEN, col);
		BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
				sprintf(numstr, "%d", i);
				mid_v3_v3v3(pos, e->v1->co, e->v2->co);
				view3d_cached_text_draw_add(pos, numstr, 0, txt_flag, col);
			}
			i++;
		}
	}

	if (em->selectmode & SCE_SELECT_FACE) {
		i= 0;
		UI_GetThemeColor3ubv(TH_DRAWEXTRA_FACEAREA, col);
		BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
			if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
				BM_face_center_mean_calc(bm, f, pos);
				sprintf(numstr, "%d", i);
				view3d_cached_text_draw_add(pos, numstr, 0, txt_flag, col);
			}
			i++;
		}
	}
}

static DMDrawOption draw_em_fancy__setFaceOpts(void *userData, int index)
{
	BMFace *efa = EDBM_get_face_for_index(userData, index);

	if (efa && !BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		GPU_enable_material(efa->mat_nr+1, NULL);
		return DM_DRAW_OPTION_NORMAL;
	}
	else
		return DM_DRAW_OPTION_SKIP;
}

static DMDrawOption draw_em_fancy__setGLSLFaceOpts(void *userData, int index)
{
	BMFace *efa = EDBM_get_face_for_index(userData, index);

	if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
		return DM_DRAW_OPTION_SKIP;
	else
		return DM_DRAW_OPTION_NORMAL;
}

static void draw_em_fancy(Scene *scene, View3D *v3d, RegionView3D *rv3d,
                          Object *ob, BMEditMesh *em, DerivedMesh *cageDM, DerivedMesh *finalDM, int dt)

{
	Mesh *me = ob->data;
	BMFace *efa_act = BM_active_face_get(em->bm, FALSE); /* annoying but active faces is stored differently */
	BMEdge *eed_act = NULL;
	BMVert *eve_act = NULL;
	
	if (em->bm->selected.last) {
		BMEditSelection *ese= em->bm->selected.last;
		/* face is handeled above */
#if 0
		if (ese->type == BM_FACE ) {
			efa_act = (BMFace *)ese->data;
		}
		else 
#endif
		if ( ese->htype == BM_EDGE ) {
			eed_act = (BMEdge *)ese->ele;
		}
		else if ( ese->htype == BM_VERT ) {
			eve_act = (BMVert *)ese->ele;
		}
	}
	
	EDBM_init_index_arrays(em, 1, 1, 1);

	if (dt>OB_WIRE) {
		if (CHECK_OB_DRAWTEXTURE(v3d, dt)) {
			if (draw_glsl_material(scene, ob, v3d, dt)) {
				glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

				finalDM->drawMappedFacesGLSL(finalDM, GPU_enable_material,
				                             draw_em_fancy__setGLSLFaceOpts, em);
				GPU_disable_material();

				glFrontFace(GL_CCW);
			}
			else {
				draw_mesh_textured(scene, v3d, rv3d, ob, finalDM, 0);
			}
		}
		else {
			/* 3 floats for position,
			 * 3 for normal and times two because the faces may actually be quads instead of triangles */
			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, (me->flag & ME_TWOSIDED) ? GL_TRUE : GL_FALSE);

			glEnable(GL_LIGHTING);
			glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);
			finalDM->drawMappedFaces(finalDM, draw_em_fancy__setFaceOpts, GPU_enable_material, NULL, me->edit_btmesh, 0);

			glFrontFace(GL_CCW);
			glDisable(GL_LIGHTING);
			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
		}

		// Setup for drawing wire over, disable zbuffer
		// write to show selected edge wires better
		UI_ThemeColor(TH_WIRE);

		bglPolygonOffset(rv3d->dist, 1.0);
		glDepthMask(0);
	}
	else {
		if (cageDM!=finalDM) {
			UI_ThemeColorBlend(TH_WIRE, TH_BACK, 0.7);
			finalDM->drawEdges(finalDM, 1, 0);
		}
	}
	
	if (me->drawflag & ME_DRAWFACES) {	/* transp faces */
		unsigned char col1[4], col2[4], col3[4];

		UI_GetThemeColor4ubv(TH_FACE, col1);
		UI_GetThemeColor4ubv(TH_FACE_SELECT, col2);
		UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, col3);
		
		glEnable(GL_BLEND);
		glDepthMask(0);		// disable write in zbuffer, needed for nice transp
		
		/* don't draw unselected faces, only selected, this is MUCH nicer when texturing */
		if (CHECK_OB_DRAWTEXTURE(v3d, dt))
			col1[3] = 0;
		
		draw_dm_faces_sel(em, cageDM, col1, col2, col3, efa_act);

		glDisable(GL_BLEND);
		glDepthMask(1);		// restore write in zbuffer
	}
	else if (efa_act) {
		/* even if draw faces is off it would be nice to draw the stipple face
		 * Make all other faces zero alpha except for the active
		 * */
		unsigned char col1[4], col2[4], col3[4];
		col1[3] = col2[3] = 0; /* don't draw */
		UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, col3);
		
		glEnable(GL_BLEND);
		glDepthMask(0);		// disable write in zbuffer, needed for nice transp
		
		draw_dm_faces_sel(em, cageDM, col1, col2, col3, efa_act);

		glDisable(GL_BLEND);
		glDepthMask(1);		// restore write in zbuffer
		
	}

	/* here starts all fancy draw-extra over */
	if ((me->drawflag & ME_DRAWEDGES)==0 && CHECK_OB_DRAWTEXTURE(v3d, dt)) {
		/* we are drawing textures and 'ME_DRAWEDGES' is disabled, don't draw any edges */
		
		/* only draw selected edges otherwise there is no way of telling if a face is selected */
		draw_em_fancy_edges(em, scene, v3d, me, cageDM, 1, eed_act);
		
	}
	else {
		if (me->drawflag & ME_DRAWSEAMS) {
			UI_ThemeColor(TH_EDGE_SEAM);
			glLineWidth(2);

			draw_dm_edges_seams(em, cageDM);

			glColor3ub(0,0,0);
			glLineWidth(1);
		}
		
		if (me->drawflag & ME_DRAWSHARP) {
			UI_ThemeColor(TH_EDGE_SHARP);
			glLineWidth(2);

			draw_dm_edges_sharp(em, cageDM);

			glColor3ub(0,0,0);
			glLineWidth(1);
		}

		if (me->drawflag & ME_DRAWCREASES && CustomData_has_layer(&em->bm->edata, CD_CREASE)) {
			draw_dm_creases(em, cageDM);
		}
		if (me->drawflag & ME_DRAWBWEIGHTS) {
			draw_dm_bweights(em, scene, cageDM);
		}

		draw_em_fancy_edges(em, scene, v3d, me, cageDM, 0, eed_act);
	}
	if (em) {
		draw_em_fancy_verts(scene, v3d, ob, em, cageDM, eve_act);

		if (me->drawflag & ME_DRAWNORMALS) {
			UI_ThemeColor(TH_NORMAL);
			draw_dm_face_normals(em, scene, cageDM);
		}
		if (me->drawflag & ME_DRAW_VNORMALS) {
			UI_ThemeColor(TH_VNORMAL);
			draw_dm_vert_normals(em, scene, cageDM);
		}

		if ( (me->drawflag & (ME_DRAWEXTRA_EDGELEN|ME_DRAWEXTRA_FACEAREA|ME_DRAWEXTRA_FACEANG)) &&
		     !(v3d->flag2 & V3D_RENDER_OVERRIDE))
		{
			draw_em_measure_stats(v3d, ob, em, &scene->unit);
		}

		if ((G.f & G_DEBUG) && (me->drawflag & ME_DRAWEXTRA_INDICES) &&
		    !(v3d->flag2 & V3D_RENDER_OVERRIDE)) {
			draw_em_indices(em);
		}
	}

	if (dt>OB_WIRE) {
		glDepthMask(1);
		bglPolygonOffset(rv3d->dist, 0.0);
		GPU_disable_material();
	}

	EDBM_free_index_arrays(em);
}

/* Mesh drawing routines */

static void draw_mesh_object_outline(View3D *v3d, Object *ob, DerivedMesh *dm)
{
	
	if ((v3d->transp == FALSE) &&  /* not when we draw the transparent pass */
	    (ob->mode & OB_MODE_ALL_PAINT) == FALSE) /* not when painting (its distracting) - campbell */
	{
		glLineWidth(UI_GetThemeValuef(TH_OUTLINE_WIDTH) * 2.0f);
		glDepthMask(0);
		
		/* if transparent, we cannot draw the edges for solid select... edges have no material info.
		 * drawFacesSolid() doesn't draw the transparent faces */
		if (ob->dtx & OB_DRAWTRANSP) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			dm->drawFacesSolid(dm, NULL, 0, GPU_enable_material);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			GPU_disable_material();
		}
		else {
			dm->drawEdges(dm, 0, 1);
		}

		glLineWidth(1.0);
		glDepthMask(1);
	}
}

static void draw_mesh_fancy(Scene *scene, ARegion *ar, View3D *v3d, RegionView3D *rv3d, Base *base, int dt, int flag)
{
	Object *ob= base->object;
	Mesh *me = ob->data;
	Material *ma= give_current_material(ob, 1);
	const short hasHaloMat = (ma && (ma->material_type == MA_TYPE_HALO));
	eWireDrawMode draw_wire= OBDRAW_WIRE_OFF;
	int /* totvert,*/ totedge, totface;
	DerivedMesh *dm= mesh_get_derived_final(scene, ob, scene->customdata_mask);
	const short is_obact= (ob == OBACT);
	int draw_flags = (is_obact && paint_facesel_test(ob)) ? DRAW_FACE_SELECT : 0;

	if (!dm)
		return;

	/* Check to draw dynamic paint colors (or weights from WeightVG modifiers).
	 * Note: Last "preview-active" modifier in stack will win! */
	if (DM_get_tessface_data_layer(dm, CD_PREVIEW_MCOL) && modifiers_isPreview(ob))
		draw_flags |= DRAW_MODIFIERS_PREVIEW;

	/* Unwanted combination */
	if (draw_flags & DRAW_FACE_SELECT) {
		draw_wire= OBDRAW_WIRE_OFF;
	}
	else if (ob->dtx & OB_DRAWWIRE) {
		draw_wire= OBDRAW_WIRE_ON_DEPTH; /* draw wire after solid using zoffset and depth buffer adjusment */
	}
	
	/* totvert = dm->getNumVerts(dm); */ /*UNUSED*/
	totedge = dm->getNumEdges(dm);
	totface = dm->getNumTessFaces(dm);
	
	/* vertexpaint, faceselect wants this, but it doesnt work for shaded? */
	glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

	if (dt==OB_BOUNDBOX) {
		if (((v3d->flag2 & V3D_RENDER_OVERRIDE) && v3d->drawtype >= OB_WIRE)==0)
			draw_bounding_volume(scene, ob, ob->boundtype);
	}
	else if (hasHaloMat || (totface==0 && totedge==0)) {
		glPointSize(1.5);
		dm->drawVerts(dm);
		glPointSize(1.0);
	}
	else if (dt==OB_WIRE || totface==0) {
		draw_wire= OBDRAW_WIRE_ON; /* draw wire only, no depth buffer stuff  */
	}
	else if ( (draw_flags & DRAW_FACE_SELECT || (is_obact && ob->mode & OB_MODE_TEXTURE_PAINT)) ||
	          CHECK_OB_DRAWTEXTURE(v3d, dt))
	{
		if ( (v3d->flag & V3D_SELECT_OUTLINE) &&
		     ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) &&
		     (base->flag & SELECT) &&
		     !(G.f & G_PICKSEL || (draw_flags & DRAW_FACE_SELECT)) &&
		     (draw_wire == OBDRAW_WIRE_OFF))
		{
			draw_mesh_object_outline(v3d, ob, dm);
		}

		if (draw_glsl_material(scene, ob, v3d, dt) && !(draw_flags & DRAW_MODIFIERS_PREVIEW)) {
			glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

			dm->drawFacesGLSL(dm, GPU_enable_material);
//			if (get_ob_property(ob, "Text"))
// XXX				draw_mesh_text(ob, 1);
			GPU_disable_material();

			glFrontFace(GL_CCW);
		}
		else {
			draw_mesh_textured(scene, v3d, rv3d, ob, dm, draw_flags);
		}

		if (!(draw_flags & DRAW_FACE_SELECT)) {
			if (base->flag & SELECT)
				UI_ThemeColor(is_obact ? TH_ACTIVE : TH_SELECT);
			else
				UI_ThemeColor(TH_WIRE);

			if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0)
				dm->drawLooseEdges(dm);
		}
	}
	else if (dt==OB_SOLID) {
		if (is_obact && ob->mode & OB_MODE_WEIGHT_PAINT) {
			/* weight paint in solid mode, special case. focus on making the weights clear
			 * rather than the shading, this is also forced in wire view */
			GPU_enable_material(0, NULL);
			dm->drawMappedFaces(dm, NULL, GPU_enable_material, NULL, me->mpoly,
			                    DM_DRAW_USE_COLORS | DM_DRAW_ALWAYS_SMOOTH);

			bglPolygonOffset(rv3d->dist, 1.0);
			glDepthMask(0);	// disable write in zbuffer, selected edge wires show better

			glEnable(GL_BLEND);
			glColor4ub(255, 255, 255, 96);
			glEnable(GL_LINE_STIPPLE);
			glLineStipple(1, 0xAAAA);

			dm->drawEdges(dm, 1, 1);

			bglPolygonOffset(rv3d->dist, 0.0);
			glDepthMask(1);
			glDisable(GL_LINE_STIPPLE);
			glDisable(GL_BLEND);

			GPU_disable_material();
			
			/* since we already draw wire as wp guide, don't draw over the top */
			draw_wire= OBDRAW_WIRE_OFF;
		}
		else if (draw_flags & DRAW_MODIFIERS_PREVIEW) {
			/* for object selection draws no shade */
			if (flag & (DRAW_PICKING|DRAW_CONSTCOLOR)) {
				dm->drawFacesSolid(dm, NULL, 0, GPU_enable_material);
			}
			else {
				/* draw outline */
				if ( (v3d->flag & V3D_SELECT_OUTLINE) &&
				     ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) &&
				     (base->flag & SELECT) &&
				     (draw_wire == OBDRAW_WIRE_OFF) &&
				     (ob->sculpt == NULL))
				{
					draw_mesh_object_outline(v3d, ob, dm);
				}

				/* materials arent compatible with vertex colors */
				GPU_end_object_materials();

				GPU_enable_material(0, NULL);
				
				/* set default spec */
				glColorMaterial(GL_FRONT_AND_BACK, GL_SPECULAR);
				glEnable(GL_COLOR_MATERIAL);	/* according manpages needed */
				glColor3ub(120, 120, 120);
				glDisable(GL_COLOR_MATERIAL);
				/* diffuse */
				glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
				glEnable(GL_LIGHTING);
				glEnable(GL_COLOR_MATERIAL);

				dm->drawMappedFaces(dm, NULL, GPU_enable_material, NULL, NULL, DM_DRAW_USE_COLORS);
				glDisable(GL_COLOR_MATERIAL);
				glDisable(GL_LIGHTING);

				GPU_disable_material();
			}
		}
		else {
			Paint *p;

			if ( (v3d->flag & V3D_SELECT_OUTLINE) &&
			     ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) &&
			     (base->flag & SELECT) &&
			     (draw_wire == OBDRAW_WIRE_OFF) &&
			     (ob->sculpt == NULL))
			{
				draw_mesh_object_outline(v3d, ob, dm);
			}

			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, (me->flag & ME_TWOSIDED) ? GL_TRUE : GL_FALSE);

			glEnable(GL_LIGHTING);
			glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

			if (ob->sculpt && (p=paint_get_active(scene))) {
				float planes[4][4];
				float (*fpl)[4] = NULL;
				int fast= (p->flags & PAINT_FAST_NAVIGATE) && (rv3d->rflag & RV3D_NAVIGATING);

				if (ob->sculpt->partial_redraw) {
					if (ar->do_draw & RGN_DRAW_PARTIAL) {
						sculpt_get_redraw_planes(planes, ar, rv3d, ob);
						fpl = planes;
						ob->sculpt->partial_redraw = 0;
					}
				}

				dm->drawFacesSolid(dm, fpl, fast, GPU_enable_material);
			}
			else
				dm->drawFacesSolid(dm, NULL, 0, GPU_enable_material);

			GPU_disable_material();

			glFrontFace(GL_CCW);
			glDisable(GL_LIGHTING);

			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

			if (base->flag & SELECT) {
				UI_ThemeColor(is_obact ? TH_ACTIVE : TH_SELECT);
			}
			else {
				UI_ThemeColor(TH_WIRE);
			}
			if (!ob->sculpt && (v3d->flag2 & V3D_RENDER_OVERRIDE)==0)
				dm->drawLooseEdges(dm);
		}
	}
	else if (dt==OB_PAINT) {
		if (is_obact) {
			if (ob && ob->mode & OB_MODE_WEIGHT_PAINT) {
				/* enforce default material settings */
				GPU_enable_material(0, NULL);
				
				/* but set default spec */
				glColorMaterial(GL_FRONT_AND_BACK, GL_SPECULAR);
				glEnable(GL_COLOR_MATERIAL);	/* according manpages needed */
				glColor3ub(120, 120, 120);
				glDisable(GL_COLOR_MATERIAL);
				/* diffuse */
				glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
				glEnable(GL_LIGHTING);
				glEnable(GL_COLOR_MATERIAL);

				dm->drawMappedFaces(dm, NULL, GPU_enable_material, NULL, me->mpoly,
				                    DM_DRAW_USE_COLORS | DM_DRAW_ALWAYS_SMOOTH);
				glDisable(GL_COLOR_MATERIAL);
				glDisable(GL_LIGHTING);

				GPU_disable_material();
			}
			else if (ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_TEXTURE_PAINT)) {
				if (me->mloopcol)
					dm->drawMappedFaces(dm, NULL, GPU_enable_material, NULL, NULL,
					                    DM_DRAW_USE_COLORS | DM_DRAW_ALWAYS_SMOOTH);
				else {
					glColor3f(1.0f, 1.0f, 1.0f);
					dm->drawMappedFaces(dm, NULL, GPU_enable_material, NULL, NULL,
					                    DM_DRAW_ALWAYS_SMOOTH);
				}
			}
		}
	}
	
	/* set default draw color back for wire or for draw-extra later on */
	if (dt!=OB_WIRE) {
		if (base->flag & SELECT) {
			if (is_obact && ob->flag & OB_FROMGROUP)
				UI_ThemeColor(TH_GROUP_ACTIVE);
			else if (ob->flag & OB_FROMGROUP)
				UI_ThemeColorShade(TH_GROUP_ACTIVE, -16);
			else if (flag!=DRAW_CONSTCOLOR)
				UI_ThemeColor(is_obact ? TH_ACTIVE : TH_SELECT);
			else
				glColor3ub(80,80,80);
		}
		else {
			if (ob->flag & OB_FROMGROUP)
				UI_ThemeColor(TH_GROUP);
			else {
				if (ob->dtx & OB_DRAWWIRE && flag==DRAW_CONSTCOLOR)
					glColor3ub(80,80,80);
				else
					UI_ThemeColor(TH_WIRE);
			}
		}
	}
	if (draw_wire != OBDRAW_WIRE_OFF) {

		/* When using wireframe object draw in particle edit mode
		 * the mesh gets in the way of seeing the particles, fade the wire color
		 * with the background. */
		if (is_obact && (ob->mode & OB_MODE_PARTICLE_EDIT)) {
			float col_wire[4], col_bg[4], col[3];

			UI_GetThemeColor3fv(TH_BACK, col_bg);
			glGetFloatv(GL_CURRENT_COLOR, col_wire);
			interp_v3_v3v3(col, col_bg, col_wire, 0.15);
			glColor3fv(col);
		}

		/* If drawing wire and drawtype is not OB_WIRE then we are
		 * overlaying the wires.
		 *
		 * UPDATE bug #10290 - With this wire-only objects can draw
		 * behind other objects depending on their order in the scene. 2x if 0's below. undo'ing zr's commit: r4059
		 *
		 * if draw wire is 1 then just drawing wire, no need for depth buffer stuff,
		 * otherwise this wire is to overlay solid mode faces so do some depth buffer tricks.
		 */
		if (dt!=OB_WIRE && (draw_wire == OBDRAW_WIRE_ON_DEPTH)) {
			bglPolygonOffset(rv3d->dist, 1.0);
			glDepthMask(0);	// disable write in zbuffer, selected edge wires show better
		}
		
		if (((v3d->flag2 & V3D_RENDER_OVERRIDE) && v3d->drawtype >= OB_SOLID)==0)
			dm->drawEdges(dm, (dt==OB_WIRE || totface==0), me->drawflag & ME_ALLEDGES);

		if (dt!=OB_WIRE && (draw_wire == OBDRAW_WIRE_ON_DEPTH)) {
			glDepthMask(1);
			bglPolygonOffset(rv3d->dist, 0.0);
		}
	}
	
	if (is_obact && paint_vertsel_test(ob)) {
		
		glColor3f(0.0f, 0.0f, 0.0f);
		glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));
		
		drawSelectedVertices(dm, ob->data);
		
		glPointSize(1.0f);
	}
	dm->release(dm);
}

/* returns 1 if nothing was drawn, for detecting to draw an object center */
static int draw_mesh_object(Scene *scene, ARegion *ar, View3D *v3d, RegionView3D *rv3d, Base *base, int dt, int flag)
{
	Object *ob= base->object;
	Object *obedit= scene->obedit;
	Mesh *me= ob->data;
	BMEditMesh *em= me->edit_btmesh;
	int do_alpha_after= 0, drawlinked= 0, retval= 0, glsl, check_alpha, i;

	/* If we are drawing shadows and any of the materials don't cast a shadow,
	 * then don't draw the object */
	if (v3d->flag2 & V3D_RENDER_SHADOW) {
		for (i=0; i<ob->totcol; ++i) {
			Material *ma= give_current_material(ob, i);
			if (ma && !(ma->mode & MA_SHADBUF)) {
				return 1;
			}
		}
	}
	
	if (obedit && ob!=obedit && ob->data==obedit->data) {
		if (ob_get_key(ob) || ob_get_key(obedit)) {}
		else if (ob->modifiers.first || obedit->modifiers.first) {}
		else drawlinked= 1;
	}
	
	if (ob==obedit || drawlinked) {
		DerivedMesh *finalDM, *cageDM;
		
		if (obedit!=ob)
			finalDM = cageDM = editbmesh_get_derived_base(ob, em);
		else
			cageDM = editbmesh_get_derived_cage_and_final(scene, ob, em, &finalDM,
			                                              scene->customdata_mask);

		if (dt>OB_WIRE) {
			glsl = draw_glsl_material(scene, ob, v3d, dt);

			GPU_begin_object_materials(v3d, rv3d, scene, ob, glsl, NULL);
		}

		draw_em_fancy(scene, v3d, rv3d, ob, em, cageDM, finalDM, dt);

		GPU_end_object_materials();

		if (obedit!=ob && finalDM)
			finalDM->release(finalDM);
	}
	else {
		/* don't create boundbox here with mesh_get_bb(), the derived system will make it, puts deformed bb's OK */
		if (me->totpoly <= 4 || ED_view3d_boundbox_clip(rv3d, ob->obmat, (ob->bb)? ob->bb: me->bb)) {
			glsl = draw_glsl_material(scene, ob, v3d, dt);
			check_alpha = check_alpha_pass(base);

			if (dt==OB_SOLID || glsl) {
				GPU_begin_object_materials(v3d, rv3d, scene, ob, glsl,
				                           (check_alpha)? &do_alpha_after: NULL);
			}

			draw_mesh_fancy(scene, ar, v3d, rv3d, base, dt, flag);

			GPU_end_object_materials();
			
			if (me->totvert==0) retval= 1;
		}
	}
	
	/* GPU_begin_object_materials checked if this is needed */
	if (do_alpha_after) {
		if (ob->dtx & OB_DRAWXRAY) {
			add_view3d_after(&v3d->afterdraw_xraytransp, base, flag);
		}
		else {
			add_view3d_after(&v3d->afterdraw_transp, base, flag);
		}
	}
	else if (ob->dtx & OB_DRAWXRAY && ob->dtx & OB_DRAWTRANSP) {
		/* special case xray+transp when alpha is 1.0, without this the object vanishes */
		if (v3d->xray == 0 && v3d->transp == 0) {
			add_view3d_after(&v3d->afterdraw_xray, base, flag);
		}
	}
	
	return retval;
}

/* ************** DRAW DISPLIST ****************** */

static int draw_index_wire= 1;
static int index3_nors_incr= 1;

/* returns 1 when nothing was drawn */
static int drawDispListwire(ListBase *dlbase)
{
	DispList *dl;
	int parts, nr;
	float *data;

	if (dlbase==NULL) return 1;
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (dl= dlbase->first; dl; dl= dl->next) {
		if (dl->parts==0 || dl->nr==0)
			continue;
		
		data= dl->verts;

		switch(dl->type) {
			case DL_SEGM:

				glVertexPointer(3, GL_FLOAT, 0, data);

				for (parts=0; parts<dl->parts; parts++)
					glDrawArrays(GL_LINE_STRIP, parts*dl->nr, dl->nr);
				
				break;
			case DL_POLY:

				glVertexPointer(3, GL_FLOAT, 0, data);

				for (parts=0; parts<dl->parts; parts++)
					glDrawArrays(GL_LINE_LOOP, parts*dl->nr, dl->nr);

				break;
			case DL_SURF:

				glVertexPointer(3, GL_FLOAT, 0, data);

				for (parts=0; parts<dl->parts; parts++) {
					if (dl->flag & DL_CYCL_U)
						glDrawArrays(GL_LINE_LOOP, parts*dl->nr, dl->nr);
					else
						glDrawArrays(GL_LINE_STRIP, parts*dl->nr, dl->nr);
				}

				for (nr=0; nr<dl->nr; nr++) {
					int ofs= 3*dl->nr;

					data= (  dl->verts )+3*nr;
					parts= dl->parts;

					if (dl->flag & DL_CYCL_V) glBegin(GL_LINE_LOOP);
					else glBegin(GL_LINE_STRIP);

					while (parts--) {
						glVertex3fv(data);
						data+=ofs;
					}
					glEnd();

/* (ton) this code crashes for me when resolv is 86 or higher... no clue */
//				glVertexPointer(3, GL_FLOAT, sizeof(float)*3*dl->nr, data + 3*nr);
//				if (dl->flag & DL_CYCL_V)
//					glDrawArrays(GL_LINE_LOOP, 0, dl->parts);
//				else
//					glDrawArrays(GL_LINE_STRIP, 0, dl->parts);
				}
				break;

			case DL_INDEX3:
				if (draw_index_wire) {
					glVertexPointer(3, GL_FLOAT, 0, dl->verts);
					glDrawElements(GL_TRIANGLES, 3*dl->parts, GL_UNSIGNED_INT, dl->index);
				}
				break;

			case DL_INDEX4:
				if (draw_index_wire) {
					glVertexPointer(3, GL_FLOAT, 0, dl->verts);
					glDrawElements(GL_QUADS, 4*dl->parts, GL_UNSIGNED_INT, dl->index);
				}
				break;
		}
	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	return 0;
}

static void drawDispListsolid(ListBase *lb, Object *ob, int glsl)
{
	DispList *dl;
	GPUVertexAttribs gattribs;
	float *data, curcol[4];
	float *ndata;
	
	if (lb==NULL) return;
	
	/* for drawing wire */
	glGetFloatv(GL_CURRENT_COLOR, curcol);

	glEnable(GL_LIGHTING);
	glEnableClientState(GL_VERTEX_ARRAY);
	
	if (ob->transflag & OB_NEG_SCALE) glFrontFace(GL_CW);
	else glFrontFace(GL_CCW);
	
	if (ob->type==OB_MBALL) {	// mball always smooth shaded
		glShadeModel(GL_SMOOTH);
	}
	
	dl= lb->first;
	while (dl) {
		data= dl->verts;
		ndata= dl->nors;

		switch(dl->type) {
			case DL_SEGM:
				if (ob->type==OB_SURF) {
					int nr;

					glDisable(GL_LIGHTING);
					glColor3fv(curcol);

					// glVertexPointer(3, GL_FLOAT, 0, dl->verts);
					// glDrawArrays(GL_LINE_STRIP, 0, dl->nr);

					glBegin(GL_LINE_STRIP);
					for (nr= dl->nr; nr; nr--, data+=3)
						glVertex3fv(data);
					glEnd();

					glEnable(GL_LIGHTING);
				}
				break;
			case DL_POLY:
				if (ob->type==OB_SURF) {
					int nr;

					glDisable(GL_LIGHTING);

					/* for some reason glDrawArrays crashes here in half of the platforms (not osx) */
					//glVertexPointer(3, GL_FLOAT, 0, dl->verts);
					//glDrawArrays(GL_LINE_LOOP, 0, dl->nr);

					glBegin(GL_LINE_LOOP);
					for (nr= dl->nr; nr; nr--, data+=3)
						glVertex3fv(data);
					glEnd();

					glEnable(GL_LIGHTING);
					break;
				}
			case DL_SURF:

				if (dl->index) {
					GPU_enable_material(dl->col+1, (glsl)? &gattribs: NULL);

					if (dl->rt & CU_SMOOTH) glShadeModel(GL_SMOOTH);
					else glShadeModel(GL_FLAT);

					glEnableClientState(GL_NORMAL_ARRAY);
					glVertexPointer(3, GL_FLOAT, 0, dl->verts);
					glNormalPointer(GL_FLOAT, 0, dl->nors);
					glDrawElements(GL_QUADS, 4*dl->totindex, GL_UNSIGNED_INT, dl->index);
					glDisableClientState(GL_NORMAL_ARRAY);
				}
				break;

			case DL_INDEX3:
				GPU_enable_material(dl->col+1, (glsl)? &gattribs: NULL);

				glVertexPointer(3, GL_FLOAT, 0, dl->verts);

				/* voor polys only one normal needed */
				if (index3_nors_incr) {
					glEnableClientState(GL_NORMAL_ARRAY);
					glNormalPointer(GL_FLOAT, 0, dl->nors);
				}
				else
					glNormal3fv(ndata);

				glDrawElements(GL_TRIANGLES, 3*dl->parts, GL_UNSIGNED_INT, dl->index);

				if (index3_nors_incr)
					glDisableClientState(GL_NORMAL_ARRAY);

				break;

			case DL_INDEX4:
				GPU_enable_material(dl->col+1, (glsl)? &gattribs: NULL);

				glEnableClientState(GL_NORMAL_ARRAY);
				glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				glNormalPointer(GL_FLOAT, 0, dl->nors);
				glDrawElements(GL_QUADS, 4*dl->parts, GL_UNSIGNED_INT, dl->index);
				glDisableClientState(GL_NORMAL_ARRAY);

				break;
		}
		dl= dl->next;
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glShadeModel(GL_FLAT);
	glDisable(GL_LIGHTING);
	glFrontFace(GL_CCW);
}

static void drawCurveDMWired(Object *ob)
{
	DerivedMesh *dm = ob->derivedFinal;
	dm->drawEdges (dm, 1, 0);
}

/* return 1 when nothing was drawn */
static int drawCurveDerivedMesh(Scene *scene, View3D *v3d, RegionView3D *rv3d, Base *base, int dt)
{
	Object *ob= base->object;
	DerivedMesh *dm = ob->derivedFinal;

	if (!dm) {
		return 1;
	}

	if (dt>OB_WIRE && dm->getNumTessFaces(dm)) {
		int glsl = draw_glsl_material(scene, ob, v3d, dt);
		GPU_begin_object_materials(v3d, rv3d, scene, ob, glsl, NULL);

		if (!glsl) {
			glEnable(GL_LIGHTING);
			dm->drawFacesSolid(dm, NULL, 0, GPU_enable_material);
			glDisable(GL_LIGHTING);
		}
		else
			dm->drawFacesGLSL(dm, GPU_enable_material);

		GPU_end_object_materials();
	}
	else {
		if (((v3d->flag2 & V3D_RENDER_OVERRIDE) && v3d->drawtype >= OB_SOLID)==0)
			drawCurveDMWired (ob);
	}

	return 0;
}

/* returns 1 when nothing was drawn */
static int drawDispList(Scene *scene, View3D *v3d, RegionView3D *rv3d, Base *base, int dt)
{
	Object *ob= base->object;
	ListBase *lb=NULL;
	DispList *dl;
	Curve *cu;
	const short render_only= (v3d->flag2 & V3D_RENDER_OVERRIDE);
	const short solid= (dt > OB_WIRE);
	int retval= 0;

	if (drawCurveDerivedMesh(scene, v3d, rv3d, base, dt) == 0) {
		return 0;
	}

	switch(ob->type) {
		case OB_FONT:
		case OB_CURVE:
			cu= ob->data;

			lb= &ob->disp;

			if (solid) {
				dl= lb->first;
				if (dl==NULL) return 1;

				if (dl->nors==NULL) addnormalsDispList(lb);
				index3_nors_incr= 0;

				if ( displist_has_faces(lb)==0) {
					if (!render_only) {
						draw_index_wire= 0;
						drawDispListwire(lb);
						draw_index_wire= 1;
					}
				}
				else {
					if (draw_glsl_material(scene, ob, v3d, dt)) {
						GPU_begin_object_materials(v3d, rv3d, scene, ob, 1, NULL);
						drawDispListsolid(lb, ob, 1);
						GPU_end_object_materials();
					}
					else {
						GPU_begin_object_materials(v3d, rv3d, scene, ob, 0, NULL);
						drawDispListsolid(lb, ob, 0);
						GPU_end_object_materials();
					}
					if (cu->editnurb && cu->bevobj==NULL && cu->taperobj==NULL && cu->ext1 == 0.0f && cu->ext2 == 0.0f) {
						cpack(0);
						draw_index_wire= 0;
						drawDispListwire(lb);
						draw_index_wire= 1;
					}
				}
				index3_nors_incr= 1;
			}
			else {
				if (!render_only || (render_only && displist_has_faces(lb))) {
					draw_index_wire= 0;
					retval= drawDispListwire(lb);
					draw_index_wire= 1;
				}
			}
			break;
		case OB_SURF:

			lb= &ob->disp;

			if (solid) {
				dl= lb->first;
				if (dl==NULL) return 1;

				if (dl->nors==NULL) addnormalsDispList(lb);

				if (draw_glsl_material(scene, ob, v3d, dt)) {
					GPU_begin_object_materials(v3d, rv3d, scene, ob, 1, NULL);
					drawDispListsolid(lb, ob, 1);
					GPU_end_object_materials();
				}
				else {
					GPU_begin_object_materials(v3d, rv3d, scene, ob, 0, NULL);
					drawDispListsolid(lb, ob, 0);
					GPU_end_object_materials();
				}
			}
			else {
				retval= drawDispListwire(lb);
			}
			break;
		case OB_MBALL:

			if ( is_basis_mball(ob)) {
				lb= &ob->disp;
				if (lb->first==NULL) makeDispListMBall(scene, ob);
				if (lb->first==NULL) return 1;

				if (solid) {

					if (draw_glsl_material(scene, ob, v3d, dt)) {
						GPU_begin_object_materials(v3d, rv3d, scene, ob, 1, NULL);
						drawDispListsolid(lb, ob, 1);
						GPU_end_object_materials();
					}
					else {
						GPU_begin_object_materials(v3d, rv3d, scene, ob, 0, NULL);
						drawDispListsolid(lb, ob, 0);
						GPU_end_object_materials();
					}
				}
				else {
					/* MetaBalls use DL_INDEX4 type of DispList */
					retval= drawDispListwire(lb);
				}
			}
			break;
	}
	
	return retval;
}

/* *********** drawing for particles ************* */
static void draw_particle_arrays(int draw_as, int totpoint, int ob_dt, int select)
{
	/* draw created data arrays */
	switch(draw_as) {
		case PART_DRAW_AXIS:
		case PART_DRAW_CROSS:
			glDrawArrays(GL_LINES, 0, 6*totpoint);
			break;
		case PART_DRAW_LINE:
			glDrawArrays(GL_LINES, 0, 2*totpoint);
			break;
		case PART_DRAW_BB:
			if (ob_dt<=OB_WIRE || select)
				glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
			else
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			glDrawArrays(GL_QUADS, 0, 4*totpoint);
			break;
		default:
			glDrawArrays(GL_POINTS, 0, totpoint);
			break;
	}
}
static void draw_particle(ParticleKey *state, int draw_as, short draw, float pixsize,
                          float imat[4][4], float *draw_line, ParticleBillboardData *bb, ParticleDrawData *pdd)
{
	float vec[3], vec2[3];
	float *vd = NULL;
	float *cd = NULL;
	float ma_col[3]= {0.0f, 0.0f, 0.0f};

	/* null only for PART_DRAW_CIRC */
	if (pdd) {
		vd = pdd->vd;
		cd = pdd->cd;

		if (pdd->ma_col) {
			copy_v3_v3(ma_col, pdd->ma_col);
		}
	}

	switch(draw_as) {
		case PART_DRAW_DOT:
		{
			if (vd) {
				copy_v3_v3(vd,state->co); pdd->vd+=3;
			}
			if (cd) {
				copy_v3_v3(cd, pdd->ma_col);
				pdd->cd+=3;
			}
			break;
		}
		case PART_DRAW_CROSS:
		case PART_DRAW_AXIS:
		{
			vec[0]=2.0f*pixsize;
			vec[1]=vec[2]=0.0;
			mul_qt_v3(state->rot,vec);
			if (draw_as==PART_DRAW_AXIS) {
				if (cd) {
					cd[1]=cd[2]=cd[4]=cd[5]=0.0;
					cd[0]=cd[3]=1.0;
					cd[6]=cd[8]=cd[9]=cd[11]=0.0;
					cd[7]=cd[10]=1.0;
					cd[13]=cd[12]=cd[15]=cd[16]=0.0;
					cd[14]=cd[17]=1.0;
					pdd->cd+=18;
				}

				copy_v3_v3(vec2,state->co);
			}
			else {
				if (cd) {
					cd[0]=cd[3]=cd[6]=cd[ 9]=cd[12]=cd[15]= ma_col[0];
					cd[1]=cd[4]=cd[7]=cd[10]=cd[13]=cd[16]= ma_col[1];
					cd[2]=cd[5]=cd[8]=cd[11]=cd[14]=cd[17]= ma_col[2];
					pdd->cd+=18;
				}
				sub_v3_v3v3(vec2, state->co, vec);
			}

			add_v3_v3(vec, state->co);
			copy_v3_v3(pdd->vd,vec); pdd->vd+=3;
			copy_v3_v3(pdd->vd,vec2); pdd->vd+=3;

			vec[1]=2.0f*pixsize;
			vec[0]=vec[2]=0.0;
			mul_qt_v3(state->rot,vec);
			if (draw_as==PART_DRAW_AXIS) {
				copy_v3_v3(vec2,state->co);
			}
			else sub_v3_v3v3(vec2, state->co, vec);

			add_v3_v3(vec, state->co);
			copy_v3_v3(pdd->vd,vec); pdd->vd+=3;
			copy_v3_v3(pdd->vd,vec2); pdd->vd+=3;

			vec[2]=2.0f*pixsize;
			vec[0]=vec[1]=0.0;
			mul_qt_v3(state->rot,vec);
			if (draw_as==PART_DRAW_AXIS) {
				copy_v3_v3(vec2,state->co);
			}
			else sub_v3_v3v3(vec2, state->co, vec);

			add_v3_v3(vec, state->co);

			copy_v3_v3(pdd->vd,vec); pdd->vd+=3;
			copy_v3_v3(pdd->vd,vec2); pdd->vd+=3;
			break;
		}
		case PART_DRAW_LINE:
		{
			copy_v3_v3(vec,state->vel);
			normalize_v3(vec);
			if (draw & PART_DRAW_VEL_LENGTH)
				mul_v3_fl(vec,len_v3(state->vel));
			madd_v3_v3v3fl(pdd->vd, state->co, vec, -draw_line[0]); pdd->vd+=3;
			madd_v3_v3v3fl(pdd->vd, state->co, vec,  draw_line[1]); pdd->vd+=3;
			if (cd) {
				cd[0]=cd[3]= ma_col[0];
				cd[1]=cd[4]= ma_col[1];
				cd[2]=cd[5]= ma_col[2];
				pdd->cd+=6;
			}
			break;
		}
		case PART_DRAW_CIRC:
		{
			drawcircball(GL_LINE_LOOP, state->co, pixsize, imat);
			break;
		}
		case PART_DRAW_BB:
		{
			float xvec[3], yvec[3], zvec[3], bb_center[3];
			if (cd) {
				cd[0]=cd[3]=cd[6]=cd[ 9]= ma_col[0];
				cd[1]=cd[4]=cd[7]=cd[10]= ma_col[1];
				cd[2]=cd[5]=cd[8]=cd[11]= ma_col[2];
				pdd->cd+=12;
			}


			copy_v3_v3(bb->vec, state->co);
			copy_v3_v3(bb->vel, state->vel);

			psys_make_billboard(bb, xvec, yvec, zvec, bb_center);
			
			add_v3_v3v3(pdd->vd, bb_center, xvec);
			add_v3_v3(pdd->vd, yvec); pdd->vd+=3;

			sub_v3_v3v3(pdd->vd, bb_center, xvec);
			add_v3_v3(pdd->vd, yvec); pdd->vd+=3;

			sub_v3_v3v3(pdd->vd, bb_center, xvec);
			sub_v3_v3v3(pdd->vd, pdd->vd,yvec); pdd->vd+=3;

			add_v3_v3v3(pdd->vd, bb_center, xvec);
			sub_v3_v3v3(pdd->vd, pdd->vd, yvec); pdd->vd+=3;

			copy_v3_v3(pdd->nd, zvec); pdd->nd+=3;
			copy_v3_v3(pdd->nd, zvec); pdd->nd+=3;
			copy_v3_v3(pdd->nd, zvec); pdd->nd+=3;
			copy_v3_v3(pdd->nd, zvec); pdd->nd+=3;
			break;
		}
	}
}
/* unified drawing of all new particle systems draw types except dupli ob & group	*/
/* mostly tries to use vertex arrays for speed										*/

/* 1. check that everything is ok & updated */
/* 2. start initializing things				*/
/* 3. initialize according to draw type		*/
/* 4. allocate drawing data arrays			*/
/* 5. start filling the arrays				*/
/* 6. draw the arrays						*/
/* 7. clean up								*/
static void draw_new_particle_system(Scene *scene, View3D *v3d, RegionView3D *rv3d,
                                     Base *base, ParticleSystem *psys, int ob_dt)
{
	Object *ob=base->object;
	ParticleEditSettings *pset = PE_settings(scene);
	ParticleSettings *part = psys->part;
	ParticleData *pars = psys->particles;
	ParticleData *pa;
	ParticleKey state, *states=NULL;
	ParticleBillboardData bb;
	ParticleSimulationData sim= {NULL};
	ParticleDrawData *pdd = psys->pdd;
	Material *ma;
	float vel[3], imat[4][4];
	float timestep, pixsize_scale, pa_size, r_tilt, r_length;
	float pa_time, pa_birthtime, pa_dietime, pa_health, intensity;
	float cfra;
	float ma_col[3]= {0.0f, 0.0f, 0.0f};
	int a, totpart, totpoint=0, totve=0, drawn, draw_as, totchild=0;
	int select=ob->flag&SELECT, create_cdata=0, need_v=0;
	GLint polygonmode[2];
	char numstr[32];
	unsigned char tcol[4]= {0, 0, 0, 255};

/* 1. */
	if (part==NULL || !psys_check_enabled(ob, psys))
		return;

	if (pars==NULL) return;

	/* don't draw normal paths in edit mode */
	if (psys_in_edit_mode(scene, psys) && (pset->flag & PE_DRAW_PART)==0)
		return;

	if (part->draw_as == PART_DRAW_REND)
		draw_as = part->ren_as;
	else
		draw_as = part->draw_as;

	if (draw_as == PART_DRAW_NOT)
		return;

/* 2. */
	sim.scene= scene;
	sim.ob= ob;
	sim.psys= psys;
	sim.psmd = psys_get_modifier(ob,psys);

	if (part->phystype==PART_PHYS_KEYED) {
		if (psys->flag&PSYS_KEYED) {
			psys_count_keyed_targets(&sim);
			if (psys->totkeyed==0)
				return;
		}
	}

	if (select) {
		select=0;
		if (psys_get_current(ob)==psys)
			select=1;
	}

	psys->flag|=PSYS_DRAWING;

	if (part->type==PART_HAIR && !psys->childcache)
		totchild=0;
	else
		totchild=psys->totchild*part->disp/100;

	ma= give_current_material(ob,part->omat);

	if (v3d->zbuf) glDepthMask(1);

	if ((ma) && (part->draw_col == PART_DRAW_COL_MAT)) {
		rgb_float_to_uchar(tcol, &(ma->r));
		copy_v3_v3(ma_col, &ma->r);
	}

	glColor3ubv(tcol);

	timestep= psys_get_timestep(&sim);

	if ( (base->flag & OB_FROMDUPLI) && (ob->flag & OB_FROMGROUP) ) {
		float mat[4][4];
		mult_m4_m4m4(mat, ob->obmat, psys->imat);
		glMultMatrixf(mat);
	}

	/* needed for text display */
	invert_m4_m4(ob->imat, ob->obmat);

	totpart=psys->totpart;

	cfra= BKE_curframe(scene);

	if (draw_as==PART_DRAW_PATH && psys->pathcache==NULL && psys->childcache==NULL)
		draw_as=PART_DRAW_DOT;

/* 3. */
	switch(draw_as) {
		case PART_DRAW_DOT:
			if (part->draw_size)
				glPointSize(part->draw_size);
			else
				glPointSize(2.0); /* default dot size */
			break;
		case PART_DRAW_CIRC:
			/* calculate view aligned matrix: */
			copy_m4_m4(imat, rv3d->viewinv);
			normalize_v3(imat[0]);
			normalize_v3(imat[1]);
			/* no break! */
		case PART_DRAW_CROSS:
		case PART_DRAW_AXIS:
			/* lets calculate the scale: */
			
			if (part->draw_size == 0.0)
				pixsize_scale = 2.0f;
			else
				pixsize_scale = part->draw_size;

			if (draw_as==PART_DRAW_AXIS)
				create_cdata = 1;
			break;
		case PART_DRAW_OB:
			if (part->dup_ob==NULL)
				draw_as=PART_DRAW_DOT;
			else
				draw_as=0;
			break;
		case PART_DRAW_GR:
			if (part->dup_group==NULL)
				draw_as=PART_DRAW_DOT;
			else
				draw_as=0;
			break;
		case PART_DRAW_BB:
			if (v3d->camera==NULL && part->bb_ob==NULL) {
				printf("Billboards need an active camera or a target object!\n");

				draw_as=part->draw_as=PART_DRAW_DOT;

				if (part->draw_size)
					glPointSize(part->draw_size);
				else
					glPointSize(2.0); /* default dot size */
			}
			else if (part->bb_ob)
				bb.ob=part->bb_ob;
			else
				bb.ob=v3d->camera;

			bb.align = part->bb_align;
			bb.anim = part->bb_anim;
			bb.lock = part->draw & PART_DRAW_BB_LOCK;
			break;
		case PART_DRAW_PATH:
			break;
		case PART_DRAW_LINE:
			need_v=1;
			break;
	}
	if (part->draw & PART_DRAW_SIZE && part->draw_as!=PART_DRAW_CIRC) {
		copy_m4_m4(imat, rv3d->viewinv);
		normalize_v3(imat[0]);
		normalize_v3(imat[1]);
	}

	if (ELEM3(draw_as, PART_DRAW_DOT, PART_DRAW_CROSS, PART_DRAW_LINE)
	    && part->draw_col > PART_DRAW_COL_MAT)
	{
		create_cdata = 1;
	}

	if (!create_cdata && pdd && pdd->cdata) {
		MEM_freeN(pdd->cdata);
		pdd->cdata = pdd->cd = NULL;
	}

/* 4. */
	if (draw_as && ELEM(draw_as, PART_DRAW_PATH, PART_DRAW_CIRC)==0) {
		int tot_vec_size = (totpart + totchild) * 3 * sizeof(float);
		int create_ndata = 0;

		if (!pdd)
			pdd = psys->pdd = MEM_callocN(sizeof(ParticleDrawData), "ParticlDrawData");

		if (part->draw_as == PART_DRAW_REND && part->trail_count > 1) {
			tot_vec_size *= part->trail_count;
			psys_make_temp_pointcache(ob, psys);
		}

		switch(draw_as) {
			case PART_DRAW_AXIS:
			case PART_DRAW_CROSS:
				tot_vec_size *= 6;
				if (draw_as != PART_DRAW_CROSS)
					create_cdata = 1;
				break;
			case PART_DRAW_LINE:
				tot_vec_size *= 2;
				break;
			case PART_DRAW_BB:
				tot_vec_size *= 4;
				create_ndata = 1;
				break;
		}

		if (pdd->tot_vec_size != tot_vec_size)
			psys_free_pdd(psys);

		if (!pdd->vdata)
			pdd->vdata = MEM_callocN(tot_vec_size, "particle_vdata");
		if (create_cdata && !pdd->cdata)
			pdd->cdata = MEM_callocN(tot_vec_size, "particle_cdata");
		if (create_ndata && !pdd->ndata)
			pdd->ndata = MEM_callocN(tot_vec_size, "particle_ndata");

		if (part->draw & PART_DRAW_VEL && draw_as != PART_DRAW_LINE) {
			if (!pdd->vedata)
				pdd->vedata = MEM_callocN(2 * (totpart + totchild) * 3 * sizeof(float), "particle_vedata");

			need_v = 1;
		}
		else if (pdd->vedata) {
			/* velocity data not needed, so free it */
			MEM_freeN(pdd->vedata);
			pdd->vedata= NULL;
		}

		pdd->vd= pdd->vdata;
		pdd->ved= pdd->vedata;
		pdd->cd= pdd->cdata;
		pdd->nd= pdd->ndata;
		pdd->tot_vec_size= tot_vec_size;
	}
	else if (psys->pdd) {
		psys_free_pdd(psys);
		MEM_freeN(psys->pdd);
		pdd = psys->pdd = NULL;
	}

	if (pdd) {
		pdd->ma_col= ma_col;
	}

	psys->lattice= psys_get_lattice(&sim);

	/* circles don't use drawdata, so have to add a special case here */
	if ((pdd || draw_as==PART_DRAW_CIRC) && draw_as!=PART_DRAW_PATH) {
		/* 5. */
		if (pdd && (pdd->flag & PARTICLE_DRAW_DATA_UPDATED) &&
		    (pdd->vedata || part->draw & (PART_DRAW_SIZE|PART_DRAW_NUM|PART_DRAW_HEALTH))==0)
		{
			totpoint = pdd->totpoint; /* draw data is up to date */
		}
		else {
			for (a=0,pa=pars; a<totpart+totchild; a++, pa++) {
				/* setup per particle individual stuff */
				if (a<totpart) {
					if (totchild && (part->draw&PART_DRAW_PARENT)==0) continue;
					if (pa->flag & PARS_NO_DISP || pa->flag & PARS_UNEXIST) continue;

					pa_time=(cfra-pa->time)/pa->lifetime;
					pa_birthtime=pa->time;
					pa_dietime = pa->dietime;
					pa_size=pa->size;
					if (part->phystype==PART_PHYS_BOIDS)
						pa_health = pa->boid->data.health;
					else
						pa_health = -1.0;

					r_tilt = 2.0f*(PSYS_FRAND(a + 21) - 0.5f);
					r_length = PSYS_FRAND(a + 22);

					if (part->draw_col > PART_DRAW_COL_MAT) {
						switch(part->draw_col) {
							case PART_DRAW_COL_VEL:
								intensity = len_v3(pa->state.vel)/part->color_vec_max;
								break;
							case PART_DRAW_COL_ACC:
								intensity = len_v3v3(pa->state.vel, pa->prev_state.vel) / ((pa->state.time - pa->prev_state.time) * part->color_vec_max);
								break;
							default:
								intensity= 1.0f; /* should never happen */
						}
						CLAMP(intensity, 0.f, 1.f);
						weight_to_rgb(ma_col, intensity);
					}
				}
				else {
					ChildParticle *cpa= &psys->child[a-totpart];

					pa_time=psys_get_child_time(psys,cpa,cfra,&pa_birthtime,&pa_dietime);
					pa_size=psys_get_child_size(psys,cpa,cfra,NULL);

					pa_health = -1.0;

					r_tilt = 2.0f*(PSYS_FRAND(a + 21) - 0.5f);
					r_length = PSYS_FRAND(a + 22);
				}

				drawn = 0;
				if (part->draw_as == PART_DRAW_REND && part->trail_count > 1) {
					float length = part->path_end * (1.0f - part->randlength * r_length);
					int trail_count = part->trail_count * (1.0f - part->randlength * r_length);
					float ct = ((part->draw & PART_ABS_PATH_TIME) ? cfra : pa_time) - length;
					float dt = length / (trail_count ? (float)trail_count : 1.0f);
					int i=0;

					ct+=dt;
					for (i=0; i < trail_count; i++, ct += dt) {
						float pixsize;

						if (part->draw & PART_ABS_PATH_TIME) {
							if (ct < pa_birthtime || ct > pa_dietime)
								continue;
						}
						else if (ct < 0.0f || ct > 1.0f)
							continue;

						state.time = (part->draw & PART_ABS_PATH_TIME) ? -ct : -(pa_birthtime + ct * (pa_dietime - pa_birthtime));
						psys_get_particle_on_path(&sim,a,&state,need_v);

						if (psys->parent)
							mul_m4_v3(psys->parent->obmat, state.co);

						/* create actiual particle data */
						if (draw_as == PART_DRAW_BB) {
							bb.offset[0] = part->bb_offset[0];
							bb.offset[1] = part->bb_offset[1];
							bb.size[0] = part->bb_size[0] * pa_size;
							if (part->bb_align==PART_BB_VEL) {
								float pa_vel = len_v3(state.vel);
								float head = part->bb_vel_head*pa_vel;
								float tail = part->bb_vel_tail*pa_vel;
								bb.size[1] = part->bb_size[1]*pa_size + head + tail;
								/* use offset to adjust the particle center. this is relative to size, so need to divide! */
								if (bb.size[1] > 0.0f)
									bb.offset[1] += (head-tail) / bb.size[1];
							}
							else
								bb.size[1] = part->bb_size[1] * pa_size;
							bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
							bb.time = ct;
						}

						pixsize = ED_view3d_pixel_size(rv3d, state.co) * pixsize_scale;

						draw_particle(&state, draw_as, part->draw, pixsize, imat, part->draw_line, &bb, psys->pdd);

						totpoint++;
						drawn = 1;
					}
				}
				else {
					state.time=cfra;
					if (psys_get_particle_state(&sim,a,&state,0)) {
						float pixsize;

						if (psys->parent)
							mul_m4_v3(psys->parent->obmat, state.co);

						/* create actiual particle data */
						if (draw_as == PART_DRAW_BB) {
							bb.offset[0] = part->bb_offset[0];
							bb.offset[1] = part->bb_offset[1];
							bb.size[0] = part->bb_size[0] * pa_size;
							if (part->bb_align==PART_BB_VEL) {
								float pa_vel = len_v3(state.vel);
								float head = part->bb_vel_head*pa_vel;
								float tail = part->bb_vel_tail*pa_vel;
								bb.size[1] = part->bb_size[1]*pa_size + head + tail;
								/* use offset to adjust the particle center. this is relative to size, so need to divide! */
								if (bb.size[1] > 0.0f)
									bb.offset[1] += (head-tail) / bb.size[1];
							}
							else
								bb.size[1] = part->bb_size[1] * pa_size;
							bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
							bb.time = pa_time;
						}

						pixsize = ED_view3d_pixel_size(rv3d, state.co) * pixsize_scale;

						draw_particle(&state, draw_as, part->draw, pixsize, imat, part->draw_line, &bb, pdd);

						totpoint++;
						drawn = 1;
					}
				}

				if (drawn) {
					/* additional things to draw for each particle	*/
					/* (velocity, size and number)					*/
					if ((part->draw & PART_DRAW_VEL) && pdd && pdd->vedata) {
						copy_v3_v3(pdd->ved,state.co);
						pdd->ved += 3;
						mul_v3_v3fl(vel, state.vel, timestep);
						add_v3_v3v3(pdd->ved, state.co, vel);
						pdd->ved+=3;

						totve++;
					}

					if (part->draw & PART_DRAW_SIZE) {
						setlinestyle(3);
						drawcircball(GL_LINE_LOOP, state.co, pa_size, imat);
						setlinestyle(0);
					}


					if ((part->draw & PART_DRAW_NUM || part->draw & PART_DRAW_HEALTH) &&
					    (v3d->flag2 & V3D_RENDER_OVERRIDE) == 0)
					{
						float vec_txt[3];
						char *val_pos= numstr;
						numstr[0]= '\0';

						if (part->draw&PART_DRAW_NUM) {
							if (a < totpart && (part->draw & PART_DRAW_HEALTH) && (part->phystype==PART_PHYS_BOIDS)) {
								sprintf(val_pos, "%d:%.2f", a, pa_health);
							}
							else {
								sprintf(val_pos, "%d", a);
							}
						}
						else {
							if (a < totpart && (part->draw & PART_DRAW_HEALTH) && (part->phystype==PART_PHYS_BOIDS)) {
								sprintf(val_pos, "%.2f", pa_health);
							}
						}

						/* in path drawing state.co is the end point */
						/* use worldspace beause object matrix is already applied */
						mul_v3_m4v3(vec_txt, ob->imat, state.co);
						view3d_cached_text_draw_add(vec_txt, numstr, 10, V3D_CACHE_TEXT_WORLDSPACE|V3D_CACHE_TEXT_ASCII, tcol);
					}
				}
			}
		}
	}
/* 6. */

	glGetIntegerv(GL_POLYGON_MODE, polygonmode);
	glEnableClientState(GL_VERTEX_ARRAY);

	if (draw_as==PART_DRAW_PATH) {
		ParticleCacheKey **cache, *path;
		float /* *cd2=NULL, */ /* UNUSED */ *cdata2=NULL;

		/* setup gl flags */
		if (1) { //ob_dt > OB_WIRE) {
			glEnableClientState(GL_NORMAL_ARRAY);

			if (part->draw_col == PART_DRAW_COL_MAT)
				glEnableClientState(GL_COLOR_ARRAY);

			glEnable(GL_LIGHTING);
			glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
			glEnable(GL_COLOR_MATERIAL);
		}
#if 0
		else {
			glDisableClientState(GL_NORMAL_ARRAY);

			glDisable(GL_COLOR_MATERIAL);
			glDisable(GL_LIGHTING);
			UI_ThemeColor(TH_WIRE);
		}
#endif

		if (totchild && (part->draw&PART_DRAW_PARENT)==0)
			totpart=0;
		else if (psys->pathcache==NULL)
			totpart=0;

		/* draw actual/parent particles */
		cache=psys->pathcache;
		for (a=0, pa=psys->particles; a<totpart; a++, pa++) {
			path=cache[a];
			if (path->steps > 0) {
				glVertexPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->co);

				if (1) { //ob_dt > OB_WIRE) {
					glNormalPointer(GL_FLOAT, sizeof(ParticleCacheKey), path->vel);
					if (part->draw_col == PART_DRAW_COL_MAT)
						glColorPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->col);
				}

				glDrawArrays(GL_LINE_STRIP, 0, path->steps + 1);
			}
		}
		
		/* draw child particles */
		cache=psys->childcache;
		for (a=0; a<totchild; a++) {
			path=cache[a];
			glVertexPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->co);

			if (1) { //ob_dt > OB_WIRE) {
				glNormalPointer(GL_FLOAT, sizeof(ParticleCacheKey), path->vel);
				if (part->draw_col == PART_DRAW_COL_MAT)
					glColorPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->col);
			}

			glDrawArrays(GL_LINE_STRIP, 0, path->steps + 1);
		}


		/* restore & clean up */
		if (1) { //ob_dt > OB_WIRE) {
			if (part->draw_col == PART_DRAW_COL_MAT)
				glDisable(GL_COLOR_ARRAY);
			glDisable(GL_COLOR_MATERIAL);
		}

		if (cdata2)
			MEM_freeN(cdata2);
		/* cd2= */ /* UNUSED */ cdata2=NULL;

		glLineWidth(1.0f);

		if ((part->draw & PART_DRAW_NUM) && (v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
			cache=psys->pathcache;

			for (a=0, pa=psys->particles; a<totpart; a++, pa++) {
				float vec_txt[3];
				BLI_snprintf(numstr, sizeof(numstr), "%i", a);
				/* use worldspace beause object matrix is already applied */
				mul_v3_m4v3(vec_txt, ob->imat, cache[a]->co);
				view3d_cached_text_draw_add(vec_txt, numstr, 10, V3D_CACHE_TEXT_WORLDSPACE|V3D_CACHE_TEXT_ASCII, tcol);
			}
		}
	}
	else if (pdd && ELEM(draw_as, 0, PART_DRAW_CIRC)==0) {
		glDisableClientState(GL_COLOR_ARRAY);

		/* enable point data array */
		if (pdd->vdata) {
			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(3, GL_FLOAT, 0, pdd->vdata);
		}
		else
			glDisableClientState(GL_VERTEX_ARRAY);

		if (select) {
			UI_ThemeColor(TH_ACTIVE);
			
			if (part->draw_size)
				glPointSize(part->draw_size + 2);
			else
				glPointSize(4.0);

			glLineWidth(3.0);

			draw_particle_arrays(draw_as, totpoint, ob_dt, 1);
		}

		/* restore from select */
		glColor3fv(ma_col);
		glPointSize(part->draw_size ? part->draw_size : 2.0);
		glLineWidth(1.0);

		/* enable other data arrays */

		/* billboards are drawn this way */
		if (pdd->ndata && ob_dt>OB_WIRE) {
			glEnableClientState(GL_NORMAL_ARRAY);
			glNormalPointer(GL_FLOAT, 0, pdd->ndata);
			glEnable(GL_LIGHTING);
		}
		else {
			glDisableClientState(GL_NORMAL_ARRAY);
			glDisable(GL_LIGHTING);
		}

		if (pdd->cdata) {
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(3, GL_FLOAT, 0, pdd->cdata);
		}

		draw_particle_arrays(draw_as, totpoint, ob_dt, 0);

		pdd->flag |= PARTICLE_DRAW_DATA_UPDATED;
		pdd->totpoint = totpoint;
	}

	if (pdd && pdd->vedata) {
		glDisableClientState(GL_COLOR_ARRAY);
		cpack(0xC0C0C0);
		
		glVertexPointer(3, GL_FLOAT, 0, pdd->vedata);
		
		glDrawArrays(GL_LINES, 0, 2*totve);
	}

	glPolygonMode(GL_FRONT, polygonmode[0]);
	glPolygonMode(GL_BACK, polygonmode[1]);

/* 7. */
	
	glDisable(GL_LIGHTING);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	if (states)
		MEM_freeN(states);

	psys->flag &= ~PSYS_DRAWING;

	/* draw data can't be saved for billboards as they must update to target changes */
	if (draw_as == PART_DRAW_BB) {
		psys_free_pdd(psys);
		pdd->flag &= ~PARTICLE_DRAW_DATA_UPDATED;
	}

	if (psys->lattice) {
		end_latt_deform(psys->lattice);
		psys->lattice= NULL;
	}

	if (pdd) {
		/* drop references to stack memory */
		pdd->ma_col= NULL;
	}

	if ( (base->flag & OB_FROMDUPLI) && (ob->flag & OB_FROMGROUP) ) {
		glLoadMatrixf(rv3d->viewmat);
	}
}

static void draw_update_ptcache_edit(Scene *scene, Object *ob, PTCacheEdit *edit)
{
	if (edit->psys && edit->psys->flag & PSYS_HAIR_UPDATED)
		PE_update_object(scene, ob, 0);

	/* create path and child path cache if it doesn't exist already */
	if (edit->pathcache == NULL)
		psys_cache_edit_paths(scene, ob, edit, CFRA);
}

static void draw_ptcache_edit(Scene *scene, View3D *v3d, PTCacheEdit *edit)
{
	ParticleCacheKey **cache, *path, *pkey;
	PTCacheEditPoint *point;
	PTCacheEditKey *key;
	ParticleEditSettings *pset = PE_settings(scene);
	int i, k, totpoint = edit->totpoint, timed = pset->flag & PE_FADE_TIME ? pset->fade_frames : 0;
	int steps=1;
	float sel_col[3];
	float nosel_col[3];
	float *pathcol = NULL, *pcol;

	if (edit->pathcache == NULL)
		return;

	PE_hide_keys_time(scene, edit, CFRA);

	/* opengl setup */
	if ((v3d->flag & V3D_ZBUF_SELECT)==0)
		glDisable(GL_DEPTH_TEST);

	/* get selection theme colors */
	UI_GetThemeColor3fv(TH_VERTEX_SELECT, sel_col);
	UI_GetThemeColor3fv(TH_VERTEX, nosel_col);

	/* draw paths */
	if (timed) {
		glEnable(GL_BLEND);
		steps = (*edit->pathcache)->steps + 1;
		pathcol = MEM_callocN(steps*4*sizeof(float), "particle path color data");
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);

	if (pset->brushtype == PE_BRUSH_WEIGHT) {
		glLineWidth(2.0f);
		glDisable(GL_LIGHTING);
	}

	cache=edit->pathcache;
	for (i=0; i<totpoint; i++) {
		path = cache[i];
		glVertexPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->co);

		if (timed) {
			for (k=0, pcol=pathcol, pkey=path; k<steps; k++, pkey++, pcol+=4) {
				copy_v3_v3(pcol, pkey->col);
				pcol[3] = 1.0f - fabsf((float)(CFRA) - pkey->time)/(float)pset->fade_frames;
			}

			glColorPointer(4, GL_FLOAT, 4*sizeof(float), pathcol);
		}
		else
			glColorPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->col);

		glDrawArrays(GL_LINE_STRIP, 0, path->steps + 1);
	}

	if (pathcol) { MEM_freeN(pathcol); pathcol = pcol = NULL; }


	/* draw edit vertices */
	if (pset->selectmode!=SCE_SELECT_PATH) {
		glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));

		if (pset->selectmode==SCE_SELECT_POINT) {
			float *pd=NULL,*pdata=NULL;
			float *cd=NULL,*cdata=NULL;
			int totkeys = 0;

			for (i=0, point=edit->points; i<totpoint; i++, point++)
				if (!(point->flag & PEP_HIDE))
					totkeys += point->totkey;

			if (edit->points && !(edit->points->keys->flag & PEK_USE_WCO))
				pd=pdata=MEM_callocN(totkeys*3*sizeof(float), "particle edit point data");
			cd=cdata=MEM_callocN(totkeys*(timed?4:3)*sizeof(float), "particle edit color data");

			for (i=0, point=edit->points; i<totpoint; i++, point++) {
				if (point->flag & PEP_HIDE)
					continue;

				for (k=0, key=point->keys; k<point->totkey; k++, key++) {
					if (pd) {
						copy_v3_v3(pd, key->co);
						pd += 3;
					}

					if (key->flag&PEK_SELECT) {
						copy_v3_v3(cd,sel_col);
					}
					else {
						copy_v3_v3(cd,nosel_col);
					}

					if (timed)
						*(cd+3) = 1.0f - fabsf((float)CFRA - *key->time)/(float)pset->fade_frames;

					cd += (timed?4:3);
				}
			}
			cd=cdata;
			pd=pdata;
			for (i=0, point=edit->points; i<totpoint; i++, point++) {
				if (point->flag & PEP_HIDE || point->totkey == 0)
					continue;

				if (point->keys->flag & PEK_USE_WCO)
					glVertexPointer(3, GL_FLOAT, sizeof(PTCacheEditKey), point->keys->world_co);
				else
					glVertexPointer(3, GL_FLOAT, 3*sizeof(float), pd);

				glColorPointer((timed?4:3), GL_FLOAT, (timed?4:3)*sizeof(float), cd);

				glDrawArrays(GL_POINTS, 0, point->totkey);

				pd += pd ? 3 * point->totkey : 0;
				cd += (timed?4:3) * point->totkey;
			}
			if (pdata) { MEM_freeN(pdata); pd=pdata=NULL; }
			if (cdata) { MEM_freeN(cdata); cd=cdata=NULL; }
		}
		else if (pset->selectmode == SCE_SELECT_END) {
			for (i=0, point=edit->points; i<totpoint; i++, point++) {
				if ((point->flag & PEP_HIDE)==0 && point->totkey) {
					key = point->keys + point->totkey - 1;
					if (key->flag & PEK_SELECT)
						glColor3fv(sel_col);
					else
						glColor3fv(nosel_col);
					/* has to be like this.. otherwise selection won't work, have try glArrayElement later..*/
					glBegin(GL_POINTS);
					glVertex3fv(key->flag & PEK_USE_WCO ? key->world_co : key->co);
					glEnd();
				}
			}
		}
	}

	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glShadeModel(GL_FLAT);
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	glLineWidth(1.0f);
	glPointSize(1.0);
}
//static void ob_draw_RE_motion(float com[3],float rotscale[3][3],float tw,float th)
static void ob_draw_RE_motion(float com[3],float rotscale[3][3],float itw,float ith,float drw_size)
{
	float tr[3][3];
	float root[3],tip[3];
	float tw,th;
	/* take a copy for not spoiling original */
	copy_m3_m3(tr,rotscale);
	tw = itw * drw_size;
	th = ith * drw_size;

	glColor4ub(0x7F, 0x00, 0x00, 155);
	glBegin(GL_LINES);
	root[1] = root[2] = 0.0f;
	root[0] = -drw_size;
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	tip[1] = tip[2] = 0.0f;
	tip[0] = drw_size;
	mul_m3_v3(tr,tip);
	add_v3_v3(tip, com);
	glVertex3fv(tip);
	glEnd();

	root[1] =0.0f; root[2] = tw;
	root[0] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	root[1] =0.0f; root[2] = -tw;
	root[0] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	root[1] = tw; root[2] = 0.0f;
	root[0] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	root[1] =-tw; root[2] = 0.0f;
	root[0] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	glColor4ub(0x00, 0x7F, 0x00, 155);

	glBegin(GL_LINES);
	root[0] = root[2] = 0.0f;
	root[1] = -drw_size;
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	tip[0] = tip[2] = 0.0f;
	tip[1] = drw_size;
	mul_m3_v3(tr,tip);
	add_v3_v3(tip, com);
	glVertex3fv(tip);
	glEnd();

	root[0] =0.0f; root[2] = tw;
	root[1] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	root[0] = 0.0f; root[2] = -tw;
	root[1] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	root[0] = tw; root[2] = 0.0f;
	root[1] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	root[0] =-tw; root[2] = 0.0f;
	root[1] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	glColor4ub(0x00, 0x00, 0x7F, 155);
	glBegin(GL_LINES);
	root[0] = root[1] = 0.0f;
	root[2] = -drw_size;
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	tip[0] = tip[1] = 0.0f;
	tip[2] = drw_size;
	mul_m3_v3(tr,tip);
	add_v3_v3(tip, com);
	glVertex3fv(tip);
	glEnd();

	root[0] =0.0f; root[1] = tw;
	root[2] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	root[0] =0.0f; root[1] = -tw;
	root[2] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	root[0] = tw; root[1] = 0.0f;
	root[2] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();

	root[0] = -tw; root[1] = 0.0f;
	root[2] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	add_v3_v3(root, com);
	glVertex3fv(root);
	glVertex3fv(tip);
	glEnd();
}

/*place to add drawers */

static void tekenhandlesN(Nurb *nu, short sel, short hide_handles)
{
	BezTriple *bezt;
	float *fp;
	int a;

	if (nu->hide || hide_handles) return;

	glBegin(GL_LINES);

	if (nu->type == CU_BEZIER) {

#define TH_HANDLE_COL_TOT ((TH_HANDLE_SEL_FREE - TH_HANDLE_FREE) + 1)
		/* use MIN2 when indexing to ensure newer files don't read outside the array */
		unsigned char handle_cols[TH_HANDLE_COL_TOT][3];
		const int basecol= sel ? TH_HANDLE_SEL_FREE : TH_HANDLE_FREE;

		for (a=0; a < TH_HANDLE_COL_TOT; a++) {
			UI_GetThemeColor3ubv(basecol + a, handle_cols[a]);
		}

		bezt= nu->bezt;
		a= nu->pntsu;
		while (a--) {
			if (bezt->hide==0) {
				if ( (bezt->f2 & SELECT)==sel) {
					fp= bezt->vec[0];

					glColor3ubv(handle_cols[MIN2(bezt->h1, TH_HANDLE_COL_TOT-1)]);
					glVertex3fv(fp);
					glVertex3fv(fp+3);

					glColor3ubv(handle_cols[MIN2(bezt->h2, TH_HANDLE_COL_TOT-1)]);
					glVertex3fv(fp+3);
					glVertex3fv(fp+6);
				}
				else if ( (bezt->f1 & SELECT)==sel) {
					fp= bezt->vec[0];

					glColor3ubv(handle_cols[MIN2(bezt->h1, TH_HANDLE_COL_TOT-1)]);
					glVertex3fv(fp);
					glVertex3fv(fp+3);
				}
				else if ( (bezt->f3 & SELECT)==sel) {
					fp= bezt->vec[1];

					glColor3ubv(handle_cols[MIN2(bezt->h2, TH_HANDLE_COL_TOT-1)]);
					glVertex3fv(fp);
					glVertex3fv(fp+3);
				}
			}
			bezt++;
		}

#undef TH_HANDLE_COL_TOT

	}
	glEnd();
}

static void tekenhandlesN_active(Nurb *nu)
{
	BezTriple *bezt;
	float *fp;
	int a;

	if (nu->hide) return;

	UI_ThemeColor(TH_ACTIVE_SPLINE);
	glLineWidth(2);

	glBegin(GL_LINES);

	if (nu->type == CU_BEZIER) {
		bezt= nu->bezt;
		a= nu->pntsu;
		while (a--) {
			if (bezt->hide==0) {
				fp= bezt->vec[0];

				glVertex3fv(fp);
				glVertex3fv(fp+3);

				glVertex3fv(fp+3);
				glVertex3fv(fp+6);
			}
			bezt++;
		}
	}
	glEnd();

	glColor3ub(0,0,0);
	glLineWidth(1);
}

static void tekenvertsN(Nurb *nu, short sel, short hide_handles, void *lastsel)
{
	BezTriple *bezt;
	BPoint *bp;
	float size;
	int a, color;

	if (nu->hide) return;

	if (sel) color= TH_VERTEX_SELECT;
	else color= TH_VERTEX;

	UI_ThemeColor(color);

	size= UI_GetThemeValuef(TH_VERTEX_SIZE);
	glPointSize(size);
	
	bglBegin(GL_POINTS);
	
	if (nu->type == CU_BEZIER) {

		bezt= nu->bezt;
		a= nu->pntsu;
		while (a--) {
			if (bezt->hide==0) {
				if (sel == 1 && bezt == lastsel) {
					UI_ThemeColor(TH_LASTSEL_POINT);
					bglVertex3fv(bezt->vec[1]);

					if (!hide_handles) {
						if (bezt->f1 & SELECT) bglVertex3fv(bezt->vec[0]);
						if (bezt->f3 & SELECT) bglVertex3fv(bezt->vec[2]);
					}

					UI_ThemeColor(color);
				}
				else if (hide_handles) {
					if ((bezt->f2 & SELECT)==sel) bglVertex3fv(bezt->vec[1]);
				}
				else {
					if ((bezt->f1 & SELECT)==sel) bglVertex3fv(bezt->vec[0]);
					if ((bezt->f2 & SELECT)==sel) bglVertex3fv(bezt->vec[1]);
					if ((bezt->f3 & SELECT)==sel) bglVertex3fv(bezt->vec[2]);
				}
			}
			bezt++;
		}
	}
	else {
		bp= nu->bp;
		a= nu->pntsu*nu->pntsv;
		while (a--) {
			if (bp->hide==0) {
				if (bp == lastsel) {
					UI_ThemeColor(TH_LASTSEL_POINT);
					bglVertex3fv(bp->vec);
					UI_ThemeColor(color);
				}
				else {
					if ((bp->f1 & SELECT)==sel) bglVertex3fv(bp->vec);
				}
			}
			bp++;
		}
	}
	
	bglEnd();
	glPointSize(1.0);
}

static void editnurb_draw_active_poly(Nurb *nu)
{
	BPoint *bp;
	int a, b;

	UI_ThemeColor(TH_ACTIVE_SPLINE);
	glLineWidth(2);

	bp= nu->bp;
	for (b=0; b<nu->pntsv; b++) {
		if (nu->flagu & 1) glBegin(GL_LINE_LOOP);
		else glBegin(GL_LINE_STRIP);

		for (a=0; a<nu->pntsu; a++, bp++) {
			glVertex3fv(bp->vec);
		}

		glEnd();
	}

	glColor3ub(0,0,0);
	glLineWidth(1);
}

static void editnurb_draw_active_nurbs(Nurb *nu)
{
	BPoint *bp, *bp1;
	int a, b, ofs;

	UI_ThemeColor(TH_ACTIVE_SPLINE);
	glLineWidth(2);

	glBegin(GL_LINES);
	bp= nu->bp;
	for (b=0; b<nu->pntsv; b++) {
		bp1= bp;
		bp++;

		for (a=nu->pntsu-1; a>0; a--, bp++) {
			if (bp->hide==0 && bp1->hide==0) {
				glVertex3fv(bp->vec);
				glVertex3fv(bp1->vec);
			}
			bp1= bp;
		}
	}

	if (nu->pntsv > 1) {	/* surface */

		ofs= nu->pntsu;
		for (b=0; b<nu->pntsu; b++) {
			bp1= nu->bp+b;
			bp= bp1+ofs;
			for (a=nu->pntsv-1; a>0; a--, bp+=ofs) {
				if (bp->hide==0 && bp1->hide==0) {
					glVertex3fv(bp->vec);
					glVertex3fv(bp1->vec);
				}
				bp1= bp;
			}
		}
	}

	glEnd();

	glColor3ub(0,0,0);
	glLineWidth(1);
}

static void draw_editnurb(Object *ob, Nurb *nurb, int sel)
{
	Nurb *nu;
	BPoint *bp, *bp1;
	int a, b, ofs, index;
	Curve *cu= (Curve*)ob->data;

	index= 0;
	nu= nurb;
	while (nu) {
		if (nu->hide==0) {
			switch(nu->type) {
				case CU_POLY:
					if (!sel && index== cu->actnu) {
						/* we should draw active spline highlight below everything */
						editnurb_draw_active_poly(nu);
					}

					UI_ThemeColor(TH_NURB_ULINE);
					bp= nu->bp;
					for (b=0; b<nu->pntsv; b++) {
						if (nu->flagu & 1) glBegin(GL_LINE_LOOP);
						else glBegin(GL_LINE_STRIP);

						for (a=0; a<nu->pntsu; a++, bp++) {
							glVertex3fv(bp->vec);
						}

						glEnd();
					}
					break;
				case CU_NURBS:
					if (!sel && index== cu->actnu) {
						/* we should draw active spline highlight below everything */
						editnurb_draw_active_nurbs(nu);
					}

					bp= nu->bp;
					for (b=0; b<nu->pntsv; b++) {
						bp1= bp;
						bp++;
						for (a=nu->pntsu-1; a>0; a--, bp++) {
							if (bp->hide==0 && bp1->hide==0) {
								if (sel) {
									if ( (bp->f1 & SELECT) && ( bp1->f1 & SELECT ) ) {
										UI_ThemeColor(TH_NURB_SEL_ULINE);

										glBegin(GL_LINE_STRIP);
										glVertex3fv(bp->vec);
										glVertex3fv(bp1->vec);
										glEnd();
									}
								}
								else {
									if ((bp->f1 & SELECT) && (bp1->f1 & SELECT)) {
										/* pass */
									}
									else {
										UI_ThemeColor(TH_NURB_ULINE);

										glBegin(GL_LINE_STRIP);
										glVertex3fv(bp->vec);
										glVertex3fv(bp1->vec);
										glEnd();
									}
								}
							}
							bp1= bp;
						}
					}
					if (nu->pntsv > 1) {	/* surface */

						ofs= nu->pntsu;
						for (b=0; b<nu->pntsu; b++) {
							bp1= nu->bp+b;
							bp= bp1+ofs;
							for (a=nu->pntsv-1; a>0; a--, bp+=ofs) {
								if (bp->hide==0 && bp1->hide==0) {
									if (sel) {
										if ( (bp->f1 & SELECT) && ( bp1->f1 & SELECT) ) {
											UI_ThemeColor(TH_NURB_SEL_VLINE);

											glBegin(GL_LINE_STRIP);
											glVertex3fv(bp->vec);
											glVertex3fv(bp1->vec);
											glEnd();
										}
									}
									else {
										if ((bp->f1 & SELECT) && (bp1->f1 & SELECT)) {
											/* pass */
										}
										else {
											UI_ThemeColor(TH_NURB_VLINE);

											glBegin(GL_LINE_STRIP);
											glVertex3fv(bp->vec);
											glVertex3fv(bp1->vec);
											glEnd();
										}
									}
								}
								bp1= bp;
							}
						}

					}
					break;
			}
		}

		++index;
		nu= nu->next;
	}
}

static void drawnurb(Scene *scene, View3D *v3d, RegionView3D *rv3d, Base *base, Nurb *nurb, int dt)
{
	ToolSettings *ts= scene->toolsettings;
	Object *ob= base->object;
	Curve *cu = ob->data;
	Nurb *nu;
	BevList *bl;
	short hide_handles = (cu->drawflag & CU_HIDE_HANDLES);
	int index;

	/* DispList */
	UI_ThemeColor(TH_WIRE);
	drawDispList(scene, v3d, rv3d, base, dt);

	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	/* first non-selected and active handles */
	index= 0;
	for (nu=nurb; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER) {
			if (index == cu->actnu && !hide_handles)
				tekenhandlesN_active(nu);
			tekenhandlesN(nu, 0, hide_handles);
		}
		++index;
	}
	draw_editnurb(ob, nurb, 0);
	draw_editnurb(ob, nurb, 1);
	/* selected handles */
	for (nu=nurb; nu; nu=nu->next) {
		if (nu->type == CU_BEZIER && (cu->drawflag & CU_HIDE_HANDLES)==0)
			tekenhandlesN(nu, 1, hide_handles);
		tekenvertsN(nu, 0, hide_handles, NULL);
	}
	
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

	/* direction vectors for 3d curve paths
	 * when at its lowest, don't render normals */
	if ((cu->flag & CU_3D) && (ts->normalsize > 0.0015f) && (cu->drawflag & CU_HIDE_NORMALS)==0) {

		UI_ThemeColor(TH_WIRE);
		for (bl=cu->bev.first,nu=nurb; nu && bl; bl=bl->next,nu=nu->next) {
			BevPoint *bevp= (BevPoint *)(bl + 1);
			int nr= bl->nr;
			int skip= nu->resolu/16;
			
			while (nr-->0) { /* accounts for empty bevel lists */
				const float fac= bevp->radius * ts->normalsize;
				float vec_a[3]; // Offset perpendicular to the curve
				float vec_b[3]; // Delta along the curve

				vec_a[0]= fac;
				vec_a[1]= 0.0f;
				vec_a[2]= 0.0f;

				vec_b[0]= -fac;
				vec_b[1]= 0.0f;
				vec_b[2]= 0.0f;
				
				mul_qt_v3(bevp->quat, vec_a);
				mul_qt_v3(bevp->quat, vec_b);
				add_v3_v3(vec_a, bevp->vec);
				add_v3_v3(vec_b, bevp->vec);

				madd_v3_v3fl(vec_a, bevp->dir, -fac);
				madd_v3_v3fl(vec_b, bevp->dir, -fac);

				glBegin(GL_LINE_STRIP);
				glVertex3fv(vec_a);
				glVertex3fv(bevp->vec);
				glVertex3fv(vec_b);
				glEnd();
				
				bevp += skip+1;
				nr -= skip;
			}
		}
	}

	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	for (nu=nurb; nu; nu=nu->next) {
		tekenvertsN(nu, 1, hide_handles, cu->lastsel);
	}
	
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
}

/* draw a sphere for use as an empty drawtype */
static void draw_empty_sphere (float size)
{
	static GLuint displist=0;
	
	if (displist == 0) {
		GLUquadricObj	*qobj;
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE);
		
		glPushMatrix();
		
		qobj = gluNewQuadric();
		gluQuadricDrawStyle(qobj, GLU_SILHOUETTE);
		gluDisk(qobj, 0.0,  1, 16, 1);
		
		glRotatef(90, 0, 1, 0);
		gluDisk(qobj, 0.0,  1, 16, 1);
		
		glRotatef(90, 1, 0, 0);
		gluDisk(qobj, 0.0,  1, 16, 1);
		
		gluDeleteQuadric(qobj);
		
		glPopMatrix();
		glEndList();
	}
	
	glScalef(size, size, size);
	glCallList(displist);
	glScalef(1.0f/size, 1.0f/size, 1.0f/size);
}

/* draw a cone for use as an empty drawtype */
static void draw_empty_cone (float size)
{
	float cent=0;
	float radius;
	GLUquadricObj *qobj = gluNewQuadric();
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE);
	
	
	glPushMatrix();
	
	radius = size;
	glTranslatef(cent,cent, cent);
	glScalef(radius, size * 2.0f, radius);
	glRotatef(-90., 1.0, 0.0, 0.0);
	gluCylinder(qobj, 1.0, 0.0, 1.0, 8, 1);

	glPopMatrix();
	
	gluDeleteQuadric(qobj);
}

/* draw points on curve speed handles */
#if 0 // XXX old animation system stuff
static void curve_draw_speed(Scene *scene, Object *ob)
{
	Curve *cu= ob->data;
	IpoCurve *icu;
	BezTriple *bezt;
	float loc[4], dir[3];
	int a;
	
	if (cu->ipo==NULL)
		return;
	
	icu= cu->ipo->curve.first;
	if (icu==NULL || icu->totvert<2)
		return;
	
	glPointSize( UI_GetThemeValuef(TH_VERTEX_SIZE) );
	bglBegin(GL_POINTS);

	for (a=0, bezt= icu->bezt; a<icu->totvert; a++, bezt++) {
		if ( where_on_path(ob, bezt->vec[1][1], loc, dir)) {
			UI_ThemeColor((bezt->f2 & SELECT) && ob==OBACT?TH_VERTEX_SELECT:TH_VERTEX);
			bglVertex3fv(loc);
		}
	}

	glPointSize(1.0);
	bglEnd();
}
#endif // XXX old animation system stuff


static void draw_textcurs(float textcurs[4][2])
{
	cpack(0);
	
	set_inverted_drawing(1);
	glBegin(GL_QUADS);
	glVertex2fv(textcurs[0]);
	glVertex2fv(textcurs[1]);
	glVertex2fv(textcurs[2]);
	glVertex2fv(textcurs[3]);
	glEnd();
	set_inverted_drawing(0);
}

static void drawspiral(const float cent[3], float rad, float tmat[][4], int start)
{
	float vec[3], vx[3], vy[3];
	const float tot_inv= (1.0f / (float)CIRCLE_RESOL);
	int a;
	char inverse= FALSE;
	float x, y, fac;

	if (start < 0) {
		inverse = TRUE;
		start= -start;
	}

	mul_v3_v3fl(vx, tmat[0], rad);
	mul_v3_v3fl(vy, tmat[1], rad);

	glBegin(GL_LINE_STRIP);

	if (inverse==0) {
		copy_v3_v3(vec, cent);
		glVertex3fv(vec);

		for (a=0; a<CIRCLE_RESOL; a++) {
			if (a+start>=CIRCLE_RESOL)
				start=-a + 1;

			fac= (float)a * tot_inv;
			x= sinval[a+start] * fac;
			y= cosval[a+start] * fac;

			vec[0]= cent[0] + (x * vx[0] + y * vy[0]);
			vec[1]= cent[1] + (x * vx[1] + y * vy[1]);
			vec[2]= cent[2] + (x * vx[2] + y * vy[2]);

			glVertex3fv(vec);
		}
	}
	else {
		fac= (float)(CIRCLE_RESOL-1) * tot_inv;
		x= sinval[start] * fac;
		y= cosval[start] * fac;

		vec[0]= cent[0] + (x * vx[0] + y * vy[0]);
		vec[1]= cent[1] + (x * vx[1] + y * vy[1]);
		vec[2]= cent[2] + (x * vx[2] + y * vy[2]);

		glVertex3fv(vec);

		for (a=0; a<CIRCLE_RESOL; a++) {
			if (a+start>=CIRCLE_RESOL)
				start=-a + 1;

			fac= (float)(-a+(CIRCLE_RESOL-1)) * tot_inv;
			x= sinval[a+start] * fac;
			y= cosval[a+start] * fac;

			vec[0]= cent[0] + (x * vx[0] + y * vy[0]);
			vec[1]= cent[1] + (x * vx[1] + y * vy[1]);
			vec[2]= cent[2] + (x * vx[2] + y * vy[2]);
			glVertex3fv(vec);
		}
	}

	glEnd();
}

/* draws a circle on x-z plane given the scaling of the circle, assuming that 
 * all required matrices have been set (used for drawing empties)
 */
static void drawcircle_size(float size)
{
	float x, y;
	short degrees;

	glBegin(GL_LINE_LOOP);

	/* coordinates are: cos(degrees*11.25)=x, sin(degrees*11.25)=y, 0.0f=z */
	for (degrees=0; degrees<CIRCLE_RESOL; degrees++) {
		x= cosval[degrees];
		y= sinval[degrees];
		
		glVertex3f(x*size, 0.0f, y*size);
	}
	
	glEnd();

}

/* needs fixing if non-identity matrice used */
static void drawtube(const float vec[3], float radius, float height, float tmat[][4])
{
	float cur[3];
	drawcircball(GL_LINE_LOOP, vec, radius, tmat);

	copy_v3_v3(cur,vec);
	cur[2]+=height;

	drawcircball(GL_LINE_LOOP, cur, radius, tmat);

	glBegin(GL_LINES);
	glVertex3f(vec[0]+radius,vec[1],vec[2]);
	glVertex3f(cur[0]+radius,cur[1],cur[2]);
	glVertex3f(vec[0]-radius,vec[1],vec[2]);
	glVertex3f(cur[0]-radius,cur[1],cur[2]);
	glVertex3f(vec[0],vec[1]+radius,vec[2]);
	glVertex3f(cur[0],cur[1]+radius,cur[2]);
	glVertex3f(vec[0],vec[1]-radius,vec[2]);
	glVertex3f(cur[0],cur[1]-radius,cur[2]);
	glEnd();
}
/* needs fixing if non-identity matrice used */
static void drawcone(const float vec[3], float radius, float height, float tmat[][4])
{
	float cur[3];

	copy_v3_v3(cur,vec);
	cur[2]+=height;

	drawcircball(GL_LINE_LOOP, cur, radius, tmat);

	glBegin(GL_LINES);
	glVertex3f(vec[0],vec[1],vec[2]);
	glVertex3f(cur[0]+radius,cur[1],cur[2]);
	glVertex3f(vec[0],vec[1],vec[2]);
	glVertex3f(cur[0]-radius,cur[1],cur[2]);
	glVertex3f(vec[0],vec[1],vec[2]);
	glVertex3f(cur[0],cur[1]+radius,cur[2]);
	glVertex3f(vec[0],vec[1],vec[2]);
	glVertex3f(cur[0],cur[1]-radius,cur[2]);
	glEnd();
}
/* return 1 if nothing was drawn */
static int drawmball(Scene *scene, View3D *v3d, RegionView3D *rv3d, Base *base, int dt)
{
	Object *ob= base->object;
	MetaBall *mb;
	MetaElem *ml;
	float imat[4][4];
	int code= 1;
	
	mb= ob->data;

	if (mb->editelems) {
		UI_ThemeColor(TH_WIRE);
		if ((G.f & G_PICKSEL)==0 ) drawDispList(scene, v3d, rv3d, base, dt);
		ml= mb->editelems->first;
	}
	else {
		if ((base->flag & OB_FROMDUPLI)==0)
			drawDispList(scene, v3d, rv3d, base, dt);
		ml= mb->elems.first;
	}

	if (ml==NULL) return 1;

	if (v3d->flag2 & V3D_RENDER_OVERRIDE) return 0;
	
	/* in case solid draw, reset wire colors */
	if (ob->flag & SELECT) {
		if (ob==OBACT) UI_ThemeColor(TH_ACTIVE);
		else UI_ThemeColor(TH_SELECT);
	}
	else UI_ThemeColor(TH_WIRE);

	invert_m4_m4(imat, rv3d->viewmatob);
	normalize_v3(imat[0]);
	normalize_v3(imat[1]);
	
	while (ml) {

		/* draw radius */
		if (mb->editelems) {
			if ((ml->flag & SELECT) && (ml->flag & MB_SCALE_RAD)) cpack(0xA0A0F0);
			else cpack(0x3030A0);
			
			if (G.f & G_PICKSEL) {
				ml->selcol1= code;
				glLoadName(code++);
			}
		}
		drawcircball(GL_LINE_LOOP, &(ml->x), ml->rad, imat);

		/* draw stiffness */
		if (mb->editelems) {
			if ((ml->flag & SELECT) && !(ml->flag & MB_SCALE_RAD)) cpack(0xA0F0A0);
			else cpack(0x30A030);
			
			if (G.f & G_PICKSEL) {
				ml->selcol2= code;
				glLoadName(code++);
			}
			drawcircball(GL_LINE_LOOP, &(ml->x), ml->rad*atanf(ml->s)/(float)M_PI_2, imat);
		}
		
		ml= ml->next;
	}
	return 0;
}

static void draw_forcefield(Scene *scene, Object *ob, RegionView3D *rv3d)
{
	PartDeflect *pd= ob->pd;
	float imat[4][4], tmat[4][4];
	float vec[3]= {0.0, 0.0, 0.0};
	int curcol;
	float size;

	/* XXX why? */
	if (ob!=scene->obedit && (ob->flag & SELECT)) {
		if (ob==OBACT) curcol= TH_ACTIVE;
		else curcol= TH_SELECT;
	}
	else curcol= TH_EMPTY;
	
	/* scale size of circle etc with the empty drawsize */
	if (ob->type == OB_EMPTY) size = ob->empty_drawsize;
	else size = 1.0;
	
	/* calculus here, is reused in PFIELD_FORCE */
	invert_m4_m4(imat, rv3d->viewmatob);
//	normalize_v3(imat[0]);		// we don't do this because field doesnt scale either... apart from wind!
//	normalize_v3(imat[1]);
	
	if (pd->forcefield == PFIELD_WIND) {
		float force_val;
		
		unit_m4(tmat);
		UI_ThemeColorBlend(curcol, TH_BACK, 0.5);
		
		//if (has_ipo_code(ob->ipo, OB_PD_FSTR))
		//	force_val = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, scene->r.cfra);
		//else
		{
			force_val = pd->f_strength;
		}
		force_val *= 0.1f;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2]= 0.5f * force_val;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2]= 1.0f * force_val;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2]= 1.5f * force_val;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2] = 0.0f; /* reset vec for max dist circle */
		
	}
	else if (pd->forcefield == PFIELD_FORCE) {
		float ffall_val;

		//if (has_ipo_code(ob->ipo, OB_PD_FFALL))
		//	ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, scene->r.cfra);
		//else
		{
			ffall_val = pd->f_power;
		}

		UI_ThemeColorBlend(curcol, TH_BACK, 0.5);
		drawcircball(GL_LINE_LOOP, vec, size, imat);
		UI_ThemeColorBlend(curcol, TH_BACK, 0.9f - 0.4f / powf(1.5f, ffall_val));
		drawcircball(GL_LINE_LOOP, vec, size * 1.5f, imat);
		UI_ThemeColorBlend(curcol, TH_BACK, 0.9f - 0.4f / powf(2.0f, ffall_val));
		drawcircball(GL_LINE_LOOP, vec, size*2.0f, imat);
	}
	else if (pd->forcefield == PFIELD_VORTEX) {
		float /*ffall_val,*/ force_val;

		unit_m4(tmat);
		//if (has_ipo_code(ob->ipo, OB_PD_FFALL))
		//	ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, scene->r.cfra);
		//else
		//	ffall_val = pd->f_power;

		//if (has_ipo_code(ob->ipo, OB_PD_FSTR))
		//	force_val = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, scene->r.cfra);
		//else
		{
			force_val = pd->f_strength;
		}

		UI_ThemeColorBlend(curcol, TH_BACK, 0.7f);
		if (force_val < 0) {
			drawspiral(vec, size, tmat, 1);
			drawspiral(vec, size, tmat, 16);
		}
		else {
			drawspiral(vec, size, tmat, -1);
			drawspiral(vec, size, tmat, -16);
		}
	}
	else if (pd->forcefield == PFIELD_GUIDE && ob->type==OB_CURVE) {
		Curve *cu= ob->data;
		if ((cu->flag & CU_PATH) && cu->path && cu->path->data) {
			float mindist, guidevec1[4], guidevec2[3];

			//if (has_ipo_code(ob->ipo, OB_PD_FSTR))
			//	mindist = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, scene->r.cfra);
			//else
			{
				mindist = pd->f_strength;
			}

			/*path end*/
			setlinestyle(3);
			where_on_path(ob, 1.0f, guidevec1, guidevec2, NULL, NULL, NULL);
			UI_ThemeColorBlend(curcol, TH_BACK, 0.5);
			drawcircball(GL_LINE_LOOP, guidevec1, mindist, imat);

			/*path beginning*/
			setlinestyle(0);
			where_on_path(ob, 0.0f, guidevec1, guidevec2, NULL, NULL, NULL);
			UI_ThemeColorBlend(curcol, TH_BACK, 0.5);
			drawcircball(GL_LINE_LOOP, guidevec1, mindist, imat);
			
			copy_v3_v3(vec, guidevec1);	/* max center */
		}
	}

	setlinestyle(3);
	UI_ThemeColorBlend(curcol, TH_BACK, 0.5);

	if (pd->falloff==PFIELD_FALL_SPHERE) {
		/* as last, guide curve alters it */
		if (pd->flag & PFIELD_USEMAX)
			drawcircball(GL_LINE_LOOP, vec, pd->maxdist, imat);

		if (pd->flag & PFIELD_USEMIN)
			drawcircball(GL_LINE_LOOP, vec, pd->mindist, imat);
	}
	else if (pd->falloff==PFIELD_FALL_TUBE) {
		float radius,distance;

		unit_m4(tmat);

		vec[0]=vec[1]=0.0f;
		radius=(pd->flag&PFIELD_USEMAXR)?pd->maxrad:1.0f;
		distance=(pd->flag&PFIELD_USEMAX)?pd->maxdist:0.0f;
		vec[2]=distance;
		distance=(pd->flag&PFIELD_POSZ)?-distance:-2.0f*distance;

		if (pd->flag & (PFIELD_USEMAX|PFIELD_USEMAXR))
			drawtube(vec,radius,distance,tmat);

		radius=(pd->flag&PFIELD_USEMINR)?pd->minrad:1.0f;
		distance=(pd->flag&PFIELD_USEMIN)?pd->mindist:0.0f;
		vec[2]=distance;
		distance=(pd->flag&PFIELD_POSZ)?-distance:-2.0f*distance;

		if (pd->flag & (PFIELD_USEMIN|PFIELD_USEMINR))
			drawtube(vec,radius,distance,tmat);
	}
	else if (pd->falloff==PFIELD_FALL_CONE) {
		float radius,distance;

		unit_m4(tmat);

		radius= DEG2RADF((pd->flag&PFIELD_USEMAXR) ? pd->maxrad : 1.0f);
		distance=(pd->flag&PFIELD_USEMAX)?pd->maxdist:0.0f;

		if (pd->flag & (PFIELD_USEMAX|PFIELD_USEMAXR)) {
			drawcone(vec, distance * sinf(radius),distance * cosf(radius), tmat);
			if ((pd->flag & PFIELD_POSZ)==0)
				drawcone(vec, distance * sinf(radius),-distance * cosf(radius),tmat);
		}

		radius= DEG2RADF((pd->flag&PFIELD_USEMINR) ? pd->minrad : 1.0f);
		distance=(pd->flag&PFIELD_USEMIN)?pd->mindist:0.0f;

		if (pd->flag & (PFIELD_USEMIN|PFIELD_USEMINR)) {
			drawcone(vec,distance*sinf(radius),distance*cosf(radius),tmat);
			if ((pd->flag & PFIELD_POSZ)==0)
				drawcone(vec, distance * sinf(radius), -distance * cosf(radius), tmat);
		}
	}
	setlinestyle(0);
}

static void draw_box(float vec[8][3])
{
	glBegin(GL_LINE_STRIP);
	glVertex3fv(vec[0]); glVertex3fv(vec[1]);glVertex3fv(vec[2]); glVertex3fv(vec[3]);
	glVertex3fv(vec[0]); glVertex3fv(vec[4]);glVertex3fv(vec[5]); glVertex3fv(vec[6]);
	glVertex3fv(vec[7]); glVertex3fv(vec[4]);
	glEnd();

	glBegin(GL_LINES);
	glVertex3fv(vec[1]); glVertex3fv(vec[5]);
	glVertex3fv(vec[2]); glVertex3fv(vec[6]);
	glVertex3fv(vec[3]); glVertex3fv(vec[7]);
	glEnd();
}

/* uses boundbox, function used by Ketsji */
#if 0
static void get_local_bounds(Object *ob, float center[3], float size[3])
{
	BoundBox *bb= object_get_boundbox(ob);
	
	if (bb==NULL) {
		zero_v3(center);
		copy_v3_v3(size, ob->size);
	}
	else {
		size[0]= 0.5*fabs(bb->vec[0][0] - bb->vec[4][0]);
		size[1]= 0.5*fabs(bb->vec[0][1] - bb->vec[2][1]);
		size[2]= 0.5*fabs(bb->vec[0][2] - bb->vec[1][2]);
		
		center[0]= (bb->vec[0][0] + bb->vec[4][0])/2.0;
		center[1]= (bb->vec[0][1] + bb->vec[2][1])/2.0;
		center[2]= (bb->vec[0][2] + bb->vec[1][2])/2.0;
	}
}
#endif

static void draw_bb_quadric(BoundBox *bb, char type)
{
	float size[3], cent[3];
	GLUquadricObj *qobj = gluNewQuadric();
	
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE);
	
	size[0]= 0.5f*fabsf(bb->vec[0][0] - bb->vec[4][0]);
	size[1]= 0.5f*fabsf(bb->vec[0][1] - bb->vec[2][1]);
	size[2]= 0.5f*fabsf(bb->vec[0][2] - bb->vec[1][2]);
	
	cent[0]= 0.5f*(bb->vec[0][0] + bb->vec[4][0]);
	cent[1]= 0.5f*(bb->vec[0][1] + bb->vec[2][1]);
	cent[2]= 0.5f*(bb->vec[0][2] + bb->vec[1][2]);
	
	glPushMatrix();
	if (type==OB_BOUND_SPHERE) {
		glTranslatef(cent[0], cent[1], cent[2]);
		glScalef(size[0], size[1], size[2]);
		gluSphere(qobj, 1.0, 8, 5);
	}
	else if (type==OB_BOUND_CYLINDER) {
		float radius = size[0] > size[1] ? size[0] : size[1];
		glTranslatef(cent[0], cent[1], cent[2]-size[2]);
		glScalef(radius, radius, 2.0f * size[2]);
		gluCylinder(qobj, 1.0, 1.0, 1.0, 8, 1);
	}
	else if (type==OB_BOUND_CONE) {
		float radius = size[0] > size[1] ? size[0] : size[1];
		glTranslatef(cent[0], cent[1], cent[2]-size[2]);
		glScalef(radius, radius, 2.0f * size[2]);
		gluCylinder(qobj, 1.0, 0.0, 1.0, 8, 1);
	}
	glPopMatrix();
	
	gluDeleteQuadric(qobj);
}

static void draw_bounding_volume(Scene *scene, Object *ob, char type)
{
	BoundBox *bb= NULL;
	
	if (ob->type==OB_MESH) {
		bb= mesh_get_bb(ob);
	}
	else if (ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		bb= ob->bb ? ob->bb : ( (Curve *)ob->data )->bb;
	}
	else if (ob->type==OB_MBALL) {
		if (is_basis_mball(ob)) {
			bb= ob->bb;
			if (bb==NULL) {
				makeDispListMBall(scene, ob);
				bb= ob->bb;
			}
		}
	}
	else if (ob->type == OB_ARMATURE) {
		bb = BKE_armature_get_bb(ob);
	}
	else {
		drawcube();
		return;
	}
	
	if (bb==NULL) return;
	
	if (type==OB_BOUND_BOX) draw_box(bb->vec);
	else draw_bb_quadric(bb, type);
	
}

static void drawtexspace(Object *ob)
{
	float vec[8][3], loc[3], size[3];
	
	if (ob->type==OB_MESH) {
		mesh_get_texspace(ob->data, loc, NULL, size);
	}
	else if (ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		Curve *cu= ob->data;
		copy_v3_v3(size, cu->size);
		copy_v3_v3(loc, cu->loc);
	}
	else if (ob->type==OB_MBALL) {
		MetaBall *mb= ob->data;
		copy_v3_v3(size, mb->size);
		copy_v3_v3(loc, mb->loc);
	}
	else return;
	
	vec[0][0]=vec[1][0]=vec[2][0]=vec[3][0]= loc[0]-size[0];
	vec[4][0]=vec[5][0]=vec[6][0]=vec[7][0]= loc[0]+size[0];
	
	vec[0][1]=vec[1][1]=vec[4][1]=vec[5][1]= loc[1]-size[1];
	vec[2][1]=vec[3][1]=vec[6][1]=vec[7][1]= loc[1]+size[1];

	vec[0][2]=vec[3][2]=vec[4][2]=vec[7][2]= loc[2]-size[2];
	vec[1][2]=vec[2][2]=vec[5][2]=vec[6][2]= loc[2]+size[2];
	
	setlinestyle(2);

	draw_box(vec);

	setlinestyle(0);
}

/* draws wire outline */
static void drawObjectSelect(Scene *scene, View3D *v3d, ARegion *ar, Base *base)
{
	RegionView3D *rv3d= ar->regiondata;
	Object *ob= base->object;
	
	glLineWidth(2.0);
	glDepthMask(0);
	
	if (ELEM3(ob->type, OB_FONT,OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		DerivedMesh *dm = ob->derivedFinal;
		int hasfaces= 0;

		if (dm) {
			hasfaces= dm->getNumTessFaces(dm);
		}
		else {
			hasfaces= displist_has_faces(&ob->disp);
		}

		if (hasfaces && ED_view3d_boundbox_clip(rv3d, ob->obmat, ob->bb ? ob->bb : cu->bb)) {
			draw_index_wire= 0;
			if (dm) {
				draw_mesh_object_outline(v3d, ob, dm);
			}
			else {
				drawDispListwire(&ob->disp);
			}
			draw_index_wire= 1;
		}
	}
	else if (ob->type==OB_MBALL) {
		if (is_basis_mball(ob)) {
			if ((base->flag & OB_FROMDUPLI)==0)
				drawDispListwire(&ob->disp);
		}
	}
	else if (ob->type==OB_ARMATURE) {
		if (!(ob->mode & OB_MODE_POSE && base == scene->basact))
			draw_armature(scene, v3d, ar, base, OB_WIRE, FALSE, TRUE);
	}

	glLineWidth(1.0);
	glDepthMask(1);
}

static void drawWireExtra(Scene *scene, RegionView3D *rv3d, Object *ob) 
{
	if (ob!=scene->obedit && (ob->flag & SELECT)) {
		if (ob==OBACT) {
			if (ob->flag & OB_FROMGROUP) UI_ThemeColor(TH_GROUP_ACTIVE);
			else UI_ThemeColor(TH_ACTIVE);
		}
		else if (ob->flag & OB_FROMGROUP)
			UI_ThemeColorShade(TH_GROUP_ACTIVE, -16);
		else
			UI_ThemeColor(TH_SELECT);
	}
	else {
		if (ob->flag & OB_FROMGROUP)
			UI_ThemeColor(TH_GROUP);
		else {
			if (ob->dtx & OB_DRAWWIRE) {
				glColor3ub(80,80,80);
			}
			else {
				UI_ThemeColor(TH_WIRE);
			}
		}
	}
	
	bglPolygonOffset(rv3d->dist, 1.0);
	glDepthMask(0);	// disable write in zbuffer, selected edge wires show better
	
	if (ELEM3(ob->type, OB_FONT, OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		if (ED_view3d_boundbox_clip(rv3d, ob->obmat, ob->bb ? ob->bb : cu->bb)) {
			if (ob->type==OB_CURVE)
				draw_index_wire= 0;

			if (ob->derivedFinal) {
				drawCurveDMWired(ob);
			}
			else {
				drawDispListwire(&ob->disp);
			}

			if (ob->type==OB_CURVE)
				draw_index_wire= 1;
		}
	}
	else if (ob->type==OB_MBALL) {
		if (is_basis_mball(ob)) {
			drawDispListwire(&ob->disp);
		}
	}

	glDepthMask(1);
	bglPolygonOffset(rv3d->dist, 0.0);
}

/* should be called in view space */
static void draw_hooks(Object *ob)
{
	ModifierData *md;
	float vec[3];
	
	for (md=ob->modifiers.first; md; md=md->next) {
		if (md->type==eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData*) md;

			mul_v3_m4v3(vec, ob->obmat, hmd->cent);

			if (hmd->object) {
				setlinestyle(3);
				glBegin(GL_LINES);
				glVertex3fv(hmd->object->obmat[3]);
				glVertex3fv(vec);
				glEnd();
				setlinestyle(0);
			}

			glPointSize(3.0);
			bglBegin(GL_POINTS);
			bglVertex3fv(vec);
			bglEnd();
			glPointSize(1.0);
		}
	}
}

static void drawRBpivot(bRigidBodyJointConstraint *data)
{
	const char *axis_str[3] = {"px", "py", "pz"};
	int axis;
	float mat[4][4];

	/* color */
	float curcol[4];
	unsigned char tcol[4];

	glGetFloatv(GL_CURRENT_COLOR, curcol);
	rgb_float_to_uchar(tcol, curcol);
	tcol[3]= 255;

	eul_to_mat4(mat,&data->axX);
	glLineWidth (4.0f);
	setlinestyle(2);
	for (axis=0; axis<3; axis++) {
		float dir[3] = {0,0,0};
		float v[3];

		copy_v3_v3(v, &data->pivX);

		dir[axis] = 1.f;
		glBegin(GL_LINES);
		mul_m4_v3(mat,dir);
		add_v3_v3(v, dir);
		glVertex3fv(&data->pivX);
		glVertex3fv(v);
		glEnd();

		view3d_cached_text_draw_add(v, axis_str[axis], 0, V3D_CACHE_TEXT_ASCII, tcol);
	}
	glLineWidth (1.0f);
	setlinestyle(0);
}

/* flag can be DRAW_PICKING	and/or DRAW_CONSTCOLOR, DRAW_SCENESET */
void draw_object(Scene *scene, ARegion *ar, View3D *v3d, Base *base, int flag)
{
	static int warning_recursive= 0;
	ModifierData *md = NULL;
	Object *ob= base->object;
	Curve *cu;
	RegionView3D *rv3d= ar->regiondata;
	float vec1[3], vec2[3];
	unsigned int col=0;
	int /*sel, drawtype,*/ colindex= 0;
	int i, selstart, selend, empty_object=0;
	short dt, dtx, zbufoff= 0;
	const short is_obact= (ob == OBACT);

	/* only once set now, will be removed too, should become a global standard */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (ob!=scene->obedit) {
		if (ob->restrictflag & OB_RESTRICT_VIEW) {
			return;
		}
		else if ((ob->restrictflag & OB_RESTRICT_RENDER) &&
		         (v3d->flag2 & V3D_RENDER_OVERRIDE))
		{
			return;
		}
	}

	/* XXX particles are not safe for simultaneous threaded render */
	if (G.rendering && ob->particlesystem.first)
		return;

	/* xray delay? */
	if ((flag & DRAW_PICKING)==0 && (base->flag & OB_FROMDUPLI)==0) {
		/* don't do xray in particle mode, need the z-buffer */
		if (!(ob->mode & OB_MODE_PARTICLE_EDIT)) {
			/* xray and transp are set when it is drawing the 2nd/3rd pass */
			if (!v3d->xray && !v3d->transp && (ob->dtx & OB_DRAWXRAY) && !(ob->dtx & OB_DRAWTRANSP)) {
				add_view3d_after(&v3d->afterdraw_xray, base, flag);
				return;
			}
		}
	}

	/* no return after this point, otherwise leaks */
	view3d_cached_text_draw_begin();
	
	/* patch? children objects with a timeoffs change the parents. How to solve! */
	/* if ( ((int)ob->ctime) != F_(scene->r.cfra)) where_is_object(scene, ob); */
	
	/* draw motion paths (in view space) */
	if (ob->mpath && (v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
		bAnimVizSettings *avs= &ob->avs;
		
		/* setup drawing environment for paths */
		draw_motion_paths_init(v3d, ar);
		
		/* draw motion path for object */
		draw_motion_path_instance(scene, ob, NULL, avs, ob->mpath);
		
		/* cleanup after drawing */
		draw_motion_paths_cleanup(v3d);
	}

	/* multiply view with object matrix.
	 * local viewmat and persmat, to calculate projections */
	ED_view3d_init_mats_rv3d_gl(ob, rv3d);

	/* which wire color */
	if ((flag & DRAW_CONSTCOLOR) == 0) {
		/* confusing logic here, there are 2 methods of setting the color
		 * 'colortab[colindex]' and 'theme_id', colindex overrides theme_id.
		 *
		 * note: no theme yet for 'colindex' */
		int theme_id= TH_WIRE;
		int theme_shade= 0;

		project_short(ar, ob->obmat[3], &base->sx);

		if ((scene->obedit == NULL) &&
		    (G.moving & G_TRANSFORM_OBJ) &&
		    (base->flag & (SELECT+BA_WAS_SEL)))
		{
			theme_id= TH_TRANSFORM;
		}
		else {
			/* Sets the 'colindex' */
			if (ob->id.lib) {
				colindex= (base->flag & (SELECT+BA_WAS_SEL)) ? 4 : 3;
			}
			else if (warning_recursive==1) {
				if (base->flag & (SELECT+BA_WAS_SEL)) {
					colindex= (scene->basact==base) ? 8 : 7;
				}
				else {
					colindex = 6;
				}
			}
			/* Sets the 'theme_id' or fallback to wire */
			else {
				if (ob->flag & OB_FROMGROUP) {
					if (base->flag & (SELECT+BA_WAS_SEL)) {
						/* uses darker active color for non-active + selected*/
						theme_id= TH_GROUP_ACTIVE;
						
						if (scene->basact != base) {
							theme_shade= -16;
						}
					}
					else {
						theme_id= TH_GROUP;
					}
				}
				else {
					if (base->flag & (SELECT+BA_WAS_SEL)) {
						theme_id= scene->basact == base ? TH_ACTIVE : TH_SELECT;
					}
					else {
						if (ob->type==OB_LAMP) theme_id= TH_LAMP;
						else if (ob->type==OB_SPEAKER) theme_id= TH_SPEAKER;
						else if (ob->type==OB_CAMERA) theme_id= TH_CAMERA;
						else if (ob->type==OB_EMPTY) theme_id= TH_EMPTY;
						/* fallback to TH_WIRE */
					}
				}
			}
		}

		/* finally set the color */
		if (colindex == 0) {
			if (theme_shade == 0) UI_ThemeColor(theme_id);
			else                 UI_ThemeColorShade(theme_id, theme_shade);
		}
		else {
			col= colortab[colindex];
			cpack(col);
		}
	}

	/* maximum drawtype */
	dt= v3d->drawtype;
	if (dt==OB_RENDER) dt= OB_SOLID;
	dt= MIN2(dt, ob->dt);
	if (v3d->zbuf==0 && dt>OB_WIRE) dt= OB_WIRE;
	dtx= 0;

	/* faceselect exception: also draw solid when dt==wire, except in editmode */
	if (is_obact && (ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT))) {
		if (ob->type==OB_MESH) {

			if (ob->mode & OB_MODE_EDIT) {
				/* pass */
			}
			else {
				if (dt<OB_SOLID) {
					zbufoff= 1;
					dt= OB_SOLID;
				}
				else {
					dt= OB_PAINT;
				}

				glEnable(GL_DEPTH_TEST);
			}
		}
		else {
			if (dt<OB_SOLID) {
				dt= OB_SOLID;
				glEnable(GL_DEPTH_TEST);
				zbufoff= 1;
			}
		}
	}
	
	/* draw-extra supported for boundbox drawmode too */
	if (dt>=OB_BOUNDBOX ) {

		dtx= ob->dtx;
		if (ob->mode & OB_MODE_EDIT) {
			// the only 2 extra drawtypes alowed in editmode
			dtx= dtx & (OB_DRAWWIRE|OB_TEXSPACE);
		}

	}

	/* bad exception, solve this! otherwise outline shows too late */
	if (ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		/* still needed for curves hidden in other layers. depgraph doesnt handle that yet */
		if (ob->disp.first==NULL) makeDispListCurveTypes(scene, ob, 0);
	}
	
	/* draw outline for selected objects, mesh does itself */
	if ((v3d->flag & V3D_SELECT_OUTLINE) && ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) && ob->type!=OB_MESH) {
		if (dt>OB_WIRE && (ob->mode & OB_MODE_EDIT)==0 && (flag & DRAW_SCENESET)==0) {
			if (!(ob->dtx&OB_DRAWWIRE) && (ob->flag&SELECT) && !(flag&DRAW_PICKING)) {
				
				drawObjectSelect(scene, v3d, ar, base);
			}
		}
	}

	switch( ob->type) {
		case OB_MESH:
			empty_object= draw_mesh_object(scene, ar, v3d, rv3d, base, dt, flag);
			if (flag!=DRAW_CONSTCOLOR) dtx &= ~OB_DRAWWIRE; // mesh draws wire itself

			break;
		case OB_FONT:
			cu= ob->data;
			if (cu->editfont) {
				draw_textcurs(cu->editfont->textcurs);

				if (cu->flag & CU_FAST) {
					cpack(0xFFFFFF);
					set_inverted_drawing(1);
					drawDispList(scene, v3d, rv3d, base, OB_WIRE);
					set_inverted_drawing(0);
				}
				else {
					drawDispList(scene, v3d, rv3d, base, dt);
				}

				if (cu->linewidth != 0.0f) {
					UI_ThemeColor(TH_WIRE);
					copy_v3_v3(vec1, ob->orig);
					copy_v3_v3(vec2, ob->orig);
					vec1[0] += cu->linewidth;
					vec2[0] += cu->linewidth;
					vec1[1] += cu->linedist * cu->fsize;
					vec2[1] -= cu->lines * cu->linedist * cu->fsize;
					setlinestyle(3);
					glBegin(GL_LINE_STRIP);
					glVertex2fv(vec1);
					glVertex2fv(vec2);
					glEnd();
					setlinestyle(0);
				}

				setlinestyle(3);
				for (i=0; i<cu->totbox; i++) {
					if (cu->tb[i].w != 0.0f) {
						UI_ThemeColor(i == (cu->actbox-1) ? TH_ACTIVE : TH_WIRE);
						vec1[0] = (cu->xof * cu->fsize) + cu->tb[i].x;
						vec1[1] = (cu->yof * cu->fsize) + cu->tb[i].y + cu->fsize;
						vec1[2] = 0.001;
						glBegin(GL_LINE_STRIP);
						glVertex3fv(vec1);
						vec1[0] += cu->tb[i].w;
						glVertex3fv(vec1);
						vec1[1] -= cu->tb[i].h;
						glVertex3fv(vec1);
						vec1[0] -= cu->tb[i].w;
						glVertex3fv(vec1);
						vec1[1] += cu->tb[i].h;
						glVertex3fv(vec1);
						glEnd();
					}
				}
				setlinestyle(0);


				if (BKE_font_getselection(ob, &selstart, &selend) && cu->selboxes) {
					float selboxw;

					cpack(0xffffff);
					set_inverted_drawing(1);
					for (i=0; i<(selend-selstart+1); i++) {
						SelBox *sb = &(cu->selboxes[i]);

						if (i<(selend-selstart)) {
							if (cu->selboxes[i+1].y == sb->y)
								selboxw= cu->selboxes[i+1].x - sb->x;
							else
								selboxw= sb->w;
						}
						else {
							selboxw= sb->w;
						}
						glBegin(GL_QUADS);
						glVertex3f(sb->x, sb->y, 0.001);
						glVertex3f(sb->x+selboxw, sb->y, 0.001);
						glVertex3f(sb->x+selboxw, sb->y+sb->h, 0.001);
						glVertex3f(sb->x, sb->y+sb->h, 0.001);
						glEnd();
					}
					set_inverted_drawing(0);
				}
			}
			else if (dt==OB_BOUNDBOX) {
				if (((v3d->flag2 & V3D_RENDER_OVERRIDE) && v3d->drawtype >= OB_WIRE) == 0) {
					draw_bounding_volume(scene, ob, ob->boundtype);
				}
			}
			else if (ED_view3d_boundbox_clip(rv3d, ob->obmat, ob->bb ? ob->bb : cu->bb)) {
				empty_object= drawDispList(scene, v3d, rv3d, base, dt);
			}

			break;
		case OB_CURVE:
		case OB_SURF:
			cu= ob->data;

			if (cu->editnurb) {
				ListBase *nurbs= curve_editnurbs(cu);
				drawnurb(scene, v3d, rv3d, base, nurbs->first, dt);
			}
			else if (dt==OB_BOUNDBOX) {
				if (((v3d->flag2 & V3D_RENDER_OVERRIDE) && (v3d->drawtype >= OB_WIRE)) == 0) {
					draw_bounding_volume(scene, ob, ob->boundtype);
				}
			}
			else if (ED_view3d_boundbox_clip(rv3d, ob->obmat, ob->bb ? ob->bb : cu->bb)) {
				empty_object= drawDispList(scene, v3d, rv3d, base, dt);

//XXX old animsys				if (cu->path)
//               					curve_draw_speed(scene, ob);
			}
			break;
		case OB_MBALL:
		{
			MetaBall *mb= ob->data;
			
			if (mb->editelems)
				drawmball(scene, v3d, rv3d, base, dt);
			else if (dt==OB_BOUNDBOX) {
				if (((v3d->flag2 & V3D_RENDER_OVERRIDE) && (v3d->drawtype >= OB_WIRE)) == 0) {
					draw_bounding_volume(scene, ob, ob->boundtype);
				}
			}
			else
				empty_object= drawmball(scene, v3d, rv3d, base, dt);
			break;
		}
		case OB_EMPTY:
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
				if (ob->empty_drawtype == OB_EMPTY_IMAGE) {
					draw_empty_image(ob);
				}
				else {
					drawaxes(ob->empty_drawsize, ob->empty_drawtype);
				}
			}
			break;
		case OB_LAMP:
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
				drawlamp(scene, v3d, rv3d, base, dt, flag);
				if (dtx || (base->flag & SELECT)) glMultMatrixf(ob->obmat);
			}
			break;
		case OB_CAMERA:
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0 ||
			    (rv3d->persp==RV3D_CAMOB && v3d->camera==ob)) /* special exception for active camera */
			{
				drawcamera(scene, v3d, rv3d, base, flag);
				break;
			}
		case OB_SPEAKER:
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0)
				drawspeaker(scene, v3d, rv3d, ob, flag);
			break;
		case OB_LATTICE:
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
				drawlattice(scene, v3d, ob);
			}
			break;
		case OB_ARMATURE:
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
				/* Do not allow boundbox in edit nor pose mode! */
				if ((dt == OB_BOUNDBOX) && (ob->mode & (OB_MODE_EDIT | OB_MODE_POSE)))
					dt = OB_WIRE;
				if (dt == OB_BOUNDBOX) {
					draw_bounding_volume(scene, ob, ob->boundtype);
				}
				else {
					if (dt>OB_WIRE)
						GPU_enable_material(0, NULL); /* we use default material */
					empty_object = draw_armature(scene, v3d, ar, base, dt, flag, FALSE);
					if (dt>OB_WIRE)
						GPU_disable_material();
				}
			}
			break;
		default:
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
				drawaxes(1.0, OB_ARROWS);
			}
	}

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {

		if (ob->soft /*&& flag & OB_SBMOTION*/) {
			float mrt[3][3],msc[3][3],mtr[3][3];
			SoftBody *sb= NULL;
			float tipw = 0.5f, tiph = 0.5f,drawsize = 4.0f;
			if ((sb= ob->soft)) {
				if (sb->solverflags & SBSO_ESTIMATEIPO) {

					glLoadMatrixf(rv3d->viewmat);
					copy_m3_m3(msc,sb->lscale);
					copy_m3_m3(mrt,sb->lrot);
					mul_m3_m3m3(mtr,mrt,msc);
					ob_draw_RE_motion(sb->lcom,mtr,tipw,tiph,drawsize);
					glMultMatrixf(ob->obmat);
				}
			}
		}

		if (ob->pd && ob->pd->forcefield) {
			draw_forcefield(scene, ob, rv3d);
		}
	}

	/* code for new particle system */
	if ((warning_recursive==0) &&
	    (ob->particlesystem.first) &&
	    (flag & DRAW_PICKING)==0 &&
	    (ob!=scene->obedit)
	    )
	{
		ParticleSystem *psys;

		if (col || (ob->flag & SELECT)) cpack(0xFFFFFF);	/* for visibility, also while wpaint */
		//glDepthMask(GL_FALSE);

		glLoadMatrixf(rv3d->viewmat);
		
		view3d_cached_text_draw_begin();

		for (psys=ob->particlesystem.first; psys; psys=psys->next) {
			/* run this so that possible child particles get cached */
			if (ob->mode & OB_MODE_PARTICLE_EDIT && is_obact) {
				PTCacheEdit *edit = PE_create_current(scene, ob);
				if (edit && edit->psys == psys)
					draw_update_ptcache_edit(scene, ob, edit);
			}

			draw_new_particle_system(scene, v3d, rv3d, base, psys, dt);
		}
		invert_m4_m4(ob->imat, ob->obmat);
		view3d_cached_text_draw_end(v3d, ar, 0, NULL);

		glMultMatrixf(ob->obmat);
		
		//glDepthMask(GL_TRUE);
		if (col) cpack(col);
	}

	/* draw edit particles last so that they can draw over child particles */
	if ( (warning_recursive==0) &&
	     (flag & DRAW_PICKING)==0 &&
	     (!scene->obedit))
	{

		if (ob->mode & OB_MODE_PARTICLE_EDIT && is_obact) {
			PTCacheEdit *edit = PE_create_current(scene, ob);
			if (edit) {
				glLoadMatrixf(rv3d->viewmat);
				draw_update_ptcache_edit(scene, ob, edit);
				draw_ptcache_edit(scene, v3d, edit);
				glMultMatrixf(ob->obmat);
			}
		}
	}

	/* draw code for smoke */
	if ((md = modifiers_findByType(ob, eModifierType_Smoke))) {
		SmokeModifierData *smd = (SmokeModifierData *)md;

		// draw collision objects
		if ((smd->type & MOD_SMOKE_TYPE_COLL) && smd->coll) {
#if 0
			SmokeCollSettings *scs = smd->coll;
			if (scs->points) {
				size_t i;

				glLoadMatrixf(rv3d->viewmat);

				if (col || (ob->flag & SELECT)) cpack(0xFFFFFF);
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				

				// glPointSize(3.0);
				bglBegin(GL_POINTS);

				for (i = 0; i < scs->numpoints; i++)
				{
					bglVertex3fv(&scs->points[3*i]);
				}

				bglEnd();
				glPointSize(1.0);

				glMultMatrixf(ob->obmat);
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
				if (col) cpack(col);
				
			}
#endif
		}

		// only draw domains
		if (smd->domain && smd->domain->fluid) {
			if (CFRA < smd->domain->point_cache[0]->startframe) {
				/* don't show smoke before simulation starts, this could be made an option in the future */
			}
			else if (!smd->domain->wt || !(smd->domain->viewsettings & MOD_SMOKE_VIEW_SHOWBIG)) {
// #if 0
				smd->domain->tex = NULL;
				GPU_create_smoke(smd, 0);
				draw_volume(ar, smd->domain->tex,
				            smd->domain->p0, smd->domain->p1,
				            smd->domain->res, smd->domain->dx,
				            smd->domain->tex_shadow);
				GPU_free_smoke(smd);
// #endif
#if 0
				int x, y, z;
				float *density = smoke_get_density(smd->domain->fluid);

				glLoadMatrixf(rv3d->viewmat);
				// glMultMatrixf(ob->obmat);

				if (col || (ob->flag & SELECT)) cpack(0xFFFFFF);
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				

				// glPointSize(3.0);
				bglBegin(GL_POINTS);

				for (x = 0; x < smd->domain->res[0]; x++) {
					for (y = 0; y < smd->domain->res[1]; y++) {
						for (z = 0; z < smd->domain->res[2]; z++) {
							float tmp[3];
							int index = smoke_get_index(x, smd->domain->res[0], y, smd->domain->res[1], z);

							if (density[index] > FLT_EPSILON) {
								float color[3];
								copy_v3_v3(tmp, smd->domain->p0);
								tmp[0] += smd->domain->dx * x + smd->domain->dx * 0.5;
								tmp[1] += smd->domain->dx * y + smd->domain->dx * 0.5;
								tmp[2] += smd->domain->dx * z + smd->domain->dx * 0.5;
								color[0] = color[1] = color[2] = density[index];
								glColor3fv(color);
								bglVertex3fv(tmp);
							}
						}
					}
				}

				bglEnd();
				glPointSize(1.0);

				glMultMatrixf(ob->obmat);
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
				if (col) cpack(col);
#endif
			}
			else if (smd->domain->wt && (smd->domain->viewsettings & MOD_SMOKE_VIEW_SHOWBIG)) {
				smd->domain->tex = NULL;
				GPU_create_smoke(smd, 1);
				draw_volume(ar, smd->domain->tex,
				            smd->domain->p0, smd->domain->p1,
				            smd->domain->res_wt, smd->domain->dx_wt,
				            smd->domain->tex_shadow);
				GPU_free_smoke(smd);
			}
		}
	}

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
		bConstraint *con;

		for (con=ob->constraints.first; con; con= con->next) {
			if (con->type==CONSTRAINT_TYPE_RIGIDBODYJOINT) {
				bRigidBodyJointConstraint *data = (bRigidBodyJointConstraint*)con->data;
				if (data->flag&CONSTRAINT_DRAW_PIVOT)
					drawRBpivot(data);
			}
		}

		if (ob->gameflag & OB_BOUNDS) {
			if (ob->boundtype!=ob->collision_boundtype || (dtx & OB_BOUNDBOX)==0) {
				setlinestyle(2);
				draw_bounding_volume(scene, ob, ob->collision_boundtype);
				setlinestyle(0);
			}
		}

		/* draw extra: after normal draw because of makeDispList */
		if (dtx && (G.f & G_RENDER_OGL)==0) {

			if (dtx & OB_AXIS) {
				drawaxes(1.0f, OB_ARROWS);
			}
			if (dtx & OB_BOUNDBOX) {
				draw_bounding_volume(scene, ob, ob->boundtype);
			}
			if (dtx & OB_TEXSPACE) {
				drawtexspace(ob);
			}
			if (dtx & OB_DRAWNAME) {
				/* patch for several 3d cards (IBM mostly) that crash on glSelect with text drawing */
				/* but, we also don't draw names for sets or duplicators */
				if (flag == 0) {
					float zero[3]= {0,0,0};
					float curcol[4];
					unsigned char tcol[4];
					glGetFloatv(GL_CURRENT_COLOR, curcol);
					rgb_float_to_uchar(tcol, curcol);
					tcol[3]= 255;
					view3d_cached_text_draw_add(zero, ob->id.name+2, 10, 0, tcol);
				}
			}
			/*if (dtx & OB_DRAWIMAGE) drawDispListwire(&ob->disp);*/
			if ((dtx & OB_DRAWWIRE) && dt>=OB_SOLID) {
				drawWireExtra(scene, rv3d, ob);
			}
		}
	}

	if (dt<=OB_SOLID && (v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
		if ((ob->gameflag & OB_DYNAMIC) ||
		    ((ob->gameflag & OB_BOUNDS) && (ob->boundtype == OB_BOUND_SPHERE)))
		{
			float imat[4][4], vec[3]= {0.0f, 0.0f, 0.0f};

			invert_m4_m4(imat, rv3d->viewmatob);

			setlinestyle(2);
			drawcircball(GL_LINE_LOOP, vec, ob->inertia, imat);
			setlinestyle(0);
		}
	}
	
	/* return warning, this is cached text draw */
	invert_m4_m4(ob->imat, ob->obmat);
	view3d_cached_text_draw_end(v3d, ar, 1, NULL);

	glLoadMatrixf(rv3d->viewmat);

	if (zbufoff) {
		glDisable(GL_DEPTH_TEST);
	}

	if ((warning_recursive) ||
	    (base->flag & OB_FROMDUPLI) ||
	    (v3d->flag2 & V3D_RENDER_OVERRIDE))
	{
		return;
	}

	/* object centers, need to be drawn in viewmat space for speed, but OK for picking select */
	if (!is_obact || !(ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT))) {
		int do_draw_center= -1;	/* defines below are zero or positive... */

		if (v3d->flag2 & V3D_RENDER_OVERRIDE) {
			/* don't draw */
		}
		else if ((scene->basact)==base)
			do_draw_center= ACTIVE;
		else if (base->flag & SELECT)
			do_draw_center= SELECT;
		else if (empty_object || (v3d->flag & V3D_DRAW_CENTERS))
			do_draw_center= DESELECT;

		if (do_draw_center != -1) {
			if (flag & DRAW_PICKING) {
				/* draw a single point for opengl selection */
				glBegin(GL_POINTS);
				glVertex3fv(ob->obmat[3]);
				glEnd();
			}
			else if ((flag & DRAW_CONSTCOLOR)==0) {
				/* we don't draw centers for duplicators and sets */
				if (U.obcenter_dia > 0) {
					/* check > 0 otherwise grease pencil can draw into the circle select which is annoying. */
					drawcentercircle(v3d, rv3d, ob->obmat[3], do_draw_center, ob->id.lib || ob->id.us>1);
				}
			}
		}
	}

	/* not for sets, duplicators or picking */
	if (flag==0 && (v3d->flag & V3D_HIDE_HELPLINES)== 0 && (v3d->flag2 & V3D_RENDER_OVERRIDE)== 0) {
		ListBase *list;
		
		/* draw hook center and offset line */
		if (ob!=scene->obedit) draw_hooks(ob);
		
		/* help lines and so */
		if (ob!=scene->obedit && ob->parent && (ob->parent->lay & v3d->lay)) {
			setlinestyle(3);
			glBegin(GL_LINES);
			glVertex3fv(ob->obmat[3]);
			glVertex3fv(ob->orig);
			glEnd();
			setlinestyle(0);
		}

		/* Drawing the constraint lines */
		if (ob->constraints.first) {
			bConstraint *curcon;
			bConstraintOb *cob;
			unsigned char col1[4], col2[4];
			
			list = &ob->constraints;
			
			UI_GetThemeColor3ubv(TH_GRID, col1);
			UI_make_axis_color(col1, col2, 'Z');
			glColor3ubv(col2);
			
			cob= constraints_make_evalob(scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
			
			for (curcon = list->first; curcon; curcon=curcon->next) {
				bConstraintTypeInfo *cti= constraint_get_typeinfo(curcon);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				if (ELEM(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_OBJECTSOLVER)) {
					/* special case for object solver and follow track constraints because they don't fill
					 * constraint targets properly (design limitation -- scene is needed for their target
					 * but it can't be accessed from get_targets callvack) */

					Object *camob= NULL;

					if (cti->type==CONSTRAINT_TYPE_FOLLOWTRACK) {
						bFollowTrackConstraint *data= (bFollowTrackConstraint *)curcon->data;

						camob= data->camera ? data->camera : scene->camera;
					}
					else if (cti->type==CONSTRAINT_TYPE_OBJECTSOLVER) {
						bObjectSolverConstraint *data= (bObjectSolverConstraint *)curcon->data;

						camob= data->camera ? data->camera : scene->camera;
					}

					if (camob) {
						setlinestyle(3);
						glBegin(GL_LINES);
						glVertex3fv(camob->obmat[3]);
						glVertex3fv(ob->obmat[3]);
						glEnd();
						setlinestyle(0);
					}
				}
				else if ((curcon->flag & CONSTRAINT_EXPAND) && (cti) && (cti->get_constraint_targets)) {
					cti->get_constraint_targets(curcon, &targets);
					
					for (ct= targets.first; ct; ct= ct->next) {
						/* calculate target's matrix */
						if (cti->get_target_matrix)
							cti->get_target_matrix(curcon, cob, ct, BKE_curframe(scene));
						else
							unit_m4(ct->matrix);
						
						setlinestyle(3);
						glBegin(GL_LINES);
						glVertex3fv(ct->matrix[3]);
						glVertex3fv(ob->obmat[3]);
						glEnd();
						setlinestyle(0);
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(curcon, &targets, 1);
				}
			}
			
			constraints_clear_evalob(cob);
		}
	}

	free_old_images();
}

/* ***************** BACKBUF SEL (BBS) ********* */

static void bbs_obmode_mesh_verts__mapFunc(void *userData, int index, float *co, float *UNUSED(no_f), short *UNUSED(no_s))
{
	bbsObmodeMeshVerts_userData *data = userData;
	MVert *mv = &data->mvert[index];
	int offset = (intptr_t) data->offset;

	if (!(mv->flag & ME_HIDE)) {
		WM_set_framebuffer_index_color(offset+index);
		bglVertex3fv(co);
	}
}

static void bbs_obmode_mesh_verts(Object *ob, DerivedMesh *dm, int offset)
{
	bbsObmodeMeshVerts_userData data;
	Mesh *me = ob->data;
	MVert *mvert = me->mvert;
	data.mvert = mvert;
	data.offset = (void*)(intptr_t) offset;
	glPointSize( UI_GetThemeValuef(TH_VERTEX_SIZE) );
	bglBegin(GL_POINTS);
	dm->foreachMappedVert(dm, bbs_obmode_mesh_verts__mapFunc, &data);
	bglEnd();
	glPointSize(1.0);
}

static void bbs_mesh_verts__mapFunc(void *userData, int index, float *co, float *UNUSED(no_f), short *UNUSED(no_s))
{
	void **ptrs = userData;
	int offset = (intptr_t) ptrs[0];
	BMVert *eve = EDBM_get_vert_for_index(ptrs[1], index);

	if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
		WM_set_framebuffer_index_color(offset+index);
		bglVertex3fv(co);
	}
}
static void bbs_mesh_verts(BMEditMesh *em, DerivedMesh *dm, int offset)
{
	void *ptrs[2] = {(void*)(intptr_t) offset, em};

	glPointSize( UI_GetThemeValuef(TH_VERTEX_SIZE) );
	bglBegin(GL_POINTS);
	dm->foreachMappedVert(dm, bbs_mesh_verts__mapFunc, ptrs);
	bglEnd();
	glPointSize(1.0);
}		

static DMDrawOption bbs_mesh_wire__setDrawOptions(void *userData, int index)
{
	void **ptrs = userData;
	int offset = (intptr_t) ptrs[0];
	BMEdge *eed = EDBM_get_edge_for_index(ptrs[1], index);

	if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
		WM_set_framebuffer_index_color(offset+index);
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}
static void bbs_mesh_wire(BMEditMesh *em, DerivedMesh *dm, int offset)
{
	void *ptrs[2] = {(void*)(intptr_t) offset, em};
	dm->drawMappedEdges(dm, bbs_mesh_wire__setDrawOptions, ptrs);
}		

static DMDrawOption bbs_mesh_solid__setSolidDrawOptions(void *userData, int index)
{
	BMFace *efa = EDBM_get_face_for_index(((void**)userData)[0], index);
	
	if (efa && !BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		if (((void**)userData)[1]) {
			WM_set_framebuffer_index_color(index+1);
		}
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

static void bbs_mesh_solid__drawCenter(void *userData, int index, float *cent, float *UNUSED(no))
{
	BMFace *efa = EDBM_get_face_for_index(((void**)userData)[0], index);

	if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		WM_set_framebuffer_index_color(index+1);

		bglVertex3fv(cent);
	}
}

/* two options, facecolors or black */
static void bbs_mesh_solid_EM(BMEditMesh *em, Scene *scene, View3D *v3d,
                              Object *ob, DerivedMesh *dm, int facecol)
{
	void *ptrs[2] = {em, NULL}; //second one being null means to draw black
	cpack(0);

	if (facecol) {
		ptrs[1] = (void*)(intptr_t) 1;
		dm->drawMappedFaces(dm, bbs_mesh_solid__setSolidDrawOptions, GPU_enable_material, NULL, ptrs, 0);

		if (check_ob_drawface_dot(scene, v3d, ob->dt)) {
			glPointSize(UI_GetThemeValuef(TH_FACEDOT_SIZE));

			bglBegin(GL_POINTS);
			dm->foreachMappedFaceCenter(dm, bbs_mesh_solid__drawCenter, ptrs);
			bglEnd();
		}

	}
	else {
		dm->drawMappedFaces(dm, bbs_mesh_solid__setSolidDrawOptions, GPU_enable_material, NULL, ptrs, 0);
	}
}

static DMDrawOption bbs_mesh_solid__setDrawOpts(void *UNUSED(userData), int index)
{
	WM_set_framebuffer_index_color(index+1);
	return DM_DRAW_OPTION_NORMAL;
}

static DMDrawOption bbs_mesh_solid_hide__setDrawOpts(void *userData, int index)
{
	Mesh *me = userData;

	if (!(me->mpoly[index].flag&ME_HIDE)) {
		WM_set_framebuffer_index_color(index+1);
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}

// must have called WM_set_framebuffer_index_color beforehand
static DMDrawOption bbs_mesh_solid_hide2__setDrawOpts(void *userData, int index)
{
	Mesh *me = userData;

	if (!(me->mpoly[index].flag & ME_HIDE)) {
		return DM_DRAW_OPTION_NORMAL;
	}
	else {
		return DM_DRAW_OPTION_SKIP;
	}
}
static void bbs_mesh_solid(Scene *scene, Object *ob)
{
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, scene->customdata_mask);
	Mesh *me = (Mesh*)ob->data;
	
	glColor3ub(0, 0, 0);

	if ((me->editflag & ME_EDIT_PAINT_MASK))
		dm->drawMappedFaces(dm, bbs_mesh_solid_hide__setDrawOpts, GPU_enable_material, NULL, me, 0);
	else
		dm->drawMappedFaces(dm, bbs_mesh_solid__setDrawOpts, GPU_enable_material, NULL, me, 0);

	dm->release(dm);
}

void draw_object_backbufsel(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob)
{
	ToolSettings *ts= scene->toolsettings;

	glMultMatrixf(ob->obmat);

	glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	switch( ob->type) {
		case OB_MESH:
			if (ob->mode & OB_MODE_EDIT) {
				Mesh *me= ob->data;
				BMEditMesh *em= me->edit_btmesh;

				DerivedMesh *dm = editbmesh_get_derived_cage(scene, ob, em, CD_MASK_BAREMESH);

				EDBM_init_index_arrays(em, 1, 1, 1);

				bbs_mesh_solid_EM(em, scene, v3d, ob, dm, ts->selectmode & SCE_SELECT_FACE);
				if (ts->selectmode & SCE_SELECT_FACE)
					bm_solidoffs = 1+em->bm->totface;
				else
					bm_solidoffs= 1;

				bglPolygonOffset(rv3d->dist, 1.0);

				// we draw edges always, for loop (select) tools
				bbs_mesh_wire(em, dm, bm_solidoffs);
				bm_wireoffs= bm_solidoffs + em->bm->totedge;

				// we draw verts if vert select mode or if in transform (for snap).
				if ((ts->selectmode & SCE_SELECT_VERTEX) || (G.moving & G_TRANSFORM_EDIT)) {
					bbs_mesh_verts(em, dm, bm_wireoffs);
					bm_vertoffs= bm_wireoffs + em->bm->totvert;
				}
				else bm_vertoffs= bm_wireoffs;

				bglPolygonOffset(rv3d->dist, 0.0);

				dm->release(dm);

				EDBM_free_index_arrays(em);
			}
			else {
				Mesh *me= ob->data;
				if (     (me->editflag & ME_EDIT_VERT_SEL) &&
				         /* currently vertex select only supports weight paint */
				         (ob->mode & OB_MODE_WEIGHT_PAINT))
				{
					DerivedMesh *dm = mesh_get_derived_final(scene, ob, scene->customdata_mask);
					glColor3ub(0, 0, 0);

					dm->drawMappedFaces(dm, bbs_mesh_solid_hide2__setDrawOpts, GPU_enable_material, NULL, me, 0);


					bbs_obmode_mesh_verts(ob, dm, 1);
					bm_vertoffs = me->totvert+1;
					dm->release(dm);
				}
				else {
					bbs_mesh_solid(scene, ob);
				}
			}
			break;
		case OB_CURVE:
		case OB_SURF:
			break;
	}

	glLoadMatrixf(rv3d->viewmat);
}


/* ************* draw object instances for bones, for example ****************** */
/*               assumes all matrices/etc set OK */

/* helper function for drawing object instances - meshes */
static void draw_object_mesh_instance(Scene *scene, View3D *v3d, RegionView3D *rv3d, 
                                      Object *ob, int dt, int outline)
{
	Mesh *me= ob->data;
	DerivedMesh *dm=NULL, *edm=NULL;
	int glsl;
	
	if (ob->mode & OB_MODE_EDIT)
		edm= editbmesh_get_derived_base(ob, me->edit_btmesh);
	else
		dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);

	if (dt<=OB_WIRE) {
		if (dm)
			dm->drawEdges(dm, 1, 0);
		else if (edm)
			edm->drawEdges(edm, 1, 0);
	}
	else {
		if (outline)
			draw_mesh_object_outline(v3d, ob, dm?dm:edm);

		if (dm) {
			glsl = draw_glsl_material(scene, ob, v3d, dt);
			GPU_begin_object_materials(v3d, rv3d, scene, ob, glsl, NULL);
		}
		else {
			glEnable(GL_COLOR_MATERIAL);
			UI_ThemeColor(TH_BONE_SOLID);
			glDisable(GL_COLOR_MATERIAL);
		}
		
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);
		glEnable(GL_LIGHTING);
		
		if (dm) {
			dm->drawFacesSolid(dm, NULL, 0, GPU_enable_material);
			GPU_end_object_materials();
		}
		else if (edm)
			edm->drawMappedFaces(edm, NULL, GPU_enable_material, NULL, NULL, 0);
		
		glDisable(GL_LIGHTING);
	}

	if (edm) edm->release(edm);
	if (dm) dm->release(dm);
}

void draw_object_instance(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob, int dt, int outline)
{
	if (ob == NULL)
		return;

	switch (ob->type) {
		case OB_MESH:
			draw_object_mesh_instance(scene, v3d, rv3d, ob, dt, outline);
			break;
		case OB_EMPTY:
			if (ob->empty_drawtype == OB_EMPTY_IMAGE) {
				draw_empty_image(ob);
			}
			else {
				drawaxes(ob->empty_drawsize, ob->empty_drawtype);
			}
			break;
	}
}
