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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, full recode and added functions
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
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
#include "BKE_pointcache.h"
#include "BKE_property.h"
#include "BKE_softbody.h"
#include "BKE_smoke.h"
#include "BKE_unit.h"
#include "BKE_utildefines.h"
#include "smoke_API.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"

#include "ED_mesh.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_types.h"

#include "UI_resources.h"

#include "WM_api.h"
#include "wm_subwindow.h"
#include "BLF_api.h"

#include "view3d_intern.h"	// own include


/* this condition has been made more complex since editmode can draw textures */
#define CHECK_OB_DRAWTEXTURE(vd, dt) \
((vd->drawtype==OB_TEXTURE && dt>OB_SOLID) || \
	(vd->drawtype==OB_SOLID && vd->flag2 & V3D_SOLID_TEX))

static void draw_bounding_volume(Scene *scene, Object *ob);

static void drawcube_size(float size);
static void drawcircle_size(float size);
static void draw_empty_sphere(float size);
static void draw_empty_cone(float size);

static int check_ob_drawface_dot(Scene *sce, View3D *vd, char dt)
{
	if((sce->toolsettings->selectmode & SCE_SELECT_FACE) == 0)
		return 0;

	if(G.f & G_BACKBUFSEL)
		return 0;

	if((vd->flag & V3D_ZBUF_SELECT) == 0)
		return 1;

	/* if its drawing textures with zbuf sel, then dont draw dots */
	if(dt==OB_TEXTURE && vd->drawtype==OB_TEXTURE)
		return 0;

	if(vd->drawtype>=OB_SOLID && vd->flag2 & V3D_SOLID_TEX)
		return 0;

	return 1;
}

/* ************* only use while object drawing **************
 * or after running ED_view3d_init_mats_rv3d
 * */
