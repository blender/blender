/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "IMB_imbuf.h"


#include "MTC_matrixops.h"

#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_constraint_types.h" // for drawing constraint
#include "DNA_effect_types.h"
#include "DNA_ipo_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_utildefines.h"
#include "BKE_curve.h"
#include "BKE_constraint.h" // for the get_constraint_target function
#include "BKE_deform.h"		// lattice_modifier()
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_object.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_editarmature.h"
#include "BIF_editika.h"
#include "BIF_editmesh.h"
#include "BIF_glutil.h"
#include "BIF_resources.h"

#include "BDR_drawmesh.h"
#include "BDR_drawobject.h"
#include "BDR_editobject.h"

#include "BSE_view.h"
#include "BSE_drawview.h"
#include "BSE_trans_types.h"

#include "blendef.h"
#include "mydevice.h"
#include "nla.h"

#include "BKE_deform.h"

/* pretty stupid */
/*  extern Lattice *editLatt; already in BKE_lattice.h  */
/* editcurve.c */
extern ListBase editNurb;
/* editmball.c */
extern ListBase editelems;

static void draw_bounding_volume(Object *ob);

	/***/

// Materials start counting at # one....
#define MAXMATBUF (MAXMAT + 1)
static float matbuf[MAXMATBUF][2][4];

static void init_gl_materials(Object *ob)
{
	extern Material defmaterial;	// render module abuse...
	Material *ma;
	int a;
	
	if(ob->totcol==0) {
		matbuf[0][0][0]= defmaterial.r;
		matbuf[0][0][1]= defmaterial.g;
		matbuf[0][0][2]= defmaterial.b;
		matbuf[0][0][3]= 1.0;

		matbuf[0][1][0]= defmaterial.specr;
		matbuf[0][1][1]= defmaterial.specg;
		matbuf[0][1][2]= defmaterial.specb;
		matbuf[0][1][3]= 1.0;
		
		/* do material 1 too, for displists! */
		VECCOPY(matbuf[1][0], matbuf[0][0]);
		VECCOPY(matbuf[1][1], matbuf[0][1]);
	}
	
	for(a=1; a<=ob->totcol; a++) {
		ma= give_current_material(ob, a);
		if(ma==NULL) ma= &defmaterial;
		if(a<MAXMATBUF) {
			matbuf[a][0][0]= (ma->ref+ma->emit)*ma->r;
			matbuf[a][0][1]= (ma->ref+ma->emit)*ma->g;
			matbuf[a][0][2]= (ma->ref+ma->emit)*ma->b;
			matbuf[a][0][3]= 1.0;
			
			matbuf[a][1][0]= ma->spec*ma->specr;
			matbuf[a][1][1]= ma->spec*ma->specg;
			matbuf[a][1][2]= ma->spec*ma->specb;
			matbuf[a][1][3]= 1.0;
		}
	}
}

static void set_gl_material(int nr)
{
	if(nr<MAXMATBUF) {
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matbuf[nr][0]);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matbuf[nr][1]);
	}
}

	/***/
	
unsigned int rect_desel[16]= {0x707070,0x0,0x0,0x707070,0x407070,0x70cccc,0x407070,0x0,0xaaffff,0xffffff,0x70cccc,0x0,0x70cccc,0xaaffff,0x407070,0x707070};
unsigned int rect_sel[16]= {0x707070,0x0,0x0,0x707070,0x702070,0xcc50cc,0x702070,0x0,0xff80ff,0xffffff,0xcc50cc,0x0,0xcc50cc,0xff80ff,0x702070,0x707070};

unsigned int rectu_desel[16]= {0xff4e4e4e,0xff5c2309,0xff000000,0xff4e4f4d,0xff000000,0xffff9d72,0xffff601c,0xff000000,0xff5d2409,0xffffffff,0xffff9d72,0xff5b2209,0xff4e4e4e,0xff5c2309,0xff010100,0xff4f4f4f};
unsigned int rectu_sel[16]= {0xff4e4e4e,0xff403c00,0xff000000,0xff4e4e4d,0xff000000,0xfffff64c,0xffaaa100,0xff000000,0xff403c00,0xffffffff,0xfffff64c,0xff403c00,0xff4f4f4f,0xff403c00,0xff010100,0xff4e4e4e};

unsigned int rectl_desel[81]= {0x777777,0x777777,0xa9fefe,0xaaffff,0xaaffff,0xaaffff,0xaaffff,0x777777,0x777777,0x777777,0xa9fefe,0xaafefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0x777777,0xaaffff,0xa9fefe,0x4e4e4e,0x0,0x124040,0x0,0x4e4e4e,0xaafefe,0xaaffff,0xaaffff,0x777777,0x0,0x227777,0x55cccc,0x227777,0x0,0x777777,0xaaffff,0xaaffff,0x777777,0x124040,0x88ffff,0xffffff,0x55cccc,0x124040,0x777777,0xaaffff,0xaaffff,0x777777,0x0,0x55cccc,0x88ffff,0x227777,0x0,0x777777,0xaaffff,0xaafefe,0xaafefe,0x4f4f4f,0x0,0x124040,0x0,0x4e4e4e,0xa9fefe,0xaaffff,0x777777,0xa9fefe,0xa9fefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0xaaffff,0xaaffff,0xaaffff,0x777777,0x777777};
unsigned int rectl_sel[81]= {0x777777,0x777777,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0xffaaff,0xffaaff,0x4e4e4e,0x10101,0x402440,0x0,0x4e4e4e,0xffaaff,0xffaaff,0xffaaff,0x777777,0x0,0x774477,0xcc77cc,0x774477,0x0,0x777777,0xffaaff,0xffaaff,0x777777,0x402440,0xffaaff,0xffffff,0xcc77cc,0x412541,0x777777,0xffaaff,0xffaaff,0x777777,0x10101,0xcc77cc,0xffaaff,0x774477,0x0,0x777777,0xffaaff,0xffaaff,0xffaaff,0x4e4e4e,0x10101,0x402440,0x0,0x4e4e4e,0xffaaff,0xffaaff,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0x777777,0x777777};
unsigned int rectlus_desel[81]= {0x777777,0x777777,0xa9fefe,0xaaffff,0xaaffff,0xaaffff,0xaaffff,0x777777,0x777777,0x777777,0xa9fefe,0xaafefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0x777777,0xaaffff,0xa9fefe,0x4e4e4e,0x0,0x5c2309,0x0,0x4e4f4d,0xaafefe,0xaaffff,0xaaffff,0x777777,0x0,0xff601c,0xff9d72,0xff601c,0x0,0x777777,0xaaffff,0xaaffff,0x777777,0x5d2409,0xffceb8,0xff9d72,0xff9d72,0x5b2209,0x777777,0xaaffff,0xaaffff,0x777777,0x10100,0xffceb8,0xffceb8,0xff601c,0x0,0x777777,0xaaffff,0xaafefe,0xaafefe,0x4e4e4e,0x0,0x5c2309,0x10100,0x4f4f4f,0xa9fefe,0xaaffff,0x777777,0xa9fefe,0xa9fefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0x777777,0x777777,0x777777,0xa9fefe,0xa9fefe,0xaaffff,0xaaffff,0xaaffff,0x777777,0x777777};
unsigned int rectlus_sel[81]= {0x777777,0x777777,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0xffaaff,0xffaaff,0x4e4e4e,0x10100,0x403c00,0x0,0x4e4e4d,0xffaaff,0xffaaff,0xffaaff,0x777777,0x0,0xaaa100,0xfff64c,0xaaa100,0x0,0x777777,0xffaaff,0xffaaff,0x777777,0x403c00,0xfffde2,0xffffff,0xfff64c,0x403c00,0x777777,0xffaaff,0xffaaff,0x777777,0x10100,0xfff64c,0xfffde2,0xaaa100,0x0,0x777777,0xffaaff,0xffaaff,0xffaaff,0x4f4f4f,0x0,0x403c00,0x10100,0x4e4e4e,0xffaaff,0xffaaff,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0x777777,0x777777,0x777777,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0xffaaff,0x777777,0x777777};
unsigned int rectllib_desel[81]= {0xff777777,0xff777777,0xb9b237,0xb9b237,0xb9b237,0xb9b237,0xb9b237,0xff777777,0xff777777,0xff777777,0xb9b237,0xb9b237,0xff777777,0xff777777,0xff777777,0xb9b237,0xb9b237,0xff777777,0xb9b237,0xb9b237,0x4e4e4e,0x0,0x5c2309,0x0,0x4e4f4d,0xb9b237,0xb9b237,0xb9b237,0xff777777,0x0,0xff601c,0xff9d72,0xff601c,0x0,0xff777777,0xb9b237,0xb9b237,0xff777777,0x5d2409,0xffceb8,0xff9d72,0xff9d72,0x5b2209,0xff777777,0xb9b237,0xb9b237,0xff777777,0x10100,0xffceb8,0xffceb8,0xff601c,0x0,0xff777777,0xb9b237,0xb9b237,0xb9b237,0x4e4e4e,0x0,0x5c2309,0x10100,0x4f4f4f,0xb9b237,0xb9b237,0xff777777,0xb9b237,0xb9b237,0xff777777,0xff777777,0xff777777,0xb9b237,0xb9b237,0xff777777,0xff777777,0xff777777,0xb9b237,0xb9b237,0xb9b237,0xb9b237,0xb9b237,0xff777777,0xff777777};
unsigned int rectllib_sel[81]= {0xff777777,0xff777777,0xfff64c,0xfff64c,0xfff64c,0xfff64c,0xfff64c,0xff777777,0xff777777,0xff777777,0xfff64c,0xfff64c,0xff777777,0xff777777,0xff777777,0xfff64c,0xfff64c,0xff777777,0xfff64c,0xfff64c,0x4e4e4e,0x10100,0x403c00,0x0,0x4e4e4d,0xfff64c,0xfff64c,0xfff64c,0xff777777,0x0,0xaaa100,0xfff64c,0xaaa100,0x0,0xff777777,0xfff64c,0xfff64c,0xff777777,0x403c00,0xfffde2,0xffffff,0xfff64c,0x403c00,0xff777777,0xfff64c,0xfff64c,0xff777777,0x10100,0xfff64c,0xfffde2,0xaaa100,0x0,0xff777777,0xfff64c,0xfff64c,0xfff64c,0x4f4f4f,0x0,0x403c00,0x10100,0x4e4e4e,0xfff64c,0xfff64c,0xff777777,0xfff64c,0xfff64c,0xff777777,0xff777777,0xff777777,0xfff64c,0xfff64c,0xff777777,0xff777777,0xff777777,0xfff64c,0xfff64c,0xfff64c,0xfff64c,0xfff64c,0xff777777,0xff777777};

unsigned int rectl_set[81]= {0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0xaaaaaa,0xaaaaaa,0x4e4e4e,0x10100,0x202020,0x0,0x4e4e4d,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xff777777,0x0,0xaaa100,0xaaaaaa,0xaaa100,0x0,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0x202020,0xfffde2,0xffffff,0xaaaaaa,0x202020,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0x10100,0xaaaaaa,0xfffde2,0xaaa100,0x0,0xff777777,0xaaaaaa,0xaaaaaa,0xaaaaaa,0x4f4f4f,0x0,0x202020,0x10100,0x4e4e4e,0xaaaaaa,0xaaaaaa,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777,0xff777777,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xaaaaaa,0xff777777,0xff777777};


static unsigned int colortab[24]=
	{0x0,		0xFF88FF, 0xFFBBFF, 
	 0x403000,	0xFFFF88, 0xFFFFBB, 
	 0x104040,	0x66CCCC, 0x77CCCC, 
	 0x101040,	0x5588FF, 0x88BBFF, 
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

void init_draw_rects(void)
{
	if(G.order==B_ENDIAN) {
		IMB_convert_rgba_to_abgr(16, rect_desel);
		IMB_convert_rgba_to_abgr(16, rect_sel);
		
		IMB_convert_rgba_to_abgr(16, rectu_desel);
		IMB_convert_rgba_to_abgr(16, rectu_sel);
		
		IMB_convert_rgba_to_abgr(81, rectl_desel);
		IMB_convert_rgba_to_abgr(81, rectl_sel);
	
		IMB_convert_rgba_to_abgr(81, rectlus_desel);
		IMB_convert_rgba_to_abgr(81, rectlus_sel);
	
		IMB_convert_rgba_to_abgr(81, rectllib_desel);
		IMB_convert_rgba_to_abgr(81, rectllib_sel);
				
		IMB_convert_rgba_to_abgr(81, rectl_set);
	}
}

static void draw_icon_centered(float *pos, unsigned int *rect, int rectsize) 
{
	float hsize= (float) rectsize/2.0;
	GLubyte dummy= 0;
	
	glRasterPos3fv(pos);
	
		/* use bitmap to shift rasterpos in pixels */
	glBitmap(0, 0, 0.0, 0.0, -hsize, -hsize, &dummy);
#if defined (__sun__) || defined ( __sun ) || defined (__sparc) || defined (__sparc__)
	glFlush(); 
#endif	
	glDrawPixels(rectsize, rectsize, GL_RGBA, GL_UNSIGNED_BYTE, rect);
}

/* bad frontbuffer call... because it is used in transform after force_draw() */
void helpline(float *vec)
{
	float vecrot[3], cent[2];
	short mval[2];

	VECCOPY(vecrot, vec);
	if(G.obedit) Mat4MulVecfl(G.obedit->obmat, vecrot);
	else if(G.obpose) Mat4MulVecfl(G.obpose->obmat, vecrot);

	getmouseco_areawin(mval);
	project_float(vecrot, cent);	// no overflow in extreme cases
	if(cent[0]!=3200.0f) {
		persp(PERSP_WIN);
		
		glDrawBuffer(GL_FRONT);
		
		BIF_ThemeColor(TH_WIRE);

		setlinestyle(3);
		glBegin(GL_LINE_STRIP); 
			glVertex2sv(mval); 
			glVertex2fv(cent); 
		glEnd();
		setlinestyle(0);
		
		persp(PERSP_VIEW);
		glFlush(); // flush display for frontbuffer
		glDrawBuffer(GL_BACK);
	}
}

void drawaxes(float size)
{
	int axis;

	for (axis=0; axis<3; axis++) {
		float v1[3]= {0.0, 0.0, 0.0};
		float v2[3]= {0.0, 0.0, 0.0};
		int arrow_axis= (axis==0)?1:0;
		
		glBegin(GL_LINES);

		v2[axis]= size;
		glVertex3fv(v1);
		glVertex3fv(v2);
			
		v1[axis]= size*0.8;
		v1[arrow_axis]= -size*0.125;
		glVertex3fv(v1);
		glVertex3fv(v2);
			
		v1[arrow_axis]= size*0.125;
		glVertex3fv(v1);
		glVertex3fv(v2);

		glEnd();
			
		v2[axis]+= size*0.125;
		glRasterPos3fv(v2);
		
		// patch for 3d cards crashing on glSelect for text drawing (IBM)
		if((G.f & G_PICKSEL) == 0) {
			if (axis==0)
				BMF_DrawString(G.font, "x");
			else if (axis==1)
				BMF_DrawString(G.font, "y");
			else
				BMF_DrawString(G.font, "z");
		}
	}
}


void drawsolidcube(float size)
{
	float n[3];

	glPushMatrix();
	glScalef(size, size, size);
	
	n[0]=0; n[1]=0; n[2]=0;
	glBegin(GL_QUADS);
		n[0]= -1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[0]); glVertex3fv(cube[1]); glVertex3fv(cube[2]); glVertex3fv(cube[3]);
		n[0]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[1]= -1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[0]); glVertex3fv(cube[4]); glVertex3fv(cube[5]); glVertex3fv(cube[1]);
		n[1]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[0]= 1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[4]); glVertex3fv(cube[7]); glVertex3fv(cube[6]); glVertex3fv(cube[5]);
		n[0]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[1]= 1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[7]); glVertex3fv(cube[3]); glVertex3fv(cube[2]); glVertex3fv(cube[6]);
		n[1]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[2]= 1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[1]); glVertex3fv(cube[5]); glVertex3fv(cube[6]); glVertex3fv(cube[2]);
		n[2]=0;
	glEnd();

	glBegin(GL_QUADS);
		n[2]= -1.0;
		glNormal3fv(n); 
		glVertex3fv(cube[7]); glVertex3fv(cube[4]); glVertex3fv(cube[0]); glVertex3fv(cube[3]);
	glEnd();
	
	glPopMatrix();
}

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

static void tekenshadbuflimits(Lamp *la, float mat[][4])
{
	float sta[3], end[3], lavec[3];

	lavec[0]= -mat[2][0];
	lavec[1]= -mat[2][1];
	lavec[2]= -mat[2][2];
	Normalise(lavec);

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
	float temp[3],plane[3],mat1[3][3],mat2[3][3],mat3[3][3],mat4[3][3],q[4],co,si,hoek;

	Normalise(lvec);
	Normalise(vvec);				/* is this the correct vector ? */

	Crossf(temp,vvec,lvec);		/* equation for a plane through vvec en lvec */
	Crossf(plane,lvec,temp);		/* a plane perpendicular to this, parrallel with lvec */

	Normalise(plane);

	/* now we've got two equations: one of a cone and one of a plane, but we have
	three unknowns. We remove one unkown by rotating the plane to z=0 (the plane normal) */

	/* rotate around cross product vector of (0,0,1) and plane normal, dot product degrees */
	/* according definition, we derive cross product is (plane[1],-plane[0],0), en cos = plane[2]);*/

	/* translating this comment to english didnt really help me understanding the math! :-) (ton) */
	
	q[1] = plane[1] ; 
	q[2] = -plane[0] ; 
	q[3] = 0 ;
	Normalise(&q[1]);

	hoek = saacos(plane[2])/2.0;
	co = cos(hoek);
	si = sqrt(1-co*co);

	q[0] =  co;
	q[1] *= si;
	q[2] *= si;
	q[3] =  0;

	QuatToMat3(q,mat1);

	/* rotate lamp vector now over acos(inp) degrees */

	vvec[0] = lvec[0] ; 
	vvec[1] = lvec[1] ; 
	vvec[2] = lvec[2] ;

	Mat3One(mat2);
	co = inp;
	si = sqrt(1-inp*inp);

	mat2[0][0] =  co;
	mat2[1][0] = -si;
	mat2[0][1] =  si;
	mat2[1][1] =  co;
	Mat3MulMat3(mat3,mat2,mat1);

	mat2[1][0] =  si;
	mat2[0][1] = -si;
	Mat3MulMat3(mat4,mat2,mat1);
	Mat3Transp(mat1);

	Mat3MulMat3(mat2,mat1,mat3);
	Mat3MulVecfl(mat2,lvec);
	Mat3MulMat3(mat2,mat1,mat4);
	Mat3MulVecfl(mat2,vvec);

	return;
}



static void drawlamp(Object *ob)
{
	Lamp *la;
	float vec[3], lvec[3], vvec[3],x,y,z;
	
	la= ob->data;
	vec[0]=vec[1]=vec[2]= 0.0;
	
	setlinestyle(4);
	
	/* yafray: for photonlight also draw lightcone as for spot */
	if ((la->type==LA_SPOT) || (la->type==LA_YF_PHOTON)) {
		
		lvec[0]=lvec[1]= 0.0; 
		lvec[2] = 1.0;
		x = G.vd->persmat[0][2];
		y = G.vd->persmat[1][2];
		z = G.vd->persmat[2][2];
		vvec[0]= x*ob->obmat[0][0] + y*ob->obmat[0][1] + z*ob->obmat[0][2];
		vvec[1]= x*ob->obmat[1][0] + y*ob->obmat[1][1] + z*ob->obmat[1][2];
		vvec[2]= x*ob->obmat[2][0] + y*ob->obmat[2][1] + z*ob->obmat[2][2];

		y = cos( M_PI*la->spotsize/360.0 );
		spotvolume(lvec, vvec, y);
		x = -la->dist;
		lvec[0] *=  x ; 
		lvec[1] *=  x ; 
		lvec[2] *=  x;
		vvec[0] *= x ; 
		vvec[1] *= x ; 
		vvec[2] *= x;

		glBegin(GL_LINE_STRIP);
			glVertex3fv(vvec);
			glVertex3fv(vec);
			glVertex3fv(lvec);
		glEnd();
		
		z = x*sqrt(1.0 - y*y);
		x *= y;

		glTranslatef(0.0 ,  0.0 ,  x);
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
		
	}
	else if ELEM(la->type, LA_HEMI, LA_SUN) {
		glBegin(GL_LINE_STRIP);
			glVertex3fv(vec); 
			vec[2]= -la->dist; 
			glVertex3fv(vec);
		glEnd();
	}
	else {
		if(la->type==LA_AREA) {
			setlinestyle(0);
			if(la->area_shape==LA_AREA_SQUARE) 
				fdrawbox(-la->area_size*0.5, -la->area_size*0.5, la->area_size*0.5, la->area_size*0.5);
			else if(la->area_shape==LA_AREA_RECT) 
				fdrawbox(-la->area_size*0.5, -la->area_sizey*0.5, la->area_size*0.5, la->area_sizey*0.5);
			setlinestyle(3);
			glBegin(GL_LINE_STRIP); 
			glVertex3f(0.0,0.0,0.0);
			glVertex3f(0.0,0.0,-la->dist);
			glEnd();
			setlinestyle(0);
		}
		else if(la->mode & LA_SPHERE) {

			float tmat[4][4], imat[4][4];
			
			vec[0]= vec[1]= vec[2]= 0.0;
			mygetmatrix(tmat);
			Mat4Invert(imat, tmat);
			
			drawcircball(vec, la->dist, imat);

		}
	}

	glPushMatrix();
	glMultMatrixf(G.vd->viewmat);
	
	VECCOPY(vec, ob->obmat[3]);
	
	setlinestyle(3);
	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec); 
		vec[2]= 0; 
		glVertex3fv(vec);
	glEnd();
	setlinestyle(0);
		
	if(la->type==LA_SPOT && (la->mode & LA_SHAD) ) {
		tekenshadbuflimits(la, ob->obmat);
	}
	glPopMatrix();
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
static void draw_focus_cross(float dist, float size)
{
	glBegin(GL_LINES);
	glVertex3f(-size, 0.f, -dist);
	glVertex3f(size, 0.f, -dist);
	glVertex3f(0.f, -size, -dist);
	glVertex3f(0.f, size, -dist);
	glEnd();
}

void drawcamera(Object *ob)
{
	/* a standing up pyramid with (0,0,0) as top */
	Camera *cam;
	World *wrld;
	float vec[8][4], tmat[4][4], fac, facx, facy, depth;

	cam= ob->data;
	
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	
	if(G.vd->persp>=2 && cam->type==CAM_ORTHO && ob==G.vd->camera) {
		facx= 0.5*cam->ortho_scale*1.28;
		facy= 0.5*cam->ortho_scale*1.024;
		depth= -cam->clipsta-0.1;
	}
	else {
		fac= cam->drawsize;
		if(G.vd->persp>=2 && ob==G.vd->camera) fac= cam->clipsta+0.1; /* that way it's always visible */
		
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

	if(G.vd->persp>=2 && ob==G.vd->camera) return;
	
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

	glBegin(GL_QUADS);

		vec[0][0]= -0.2*cam->drawsize; 
		vec[0][1]= cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.2*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][1]= 1.6*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= -0.2*cam->drawsize; 
		glVertex3fv(vec[0]);
	glEnd();

	glBegin(GL_TRIANGLES);
	
		vec[0][0]= -0.4*cam->drawsize;
		vec[0][1]= 1.6*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.0; 
		vec[0][1]= 2.0*cam->drawsize;
		glVertex3fv(vec[0]);
		
		vec[0][0]= 0.4*cam->drawsize; 
		vec[0][1]= 1.6*cam->drawsize;
		glVertex3fv(vec[0]);
	
	glEnd();
	
	if(cam->flag & (CAM_SHOWLIMITS+CAM_SHOWMIST)) {
		myloadmatrix(G.vd->viewmat);
		Mat4CpyMat4(vec, ob->obmat);
		Mat4Ortho(vec);
		mymultmatrix(vec);

		MTC_Mat4SwapMat4(G.vd->persmat, tmat);
		mygetsingmatrix(G.vd->persmat);

		if(cam->flag & CAM_SHOWLIMITS) {
			draw_limit_line(cam->clipsta, cam->clipend, 0x77FFFF);
			/* yafray: dof focus point */
			if (G.scene->r.renderer==R_YAFRAY) draw_focus_cross(cam->YF_dofdist, cam->drawsize);
		}

		wrld= G.scene->world;
		if(cam->flag & CAM_SHOWMIST) 
			if(wrld) draw_limit_line(wrld->miststa, wrld->miststa+wrld->mistdist, 0xFFFFFF);
			
		MTC_Mat4SwapMat4(G.vd->persmat, tmat);
	}
}

static void tekenvertslatt(short sel)
{
	Lattice *lt;
	BPoint *bp;
	float size;
	int a, uxt, u, vxt, v, wxt, w;

	size= BIF_GetThemeValuef(TH_VERTEX_SIZE);
	glPointSize(size);

	if(sel) BIF_ThemeColor(TH_VERTEX_SELECT);
	else BIF_ThemeColor(TH_VERTEX);

	bglBegin(GL_POINTS);

	bp= editLatt->def;
	lt= editLatt;
	
	if(lt->flag & LT_OUTSIDE) {
		
		for(w=0; w<lt->pntsw; w++) {
			if(w==0 || w==lt->pntsw-1) wxt= 1; else wxt= 0;
			for(v=0; v<lt->pntsv; v++) {
				if(v==0 || v==lt->pntsv-1) vxt= 1; else vxt= 0;
				
				for(u=0; u<lt->pntsu; u++, bp++) {
					if(u==0 || u==lt->pntsu-1) uxt= 1; else uxt= 0;
					if(uxt || vxt || wxt) {
						if(bp->hide==0) {
							if((bp->f1 & 1)==sel) bglVertex3fv(bp->vec);
						}
					}
				}
			}
		}
	}
	else {

		a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
		while(a--) {
			if(bp->hide==0) {
				if((bp->f1 & 1)==sel) bglVertex3fv(bp->vec);
			}
			bp++;
		}
	}
	
	glPointSize(1.0);
	bglEnd();	
}

static void calc_lattverts(void)
{
	BPoint *bp;
	float mat[4][4];
	int a;

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);
	
	 bp= editLatt->def;
	
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	while(a--) {
		project_short(bp->vec, bp->s);
		bp++;
	}

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
}


void calc_lattverts_ext(void)
{

	areawinset(curarea->win);
	persp(PERSP_VIEW);
	mymultmatrix(G.obedit->obmat);
	calc_lattverts();
	myloadmatrix(G.vd->viewmat);
	
}


static void drawlattice(Object *ob)
{
	Lattice *lt;
	BPoint *bp, *bpu;
	int u, v, w, dv, dw, uxt, vxt, wxt;

	lt= ob->data;
	if(ob==G.obedit) {
		bp= editLatt->def;
		
		cpack(0x004000);
	}
	else {
		lattice_modifier(ob, 's');
		bp= lt->def;
	}
	
	dv= lt->pntsu;
	dw= dv*lt->pntsv;
	
	if(lt->flag & LT_OUTSIDE) {
		
		for(w=0; w<lt->pntsw; w++) {
			
			if(w==0 || w==lt->pntsw-1) wxt= 1; else wxt= 0;
			
			for(v=0; v<lt->pntsv; v++) {
				
				if(v==0 || v==lt->pntsv-1) vxt= 1; else vxt= 0;
				
				for(u=0, bpu=0; u<lt->pntsu; u++, bp++) {
				
					if(u==0 || u==lt->pntsu-1) uxt= 1; else uxt= 0;
					
					if(uxt || vxt || wxt) {
					
						if(w && (uxt || vxt)) {

							glBegin(GL_LINE_STRIP);
							glVertex3fv( (bp-dw)->vec ); glVertex3fv(bp->vec);
							glEnd();
						}
						if(v && (uxt || wxt)) {

							glBegin(GL_LINES);
							glVertex3fv( (bp-dv)->vec ); glVertex3fv(bp->vec);
							glEnd();
						}
						if(u && (vxt || wxt)) {

							glBegin(GL_LINES);
							glVertex3fv(bpu->vec); glVertex3fv(bp->vec);
							glEnd();
						}
					}
					
					bpu= bp;
				}
			}
		}		
	}
	else {
		for(w=0; w<lt->pntsw; w++) {
			
			for(v=0; v<lt->pntsv; v++) {
				
				for(u=0, bpu=0; u<lt->pntsu; u++, bp++) {
				
					if(w) {

						glBegin(GL_LINES);
						glVertex3fv( (bp-dw)->vec ); glVertex3fv(bp->vec);
						glEnd();
					}
					if(v) {

						glBegin(GL_LINES);
						glVertex3fv( (bp-dv)->vec ); glVertex3fv(bp->vec);
						glEnd();
					}
					if(u) {

						glBegin(GL_LINES);
						glVertex3fv(bpu->vec); glVertex3fv(bp->vec);
						glEnd();
					}
					bpu= bp;
				}
			}
		}
	}
	
	if(ob==G.obedit) {
		
		calc_lattverts();
		
		if(G.zbuf) glDisable(GL_DEPTH_TEST);
		
		tekenvertslatt(0);
		tekenvertslatt(1);
		
		if(G.zbuf) glEnable(GL_DEPTH_TEST); 
	}
	else lattice_modifier(ob, 'e');

}

/* ***************** ******************** */

int subsurf_optimal(Object *ob)
{
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		if( (me->flag & ME_OPT_EDGES) && (me->flag & ME_SUBSURF) && me->subdiv) return 1;
	}
	return 0;
}


void calc_mesh_facedots_ext(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	float mat[4][4];

	if(em->faces.first==NULL) return;
	efa= em->faces.first;

	areawinset(curarea->win);
	persp(PERSP_VIEW);
	
	mymultmatrix(G.obedit->obmat);

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);

	efa= em->faces.first;
	while(efa) {
		if( efa->h==0) {
			project_short(efa->cent, &(efa->xs));
		}
		efa= efa->next;
	}
	MTC_Mat4SwapMat4(G.vd->persmat, mat);

	myloadmatrix(G.vd->viewmat);
}

/* window coord, assuming all matrices are set OK */
static void calc_meshverts(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	float mat[4][4];

	if(em->verts.first==0) return;
	eve= em->verts.first;

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);

	if(subsurf_optimal(G.obedit)) { // separate loop for speed 
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0 && eve->ssco) project_short(eve->ssco, &(eve->xs));
		}
	}
	else {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0) project_short(eve->co, &(eve->xs));
		}
	}
	MTC_Mat4SwapMat4(G.vd->persmat, mat);
}

/* window coord for current window, sets matrices temporal */
void calc_meshverts_ext(void)
{

	areawinset(curarea->win);
	persp(PERSP_VIEW);
	
	mymultmatrix(G.obedit->obmat);
	calc_meshverts();
	myloadmatrix(G.vd->viewmat);
	
}

/* window coord for current window, sets matrices temporal, sets (eve->f & 2) when not visible */
void calc_meshverts_ext_f2(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	float mat[4][4];
	int optimal= subsurf_optimal(G.obedit);
	
	if(em->verts.first==0) return;
	eve= em->verts.first;

	/* matrices */
	areawinset(curarea->win);
	persp(PERSP_VIEW);
	mymultmatrix(G.obedit->obmat);
	
	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);

	for(eve= em->verts.first; eve; eve= eve->next) {
		eve->f &= ~2;
		if(eve->h==0) {
			if(optimal && eve->ssco) project_short_noclip(eve->ssco, &(eve->xs));
			else project_short_noclip(eve->co, &(eve->xs));
	
			if( eve->xs >= 0 && eve->ys >= 0 && eve->xs<curarea->winx && eve->ys<curarea->winy);
			else eve->f |= 2;
		}
	}
	
	/* restore */
	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	myloadmatrix(G.vd->viewmat);
	
}


static void calc_Nurbverts(Nurb *nurb)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float mat[4][4];
	int a;

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
	mygetsingmatrix(G.vd->persmat);

	nu= nurb;
	while(nu) {
		if((nu->type & 7)==1) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				project_short(bezt->vec[0], bezt->s[0]);
				project_short(bezt->vec[1], bezt->s[1]);
				project_short(bezt->vec[2], bezt->s[2]);
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				project_short(bp->vec, bp->s);
				bp++;
			}
		}
		nu= nu->next;
	}

	MTC_Mat4SwapMat4(G.vd->persmat, mat);
}

void calc_nurbverts_ext(void)
{

	areawinset(curarea->win);
	persp(PERSP_VIEW);
	mymultmatrix(G.obedit->obmat);
	calc_Nurbverts(editNurb.first);
	myloadmatrix(G.vd->viewmat);
	
}

////