static void view3d_project_short_clip(ARegion *ar, float *vec, short *adr, int local)
{
	RegionView3D *rv3d= ar->regiondata;
	float fx, fy, vec4[4];
	
	adr[0]= IS_CLIPPED;
	
	/* clipplanes in eye space */
	if(rv3d->rflag & RV3D_CLIPPING) {
		if(view3d_test_clipping(rv3d, vec, local))
			return;
	}
	
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	mul_m4_v4(rv3d->persmatob, vec4);
	
	/* clipplanes in window space */
	if( vec4[3]>BL_NEAR_CLIP ) {	/* is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>0 && fx<ar->winx) {
			
			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>0.0 && fy< (float)ar->winy) {
				adr[0]= (short)floor(fx); 
				adr[1]= (short)floor(fy);
			}
		}
	}
}

/* only use while object drawing */
static void view3d_project_short_noclip(ARegion *ar, float *vec, short *adr)
{
	RegionView3D *rv3d= ar->regiondata;
	float fx, fy, vec4[4];
	
	adr[0]= IS_CLIPPED;
	
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	mul_m4_v4(rv3d->persmatob, vec4);
	
	if( vec4[3]>BL_NEAR_CLIP ) {	/* is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>-32700 && fx<32700) {
			
			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>-32700.0 && fy<32700.0) {
				adr[0]= (short)floor(fx); 
				adr[1]= (short)floor(fy);
			}
		}
	}
}

/* ************************ */

/* check for glsl drawing */

int draw_glsl_material(Scene *scene, Object *ob, View3D *v3d, int dt)
{
	if(!GPU_glsl_support())
		return 0;
	if(G.f & G_PICKSEL)
		return 0;
	if(!CHECK_OB_DRAWTEXTURE(v3d, dt))
		return 0;
	if(ob==OBACT && (ob && ob->mode & OB_MODE_WEIGHT_PAINT))
		return 0;
	
	return (scene->gm.matmode == GAME_MAT_GLSL) && (dt >= OB_SHADED);
}

static int check_material_alpha(Base *base, Mesh *me, int glsl)
{
	if(base->flag & OB_FROMDUPLI)
		return 0;

	if(G.f & G_PICKSEL)
		return 0;
			
	if(me->edit_mesh)
		return 0;
	
	return (glsl || (base->object->dtx & OB_DRAWTRANSP));
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

/* ----------------- OpenGL Circle Drawing - Tables for Optimised Drawing Speed ------------------ */
/* 32 values of sin function (still same result!) */
static float sinval[32] = {
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
static float cosval[32] ={
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

static void draw_xyz_wire(RegionView3D *rv3d, float mat[][4], float *c, float size, int axis)
{
	float v1[3]= {0.f, 0.f, 0.f}, v2[3] = {0.f, 0.f, 0.f};
	float imat[4][4];
	float dim;
	float dx[3], dy[3];

	/* hrms, really only works properly after glLoadMatrixf(rv3d->viewmat); */
	float pixscale= rv3d->persmat[0][3]*c[0]+ rv3d->persmat[1][3]*c[1]+ rv3d->persmat[2][3]*c[2] + rv3d->persmat[3][3];
	pixscale*= rv3d->pixsize;

	/* halfway blend between fixed size in worldspace vs viewspace -
	 * alleviates some of the weirdness due to not using viewmat for gl matrix */
	dim = (0.05*size*0.5) + (size*10.f*pixscale*0.5);

	invert_m4_m4(imat, mat);
	normalize_v3(imat[0]);
	normalize_v3(imat[1]);
	
	copy_v3_v3(dx, imat[0]);
	copy_v3_v3(dy, imat[1]);
	
	mul_v3_fl(dx, dim);
	mul_v3_fl(dy, dim);

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
			add_v3_v3v3(v1, c, dy);
			
			glVertex3fv(v1);
			
			mul_v3_fl(dx, 2.f);
			add_v3_v3(v1, dx);

			glVertex3fv(v1);
			
			mul_v3_fl(dy, 2.f);
			sub_v3_v3(v1, dx);
			sub_v3_v3(v1, dy);
			
			glVertex3fv(v1);
			
			add_v3_v3(v1, dx);
		
			glVertex3fv(v1);
			
			glEnd();
			break;
	}
	
}

/* flag is same as for draw_object */
void drawaxes(RegionView3D *rv3d, float mat[][4], float size, int flag, char drawtype)
{
	int axis;
	float v1[3]= {0.0, 0.0, 0.0};
	float v2[3]= {0.0, 0.0, 0.0};
	float v3[3]= {0.0, 0.0, 0.0};
	
	switch(drawtype) {
	
	case OB_PLAINAXES:
		for (axis=0; axis<3; axis++) {
			float v1[3]= {0.0, 0.0, 0.0};
			float v2[3]= {0.0, 0.0, 0.0};
			
			glBegin(GL_LINES);
			
			v1[axis]= size;
			v2[axis]= -size;
			glVertex3fv(v1);
			glVertex3fv(v2);
			
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
		
		v2[0]= size*0.035; v2[1] = size*0.035;
		v3[0]= size*-0.035; v3[1] = size*0.035;
		v2[2]= v3[2]= size*0.75;
		
		for (axis=0; axis<4; axis++) {
			if (axis % 2 == 1) {
				v2[0] *= -1;
				v3[1] *= -1;
			} else {
				v2[1] *= -1;
				v3[0] *= -1;
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
		for (axis=0; axis<3; axis++) {
			float v1[3]= {0.0, 0.0, 0.0};
			float v2[3]= {0.0, 0.0, 0.0};
			int arrow_axis= (axis==0)?1:0;
			
			glBegin(GL_LINES);
			
			v2[axis]= size;
			glVertex3fv(v1);
			glVertex3fv(v2);
				
			v1[axis]= size*0.85;
			v1[arrow_axis]= -size*0.08;
			glVertex3fv(v1);
			glVertex3fv(v2);
				
			v1[arrow_axis]= size*0.08;
			glVertex3fv(v1);
			glVertex3fv(v2);
			
			glEnd();
				
			v2[axis]+= size*0.125;
			
			draw_xyz_wire(rv3d, mat, v2, size, axis);
		}
		break;
	}
}

void drawcircball(int mode, float *cent, float rad, float tmat[][4])
{
	float vec[3], vx[3], vy[3];
	int a, tot=32;
	
	VECCOPY(vx, tmat[0]);
	VECCOPY(vy, tmat[1]);
	mul_v3_fl(vx, rad);
	mul_v3_fl(vy, rad);
	
	glBegin(mode);
	for(a=0; a<tot; a++) {
		vec[0]= cent[0] + *(sinval+a) * vx[0] + *(cosval+a) * vy[0];
		vec[1]= cent[1] + *(sinval+a) * vx[1] + *(cosval+a) * vy[1];
		vec[2]= cent[2] + *(sinval+a) * vx[2] + *(cosval+a) * vy[2];
		glVertex3fv(vec);
	}
	glEnd();
}

/* circle for object centers, special_color is for library or ob users */
static void drawcentercircle(View3D *v3d, RegionView3D *rv3d, float *vec, int selstate, int special_color)
{
	float size;
	
	size= rv3d->persmat[0][3]*vec[0]+ rv3d->persmat[1][3]*vec[1]+ rv3d->persmat[2][3]*vec[2]+ rv3d->persmat[3][3];
	size*= rv3d->pixsize*((float)U.obcenter_dia*0.5f);

	/* using gldepthfunc guarantees that it does write z values, but not checks for it, so centers remain visible independt order of drawing */
	if(v3d->zbuf)  glDepthFunc(GL_ALWAYS);
	glEnable(GL_BLEND);
	
	if(special_color) {
		if (selstate==ACTIVE || selstate==SELECT) glColor4ub(0x88, 0xFF, 0xFF, 155);

		else glColor4ub(0x55, 0xCC, 0xCC, 155);
	}
	else {
		if (selstate == ACTIVE) UI_ThemeColorShadeAlpha(TH_ACTIVE, 0, -80);
		else if (selstate == SELECT) UI_ThemeColorShadeAlpha(TH_SELECT, 0, -80);
		else if (selstate == DESELECT) UI_ThemeColorShadeAlpha(TH_TRANSFORM, 0, -80);
	}
	drawcircball(GL_POLYGON, vec, size, rv3d->viewinv);
	
	UI_ThemeColorShadeAlpha(TH_WIRE, 0, -30);
	drawcircball(GL_LINE_LOOP, vec, size, rv3d->viewinv);
	
	glDisable(GL_BLEND);
	if(v3d->zbuf)  glDepthFunc(GL_LEQUAL);
}

/* *********** text drawing for object/particles/armature ************* */
static ListBase CachedText[3];
static int CachedTextLevel= 0;

typedef struct ViewCachedString {
	struct ViewCachedString *next, *prev;
	float vec[3], col[4];
	char str[128]; 
	short mval[2];
	short xoffs;
	short flag;
} ViewCachedString;

void view3d_cached_text_draw_begin()
{
	ListBase *strings= &CachedText[CachedTextLevel];
	strings->first= strings->last= NULL;
	CachedTextLevel++;
}

void view3d_cached_text_draw_add(float x, float y, float z, char *str, short xoffs, short flag)
{
	ListBase *strings= &CachedText[CachedTextLevel-1];
	ViewCachedString *vos= MEM_callocN(sizeof(ViewCachedString), "ViewCachedString");

	BLI_addtail(strings, vos);
	BLI_strncpy(vos->str, str, 128);
	vos->vec[0]= x;
	vos->vec[1]= y;
	vos->vec[2]= z;
	glGetFloatv(GL_CURRENT_COLOR, vos->col);
	vos->xoffs= xoffs;
	vos->flag= flag;
}

void view3d_cached_text_draw_end(View3D *v3d, ARegion *ar, int depth_write, float mat[][4])
{
	RegionView3D *rv3d= ar->regiondata;
	ListBase *strings= &CachedText[CachedTextLevel-1];
	ViewCachedString *vos;
	int a, tot= 0;
	
	/* project first and test */
	for(vos= strings->first; vos; vos= vos->next) {
		if(mat && !(vos->flag & V3D_CACHE_TEXT_WORLDSPACE))
			mul_m4_v3(mat, vos->vec);
		view3d_project_short_clip(ar, vos->vec, vos->mval, 0);
		if(vos->mval[0]!=IS_CLIPPED)
			tot++;
	}

	if(tot) {
#if 0
		bglMats mats; /* ZBuffer depth vars */
		double ux, uy, uz;
		float depth;

		if(v3d->zbuf)
			bgl_get_mats(&mats);
#endif
		if(rv3d->rflag & RV3D_CLIPPING)
			for(a=0; a<6; a++)
				glDisable(GL_CLIP_PLANE0+a);
		
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		ED_region_pixelspace(ar);
		
		if(depth_write) {
			if(v3d->zbuf) glDisable(GL_DEPTH_TEST);
		}
		else glDepthMask(0);
		
		for(vos= strings->first; vos; vos= vos->next) {
#if 0       // too slow, reading opengl info while drawing is very bad, better to see if we cn use the zbuffer while in pixel space - campbell
			if(v3d->zbuf && (vos->flag & V3D_CACHE_TEXT_ZBUF)) {
				gluProject(vos->vec[0], vos->vec[1], vos->vec[2], mats.modelview, mats.projection, (GLint *)mats.viewport, &ux, &uy, &uz);
				glReadPixels(ar->winrct.xmin+vos->mval[0]+vos->xoffs, ar->winrct.ymin+vos->mval[1], 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

				if(uz > depth)
					continue;
			}
#endif
			if(vos->mval[0]!=IS_CLIPPED) {
				glColor3fv(vos->col);
				BLF_draw_default((float)vos->mval[0]+vos->xoffs, (float)vos->mval[1], (depth_write)? 0.0f: 2.0f, vos->str);
			}
		}
		
		if(depth_write) {
			if(v3d->zbuf) glEnable(GL_DEPTH_TEST);
		}
		else glDepthMask(1);
		
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();

		if(rv3d->rflag & RV3D_CLIPPING)
			for(a=0; a<6; a++)
				glEnable(GL_CLIP_PLANE0+a);
	}
	
	if(strings->first) 
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
		glVertex3f(-size,-size,-size); glVertex3f(-size,-size,size);glVertex3f(-size,size,size); glVertex3f(-size,size,-size);
		glVertex3f(-size,-size,-size); glVertex3f(size,-size,-size);glVertex3f(size,-size,size); glVertex3f(size,size,size);
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
static void drawcube_size(float *size)
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

	lavec[0]= -mat[2][0];
	lavec[1]= -mat[2][1];
	lavec[2]= -mat[2][2];
	normalize_v3(lavec);

	sta[0]= mat[3][0]+ la->clipsta*lavec[0];
	sta[1]= mat[3][1]+ la->clipsta*lavec[1];
	sta[2]= mat[3][2]+ la->clipsta*lavec[2];

	end[0]= mat[3][0]+ la->clipend*lavec[0];
	end[1]= mat[3][1]+ la->clipend*lavec[1];
	end[2]= mat[3][2]+ la->clipend*lavec[2];


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



static void spotvolume(float *lvec, float *vvec, float inp)
{
	/* camera is at 0,0,0 */
	float temp[3],plane[3],mat1[3][3],mat2[3][3],mat3[3][3],mat4[3][3],q[4],co,si,angle;

	normalize_v3(lvec);
	normalize_v3(vvec);				/* is this the correct vector ? */

	cross_v3_v3v3(temp,vvec,lvec);		/* equation for a plane through vvec en lvec */
	cross_v3_v3v3(plane,lvec,temp);		/* a plane perpendicular to this, parrallel with lvec */

	normalize_v3(plane);

	/* now we've got two equations: one of a cone and one of a plane, but we have
	three unknowns. We remove one unkown by rotating the plane to z=0 (the plane normal) */

	/* rotate around cross product vector of (0,0,1) and plane normal, dot product degrees */
	/* according definition, we derive cross product is (plane[1],-plane[0],0), en cos = plane[2]);*/

	/* translating this comment to english didnt really help me understanding the math! :-) (ton) */
	
	q[1] = plane[1] ; 
	q[2] = -plane[0] ; 
	q[3] = 0 ;
	normalize_v3(&q[1]);

	angle = saacos(plane[2])/2.0;
	co = cos(angle);
	si = sqrt(1-co*co);

	q[0] =  co;
	q[1] *= si;
	q[2] *= si;
	q[3] =  0;

	quat_to_mat3(mat1,q);

	/* rotate lamp vector now over acos(inp) degrees */

	vvec[0] = lvec[0] ; 
	vvec[1] = lvec[1] ; 
	vvec[2] = lvec[2] ;

	unit_m3(mat2);
	co = inp;
	si = sqrt(1-inp*inp);

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
	float vec[3];

	z= fabs(z);

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0f, 0.0f, -x);

	if(la->mode & LA_SQUARE) {
		vec[0]= z;
		vec[1]= z;
		vec[2]= 0.0;

		glVertex3fv(vec);
		vec[1]= -z;
		glVertex3fv(vec);
		vec[0]= -z;
		glVertex3fv(vec);
		vec[1]= z;
		glVertex3fv(vec);
	}
	else {
		float angle;
		int a;

		for(a=0; a<33; a++) {
			angle= a*M_PI*2/(33-1);
			glVertex3f(z*cos(angle), z*sin(angle), 0);
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

	/* draw front side lightening */
	glCullFace(GL_BACK);

	glBlendFunc(GL_ONE,  GL_ONE); 
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
	Lamp *la= ob->data;
	float vec[3], lvec[3], vvec[3], circrad, x,y,z;
	float pixsize, lampsize;
	float imat[4][4], curcol[4];
	char col[4];
	int drawcone= (dt>OB_WIRE && !(G.f & G_PICKSEL) && la->type == LA_SPOT && (la->mode & LA_SHOW_CONE));
	
	if(drawcone && !v3d->transp) {
		/* in this case we need to draw delayed */
		add_view3d_after(v3d, base, V3D_TRANSP, flag);
		return;
	}
	
	/* we first draw only the screen aligned & fixed scale stuff */
	glPushMatrix();
	glLoadMatrixf(rv3d->viewmat);

	/* lets calculate the scale: */
	pixsize= rv3d->persmat[0][3]*ob->obmat[3][0]+ rv3d->persmat[1][3]*ob->obmat[3][1]+ rv3d->persmat[2][3]*ob->obmat[3][2]+ rv3d->persmat[3][3];
	pixsize*= rv3d->pixsize;
	lampsize= pixsize*((float)U.obcenter_dia*0.5f);

	/* and view aligned matrix: */
	copy_m4_m4(imat, rv3d->viewinv);
	normalize_v3(imat[0]);
	normalize_v3(imat[1]);
	
	/* for AA effects */
	glGetFloatv(GL_CURRENT_COLOR, curcol);
	curcol[3]= 0.6;
	glColor4fv(curcol);
	
	if(lampsize > 0.0f) {

		if(ob->id.us>1) {
			if (ob==OBACT || (ob->flag & SELECT)) glColor4ub(0x88, 0xFF, 0xFF, 155);
			else glColor4ub(0x77, 0xCC, 0xCC, 155);
		}
		
		/* Inner Circle */
		VECCOPY(vec, ob->obmat[3]);
		glEnable(GL_BLEND);
		drawcircball(GL_LINE_LOOP, vec, lampsize, imat);
		glDisable(GL_BLEND);
		drawcircball(GL_POLYGON, vec, lampsize, imat);
		
		/* restore */
		if(ob->id.us>1)
			glColor4fv(curcol);
			
		/* Outer circle */
		circrad = 3.0f*lampsize;
		setlinestyle(3);

		drawcircball(GL_LINE_LOOP, vec, circrad, imat);

		/* draw dashed outer circle if shadow is on. remember some lamps can't have certain shadows! */
		if(la->type!=LA_HEMI) {
			if(	(la->mode & LA_SHAD_RAY) ||
				((la->mode & LA_SHAD_BUF) && (la->type==LA_SPOT))
			) {
				drawcircball(GL_LINE_LOOP, vec, circrad + 3.0f*pixsize, imat);
			}
		}
	}
	else {
		setlinestyle(3);
		circrad = 0.0f;
	}
	
	/* draw the pretty sun rays */
	if(la->type==LA_SUN) {
		float v1[3], v2[3], mat[3][3];
		short axis;
		
		/* setup a 45 degree rotation matrix */
		vec_rot_to_mat3( mat,imat[2], M_PI/4.0f);
		
		/* vectors */
		VECCOPY(v1, imat[0]);
		mul_v3_fl(v1, circrad*1.2f);
		VECCOPY(v2, imat[0]);
		mul_v3_fl(v2, circrad*2.5f);
		
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
		if(la->mode & LA_SPHERE) {
			drawcircball(GL_LINE_LOOP, vec, la->dist, imat);
		}
		/* yafray: for photonlight also draw lightcone as for spot */
	}
	
	glPopMatrix();	/* back in object space */
	vec[0]= vec[1]= vec[2]= 0.0f;
	
	if ((la->type==LA_SPOT) || (la->type==LA_YF_PHOTON)) {	
		lvec[0]=lvec[1]= 0.0; 
		lvec[2] = 1.0;
		x = rv3d->persmat[0][2];
		y = rv3d->persmat[1][2];
		z = rv3d->persmat[2][2];
		vvec[0]= x*ob->obmat[0][0] + y*ob->obmat[0][1] + z*ob->obmat[0][2];
		vvec[1]= x*ob->obmat[1][0] + y*ob->obmat[1][1] + z*ob->obmat[1][2];
		vvec[2]= x*ob->obmat[2][0] + y*ob->obmat[2][1] + z*ob->obmat[2][2];

		y = cos( M_PI*la->spotsize/360.0 );
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
		
		z = x*sqrt(1.0 - y*y);
		x *= y;

		/* draw the circle/square at the end of the cone */
		glTranslatef(0.0, 0.0 ,  x);
		if(la->mode & LA_SQUARE) {
			vvec[0]= fabs(z);
			vvec[1]= fabs(z);
			vvec[2]= 0.0;
			glBegin(GL_LINE_LOOP);
				glVertex3fv(vvec);
				vvec[1]= -fabs(z);
				glVertex3fv(vvec);
				vvec[0]= -fabs(z);
				glVertex3fv(vvec);
				vvec[1]= fabs(z);
				glVertex3fv(vvec);
			glEnd();
		}
		else circ(0.0, 0.0, fabs(z));
		
		/* draw the circle/square representing spotbl */
		if(la->type==LA_SPOT) {
			float spotblcirc = fabs(z)*(1 - pow(la->spotblend, 2));
			/* hide line if it is zero size or overlaps with outer border,
			   previously it adjusted to always to show it but that seems
			   confusing because it doesn't show the actual blend size */
			if (spotblcirc != 0 && spotblcirc != fabs(z))
				circ(0.0, 0.0, spotblcirc);
		}

		if(drawcone)
			draw_transp_spot_volume(la, x, z);
	}
	else if ELEM(la->type, LA_HEMI, LA_SUN) {
		
		/* draw the line from the circle along the dist */
		glBegin(GL_LINE_STRIP);
			vec[2] = -circrad;
			glVertex3fv(vec); 
			vec[2]= -la->dist; 
			glVertex3fv(vec);
		glEnd();
		
		if(la->type==LA_HEMI) {
			/* draw the hemisphere curves */
			short axis, steps, dir;
			float outdist, zdist, mul;
			vec[0]=vec[1]=vec[2]= 0.0;
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
					} else if (axis == 2 || axis == 3) { 		/* y axis up, y axis down */
						/* make the arcs start at the edge of the energy circle */
						if (steps == 0) v[1] = dir*circrad;
						else v[1] = v[1] + dir*(steps*outdist); 
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
	} else if(la->type==LA_AREA) {
		setlinestyle(3);
		if(la->area_shape==LA_AREA_SQUARE) 
			fdrawbox(-la->area_size*0.5, -la->area_size*0.5, la->area_size*0.5, la->area_size*0.5);
		else if(la->area_shape==LA_AREA_RECT) 
			fdrawbox(-la->area_size*0.5, -la->area_sizey*0.5, la->area_size*0.5, la->area_sizey*0.5);

		glBegin(GL_LINE_STRIP); 
		glVertex3f(0.0,0.0,-circrad);
		glVertex3f(0.0,0.0,-la->dist);
		glEnd();
	}
	
	/* and back to viewspace */
	glLoadMatrixf(rv3d->viewmat);
	VECCOPY(vec, ob->obmat[3]);

	setlinestyle(0);
	
	if(la->type==LA_SPOT && (la->mode & LA_SHAD_BUF) ) {
		drawshadbuflimits(la, ob->obmat);
	}
	
	UI_GetThemeColor4ubv(TH_LAMP, col);
	glColor4ub(col[0], col[1], col[2], col[3]);
	 
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
/* qdn: now also enabled for Blender to set focus point for defocus composit node */
static void draw_focus_cross(float dist, float size)
{
	glBegin(GL_LINES);
	glVertex3f(-size, 0.f, -dist);
	glVertex3f(size, 0.f, -dist);
	glVertex3f(0.f, -size, -dist);
	glVertex3f(0.f, size, -dist);
	glEnd();
}

/* flag similar to draw_object() */
static void drawcamera(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob, int flag)
{
	/* a standing up pyramid with (0,0,0) as top */
	Camera *cam;
	World *wrld;
	float nobmat[4][4], vec[8][4], fac, facx, facy, depth;
	int i;

	cam= ob->data;
	
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	
	if(rv3d->persp>=2 && cam->type==CAM_ORTHO && ob==v3d->camera) {
		facx= 0.5*cam->ortho_scale*1.28;
		facy= 0.5*cam->ortho_scale*1.024;
		depth= -cam->clipsta-0.1;
	}
	else {
		fac= cam->drawsize;
		if(rv3d->persp>=2 && ob==v3d->camera) fac= cam->clipsta+0.1; /* that way it's always visible */
		
		depth= - fac*cam->lens/16.0;
		facx= fac*1.28;
		facy= fac*1.024;
	}
	
	vec[0][0]= 0.0; vec[0][1]= 0.0; vec[0][2]= 0.001;	/* GLBUG: for picking at iris Entry (well thats old!) */
	vec[1][0]= facx; vec[1][1]= facy; vec[1][2]= depth;
	vec[2][0]= facx; vec[2][1]= -facy; vec[2][2]= depth;
	vec[3][0]= -facx; vec[3][1]= -facy; vec[3][2]= depth;
	vec[4][0]= -facx; vec[4][1]= facy; vec[4][2]= depth;

	glBegin(GL_LINE_LOOP);
		glVertex3fv(vec[1]); 
		glVertex3fv(vec[2]); 
		glVertex3fv(vec[3]); 
		glVertex3fv(vec[4]);
	glEnd();
	

	if(rv3d->persp>=2 && ob==v3d->camera) return;
	
	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec[2]); 
		glVertex3fv(vec[0]);
		glVertex3fv(vec[1]);
		glVertex3fv(vec[4]);
		glVertex3fv(vec[0]);
		glVertex3fv(vec[3]); 
	glEnd();


	/* arrow on top */
	vec[0][2]= depth;

	
	/* draw an outline arrow for inactive cameras and filled
	 * for active cameras. We actually draw both outline+filled
	 * for active cameras so the wire can be seen side-on */	
	for (i=0;i<2;i++) {
		if (i==0) glBegin(GL_LINE_LOOP);
		else if (i==1 && (ob == v3d->camera)) glBegin(GL_TRIANGLES);
		else break;
		
		vec[0][0]= -0.7*cam->drawsize;
		vec[0][1]= 1.1*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.0; 
		vec[0][1]= 1.8*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.7*cam->drawsize; 
		vec[0][1]= 1.1*cam->drawsize;
		glVertex3fv(vec[0]);
	
		glEnd();
	}

	if(flag==0) {
		if(cam->flag & (CAM_SHOWLIMITS+CAM_SHOWMIST)) {
			/* draw in normalized object matrix space */
			copy_m4_m4(nobmat, ob->obmat);
			normalize_m4(nobmat);

			glPushMatrix();
			glLoadMatrixf(rv3d->viewmat);
			glMultMatrixf(nobmat);

			if(cam->flag & CAM_SHOWLIMITS) {
				draw_limit_line(cam->clipsta, cam->clipend, 0x77FFFF);
				/* qdn: was yafray only, now also enabled for Blender to be used with defocus composit node */
				draw_focus_cross(dof_camera(ob), cam->drawsize);
			}

			wrld= scene->world;
			if(cam->flag & CAM_SHOWMIST) 
				if(wrld) draw_limit_line(wrld->miststa, wrld->miststa+wrld->mistdist, 0xFFFFFF);
				
			glPopMatrix();
		}
	}
}

static void lattice_draw_verts(Lattice *lt, DispList *dl, short sel)
{
	BPoint *bp = lt->def;
	float *co = dl?dl->verts:NULL;
	int u, v, w;

	UI_ThemeColor(sel?TH_VERTEX_SELECT:TH_VERTEX);
	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));
	bglBegin(GL_POINTS);

	for(w=0; w<lt->pntsw; w++) {
		int wxt = (w==0 || w==lt->pntsw-1);
		for(v=0; v<lt->pntsv; v++) {
			int vxt = (v==0 || v==lt->pntsv-1);
			for(u=0; u<lt->pntsu; u++, bp++, co+=3) {
				int uxt = (u==0 || u==lt->pntsu-1);
				if(!(lt->flag & LT_OUTSIDE) || uxt || vxt || wxt) {
					if(bp->hide==0) {
						if((bp->f1 & SELECT)==sel) {
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
	BPoint *bp = lt->editlatt->def;
	DispList *dl = find_displist(&obedit->disp, DL_VERTS);
	float *co = dl?dl->verts:NULL;
	int i, N = lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
	short s[2] = {IS_CLIPPED, 0};

	ED_view3d_local_clipping(vc->rv3d, obedit->obmat); /* for local clipping lookups */

	for (i=0; i<N; i++, bp++, co+=3) {
		if (bp->hide==0) {
			view3d_project_short_clip(vc->ar, dl?co:bp->vec, s, 1);
			if (s[0] != IS_CLIPPED)
				func(userData, bp, s[0], s[1]);
		}
	}
}

static void drawlattice__point(Lattice *lt, DispList *dl, int u, int v, int w, int use_wcol)
{
	int index = ((w*lt->pntsv + v)*lt->pntsu) + u;

	if(use_wcol) {
		float col[3];
		MDeformWeight *mdw= defvert_find_index (lt->dvert+index, use_wcol-1);
		
		weight_to_rgb(mdw?mdw->weight:0.0f, col, col+1, col+2);
		glColor3fv(col);

	}
	
	if (dl) {
		glVertex3fv(&dl->verts[index*3]);
	} else {
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
	if(ob->disp.first==NULL)
		lattice_calc_modifiers(scene, ob);
	dl= find_displist(&ob->disp, DL_VERTS);
	
	if(is_edit) {
		lt= lt->editlatt;

		cpack(0x004000);
		
		if(ob->defbase.first && lt->dvert) {
			use_wcol= ob->actdef;
			glShadeModel(GL_SMOOTH);
		}
	}
	
	glBegin(GL_LINES);
	for(w=0; w<lt->pntsw; w++) {
		int wxt = (w==0 || w==lt->pntsw-1);
		for(v=0; v<lt->pntsv; v++) {
			int vxt = (v==0 || v==lt->pntsv-1);
			for(u=0; u<lt->pntsu; u++) {
				int uxt = (u==0 || u==lt->pntsu-1);

				if(w && ((uxt || vxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u, v, w-1, use_wcol);
					drawlattice__point(lt, dl, u, v, w, use_wcol);
				}
				if(v && ((uxt || wxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u, v-1, w, use_wcol);
					drawlattice__point(lt, dl, u, v, w, use_wcol);
				}
				if(u && ((vxt || wxt) || !(lt->flag & LT_OUTSIDE))) {
					drawlattice__point(lt, dl, u-1, v, w, use_wcol);
					drawlattice__point(lt, dl, u, v, w, use_wcol);
				}
			}
		}
	}		
	glEnd();
	
	/* restoration for weight colors */
	if(use_wcol)
		glShadeModel(GL_FLAT);

	if(is_edit) {
		if(v3d->zbuf) glDisable(GL_DEPTH_TEST);
		
		lattice_draw_verts(lt, dl, 0);
		lattice_draw_verts(lt, dl, 1);
		
		if(v3d->zbuf) glEnable(GL_DEPTH_TEST); 
	}
}

/* ***************** ******************** */

/* Note! - foreach funcs should be called while drawing or directly after
 * if not, ED_view3d_init_mats_rv3d() can be used for selection tools
 * but would not give correct results with dupli's for eg. which dont
 * use the object matrix in the useual way */
static void mesh_foreachScreenVert__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	struct { void (*func)(void *userData, EditVert *eve, int x, int y, int index); void *userData; ViewContext vc; int clipVerts; } *data = userData;
	EditVert *eve = EM_get_vert_for_index(index);

	if (eve->h==0) {
		short s[2]= {IS_CLIPPED, 0};

		if (data->clipVerts) {
			view3d_project_short_clip(data->vc.ar, co, s, 1);
		} else {
			view3d_project_short_noclip(data->vc.ar, co, s);
		}

		if (s[0]!=IS_CLIPPED)
			data->func(data->userData, eve, s[0], s[1], index);
	}
}

void mesh_foreachScreenVert(ViewContext *vc, void (*func)(void *userData, EditVert *eve, int x, int y, int index), void *userData, int clipVerts)
{
	struct { void (*func)(void *userData, EditVert *eve, int x, int y, int index); void *userData; ViewContext vc; int clipVerts; } data;
	DerivedMesh *dm = editmesh_get_derived_cage(vc->scene, vc->obedit, vc->em, CD_MASK_BAREMESH);
	
	data.vc= *vc;
	data.func = func;
	data.userData = userData;
	data.clipVerts = clipVerts;

	if(clipVerts)
		ED_view3d_local_clipping(vc->rv3d, vc->obedit->obmat); /* for local clipping lookups */

	EM_init_index_arrays(vc->em, 1, 0, 0);
	dm->foreachMappedVert(dm, mesh_foreachScreenVert__mapFunc, &data);
	EM_free_index_arrays();

	dm->release(dm);
}

static void mesh_foreachScreenEdge__mapFunc(void *userData, int index, float *v0co, float *v1co)
{
	struct { void (*func)(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index); void *userData; ViewContext vc; int clipVerts; } *data = userData;
	EditEdge *eed = EM_get_edge_for_index(index);
	short s[2][2];

	if (eed->h==0) {
		if (data->clipVerts==1) {
			view3d_project_short_clip(data->vc.ar, v0co, s[0], 1);
			view3d_project_short_clip(data->vc.ar, v1co, s[1], 1);
		} else {
			view3d_project_short_noclip(data->vc.ar, v0co, s[0]);
			view3d_project_short_noclip(data->vc.ar, v1co, s[1]);

			if (data->clipVerts==2) {
				if (!(s[0][0]>=0 && s[0][1]>= 0 && s[0][0]<data->vc.ar->winx && s[0][1]<data->vc.ar->winy))
					if (!(s[1][0]>=0 && s[1][1]>= 0 && s[1][0]<data->vc.ar->winx && s[1][1]<data->vc.ar->winy)) 
						return;
			}
		}

		data->func(data->userData, eed, s[0][0], s[0][1], s[1][0], s[1][1], index);
	}
}

void mesh_foreachScreenEdge(ViewContext *vc, void (*func)(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index), void *userData, int clipVerts)
{
	struct { void (*func)(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index); void *userData; ViewContext vc; int clipVerts; } data;
	DerivedMesh *dm = editmesh_get_derived_cage(vc->scene, vc->obedit, vc->em, CD_MASK_BAREMESH);

	data.vc= *vc;
	data.func = func;
	data.userData = userData;
	data.clipVerts = clipVerts;

	if(clipVerts)
		ED_view3d_local_clipping(vc->rv3d, vc->obedit->obmat); /* for local clipping lookups */

	EM_init_index_arrays(vc->em, 0, 1, 0);
	dm->foreachMappedEdge(dm, mesh_foreachScreenEdge__mapFunc, &data);
	EM_free_index_arrays();

	dm->release(dm);
}

static void mesh_foreachScreenFace__mapFunc(void *userData, int index, float *cent, float *no)
{
	struct { void (*func)(void *userData, EditFace *efa, int x, int y, int index); void *userData; ViewContext vc; } *data = userData;
	EditFace *efa = EM_get_face_for_index(index);
	short s[2];

	if (efa && efa->h==0 && efa->fgonf!=EM_FGON) {
		view3d_project_short_clip(data->vc.ar, cent, s, 1);

		data->func(data->userData, efa, s[0], s[1], index);
	}
}

void mesh_foreachScreenFace(ViewContext *vc, void (*func)(void *userData, EditFace *efa, int x, int y, int index), void *userData)
{
	struct { void (*func)(void *userData, EditFace *efa, int x, int y, int index); void *userData; ViewContext vc; } data;
	DerivedMesh *dm = editmesh_get_derived_cage(vc->scene, vc->obedit, vc->em, CD_MASK_BAREMESH);

	data.vc= *vc;
	data.func = func;
	data.userData = userData;

	//if(clipVerts)
	ED_view3d_local_clipping(vc->rv3d, vc->obedit->obmat); /* for local clipping lookups */

	EM_init_index_arrays(vc->em, 0, 0, 1);
	dm->foreachMappedFaceCenter(dm, mesh_foreachScreenFace__mapFunc, &data);
	EM_free_index_arrays();

	dm->release(dm);
}

void nurbs_foreachScreenVert(ViewContext *vc, void (*func)(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y), void *userData)
{
	Curve *cu= vc->obedit->data;
	short s[2] = {IS_CLIPPED, 0};
	Nurb *nu;
	int i;

	ED_view3d_local_clipping(vc->rv3d, vc->obedit->obmat); /* for local clipping lookups */

	for (nu= cu->editnurb->first; nu; nu=nu->next) {
		if(nu->type == CU_BEZIER) {
			for (i=0; i<nu->pntsu; i++) {
				BezTriple *bezt = &nu->bezt[i];

				if(bezt->hide==0) {
					
					if(cu->drawflag & CU_HIDE_HANDLES) {
						view3d_project_short_clip(vc->ar, bezt->vec[1], s, 1);
						if (s[0] != IS_CLIPPED)
							func(userData, nu, NULL, bezt, 1, s[0], s[1]);
					} else {
						view3d_project_short_clip(vc->ar, bezt->vec[0], s, 1);
						if (s[0] != IS_CLIPPED)
							func(userData, nu, NULL, bezt, 0, s[0], s[1]);
						view3d_project_short_clip(vc->ar, bezt->vec[1], s, 1);
						if (s[0] != IS_CLIPPED)
							func(userData, nu, NULL, bezt, 1, s[0], s[1]);
						view3d_project_short_clip(vc->ar, bezt->vec[2], s, 1);
						if (s[0] != IS_CLIPPED)
							func(userData, nu, NULL, bezt, 2, s[0], s[1]);
					}
				}
			}
		}
		else {
			for (i=0; i<nu->pntsu*nu->pntsv; i++) {
				BPoint *bp = &nu->bp[i];

				if(bp->hide==0) {
					view3d_project_short_clip(vc->ar, bp->vec, s, 1);
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
	ToolSettings *ts= ((Scene *)userData)->toolsettings;
	EditFace *efa = EM_get_face_for_index(index);

	if (efa->h==0 && efa->fgonf!=EM_FGON) {
		glVertex3fv(cent);
		glVertex3f(	cent[0] + no[0]*ts->normalsize,
					cent[1] + no[1]*ts->normalsize,
					cent[2] + no[2]*ts->normalsize);
	}
}
static void draw_dm_face_normals(Scene *scene, DerivedMesh *dm) 
{
	glBegin(GL_LINES);
	dm->foreachMappedFaceCenter(dm, draw_dm_face_normals__mapFunc, scene);
	glEnd();
}

static void draw_dm_face_centers__mapFunc(void *userData, int index, float *cent, float *no)
{
	EditFace *efa = EM_get_face_for_index(index);
	int sel = *((int*) userData);

	if (efa->h==0 && efa->fgonf!=EM_FGON && (efa->f&SELECT)==sel) {
		bglVertex3fv(cent);
	}
}
static void draw_dm_face_centers(DerivedMesh *dm, int sel)
{
	bglBegin(GL_POINTS);
	dm->foreachMappedFaceCenter(dm, draw_dm_face_centers__mapFunc, &sel);
	bglEnd();
}

static void draw_dm_vert_normals__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	Scene *scene= (Scene *)userData;
	ToolSettings *ts= scene->toolsettings;
	EditVert *eve = EM_get_vert_for_index(index);

	if (eve->h==0) {
		glVertex3fv(co);

		if (no_f) {
			glVertex3f(	co[0] + no_f[0]*ts->normalsize,
						co[1] + no_f[1]*ts->normalsize,
						co[2] + no_f[2]*ts->normalsize);
		} else {
			glVertex3f(	co[0] + no_s[0]*ts->normalsize/32767.0f,
						co[1] + no_s[1]*ts->normalsize/32767.0f,
						co[2] + no_s[2]*ts->normalsize/32767.0f);
		}
	}
}
static void draw_dm_vert_normals(Scene *scene, DerivedMesh *dm) 
{
	glBegin(GL_LINES);
	dm->foreachMappedVert(dm, draw_dm_vert_normals__mapFunc, scene);
	glEnd();
}

	/* Draw verts with color set based on selection */
static void draw_dm_verts__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	struct { int sel; EditVert *eve_act; } * data = userData;
	EditVert *eve = EM_get_vert_for_index(index);

	if (eve->h==0 && (eve->f&SELECT)==data->sel) {
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
		} else {
			bglVertex3fv(co);
		}
	}
}

static void draw_dm_verts(DerivedMesh *dm, int sel, EditVert *eve_act)
{
	struct { int sel; EditVert *eve_act; } data;
	data.sel = sel;
	data.eve_act = eve_act;

	bglBegin(GL_POINTS);
	dm->foreachMappedVert(dm, draw_dm_verts__mapFunc, &data);
	bglEnd();
}

	/* Draw edges with color set based on selection */
static int draw_dm_edges_sel__setDrawOptions(void *userData, int index)
{
	EditEdge *eed = EM_get_edge_for_index(index);
	//unsigned char **cols = userData, *col;
	struct { unsigned char *baseCol, *selCol, *actCol; EditEdge *eed_act; } * data = userData;
	unsigned char *col;

	if (eed->h==0) {
		if (eed==data->eed_act) {
			glColor4ubv(data->actCol);
		} else {
			if (eed->f&SELECT) {
				col = data->selCol;
			} else {
				col = data->baseCol;
			}
			/* no alpha, this is used so a transparent color can disable drawing unselected edges in editmode  */
			if (col[3]==0) return 0;
			
			glColor4ubv(col);
		}
		return 1;
	} else {
		return 0;
	}
}
static void draw_dm_edges_sel(DerivedMesh *dm, unsigned char *baseCol, unsigned char *selCol, unsigned char *actCol, EditEdge *eed_act) 
{
	struct { unsigned char *baseCol, *selCol, *actCol; EditEdge *eed_act; } data;
	
	data.baseCol = baseCol;
	data.selCol = selCol;
	data.actCol = actCol;
	data.eed_act = eed_act;
	dm->drawMappedEdges(dm, draw_dm_edges_sel__setDrawOptions, &data);
}

	/* Draw edges */
static int draw_dm_edges__setDrawOptions(void *userData, int index)
{
	return EM_get_edge_for_index(index)->h==0;
}
static void draw_dm_edges(DerivedMesh *dm) 
{
	dm->drawMappedEdges(dm, draw_dm_edges__setDrawOptions, NULL);
}

	/* Draw edges with color interpolated based on selection */
static int draw_dm_edges_sel_interp__setDrawOptions(void *userData, int index)
{
	return EM_get_edge_for_index(index)->h==0;
}
static void draw_dm_edges_sel_interp__setDrawInterpOptions(void *userData, int index, float t)
{
	EditEdge *eed = EM_get_edge_for_index(index);
	unsigned char **cols = userData;
	unsigned char *col0 = cols[(eed->v1->f&SELECT)?1:0];
	unsigned char *col1 = cols[(eed->v2->f&SELECT)?1:0];

	glColor4ub(	col0[0] + (col1[0]-col0[0])*t,
				col0[1] + (col1[1]-col0[1])*t,
				col0[2] + (col1[2]-col0[2])*t,
				col0[3] + (col1[3]-col0[3])*t);
}

static void draw_dm_edges_sel_interp(DerivedMesh *dm, unsigned char *baseCol, unsigned char *selCol)
{
	unsigned char *cols[2] = {baseCol, selCol};

	dm->drawMappedEdgesInterp(dm, draw_dm_edges_sel_interp__setDrawOptions, draw_dm_edges_sel_interp__setDrawInterpOptions, cols);
}

	/* Draw only seam edges */
static int draw_dm_edges_seams__setDrawOptions(void *userData, int index)
{
	EditEdge *eed = EM_get_edge_for_index(index);

	return (eed->h==0 && eed->seam);
}
static void draw_dm_edges_seams(DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges_seams__setDrawOptions, NULL);
}

	/* Draw only sharp edges */
static int draw_dm_edges_sharp__setDrawOptions(void *userData, int index)
{
	EditEdge *eed = EM_get_edge_for_index(index);

	return (eed->h==0 && eed->sharp);
}
static void draw_dm_edges_sharp(DerivedMesh *dm)
{
	dm->drawMappedEdges(dm, draw_dm_edges_sharp__setDrawOptions, NULL);
}


	/* Draw faces with color set based on selection
	 * return 2 for the active face so it renders with stipple enabled */
static int draw_dm_faces_sel__setDrawOptions(void *userData, int index, int *drawSmooth_r)
{
	struct { unsigned char *cols[3]; EditFace *efa_act; } * data = userData;
	EditFace *efa = EM_get_face_for_index(index);
	unsigned char *col;
	
	if (efa->h==0) {
		if (efa == data->efa_act) {
			glColor4ubv(data->cols[2]);
			return 2; /* stipple */
		} else {
			col = data->cols[(efa->f&SELECT)?1:0];
			if (col[3]==0) return 0;
			glColor4ubv(col);
			return 1;
		}
	}
	return 0;
}

/* also draws the active face */
static void draw_dm_faces_sel(DerivedMesh *dm, unsigned char *baseCol, unsigned char *selCol, unsigned char *actCol, EditFace *efa_act) 
{
	struct { unsigned char *cols[3]; EditFace *efa_act; } data;
	data.cols[0] = baseCol;
	data.cols[1] = selCol;
	data.cols[2] = actCol;
	data.efa_act = efa_act;

	dm->drawMappedFaces(dm, draw_dm_faces_sel__setDrawOptions, &data, 0);
}

static int draw_dm_creases__setDrawOptions(void *userData, int index)
{
	EditEdge *eed = EM_get_edge_for_index(index);

	if (eed->h==0 && eed->crease!=0.0) {
		UI_ThemeColorBlend(TH_WIRE, TH_EDGE_CREASE, eed->crease);
		return 1;
	} else {
		return 0;
	}
}
static void draw_dm_creases(DerivedMesh *dm)
{
	glLineWidth(3.0);
	dm->drawMappedEdges(dm, draw_dm_creases__setDrawOptions, NULL);
	glLineWidth(1.0);
}

static int draw_dm_bweights__setDrawOptions(void *userData, int index)
{
	EditEdge *eed = EM_get_edge_for_index(index);

	if (eed->h==0 && eed->bweight!=0.0) {
		UI_ThemeColorBlend(TH_WIRE, TH_EDGE_SELECT, eed->bweight);
		return 1;
	} else {
		return 0;
	}
}
static void draw_dm_bweights__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	EditVert *eve = EM_get_vert_for_index(index);

	if (eve->h==0 && eve->bweight!=0.0) {
		UI_ThemeColorBlend(TH_VERTEX, TH_VERTEX_SELECT, eve->bweight);
		bglVertex3fv(co);
	}
}
static void draw_dm_bweights(Scene *scene, DerivedMesh *dm)
{
	ToolSettings *ts= scene->toolsettings;

	if (ts->selectmode & SCE_SELECT_VERTEX) {
		glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE) + 2);
		bglBegin(GL_POINTS);
		dm->foreachMappedVert(dm, draw_dm_bweights__mapFunc, NULL);
		bglEnd();
	}
	else {
		glLineWidth(3.0);
		dm->drawMappedEdges(dm, draw_dm_bweights__setDrawOptions, NULL);
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

static void draw_em_fancy_verts(Scene *scene, View3D *v3d, Object *obedit, EditMesh *em, DerivedMesh *cageDM, EditVert *eve_act)
{
	ToolSettings *ts= scene->toolsettings;
	int sel;

	if(v3d->zbuf) glDepthMask(0);		// disable write in zbuffer, zbuf select

	for (sel=0; sel<2; sel++) {
		char col[4], fcol[4];
		int pass;

		UI_GetThemeColor3ubv(sel?TH_VERTEX_SELECT:TH_VERTEX, col);
		UI_GetThemeColor3ubv(sel?TH_FACE_DOT:TH_WIRE, fcol);

		for (pass=0; pass<2; pass++) {
			float size = UI_GetThemeValuef(TH_VERTEX_SIZE);
			float fsize = UI_GetThemeValuef(TH_FACEDOT_SIZE);

			if (pass==0) {
				if(v3d->zbuf && !(v3d->flag&V3D_ZBUF_SELECT)) {
					glDisable(GL_DEPTH_TEST);
						
					glEnable(GL_BLEND);
				} else {
					continue;
				}

				size = (size>2.1?size/2.0:size);
				fsize = (fsize>2.1?fsize/2.0:fsize);
				col[3] = fcol[3] = 100;
			} else {
				col[3] = fcol[3] = 255;
			}
				
			if(ts->selectmode & SCE_SELECT_VERTEX) {
				glPointSize(size);
				glColor4ubv((GLubyte *)col);
				draw_dm_verts(cageDM, sel, eve_act);
			}
			
			if(check_ob_drawface_dot(scene, v3d, obedit->dt)) {
				glPointSize(fsize);
				glColor4ubv((GLubyte *)fcol);
				draw_dm_face_centers(cageDM, sel);
			}
			
			if (pass==0) {
				glDisable(GL_BLEND);
				glEnable(GL_DEPTH_TEST);
			}
		}
	}

	if(v3d->zbuf) glDepthMask(1);
	glPointSize(1.0);
}

static void draw_em_fancy_edges(Scene *scene, View3D *v3d, Mesh *me, DerivedMesh *cageDM, short sel_only, EditEdge *eed_act)
{
	ToolSettings *ts= scene->toolsettings;
	int pass;
	unsigned char wireCol[4], selCol[4], actCol[4];

	/* since this function does transparant... */
	UI_GetThemeColor4ubv(TH_EDGE_SELECT, (char *)selCol);
	UI_GetThemeColor4ubv(TH_WIRE, (char *)wireCol);
	UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, (char *)actCol);
	
	/* when sel only is used, dont render wire, only selected, this is used for
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
			} else {
				continue;
			}
		} else {
			selCol[3] = 255;
			if (!sel_only) wireCol[3] = 255;
		}

		if(ts->selectmode == SCE_SELECT_FACE) {
			draw_dm_edges_sel(cageDM, wireCol, selCol, actCol, eed_act);
		}	
		else if( (me->drawflag & ME_DRAWEDGES) || (ts->selectmode & SCE_SELECT_EDGE) ) {	
			if(cageDM->drawMappedEdgesInterp && (ts->selectmode & SCE_SELECT_VERTEX)) {
				glShadeModel(GL_SMOOTH);
				draw_dm_edges_sel_interp(cageDM, wireCol, selCol);
				glShadeModel(GL_FLAT);
			} else {
				draw_dm_edges_sel(cageDM, wireCol, selCol, actCol, eed_act);
			}
		}
		else {
			if (!sel_only) {
				glColor4ubv(wireCol);
				draw_dm_edges(cageDM);
			}
		}

		if (pass==0) {
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
		}
	}
}	

static void draw_em_measure_stats(View3D *v3d, RegionView3D *rv3d, Object *ob, EditMesh *em, UnitSettings *unit)
{
	Mesh *me= ob->data;
	EditEdge *eed;
	EditFace *efa;
	float v1[3], v2[3], v3[3], v4[3], x, y, z;
	float fvec[3];
	char val[32]; /* Stores the measurement display text here */
	char conv_float[5]; /* Use a float conversion matching the grid size */
	float area, col[3]; /* area of the face,  color of the text to draw */
	float grid= unit->system ? unit->scale_length : v3d->grid;
	int do_split= unit->flag & USER_UNIT_OPT_SPLIT;

	if(v3d->flag2 & V3D_RENDER_OVERRIDE)
		return;

	/* make the precision of the pronted value proportionate to the gridsize */

	if (grid < 0.01f)
		strcpy(conv_float, "%.6f");
	else if (grid < 0.1f)
		strcpy(conv_float, "%.5f");
	else if (grid < 1.0f)
		strcpy(conv_float, "%.4f");
	else if (grid < 10.0f)
		strcpy(conv_float, "%.3f");
	else
		strcpy(conv_float, "%.2f");
	
	
	if(v3d->zbuf && (v3d->flag & V3D_ZBUF_SELECT)==0)
		glDisable(GL_DEPTH_TEST);

	if(v3d->zbuf) bglPolygonOffset(rv3d->dist, 5.0f);
	
	if(me->drawflag & ME_DRAW_EDGELEN) {
		UI_GetThemeColor3fv(TH_TEXT, col);
		/* make color a bit more red */
		if(col[0]> 0.5f) {col[1]*=0.7f; col[2]*= 0.7f;}
		else col[0]= col[0]*0.7f + 0.3f;
		glColor3fv(col);
		
		for(eed= em->edges.first; eed; eed= eed->next) {
			/* draw non fgon edges, or selected edges, or edges next to selected verts while draging */
			if((eed->h != EM_FGON) && ((eed->f & SELECT) || (G.moving && ((eed->v1->f & SELECT) || (eed->v2->f & SELECT)) ))) {
				VECCOPY(v1, eed->v1->co);
				VECCOPY(v2, eed->v2->co);
				
				x= 0.5f*(v1[0]+v2[0]);
				y= 0.5f*(v1[1]+v2[1]);
				z= 0.5f*(v1[2]+v2[2]);
				
				if(v3d->flag & V3D_GLOBAL_STATS) {
					mul_m4_v3(ob->obmat, v1);
					mul_m4_v3(ob->obmat, v2);
				}
				if(unit->system)
					bUnit_AsString(val, sizeof(val), len_v3v3(v1, v2)*unit->scale_length, 3, unit->system, B_UNIT_LENGTH, do_split, FALSE);
				else
					sprintf(val, conv_float, len_v3v3(v1, v2));
				
				view3d_cached_text_draw_add(x, y, z, val, 0, 0);
			}
		}
	}

	if(me->drawflag & ME_DRAW_FACEAREA) {
// XXX		extern int faceselectedOR(EditFace *efa, int flag);		// editmesh.h shouldn't be in this file... ok for now?
		
		UI_GetThemeColor3fv(TH_TEXT, col);
		/* make color a bit more green */
		if(col[1]> 0.5f) {col[0]*=0.7f; col[2]*= 0.7f;}
		else col[1]= col[1]*0.7f + 0.3f;
		glColor3fv(col);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if((efa->f & SELECT)) { // XXX || (G.moving && faceselectedOR(efa, SELECT)) ) {
				VECCOPY(v1, efa->v1->co);
				VECCOPY(v2, efa->v2->co);
				VECCOPY(v3, efa->v3->co);
				if (efa->v4) {
					VECCOPY(v4, efa->v4->co);
				}
				if(v3d->flag & V3D_GLOBAL_STATS) {
					mul_m4_v3(ob->obmat, v1);
					mul_m4_v3(ob->obmat, v2);
					mul_m4_v3(ob->obmat, v3);
					if (efa->v4) mul_m4_v3(ob->obmat, v4);
				}
				
				if (efa->v4)
					area=  area_quad_v3(v1, v2, v3, v4);
				else
					area = area_tri_v3(v1, v2, v3);

				if(unit->system)
					bUnit_AsString(val, sizeof(val), area*unit->scale_length, 3, unit->system, B_UNIT_LENGTH, do_split, FALSE); // XXX should be B_UNIT_AREA
				else
					sprintf(val, conv_float, area);

				view3d_cached_text_draw_add(efa->cent[0], efa->cent[1], efa->cent[2], val, 0, 0);
			}
		}
	}

	if(me->drawflag & ME_DRAW_EDGEANG) {
		EditEdge *e1, *e2, *e3, *e4;
		
		UI_GetThemeColor3fv(TH_TEXT, col);
		/* make color a bit more blue */
		if(col[2]> 0.5f) {col[0]*=0.7f; col[1]*= 0.7f;}
		else col[2]= col[2]*0.7f + 0.3f;
		glColor3fv(col);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			VECCOPY(v1, efa->v1->co);
			VECCOPY(v2, efa->v2->co);
			VECCOPY(v3, efa->v3->co);
			if(efa->v4) {
				VECCOPY(v4, efa->v4->co); 
			}
			else {
				VECCOPY(v4, v3);
			}
			if(v3d->flag & V3D_GLOBAL_STATS) {
				mul_m4_v3(ob->obmat, v1);
				mul_m4_v3(ob->obmat, v2);
				mul_m4_v3(ob->obmat, v3);
				mul_m4_v3(ob->obmat, v4);
			}
			
			e1= efa->e1;
			e2= efa->e2;
			e3= efa->e3;
			if(efa->e4) e4= efa->e4; else e4= e3;
			
			/* Calculate the angles */
				
			if( (e4->f & e1->f & SELECT) || (G.moving && (efa->v1->f & SELECT)) ) {
				/* Vec 1 */
				sprintf(val,"%.3f", RAD2DEG(angle_v3v3v3(v4, v1, v2)));
				interp_v3_v3v3(fvec, efa->cent, efa->v1->co, 0.8f);
				view3d_cached_text_draw_add(fvec[0], fvec[1], fvec[2], val, 0, 0);
			}
			if( (e1->f & e2->f & SELECT) || (G.moving && (efa->v2->f & SELECT)) ) {
				/* Vec 2 */
				sprintf(val,"%.3f", RAD2DEG(angle_v3v3v3(v1, v2, v3)));
				interp_v3_v3v3(fvec, efa->cent, efa->v2->co, 0.8f);
				view3d_cached_text_draw_add(fvec[0], fvec[1], fvec[2], val, 0, 0);
			}
			if( (e2->f & e3->f & SELECT) || (G.moving && (efa->v3->f & SELECT)) ) {
				/* Vec 3 */
				if(efa->v4) 
					sprintf(val,"%.3f", RAD2DEG(angle_v3v3v3(v2, v3, v4)));
				else
					sprintf(val,"%.3f", RAD2DEG(angle_v3v3v3(v2, v3, v1)));
				interp_v3_v3v3(fvec, efa->cent, efa->v3->co, 0.8f);
				view3d_cached_text_draw_add(fvec[0], fvec[1], fvec[2], val, 0, 0);
			}
				/* Vec 4 */
			if(efa->v4) {
				if( (e3->f & e4->f & SELECT) || (G.moving && (efa->v4->f & SELECT)) ) {
					sprintf(val,"%.3f", RAD2DEG(angle_v3v3v3(v3, v4, v1)));
					interp_v3_v3v3(fvec, efa->cent, efa->v4->co, 0.8f);
					view3d_cached_text_draw_add(fvec[0], fvec[1], fvec[2], val, 0, 0);
				}
			}
		}
	}
	
	if(v3d->zbuf) {
		glEnable(GL_DEPTH_TEST);
		bglPolygonOffset(rv3d->dist, 0.0f);
	}
}

static int draw_em_fancy__setFaceOpts(void *userData, int index, int *drawSmooth_r)
{
	EditFace *efa = EM_get_face_for_index(index);

	if (efa->h==0) {
		GPU_enable_material(efa->mat_nr+1, NULL);
		return 1;
	}
	else
		return 0;
}

static int draw_em_fancy__setGLSLFaceOpts(void *userData, int index)
{
	EditFace *efa = EM_get_face_for_index(index);

	return (efa->h==0);
}

static void draw_em_fancy(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob, EditMesh *em, DerivedMesh *cageDM, DerivedMesh *finalDM, int dt)
{
	Mesh *me = ob->data;
	EditFace *efa_act = EM_get_actFace(em, 0); /* annoying but active faces is stored differently */
	EditEdge *eed_act = NULL;
	EditVert *eve_act = NULL;
	
	if (em->selected.last) {
		EditSelection *ese = em->selected.last;
		/* face is handeled above */
		/*if (ese->type == EDITFACE ) {
			efa_act = (EditFace *)ese->data;
		} else */ if ( ese->type == EDITEDGE ) {
			eed_act = (EditEdge *)ese->data;
		} else if ( ese->type == EDITVERT ) {
			eve_act = (EditVert *)ese->data;
		}
	}
	
	EM_init_index_arrays(em, 1, 1, 1);

	if(dt>OB_WIRE) {
		if(CHECK_OB_DRAWTEXTURE(v3d, dt)) {
			if(draw_glsl_material(scene, ob, v3d, dt)) {
				glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

				finalDM->drawMappedFacesGLSL(finalDM, GPU_enable_material,
					draw_em_fancy__setGLSLFaceOpts, NULL);
				GPU_disable_material();

				glFrontFace(GL_CCW);
			}
			else {
				draw_mesh_textured(scene, v3d, rv3d, ob, finalDM, 0);
			}
		}
		else {
			/* 3 floats for position, 3 for normal and times two because the faces may actually be quads instead of triangles */
			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, me->flag & ME_TWOSIDED);

			glEnable(GL_LIGHTING);
			glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

			finalDM->drawMappedFaces(finalDM, draw_em_fancy__setFaceOpts, 0, 0);

			glFrontFace(GL_CCW);
			glDisable(GL_LIGHTING);
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
	
	if((me->drawflag & (ME_DRAWFACES)) || paint_facesel_test(ob)) {	/* transp faces */
		unsigned char col1[4], col2[4], col3[4];
			
		UI_GetThemeColor4ubv(TH_FACE, (char *)col1);
		UI_GetThemeColor4ubv(TH_FACE_SELECT, (char *)col2);
		UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, (char *)col3);
		
		glEnable(GL_BLEND);
		glDepthMask(0);		// disable write in zbuffer, needed for nice transp
		
		/* dont draw unselected faces, only selected, this is MUCH nicer when texturing */
		if CHECK_OB_DRAWTEXTURE(v3d, dt)
			col1[3] = 0;
		
		draw_dm_faces_sel(cageDM, col1, col2, col3, efa_act);

		glDisable(GL_BLEND);
		glDepthMask(1);		// restore write in zbuffer
	} else if (efa_act) {
		/* even if draw faces is off it would be nice to draw the stipple face
		 * Make all other faces zero alpha except for the active
		 * */
		unsigned char col1[4], col2[4], col3[4];
		col1[3] = col2[3] = 0; /* dont draw */
		UI_GetThemeColor4ubv(TH_EDITMESH_ACTIVE, (char *)col3);
		
		glEnable(GL_BLEND);
		glDepthMask(0);		// disable write in zbuffer, needed for nice transp
		
		draw_dm_faces_sel(cageDM, col1, col2, col3, efa_act);

		glDisable(GL_BLEND);
		glDepthMask(1);		// restore write in zbuffer
		
	}

	/* here starts all fancy draw-extra over */
	if((me->drawflag & ME_DRAWEDGES)==0 && CHECK_OB_DRAWTEXTURE(v3d, dt)) {
		/* we are drawing textures and 'ME_DRAWEDGES' is disabled, dont draw any edges */
		
		/* only draw selected edges otherwise there is no way of telling if a face is selected */
		draw_em_fancy_edges(scene, v3d, me, cageDM, 1, eed_act);
		
	} else {
		if(me->drawflag & ME_DRAWSEAMS) {
			UI_ThemeColor(TH_EDGE_SEAM);
			glLineWidth(2);
	
			draw_dm_edges_seams(cageDM);
	
			glColor3ub(0,0,0);
			glLineWidth(1);
		}
		
		if(me->drawflag & ME_DRAWSHARP) {
			UI_ThemeColor(TH_EDGE_SHARP);
			glLineWidth(2);
	
			draw_dm_edges_sharp(cageDM);
	
			glColor3ub(0,0,0);
			glLineWidth(1);
		}
	
		if(me->drawflag & ME_DRAWCREASES) {
			draw_dm_creases(cageDM);
		}
		if(me->drawflag & ME_DRAWBWEIGHTS) {
			draw_dm_bweights(scene, cageDM);
		}
	
		draw_em_fancy_edges(scene, v3d, me, cageDM, 0, eed_act);
	}
	if(em) {
// XXX		retopo_matrix_update(v3d);

		draw_em_fancy_verts(scene, v3d, ob, em, cageDM, eve_act);

		if(me->drawflag & ME_DRAWNORMALS) {
			UI_ThemeColor(TH_NORMAL);
			draw_dm_face_normals(scene, cageDM);
		}
		if(me->drawflag & ME_DRAW_VNORMALS) {
			UI_ThemeColor(TH_VNORMAL);
			draw_dm_vert_normals(scene, cageDM);
		}

		if(me->drawflag & (ME_DRAW_EDGELEN|ME_DRAW_FACEAREA|ME_DRAW_EDGEANG))
			draw_em_measure_stats(v3d, rv3d, ob, em, &scene->unit);
	}

	if(dt>OB_WIRE) {
		glDepthMask(1);
		bglPolygonOffset(rv3d->dist, 0.0);
		GPU_disable_material();
	}

	EM_free_index_arrays();
}

/* Mesh drawing routines */

static void draw_mesh_object_outline(View3D *v3d, Object *ob, DerivedMesh *dm)
{
	
	if(v3d->transp==0) {	// not when we draw the transparent pass
		glLineWidth(2.0);
		glDepthMask(0);
		
		/* if transparent, we cannot draw the edges for solid select... edges have no material info.
		   drawFacesSolid() doesn't draw the transparent faces */
		if(ob->dtx & OB_DRAWTRANSP) {
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

static int wpaint__setSolidDrawOptions(void *userData, int index, int *drawSmooth_r)
{
	*drawSmooth_r = 1;
	return 1;
}

static void draw_mesh_fancy(Scene *scene, ARegion *ar, View3D *v3d, RegionView3D *rv3d, Base *base, int dt, int flag)
{
	Object *ob= base->object;
	Mesh *me = ob->data;
	Material *ma= give_current_material(ob, 1);
	int hasHaloMat = (ma && (ma->material_type == MA_TYPE_HALO));
	int draw_wire = 0;
	int totvert, totedge, totface;
	DispList *dl;
	DerivedMesh *dm= mesh_get_derived_final(scene, ob, v3d->customdata_mask);

	if(!dm)
		return;
	
	if (ob->dtx&OB_DRAWWIRE) {
		draw_wire = 2; /* draw wire after solid using zoffset and depth buffer adjusment */
	}
	
	totvert = dm->getNumVerts(dm);
	totedge = dm->getNumEdges(dm);
	totface = dm->getNumFaces(dm);
	
	/* vertexpaint, faceselect wants this, but it doesnt work for shaded? */
	if(dt!=OB_SHADED)
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

		// Unwanted combination.
	if (ob==OBACT && paint_facesel_test(ob)) draw_wire = 0;

	if(dt==OB_BOUNDBOX) {
		if((v3d->flag2 & V3D_RENDER_OVERRIDE && v3d->drawtype >= OB_WIRE)==0)
			draw_bounding_volume(scene, ob);
	}
	else if(hasHaloMat || (totface==0 && totedge==0)) {
		glPointSize(1.5);
		dm->drawVerts(dm);
		glPointSize(1.0);
	}
	else if(dt==OB_WIRE || totface==0) {
		draw_wire = 1; /* draw wire only, no depth buffer stuff  */
	}
	else if(	(ob==OBACT && (ob->mode & OB_MODE_TEXTURE_PAINT || paint_facesel_test(ob))) ||
				CHECK_OB_DRAWTEXTURE(v3d, dt))
	{
		int faceselect= (ob==OBACT && paint_facesel_test(ob));
		if ((v3d->flag&V3D_SELECT_OUTLINE) && ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) && (base->flag&SELECT) && !(G.f&G_PICKSEL || paint_facesel_test(ob)) && !draw_wire) {
			draw_mesh_object_outline(v3d, ob, dm);
		}

		if(draw_glsl_material(scene, ob, v3d, dt)) {
			glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

			dm->drawFacesGLSL(dm, GPU_enable_material);
//			if(get_ob_property(ob, "Text"))
// XXX				draw_mesh_text(ob, 1);
			GPU_disable_material();

			glFrontFace(GL_CCW);
		}
		else {
			draw_mesh_textured(scene, v3d, rv3d, ob, dm, faceselect);
		}

		if(!faceselect) {
			if(base->flag & SELECT)
				UI_ThemeColor((ob==OBACT)?TH_ACTIVE:TH_SELECT);
			else
				UI_ThemeColor(TH_WIRE);

			if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0)
				dm->drawLooseEdges(dm);
		}
	}
	else if(dt==OB_SOLID) {
		if(ob==OBACT && ob && ob->mode & OB_MODE_WEIGHT_PAINT) {
			/* weight paint in solid mode, special case. focus on making the weights clear
			 * rather then the shading, this is also forced in wire view */
			GPU_enable_material(0, NULL);
			dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions, me->mface, 1);

			bglPolygonOffset(rv3d->dist, 1.0);
			glDepthMask(0);	// disable write in zbuffer, selected edge wires show better

			glEnable(GL_BLEND);
			glColor4ub(196, 196, 196, 196);
			glEnable(GL_LINE_STIPPLE);
			glLineStipple(1, 0x8888);

			dm->drawEdges(dm, 1, 0);

			bglPolygonOffset(rv3d->dist, 0.0);
			glDepthMask(1);
			glDisable(GL_LINE_STIPPLE);

			GPU_disable_material();


		}
		else {
			Paint *p;

			if((v3d->flag&V3D_SELECT_OUTLINE) && ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) && (base->flag&SELECT) && !draw_wire && !ob->sculpt)
				draw_mesh_object_outline(v3d, ob, dm);

			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, me->flag & ME_TWOSIDED );

			glEnable(GL_LIGHTING);
			glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);

			if(ob->sculpt && (p=paint_get_active(scene))) {
				float planes[4][4];
				float (*fpl)[4] = NULL;
				int fast= (p->flags & PAINT_FAST_NAVIGATE) && (rv3d->rflag & RV3D_NAVIGATING);

				if(ob->sculpt->partial_redraw) {
					if(ar->do_draw & RGN_DRAW_PARTIAL) {
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

			if(base->flag & SELECT) {
				UI_ThemeColor((ob==OBACT)?TH_ACTIVE:TH_SELECT);
			} else {
				UI_ThemeColor(TH_WIRE);
			}
			if(!ob->sculpt && (v3d->flag2 & V3D_RENDER_OVERRIDE)==0)
				dm->drawLooseEdges(dm);
		}
	}
	else if(dt==OB_SHADED) {
		int do_draw= 1;	/* to resolve all G.f settings below... */
		
		if(ob==OBACT) {
			do_draw= 0;
			if(ob && ob->mode & OB_MODE_WEIGHT_PAINT) {
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

				dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions, me->mface, 1);
				glDisable(GL_COLOR_MATERIAL);
				glDisable(GL_LIGHTING);

				GPU_disable_material();
			}
			else if(ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_TEXTURE_PAINT)) {
				if(me->mcol)
					dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions, NULL, 1);
				else {
					glColor3f(1.0f, 1.0f, 1.0f);
					dm->drawMappedFaces(dm, wpaint__setSolidDrawOptions, NULL, 0);
				}
			}
			else do_draw= 1;
		}
		if(do_draw) {
			dl = ob->disp.first;
			if (!dl || !dl->col1) {
				/* release and reload derivedmesh because it might be freed in
				   shadeDispList due to a different datamask */
				dm->release(dm);
				shadeDispList(scene, base);
				dl = find_displist(&ob->disp, DL_VERTCOL);
				dm= mesh_get_derived_final(scene, ob, v3d->customdata_mask);
			}

			if ((v3d->flag&V3D_SELECT_OUTLINE) && ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) && (base->flag&SELECT) && !draw_wire) {
				draw_mesh_object_outline(v3d, ob, dm);
			}

				/* False for dupliframe objects */
			if (dl) {
				unsigned int *obCol1 = dl->col1;
				unsigned int *obCol2 = dl->col2;

				dm->drawFacesColored(dm, me->flag&ME_TWOSIDED, (unsigned char*) obCol1, (unsigned char*) obCol2);
			}

			if(base->flag & SELECT) {
				UI_ThemeColor((ob==OBACT)?TH_ACTIVE:TH_SELECT);
			} else {
				UI_ThemeColor(TH_WIRE);
			}
			if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0)
				dm->drawLooseEdges(dm);
		}
	}
	
	/* set default draw color back for wire or for draw-extra later on */
	if (dt!=OB_WIRE) {
		if(base->flag & SELECT) {
			if(ob==OBACT && ob->flag & OB_FROMGROUP) 
				UI_ThemeColor(TH_GROUP_ACTIVE);
			else if(ob->flag & OB_FROMGROUP) 
				UI_ThemeColorShade(TH_GROUP_ACTIVE, -16);
			else if(flag!=DRAW_CONSTCOLOR)
				UI_ThemeColor((ob==OBACT)?TH_ACTIVE:TH_SELECT);
			else
				glColor3ub(80,80,80);
		} else {
			if (ob->flag & OB_FROMGROUP) 
				UI_ThemeColor(TH_GROUP);
			else {
				if(ob->dtx & OB_DRAWWIRE && flag==DRAW_CONSTCOLOR)
					glColor3ub(80,80,80);
				else
					UI_ThemeColor(TH_WIRE);
			}
		}
	}
	if (draw_wire) {

		/* When using wireframe object traw in particle edit mode
		 * the mesh gets in the way of seeing the particles, fade the wire color
		 * with the background. */
		if(ob==OBACT && (ob->mode & OB_MODE_PARTICLE_EDIT)) {
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
		if (dt!=OB_WIRE && draw_wire==2) {
			bglPolygonOffset(rv3d->dist, 1.0);
			glDepthMask(0);	// disable write in zbuffer, selected edge wires show better
		}
		
		if((v3d->flag2 & V3D_RENDER_OVERRIDE && v3d->drawtype >= OB_SOLID)==0)
			dm->drawEdges(dm, (dt==OB_WIRE || totface==0), 0);

		if (dt!=OB_WIRE && draw_wire==2) {
			glDepthMask(1);
			bglPolygonOffset(rv3d->dist, 0.0);
		}
	}

	dm->release(dm);
}

/* returns 1 if nothing was drawn, for detecting to draw an object center */
static int draw_mesh_object(Scene *scene, ARegion *ar, View3D *v3d, RegionView3D *rv3d, Base *base, int dt, int flag)
{
	Object *ob= base->object;
	Object *obedit= scene->obedit;
	Mesh *me= ob->data;
	EditMesh *em= me->edit_mesh;
	int do_alpha_pass= 0, drawlinked= 0, retval= 0, glsl, check_alpha;
	
	if(obedit && ob!=obedit && ob->data==obedit->data) {
		if(ob_get_key(ob));
		else drawlinked= 1;
	}
	
	if(ob==obedit || drawlinked) {
		DerivedMesh *finalDM, *cageDM;
		
		if (obedit!=ob)
			finalDM = cageDM = editmesh_get_derived_base(ob, em);
		else
			cageDM = editmesh_get_derived_cage_and_final(scene, ob, em, &finalDM,
											v3d->customdata_mask);

		if(dt>OB_WIRE) {
			// no transp in editmode, the fancy draw over goes bad then
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
		if(me->totface<=4 || boundbox_clip(rv3d, ob->obmat, (ob->bb)? ob->bb: me->bb)) {
			glsl = draw_glsl_material(scene, ob, v3d, dt);
			check_alpha = check_material_alpha(base, me, glsl);

			if(dt==OB_SOLID || glsl) {
				GPU_begin_object_materials(v3d, rv3d, scene, ob, glsl,
					(check_alpha)? &do_alpha_pass: NULL);
			}

			draw_mesh_fancy(scene, ar, v3d, rv3d, base, dt, flag);

			GPU_end_object_materials();
			
			if(me->totvert==0) retval= 1;
		}
	}
	
	/* GPU_begin_object_materials checked if this is needed */
	if(do_alpha_pass) add_view3d_after(v3d, base, V3D_TRANSP, flag);
	
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

	if(dlbase==NULL) return 1;
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); 

	for(dl= dlbase->first; dl; dl= dl->next) {
		if(dl->parts==0 || dl->nr==0)
			continue;
		
		data= dl->verts;
	
		switch(dl->type) {
		case DL_SEGM:
			
			glVertexPointer(3, GL_FLOAT, 0, data);
			
			for(parts=0; parts<dl->parts; parts++)
				glDrawArrays(GL_LINE_STRIP, parts*dl->nr, dl->nr);
				
			break;
		case DL_POLY:
			
			glVertexPointer(3, GL_FLOAT, 0, data);
			
			for(parts=0; parts<dl->parts; parts++)
				glDrawArrays(GL_LINE_LOOP, parts*dl->nr, dl->nr);
			
			break;
		case DL_SURF:
			
			glVertexPointer(3, GL_FLOAT, 0, data);
			
			for(parts=0; parts<dl->parts; parts++) {
				if(dl->flag & DL_CYCL_U) 
					glDrawArrays(GL_LINE_LOOP, parts*dl->nr, dl->nr);
				else
					glDrawArrays(GL_LINE_STRIP, parts*dl->nr, dl->nr);
			}
			
			for(nr=0; nr<dl->nr; nr++) {
				int ofs= 3*dl->nr;
				
				data= (  dl->verts )+3*nr;
				parts= dl->parts;

				if(dl->flag & DL_CYCL_V) glBegin(GL_LINE_LOOP);
				else glBegin(GL_LINE_STRIP);
				
				while(parts--) {
					glVertex3fv(data);
					data+=ofs;
				}
				glEnd();
				
				/* (ton) this code crashes for me when resolv is 86 or higher... no clue */
//				glVertexPointer(3, GL_FLOAT, sizeof(float)*3*dl->nr, data + 3*nr);
//				if(dl->flag & DL_CYCL_V) 
//					glDrawArrays(GL_LINE_LOOP, 0, dl->parts);
//				else
//					glDrawArrays(GL_LINE_STRIP, 0, dl->parts);
			}
			break;
			
		case DL_INDEX3:
			if(draw_index_wire) {
				glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				glDrawElements(GL_TRIANGLES, 3*dl->parts, GL_UNSIGNED_INT, dl->index);
			}
			break;
			
		case DL_INDEX4:
			if(draw_index_wire) {
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
	
	if(lb==NULL) return;
	
	/* for drawing wire */
	glGetFloatv(GL_CURRENT_COLOR, curcol);

	glEnable(GL_LIGHTING);
	glEnableClientState(GL_VERTEX_ARRAY);
	
	if(ob->transflag & OB_NEG_SCALE) glFrontFace(GL_CW);
	else glFrontFace(GL_CCW);
	
	if(ob->type==OB_MBALL) {	// mball always smooth shaded
		glShadeModel(GL_SMOOTH);
	}
	
	dl= lb->first;
	while(dl) {
		data= dl->verts;
		ndata= dl->nors;

		switch(dl->type) {
		case DL_SEGM:
			if(ob->type==OB_SURF) {
				int nr;

				glDisable(GL_LIGHTING);
				glColor3fv(curcol);
				
				// glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				// glDrawArrays(GL_LINE_STRIP, 0, dl->nr);

				glBegin(GL_LINE_STRIP);
				for(nr= dl->nr; nr; nr--, data+=3)
					glVertex3fv(data);
				glEnd();

				glEnable(GL_LIGHTING);
			}
			break;
		case DL_POLY:
			if(ob->type==OB_SURF) {
				int nr;

				glDisable(GL_LIGHTING);
				
				/* for some reason glDrawArrays crashes here in half of the platforms (not osx) */
				//glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				//glDrawArrays(GL_LINE_LOOP, 0, dl->nr);
				
				glBegin(GL_LINE_LOOP);
				for(nr= dl->nr; nr; nr--, data+=3)
					glVertex3fv(data);
				glEnd();
				
				glEnable(GL_LIGHTING);
				break;
			}
		case DL_SURF:
			
			if(dl->index) {
				GPU_enable_material(dl->col+1, (glsl)? &gattribs: NULL);
				
				if(dl->rt & CU_SMOOTH) glShadeModel(GL_SMOOTH);
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
			if(index3_nors_incr) {
				glEnableClientState(GL_NORMAL_ARRAY);
				glNormalPointer(GL_FLOAT, 0, dl->nors);
			}
			else
				glNormal3fv(ndata);
			
			glDrawElements(GL_TRIANGLES, 3*dl->parts, GL_UNSIGNED_INT, dl->index);
			
			if(index3_nors_incr)
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

static void drawDispListshaded(ListBase *lb, Object *ob)
{
	DispList *dl, *dlob;
	unsigned int *cdata;

	if(lb==NULL) return;

	glShadeModel(GL_SMOOTH);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	
	dl= lb->first;
	dlob= ob->disp.first;
	while(dl && dlob) {
		
		cdata= dlob->col1;
		if(cdata==NULL) break;
		
		switch(dl->type) {
		case DL_SURF:
			if(dl->index) {
				glVertexPointer(3, GL_FLOAT, 0, dl->verts);
				glColorPointer(4, GL_UNSIGNED_BYTE, 0, cdata);
				glDrawElements(GL_QUADS, 4*dl->totindex, GL_UNSIGNED_INT, dl->index);
			}			
			break;

		case DL_INDEX3:
			
			glVertexPointer(3, GL_FLOAT, 0, dl->verts);
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, cdata);
			glDrawElements(GL_TRIANGLES, 3*dl->parts, GL_UNSIGNED_INT, dl->index);
			break;

		case DL_INDEX4:
			
			glVertexPointer(3, GL_FLOAT, 0, dl->verts);
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, cdata);
			glDrawElements(GL_QUADS, 4*dl->parts, GL_UNSIGNED_INT, dl->index);
			break;
		}
		
		dl= dl->next;
		dlob= dlob->next;
	}
	
	glShadeModel(GL_FLAT);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
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

	if(dt>OB_WIRE && dm->getNumFaces(dm)) {
		int glsl = draw_glsl_material(scene, ob, v3d, dt);
		GPU_begin_object_materials(v3d, rv3d, scene, ob, glsl, NULL);

		if (!glsl)
			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);

		glEnable(GL_LIGHTING);
		dm->drawFacesSolid(dm, NULL, 0, GPU_enable_material);
		glDisable(GL_LIGHTING);
		GPU_end_object_materials();
	} else {
		if((v3d->flag2 & V3D_RENDER_OVERRIDE && v3d->drawtype >= OB_SOLID)==0)
			drawCurveDMWired (ob);
	}

	return 0;
}

/* returns 1 when nothing was drawn */
static int drawDispList(Scene *scene, View3D *v3d, RegionView3D *rv3d, Base *base, int dt)
{
	Object *ob= base->object;
	ListBase *lb=0;
	DispList *dl;
	Curve *cu;
	int solid, retval= 0;
	
	solid= (dt > OB_WIRE);

	if (drawCurveDerivedMesh(scene, v3d, rv3d, base, dt) == 0) {
		return 0;
	}

	switch(ob->type) {
	case OB_FONT:
	case OB_CURVE:
		cu= ob->data;
		
		lb= &cu->disp;
		
		if(solid) {
			dl= lb->first;
			if(dl==NULL) return 1;

			if(dl->nors==0) addnormalsDispList(ob, lb);
			index3_nors_incr= 0;
			
			if( displist_has_faces(lb)==0) {
				draw_index_wire= 0;
				drawDispListwire(lb);
				draw_index_wire= 1;
			}
			else {
				if(draw_glsl_material(scene, ob, v3d, dt)) {
					GPU_begin_object_materials(v3d, rv3d, scene, ob, 1, NULL);
					drawDispListsolid(lb, ob, 1);
					GPU_end_object_materials();
				}
				else if(dt == OB_SHADED) {
					if(ob->disp.first==0) shadeDispList(scene, base);
					drawDispListshaded(lb, ob);
				}
				else {
					GPU_begin_object_materials(v3d, rv3d, scene, ob, 0, NULL);
					glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
					drawDispListsolid(lb, ob, 0);
					GPU_end_object_materials();
				}
				if(cu->editnurb && cu->bevobj==NULL && cu->taperobj==NULL && cu->ext1 == 0.0 && cu->ext2 == 0.0) {
					cpack(0);
					draw_index_wire= 0;
					drawDispListwire(lb);
					draw_index_wire= 1;
				}
			}
			index3_nors_incr= 1;
		}
		else {
			draw_index_wire= 0;
			retval= drawDispListwire(lb);
			draw_index_wire= 1;
		}
		break;
	case OB_SURF:

		lb= &((Curve *)ob->data)->disp;
		
		if(solid) {
			dl= lb->first;
			if(dl==NULL) return 1;
			
			if(dl->nors==NULL) addnormalsDispList(ob, lb);
			
			if(draw_glsl_material(scene, ob, v3d, dt)) {
				GPU_begin_object_materials(v3d, rv3d, scene, ob, 1, NULL);
				drawDispListsolid(lb, ob, 1);
				GPU_end_object_materials();
			}
			else if(dt==OB_SHADED) {
				if(ob->disp.first==NULL) shadeDispList(scene, base);
				drawDispListshaded(lb, ob);
			}
			else {
				GPU_begin_object_materials(v3d, rv3d, scene, ob, 0, NULL);
				glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
				drawDispListsolid(lb, ob, 0);
				GPU_end_object_materials();
			}
		}
		else {
			retval= drawDispListwire(lb);
		}
		break;
	case OB_MBALL:
		
		if( is_basis_mball(ob)) {
			lb= &ob->disp;
			if(lb->first==NULL) makeDispListMBall(scene, ob);
			if(lb->first==NULL) return 1;
			
			if(solid) {
				
				if(draw_glsl_material(scene, ob, v3d, dt)) {
					GPU_begin_object_materials(v3d, rv3d, scene, ob, 1, NULL);
					drawDispListsolid(lb, ob, 1);
					GPU_end_object_materials();
				}
				else if(dt == OB_SHADED) {
					dl= lb->first;
					if(dl && dl->col1==0) shadeDispList(scene, base);
					drawDispListshaded(lb, ob);
				}
				else {
					GPU_begin_object_materials(v3d, rv3d, scene, ob, 0, NULL);
					glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
					drawDispListsolid(lb, ob, 0);
					GPU_end_object_materials();
				}
			}
			else{
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
	switch(draw_as){
		case PART_DRAW_AXIS:
		case PART_DRAW_CROSS:
			glDrawArrays(GL_LINES, 0, 6*totpoint);
			break;
		case PART_DRAW_LINE:
			glDrawArrays(GL_LINES, 0, 2*totpoint);
			break;
		case PART_DRAW_BB:
			if(ob_dt<=OB_WIRE || select)
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
static void draw_particle(ParticleKey *state, int draw_as, short draw, float pixsize, float imat[4][4], float *draw_line, ParticleBillboardData *bb, ParticleDrawData *pdd)
{
	float vec[3], vec2[3];
	float *vd = pdd->vd;
	float *cd = pdd->cd;
	float ma_r=0.0f;
	float ma_g=0.0f;
	float ma_b=0.0f;

	if(pdd->ma_r) {
		ma_r = *pdd->ma_r;
		ma_g = *pdd->ma_g;
		ma_b = *pdd->ma_b;
	}

	switch(draw_as){
		case PART_DRAW_DOT:
		{
			if(vd) {
				VECCOPY(vd,state->co) pdd->vd+=3;
			}
			if(cd) {
				cd[0]=ma_r;
				cd[1]=ma_g;
				cd[2]=ma_b;
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
			if(draw_as==PART_DRAW_AXIS) {
				cd[1]=cd[2]=cd[4]=cd[5]=0.0;
				cd[0]=cd[3]=1.0;
				cd[6]=cd[8]=cd[9]=cd[11]=0.0;
				cd[7]=cd[10]=1.0;
				cd[13]=cd[12]=cd[15]=cd[16]=0.0;
				cd[14]=cd[17]=1.0;
				pdd->cd+=18;

				VECCOPY(vec2,state->co);
			}
			else {
				if(cd) {
					cd[0]=cd[3]=cd[6]=cd[9]=cd[12]=cd[15]=ma_r;
					cd[1]=cd[4]=cd[7]=cd[10]=cd[13]=cd[16]=ma_g;
					cd[2]=cd[5]=cd[8]=cd[11]=cd[14]=cd[17]=ma_b;
					pdd->cd+=18;
				}
				VECSUB(vec2,state->co,vec);
			}

			VECADD(vec,state->co,vec);
			VECCOPY(pdd->vd,vec); pdd->vd+=3;
			VECCOPY(pdd->vd,vec2); pdd->vd+=3;
				
			vec[1]=2.0f*pixsize;
			vec[0]=vec[2]=0.0;
			mul_qt_v3(state->rot,vec);
			if(draw_as==PART_DRAW_AXIS){
				VECCOPY(vec2,state->co);
			}		
			else VECSUB(vec2,state->co,vec);

			VECADD(vec,state->co,vec);
			VECCOPY(pdd->vd,vec); pdd->vd+=3;
			VECCOPY(pdd->vd,vec2); pdd->vd+=3;

			vec[2]=2.0f*pixsize;
			vec[0]=vec[1]=0.0;
			mul_qt_v3(state->rot,vec);
			if(draw_as==PART_DRAW_AXIS){
				VECCOPY(vec2,state->co);
			}
			else VECSUB(vec2,state->co,vec);

			VECADD(vec,state->co,vec);

			VECCOPY(pdd->vd,vec); pdd->vd+=3;
			VECCOPY(pdd->vd,vec2); pdd->vd+=3;
			break;
		}
		case PART_DRAW_LINE:
		{
			VECCOPY(vec,state->vel);
			normalize_v3(vec);
			if(draw & PART_DRAW_VEL_LENGTH)
				mul_v3_fl(vec,len_v3(state->vel));
			VECADDFAC(pdd->vd,state->co,vec,-draw_line[0]); pdd->vd+=3;
			VECADDFAC(pdd->vd,state->co,vec,draw_line[1]); pdd->vd+=3;
			if(cd) {
				cd[0]=cd[3]=ma_r;
				cd[1]=cd[4]=ma_g;
				cd[2]=cd[5]=ma_b;
				pdd->cd+=6;
			}
			break;
		}
		case PART_DRAW_CIRC:
		{
			if(pdd->ma_r)
				glColor3f(ma_r,ma_g,ma_b);
			drawcircball(GL_LINE_LOOP, state->co, pixsize, imat);
			break;
		}
		case PART_DRAW_BB:
		{
			float xvec[3], yvec[3], zvec[3], bb_center[3];
			if(cd) {
				cd[0]=cd[3]=cd[6]=cd[9]=ma_r;
				cd[1]=cd[4]=cd[7]=cd[10]=ma_g;
				cd[2]=cd[5]=cd[8]=cd[11]=ma_b;
				pdd->cd+=12;
			}


			VECCOPY(bb->vec, state->co);
			VECCOPY(bb->vel, state->vel);

			psys_make_billboard(bb, xvec, yvec, zvec, bb_center);
			
			VECADD(pdd->vd,bb_center,xvec);
			VECADD(pdd->vd,pdd->vd,yvec); pdd->vd+=3;

			VECSUB(pdd->vd,bb_center,xvec);
			VECADD(pdd->vd,pdd->vd,yvec); pdd->vd+=3;

			VECSUB(pdd->vd,bb_center,xvec);
			VECSUB(pdd->vd,pdd->vd,yvec); pdd->vd+=3;

			VECADD(pdd->vd,bb_center,xvec);
			VECSUB(pdd->vd,pdd->vd,yvec); pdd->vd+=3;

			VECCOPY(pdd->nd, zvec); pdd->nd+=3;
			VECCOPY(pdd->nd, zvec); pdd->nd+=3;
			VECCOPY(pdd->nd, zvec); pdd->nd+=3;
			VECCOPY(pdd->nd, zvec); pdd->nd+=3;
			break;
		}
	}
}
/* unified drawing of all new particle systems draw types except dupli ob & group	*/
/* mostly tries to use vertex arrays for speed										*/

/* 1. check that everything is ok & updated */
/* 2. start initialising things				*/
/* 3. initialize according to draw type		*/
/* 4. allocate drawing data arrays			*/
/* 5. start filling the arrays				*/
/* 6. draw the arrays						*/
/* 7. clean up								*/
static void draw_new_particle_system(Scene *scene, View3D *v3d, RegionView3D *rv3d, Base *base, ParticleSystem *psys, int ob_dt)
{
	Object *ob=base->object;
	ParticleSystemModifierData *psmd;
	ParticleEditSettings *pset = PE_settings(scene);
	ParticleSettings *part;
	ParticleData *pars, *pa;
	ParticleKey state, *states=0;
	ParticleBillboardData bb;
	ParticleSimulationData sim = {scene, ob, psys, NULL};
	ParticleDrawData *pdd = psys->pdd;
	Material *ma;
	float vel[3], imat[4][4];
	float timestep, pixsize=1.0, pa_size, r_tilt, r_length;
	float pa_time, pa_birthtime, pa_dietime, pa_health;
	float cfra= bsystem_time(scene, ob,(float)CFRA,0.0);
	float ma_r=0.0f, ma_g=0.0f, ma_b=0.0f;
	int a, totpart, totpoint=0, totve=0, drawn, draw_as, totchild=0;
	int select=ob->flag&SELECT, create_cdata=0, need_v=0;
	GLint polygonmode[2];
	char val[32];

/* 1. */
	if(psys==0)
		return;

	part=psys->part;
	pars=psys->particles;

	if(part==0 || !psys_check_enabled(ob, psys))
		return;

	if(pars==0) return;

	/* don't draw normal paths in edit mode */
	if(psys_in_edit_mode(scene, psys) && (pset->flag & PE_DRAW_PART)==0)
		return;
		
	if(part->draw_as == PART_DRAW_REND)
		draw_as = part->ren_as;
	else
		draw_as = part->draw_as;

	if(draw_as == PART_DRAW_NOT)
		return;

/* 2. */
	sim.psmd = psmd = psys_get_modifier(ob,psys);

	if(part->phystype==PART_PHYS_KEYED){
		if(psys->flag&PSYS_KEYED){
			psys_count_keyed_targets(&sim);
			if(psys->totkeyed==0)
				return;
		}
	}

	if(select){
		select=0;
		if(psys_get_current(ob)==psys)
			select=1;
	}

	psys->flag|=PSYS_DRAWING;

	if(part->type==PART_HAIR && !psys->childcache)
		totchild=0;
	else
		totchild=psys->totchild*part->disp/100;

	ma= give_current_material(ob,part->omat);

	if(v3d->zbuf) glDepthMask(1);

	if((ma) && (part->draw&PART_DRAW_MAT_COL)) {
		glColor3f(ma->r,ma->g,ma->b);

		ma_r = ma->r;
		ma_g = ma->g;
		ma_b = ma->b;
	}
	else
		cpack(0);

	if(pdd) {
		pdd->ma_r = &ma_r;
		pdd->ma_g = &ma_g;
		pdd->ma_b = &ma_b;
	}

	timestep= psys_get_timestep(&sim);

	if( (base->flag & OB_FROMDUPLI) && (ob->flag & OB_FROMGROUP) ) {
		float mat[4][4];
		mul_m4_m4m4(mat, psys->imat, ob->obmat);
		glMultMatrixf(mat);
	}

	/* needed for text display */
	invert_m4_m4(ob->imat, ob->obmat);

	totpart=psys->totpart;

	//if(part->flag&PART_GLOB_TIME)
	cfra=bsystem_time(scene, 0, (float)CFRA, 0.0f);

	if(draw_as==PART_DRAW_PATH && psys->pathcache==NULL && psys->childcache==NULL)
		draw_as=PART_DRAW_DOT;

/* 3. */
	switch(draw_as){
		case PART_DRAW_DOT:
			if(part->draw_size)
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
			pixsize= rv3d->persmat[0][3]*ob->obmat[3][0]+ rv3d->persmat[1][3]*ob->obmat[3][1]+ rv3d->persmat[2][3]*ob->obmat[3][2]+ rv3d->persmat[3][3];
			pixsize*= rv3d->pixsize;
			if(part->draw_size==0.0)
				pixsize*=2.0;
			else
				pixsize*=part->draw_size;

			if(draw_as==PART_DRAW_AXIS)
				create_cdata = 1;
			break;
		case PART_DRAW_OB:
			if(part->dup_ob==0)
				draw_as=PART_DRAW_DOT;
			else
				draw_as=0;
			break;
		case PART_DRAW_GR:
			if(part->dup_group==0)
				draw_as=PART_DRAW_DOT;
			else
				draw_as=0;
			break;
		case PART_DRAW_BB:
			if(v3d->camera==0 && part->bb_ob==0){
				printf("Billboards need an active camera or a target object!\n");

				draw_as=part->draw_as=PART_DRAW_DOT;

				if(part->draw_size)
					glPointSize(part->draw_size);
				else
					glPointSize(2.0); /* default dot size */
			}
			else if(part->bb_ob)
				bb.ob=part->bb_ob;
			else
				bb.ob=v3d->camera;

			bb.align = part->bb_align;
			bb.anim = part->bb_anim;
			bb.lock = part->draw & PART_DRAW_BB_LOCK;
			bb.offset[0] = part->bb_offset[0];
			bb.offset[1] = part->bb_offset[1];
			break;
		case PART_DRAW_PATH:
			break;
		case PART_DRAW_LINE:
			need_v=1;
			break;
	}
	if(part->draw & PART_DRAW_SIZE && part->draw_as!=PART_DRAW_CIRC){
		copy_m4_m4(imat, rv3d->viewinv);
		normalize_v3(imat[0]);
		normalize_v3(imat[1]);
	}

	if(!create_cdata && pdd && pdd->cdata) {
		MEM_freeN(pdd->cdata);
		pdd->cdata = pdd->cd = NULL;
	}

/* 4. */
	if(draw_as && ELEM(draw_as, PART_DRAW_PATH, PART_DRAW_CIRC)==0) {
		int tot_vec_size = (totpart + totchild) * 3 * sizeof(float);
		int create_ndata = 0;

		if(!pdd)
			pdd = psys->pdd = MEM_callocN(sizeof(ParticleDrawData), "ParticlDrawData");

		if(part->draw_as == PART_DRAW_REND && part->trail_count > 1) {
			tot_vec_size *= part->trail_count;
			psys_make_temp_pointcache(ob, psys);
		}

		switch(draw_as) {
			case PART_DRAW_AXIS:
			case PART_DRAW_CROSS:
				tot_vec_size *= 6;
				if(draw_as != PART_DRAW_CROSS)
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

		if(pdd->tot_vec_size != tot_vec_size)
			psys_free_pdd(psys);

		if(!pdd->vdata)
			pdd->vdata = MEM_callocN(tot_vec_size, "particle_vdata");
		if(create_cdata && !pdd->cdata)
			pdd->cdata = MEM_callocN(tot_vec_size, "particle_cdata");
		if(create_ndata && !pdd->ndata)
			pdd->ndata = MEM_callocN(tot_vec_size, "particle_vdata");

		if(part->draw & PART_DRAW_VEL && draw_as != PART_DRAW_LINE) {
			if(!pdd->vedata)
				pdd->vedata = MEM_callocN(2 * (totpart + totchild) * 3 * sizeof(float), "particle_vedata");

			need_v = 1;
		} else if (pdd->vedata) {
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

	psys->lattice= psys_get_lattice(&sim);

	if(pdd && draw_as!=PART_DRAW_PATH){
/* 5. */
		if((pdd->flag & PARTICLE_DRAW_DATA_UPDATED)
			&& (pdd->vedata || part->draw & (PART_DRAW_SIZE|PART_DRAW_NUM|PART_DRAW_HEALTH))==0) {
			totpoint = pdd->totpoint; /* draw data is up to date */
		}
		else for(a=0,pa=pars; a<totpart+totchild; a++, pa++){
			/* setup per particle individual stuff */
			if(a<totpart){
				if(totchild && (part->draw&PART_DRAW_PARENT)==0) continue;
				if(pa->flag & PARS_NO_DISP || pa->flag & PARS_UNEXIST) continue;

				pa_time=(cfra-pa->time)/pa->lifetime;
				pa_birthtime=pa->time;
				pa_dietime = pa->dietime;
				pa_size=pa->size;
				if(part->phystype==PART_PHYS_BOIDS)
					pa_health = pa->boid->data.health;
				else
					pa_health = -1.0;

#if 0 // XXX old animation system	
				if((part->flag&PART_ABS_TIME)==0){			
					if(ma && ma->ipo){
						IpoCurve *icu;

						/* correction for lifetime */
						calc_ipo(ma->ipo, 100.0f*pa_time);

						for(icu = ma->ipo->curve.first; icu; icu=icu->next) {
							if(icu->adrcode == MA_COL_R)
								ma_r = icu->curval;
							else if(icu->adrcode == MA_COL_G)
								ma_g = icu->curval;
							else if(icu->adrcode == MA_COL_B)
								ma_b = icu->curval;
						}
					}
					if(part->ipo) {
						IpoCurve *icu;

						/* correction for lifetime */
						calc_ipo(part->ipo, 100*pa_time);

						for(icu = part->ipo->curve.first; icu; icu=icu->next) {
							if(icu->adrcode == PART_SIZE)
								pa_size = icu->curval;
						}
					}
				}
#endif // XXX old animation system

				r_tilt = 2.0f*(PSYS_FRAND(a + 21) - 0.5f);
				r_length = PSYS_FRAND(a + 22);
			}
			else{
				ChildParticle *cpa= &psys->child[a-totpart];

				pa_time=psys_get_child_time(psys,cpa,cfra,&pa_birthtime,&pa_dietime);

#if 0 // XXX old animation system
				if((part->flag&PART_ABS_TIME)==0) {
					if(ma && ma->ipo){
						IpoCurve *icu;

						/* correction for lifetime */
						calc_ipo(ma->ipo, 100.0f*pa_time);

						for(icu = ma->ipo->curve.first; icu; icu=icu->next) {
							if(icu->adrcode == MA_COL_R)
								ma_r = icu->curval;
							else if(icu->adrcode == MA_COL_G)
								ma_g = icu->curval;
							else if(icu->adrcode == MA_COL_B)
								ma_b = icu->curval;
						}
					}
				}
#endif // XXX old animation system

				pa_size=psys_get_child_size(psys,cpa,cfra,0);

				pa_health = -1.0;

				r_tilt = 2.0f*(PSYS_FRAND(a + 21) - 0.5f);
				r_length = PSYS_FRAND(a + 22);
			}

			drawn = 0;
			if(part->draw_as == PART_DRAW_REND && part->trail_count > 1) {
				float length = part->path_end * (1.0 - part->randlength * r_length);
				int trail_count = part->trail_count * (1.0 - part->randlength * r_length);
				float ct = ((part->draw & PART_ABS_PATH_TIME) ? cfra : pa_time) - length;
				float dt = length / (trail_count ? (float)trail_count : 1.0f);
				int i=0;

				ct+=dt;
				for(i=0; i < trail_count; i++, ct += dt) {
					if(part->draw & PART_ABS_PATH_TIME) {
						if(ct < pa_birthtime || ct > pa_dietime)
							continue;
					}
					else if(ct < 0.0f || ct > 1.0f)
						continue;

					state.time = (part->draw & PART_ABS_PATH_TIME) ? -ct : -(pa_birthtime + ct * (pa_dietime - pa_birthtime));
					psys_get_particle_on_path(&sim,a,&state,need_v);
					
					if(psys->parent)
						mul_m4_v3(psys->parent->obmat, state.co);

					/* create actiual particle data */
					if(draw_as == PART_DRAW_BB) {
						bb.size = pa_size;
						bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
						bb.time = ct;
					}

					draw_particle(&state, draw_as, part->draw, pixsize, imat, part->draw_line, &bb, psys->pdd);

					totpoint++;
					drawn = 1;
				}
			}
			else
			{
				state.time=cfra;
				if(psys_get_particle_state(&sim,a,&state,0)){
					if(psys->parent)
						mul_m4_v3(psys->parent->obmat, state.co);

					/* create actiual particle data */
					if(draw_as == PART_DRAW_BB) {
						bb.size = pa_size;
						bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
						bb.time = pa_time;
					}

					draw_particle(&state, draw_as, part->draw, pixsize, imat, part->draw_line, &bb, pdd);

					totpoint++;
					drawn = 1;
				}
			}

			if(drawn) {
				/* additional things to draw for each particle	*/
				/* (velocity, size and number)					*/
				if((part->draw & PART_DRAW_VEL) && pdd->vedata){
					VECCOPY(pdd->ved,state.co);
					pdd->ved+=3;
					VECCOPY(vel,state.vel);
					mul_v3_fl(vel,timestep);
					VECADD(pdd->ved,state.co,vel);
					pdd->ved+=3;

					totve++;
				}

				if(part->draw & PART_DRAW_SIZE){
					setlinestyle(3);
					drawcircball(GL_LINE_LOOP, state.co, pa_size, imat);
					setlinestyle(0);
				}


				if((part->draw & PART_DRAW_NUM || part->draw & PART_DRAW_HEALTH) && (v3d->flag2 & V3D_RENDER_OVERRIDE)==0){
					float vec_txt[3];
					char *val_pos= val;
					val[0]= '\0';

					if(part->draw&PART_DRAW_NUM)
						val_pos += sprintf(val, "%i", a);

					if((part->draw & PART_DRAW_HEALTH) && a < totpart && part->phystype==PART_PHYS_BOIDS)
						sprintf(val_pos, (val_pos==val) ? "%.2f" : ":%.2f", pa_health);

					/* in path drawing state.co is the end point */
					/* use worldspace beause object matrix is alredy applied */
					mul_v3_m4v3(vec_txt, ob->imat, state.co);
					view3d_cached_text_draw_add(vec_txt[0],  vec_txt[1],  vec_txt[2], val, 10, V3D_CACHE_TEXT_WORLDSPACE);
				}
			}
		}
	}
/* 6. */

	glGetIntegerv(GL_POLYGON_MODE, polygonmode);
	glEnableClientState(GL_VERTEX_ARRAY);

	if(draw_as==PART_DRAW_PATH){
		ParticleCacheKey **cache, *path;
		float *cd2=0,*cdata2=0;

		/* setup gl flags */
		if (1) { //ob_dt > OB_WIRE) {
			glEnableClientState(GL_NORMAL_ARRAY);

			if(part->draw&PART_DRAW_MAT_COL)
				glEnableClientState(GL_COLOR_ARRAY);

			glEnable(GL_LIGHTING);
			glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
			glEnable(GL_COLOR_MATERIAL);
		}
		/*else {
			glDisableClientState(GL_NORMAL_ARRAY);

			glDisable(GL_COLOR_MATERIAL);
			glDisable(GL_LIGHTING);
			UI_ThemeColor(TH_WIRE);
		}*/

		if(totchild && (part->draw&PART_DRAW_PARENT)==0)
			totpart=0;
		else if(psys->pathcache==NULL)
			totpart=0;

		/* draw actual/parent particles */
		cache=psys->pathcache;
		for(a=0, pa=psys->particles; a<totpart; a++, pa++){
			path=cache[a];
			if(path->steps > 0) {
				glVertexPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->co);

				if(1) { //ob_dt > OB_WIRE) {
					glNormalPointer(GL_FLOAT, sizeof(ParticleCacheKey), path->vel);
					if(part->draw&PART_DRAW_MAT_COL)
						glColorPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->col);
				}

				glDrawArrays(GL_LINE_STRIP, 0, path->steps + 1);
			}
		}
		
		/* draw child particles */
		cache=psys->childcache;
		for(a=0; a<totchild; a++){
			path=cache[a];
			glVertexPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->co);

			if(1) { //ob_dt > OB_WIRE) {
				glNormalPointer(GL_FLOAT, sizeof(ParticleCacheKey), path->vel);
				if(part->draw&PART_DRAW_MAT_COL)
					glColorPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->col);
			}

			glDrawArrays(GL_LINE_STRIP, 0, path->steps + 1);
		}


		/* restore & clean up */
		if(1) { //ob_dt > OB_WIRE) {
			if(part->draw&PART_DRAW_MAT_COL)
				glDisable(GL_COLOR_ARRAY);
			glDisable(GL_COLOR_MATERIAL);
		}

		if(cdata2)
			MEM_freeN(cdata2);
		cd2=cdata2=0;

		glLineWidth(1.0f);
	}
	else if(pdd && ELEM(draw_as, 0, PART_DRAW_CIRC)==0){
		glDisableClientState(GL_COLOR_ARRAY);

		/* enable point data array */
		if(pdd->vdata){
			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(3, GL_FLOAT, 0, pdd->vdata);
		}
		else
			glDisableClientState(GL_VERTEX_ARRAY);

		if(select) {
			UI_ThemeColor(TH_ACTIVE);
			
			if(part->draw_size)
				glPointSize(part->draw_size + 2);
			else
				glPointSize(4.0);

			glLineWidth(3.0);

			draw_particle_arrays(draw_as, totpoint, ob_dt, 1);
		}

		/* restore from select */
		glColor3f(ma_r,ma_g,ma_b);
		glPointSize(part->draw_size ? part->draw_size : 2.0);
		glLineWidth(1.0);

		/* enable other data arrays */

		/* billboards are drawn this way */
		if(pdd->ndata && ob_dt>OB_WIRE){
			glEnableClientState(GL_NORMAL_ARRAY);
			glNormalPointer(GL_FLOAT, 0, pdd->ndata);
			glEnable(GL_LIGHTING);
		}
		else{
			glDisableClientState(GL_NORMAL_ARRAY);
			glDisable(GL_LIGHTING);
		}

		if(pdd->cdata){
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(3, GL_FLOAT, 0, pdd->cdata);
		}

		draw_particle_arrays(draw_as, totpoint, ob_dt, 0);

		pdd->flag |= PARTICLE_DRAW_DATA_UPDATED;
		pdd->totpoint = totpoint;
	}

	if(pdd && pdd->vedata){
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

	if(states)
		MEM_freeN(states);

	psys->flag &= ~PSYS_DRAWING;

	/* draw data can't be saved for billboards as they must update to target changes */
	if(draw_as == PART_DRAW_BB) {
		psys_free_pdd(psys);
		pdd->flag &= ~PARTICLE_DRAW_DATA_UPDATED;
	}

	if(psys->lattice){
		end_latt_deform(psys->lattice);
		psys->lattice= NULL;
	}

	if( (base->flag & OB_FROMDUPLI) && (ob->flag & OB_FROMGROUP) )
		glLoadMatrixf(rv3d->viewmat);
}

static void draw_update_ptcache_edit(Scene *scene, Object *ob, PTCacheEdit *edit)
{
	if(edit->psys && edit->psys->flag & PSYS_HAIR_UPDATED)
		PE_update_object(scene, ob, 0);

	/* create path and child path cache if it doesn't exist already */
	if(edit->pathcache==0)
		psys_cache_edit_paths(scene, ob, edit, CFRA);
}

static void draw_ptcache_edit(Scene *scene, View3D *v3d, RegionView3D *rv3d, Object *ob, PTCacheEdit *edit, int dt)
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

	if(edit->pathcache==0)
		return;

	PE_hide_keys_time(scene, edit, CFRA);

	/* opengl setup */
	if((v3d->flag & V3D_ZBUF_SELECT)==0)
		glDisable(GL_DEPTH_TEST);

	/* get selection theme colors */
	UI_GetThemeColor3fv(TH_VERTEX_SELECT, sel_col);
	UI_GetThemeColor3fv(TH_VERTEX, nosel_col);

	/* draw paths */
	if(timed) {
		glEnable(GL_BLEND);
		steps = (*edit->pathcache)->steps + 1;
		pathcol = MEM_callocN(steps*4*sizeof(float), "particle path color data");
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glShadeModel(GL_SMOOTH);

	if(pset->brushtype == PE_BRUSH_WEIGHT) {
		glLineWidth(2.0f);
		glDisable(GL_LIGHTING);
	}

	cache=edit->pathcache;
	for(i=0; i<totpoint; i++){
		path = cache[i];
		glVertexPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->co);

		if(timed) {
			for(k=0, pcol=pathcol, pkey=path; k<steps; k++, pkey++, pcol+=4){
				VECCOPY(pcol, pkey->col);
				pcol[3] = 1.0f - fabs((float)CFRA - pkey->time)/(float)pset->fade_frames;
			}

			glColorPointer(4, GL_FLOAT, 4*sizeof(float), pathcol);
		}
		else
			glColorPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->col);

		glDrawArrays(GL_LINE_STRIP, 0, path->steps + 1);
	}

	if(pathcol) { MEM_freeN(pathcol); pathcol = pcol = NULL; }


	/* draw edit vertices */
	if(pset->selectmode!=SCE_SELECT_PATH){
		glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));

		if(pset->selectmode==SCE_SELECT_POINT){
			float *pd=0,*pdata=0;
			float *cd=0,*cdata=0;
			int totkeys = 0;

			for (i=0, point=edit->points; i<totpoint; i++, point++)
				if(!(point->flag & PEP_HIDE))
					totkeys += point->totkey;

			if(edit->points && !(edit->points->keys->flag & PEK_USE_WCO))
				pd=pdata=MEM_callocN(totkeys*3*sizeof(float), "particle edit point data");
			cd=cdata=MEM_callocN(totkeys*(timed?4:3)*sizeof(float), "particle edit color data");

			for(i=0, point=edit->points; i<totpoint; i++, point++){
				if(point->flag & PEP_HIDE)
					continue;

				for(k=0, key=point->keys; k<point->totkey; k++, key++){
					if(pd) {
						VECCOPY(pd, key->co);
						pd += 3;
					}

					if(key->flag&PEK_SELECT){
						VECCOPY(cd,sel_col);
					}
					else{
						VECCOPY(cd,nosel_col);
					}

					if(timed)
						*(cd+3) = 1.0f - fabs((float)CFRA - *key->time)/(float)pset->fade_frames;

					cd += (timed?4:3);
				}
			}
			cd=cdata;
			pd=pdata;
			for(i=0, point=edit->points; i<totpoint; i++, point++){
				if(point->flag & PEP_HIDE)
					continue;

				if(point->keys->flag & PEK_USE_WCO)
					glVertexPointer(3, GL_FLOAT, sizeof(PTCacheEditKey), point->keys->world_co);
				else
					glVertexPointer(3, GL_FLOAT, 3*sizeof(float), pd);

				glColorPointer((timed?4:3), GL_FLOAT, (timed?4:3)*sizeof(float), cd);

				glDrawArrays(GL_POINTS, 0, point->totkey);

				pd += pd ? 3 * point->totkey : 0;
				cd += (timed?4:3) * point->totkey;
			}
			if(pdata) { MEM_freeN(pdata); pd=pdata=0; }
			if(cdata) { MEM_freeN(cdata); cd=cdata=0; }
		}
		else if(pset->selectmode == SCE_SELECT_END){
			for(i=0, point=edit->points; i<totpoint; i++, point++){
				if((point->flag & PEP_HIDE)==0){
					key = point->keys + point->totkey - 1;
					if(key->flag & PEK_SELECT)
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
	glEnable(GL_DEPTH_TEST);
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
	VECADD(root,root,com);
	glVertex3fv(root); 
	tip[1] = tip[2] = 0.0f;
	tip[0] = drw_size;
	mul_m3_v3(tr,tip);
	VECADD(tip,tip,com);
	glVertex3fv(tip); 
	glEnd();

	root[1] =0.0f; root[2] = tw;
	root[0] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	root[1] =0.0f; root[2] = -tw;
	root[0] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	root[1] =tw; root[2] = 0.0f;
	root[0] =th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	root[1] =-tw; root[2] = 0.0f;
	root[0] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	glColor4ub(0x00, 0x7F, 0x00, 155);

	glBegin(GL_LINES);
	root[0] = root[2] = 0.0f;
	root[1] = -drw_size;
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	tip[0] = tip[2] = 0.0f;
	tip[1] = drw_size;
	mul_m3_v3(tr,tip);
	VECADD(tip,tip,com);
	glVertex3fv(tip); 
	glEnd();

	root[0] =0.0f; root[2] = tw;
	root[1] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	root[0] =0.0f; root[2] = -tw;
	root[1] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	root[0] =tw; root[2] = 0.0f;
	root[1] =th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	root[0] =-tw; root[2] = 0.0f;
	root[1] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	glColor4ub(0x00, 0x00, 0x7F, 155);
	glBegin(GL_LINES);
	root[0] = root[1] = 0.0f;
	root[2] = -drw_size;
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	tip[0] = tip[1] = 0.0f;
	tip[2] = drw_size;
	mul_m3_v3(tr,tip);
	VECADD(tip,tip,com);
	glVertex3fv(tip); 
	glEnd();

	root[0] =0.0f; root[1] = tw;
	root[2] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	root[0] =0.0f; root[1] = -tw;
	root[2] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	root[0] = tw; root[1] = 0.0f;
	root[2] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();

	root[0] = -tw; root[1] = 0.0f;
	root[2] = th;
	glBegin(GL_LINES);
	mul_m3_v3(tr,root);
	VECADD(root,root,com);
	glVertex3fv(root); 
	glVertex3fv(tip); 
	glEnd();
}

/*place to add drawers */

static void tekenhandlesN(Nurb *nu, short sel, short hide_handles)
{
	BezTriple *bezt;
	float *fp;
	int basecol;
	int a;
	
	if(nu->hide || hide_handles) return;

	glBegin(GL_LINES); 
	
	if(nu->type == CU_BEZIER) {
		if(sel) basecol= TH_HANDLE_SEL_FREE;
		else basecol= TH_HANDLE_FREE;

		bezt= nu->bezt;
		a= nu->pntsu;
		while(a--) {
			if(bezt->hide==0) {
				if( (bezt->f2 & SELECT)==sel) {
					fp= bezt->vec[0];

					UI_ThemeColor(basecol + bezt->h1);
					glVertex3fv(fp);
					glVertex3fv(fp+3); 

					UI_ThemeColor(basecol + bezt->h2);
					glVertex3fv(fp+3); 
					glVertex3fv(fp+6); 
				}
				else if( (bezt->f1 & SELECT)==sel) {
					fp= bezt->vec[0];

					UI_ThemeColor(basecol + bezt->h1);
					glVertex3fv(fp); 
					glVertex3fv(fp+3); 
				}
				else if( (bezt->f3 & SELECT)==sel) {
					fp= bezt->vec[1];

					UI_ThemeColor(basecol + bezt->h2);
					glVertex3fv(fp); 
					glVertex3fv(fp+3); 
				}
			}
			bezt++;
		}
	}
	glEnd();
}

static void tekenhandlesN_active(Nurb *nu)
{
	BezTriple *bezt;
	float *fp;
	int a;

	if(nu->hide) return;

	UI_ThemeColor(TH_ACTIVE_SPLINE);
	glLineWidth(2);

	glBegin(GL_LINES);

	if(nu->type == CU_BEZIER) {
		bezt= nu->bezt;
		a= nu->pntsu;
		while(a--) {
			if(bezt->hide==0) {
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

	if(nu->hide) return;

	if(sel) color= TH_VERTEX_SELECT;
	else color= TH_VERTEX;

	UI_ThemeColor(color);

	size= UI_GetThemeValuef(TH_VERTEX_SIZE);
	glPointSize(size);
	
	bglBegin(GL_POINTS);
	
	if(nu->type == CU_BEZIER) {

		bezt= nu->bezt;
		a= nu->pntsu;
		while(a--) {
			if(bezt->hide==0) {
				if (bezt == lastsel) {
					UI_ThemeColor(TH_LASTSEL_POINT);
					bglVertex3fv(bezt->vec[1]);

					if (!hide_handles) {
						bglVertex3fv(bezt->vec[0]);
						bglVertex3fv(bezt->vec[2]);
					}

					UI_ThemeColor(color);
				} else if (hide_handles) {
					if((bezt->f2 & SELECT)==sel) bglVertex3fv(bezt->vec[1]);
				} else {
					if((bezt->f1 & SELECT)==sel) bglVertex3fv(bezt->vec[0]);
					if((bezt->f2 & SELECT)==sel) bglVertex3fv(bezt->vec[1]);
					if((bezt->f3 & SELECT)==sel) bglVertex3fv(bezt->vec[2]);
				}
			}
			bezt++;
		}
	}
	else {
		bp= nu->bp;
		a= nu->pntsu*nu->pntsv;
		while(a--) {
			if(bp->hide==0) {
				if (bp == lastsel) {
					UI_ThemeColor(TH_LASTSEL_POINT);
					bglVertex3fv(bp->vec);
					UI_ThemeColor(color);
				} else {
					if((bp->f1 & SELECT)==sel) bglVertex3fv(bp->vec);
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
	for(b=0; b<nu->pntsv; b++) {
		if(nu->flagu & 1) glBegin(GL_LINE_LOOP);
		else glBegin(GL_LINE_STRIP);

		for(a=0; a<nu->pntsu; a++, bp++) {
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
	for(b=0; b<nu->pntsv; b++) {
		bp1= bp;
		bp++;

		for(a=nu->pntsu-1; a>0; a--, bp++) {
			if(bp->hide==0 && bp1->hide==0) {
				glVertex3fv(bp->vec);
				glVertex3fv(bp1->vec);
			}
			bp1= bp;
		}
	}

	if(nu->pntsv > 1) {	/* surface */

		ofs= nu->pntsu;
		for(b=0; b<nu->pntsu; b++) {
			bp1= nu->bp+b;
			bp= bp1+ofs;
			for(a=nu->pntsv-1; a>0; a--, bp+=ofs) {
				if(bp->hide==0 && bp1->hide==0) {
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
	while(nu) {
		if(nu->hide==0) {
			switch(nu->type) {
			case CU_POLY:
				if (!sel && index== cu->actnu) {
					/* we should draw active spline highlight below everything */
					editnurb_draw_active_poly(nu);
				}

				UI_ThemeColor(TH_NURB_ULINE);
				bp= nu->bp;
				for(b=0; b<nu->pntsv; b++) {
					if(nu->flagu & 1) glBegin(GL_LINE_LOOP);
					else glBegin(GL_LINE_STRIP);

					for(a=0; a<nu->pntsu; a++, bp++) {
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
				for(b=0; b<nu->pntsv; b++) {
					bp1= bp;
					bp++;
					for(a=nu->pntsu-1; a>0; a--, bp++) {
						if(bp->hide==0 && bp1->hide==0) {
							if(sel) {
								if( (bp->f1 & SELECT) && ( bp1->f1 & SELECT ) ) {
									UI_ThemeColor(TH_NURB_SEL_ULINE);
		
									glBegin(GL_LINE_STRIP);
									glVertex3fv(bp->vec); 
									glVertex3fv(bp1->vec);
									glEnd();
								}
							}
							else {
								if( (bp->f1 & SELECT) && ( bp1->f1 & SELECT) );
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
				if(nu->pntsv > 1) {	/* surface */

					ofs= nu->pntsu;
					for(b=0; b<nu->pntsu; b++) {
						bp1= nu->bp+b;
						bp= bp1+ofs;
						for(a=nu->pntsv-1; a>0; a--, bp+=ofs) {
							if(bp->hide==0 && bp1->hide==0) {
								if(sel) {
									if( (bp->f1 & SELECT) && ( bp1->f1 & SELECT) ) {
										UI_ThemeColor(TH_NURB_SEL_VLINE);
			
										glBegin(GL_LINE_STRIP);
										glVertex3fv(bp->vec); 
										glVertex3fv(bp1->vec);
										glEnd();
									}
								}
								else {
									if( (bp->f1 & SELECT) && ( bp1->f1 & SELECT) );
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

// XXX	retopo_matrix_update(v3d);

	/* DispList */
	UI_ThemeColor(TH_WIRE);
	drawDispList(scene, v3d, rv3d, base, dt);

	if(v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	/* first non-selected and active handles */
	index= 0;
	for(nu=nurb; nu; nu=nu->next) {
		if(nu->type == CU_BEZIER) {
			if (index == cu->actnu && !hide_handles)
				tekenhandlesN_active(nu);
			tekenhandlesN(nu, 0, hide_handles);
		}
		++index;
	}
	draw_editnurb(ob, nurb, 0);
	draw_editnurb(ob, nurb, 1);
	/* selected handles */
	for(nu=nurb; nu; nu=nu->next) {
		if(nu->type == CU_BEZIER && (cu->drawflag & CU_HIDE_HANDLES)==0)
			tekenhandlesN(nu, 1, hide_handles);
		tekenvertsN(nu, 0, hide_handles, NULL);
	}
	
	if(v3d->zbuf) glEnable(GL_DEPTH_TEST);

	/*	direction vectors for 3d curve paths
		when at its lowest, dont render normals */
	if(cu->flag & CU_3D && ts->normalsize > 0.0015 && (cu->drawflag & CU_HIDE_NORMALS)==0) {

		UI_ThemeColor(TH_WIRE);
		for (bl=cu->bev.first,nu=nurb; nu && bl; bl=bl->next,nu=nu->next) {
			BevPoint *bevp= (BevPoint *)(bl+1);		
			int nr= bl->nr;
			int skip= nu->resolu/16;
			
			while (nr-->0) { /* accounts for empty bevel lists */
				float fac= bevp->radius * ts->normalsize;
				float vec_a[3] = { fac,0, 0}; // Offset perpendicular to the curve
				float vec_b[3] = {-fac,0, 0}; // Delta along the curve

				mul_qt_v3(bevp->quat, vec_a);
				mul_qt_v3(bevp->quat, vec_b);
				add_v3_v3(vec_a, bevp->vec);
				add_v3_v3(vec_b, bevp->vec);
				
				VECSUBFAC(vec_a, vec_a, bevp->dir, fac);
				VECSUBFAC(vec_b, vec_b, bevp->dir, fac);

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

	if(v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	for(nu=nurb; nu; nu=nu->next) {
		tekenvertsN(nu, 1, hide_handles, cu->lastsel);
	}
	
	if(v3d->zbuf) glEnable(GL_DEPTH_TEST); 
}

/* draw a sphere for use as an empty drawtype */
static void draw_empty_sphere (float size)
{
	static GLuint displist=0;
	
	if (displist == 0) {
		GLUquadricObj	*qobj;
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE_AND_EXECUTE);
		
		glPushMatrix();
		
		qobj	= gluNewQuadric(); 
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
	glScalef(1/size, 1/size, 1/size);
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
	glScalef(radius, 2.0*size, radius);
	glRotatef(-90., 1.0, 0.0, 0.0);
	gluCylinder(qobj, 1.0, 0.0, 1.0, 8, 1);

	glPopMatrix();
	
	gluDeleteQuadric(qobj); 
}

/* draw points on curve speed handles */
static void curve_draw_speed(Scene *scene, Object *ob)
{
#if 0 // XXX old animation system stuff
	Curve *cu= ob->data;
	IpoCurve *icu;
	BezTriple *bezt;
	float loc[4], dir[3];
	int a;
	
	if(cu->ipo==NULL)
		return;
	
	icu= cu->ipo->curve.first; 
	if(icu==NULL || icu->totvert<2)
		return;
	
	glPointSize( UI_GetThemeValuef(TH_VERTEX_SIZE) );
	bglBegin(GL_POINTS);

	for(a=0, bezt= icu->bezt; a<icu->totvert; a++, bezt++) {
		if( where_on_path(ob, bezt->vec[1][1], loc, dir)) {
			UI_ThemeColor((bezt->f2 & SELECT) && ob==OBACT?TH_VERTEX_SELECT:TH_VERTEX);
			bglVertex3fv(loc);
		}
	}

	glPointSize(1.0);
	bglEnd();
#endif // XXX old animation system stuff
}


static void draw_textcurs(float textcurs[][2])
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

static void drawspiral(float *cent, float rad, float tmat[][4], int start)
{
	float vec[3], vx[3], vy[3];
	int a, tot=32;
	char inverse=0;
		
	if (start < 0) {
		inverse = 1;
		start *= -1;
	}

	VECCOPY(vx, tmat[0]);
	VECCOPY(vy, tmat[1]);
	mul_v3_fl(vx, rad);
	mul_v3_fl(vy, rad);

	VECCOPY(vec, cent);

	if (inverse==0) {
		for(a=0; a<tot; a++) {
			if (a+start>31)
				start=-a + 1;
			glBegin(GL_LINES);							
			glVertex3fv(vec);
			vec[0]= cent[0] + *(sinval+a+start) * (vx[0] * (float)a/(float)tot) + *(cosval+a+start) * (vy[0] * (float)a/(float)tot);
			vec[1]= cent[1] + *(sinval+a+start) * (vx[1] * (float)a/(float)tot) + *(cosval+a+start) * (vy[1] * (float)a/(float)tot);
			vec[2]= cent[2] + *(sinval+a+start) * (vx[2] * (float)a/(float)tot) + *(cosval+a+start) * (vy[2] * (float)a/(float)tot);
			glVertex3fv(vec);
			glEnd();
		}
	}
	else {
		a=0;
		vec[0]= cent[0] + *(sinval+a+start) * (vx[0] * (float)(-a+31)/(float)tot) + *(cosval+a+start) * (vy[0] * (float)(-a+31)/(float)tot);
		vec[1]= cent[1] + *(sinval+a+start) * (vx[1] * (float)(-a+31)/(float)tot) + *(cosval+a+start) * (vy[1] * (float)(-a+31)/(float)tot);
		vec[2]= cent[2] + *(sinval+a+start) * (vx[2] * (float)(-a+31)/(float)tot) + *(cosval+a+start) * (vy[2] * (float)(-a+31)/(float)tot);
		for(a=0; a<tot; a++) {
			if (a+start>31)
				start=-a + 1;
			glBegin(GL_LINES);							
			glVertex3fv(vec);
			vec[0]= cent[0] + *(sinval+a+start) * (vx[0] * (float)(-a+31)/(float)tot) + *(cosval+a+start) * (vy[0] * (float)(-a+31)/(float)tot);
			vec[1]= cent[1] + *(sinval+a+start) * (vx[1] * (float)(-a+31)/(float)tot) + *(cosval+a+start) * (vy[1] * (float)(-a+31)/(float)tot);
			vec[2]= cent[2] + *(sinval+a+start) * (vx[2] * (float)(-a+31)/(float)tot) + *(cosval+a+start) * (vy[2] * (float)(-a+31)/(float)tot);
			glVertex3fv(vec);
			glEnd();
		}
	}
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
	for (degrees=0; degrees<32; degrees++) {
		x= *(cosval + degrees);
		y= *(sinval + degrees);
		
		glVertex3f(x*size, 0.0f, y*size);
	}
	
	glEnd();

}

/* needs fixing if non-identity matrice used */
static void drawtube(float *vec, float radius, float height, float tmat[][4])
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
static void drawcone(float *vec, float radius, float height, float tmat[][4])
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

	if(mb->editelems) {
		UI_ThemeColor(TH_WIRE);
		if((G.f & G_PICKSEL)==0 ) drawDispList(scene, v3d, rv3d, base, dt);
		ml= mb->editelems->first;
	}
	else {
		if((base->flag & OB_FROMDUPLI)==0) 
			drawDispList(scene, v3d, rv3d, base, dt);
		ml= mb->elems.first;
	}

	if(ml==NULL) return 1;
	
	/* in case solid draw, reset wire colors */
	if(ob->flag & SELECT) {
		if(ob==OBACT) UI_ThemeColor(TH_ACTIVE);
		else UI_ThemeColor(TH_SELECT);
	}
	else UI_ThemeColor(TH_WIRE);

	invert_m4_m4(imat, rv3d->viewmatob);
	normalize_v3(imat[0]);
	normalize_v3(imat[1]);
	
	while(ml) {
	
		/* draw radius */
		if(mb->editelems) {
			if((ml->flag & SELECT) && (ml->flag & MB_SCALE_RAD)) cpack(0xA0A0F0);
			else cpack(0x3030A0);
			
			if(G.f & G_PICKSEL) {
				ml->selcol1= code;
				glLoadName(code++);
			}
		}
		drawcircball(GL_LINE_LOOP, &(ml->x), ml->rad, imat);

		/* draw stiffness */
		if(mb->editelems) {
			if((ml->flag & SELECT) && !(ml->flag & MB_SCALE_RAD)) cpack(0xA0F0A0);
			else cpack(0x30A030);
			
			if(G.f & G_PICKSEL) {
				ml->selcol2= code;
				glLoadName(code++);
			}
			drawcircball(GL_LINE_LOOP, &(ml->x), ml->rad*atan(ml->s)/M_PI_2, imat);
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
	if(ob!=scene->obedit && (ob->flag & SELECT)) {
		if(ob==OBACT) curcol= TH_ACTIVE;
		else curcol= TH_SELECT;
	}
	else curcol= TH_WIRE;
	
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
			force_val = pd->f_strength;
		force_val*= 0.1;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2]= 0.5*force_val;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2]= 1.0*force_val;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2]= 1.5*force_val;
		drawcircball(GL_LINE_LOOP, vec, size, tmat);
		vec[2] = 0; /* reset vec for max dist circle */
		
	}
	else if (pd->forcefield == PFIELD_FORCE) {
		float ffall_val;

		//if (has_ipo_code(ob->ipo, OB_PD_FFALL)) 
		//	ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, scene->r.cfra);
		//else 
			ffall_val = pd->f_power;

		UI_ThemeColorBlend(curcol, TH_BACK, 0.5);
		drawcircball(GL_LINE_LOOP, vec, size, imat);
		UI_ThemeColorBlend(curcol, TH_BACK, 0.9 - 0.4 / pow(1.5, (double)ffall_val));
		drawcircball(GL_LINE_LOOP, vec, size*1.5, imat);
		UI_ThemeColorBlend(curcol, TH_BACK, 0.9 - 0.4 / pow(2.0, (double)ffall_val));
		drawcircball(GL_LINE_LOOP, vec, size*2.0, imat);
	}
	else if (pd->forcefield == PFIELD_VORTEX) {
		float ffall_val, force_val;

		unit_m4(tmat);
		//if (has_ipo_code(ob->ipo, OB_PD_FFALL)) 
		//	ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, scene->r.cfra);
		//else 
			ffall_val = pd->f_power;

		//if (has_ipo_code(ob->ipo, OB_PD_FSTR))
		//	force_val = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, scene->r.cfra);
		//else 
			force_val = pd->f_strength;

		UI_ThemeColorBlend(curcol, TH_BACK, 0.7);
		if (force_val < 0) {
			drawspiral(vec, size*1.0, tmat, 1);
			drawspiral(vec, size*1.0, tmat, 16);
		}
		else {
			drawspiral(vec, size*1.0, tmat, -1);
			drawspiral(vec, size*1.0, tmat, -16);
		}
	}
	else if (pd->forcefield == PFIELD_GUIDE && ob->type==OB_CURVE) {
		Curve *cu= ob->data;
		if((cu->flag & CU_PATH) && cu->path && cu->path->data) {
			float mindist, guidevec1[4], guidevec2[3];

			//if (has_ipo_code(ob->ipo, OB_PD_FSTR))
			//	mindist = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, scene->r.cfra);
			//else 
				mindist = pd->f_strength;

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
			
			VECCOPY(vec, guidevec1);	/* max center */
		}
	}

	setlinestyle(3);
	UI_ThemeColorBlend(curcol, TH_BACK, 0.5);

	if(pd->falloff==PFIELD_FALL_SPHERE){
		/* as last, guide curve alters it */
		if(pd->flag & PFIELD_USEMAX)
			drawcircball(GL_LINE_LOOP, vec, pd->maxdist, imat);		

		if(pd->flag & PFIELD_USEMIN)
			drawcircball(GL_LINE_LOOP, vec, pd->mindist, imat);
	}
	else if(pd->falloff==PFIELD_FALL_TUBE){
		float radius,distance;

		unit_m4(tmat);

		vec[0]=vec[1]=0.0f;
		radius=(pd->flag&PFIELD_USEMAXR)?pd->maxrad:1.0f;
		distance=(pd->flag&PFIELD_USEMAX)?pd->maxdist:0.0f;
		vec[2]=distance;
		distance=(pd->flag&PFIELD_POSZ)?-distance:-2.0f*distance;

		if(pd->flag & (PFIELD_USEMAX|PFIELD_USEMAXR))
			drawtube(vec,radius,distance,tmat);

		radius=(pd->flag&PFIELD_USEMINR)?pd->minrad:1.0f;
		distance=(pd->flag&PFIELD_USEMIN)?pd->mindist:0.0f;
		vec[2]=distance;
		distance=(pd->flag&PFIELD_POSZ)?-distance:-2.0f*distance;

		if(pd->flag & (PFIELD_USEMIN|PFIELD_USEMINR))
			drawtube(vec,radius,distance,tmat);
	}
	else if(pd->falloff==PFIELD_FALL_CONE){
		float radius,distance;

		unit_m4(tmat);

		radius=(pd->flag&PFIELD_USEMAXR)?pd->maxrad:1.0f;
		radius*=(float)M_PI/180.0f;
		distance=(pd->flag&PFIELD_USEMAX)?pd->maxdist:0.0f;

		if(pd->flag & (PFIELD_USEMAX|PFIELD_USEMAXR)){
			drawcone(vec,distance*sin(radius),distance*cos(radius),tmat);
			if((pd->flag & PFIELD_POSZ)==0)
				drawcone(vec,distance*sin(radius),-distance*cos(radius),tmat);
		}

		radius=(pd->flag&PFIELD_USEMINR)?pd->minrad:1.0f;
		radius*=(float)M_PI/180.0f;
		distance=(pd->flag&PFIELD_USEMIN)?pd->mindist:0.0f;

		if(pd->flag & (PFIELD_USEMIN|PFIELD_USEMINR)){
			drawcone(vec,distance*sin(radius),distance*cos(radius),tmat);
			if((pd->flag & PFIELD_POSZ)==0)
				drawcone(vec,distance*sin(radius),-distance*cos(radius),tmat);
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
void get_local_bounds(Object *ob, float *center, float *size)
{
	BoundBox *bb= object_get_boundbox(ob);
	
	if(bb==NULL) {
		center[0]= center[1]= center[2]= 0.0;
		VECCOPY(size, ob->size);
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



static void draw_bb_quadric(BoundBox *bb, short type)
{
	float size[3], cent[3];
	GLUquadricObj *qobj = gluNewQuadric(); 
	
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
	
	size[0]= 0.5*fabs(bb->vec[0][0] - bb->vec[4][0]);
	size[1]= 0.5*fabs(bb->vec[0][1] - bb->vec[2][1]);
	size[2]= 0.5*fabs(bb->vec[0][2] - bb->vec[1][2]);
	
	cent[0]= (bb->vec[0][0] + bb->vec[4][0])/2.0;
	cent[1]= (bb->vec[0][1] + bb->vec[2][1])/2.0;
	cent[2]= (bb->vec[0][2] + bb->vec[1][2])/2.0;
	
	glPushMatrix();
	if(type==OB_BOUND_SPHERE) {
		glTranslatef(cent[0], cent[1], cent[2]);
		glScalef(size[0], size[1], size[2]);
		gluSphere(qobj, 1.0, 8, 5);
	}	
	else if(type==OB_BOUND_CYLINDER) {
		float radius = size[0] > size[1] ? size[0] : size[1];
		glTranslatef(cent[0], cent[1], cent[2]-size[2]);
		glScalef(radius, radius, 2.0*size[2]);
		gluCylinder(qobj, 1.0, 1.0, 1.0, 8, 1);
	}
	else if(type==OB_BOUND_CONE) {
		float radius = size[0] > size[1] ? size[0] : size[1];
		glTranslatef(cent[0], cent[2]-size[2], cent[1]);
		glScalef(radius, 2.0*size[2], radius);
		glRotatef(-90., 1.0, 0.0, 0.0);
		gluCylinder(qobj, 1.0, 0.0, 1.0, 8, 1);
	}
	glPopMatrix();
	
	gluDeleteQuadric(qobj); 
}

static void draw_bounding_volume(Scene *scene, Object *ob)
{
	BoundBox *bb=0;
	
	if(ob->type==OB_MESH) {
		bb= mesh_get_bb(ob);
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		bb= ob->bb ? ob->bb : ( (Curve *)ob->data )->bb;
	}
	else if(ob->type==OB_MBALL) {
		bb= ob->bb;
		if(bb==0) {
			makeDispListMBall(scene, ob);
			bb= ob->bb;
		}
	}
	else {
		drawcube();
		return;
	}
	
	if(bb==0) return;
	
	if(ob->boundtype==OB_BOUND_BOX) draw_box(bb->vec);
	else draw_bb_quadric(bb, ob->boundtype);
	
}

static void drawtexspace(Object *ob)
{
	float vec[8][3], loc[3], size[3];
	
	if(ob->type==OB_MESH) {
		mesh_get_texspace(ob->data, loc, NULL, size);
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		Curve *cu= ob->data;
		VECCOPY(size, cu->size);
		VECCOPY(loc, cu->loc);
	}
	else if(ob->type==OB_MBALL) {
		MetaBall *mb= ob->data;
		VECCOPY(size, mb->size);
		VECCOPY(loc, mb->loc);
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
static void drawSolidSelect(Scene *scene, View3D *v3d, ARegion *ar, Base *base) 
{
	RegionView3D *rv3d= ar->regiondata;
	Object *ob= base->object;
	
	glLineWidth(2.0);
	glDepthMask(0);
	
	if(ELEM3(ob->type, OB_FONT,OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		DerivedMesh *dm = ob->derivedFinal;
		int hasfaces= 0;

		if (dm) {
			hasfaces= dm->getNumFaces(dm);
		} else {
			hasfaces= displist_has_faces(&cu->disp);
		}

		if (hasfaces && boundbox_clip(rv3d, ob->obmat, ob->bb ? ob->bb : cu->bb)) {
			draw_index_wire= 0;
			if (dm) {
				draw_mesh_object_outline(v3d, ob, dm);
			} else {
				drawDispListwire(&cu->disp);
			}
			draw_index_wire= 1;
		}
	} else if (ob->type==OB_MBALL) {
		if((base->flag & OB_FROMDUPLI)==0) 
			drawDispListwire(&ob->disp);
	}
	else if(ob->type==OB_ARMATURE) {
		if(!(ob->mode & OB_MODE_POSE))
			draw_armature(scene, v3d, ar, base, OB_WIRE, 0);
	}

	glLineWidth(1.0);
	glDepthMask(1);
}

static void drawWireExtra(Scene *scene, RegionView3D *rv3d, Object *ob) 
{
	if(ob!=scene->obedit && (ob->flag & SELECT)) {
		if(ob==OBACT) {
			if(ob->flag & OB_FROMGROUP) UI_ThemeColor(TH_GROUP_ACTIVE);
			else UI_ThemeColor(TH_ACTIVE);
		}
		else if(ob->flag & OB_FROMGROUP)
			UI_ThemeColorShade(TH_GROUP_ACTIVE, -16);
		else
			UI_ThemeColor(TH_SELECT);
	}
	else {
		if(ob->flag & OB_FROMGROUP)
			UI_ThemeColor(TH_GROUP);
		else {
			if(ob->dtx & OB_DRAWWIRE) {
				glColor3ub(80,80,80);
			} else {
				UI_ThemeColor(TH_WIRE);
			}
		}
	}
	
	bglPolygonOffset(rv3d->dist, 1.0);
	glDepthMask(0);	// disable write in zbuffer, selected edge wires show better
	
	if (ELEM3(ob->type, OB_FONT, OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		if (boundbox_clip(rv3d, ob->obmat, ob->bb ? ob->bb : cu->bb)) {
			if (ob->type==OB_CURVE)
				draw_index_wire= 0;

			if (ob->derivedFinal) {
				drawCurveDMWired(ob);
			} else {
				drawDispListwire(&cu->disp);
			}

			if (ob->type==OB_CURVE)
				draw_index_wire= 1;
		}
	} else if (ob->type==OB_MBALL) {
		drawDispListwire(&ob->disp);
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

			if(hmd->object) {
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

//<rcruiz>
void drawRBpivot(bRigidBodyJointConstraint *data)
{
	float radsPerDeg = 6.283185307179586232f / 360.f;
	int axis;
	float v1[3]= {data->pivX, data->pivY, data->pivZ};
	float eu[3]= {radsPerDeg*data->axX, radsPerDeg*data->axY, radsPerDeg*data->axZ};
	float mat[4][4];

	eul_to_mat4(mat,eu);
	glLineWidth (4.0f);
	setlinestyle(2);
	for (axis=0; axis<3; axis++) {
		float dir[3] = {0,0,0};
		float v[3]= {data->pivX, data->pivY, data->pivZ};

		dir[axis] = 1.f;
		glBegin(GL_LINES);
		mul_m4_v3(mat,dir);
		v[0] += dir[0];
		v[1] += dir[1];
		v[2] += dir[2];
		glVertex3fv(v1);
		glVertex3fv(v);			
		glEnd();
		if (axis==0)
			view3d_cached_text_draw_add(v[0], v[1], v[2], "px", 0, 0);
		else if (axis==1)
			view3d_cached_text_draw_add(v[0], v[1], v[2], "py", 0, 0);
		else
			view3d_cached_text_draw_add(v[0], v[1], v[2], "pz", 0, 0);
	}
	glLineWidth (1.0f);
	setlinestyle(0);
}

/* flag can be DRAW_PICKING	and/or DRAW_CONSTCOLOR, DRAW_SCENESET */
void draw_object(Scene *scene, ARegion *ar, View3D *v3d, Base *base, int flag)
{
	static int warning_recursive= 0;
	ModifierData *md = NULL;
	Object *ob;
	Curve *cu;
	RegionView3D *rv3d= ar->regiondata;
	//float cfraont;
	float vec1[3], vec2[3];
	unsigned int col=0;
	int /*sel, drawtype,*/ colindex= 0;
	int i, selstart, selend, empty_object=0;
	short dt, dtx, zbufoff= 0;

	/* only once set now, will be removed too, should become a global standard */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	ob= base->object;

	if (ob!=scene->obedit) {
		if (ob->restrictflag & OB_RESTRICT_VIEW) 
			return;
	}

	/* XXX particles are not safe for simultaneous threaded render */
	if(G.rendering && ob->particlesystem.first)
		return;

	/* xray delay? */
	if((flag & DRAW_PICKING)==0 && (base->flag & OB_FROMDUPLI)==0) {
		/* don't do xray in particle mode, need the z-buffer */
		if(!(ob->mode & OB_MODE_PARTICLE_EDIT)) {
			/* xray and transp are set when it is drawing the 2nd/3rd pass */
			if(!v3d->xray && !v3d->transp && (ob->dtx & OB_DRAWXRAY) && !(ob->dtx & OB_DRAWTRANSP)) {
				add_view3d_after(v3d, base, V3D_XRAY, flag);
				return;
			}
		}
	}

	/* no return after this point, otherwise leaks */
	view3d_cached_text_draw_begin();
	

	/* draw keys? */
#if 0 // XXX old animation system
	if(base==(scene->basact) || (base->flag & (SELECT+BA_WAS_SEL))) {
		if(flag==0 && warning_recursive==0 && ob!=scene->obedit) {
			if(ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
				ListBase elems;
				CfraElem *ce;
				float temp[7][3];

				warning_recursive= 1;

				elems.first= elems.last= 0;
				// warning: no longer checks for certain ob-keys only... (so does this need to use the proper ipokeys then?)
				make_cfra_list(ob->ipo, &elems); 

				cfraont= (scene->r.cfra);
				drawtype= v3d->drawtype;
				if(drawtype>OB_WIRE) v3d->drawtype= OB_WIRE;
				sel= base->flag;
				memcpy(temp, &ob->loc, 7*3*sizeof(float));

				ipoflag= ob->ipoflag;
				ob->ipoflag &= ~OB_OFFS_OB;

				set_no_parent_ipo(1);
				disable_speed_curve(1);

				if ((ob->ipoflag & OB_DRAWKEYSEL)==0) {
					ce= elems.first;
					while(ce) {
						if(!ce->sel) {
							(scene->r.cfra)= ce->cfra/scene->r.framelen;

							base->flag= 0;

							where_is_object_time(scene, ob, (scene->r.cfra));
							draw_object(scene, ar, v3d, base, 0);
						}
						ce= ce->next;
					}
				}

				ce= elems.first;
				while(ce) {
					if(ce->sel) {
						(scene->r.cfra)= ce->cfra/scene->r.framelen;

						base->flag= SELECT;

						where_is_object_time(scene, ob, (scene->r.cfra));
						draw_object(scene, ar, v3d, base, 0);
					}
					ce= ce->next;
				}

				set_no_parent_ipo(0);
				disable_speed_curve(0);

				base->flag= sel;
				ob->ipoflag= ipoflag;

				/* restore icu->curval */
				(scene->r.cfra)= cfraont;

				memcpy(&ob->loc, temp, 7*3*sizeof(float));
				where_is_object(scene, ob);
				v3d->drawtype= drawtype;

				BLI_freelistN(&elems);

				warning_recursive= 0;
			}
		}
	}
#endif // XXX old animation system

	/* patch? children objects with a timeoffs change the parents. How to solve! */
	/* if( ((int)ob->ctime) != F_(scene->r.cfra)) where_is_object(scene, ob); */
	
	/* draw motion paths (in view space) */
	if (ob->mpath) {
		bAnimVizSettings *avs= &ob->avs;
		
		/* setup drawing environment for paths */
		draw_motion_paths_init(scene, v3d, ar);
		
		/* draw motion path for object */
		draw_motion_path_instance(scene, v3d, ar, ob, NULL, avs, ob->mpath);
		
		/* cleanup after drawing */
		draw_motion_paths_cleanup(scene, v3d, ar);
	}

	/* multiply view with object matrix.
	 * local viewmat and persmat, to calculate projections */
	ED_view3d_init_mats_rv3d(ob, rv3d);

	/* which wire color */
	if((flag & DRAW_CONSTCOLOR) == 0) {
		project_short(ar, ob->obmat[3], &base->sx);

		if( (!scene->obedit) && (G.moving & G_TRANSFORM_OBJ) && (base->flag & (SELECT+BA_WAS_SEL))) UI_ThemeColor(TH_TRANSFORM);
		else {

			if(ob->type==OB_LAMP) UI_ThemeColor(TH_LAMP);
			else UI_ThemeColor(TH_WIRE);

			if((scene->basact)==base) {
				if(base->flag & (SELECT+BA_WAS_SEL)) UI_ThemeColor(TH_ACTIVE);
			}
			else {
				if(base->flag & (SELECT+BA_WAS_SEL)) UI_ThemeColor(TH_SELECT);
			}

			// no theme yet
			if(ob->id.lib) {
				if(base->flag & (SELECT+BA_WAS_SEL)) colindex = 4;
				else colindex = 3;
			}
			else if(warning_recursive==1) {
				if(base->flag & (SELECT+BA_WAS_SEL)) {
					if(scene->basact==base) colindex = 8;
					else colindex= 7;
				}
				else colindex = 6;
			}
			else if(ob->flag & OB_FROMGROUP) {
				if(base->flag & (SELECT+BA_WAS_SEL)) {
					if(scene->basact==base) UI_ThemeColor(TH_GROUP_ACTIVE);
					else UI_ThemeColorShade(TH_GROUP_ACTIVE, -16); 
				}
				else UI_ThemeColor(TH_GROUP);
				colindex= 0;
			}

		}	

		if(colindex) {
			col= colortab[colindex];
			cpack(col);
		}
	}

	/* maximum drawtype */
	dt= MIN2(v3d->drawtype, ob->dt);
	if(v3d->zbuf==0 && dt>OB_WIRE) dt= OB_WIRE;
	dtx= 0;

	/* faceselect exception: also draw solid when dt==wire, except in editmode */
	if(ob==OBACT && (ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT))) {
		if(ob->type==OB_MESH) {

			if(ob->mode & OB_MODE_EDIT);
			else {
				if(dt<OB_SOLID) {
					zbufoff= 1;
					dt= OB_SOLID;
				}
				else {
					dt= OB_SHADED;
				}

				glEnable(GL_DEPTH_TEST);
			}
		}
		else {
			if(dt<OB_SOLID) {
				dt= OB_SOLID;
				glEnable(GL_DEPTH_TEST);
				zbufoff= 1;
			}
		}
	}
	
	/* draw-extra supported for boundbox drawmode too */
	if(dt>=OB_BOUNDBOX ) {

		dtx= ob->dtx;
		if(ob->mode & OB_MODE_EDIT) {
			// the only 2 extra drawtypes alowed in editmode
			dtx= dtx & (OB_DRAWWIRE|OB_TEXSPACE);
		}

	}

	/* bad exception, solve this! otherwise outline shows too late */
	if(ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		cu= ob->data;
		/* still needed for curves hidden in other layers. depgraph doesnt handle that yet */
		if (cu->disp.first==NULL) makeDispListCurveTypes(scene, ob, 0);
	}
	
	/* draw outline for selected solid objects, mesh does itself */
	if((v3d->flag & V3D_SELECT_OUTLINE) && ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) && ob->type!=OB_MESH) {
		if(dt>OB_WIRE && dt<OB_TEXTURE && (ob->mode & OB_MODE_EDIT)==0 && (flag & DRAW_SCENESET)==0) {
			if (!(ob->dtx&OB_DRAWWIRE) && (ob->flag&SELECT) && !(flag&DRAW_PICKING)) {
				
				drawSolidSelect(scene, v3d, ar, base);
			}
		}
	}

	switch( ob->type) {
		case OB_MESH:
			empty_object= draw_mesh_object(scene, ar, v3d, rv3d, base, dt, flag);
			if(flag!=DRAW_CONSTCOLOR) dtx &= ~OB_DRAWWIRE; // mesh draws wire itself

			break;
		case OB_FONT:
			cu= ob->data;
			if(cu->editfont) {
				draw_textcurs(cu->editfont->textcurs);

				if (cu->flag & CU_FAST) {
					cpack(0xFFFFFF);
					set_inverted_drawing(1);
					drawDispList(scene, v3d, rv3d, base, OB_WIRE);
					set_inverted_drawing(0);
				} else {
					drawDispList(scene, v3d, rv3d, base, dt);
				}

				if (cu->linewidth != 0.0) {
					cpack(0xff44ff);
					UI_ThemeColor(TH_WIRE);
					VECCOPY(vec1, ob->orig);
					VECCOPY(vec2, ob->orig);
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
					if (cu->tb[i].w != 0.0) {
						if (i == (cu->actbox-1))
							UI_ThemeColor(TH_ACTIVE);
						else
							UI_ThemeColor(TH_WIRE);
						vec1[0] = cu->tb[i].x;
						vec1[1] = cu->tb[i].y + cu->fsize;
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
			else if(dt==OB_BOUNDBOX) {
				if((v3d->flag2 & V3D_RENDER_OVERRIDE && v3d->drawtype >= OB_WIRE)==0)
					draw_bounding_volume(scene, ob);
			}
			else if(boundbox_clip(rv3d, ob->obmat, ob->bb ? ob->bb : cu->bb))
				empty_object= drawDispList(scene, v3d, rv3d, base, dt);

			break;
		case OB_CURVE:
		case OB_SURF:
			cu= ob->data;

			if(cu->editnurb) {
				drawnurb(scene, v3d, rv3d, base, cu->editnurb->first, dt);
			}
			else if(dt==OB_BOUNDBOX) {
				if((v3d->flag2 & V3D_RENDER_OVERRIDE && v3d->drawtype >= OB_WIRE)==0)
					draw_bounding_volume(scene, ob);
			}
			else if(boundbox_clip(rv3d, ob->obmat, ob->bb ? ob->bb : cu->bb)) {
				empty_object= drawDispList(scene, v3d, rv3d, base, dt);
				
				if(cu->path)
					curve_draw_speed(scene, ob);
			}
			break;
		case OB_MBALL:
		{
			MetaBall *mb= ob->data;
			
			if(mb->editelems) 
				drawmball(scene, v3d, rv3d, base, dt);
			else if(dt==OB_BOUNDBOX) {
				if((v3d->flag2 & V3D_RENDER_OVERRIDE && v3d->drawtype >= OB_WIRE)==0)
					draw_bounding_volume(scene, ob);
			}
			else 
				empty_object= drawmball(scene, v3d, rv3d, base, dt);
			break;
		}
		case OB_EMPTY:
			if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0)
				drawaxes(rv3d, rv3d->viewmatob, ob->empty_drawsize, flag, ob->empty_drawtype);
			break;
		case OB_LAMP:
			if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
				drawlamp(scene, v3d, rv3d, base, dt, flag);
				if(dtx || (base->flag & SELECT)) glMultMatrixf(ob->obmat);
			}
			break;
		case OB_CAMERA:
			if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0 || (rv3d->persp==RV3D_CAMOB && v3d->camera==ob)) /* special exception for active camera */
				drawcamera(scene, v3d, rv3d, ob, flag);
			break;
		case OB_LATTICE:
			if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
				drawlattice(scene, v3d, ob);
			}
			break;
		case OB_ARMATURE:
			if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
				if(dt>OB_WIRE) GPU_enable_material(0, NULL); // we use default material
				empty_object= draw_armature(scene, v3d, ar, base, dt, flag);
				if(dt>OB_WIRE) GPU_disable_material();
			}
			break;
		default:
			if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
				drawaxes(rv3d, rv3d->viewmatob, 1.0, flag, OB_ARROWS);
			}
	}

	if((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {

		if(ob->soft /*&& flag & OB_SBMOTION*/){
			float mrt[3][3],msc[3][3],mtr[3][3]; 
			SoftBody *sb = 0;
			float tipw = 0.5f, tiph = 0.5f,drawsize = 4.0f;
			if ((sb= ob->soft)){
				if(sb->solverflags & SBSO_ESTIMATEIPO){

					glLoadMatrixf(rv3d->viewmat);
					copy_m3_m3(msc,sb->lscale);
					copy_m3_m3(mrt,sb->lrot);
					mul_m3_m3m3(mtr,mrt,msc); 
					ob_draw_RE_motion(sb->lcom,mtr,tipw,tiph,drawsize);
					glMultMatrixf(ob->obmat);
				}
			}
		}

		if(ob->pd && ob->pd->forcefield) {
			draw_forcefield(scene, ob, rv3d);
		}
	}

	/* code for new particle system */
	if(		(warning_recursive==0) &&
			(ob->particlesystem.first) &&
			(flag & DRAW_PICKING)==0 &&
			(ob!=scene->obedit)	
	  ) {
		ParticleSystem *psys;

		if(col || (ob->flag & SELECT)) cpack(0xFFFFFF);	/* for visibility, also while wpaint */
		//glDepthMask(GL_FALSE);

		glLoadMatrixf(rv3d->viewmat);
		
		view3d_cached_text_draw_begin();

		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			/* run this so that possible child particles get cached */
			if(ob->mode & OB_MODE_PARTICLE_EDIT && ob==OBACT) {
				PTCacheEdit *edit = PE_create_current(scene, ob);
				if(edit && edit->psys == psys)
					draw_update_ptcache_edit(scene, ob, edit);
			}

			draw_new_particle_system(scene, v3d, rv3d, base, psys, dt);
		}
		
		view3d_cached_text_draw_end(v3d, ar, 0, NULL);

		glMultMatrixf(ob->obmat);
		
		//glDepthMask(GL_TRUE);
		if(col) cpack(col);
	}

	/* draw edit particles last so that they can draw over child particles */
	if(		(warning_recursive==0) &&
			(flag & DRAW_PICKING)==0 &&
			(!scene->obedit)	
	  ) {

		if(ob->mode & OB_MODE_PARTICLE_EDIT && ob==OBACT) {
			PTCacheEdit *edit = PE_create_current(scene, ob);
			if(edit) {
				glLoadMatrixf(rv3d->viewmat);
				draw_ptcache_edit(scene, v3d, rv3d, ob, edit, dt);
				glMultMatrixf(ob->obmat);
			}
		}
	}

	/* draw code for smoke */
	if((md = modifiers_findByType(ob, eModifierType_Smoke)))
	{
		SmokeModifierData *smd = (SmokeModifierData *)md;

		// draw collision objects
		if((smd->type & MOD_SMOKE_TYPE_COLL) && smd->coll)
		{
			/*SmokeCollSettings *scs = smd->coll;
			if(scs->points)
			{
				size_t i;

				glLoadMatrixf(rv3d->viewmat);

				if(col || (ob->flag & SELECT)) cpack(0xFFFFFF);	
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				

				// glPointSize(3.0);
				bglBegin(GL_POINTS);

				for(i = 0; i < scs->numpoints; i++)
				{
					bglVertex3fv(&scs->points[3*i]);
				}

				bglEnd();
				glPointSize(1.0);

				glMultMatrixf(ob->obmat);
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
				if(col) cpack(col);
				
			}
			*/
		}

		// only draw domains
		if(smd->domain && smd->domain->fluid)
		{
			if(!smd->domain->wt || !(smd->domain->viewsettings & MOD_SMOKE_VIEW_SHOWBIG))
			{
// #if 0
				smd->domain->tex = NULL;
				GPU_create_smoke(smd, 0);
				draw_volume(scene, ar, v3d, base, smd->domain->tex, smd->domain->p0, smd->domain->p1, smd->domain->res, smd->domain->dx, smd->domain->tex_shadow);
				GPU_free_smoke(smd);
// #endif
#if 0
				int x, y, z;
				float *density = smoke_get_density(smd->domain->fluid);

				glLoadMatrixf(rv3d->viewmat);
				// glMultMatrixf(ob->obmat);	

				if(col || (ob->flag & SELECT)) cpack(0xFFFFFF);	
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				

				// glPointSize(3.0);
				bglBegin(GL_POINTS);

				for(x = 0; x < smd->domain->res[0]; x++)
					for(y = 0; y < smd->domain->res[1]; y++)
						for(z = 0; z < smd->domain->res[2]; z++)
				{
					float tmp[3];
					int index = smoke_get_index(x, smd->domain->res[0], y, smd->domain->res[1], z);

					if(density[index] > FLT_EPSILON)
					{
						float color[3];
						VECCOPY(tmp, smd->domain->p0);
						tmp[0] += smd->domain->dx * x + smd->domain->dx * 0.5;
						tmp[1] += smd->domain->dx * y + smd->domain->dx * 0.5;
						tmp[2] += smd->domain->dx * z + smd->domain->dx * 0.5;
						color[0] = color[1] = color[2] = density[index];
						glColor3fv(color);
						bglVertex3fv(tmp);
					}
				}

				bglEnd();
				glPointSize(1.0);

				glMultMatrixf(ob->obmat);
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
				if(col) cpack(col);
#endif
			}
			else if(smd->domain->wt && (smd->domain->viewsettings & MOD_SMOKE_VIEW_SHOWBIG))
			{
				smd->domain->tex = NULL;
				GPU_create_smoke(smd, 1);
				draw_volume(scene, ar, v3d, base, smd->domain->tex, smd->domain->p0, smd->domain->p1, smd->domain->res_wt, smd->domain->dx_wt, smd->domain->tex_shadow);
				GPU_free_smoke(smd);
			}
		}
	}

	if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {

		bConstraint *con;
		for(con=ob->constraints.first; con; con= con->next) 
		{
			if(con->type==CONSTRAINT_TYPE_RIGIDBODYJOINT) 
			{
				bRigidBodyJointConstraint *data = (bRigidBodyJointConstraint*)con->data;
				if(data->flag&CONSTRAINT_DRAW_PIVOT)
					drawRBpivot(data);
			}
		}

		/* draw extra: after normal draw because of makeDispList */
		if(dtx && (G.f & G_RENDER_OGL)==0) {
        
			if(dtx & OB_AXIS) {
				drawaxes(rv3d, rv3d->viewmatob, 1.0f, flag, OB_ARROWS);
			}
			if(dtx & OB_BOUNDBOX) {
				if((v3d->flag2 & V3D_RENDER_OVERRIDE)==0)
					draw_bounding_volume(scene, ob);
			}
			if(dtx & OB_TEXSPACE) drawtexspace(ob);
			if(dtx & OB_DRAWNAME) {
				/* patch for several 3d cards (IBM mostly) that crash on glSelect with text drawing */
				/* but, we also dont draw names for sets or duplicators */
				if(flag == 0) {
					view3d_cached_text_draw_add(0.0f, 0.0f, 0.0f, ob->id.name+2, 10, 0);
				}
			}
			/*if(dtx & OB_DRAWIMAGE) drawDispListwire(&ob->disp);*/
			if((dtx & OB_DRAWWIRE) && dt>=OB_SOLID) drawWireExtra(scene, rv3d, ob);
		}
	}

	if(dt<OB_SHADED) {
		if((ob->gameflag & OB_DYNAMIC) || 
			((ob->gameflag & OB_BOUNDS) && (ob->boundtype == OB_BOUND_SPHERE))) {
			float imat[4][4], vec[3];

			vec[0]= vec[1]= vec[2]= 0.0;
			invert_m4_m4(imat, rv3d->viewmatob);

			setlinestyle(2);
			drawcircball(GL_LINE_LOOP, vec, ob->inertia, imat);
			setlinestyle(0);
		}
	}
	
	/* return warning, this is cached text draw */
	view3d_cached_text_draw_end(v3d, ar, 1, NULL);

	glLoadMatrixf(rv3d->viewmat);

	if(zbufoff) glDisable(GL_DEPTH_TEST);

	if(warning_recursive) return;
	if(base->flag & OB_FROMDUPLI) return;
	if(v3d->flag2 & V3D_RENDER_OVERRIDE) return;

	/* object centers, need to be drawn in viewmat space for speed, but OK for picking select */
	if(ob!=OBACT || !(ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT))) {
		int do_draw_center= -1;	/* defines below are zero or positive... */

		if(v3d->flag2 & V3D_RENDER_OVERRIDE) {
			/* dont draw */
		} else if((scene->basact)==base)
			do_draw_center= ACTIVE;
		else if(base->flag & SELECT) 
			do_draw_center= SELECT;
		else if(empty_object || (v3d->flag & V3D_DRAW_CENTERS)) 
			do_draw_center= DESELECT;

		if(do_draw_center != -1) {
			if(flag & DRAW_PICKING) {
				/* draw a single point for opengl selection */
				glBegin(GL_POINTS);
				glVertex3fv(ob->obmat[3]);
				glEnd();
			} 
			else if((flag & DRAW_CONSTCOLOR)==0) {
				/* we don't draw centers for duplicators and sets */
				if(U.obcenter_dia > 0) {
					/* check > 0 otherwise grease pencil can draw into the circle select which is annoying. */
					drawcentercircle(v3d, rv3d, ob->obmat[3], do_draw_center, ob->id.lib || ob->id.us>1);
				}
			}
		}
	}

	/* not for sets, duplicators or picking */
	if(flag==0 && (v3d->flag & V3D_HIDE_HELPLINES)== 0 && (v3d->flag2 & V3D_RENDER_OVERRIDE)== 0) {
		ListBase *list;
		
		/* draw hook center and offset line */
		if(ob!=scene->obedit) draw_hooks(ob);
		
		/* help lines and so */
		if(ob!=scene->obedit && ob->parent && (ob->parent->lay & v3d->lay)) {
			setlinestyle(3);
			glBegin(GL_LINES);
			glVertex3fv(ob->obmat[3]);
			glVertex3fv(ob->orig);
			glEnd();
			setlinestyle(0);
		}

		/* Drawing the constraint lines */
		list = &ob->constraints;
		if (list) {
			bConstraint *curcon;
			bConstraintOb *cob;
			char col[4], col2[4];
			
			UI_GetThemeColor3ubv(TH_GRID, col);
			UI_make_axis_color(col, col2, 'z');
			glColor3ubv((GLubyte *)col2);
			
			cob= constraints_make_evalob(scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
			
			for (curcon = list->first; curcon; curcon=curcon->next) {
				bConstraintTypeInfo *cti= constraint_get_typeinfo(curcon);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				if ((curcon->flag & CONSTRAINT_EXPAND) && (cti) && (cti->get_constraint_targets)) {
					cti->get_constraint_targets(curcon, &targets);
					
					for (ct= targets.first; ct; ct= ct->next) {
						/* calculate target's matrix */
						if (cti->get_target_matrix) 
							cti->get_target_matrix(curcon, cob, ct, bsystem_time(scene, ob, (float)(scene->r.cfra), give_timeoffset(ob)));
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

static void bbs_mesh_verts__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	int offset = (intptr_t) userData;
	EditVert *eve = EM_get_vert_for_index(index);

	if (eve->h==0) {
		WM_set_framebuffer_index_color(offset+index);
		bglVertex3fv(co);
	}
}
static void bbs_mesh_verts(DerivedMesh *dm, int offset)
{
	glPointSize( UI_GetThemeValuef(TH_VERTEX_SIZE) );
	bglBegin(GL_POINTS);
	dm->foreachMappedVert(dm, bbs_mesh_verts__mapFunc, (void*)(intptr_t) offset);
	bglEnd();
	glPointSize(1.0);
}		

static int bbs_mesh_wire__setDrawOptions(void *userData, int index)
{
	int offset = (intptr_t) userData;
	EditEdge *eed = EM_get_edge_for_index(index);

	if (eed->h==0) {
		WM_set_framebuffer_index_color(offset+index);
		return 1;
	} else {
		return 0;
	}
}
static void bbs_mesh_wire(DerivedMesh *dm, int offset)
{
	dm->drawMappedEdges(dm, bbs_mesh_wire__setDrawOptions, (void*)(intptr_t) offset);
}		

static int bbs_mesh_solid__setSolidDrawOptions(void *userData, int index, int *drawSmooth_r)
{
	if (EM_get_face_for_index(index)->h==0) {
		if (userData) {
			WM_set_framebuffer_index_color(index+1);
		}
		return 1;
	} else {
		return 0;
	}
}

static void bbs_mesh_solid__drawCenter(void *userData, int index, float *cent, float *no)
{
	EditFace *efa = EM_get_face_for_index(index);

	if (efa->h==0 && efa->fgonf!=EM_FGON) {
		WM_set_framebuffer_index_color(index+1);

		bglVertex3fv(cent);
	}
}

/* two options, facecolors or black */
static void bbs_mesh_solid_EM(Scene *scene, View3D *v3d, Object *ob, DerivedMesh *dm, int facecol)
{
	cpack(0);

	if (facecol) {
		dm->drawMappedFaces(dm, bbs_mesh_solid__setSolidDrawOptions, (void*)(intptr_t) 1, 0);

		if(check_ob_drawface_dot(scene, v3d, ob->dt)) {
			glPointSize(UI_GetThemeValuef(TH_FACEDOT_SIZE));
		
			bglBegin(GL_POINTS);
			dm->foreachMappedFaceCenter(dm, bbs_mesh_solid__drawCenter, NULL);
			bglEnd();
		}

	} else {
		dm->drawMappedFaces(dm, bbs_mesh_solid__setSolidDrawOptions, (void*) 0, 0);
	}
}

static int bbs_mesh_solid__setDrawOpts(void *userData, int index, int *drawSmooth_r)
{
	WM_set_framebuffer_index_color(index+1);
	return 1;
}

static int bbs_mesh_solid_hide__setDrawOpts(void *userData, int index, int *drawSmooth_r)
{
	Mesh *me = userData;

	if (!(me->mface[index].flag&ME_HIDE)) {
		WM_set_framebuffer_index_color(index+1);
		return 1;
	} else {
		return 0;
	}
}

static void bbs_mesh_solid(Scene *scene, View3D *v3d, Object *ob)
{
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, v3d->customdata_mask);
	Mesh *me = (Mesh*)ob->data;
	int face_sel_mode = (me->flag & ME_EDIT_PAINT_MASK) ? 1:0;
	
	glColor3ub(0, 0, 0);
		
	if(face_sel_mode)	dm->drawMappedFaces(dm, bbs_mesh_solid_hide__setDrawOpts, me, 0);
	else				dm->drawMappedFaces(dm, bbs_mesh_solid__setDrawOpts, me, 0);

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
	{
		if(ob->mode & OB_MODE_EDIT) {
			Mesh *me= ob->data;
			EditMesh *em= me->edit_mesh;

			DerivedMesh *dm = editmesh_get_derived_cage(scene, ob, em, CD_MASK_BAREMESH);

			EM_init_index_arrays(em, 1, 1, 1);

			bbs_mesh_solid_EM(scene, v3d, ob, dm, ts->selectmode & SCE_SELECT_FACE);
			if(ts->selectmode & SCE_SELECT_FACE)
				em_solidoffs = 1+em->totface;
			else
				em_solidoffs= 1;
			
			bglPolygonOffset(rv3d->dist, 1.0);
			
			// we draw edges always, for loop (select) tools
			bbs_mesh_wire(dm, em_solidoffs);
			em_wireoffs= em_solidoffs + em->totedge;
			
			// we draw verts if vert select mode or if in transform (for snap).
			if(ts->selectmode & SCE_SELECT_VERTEX || G.moving & G_TRANSFORM_EDIT) {
				bbs_mesh_verts(dm, em_wireoffs);
				em_vertoffs= em_wireoffs + em->totvert;
			}
			else em_vertoffs= em_wireoffs;
			
			bglPolygonOffset(rv3d->dist, 0.0);

			dm->release(dm);

			EM_free_index_arrays();
		}
		else bbs_mesh_solid(scene, v3d, ob);
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
	
	if(ob->mode & OB_MODE_EDIT)
		edm= editmesh_get_derived_base(ob, me->edit_mesh);
	else 
		dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);

	if(dt<=OB_WIRE) {
		if(dm)
			dm->drawEdges(dm, 1, 0);
		else if(edm)
			edm->drawEdges(edm, 1, 0);	
	}
	else {
		if(outline)
			draw_mesh_object_outline(v3d, ob, dm?dm:edm);

		if(dm) {
			glsl = draw_glsl_material(scene, ob, v3d, dt);
			GPU_begin_object_materials(v3d, rv3d, scene, ob, glsl, NULL);
		}
		else {
			glEnable(GL_COLOR_MATERIAL);
			UI_ThemeColor(TH_BONE_SOLID);
			glDisable(GL_COLOR_MATERIAL);
		}
		
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);
		glEnable(GL_LIGHTING);
		
		if(dm) {
			dm->drawFacesSolid(dm, NULL, 0, GPU_enable_material);
			GPU_end_object_materials();
		}
		else if(edm)
			edm->drawMappedFaces(edm, NULL, NULL, 0);
		
		glDisable(GL_LIGHTING);
	}

	if(edm) edm->release(edm);
	if(dm) dm->release(dm);
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
			drawaxes(rv3d, rv3d->viewmatob, ob->empty_drawsize, 0, ob->empty_drawtype);
			break;
	}
}