static void calc_weightpaint_vert_color(Object *ob, int vert, unsigned char *col)
{
	Mesh *me = ob->data;
	float fr, fg, fb, input = 0.0;
	int i;

	if (me->dvert) {
		for (i=0; i<me->dvert[vert].totweight; i++)
			if (me->dvert[vert].dw[i].def_nr==ob->actdef-1)
				input+=me->dvert[vert].dw[i].weight;		
	}

	CLAMP(input, 0.0, 1.0);

	fr = fg = fb = 85;
	if (input<=0.25f){
		fr=0.0f;
		fg=255.0f * (input*4.0f);
		fb=255.0f;
	}
	else if (input<=0.50f){
		fr=0.0f;
		fg=255.0f;
		fb=255.0f * (1.0f-((input-0.25f)*4.0f)); 
	}
	else if (input<=0.75){
		fr=255.0f * ((input-0.50f)*4.0f);
		fg=255.0f;
		fb=0.0f;
	}
	else if (input<=1.0){
		fr=255.0f;
		fg=255.0f * (1.0f-((input-0.75f)*4.0f)); 
		fb=0.0f;
	}

	col[3] = (unsigned char)(fr * ((input/2.0f)+0.5f));
	col[2] = (unsigned char)(fg * ((input/2.0f)+0.5f));
	col[1] = (unsigned char)(fb * ((input/2.0f)+0.5f));
	col[0] = 255;
}
static unsigned int *calc_weightpaint_colors(Object *ob) 
{
	Mesh *me = ob->data;
	MFace *mf = me->mface;
	unsigned char *wtcol;
	int i;
	
	wtcol = MEM_callocN (sizeof (unsigned char) * me->totface*4*4, "weightmap");
	
	memset(wtcol, 0x55, sizeof (unsigned char) * me->totface*4*4);
	for (i=0; i<me->totface; i++, mf++){
		calc_weightpaint_vert_color(ob, mf->v1, &wtcol[(i*4 + 0)*4]); 
		calc_weightpaint_vert_color(ob, mf->v2, &wtcol[(i*4 + 1)*4]); 
		if (mf->v3)
			calc_weightpaint_vert_color(ob, mf->v3, &wtcol[(i*4 + 2)*4]); 
		if (mf->v4)
			calc_weightpaint_vert_color(ob, mf->v4, &wtcol[(i*4 + 3)*4]); 
	}
	
	return (unsigned int*) wtcol;
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

static void displistmesh_draw_solid(DispListMesh *dlm, float *nors) 
{
	int glmode=-1, shademodel=-1, matnr=-1;
	int i;

#define PASSVERT(ind) {						\
	if (shademodel==GL_SMOOTH)				\
		glNormal3sv(dlm->mvert[(ind)].no);	\
	glVertex3fv(dlm->mvert[(ind)].co);		\
}

	glBegin(glmode=GL_QUADS);
	for (i=0; i<dlm->totface; i++) {
		MFace *mf= &dlm->mface[i];
		
		if (mf->v3) {
			int new_glmode = mf->v4?GL_QUADS:GL_TRIANGLES;
			int new_shademodel = (mf->flag&ME_SMOOTH)?GL_SMOOTH:GL_FLAT;
			int new_matnr = mf->mat_nr+1;
			
			if(new_glmode!=glmode || new_shademodel!=shademodel || new_matnr!=matnr) {
				glEnd();

				if (new_matnr!=matnr) {
					set_gl_material(matnr=new_matnr);
				}

				glShadeModel(shademodel=new_shademodel);
				glBegin(glmode=new_glmode);
			}
			
			if (shademodel==GL_FLAT)
				glNormal3fv(&nors[i*3]);
				
			PASSVERT(mf->v1);
			PASSVERT(mf->v2);
			PASSVERT(mf->v3);
			if (mf->v4)
				PASSVERT(mf->v4);
		}
	}
	glEnd();
	
#undef PASSVERT
}

static void displistmesh_draw_colored(DispListMesh *dlm, unsigned char *vcols1, unsigned char *vcols2) 
{
	int i, lmode;
	
	glShadeModel(GL_SMOOTH);
	if (vcols2)
		glEnable(GL_CULL_FACE);
		
#define PASSVERT(vidx, fidx) {					\
	unsigned char *col= &colbase[fidx*4];		\
	glColor3ub(col[3], col[2], col[1]);			\
	glVertex3fv(dlm->mvert[(vidx)].co);			\
}

	glBegin(lmode= GL_QUADS);
	for (i=0; i<dlm->totface; i++) {
		MFace *mf= &dlm->mface[i];
		
		if (mf->v3) {
			int nmode= mf->v4?GL_QUADS:GL_TRIANGLES;
			unsigned char *colbase= &vcols1[i*16];
			
			if (nmode!=lmode) {
				glEnd();
				glBegin(lmode= nmode);
			}
			
			PASSVERT(mf->v1, 0);
			PASSVERT(mf->v2, 1);
			PASSVERT(mf->v3, 2);
			if (mf->v4)
				PASSVERT(mf->v4, 3);
			
			if (vcols2) {
				unsigned char *colbase= &vcols2[i*16];

				if (mf->v4)
					PASSVERT(mf->v4, 3);
				PASSVERT(mf->v3, 2);
				PASSVERT(mf->v2, 1);
				PASSVERT(mf->v1, 0);
			}
		}
	}
	glEnd();

	if (vcols2)
		glDisable(GL_CULL_FACE);
	
#undef PASSVERT
}

	// draw all edges of derived mesh as lines
static void draw_ss_edges(DispListMesh *dlm)
{
	MVert *mvert= dlm->mvert;
	int i;

	if (dlm->medge) {
		MEdge *medge= dlm->medge;
	
		glBegin(GL_LINES);
		for (i=0; i<dlm->totedge; i++, medge++) {
			glVertex3fv(mvert[medge->v1].co); 
			glVertex3fv(mvert[medge->v2].co);
		}
		glEnd();
	} else {
		MFace *mface= dlm->mface;

		for (i=0; i<dlm->totface; i++, mface++) {
			glBegin(GL_LINE_LOOP);
			glVertex3fv(mvert[mface->v1].co);
			glVertex3fv(mvert[mface->v2].co);
			if (mface->v3) {
				glVertex3fv(mvert[mface->v3].co);
				if (mface->v4)
					glVertex3fv(mvert[mface->v4].co);
			}
			glEnd();
		}
	}
}

	// draw exterior edges of derived mesh as lines
	//  o don't draw edges corresponding to hidden edges
	//  o if useCol is true set color based on selection flag
	//  o if onlySeams is true, only draw edges with seam set
	//
	// this function *must* be called on DLM's with ->medge defined
static void draw_ss_em_exterior_edges(DispListMesh *dlm, int useColor, char *baseCol, char *selCol, int onlySeams)
{
	MEdge *medge= dlm->medge;
	MVert *mvert= dlm->mvert;
	int a;
	
	glBegin(GL_LINES);
	for (a=0; a<dlm->totedge; a++, medge++) {
		if (medge->flag&ME_EDGEDRAW) {
			EditEdge *eed = dlm->editedge[a];
			if (eed && eed->h==0 && (!onlySeams || eed->seam)) {
				if (useColor) {
					glColor4ubv((eed->f & SELECT)?selCol:baseCol);
				}
				glVertex3fv(mvert[medge->v1].co); 
				glVertex3fv(mvert[medge->v2].co);
			}
		}
	}
	glEnd();
}

	// draw exterior edges of derived mesh as lines
	//
	// this function *must* be called on DLM's with ->medge defined
static void draw_ss_exterior_edges(DispListMesh *dlm)
{
	MEdge *medge= dlm->medge;
	MVert *mvert= dlm->mvert;
	int a;
	
	glBegin(GL_LINES);
	for (a=0; a<dlm->totedge; a++, medge++) {
		if (medge->flag&ME_EDGEDRAW) {
			glVertex3fv(mvert[medge->v1].co); 
			glVertex3fv(mvert[medge->v2].co);
		}
	}
	glEnd();
}

	// draw edges of edit mesh as lines
	//  o don't draw edges corresponding to hidden edges
	//  o if useCol is 0 don't set color
	//  o if useCol is 1 set color based on edge selection flag
	//  o if useCol is 2 set color based on vert selection flag
	//  o if onlySeams is true, only draw edges with seam set
static void draw_em_edges(EditMesh *em, int useColor, char *baseCol, char *selCol, int onlySeams) 
{
	EditEdge *eed;

	glBegin(GL_LINES);
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->h==0 && (!onlySeams || eed->seam)) {
			if (useColor==1) {
				glColor4ubv((eed->f&SELECT)?selCol:baseCol);
			} else if (useColor==2) {
				glColor4ubv((eed->v1->f&SELECT)?selCol:baseCol);
			}
			glVertex3fv(eed->v1->co);
			if (useColor==2) {
				glColor4ubv((eed->v2->f&SELECT)?selCol:baseCol);
			}
			glVertex3fv(eed->v2->co);
		}
	}
	glEnd();
}

	// draw editmesh faces as lines
	//  o no color
	//  o only if efa->h, efa->f&SELECT, and edges are unhidden
static void draw_em_sel_faces_as_lines(EditMesh *em)
{
	EditFace *efa;

	glBegin(GL_LINES);
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h==0 && (efa->f & SELECT)) { 
			if(efa->e1->h==0) {
				glVertex3fv(efa->v1->co);
				glVertex3fv(efa->v2->co);
			}
			if(efa->e2->h==0) {
				glVertex3fv(efa->v2->co);
				glVertex3fv(efa->v3->co);
			}
			if(efa->e3->h==0) {
				glVertex3fv(efa->e3->v1->co);
				glVertex3fv(efa->e3->v2->co);
			}
			if(efa->e4 && efa->e4->h==0) {
				glVertex3fv(efa->e4->v1->co);
				glVertex3fv(efa->e4->v2->co);
			}
		}
	}
	glEnd();
}

	// draw editmesh face normals as lines
	//  o no color
	//  o only if efa->h==0, efa->fgonf!=EM_FGON
	//  o scale normal by normalLength parameter
static void draw_em_face_normals(EditMesh *em, float normalLength)
{
	EditFace *efa;

	glBegin(GL_LINES);
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h==0 && efa->fgonf!=EM_FGON) {
			glVertex3fv(efa->cent);
			glVertex3f(	efa->cent[0] + normalLength*efa->n[0],
						efa->cent[1] + normalLength*efa->n[1],
						efa->cent[2] + normalLength*efa->n[2]);
			
		}
	}
	glEnd();
}

	// draw faces of derived mesh
	//  o if useCol is true set color based on selection flag
static void draw_ss_faces(DispListMesh *dlm, int useColor, char *baseCol, char *selCol) {
	MFace *mface= dlm->mface;
	int a;

	for(a=0; a<dlm->totface; a++, mface++) {
		if(mface->v3) {
			if (useColor) {
				EditFace *efa= dlm->editface[a];
				glColor4ubv((efa->f & SELECT)?selCol:baseCol);
			}
			
			glBegin(mface->v4?GL_QUADS:GL_TRIANGLES);
			glVertex3fv(dlm->mvert[mface->v1].co);
			glVertex3fv(dlm->mvert[mface->v2].co);
			glVertex3fv(dlm->mvert[mface->v3].co);
			if (mface->v4) glVertex3fv(dlm->mvert[mface->v4].co);
			glEnd();
		}
	}
}

	// draw faces of editmesh
	//  o if useCol is 1 set color based on selection flag
	//  o if useCol is 2 set material
	//  o only draw if efa->h==0
	//
	// XXX: Why the discrepancy between hidden faces in this and draw_ss_faces?
static void draw_em_faces(EditMesh *em, int useColor, char *baseCol, char *selCol, int useNormal) {
	EditFace *efa;
	int lastMat = -1;

	for (efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h==0) {
			if (useColor==1) {
				glColor4ubv((efa->f & SELECT)?selCol:baseCol);
			} else if (useColor==2) {
				if (lastMat!=efa->mat_nr+1) {
					set_gl_material(lastMat = efa->mat_nr+1);
				}
			}

			if (useNormal) {
				glNormal3fv(efa->n);
			}
			
			glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
			glVertex3fv(efa->v1->co);
			glVertex3fv(efa->v2->co);
			glVertex3fv(efa->v3->co);
			if(efa->v4) glVertex3fv(efa->v4->co);
			glEnd();
		}
	}
}

	// draw verts of mesh as points
	//  o no color
	//  o respect build effect if useBuildVars is true
	//  o draw verts using extverts array if non-NULL
static void draw_mesh_verts(Object *ob, int useBuildVars, float *extverts)
{
	Mesh *me = ob->data;
	int a, start=0, end=me->totvert;
	MVert *mvert = me->mvert;

	set_buildvars(ob, &start, &end);

	glBegin(GL_POINTS);
	if(extverts) {
		extverts+= 3*start;
		for(a= start; a<end; a++, extverts+=3) { /* DispList found, Draw displist */
			glVertex3fv(extverts);
		}
	}
	else {
		mvert+= start;
		for(a= start; a<end; a++, mvert++) { /* else Draw mesh verts directly */
			glVertex3fv(mvert->co);
		}
	}
	glEnd();
}

	// draw edges of mesh as lines
	//  o no color
	//  o respect build effect if useBuildVars is true
	//  o draw verts using extverts array if non-NULL
static void draw_mesh_edges(Object *ob, int useBuildVars, float *extverts)
{
	Mesh *me = ob->data;
	int a, start= 0, end= me->totface;
	MVert *mvert = me->mvert;
	MFace *mface = me->mface;
	float *f1, *f2, *f3, *f4;

	if (useBuildVars) {
		set_buildvars(ob, &start, &end);
		mface+= start;
	}
	
		// edges can't cope with buildvars, draw with
		// faces if build is in use.
	if(me->medge && start==0 && end==me->totface) {
		MEdge *medge= me->medge;
		
		glBegin(GL_LINES);
		for(a=me->totedge; a>0; a--, medge++) {
			if(medge->flag & ME_EDGEDRAW) {
				if(extverts) {
					f1= extverts+3*medge->v1;
					f2= extverts+3*medge->v2;
				}
				else {
					f1= (mvert+medge->v1)->co;
					f2= (mvert+medge->v2)->co;
				}
				glVertex3fv(f1); glVertex3fv(f2); 
			}
		}
		glEnd();
	}
	else {
		for(a=start; a<end; a++, mface++) {
			int test= mface->edcode;
			
			if(test) {
				if(extverts) {
					f1= extverts+3*mface->v1;
					f2= extverts+3*mface->v2;
				}
				else {
					f1= (mvert+mface->v1)->co;
					f2= (mvert+mface->v2)->co;
				}
				
				if(mface->v4) {
					if(extverts) {
						f3= extverts+3*mface->v3;
						f4= extverts+3*mface->v4;
					}
					else {
						f3= (mvert+mface->v3)->co;
						f4= (mvert+mface->v4)->co;
					}
					
					if(test== ME_V1V2+ME_V2V3+ME_V3V4+ME_V4V1) {
						glBegin(GL_LINE_LOOP);
							glVertex3fv(f1); glVertex3fv(f2); glVertex3fv(f3); glVertex3fv(f4);
						glEnd();
					}
					else if(test== ME_V1V2+ME_V2V3+ME_V3V4) {

						glBegin(GL_LINE_STRIP);
							glVertex3fv(f1); glVertex3fv(f2); glVertex3fv(f3); glVertex3fv(f4);
						glEnd();
					}
					else if(test== ME_V2V3+ME_V3V4+ME_V4V1) {

						glBegin(GL_LINE_STRIP);
							glVertex3fv(f2); glVertex3fv(f3); glVertex3fv(f4); glVertex3fv(f1);
						glEnd();
					}
					else if(test== ME_V3V4+ME_V4V1+ME_V1V2) {

						glBegin(GL_LINE_STRIP);
							glVertex3fv(f3); glVertex3fv(f4); glVertex3fv(f1); glVertex3fv(f2);
						glEnd();
					}
					else if(test== ME_V4V1+ME_V1V2+ME_V2V3) {

						glBegin(GL_LINE_STRIP);
							glVertex3fv(f4); glVertex3fv(f1); glVertex3fv(f2); glVertex3fv(f3);
						glEnd();
					}
					else {
						if(test & ME_V1V2) {

							glBegin(GL_LINE_STRIP);
								glVertex3fv(f1); glVertex3fv(f2);
							glEnd();
						}
						if(test & ME_V2V3) {

							glBegin(GL_LINE_STRIP);
								glVertex3fv(f2); glVertex3fv(f3);
							glEnd();
						}
						if(test & ME_V3V4) {

							glBegin(GL_LINE_STRIP);
								glVertex3fv(f3); glVertex3fv(f4);
							glEnd();
						}
						if(test & ME_V4V1) {

							glBegin(GL_LINE_STRIP);
								glVertex3fv(f4); glVertex3fv(f1);
							glEnd();
						}
					}
				}
				else if(mface->v3) {
					if(extverts) f3= extverts+3*mface->v3;
					else f3= (mvert+mface->v3)->co;

					if(test== ME_V1V2+ME_V2V3+ME_V3V1) {
						glBegin(GL_LINE_LOOP);
							glVertex3fv(f1); glVertex3fv(f2); glVertex3fv(f3);
						glEnd();
					}
					else if(test== ME_V1V2+ME_V2V3) {

						glBegin(GL_LINE_STRIP);
							glVertex3fv(f1); glVertex3fv(f2); glVertex3fv(f3);
						glEnd();
					}
					else if(test== ME_V2V3+ME_V3V1) {

						glBegin(GL_LINE_STRIP);
							glVertex3fv(f2); glVertex3fv(f3); glVertex3fv(f1);
						glEnd();
					}
					else if(test== ME_V1V2+ME_V3V1) {

						glBegin(GL_LINE_STRIP);
							glVertex3fv(f3); glVertex3fv(f1); glVertex3fv(f2);
						glEnd();
					}
					else {
						if(test & ME_V1V2) {

							glBegin(GL_LINE_STRIP);
								glVertex3fv(f1); glVertex3fv(f2);
							glEnd();
						}
						if(test & ME_V2V3) {

							glBegin(GL_LINE_STRIP);
								glVertex3fv(f2); glVertex3fv(f3);
							glEnd();
						}
						if(test & ME_V3V1) {

							glBegin(GL_LINE_STRIP);
								glVertex3fv(f3); glVertex3fv(f1);
							glEnd();
						}
					}
				}
				else if(test & ME_V1V2) {

					glBegin(GL_LINE_STRIP);
						glVertex3fv(f1); glVertex3fv(f2);
					glEnd();
				}
			}
		}
	}
}
		// draw ss exterior verts as bgl points
		//  o no color
		//  o only if eve->h, sel flag matches
static void draw_ss_em_exterior_verts(EditMesh *em, int sel) {
	EditVert *eve;

	bglBegin(GL_POINTS);
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->h==0 && (eve->f & SELECT)==sel && eve->ssco) 
			bglVertex3fv(eve->ssco);
	}
	bglEnd();		
}

		// draw editmesh verts as bgl points
		//  o no color
		//  o only if eve->h, sel flag matches
static void draw_em_verts(EditMesh *em, int sel) {
	EditVert *eve;

	bglBegin(GL_POINTS);
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->h==0 && (eve->f & SELECT)==sel ) bglVertex3fv(eve->co);
	}
	bglEnd();		
}

	// draw editmesh face centers as bgl points
	//  o no color
	//  o only if efa->h, efa->fgonf!=EM_FGON, matching sel
static void draw_em_face_centers(EditMesh *em, int sel) {
	EditFace *efa;

	bglBegin(GL_POINTS);
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h==0 && efa->fgonf!=EM_FGON && (efa->f&SELECT)==sel) {
			bglVertex3fv(efa->cent);
		}
	}
	bglEnd();
}

static void draw_mesh_faces(Object *ob, int useBuildVars, float *extverts, float *nors)
{
	Mesh *me = ob->data;
	MVert *mvert= me->mvert;
	MFace *mface= me->mface;
	int a, start=0, end=me->totface;
	int glmode=-1, cullmode=-1, shademodel=-1, matnr=-1;

	if (useBuildVars) {
		set_buildvars(ob, &start, &end);
		mface+= start;
	}
	
#define PASSVERT(co, index, punoBit) {			\
	if (shademodel==GL_SMOOTH) {				\
		short *no = (mvert+index)->no;			\
		if (mface->puno&punoBit) {				\
			glNormal3s(-no[0], -no[1], -no[2]); \
		} else {								\
			glNormal3sv(no);					\
		}										\
	}											\
	glVertex3fv(co);							\
}

	glBegin(glmode=GL_QUADS);
	for(a=start; a<end; a++, mface++, nors+=3) {
		if(mface->v3) {
			int new_glmode, new_matnr, new_shademodel;
			float *v1, *v2, *v3, *v4;

			if(extverts) {
				v1= extverts+3*mface->v1;
				v2= extverts+3*mface->v2;
				v3= mface->v3?extverts+3*mface->v3:NULL;
				v4= mface->v4?extverts+3*mface->v4:NULL;
			}
			else {
				v1= (mvert+mface->v1)->co;
				v2= (mvert+mface->v2)->co;
				v3= mface->v3?(mvert+mface->v3)->co:NULL;
				v4= mface->v4?(mvert+mface->v4)->co:NULL;
			}
				
			new_glmode = v4?GL_QUADS:GL_TRIANGLES;
			new_matnr = mface->mat_nr+1;
			new_shademodel = !(me->flag&ME_AUTOSMOOTH) && (mface->flag & ME_SMOOTH);
			
			if (new_glmode!=glmode || new_matnr!=matnr || new_shademodel!=shademodel) {
				glEnd();

				if (new_matnr!=matnr) {
					set_gl_material(matnr=new_matnr);
				}

				glShadeModel(shademodel=new_shademodel);
				glBegin(glmode=new_glmode);
			}
				
			if(shademodel==GL_FLAT) 
				glNormal3fv(nors);

			PASSVERT(v1, mface->v1, ME_FLIPV1);
			PASSVERT(v2, mface->v2, ME_FLIPV2);
			PASSVERT(v3, mface->v3, ME_FLIPV3);
			if (v4) {
				PASSVERT(v4, mface->v4, ME_FLIPV4);
			}
		}
	}
	glEnd();

	glShadeModel(GL_FLAT);
#undef PASSVERT
}

static void draw_mesh_loose_edges(Object *ob, int useBuildVars, float *extverts)
{
	Mesh *me = ob->data;
	MVert *mvert= me->mvert;
	MFace *mface= me->mface;
	int a, start=0, end=me->totface;

	if (useBuildVars) {
		set_buildvars(ob, &start, &end);
		mface+= start;
	}
		
	glBegin(GL_LINES);
	for(a=start; a<end; a++, mface++) {
		float *v1, *v2, *v3, *v4;

		if(extverts) {
			v1= extverts+3*mface->v1;
			v2= extverts+3*mface->v2;
			v3= mface->v3?extverts+3*mface->v3:NULL;
			v4= mface->v4?extverts+3*mface->v4:NULL;
		}
		else {
			v1= (mvert+mface->v1)->co;
			v2= (mvert+mface->v2)->co;
			v3= mface->v3?(mvert+mface->v3)->co:NULL;
			v4= mface->v4?(mvert+mface->v4)->co:NULL;
		}
			
		if(!mface->v3) {
			glVertex3fv(v1);
			glVertex3fv(v2);
		} 
	}
	glEnd();
}

static void draw_mesh_colored(Object *ob, int useBuildVars, int useTwoSide, unsigned int *col1, unsigned int *col2, float *extverts)
{
	Mesh *me= ob->data;
	MVert *mvert= me->mvert;
	MFace *mface= me->mface;
	int a, glmode, start=0, end=me->totface;
	unsigned char *cp1, *cp2;

	if (useBuildVars) {
		set_buildvars(ob, &start, &end);
		mface+= start;
		col1+= 4*start;
		if(col2) col2+= 4*start;
	}
	
	cp1= (char *)col1;
	if(col2) {
		cp2= (char *)col2;
	} else {
		cp2= NULL;
		useTwoSide= 0;
	}

	glEnable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
	glBegin(glmode=GL_QUADS);
	for(a=start; a<end; a++, mface++, cp1+= 16) {
		if(mface->v3) {
			int new_glmode= mface->v4?GL_QUADS:GL_TRIANGLES;
			float *v1, *v2, *v3, *v4;

			if(extverts) {
				v1= extverts+3*mface->v1;
				v2= extverts+3*mface->v2;
				v3= extverts+3*mface->v3;
				v4= mface->v4?extverts+3*mface->v4:NULL;
			}
			else {
				v1= (mvert+mface->v1)->co;
				v2= (mvert+mface->v2)->co;
				v3= (mvert+mface->v3)->co;
				v4= mface->v4?(mvert+mface->v4)->co:NULL;
			}

			if (new_glmode!=glmode) {
				glEnd();
				glBegin(glmode= new_glmode);
			}
				
			glColor3ub(cp1[3], cp1[2], cp1[1]);
			glVertex3fv( v1 );
			glColor3ub(cp1[7], cp1[6], cp1[5]);
			glVertex3fv( v2 );
			glColor3ub(cp1[11], cp1[10], cp1[9]);
			glVertex3fv( v3 );
			if(v4) {
				glColor3ub(cp1[15], cp1[14], cp1[13]);
				glVertex3fv( v4 );
			}
				
			if(useTwoSide) {
				glColor3ub(cp2[11], cp2[10], cp2[9]);
				glVertex3fv( v3 );
				glColor3ub(cp2[7], cp2[6], cp2[5]);
				glVertex3fv( v2 );
				glColor3ub(cp2[3], cp2[2], cp2[1]);
				glVertex3fv( v1 );
				if(mface->v4) {
					glColor3ub(cp2[15], cp2[14], cp2[13]);
					glVertex3fv( v4 );
				}
			}
		}
		if(col2) cp2+= 16;
	}
	glEnd();

	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
}

/* Second section of routines: Combine first sets to form fancy
 * drawing routines (for example rendering twice to get overlays).
 *
 * Also includes routines that are basic drawing but are too
 * specialized to be split out (like drawing creases or measurements).
 */

/* EditMesh drawing routines*/

static void draw_em_fancy_verts(EditMesh *em, int optimal, int sel)
{
	char col[4], fcol[4];
	int pass;

	if(G.zbuf) glDepthMask(0);		// disable write in zbuffer, zbuf select

	BIF_GetThemeColor3ubv(sel?TH_VERTEX_SELECT:TH_VERTEX, col);
	BIF_GetThemeColor3ubv(sel?TH_FACE_DOT:TH_WIRE, fcol);

	for (pass=0; pass<2; pass++) {
		float size = BIF_GetThemeValuef(TH_VERTEX_SIZE);
		float fsize = BIF_GetThemeValuef(TH_FACEDOT_SIZE);

		if (pass==0) {
			if(G.zbuf && !(G.vd->flag&V3D_ZBUF_SELECT)) {
				glDisable(GL_DEPTH_TEST);
					
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
			
		if(G.scene->selectmode & SCE_SELECT_VERTEX) {
			glPointSize(size);
			glColor4ubv(col);
			if(optimal) {
				draw_ss_em_exterior_verts(em, sel);
			}
			else {
				draw_em_verts(em, sel);
			}
		}
		
		if(G.scene->selectmode & SCE_SELECT_FACE) {
			glPointSize(fsize);
			glColor4ubv(fcol);
			draw_em_face_centers(em, sel);
		}
		
		if (pass==0) {
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
		}
	}

	if(G.zbuf) glDepthMask(1);
	glPointSize(1.0);
}

static void draw_em_fancy_edges(EditMesh *em, DispListMesh *dlm, int optimal)
{
	int pass;
	char wire[4], sel[4];

	/* since this function does transparant... */
	BIF_GetThemeColor3ubv(TH_EDGE_SELECT, sel);
	BIF_GetThemeColor3ubv(TH_WIRE, wire);

	for (pass=0; pass<2; pass++) {
			/* show wires in transparant when no zbuf clipping for select */
		if (pass==0) {
			if (G.zbuf && (G.vd->flag & V3D_ZBUF_SELECT)==0) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glEnable(GL_BLEND);
				glDisable(GL_DEPTH_TEST);

				wire[3] = sel[3] = 85;
			} else {
				continue;
			}
		} else {
			wire[3] = sel[3] = 255;
		}

		if(G.scene->selectmode == SCE_SELECT_FACE) {
			if(optimal) {
				draw_ss_em_exterior_edges(dlm, 1, wire, sel, 0);
			}
			else {
				/* draw faces twice, to have selected ones on top */
				/* we draw unselected the edges though, so they show in face mode */
				glColor4ubv(wire);
				draw_em_edges(em, 0, NULL, NULL, 0);

				glColor4ubv(sel);
				draw_em_sel_faces_as_lines(em);
			}
		}	
		else if( (G.f & G_DRAWEDGES) || (G.scene->selectmode & SCE_SELECT_EDGE) ) {	
			/* Use edge highlighting */
			
			/* (bleeding edges) to illustrate selection is defined on vertex basis */
			/* but cannot do with subdivided edges... */
			if(!optimal && (G.scene->selectmode & SCE_SELECT_VERTEX)) {
				glShadeModel(GL_SMOOTH);
				draw_em_edges(em, 2, wire, sel, 0);
				glShadeModel(GL_FLAT);
			}
			else {
				if(optimal) {
					draw_ss_em_exterior_edges(dlm, 1, wire, sel, 0);
				}
				else {
					draw_em_edges(em, 1, wire, sel, 0);
				}
			}
		}
		else {
			glColor4ubv(wire);
			if(optimal) {
				draw_ss_em_exterior_edges(dlm, 0, NULL, NULL, 0);
			}
			else {
				draw_em_edges(em, 0, NULL, NULL, 0);
			}
		}

		if (pass==0) {
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
		}
	}
}	

static void draw_em_creases(EditMesh *em)
{
	EditEdge *eed;
	float fac, *v1, *v2, vec[3];
	
	glLineWidth(3.0);
	glBegin(GL_LINES);
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->h==0 && eed->crease!=0.0) {
			if(eed->f & SELECT) BIF_ThemeColor(TH_EDGE_SELECT);
			else BIF_ThemeColor(TH_WIRE);
			
			v1= eed->v1->co; v2= eed->v2->co;
			VECSUB(vec, v2, v1);
			fac= 0.5 + eed->crease/2.0;
			glVertex3f(v1[0] + fac*vec[0], v1[1] + fac*vec[1], v1[2] + fac*vec[2] );
			glVertex3f(v2[0] - fac*vec[0], v2[1] - fac*vec[1], v2[2] - fac*vec[2] );
		}
	}
	glEnd();
	glLineWidth(1.0);
}

static void draw_em_measure_stats(EditMesh *em)
{
	EditEdge *eed;
	EditFace *efa;
	float *v1, *v2, *v3, *v4, fvec[3];
	char val[32]; /* Stores the measurement display text here */
	float area, col[3]; /* area of the face,  colour of the text to draw */
	
	if(G.zbuf && (G.vd->flag & V3D_ZBUF_SELECT)==0)
		glDisable(GL_DEPTH_TEST);

	if(G.zbuf) bglPolygonOffset(5.0);
	
	if(G.f & G_DRAW_EDGELEN) {
		BIF_GetThemeColor3fv(TH_TEXT, col);
		/* make color a bit more red */
		if(col[0]> 0.5) {col[1]*=0.7; col[2]*= 0.7;}
		else col[0]= col[0]*0.7 + 0.3;
		glColor3fv(col);
		
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->f & SELECT) {
				v1= eed->v1->co;
				v2= eed->v2->co;
				
				glRasterPos3f( 0.5*(v1[0]+v2[0]),  0.5*(v1[1]+v2[1]),  0.5*(v1[2]+v2[2]));
				sprintf(val,"%.3f", VecLenf(v1, v2));
				BMF_DrawString( G.fonts, val);
			}
		}
	}

	if(G.f & G_DRAW_FACEAREA) {
		BIF_GetThemeColor3fv(TH_TEXT, col);
		/* make color a bit more green */
		if(col[1]> 0.5) {col[0]*=0.7; col[2]*= 0.7;}
		else col[1]= col[1]*0.7 + 0.3;
		glColor3fv(col);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				if (efa->v4)
					area=  AreaQ3Dfl( efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co);
				else
					area = AreaT3Dfl( efa->v1->co, efa->v2->co, efa->v3->co);

				sprintf(val,"%.3f", area);
				glRasterPos3fv(efa->cent);
				BMF_DrawString( G.fonts, val);
			}
		}
	}

	if(G.f & G_DRAW_EDGEANG) {
		EditEdge *e1, *e2, *e3, *e4;
		
		BIF_GetThemeColor3fv(TH_TEXT, col);
		/* make color a bit more blue */
		if(col[2]> 0.5) {col[0]*=0.7; col[1]*= 0.7;}
		else col[2]= col[2]*0.7 + 0.3;
		glColor3fv(col);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			v1= efa->v1->co;
			v2= efa->v2->co;
			v3= efa->v3->co;
			if(efa->v4) v4= efa->v4->co; else v4= v3;
			e1= efa->e1;
			e2= efa->e2;
			e3= efa->e3;
			if(efa->e4) e4= efa->e4; else e4= e3;
			
			/* Calculate the angles */
				
			if(e4->f & e1->f & SELECT ) {
				/* Vec 1 */
				sprintf(val,"%.3f", VecAngle3(v4, v1, v2));
				fvec[0]= 0.2*efa->cent[0] + 0.8*v1[0];
				fvec[1]= 0.2*efa->cent[1] + 0.8*v1[1];
				fvec[2]= 0.2*efa->cent[2] + 0.8*v1[2];
				glRasterPos3fv(fvec);
				BMF_DrawString( G.fonts, val);
			}
			if(e1->f & e2->f & SELECT ) {
				/* Vec 2 */
				sprintf(val,"%.3f", VecAngle3(v1, v2, v3));
				fvec[0]= 0.2*efa->cent[0] + 0.8*v2[0];
				fvec[1]= 0.2*efa->cent[1] + 0.8*v2[1];
				fvec[2]= 0.2*efa->cent[2] + 0.8*v2[2];
				glRasterPos3fv(fvec);
				BMF_DrawString( G.fonts, val);
			}
			if(e2->f & e3->f & SELECT ) {
				/* Vec 3 */
				if(efa->v4) 
					sprintf(val,"%.3f", VecAngle3(v2, v3, v4));
				else
					sprintf(val,"%.3f", VecAngle3(v2, v3, v1));
				fvec[0]= 0.2*efa->cent[0] + 0.8*v3[0];
				fvec[1]= 0.2*efa->cent[1] + 0.8*v3[1];
				fvec[2]= 0.2*efa->cent[2] + 0.8*v3[2];
				glRasterPos3fv(fvec);
				BMF_DrawString( G.fonts, val);
			}
				/* Vec 4 */
			if(efa->v4) {
				if(e3->f & e4->f & SELECT ) {
					sprintf(val,"%.3f", VecAngle3(v3, v4, v1));

					fvec[0]= 0.2*efa->cent[0] + 0.8*v4[0];
					fvec[1]= 0.2*efa->cent[1] + 0.8*v4[1];
					fvec[2]= 0.2*efa->cent[2] + 0.8*v4[2];
					glRasterPos3fv(fvec);
					BMF_DrawString( G.fonts, val);
				}
			}
		}
	}    
	
	if(G.zbuf) {
		glEnable(GL_DEPTH_TEST);
		bglPolygonOffset(0.0);
	}
}

static void draw_em_fancy(Object *ob, EditMesh *em, DispListMesh *meDLM, float *meNors, int optimal, int dt)
{
	extern float editbutsize;	/* buttons.c */
	Mesh *me = ob->data;

	if(dt>OB_WIRE) {
		init_gl_materials(ob);
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, me->flag & ME_TWOSIDED);

		glEnable(GL_LIGHTING);
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);
		
		if(meDLM) {
			displistmesh_draw_solid(meDLM, meNors);
		} else {
			draw_em_faces(em, 2, NULL, NULL, 1);
		}

		glFrontFace(GL_CCW);
		glDisable(GL_LIGHTING);

			// Setup for drawing wire over, disable zbuffer
			// write to show selected edge wires better
		BIF_ThemeColor(TH_WIRE);

		bglPolygonOffset(1.0);
		glDepthMask(0);
	} else {
		if (meDLM) {
			BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.7);
			if (optimal) {
				draw_ss_exterior_edges(meDLM);
			} else {
				draw_ss_edges(meDLM);
			}
		}
	}

	if( (G.f & (G_FACESELECT+G_DRAWFACES))) {	/* transp faces */
		char col1[4], col2[4];
			
		BIF_GetThemeColor4ubv(TH_FACE, col1);
		BIF_GetThemeColor4ubv(TH_FACE_SELECT, col2);
		
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glDepthMask(0);		// disable write in zbuffer, needed for nice transp
		
		if(optimal) {
			draw_ss_faces(meDLM, 1, col1, col2);
		}
		else {
			draw_em_faces(em, 1, col1, col2, 0);
		}

		glDisable(GL_BLEND);
		glDepthMask(1);		// restore write in zbuffer
	}

	/* here starts all fancy draw-extra over */

	if(G.f & G_DRAWSEAMS) {
		BIF_ThemeColor(TH_EDGE_SEAM);
		glLineWidth(2);

		if(optimal) {
			draw_ss_em_exterior_edges(meDLM, 0, NULL, NULL, 1);
		}
		else {
			draw_em_edges(em, 0, NULL, NULL, 1);
		}

		glColor3ub(0,0,0);
		glLineWidth(1);
	}

	draw_em_fancy_edges(em, meDLM, optimal);

	if(G.f & G_DRAWCREASES) {
		draw_em_creases(em);
	}

	if(ob==G.obedit) {
			// XXX Not clear this is needed here. - zr
		calc_meshverts();

		draw_em_fancy_verts(em, optimal, 0);
		draw_em_fancy_verts(em, optimal, 1);

		if(G.f & G_DRAWNORMALS) {
			BIF_ThemeColor(TH_NORMAL);
			draw_em_face_normals(em, editbutsize);
		}

		if(G.f & (G_DRAW_EDGELEN|G_DRAW_FACEAREA|G_DRAW_EDGEANG))
			draw_em_measure_stats(em);
	}

	if(dt>OB_WIRE) {
		glDepthMask(1);
		bglPolygonOffset(0.0);
	}
}

/* Mesh drawing routines*/

static void draw_mesh_object_outline(Object *ob, DispListMesh *meDLM, float *obExtVerts)
{
	glLineWidth(2.0);
	glDepthMask(0);
				
	if(meDLM) {
		draw_ss_edges(meDLM);
	} 
	else {
		draw_mesh_edges(ob, 1, obExtVerts);
	}
				
	glLineWidth(1.0);
	glDepthMask(1);
}

static void draw_mesh_fancy(Object *ob, DispListMesh *meDLM, float *meNors, int optimal, int dt)
{
	Mesh *me = ob->data;
	Material *ma= give_current_material(ob, 1);
	int hasHaloMat = (ma && (ma->mode&MA_HALO));
	int draw_wire = ob->dtx&OB_DRAWWIRE;
	DispList *dl, *obDL = ob->disp.first;
	unsigned int *obCol1 = obDL?obDL->col1:NULL;
	unsigned int *obCol2 = obDL?obDL->col2:NULL;
	float *obExtVerts;

	dl = find_displist(&ob->disp, DL_VERTS);
	obExtVerts = dl?dl->verts:NULL;

		// Unwanted combination.
	if (G.f&G_FACESELECT) draw_wire = 0;

		// This is only for objects from the decimator and
		// is a temporal solution, a reconstruction of the
		// displist system should take care of it (zr/ton)
	if(obDL && obDL->mesh) {
		if (obDL->mesh->medge && (obDL->mesh->flag&ME_OPT_EDGES)) {
			draw_ss_exterior_edges(obDL->mesh);
		} else {
			draw_ss_edges(obDL->mesh);
		}
	}
	else if(dt==OB_BOUNDBOX) {
		draw_bounding_volume(ob);
	}
	else if(hasHaloMat || me->totface==0 || me->totedge==0) {
		glPointSize(1.5);
		draw_mesh_verts(ob, 1, obExtVerts);
		glPointSize(1.0);
	}
	else if(dt==OB_WIRE) {
		draw_wire = 1;
	}
	else if( (ob==OBACT && (G.f & G_FACESELECT)) || (G.vd->drawtype==OB_TEXTURE && dt>OB_SOLID)) {
		draw_tface_mesh(ob, ob->data, dt);
	}
	else if(dt==OB_SOLID ) {
		if ((G.vd->flag&V3D_SELECT_OUTLINE) && (ob->flag&SELECT) && !draw_wire) {
			draw_mesh_object_outline(ob, meDLM, obExtVerts);
		}

		init_gl_materials(ob);
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, me->flag & ME_TWOSIDED );

		glEnable(GL_LIGHTING);
		glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);
		
				/* vertexpaint only true when selecting */
		if ((G.f & (G_VERTEXPAINT+G_FACESELECT+G_TEXTUREPAINT+G_WEIGHTPAINT)) && ob==OBACT) {
			draw_mesh_faces(ob, 1, obExtVerts, NULL);
		} else {
			if(meDLM) {
				displistmesh_draw_solid(meDLM, meNors);
			} else {
				draw_mesh_faces(ob, 1, obExtVerts, meNors);
			}
		}

		glFrontFace(GL_CCW);
		glDisable(GL_LIGHTING);

		if(!meDLM) {
			BIF_ThemeColor(TH_WIRE);

			draw_mesh_loose_edges(ob, 1, obExtVerts);
		}
	}
	else if(dt==OB_SHADED) {
		if( (G.f & G_WEIGHTPAINT)) {
			unsigned int *wtcol = calc_weightpaint_colors(ob);
			draw_mesh_colored(ob, 1, me->flag&ME_TWOSIDED, (unsigned int*)wtcol, 0, obExtVerts);
			MEM_freeN (wtcol);
		}
		else if((G.f & (G_VERTEXPAINT+G_TEXTUREPAINT)) && me->mcol) {
			draw_mesh_colored(ob, 1, me->flag&ME_TWOSIDED, (unsigned int *)me->mcol, 0, obExtVerts);
		}
		else if((G.f & (G_VERTEXPAINT+G_TEXTUREPAINT)) && me->tface) {
			tface_to_mcol(me);
			draw_mesh_colored(ob, 1, me->flag&ME_TWOSIDED, (unsigned int *)me->mcol, 0, obExtVerts);
			MEM_freeN(me->mcol); 
			me->mcol= 0;
		}
		else {
			if ((G.vd->flag&V3D_SELECT_OUTLINE) && (ob->flag&SELECT) && !draw_wire) {
				draw_mesh_object_outline(ob, meDLM, obExtVerts);
			}

			if(meDLM) {
				displistmesh_draw_colored(meDLM, (unsigned char*) obCol1, (unsigned char*) obCol2);
			} else {
				draw_mesh_colored(ob, 1, me->flag&ME_TWOSIDED, obCol1, obCol2, obExtVerts);
			}
		}
	}

	if (draw_wire) {
			/* If drawing wire and drawtype is not OB_WIRE then we are
				* overlaying the wires.
				*/
		if (dt!=OB_WIRE) {
			if(ob->flag & SELECT) {
				BIF_ThemeColor((ob==OBACT)?TH_ACTIVE:TH_SELECT);
			} else {
				BIF_ThemeColor(TH_WIRE);
			}

			bglPolygonOffset(1.0);
			glDepthMask(0);	// disable write in zbuffer, selected edge wires show better
		}

		if(meDLM) {
				// XXX is !dlm->medge possible?
			if (meDLM->medge && (meDLM->flag&ME_OPT_EDGES)) {
				draw_ss_exterior_edges(meDLM);
			}
			else {
				draw_ss_edges(meDLM);
			}
		} 
		else {
			draw_mesh_edges(ob, 1, obExtVerts);
		}

		if (dt!=OB_WIRE) {
			glDepthMask(1);
			bglPolygonOffset(0.0);
		}
	}
}

static void draw_mesh_object(Object *ob, int dt)
{
	Mesh *me= ob->data;
	DispList *meDL;
	DispListMesh *meDLM;
	float *meNors;
	int optimal;

		/* First thing is to make sure any needed data is calculated.
		 * This includes displists on both Object and Mesh, the
		 * bounding box, DispList normals, and shaded colors.
		 *
		 * Sometimes the calculation is skipped if it won't be used,
		 * but at the moment it is hard to verify this for sure in
		 * the code. Tread carefully!
		 */

		/* Check for need for displist (it's zero when parent, key, or hook changed) */
	if(ob->disp.first==NULL) {
		if(ob->parent && ob->partype==PARSKEL) makeDispList(ob);
		else if(ob->parent && ob->parent->type==OB_LATTICE) makeDispList(ob);
		else if(ob->hooks.first) makeDispList(ob);
		else if(ob->softflag & 0x01) makeDispList(ob);
		else if(ob->effect.first) {	// as last check
			Effect *eff= ob->effect.first;
			if(eff->type==EFF_WAVE) makeDispList(ob);
		}
	}
	if(me->disp.first==NULL && mesh_uses_displist(me)) {
		makeDispList(ob);
	}

	if (ob==G.obedit) {
		if (dt>OB_WIRE && mesh_uses_displist(me)) {
			DispList *dl = me->disp.first;

			if(!dl || !dl->nors) {
				addnormalsDispList(ob, &me->disp);
			}
		}
	} else {
		if(me->bb==NULL) tex_space_mesh(me);
		if(me->totface>4) if(boundbox_clip(ob->obmat, me->bb)==0) return;

		if (dt==OB_SOLID) {
			DispList *dl = me->disp.first;

			if(!dl || !dl->nors) {
				addnormalsDispList(ob, &me->disp);
			}
		}

		if (dt==OB_SHADED && !(G.f & (G_WEIGHTPAINT|G_VERTEXPAINT|G_TEXTUREPAINT))) {
			DispList *dl = ob->disp.first;

			if (!dl || !dl->col1) {
				shadeDispList(ob);
			}
		}
	}

	meDL = me->disp.first;
	meNors = meDL?meDL->nors:NULL;
	meDLM = (mesh_uses_displist(me) && meDL)?meDL->mesh:NULL;
	optimal = meDLM && meDLM->medge && me->flag&ME_OPT_EDGES;

	if(ob==G.obedit || (G.obedit && ob->data==G.obedit->data)) {
		draw_em_fancy(ob, G.editMesh, meDLM, meNors, optimal, dt);
	}
	else {
		draw_mesh_fancy(ob, meDLM, meNors, optimal, dt);
	}
}

/* ************** DRAW DISPLIST ****************** */

static int draw_index_wire= 1;
static int index3_nors_incr= 1;

static void drawDispListwire(ListBase *dlbase)
{
		// This routine has been cleaned so that no DispLists of type
		// DispListMesh should go through here. - zr
	DispList *dl;
	int parts, nr, ofs, *index;
	float *data;

	if(dlbase==0) return;

	dl= dlbase->first;
	while(dl) {
		data= dl->verts;
	
		switch(dl->type) {
		case DL_SEGM:
			parts= dl->parts;
			while(parts--) {
				nr= dl->nr;
				glBegin(GL_LINE_STRIP);
				while(nr--) {
					glVertex3fv(data);
					data+=3;
				}
				glEnd();
			}
			break;
		case DL_POLY:
			parts= dl->parts;
			while(parts--) {
				nr= dl->nr;
				glBegin(GL_LINE_LOOP);
				while(nr--) {
					glVertex3fv(data);
					data+=3;
				}
				glEnd();
			}
			break;
		case DL_SURF:
			parts= dl->parts;
			while(parts--) {
				nr= dl->nr;
				if(dl->flag & DL_CYCL_U) glBegin(GL_LINE_LOOP);
				else glBegin(GL_LINE_STRIP);

				while(nr--) {
					glVertex3fv(data);
					data+=3;
				}
				glEnd();
			}
			ofs= 3*dl->nr;
			nr= dl->nr;
			while(nr--) {
				data= (  dl->verts )+3*nr;
				parts= dl->parts;
				if(dl->flag & DL_CYCL_V) glBegin(GL_LINE_LOOP);
				else glBegin(GL_LINE_STRIP);
				
				while(parts--) {
					glVertex3fv(data);
					data+=ofs;
				}
				glEnd();
			}
			break;
			
		case DL_INDEX3:
			if(draw_index_wire) {
				parts= dl->parts;
				data= dl->verts;
				index= dl->index;
				while(parts--) {

					glBegin(GL_LINE_LOOP);
						glVertex3fv(data+3*index[0]);
						glVertex3fv(data+3*index[1]);
						glVertex3fv(data+3*index[2]);
					glEnd();
					index+= 3;
				}
			}
			break;
			
		case DL_INDEX4:
			if(draw_index_wire) {
				parts= dl->parts;
				data= dl->verts;
				index= dl->index;
				while(parts--) {

					glBegin(GL_LINE_LOOP);
						glVertex3fv(data+3*index[0]);
						glVertex3fv(data+3*index[1]);
						glVertex3fv(data+3*index[2]);
						if(index[3]) glVertex3fv(data+3*index[3]);
					glEnd();
					index+= 4;
				}
			}
			break;
		}
		dl= dl->next;
	}
}

static void drawDispListsolid(ListBase *lb, Object *ob)
{
	DispList *dl;
	int parts, ofs, p1, p2, p3, p4, a, b, *index;
	float *data, *v1, *v2, *v3, *v4;
	float *ndata, *n1, *n2, *n3, *n4;
	
	if(lb==0) return;
	
	glEnable(GL_LIGHTING);
	
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
		case DL_SURF:

			set_gl_material(dl->col+1);
			
			if(dl->rt & CU_SMOOTH) glShadeModel(GL_SMOOTH);
			else glShadeModel(GL_FLAT);

			for(a=0; a<dl->parts; a++) {
				
				DL_SURFINDEX(dl->flag & DL_CYCL_U, dl->flag & DL_CYCL_V, dl->nr, dl->parts);
				
				v1= data+ 3*p1; 
				v2= data+ 3*p2;
				v3= data+ 3*p3; 
				v4= data+ 3*p4;
				n1= ndata+ 3*p1; 
				n2= ndata+ 3*p2;
				n3= ndata+ 3*p3; 
				n4= ndata+ 3*p4;
				
				glBegin(GL_QUAD_STRIP);
				
				glNormal3fv(n2); glVertex3fv(v2);
				glNormal3fv(n4); glVertex3fv(v4);

				for(; b<dl->nr; b++) {
					
					glNormal3fv(n1); glVertex3fv(v1);
					glNormal3fv(n3); glVertex3fv(v3);

					v2= v1; v1+= 3;
					v4= v3; v3+= 3;
					n2= n1; n1+= 3;
					n4= n3; n3+= 3;
				}
				
				
				glEnd();
			}
			break;

		case DL_INDEX3:
		
			parts= dl->parts;
			data= dl->verts;
			ndata= dl->nors;
			index= dl->index;

			set_gl_material(dl->col+1);
							
			/* voor polys only one normal needed */
			if(index3_nors_incr==0) {
				while(parts--) {

					glBegin(GL_TRIANGLES);
						glNormal3fv(ndata);
						glVertex3fv(data+3*index[0]);
						glVertex3fv(data+3*index[1]);
						glVertex3fv(data+3*index[2]);
					glEnd();
					index+= 3;
				}
			}
			else {
				while(parts--) {

					glBegin(GL_TRIANGLES);
						ofs= 3*index[0];
						glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
						ofs= 3*index[1];
						glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
						ofs= 3*index[2];
						glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					glEnd();
					index+= 3;
				}
			}
			break;

		case DL_INDEX4:

			parts= dl->parts;
			data= dl->verts;
			ndata= dl->nors;
			index= dl->index;

			set_gl_material(dl->col+1);
		
			while(parts--) {

				glBegin(index[3]?GL_QUADS:GL_TRIANGLES);
					ofs= 3*index[0];
					glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					ofs= 3*index[1];
					glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					ofs= 3*index[2];
					glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					if(index[3]) {
						ofs= 3*index[3];
						glNormal3fv(ndata+ofs); glVertex3fv(data+ofs);
					}
				glEnd();
				index+= 4;
			}
			break;
		}
		dl= dl->next;
	}

	glShadeModel(GL_FLAT);
	glDisable(GL_LIGHTING);
	glFrontFace(GL_CCW);
}

static void drawDispListshaded(ListBase *lb, Object *ob)
{
		// This routine has been cleaned so that no DispLists of type
		// DispListMesh should go through here. - zr
	DispList *dl, *dlob;
	int parts, p1, p2, p3, p4, a, b, *index;
	float *data, *v1, *v2, *v3, *v4;/*  , *extverts=0 */
	unsigned int *cdata, *c1, *c2, *c3, *c4;
	char *cp;

	if(lb==0) return;

	glShadeModel(GL_SMOOTH);

	dl= lb->first;
	dlob= ob->disp.first;
	while(dl && dlob) {
		
		cdata= dlob->col1;
		data= dl->verts;
		if(cdata==0) break;
		
		switch(dl->type) {
		case DL_SURF:

			for(a=0; a<dl->parts; a++) {

				DL_SURFINDEX(dl->flag & DL_CYCL_U, dl->flag & DL_CYCL_V, dl->nr, dl->parts);

				v1= data+ 3*p1; 
				v2= data+ 3*p2;
				v3= data+ 3*p3; 
				v4= data+ 3*p4;
				c1= cdata+ p1; 
				c2= cdata+ p2;
				c3= cdata+ p3; 
				c4= cdata+ p4;

				for(; b<dl->nr; b++) {

					glBegin(GL_QUADS);
						cp= (char *)c1;
						glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(v1);
						cp= (char *)c2;
						glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(v2);
						cp= (char *)c4;
						glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(v4);
						cp= (char *)c3;
						glColor3ub(cp[3], cp[2], cp[1]);
						glVertex3fv(v3);
					glEnd();

					v2= v1; v1+= 3;
					v4= v3; v3+= 3;
					c2= c1; c1++;
					c4= c3; c3++;
				}
			}
			break;

		case DL_INDEX3:
			
			parts= dl->parts;
			index= dl->index;
			
			while(parts--) {

				glBegin(GL_TRIANGLES);
					cp= (char *)(cdata+index[0]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[0]);

					cp= (char *)(cdata+index[1]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[1]);

					cp= (char *)(cdata+index[2]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[2]);
				glEnd();
				index+= 3;
			}
			break;

		case DL_INDEX4:
		
			parts= dl->parts;
			index= dl->index;
			while(parts--) {

				glBegin(index[3]?GL_QUADS:GL_TRIANGLES);
					cp= (char *)(cdata+index[0]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[0]);

					cp= (char *)(cdata+index[1]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[1]);

					cp= (char *)(cdata+index[2]);
					glColor3ub(cp[3], cp[2], cp[1]);					
					glVertex3fv(data+3*index[2]);
					
					if(index[3]) {
					
						cp= (char *)(cdata+index[3]);
						glColor3ub(cp[3], cp[2], cp[1]);	
						glVertex3fv(data+3*index[3]);
					}
				glEnd();
				index+= 4;
			}
			break;
			
		}
		dl= dl->next;
		dlob= dlob->next;
	}
	
	glShadeModel(GL_FLAT);
}

static void drawDispList(Object *ob, int dt)
{
	ListBase *lb=0;
	DispList *dl;
	Curve *cu;
	int solid;

	
	solid= (dt > OB_WIRE);

	switch(ob->type) {
	case OB_FONT:
	case OB_CURVE:
		cu= ob->data;
		
		lb= &cu->disp;
		if(lb->first==0) makeDispList(ob);
		
		if(solid) {
			dl= lb->first;
			if(dl==0) return;
			
			/* rule: dl->type INDEX3 is always first in list */
			if(dl->type!=DL_INDEX3) {
				if(ob==G.obedit) curve_to_filledpoly(ob->data, &editNurb, lb);
				else curve_to_filledpoly(ob->data, &cu->nurb, lb);
				
				dl= lb->first;
			}
			if(dl->nors==0) addnormalsDispList(ob, lb);
			
			index3_nors_incr= 0;
			
			if( displist_has_faces(lb)==0) {
				draw_index_wire= 0;
				drawDispListwire(lb);
				draw_index_wire= 1;
			}
			else {
				if(dt==OB_SHADED) {
					if(ob->disp.first==0) shadeDispList(ob);
					drawDispListshaded(lb, ob);
				}
				else {
					init_gl_materials(ob);
					glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
					drawDispListsolid(lb, ob);
				}
				if(ob==G.obedit) {
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
			drawDispListwire(lb);
			draw_index_wire= 1;
		}
		break;
	case OB_SURF:
	
		lb= &((Curve *)ob->data)->disp;
		if(lb->first==0) makeDispList(ob);
		
		if(solid) {
			dl= lb->first;
			if(dl==0) return;
			
			if(dl->nors==0) addnormalsDispList(ob, lb);
			
			if(dt==OB_SHADED) {
				if(ob->disp.first==0) shadeDispList(ob);
				drawDispListshaded(lb, ob);
			}
			else {
				init_gl_materials(ob);
				glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
			
				drawDispListsolid(lb, ob);
			}
		}
		else {
			drawDispListwire(lb);
		}
		break;
	case OB_MBALL:
		
		if( is_basis_mball(ob)) {
			lb= &ob->disp;
			if(lb->first==0) makeDispList(ob);
	
			if(solid) {
				
				if(dt==OB_SHADED) {
					dl= lb->first;
					if(dl && dl->col1==0) shadeDispList(ob);
					drawDispListshaded(lb, ob);
				}
				else {
					init_gl_materials(ob);
					glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
				
					drawDispListsolid(lb, ob);	
				}
			}
			else{
				/* MetaBalls use DL_INDEX4 type of DispList */
				drawDispListwire(lb);
			}
		}
		break;
	}
	
}

/* ******************************** */


static void draw_particle_system(Object *ob, PartEff *paf)
{
	Particle *pa;
	float ptime, ctime, vec[3], vec1[3];
	int a;
	
	pa= paf->keys;
	if(pa==0) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==0) return;
	}
	
	myloadmatrix(G.vd->viewmat);
	
	if(ob->ipoflag & OB_OFFS_PARTICLE) ptime= ob->sf;
	else ptime= 0.0;
	ctime= bsystem_time(ob, 0, (float)(G.scene->r.cfra), ptime);

	glPointSize(1.0);
	if(paf->stype!=PAF_VECT) glBegin(GL_POINTS);

	for(a=0; a<paf->totpart; a++, pa+=paf->totkey) {
		
		if(ctime > pa->time) {
			if(ctime < pa->time+pa->lifetime) {
			
				if(paf->stype==PAF_VECT) {
					where_is_particle(paf, pa, ctime, vec);
					where_is_particle(paf, pa, ctime+1.0, vec1);
		

					glBegin(GL_LINE_STRIP);
						glVertex3fv(vec);
						glVertex3fv(vec1);
					glEnd();
					
				}
				else {
					where_is_particle(paf, pa, ctime, vec);
					
					glVertex3fv(vec);
						
				}
			}
		}
	}
	if(paf->stype!=PAF_VECT) glEnd();
	
	mymultmatrix(ob->obmat);	// bring back local matrix for dtx
}

static void draw_static_particle_system(Object *ob, PartEff *paf)
{
	Particle *pa;
	float ctime, mtime, vec[3], vec1[3];
	int a;
	
	pa= paf->keys;
	if(pa==0) {
		build_particle_system(ob);
		pa= paf->keys;
		if(pa==0) return;
	}
	
	glPointSize(1.0);
	if(paf->stype!=PAF_VECT) glBegin(GL_POINTS);

	for(a=0; a<paf->totpart; a++, pa+=paf->totkey) {
		
		where_is_particle(paf, pa, pa->time, vec1);
		
		mtime= pa->time+pa->lifetime+paf->staticstep-1;
		
		for(ctime= pa->time; ctime<mtime; ctime+=paf->staticstep) {
			
			/* make sure hair grows until the end.. */ 
			if(ctime>pa->time+pa->lifetime) ctime= pa->time+pa->lifetime;
			
			if(paf->stype==PAF_VECT) {
				where_is_particle(paf, pa, ctime+1, vec);

				glBegin(GL_LINE_STRIP);
					glVertex3fv(vec);
					glVertex3fv(vec1);
				glEnd();
				
				VECCOPY(vec1, vec);
			}
			else {
				where_is_particle(paf, pa, ctime, vec);
				
				glVertex3fv(vec);
					
			}
		}
	}
	if(paf->stype!=PAF_VECT) glEnd();

}

unsigned int nurbcol[8]= {
	0, 0x9090, 0x409030, 0x603080, 0, 0x40fff0, 0x40c033, 0xA090F0 };

static void tekenhandlesN(Nurb *nu, short sel)
{
	BezTriple *bezt;
	float *fp;
	unsigned int *col;
	int a;

	if(nu->hide) return;
	if( (nu->type & 7)==1) {
		if(sel) col= nurbcol+4;
		else col= nurbcol;

		bezt= nu->bezt;
		a= nu->pntsu;
		while(a--) {
			if(bezt->hide==0) {
				if( (bezt->f2 & 1)==sel) {
					fp= bezt->vec[0];
					cpack(col[bezt->h1]);

					glBegin(GL_LINE_STRIP); 
					glVertex3fv(fp);
					glVertex3fv(fp+3); 
					glEnd();
					cpack(col[bezt->h2]);

					glBegin(GL_LINE_STRIP); 
					glVertex3fv(fp+3); 
					glVertex3fv(fp+6); 
					glEnd();
				}
				else if( (bezt->f1 & 1)==sel) {
					fp= bezt->vec[0];
					cpack(col[bezt->h1]);

					glBegin(GL_LINE_STRIP); 
					glVertex3fv(fp); 
					glVertex3fv(fp+3); 
					glEnd();
				}
				else if( (bezt->f3 & 1)==sel) {
					fp= bezt->vec[1];
					cpack(col[bezt->h2]);

					glBegin(GL_LINE_STRIP); 
					glVertex3fv(fp); 
					glVertex3fv(fp+3); 
					glEnd();
				}
			}
			bezt++;
		}
	}
}

static void tekenvertsN(Nurb *nu, short sel)
{
	BezTriple *bezt;
	BPoint *bp;
	float size;
	int a;

	if(nu->hide) return;

	if(sel) BIF_ThemeColor(TH_VERTEX_SELECT);
	else BIF_ThemeColor(TH_VERTEX);

	size= BIF_GetThemeValuef(TH_VERTEX_SIZE);
	glPointSize(size);
	
	bglBegin(GL_POINTS);
	
	if((nu->type & 7)==1) {

		bezt= nu->bezt;
		a= nu->pntsu;
		while(a--) {
			if(bezt->hide==0) {
				if((bezt->f1 & 1)==sel) bglVertex3fv(bezt->vec[0]);
				if((bezt->f2 & 1)==sel) bglVertex3fv(bezt->vec[1]);
				if((bezt->f3 & 1)==sel) bglVertex3fv(bezt->vec[2]);
			}
			bezt++;
		}
	}
	else {
		bp= nu->bp;
		a= nu->pntsu*nu->pntsv;
		while(a--) {
			if(bp->hide==0) {
				if((bp->f1 & 1)==sel) bglVertex3fv(bp->vec);
			}
			bp++;
		}
	}
	
	bglEnd();
	glPointSize(1.0);
}

static void draw_editnurb(Object *ob, Nurb *nurb, int sel)
{
	Nurb *nu;
	BPoint *bp, *bp1;
	int a, b, ofs;
	
	nu= nurb;
	while(nu) {
		if(nu->hide==0) {
			switch(nu->type & 7) {
			case CU_POLY:
				cpack(nurbcol[3]);
				bp= nu->bp;
				for(b=0; b<nu->pntsv; b++) {
					if(nu->flagu & 1) glBegin(GL_LINE_LOOP);

					else glBegin(GL_LINE_STRIP);

					for(a=0; a<nu->pntsu; a++, bp++) {
						glVertex3fv(bp->vec);
					}

					if(nu->flagu & 1) glEnd();
					else glEnd();
				}
				break;
			case CU_NURBS:

				bp= nu->bp;
				for(b=0; b<nu->pntsv; b++) {
					bp1= bp;
					bp++;
					for(a=nu->pntsu-1; a>0; a--, bp++) {
						if(bp->hide==0 && bp1->hide==0) {
							if(sel) {
								if( (bp->f1 & 1) && ( bp1->f1 & 1) ) {
									cpack(nurbcol[5]);
		
									glBegin(GL_LINE_STRIP);
									glVertex3fv(bp->vec); 
									glVertex3fv(bp1->vec);
									glEnd();
								}
							}
							else {
								if( (bp->f1 & 1) && ( bp1->f1 & 1) );
								else {
									cpack(nurbcol[1]);
		
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
									if( (bp->f1 & 1) && ( bp1->f1 & 1) ) {
										cpack(nurbcol[7]);
			
										glBegin(GL_LINE_STRIP);
										glVertex3fv(bp->vec); 
										glVertex3fv(bp1->vec);
										glEnd();
									}
								}
								else {
									if( (bp->f1 & 1) && ( bp1->f1 & 1) );
									else {
										cpack(nurbcol[3]);
			
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
		nu= nu->next;
	}
}

static void drawnurb(Object *ob, Nurb *nurb, int dt)
{
	extern float editbutsize;	/* buttons.c */
	Curve *cu;
	Nurb *nu;
	BevPoint *bevp;
	BevList *bl;
	float vec[3];
	int a, nr, skip;

	/* first non-selected handles */
	nu= nurb;
	while(nu) {
		if((nu->type & 7)==CU_BEZIER) {
			tekenhandlesN(nu, 0);
		}
		nu= nu->next;
	}
	
	/* then DispList */
	
	BIF_ThemeColor(TH_WIRE);
	cu= ob->data;
	drawDispList(ob, dt);

	draw_editnurb(ob, nurb, 0);
	draw_editnurb(ob, nurb, 1);

	if(cu->flag & CU_3D) {
	
		if(cu->bev.first==0) makeBevelList(ob);
		
		BIF_ThemeColor(TH_WIRE);
		bl= cu->bev.first;
		nu= nurb;
		while(nu && bl) {
			bevp= (BevPoint *)(bl+1);		
			nr= bl->nr;
			
			skip= nu->resolu/16;
			
			while(nr-- > 0) {
				
				glBegin(GL_LINE_STRIP);
				vec[0]= bevp->x-editbutsize*bevp->mat[0][0];
				vec[1]= bevp->y-editbutsize*bevp->mat[0][1];
				vec[2]= bevp->z-editbutsize*bevp->mat[0][2];
				glVertex3fv(vec);
				vec[0]= bevp->x+editbutsize*bevp->mat[0][0];
				vec[1]= bevp->y+editbutsize*bevp->mat[0][1];
				vec[2]= bevp->z+editbutsize*bevp->mat[0][2];
				glVertex3fv(vec);

				glEnd();
				
				bevp++;
				
				a= skip;
				while(a--) {
					bevp++;
					nr--;
				}
			}

			bl= bl->next;
			nu= nu->next;
		}
	}

	calc_Nurbverts(nurb);

	if(G.zbuf) glDisable(GL_DEPTH_TEST);
	
	nu= nurb;
	while(nu) {
		if((nu->type & 7)==1) tekenhandlesN(nu, 1);
		tekenvertsN(nu, 0);
		nu= nu->next;
	}

	nu= nurb;
	while(nu) {
		tekenvertsN(nu, 1);
		nu= nu->next;
	}
	
	if(G.zbuf) glEnable(GL_DEPTH_TEST); 
}

static void tekentextcurs(void)
{
	cpack(0);

	glBegin(GL_QUADS);
	glVertex2fv(G.textcurs[0]);
	glVertex2fv(G.textcurs[1]);
	glVertex2fv(G.textcurs[2]);
	glVertex2fv(G.textcurs[3]);
	glEnd();
}

static void drawspiral(float *cent, float rad, float tmat[][4], int start)
{
	float vec[3], vx[3], vy[3];
	int a, tot=32;
	char inverse=0;
	/* 32 values of sin function (still same result!) */
	static float si[32] = {0.00000000,
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
		0.00000000};
	/* 32 values of cos function (still same result!) */
	static float co[32] ={1.00000000,
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
                1.00000000};
		
	if (start < 0) {
		inverse = 1;
		start *= -1;
	}

	VECCOPY(vx, tmat[0]);
	VECCOPY(vy, tmat[1]);
	VecMulf(vx, rad);
	VecMulf(vy, rad);

	VECCOPY(vec, cent);

	if (inverse==0) {
		for(a=0; a<tot; a++) {
			if (a+start>31)
				start=-a + 1;
			glBegin(GL_LINES);							
			glVertex3fv(vec);
			vec[0]= cent[0] + *(si+a+start) * (vx[0] * (float)a/(float)tot) + *(co+a+start) * (vy[0] * (float)a/(float)tot);
			vec[1]= cent[1] + *(si+a+start) * (vx[1] * (float)a/(float)tot) + *(co+a+start) * (vy[1] * (float)a/(float)tot);
			vec[2]= cent[2] + *(si+a+start) * (vx[2] * (float)a/(float)tot) + *(co+a+start) * (vy[2] * (float)a/(float)tot);
			glVertex3fv(vec);
			glEnd();
		}
	}
	else {
		a=0;
		vec[0]= cent[0] + *(si+a+start) * (vx[0] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[0] * (float)(-a+31)/(float)tot);
		vec[1]= cent[1] + *(si+a+start) * (vx[1] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[1] * (float)(-a+31)/(float)tot);
		vec[2]= cent[2] + *(si+a+start) * (vx[2] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[2] * (float)(-a+31)/(float)tot);
		for(a=0; a<tot; a++) {
			if (a+start>31)
				start=-a + 1;
			glBegin(GL_LINES);							
			glVertex3fv(vec);
			vec[0]= cent[0] + *(si+a+start) * (vx[0] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[0] * (float)(-a+31)/(float)tot);
			vec[1]= cent[1] + *(si+a+start) * (vx[1] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[1] * (float)(-a+31)/(float)tot);
			vec[2]= cent[2] + *(si+a+start) * (vx[2] * (float)(-a+31)/(float)tot) + *(co+a+start) * (vy[2] * (float)(-a+31)/(float)tot);
			glVertex3fv(vec);
			glEnd();
		}
	}
}

void drawcircball(float *cent, float rad, float tmat[][4])
{
	float vec[3], vx[3], vy[3];
	int a, tot=32;

	/* 32 values of sin function (still same result!) */
	static float si[32] = {0.00000000,
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
		0.00000000};
	/* 32 values of cos function (still same result!) */
	static float co[32] ={1.00000000,
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
                1.00000000};
		
	VECCOPY(vx, tmat[0]);
	VECCOPY(vy, tmat[1]);
	VecMulf(vx, rad);
	VecMulf(vy, rad);
	
	glBegin(GL_LINE_LOOP);
	for(a=0; a<tot; a++) {
		vec[0]= cent[0] + *(si+a) * vx[0] + *(co+a) * vy[0];
		vec[1]= cent[1] + *(si+a) * vx[1] + *(co+a) * vy[1];
		vec[2]= cent[2] + *(si+a) * vx[2] + *(co+a) * vy[2];
		glVertex3fv(vec);
	}
	glEnd();
}

static void drawmball(Object *ob, int dt)
{
	MetaBall *mb;
	MetaElem *ml;
	float imat[4][4], tmat[4][4];
	int code= 1;
	
	mb= ob->data;

	if(ob==G.obedit) {
		BIF_ThemeColor(TH_WIRE);
		if((G.f & G_PICKSEL)==0 ) drawDispList(ob, dt);
		ml= editelems.first;
	}
	else {
		drawDispList(ob, dt);
		ml= mb->elems.first;
	}

	/* in case solid draw, reset wire colors */
	if(ob!=G.obedit && (ob->flag & SELECT)) {
		if(ob==OBACT) BIF_ThemeColor(TH_ACTIVE);
		else BIF_ThemeColor(TH_SELECT);
	}
	else BIF_ThemeColor(TH_WIRE);

	mygetmatrix(tmat);
	Mat4Invert(imat, tmat);
	Normalise(imat[0]);
	Normalise(imat[1]);
	
	while(ml) {
		
		if(ob==G.obedit) {
			if(ml->flag & SELECT) cpack(0xA0A0F0);
			else cpack(0x3030A0);
			
			if(G.f & G_PICKSEL) {
				ml->selcol= code;
				glLoadName(code++);
			}
		}
		drawcircball(&(ml->x), ml->rad, imat);
		
		ml= ml->next;
	}
}

static void draw_forcefield(Object *ob)
{
	float imat[4][4], tmat[4][4];
	float vec[3]= {0.0, 0.0, 0.0};
	
	if (ob->pd->forcefield == PFIELD_FORCE) {
		float ffall_val;

		mygetmatrix(tmat);
		Mat4Invert(imat, tmat);
		Normalise(imat[0]);
		Normalise(imat[1]);
		if (has_ipo_code(ob->ipo, OB_PD_FFALL)) 
			ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, G.scene->r.cfra);
		else 
			ffall_val = ob->pd->f_power;

		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.5);
		drawcircball(vec, 1.0, imat);
		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.9 - 0.4 / pow(1.5, (double)ffall_val));
		drawcircball(vec, 1.5, imat);
		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.9 - 0.4 / pow(2.0, (double)ffall_val));
		drawcircball(vec, 2.0, imat);
	}
	else if (ob->pd->forcefield == PFIELD_VORTEX) {
		float ffall_val, force_val;

		Mat4One(imat);
		if (has_ipo_code(ob->ipo, OB_PD_FFALL)) 
			ffall_val = IPO_GetFloatValue(ob->ipo, OB_PD_FFALL, G.scene->r.cfra);
		else 
			ffall_val = ob->pd->f_power;

		if (has_ipo_code(ob->ipo, OB_PD_FSTR))
			force_val = IPO_GetFloatValue(ob->ipo, OB_PD_FSTR, G.scene->r.cfra);
		else 
			force_val = ob->pd->f_strength;

		BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.7);
		if (force_val < 0) {
			drawspiral(vec, 1.0, imat, 1);
			drawspiral(vec, 1.0, imat, 16);
		}
		else {
			drawspiral(vec, 1.0, imat, -1);
			drawspiral(vec, 1.0, imat, -16);
		}
	}
	
}

static void draw_bb_box(BoundBox *bb)
{
	float *vec;

	vec= bb->vec[0];

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec); glVertex3fv(vec+3);glVertex3fv(vec+6); glVertex3fv(vec+9);
		glVertex3fv(vec); glVertex3fv(vec+12);glVertex3fv(vec+15); glVertex3fv(vec+18);
		glVertex3fv(vec+21); glVertex3fv(vec+12);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+3); glVertex3fv(vec+15);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+6); glVertex3fv(vec+18);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+9); glVertex3fv(vec+21);
	glEnd();
	
}

void get_local_bounds(Object *ob, float *centre, float *size)
{
	BoundBox *bb= NULL;
	/* uses boundbox, function used by Ketsji */
	
	if(ob->type==OB_MESH) {
		bb= ( (Mesh *)ob->data )->bb;
		if(bb==0) {
			tex_space_mesh(ob->data);
			bb= ( (Mesh *)ob->data )->bb;
		}
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		bb= ( (Curve *)ob->data )->bb;
	}
	else if(ob->type==OB_MBALL) {
		bb= ob->bb;
	}
	if(bb==NULL) {
		centre[0]= centre[1]= centre[2]= 0.0;
		VECCOPY(size, ob->size);
	}
	else {
		size[0]= 0.5*fabs(bb->vec[0][0] - bb->vec[4][0]);
		size[1]= 0.5*fabs(bb->vec[0][1] - bb->vec[2][1]);
		size[2]= 0.5*fabs(bb->vec[0][2] - bb->vec[1][2]);
		
		centre[0]= (bb->vec[0][0] + bb->vec[4][0])/2.0;
		centre[1]= (bb->vec[0][1] + bb->vec[2][1])/2.0;
		centre[2]= (bb->vec[0][2] + bb->vec[1][2])/2.0;
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

static void draw_bounding_volume(Object *ob)
{
	BoundBox *bb=0;
	
	if(ob->type==OB_MESH) {
		bb= ( (Mesh *)ob->data )->bb;
		if(bb==0) {
			tex_space_mesh(ob->data);
			bb= ( (Mesh *)ob->data )->bb;
		}
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		bb= ( (Curve *)ob->data )->bb;
		if(bb==0) {
			makeDispList(ob);
			bb= ( (Curve *)ob->data )->bb;
		}
	}
	else if(ob->type==OB_MBALL) {
		bb= ob->bb;
		if(bb==0) {
			makeDispList(ob);
			bb= ob->bb;
		}
	}
	else {
		drawcube();
		return;
	}
	
	if(bb==0) return;
	
	if(ob->boundtype==OB_BOUND_BOX) draw_bb_box(bb);
	else draw_bb_quadric(bb, ob->boundtype);
	
}

static void drawtexspace(Object *ob)
{
	Mesh *me;
	MetaBall *mb;
	Curve *cu;
	BoundBox bb;
	float *vec, *loc, *size;
	
	if(ob->type==OB_MESH) {
		me= ob->data;
		size= me->size;
		loc= me->loc;
	}
	else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) {
		cu= ob->data;
		size= cu->size;
		loc= cu->loc;
	}
	else if(ob->type==OB_MBALL) {
		mb= ob->data;
		size= mb->size;
		loc= mb->loc;
	}
	else return;
	
	bb.vec[0][0]=bb.vec[1][0]=bb.vec[2][0]=bb.vec[3][0]= loc[0]-size[0];
	bb.vec[4][0]=bb.vec[5][0]=bb.vec[6][0]=bb.vec[7][0]= loc[0]+size[0];
	
	bb.vec[0][1]=bb.vec[1][1]=bb.vec[4][1]=bb.vec[5][1]= loc[1]-size[1];
	bb.vec[2][1]=bb.vec[3][1]=bb.vec[6][1]=bb.vec[7][1]= loc[1]+size[1];

	bb.vec[0][2]=bb.vec[3][2]=bb.vec[4][2]=bb.vec[7][2]= loc[2]-size[2];
	bb.vec[1][2]=bb.vec[2][2]=bb.vec[5][2]=bb.vec[6][2]= loc[2]+size[2];
	
	setlinestyle(2);
		
	vec= bb.vec[0];

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec); glVertex3fv(vec+3);glVertex3fv(vec+6); glVertex3fv(vec+9);
		glVertex3fv(vec); glVertex3fv(vec+12);glVertex3fv(vec+15); glVertex3fv(vec+18);
		glVertex3fv(vec+21); glVertex3fv(vec+12);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+3); glVertex3fv(vec+15);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+6); glVertex3fv(vec+18);
	glEnd();

	glBegin(GL_LINE_STRIP);
		glVertex3fv(vec+9); glVertex3fv(vec+21);
	glEnd();
	
	setlinestyle(0);
}

/* draws wire outline */
static void drawSolidSelect(Object *ob) 
{
	glLineWidth(2.0);
	glDepthMask(0);
		
	if(ELEM3(ob->type, OB_FONT,OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		if (displist_has_faces(&cu->disp) && boundbox_clip(ob->obmat, cu->bb)) {
			drawDispListwire(&cu->disp);
		}
	} else if (ob->type==OB_MBALL) {
		drawDispListwire(&ob->disp);
	}

	glLineWidth(1.0);
	glDepthMask(1);
}

static void drawWireExtra(Object *ob) 
{
	if(ob!=G.obedit && (ob->flag & SELECT)) {
		if(ob==OBACT) BIF_ThemeColor(TH_ACTIVE);
		else BIF_ThemeColor(TH_SELECT);
	}
	else BIF_ThemeColor(TH_WIRE);

	bglPolygonOffset(1.0);
	glDepthMask(0);	// disable write in zbuffer, selected edge wires show better
	
	if (ELEM3(ob->type, OB_FONT, OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		if (boundbox_clip(ob->obmat, cu->bb)) {
			if (ob->type==OB_CURVE)
				draw_index_wire= 0;
			drawDispListwire(&cu->disp);
			if (ob->type==OB_CURVE)
				draw_index_wire= 1;
		}
	} else if (ob->type==OB_MBALL) {
		drawDispListwire(&ob->disp);
	}

	glDepthMask(1);
	bglPolygonOffset(0.0);
}

/* should be called in view space */
static void draw_hooks(Object *ob)
{
	ObHook *hook;
	float vec[3];
	
	for(hook= ob->hooks.first; hook; hook= hook->next) {
		
		VecMat4MulVecfl(vec, ob->obmat, hook->cent);
		if(hook->parent) {
			setlinestyle(3);
			glBegin(GL_LINES);
			glVertex3fv(hook->parent->obmat[3]);
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

void draw_object(Base *base)
{
	Object *ob;
	Curve *cu;
	ListBase elems;
	CfraElem *ce;
	float cfraont, axsize=1.0;
	unsigned int *rect, col=0;
	static int warning_recursive= 0;
	int sel, drawtype, colindex= 0, ipoflag;
	short dt, dtx, zbufoff= 0;
	int draw_solid_outline = 0;

	ob= base->object;

	/* draw keys? */
	if(base==(G.scene->basact) || (base->flag & (SELECT+BA_WASSEL))) {
		if(warning_recursive==0 && ob!=G.obedit) {
			if(ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
				float temp[7][3];

				warning_recursive= 1;
				
				elems.first= elems.last= 0;
				make_cfra_list(ob->ipo, &elems);
		
				cfraont= (G.scene->r.cfra);
				drawtype= G.vd->drawtype;
				if(drawtype>OB_WIRE) G.vd->drawtype= OB_WIRE;
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
							(G.scene->r.cfra)= ce->cfra/G.scene->r.framelen;
						
							base->flag= 0;
						
							where_is_object_time(ob, (G.scene->r.cfra));
							draw_object(base);
						}
						ce= ce->next;
					}
				}
				
				ce= elems.first;
				while(ce) {
					if(ce->sel) {
						(G.scene->r.cfra)= ce->cfra/G.scene->r.framelen;
						
						base->flag= SELECT;
						
						where_is_object_time(ob, (G.scene->r.cfra));
						draw_object(base);
					}
					ce= ce->next;
				}

				set_no_parent_ipo(0);
				disable_speed_curve(0);
				
				base->flag= sel;
				ob->ipoflag= ipoflag;
				
				/* restore icu->curval */
				(G.scene->r.cfra)= cfraont;
				
				memcpy(&ob->loc, temp, 7*3*sizeof(float));
				where_is_object(ob);
				G.vd->drawtype= drawtype;
				
				BLI_freelistN(&elems);

				warning_recursive= 0;
			}
		}
	}
	
	/* patch? children objects with a timeoffs change the parents. How to solve! */
	/* if( ((int)ob->ctime) != F_(G.scene->r.cfra)) where_is_object(ob); */

	mymultmatrix(ob->obmat);

	/* which wire color */
	if((G.f & G_PICKSEL) == 0) {
		project_short(ob->obmat[3], &base->sx);
		
		if((G.moving & G_TRANSFORM_OBJ) && (base->flag & (SELECT+BA_PARSEL))) BIF_ThemeColor(TH_TRANSFORM);
		else {
		
			BIF_ThemeColor(TH_WIRE);
			if((G.scene->basact)==base) {
				if(base->flag & (SELECT+BA_WASSEL)) BIF_ThemeColor(TH_ACTIVE);
			}
			else {
				if(base->flag & (SELECT+BA_WASSEL)) BIF_ThemeColor(TH_SELECT);
			}
			
			// no theme yet
			if(ob->id.lib) {
				if(base->flag & (SELECT+BA_WASSEL)) colindex = 4;
				else colindex = 3;
			}
			else if(warning_recursive==1) {
				if(base->flag & (SELECT+BA_WASSEL)) colindex = 7;
				else colindex = 6;
			}

		}	
		
		if(colindex) {
			col= colortab[colindex];
			cpack(col);
		}
	}
	
	/* maximum drawtype */
	dt= MIN2(G.vd->drawtype, ob->dt);
	if(G.zbuf==0 && dt>OB_WIRE) dt= OB_WIRE;
	dtx= 0;
	
	/* faceselect exception: also draw solid when dt==wire, except in editmode */
	if(ob==((G.scene->basact) ? (G.scene->basact->object) : 0) && (G.f & (G_FACESELECT+G_VERTEXPAINT+G_TEXTUREPAINT+G_WEIGHTPAINT))) {
		if(ob->type==OB_MESH) {
			
			if(ob==G.obedit);
			else {
				dt= OB_SHADED;
		
				glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
				glEnable(GL_DEPTH_TEST);
				zbufoff= 1;
			}
		}
		else {
			if(dt<OB_SOLID) {
				dt= OB_SOLID;
				glClearDepth(1.); glClear(GL_DEPTH_BUFFER_BIT);
				glEnable(GL_DEPTH_TEST);
				zbufoff= 1;
			}
		}
	}
	if(dt>=OB_WIRE ) {

		dtx= ob->dtx;
		if(G.obedit==ob) {
			// the only 2 extra drawtypes alowed in editmode
			dtx= dtx & (OB_DRAWWIRE|OB_TEXSPACE);
		}
		
		if(G.f & G_DRAW_EXT) {
			if(ob->type==OB_EMPTY || ob->type==OB_CAMERA || ob->type==OB_LAMP) dt= OB_WIRE;
		}
	}
	
		/* draw outline for selected solid objects, mesh does itself */
	if((G.vd->flag & V3D_SELECT_OUTLINE) && ob->type!=OB_MESH) {
		if(dt>OB_WIRE && dt<OB_TEXTURE && ob!=G.obedit) {
			if (!(ob->dtx&OB_DRAWWIRE) && (ob->flag&SELECT)) {
				drawSolidSelect(ob);
			}
		}
	}

	switch( ob->type) {
	case OB_MESH:
		if (!(base->flag&OB_RADIO)) {
			draw_mesh_object(ob, dt);
			dtx &= ~OB_DRAWWIRE; // mesh draws wire itself

			{
				PartEff *paf = give_parteff(ob);

				if(paf) {
					if(col) cpack(0xFFFFFF);	/* for visibility */
					if(paf->flag & PAF_STATIC) draw_static_particle_system(ob, paf);
					else if((G.f & G_PICKSEL) == 0) draw_particle_system(ob, paf);	// selection errors happen to easy
					if(col) cpack(col);
				}
			}
		}

		break;
	case OB_FONT:
		cu= ob->data;
		if(ob==G.obedit) {
			tekentextcurs();
			cpack(0xFFFF90);
			drawDispList(ob, OB_WIRE);
		}
		else if(dt==OB_BOUNDBOX) draw_bounding_volume(ob);
		else if(boundbox_clip(ob->obmat, cu->bb)) drawDispList(ob, dt);
			
		break;
	case OB_CURVE:
	case OB_SURF:
		cu= ob->data;
		
		if(ob==G.obedit) {
			drawnurb(ob, editNurb.first, dt);
		}
		else if(dt==OB_BOUNDBOX) draw_bounding_volume(ob);
		else if(boundbox_clip(ob->obmat, cu->bb)) drawDispList(ob, dt);
		
		break;
	case OB_MBALL:
		if(ob==G.obedit) drawmball(ob, dt);
		else if(dt==OB_BOUNDBOX) draw_bounding_volume(ob);
		else drawmball(ob, dt);
		break;
	case OB_EMPTY:
		drawaxes(1.0);
		break;
	case OB_LAMP:
		drawlamp(ob);
		break;
	case OB_CAMERA:
		drawcamera(ob);
		break;
	case OB_LATTICE:
		drawlattice(ob);
		break;
	case OB_ARMATURE:
		draw_armature (ob);
		break;
	default:
		drawaxes(1.0);
	}
	if(ob->pd && ob->pd->forcefield) draw_forcefield(ob);


	/* draw extra: after normal draw because of makeDispList */
	if(dtx) {
		if(G.f & G_SIMULATION);
		else if(dtx & OB_AXIS) {
			drawaxes(axsize);
		}
		if(dtx & OB_BOUNDBOX) draw_bounding_volume(ob);
		if(dtx & OB_TEXSPACE) drawtexspace(ob);
		if(dtx & OB_DRAWNAME) {
			// patch for several 3d cards (IBM mostly) that crash on glSelect with text drawing
			if((G.f & G_PICKSEL) == 0) {
				glRasterPos3f(0.0,  0.0,  0.0);
				
				BMF_DrawString(G.font, " ");
				BMF_DrawString(G.font, ob->id.name+2);
			}
		}
		if(dtx & OB_DRAWIMAGE) drawDispListwire(&ob->disp);
		if((dtx & OB_DRAWWIRE) && dt>=OB_SOLID) drawWireExtra(ob);
	}
	
	if(dt<OB_SHADED) {
		if((ob->gameflag & OB_ACTOR) && (ob->gameflag & OB_DYNAMIC)) {
			float tmat[4][4], imat[4][4], vec[3];
			
			vec[0]= vec[1]= vec[2]= 0.0;
			mygetmatrix(tmat);
			Mat4Invert(imat, tmat);
			
			setlinestyle(2);
			drawcircball(vec, ob->inertia, imat);
			setlinestyle(0);
		}
	}

	myloadmatrix(G.vd->viewmat);

	if(zbufoff) glDisable(GL_DEPTH_TEST);

	if(warning_recursive) return;
	if(base->flag & OB_FROMDUPLI) return;
	if(base->flag & OB_RADIO) return;
	if(G.f & G_SIMULATION) return;

	if((G.f & (G_PICKSEL))==0) {
		ListBase *list;

		/* draw hook center and offset line */
		if(ob->hooks.first && ob!=G.obedit) draw_hooks(ob);

		/* help lines and so */
		if(ob!=G.obedit && ob->parent && (ob->parent->lay & G.vd->lay)) {
			setlinestyle(3);
			glBegin(GL_LINES);
			glVertex3fv(ob->obmat[3]);
			glVertex3fv(ob->orig);
			glEnd();
			setlinestyle(0);
		}
		
		/* Drawing the constraint lines */
		list = &ob->constraints;
		if (list){
			/*
			extern void make_axis_color(char *col, char *col2, char axis);	// drawview.c
			 */
			bConstraint *curcon;
			float size[3], tmat[4][4];
			char col[4], col2[4];
			
			BIF_GetThemeColor3ubv(TH_GRID, col);
			make_axis_color(col, col2, 'z');
			glColor3ubv(col2);
			
			for (curcon = list->first; curcon; curcon=curcon->next){
				if ((curcon->flag & CONSTRAINT_EXPAND)&&(curcon->type!=CONSTRAINT_TYPE_NULL)&&(constraint_has_target(curcon))){
					get_constraint_target_matrix(curcon, TARGET_OBJECT, NULL, tmat, size, bsystem_time(ob, 0, (float)(G.scene->r.cfra), ob->sf));
					setlinestyle(3);
					glBegin(GL_LINES);
					glVertex3fv(tmat[3]);
					glVertex3fv(ob->obmat[3]);
					glEnd();
					setlinestyle(0);
				}
			}
		}

		/* object centers */
		if(G.zbuf) glDisable(GL_DEPTH_TEST);
		if(ob->type == OB_LAMP) {
			if(ob->id.lib) {
				if(base->flag & SELECT) rect= rectllib_sel;
				else rect= rectllib_desel;
			}
			else if(ob->id.us>1) {
				if(base->flag & SELECT) rect= rectlus_sel;
				else rect= rectlus_desel;
			}
			else {
				if(base->flag & SELECT) rect= rectl_sel;
				else rect= rectl_desel;
			}
			draw_icon_centered(ob->obmat[3], rect, 9);
		}
		else {
			if(ob->id.lib || ob->id.us>1) {
				if(base->flag & SELECT) rect= rectu_sel;
				else rect= rectu_desel;
			}
			else {
				if(base->flag & SELECT) rect= rect_sel;
				/* The center of the active object (which need not 
				 * be selected) gets drawn as if it were selected
				 */
				else if(base==(G.scene->basact)) rect= rect_sel;
				else rect= rect_desel;
			}
			draw_icon_centered(ob->obmat[3], rect, 4);
		}
		if(G.zbuf) glEnable(GL_DEPTH_TEST);
		
	}
	else if((G.f & (G_VERTEXPAINT|G_FACESELECT|G_TEXTUREPAINT|G_WEIGHTPAINT))==0) {
	
		glBegin(GL_POINTS);
			glVertex3fv(ob->obmat[3]);
		glEnd();
		
	}
}

void draw_object_ext(Base *base)
{
	
	if(G.vd==NULL || base==NULL) return;
	
	if(G.vd->drawtype > OB_WIRE) {
		G.zbuf= 1;
		glEnable(GL_DEPTH_TEST);
	}
	
	G.f |= G_DRAW_EXT;

	glDrawBuffer(GL_FRONT);
	persp(PERSP_VIEW);

	draw_object(base);

	G.f &= ~G_DRAW_EXT;

	glFlush();		/* reveil frontbuffer drawing */
	glDrawBuffer(GL_BACK);
	
	if(G.zbuf) {
		G.zbuf= 0;
		glDisable(GL_DEPTH_TEST);
	}
	curarea->win_swap= WIN_FRONT_OK;
}

/* ***************** BACKBUF SEL (BBS) ********* */

static int bbs_mesh_verts(Object *ob, int offset)
{
	EditVert *eve;
	int a= offset, optimal= subsurf_optimal(ob);
	
	glPointSize( BIF_GetThemeValuef(TH_VERTEX_SIZE) );
	
	bglBegin(GL_POINTS);
	for(eve= G.editMesh->verts.first; eve; eve= eve->next, a++) {
		if(eve->h==0) {
			cpack( index_to_framebuffer(a) );
			if(optimal && eve->ssco) bglVertex3fv(eve->ssco);
			else bglVertex3fv(eve->co);
		}
	}
	bglEnd();
	
	glPointSize(1.0);
	return a;
}		

/* two options, edgecolors or black */
static int bbs_mesh_wire(Object *ob, int offset)
{
	EditEdge *eed;
	Mesh *me= ob->data;
	DispList *dl= find_displist(&me->disp, DL_MESH);
	DispListMesh *dlm= NULL;
	int index, b, retval, optimal=0;

	if(dl) dlm= dl->mesh;
	optimal= subsurf_optimal(ob);
	
	if(dlm && optimal) {
		MEdge *medge= dlm->medge;
		MVert *mvert= dlm->mvert;
		
		// tuck original indices in vn
		for(b=0, eed= G.editMesh->edges.first; eed; eed= eed->next, b++) eed->vn= (EditVert *)(b+offset);
		retval= b+offset;
		glBegin(GL_LINES);
		for (b=0; b<dlm->totedge; b++, medge++) {
			if(medge->flag & ME_EDGEDRAW) {
				eed= dlm->editedge[b];
				if(eed && eed->h==0) {
					
					index= (int)eed->vn;
					cpack(index_to_framebuffer(index));
					
					glVertex3fv(mvert[medge->v1].co); 
					glVertex3fv(mvert[medge->v2].co);
				}
			}
		}
		glEnd();
	}
	else {
		index= offset;
		cpack(0);
		glBegin(GL_LINES);
		for(eed= G.editMesh->edges.first; eed; eed= eed->next, index++) {
			if(eed->h==0) {

				cpack(index_to_framebuffer(index));

				glVertex3fv(eed->v1->co);
				glVertex3fv(eed->v2->co);
			}
		}
		glEnd();
		retval= index;
	}
	return retval;
}		
		
/* two options, facecolors or black */
static int bbs_mesh_solid(Object *ob, int facecol)
{
	int glmode, a;
	
	cpack(0);

	if(ob==G.obedit) {
		Mesh *me= ob->data;
		EditFace *efa;
		DispList *dl= find_displist(&me->disp, DL_MESH);
		DispListMesh *dlm= NULL;
		int b;
		
		if(dl) dlm= dl->mesh;
		a= 0; 

		if(dlm && dlm->editface) {
			EditFace *prevefa;
			MFace *mface;
			efa= NULL;
			
			// tuck original indices in efa->prev
			for(b=1, efa= G.editMesh->faces.first; efa; efa= efa->next, b++) efa->prev= (EditFace *)(b);
			a= b+1;	// correct return value, next loop excludes hidden faces

			for(b=0, mface= dlm->mface; b<dlm->totface; b++, mface++) {
				if(mface->v3) {
					if(facecol) {
						efa= dlm->editface[b];
						cpack(index_to_framebuffer((int)efa->prev));
					}
					
					glBegin(mface->v4?GL_QUADS:GL_TRIANGLES);
					glVertex3fv(dlm->mvert[mface->v1].co);
					glVertex3fv(dlm->mvert[mface->v2].co);
					glVertex3fv(dlm->mvert[mface->v3].co);
					if (mface->v4) glVertex3fv(dlm->mvert[mface->v4].co);
					glEnd();
				}
			}
			
			if(facecol && (G.scene->selectmode & SCE_SELECT_FACE)) {		
				glPointSize(BIF_GetThemeValuef(TH_FACEDOT_SIZE));
				
				bglBegin(GL_POINTS);
				for(efa= G.editMesh->faces.first; efa; efa= efa->next) {
					if(efa->h==0) {
						if(efa->fgonf==EM_FGON);
						else {
							cpack(index_to_framebuffer((int)efa->prev));
							bglVertex3fv(efa->cent);
						}
					}
				}
				bglEnd();
			}
			
			for (prevefa= NULL, efa= G.editMesh->faces.first; efa; prevefa= efa, efa= efa->next)
				efa->prev= prevefa;

		}
		else {
			a= 1;
			glBegin(GL_QUADS);
			glmode= GL_QUADS;
			for(efa= G.editMesh->faces.first; efa; efa= efa->next, a++) {
				if(efa->h==0) {
					if(efa->v4) {if(glmode==GL_TRIANGLES) {glmode= GL_QUADS; glEnd(); glBegin(GL_QUADS);}}
					else {if(glmode==GL_QUADS) {glmode= GL_TRIANGLES; glEnd(); glBegin(GL_TRIANGLES);}}

					if(facecol) {
						int i= index_to_framebuffer(a);
						cpack(i);
					}
					glVertex3fv(efa->v1->co);
					glVertex3fv(efa->v2->co);
					glVertex3fv(efa->v3->co);
					if(efa->v4) glVertex3fv(efa->v4->co);
				}
			}
			glEnd();
		}
		if(facecol) return a;
	}
	else {
		Mesh *me= ob->data;
		MVert *mvert;
		MFace *mface;
		TFace *tface;
		DispList *dl;
		float *extverts=NULL;
		int a, totface, hastface, i;
		
		mvert= me->mvert;
		mface= me->mface;
		tface= me->tface;
		hastface = (me->tface != NULL);
		totface= me->totface;

		dl= find_displist(&ob->disp, DL_VERTS);
		if(dl) extverts= dl->verts;
		
		glBegin(GL_QUADS);
		glmode= GL_QUADS;
		
		for(a=0; a<totface; a++, mface++, tface++) {
			if(mface->v3) {
				if(facecol) {
					if(hastface && tface->flag & TF_HIDE) continue;
					i= index_to_framebuffer(a+1);
					cpack(i);
				}

				if(mface->v4) {if(glmode==GL_TRIANGLES) {glmode= GL_QUADS; glEnd(); glBegin(GL_QUADS);}}
				else {if(glmode==GL_QUADS) {glmode= GL_TRIANGLES; glEnd(); glBegin(GL_TRIANGLES);}}
				
				if(extverts) {
					glVertex3fv( extverts+3*mface->v1 );
					glVertex3fv( extverts+3*mface->v2 );
					glVertex3fv( extverts+3*mface->v3 );
					if(mface->v4) glVertex3fv( extverts+3*mface->v4 );
				}
				else {
					glVertex3fv( (mvert+mface->v1)->co );
					glVertex3fv( (mvert+mface->v2)->co );
					glVertex3fv( (mvert+mface->v3)->co );
					if(mface->v4) glVertex3fv( (mvert+mface->v4)->co );
				}
			}
		}
		glEnd();
	}
	return 1;
}

void draw_object_backbufsel(Object *ob)
{
	extern int em_solidoffs, em_wireoffs, em_vertoffs;	// let linker solve it... from editmesh_mods.c 
	
	mymultmatrix(ob->obmat);

	glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	switch( ob->type) {
	case OB_MESH:
		if(G.obedit) {

			em_solidoffs= bbs_mesh_solid(ob, G.scene->selectmode & SCE_SELECT_FACE);
			
			bglPolygonOffset(1.0);
			
			// we draw edges always, for loop (select) tools
			em_wireoffs= bbs_mesh_wire(ob, em_solidoffs);
			
			if(G.scene->selectmode & SCE_SELECT_VERTEX) 
				em_vertoffs= bbs_mesh_verts(ob, em_wireoffs);
			else em_vertoffs= em_wireoffs;
			
			bglPolygonOffset(0.0);
		}
		else bbs_mesh_solid(ob, 1);	// 1= facecol, faceselect
		
		break;
	case OB_CURVE:
	case OB_SURF:
		break;
	}

	myloadmatrix(G.vd->viewmat);

}
