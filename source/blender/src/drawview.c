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

//#define NAN_LINEAR_PHYSICS

#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#include <sys/times.h>
#else
#include <io.h>
#endif   

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BMF_Api.h"

#include "IMB_imbuf_types.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_space_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_anim.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_ika.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BIF_butspace.h"
#include "BIF_drawimage.h"
#include "BIF_editgroup.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_poseobject.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"

#include "BDR_drawmesh.h"
#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_vpaint.h"

#include "BSE_drawview.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_seqaudio.h"
#include "BSE_trans_types.h"
#include "BSE_time.h"
#include "BSE_view.h"

#include "RE_renderconverter.h"

#include "BPY_extern.h"

#include "blendef.h"
#include "mydevice.h"
#include "butspace.h"  // event codes

#include "BIF_transform.h"

/* Modules used */
#include "render.h"		// for ogl render
#include "radio.h"

/* locals */
void drawname(Object *ob);
void star_stuff_init_func(void);
void star_stuff_vertex_func(float* i);
void star_stuff_term_func(void);

void star_stuff_init_func(void)
{
	cpack(-1);
	glPointSize(1.0);
	glBegin(GL_POINTS);
}
void star_stuff_vertex_func(float* i)
{
	glVertex3fv(i);
}
void star_stuff_term_func(void)
{
	glEnd();
}

void setalpha_bgpic(BGpic *bgpic)
{
	int x, y, alph;
	char *rect;
	
	alph= (int)(255.0*(1.0-bgpic->blend));
	
	rect= (char *)bgpic->rect;
	for(y=0; y< bgpic->yim; y++) {
		for(x= bgpic->xim; x>0; x--, rect+=4) {
			rect[3]= alph;
		}
	}
}


void default_gl_light(void)
{
	int a;
	
	/* initialize */
	if(U.light[0].flag==0 && U.light[1].flag==0 && U.light[2].flag==0) {
		U.light[0].flag= 1;
		U.light[0].vec[0]= -0.3; U.light[0].vec[1]= 0.3; U.light[0].vec[2]= 0.9;
		U.light[0].col[0]= 0.8; U.light[0].col[1]= 0.8; U.light[0].col[2]= 0.8;
		U.light[0].spec[0]= 0.5; U.light[0].spec[1]= 0.5; U.light[0].spec[2]= 0.5;
		U.light[0].spec[3]= 1.0;
		
		U.light[1].flag= 0;
		U.light[1].vec[0]= 0.5; U.light[1].vec[1]= 0.5; U.light[1].vec[2]= 0.1;
		U.light[1].col[0]= 0.4; U.light[1].col[1]= 0.4; U.light[1].col[2]= 0.8;
		U.light[1].spec[0]= 0.3; U.light[1].spec[1]= 0.3; U.light[1].spec[2]= 0.5;
		U.light[1].spec[3]= 1.0;
	
		U.light[2].flag= 0;
		U.light[2].vec[0]= 0.3; U.light[2].vec[1]= -0.3; U.light[2].vec[2]= -0.2;
		U.light[2].col[0]= 0.8; U.light[2].col[1]= 0.5; U.light[2].col[2]= 0.4;
		U.light[2].spec[0]= 0.5; U.light[2].spec[1]= 0.4; U.light[2].spec[2]= 0.3;
		U.light[2].spec[3]= 1.0;
	}
	

	glLightfv(GL_LIGHT0, GL_POSITION, U.light[0].vec); 
	glLightfv(GL_LIGHT0, GL_DIFFUSE, U.light[0].col); 
	glLightfv(GL_LIGHT0, GL_SPECULAR, U.light[0].spec); 

	glLightfv(GL_LIGHT1, GL_POSITION, U.light[1].vec); 
	glLightfv(GL_LIGHT1, GL_DIFFUSE, U.light[1].col); 
	glLightfv(GL_LIGHT1, GL_SPECULAR, U.light[1].spec); 

	glLightfv(GL_LIGHT2, GL_POSITION, U.light[2].vec); 
	glLightfv(GL_LIGHT2, GL_DIFFUSE, U.light[2].col); 
	glLightfv(GL_LIGHT2, GL_SPECULAR, U.light[2].spec); 

	for(a=0; a<8; a++) {
		if(a<3) {
			if(U.light[a].flag) glEnable(GL_LIGHT0+a);
			else glDisable(GL_LIGHT0+a);
			
			// clear stuff from other opengl lamp usage
			glLightf(GL_LIGHT0+a, GL_SPOT_CUTOFF, 180.0);
			glLightf(GL_LIGHT0+a, GL_CONSTANT_ATTENUATION, 1.0);
			glLightf(GL_LIGHT0+a, GL_LINEAR_ATTENUATION, 0.0);
		}
		else glDisable(GL_LIGHT0+a);
	}
	
	glDisable(GL_LIGHTING);

	glDisable(GL_COLOR_MATERIAL);
}

/* also called when render 'ogl' */
void init_gl_stuff(void)	
{
	float mat_specular[] = { 0.5, 0.5, 0.5, 1.0 };
	float mat_shininess[] = { 35.0 };
/*  	float one= 1.0; */
	int a, x, y;
	GLubyte pat[32*32];
	const GLubyte *patc= pat;
		
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);

	default_gl_light();
	
	/* no local viewer, looks ugly in ortho mode */
	/* glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, &one); */
	
	glDepthFunc(GL_LEQUAL);
	/* scaling matrices */
	glEnable(GL_NORMALIZE);

	glShadeModel(GL_FLAT);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FOG);
	glDisable(GL_LIGHTING);
	glDisable(GL_LOGIC_OP);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);

	glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
	glPixelTransferi(GL_RED_SCALE, 1);
	glPixelTransferi(GL_RED_BIAS, 0);
	glPixelTransferi(GL_GREEN_SCALE, 1);
	glPixelTransferi(GL_GREEN_BIAS, 0);
	glPixelTransferi(GL_BLUE_SCALE, 1);
	glPixelTransferi(GL_BLUE_BIAS, 0);
	glPixelTransferi(GL_ALPHA_SCALE, 1);
	glPixelTransferi(GL_ALPHA_BIAS, 0);
	
	glPixelTransferi(GL_DEPTH_BIAS, 0);
	glPixelTransferi(GL_DEPTH_SCALE, 1);
	glDepthRange(0.0, 1.0);
	
	a= 0;
	for(x=0; x<32; x++) {
		for(y=0; y<4; y++) {
			if( (x) & 1) pat[a++]= 0x88;
			else pat[a++]= 0x22;
		}
	}
	
	glPolygonStipple(patc);


	init_realtime_GL();	
}

void circf(float x, float y, float rad)
{
	GLUquadricObj *qobj = gluNewQuadric(); 
	
	gluQuadricDrawStyle(qobj, GLU_FILL); 
	
	glPushMatrix(); 
	
	glTranslatef(x,  y, 0.); 
	
	gluDisk( qobj, 0.0,  rad, 32, 1); 
	
	glPopMatrix(); 
	
	gluDeleteQuadric(qobj);
}

void circ(float x, float y, float rad)
{
	GLUquadricObj *qobj = gluNewQuadric(); 
	
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
	
	glPushMatrix(); 
	
	glTranslatef(x,  y, 0.); 
	
	gluDisk( qobj, 0.0,  rad, 32, 1); 
	
	glPopMatrix(); 
	
	gluDeleteQuadric(qobj);
}

/* **********  ********** */

static void draw_bgpic(void)
{
	BGpic *bgpic;
	Image *ima;
	float vec[4], fac, asp, zoomx, zoomy;
	float x1, y1, x2, y2, cx, cy;
	
	bgpic= G.vd->bgpic;
	if(bgpic==0) return;
	
	if(bgpic->tex) {
		init_render_texture(bgpic->tex);
		free_unused_animimages();
		ima= bgpic->tex->ima;
		end_render_texture(bgpic->tex);
	}
	else {
		ima= bgpic->ima;
	}
	
	if(ima==0) return;
	if(ima->ok==0) return;
	
	/* test for image */
	if(ima->ibuf==0) {
	
		if(bgpic->rect) MEM_freeN(bgpic->rect);
		bgpic->rect= 0;
		
		if(bgpic->tex) {
			ima_ibuf_is_nul(bgpic->tex, bgpic->tex->ima);
		}
		else {
			waitcursor(1);
			load_image(ima, IB_rect, G.sce, G.scene->r.cfra);
			waitcursor(0);
		}
		if(ima->ibuf==0) {
			ima->ok= 0;
			return;
		}
	}

	if(bgpic->rect==0) {
		
		bgpic->rect= MEM_dupallocN(ima->ibuf->rect);
		bgpic->xim= ima->ibuf->x;
		bgpic->yim= ima->ibuf->y;
		setalpha_bgpic(bgpic);
	}

	if(G.vd->persp==2) {
		rcti vb;

		calc_viewborder(G.vd, &vb);

		x1= vb.xmin;
		y1= vb.ymin;
		x2= vb.xmax;
		y2= vb.ymax;
	}
	else {
		/* calc window coord */
		initgrabz(0.0, 0.0, 0.0);
		window_to_3d(vec, 1, 0);
		fac= MAX3( fabs(vec[0]), fabs(vec[1]), fabs(vec[1]) );
		fac= 1.0/fac;
	
		asp= ( (float)ima->ibuf->y)/(float)ima->ibuf->x;

			/* This code below was lifted from project_short_noclip
			 * because that code fails if the point is to close to
			 * the near clipping plane but we don't really care about
			 * that here.
			 */
		vec[0]= vec[1]= vec[2]= 0.0; vec[3]= 1.0;
		Mat4MulVec4fl(G.vd->persmat, vec);

		cx= (curarea->winx/2.0)*(1 + vec[0]/vec[3]);	
		cy= (curarea->winy/2.0)*(1 + vec[1]/vec[3]);
	
		x1=  cx+ fac*(bgpic->xof-bgpic->size);
		y1=  cy+ asp*fac*(bgpic->yof-bgpic->size);
		x2=  cx+ fac*(bgpic->xof+bgpic->size);
		y2=  cy+ asp*fac*(bgpic->yof+bgpic->size);
	}
	
	/* complete clip? */
	
	if(x2 < 0 ) return;
	if(y2 < 0 ) return;
	if(x1 > curarea->winx ) return;
	if(y1 > curarea->winy ) return;
	
	zoomx= (x2-x1)/ima->ibuf->x;
	zoomy= (y2-y1)/ima->ibuf->y;

	glEnable(GL_BLEND);
	if(G.zbuf) glDisable(GL_DEPTH_TEST);

	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); 
	 
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	
	glaDefine2DArea(&curarea->winrct);
	glPixelZoom(zoomx, zoomy);
	glaDrawPixelsSafe(x1, y1, ima->ibuf->x, ima->ibuf->y, bgpic->rect);
	glPixelZoom(1.0, 1.0);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	
	glBlendFunc(GL_ONE,  GL_ZERO); 
	glDisable(GL_BLEND);
	if(G.zbuf) glEnable(GL_DEPTH_TEST);
	
	areawinset(curarea->win);	// restore viewport / scissor
}

void timestr(double time, char *str)
{
	/* format 00:00:00.00 (hr:min:sec) string has to be 12 long */
	int  hr= (int)      time/(60*60);
	int min= (int) fmod(time/60, 60.0);
	int sec= (int) fmod(time, 60.0);
	int hun= (int) fmod(time*100.0, 100.0);

	if (hr) {
		sprintf(str, "%.2d:%.2d:%.2d.%.2d",hr,min,sec,hun);
	} else {
		sprintf(str, "%.2d:%.2d.%.2d",min,sec,hun);
	}

	str[11]=0;
}

static void drawgrid_draw(float wx, float wy, float x, float y, float dx)
{
	float fx, fy;
	
	x+= (wx); 
	y+= (wy);
	fx= x/dx;
	fx= x-dx*floor(fx);
	
	while(fx< curarea->winx) {
		fdrawline(fx,  0.0,  fx,  (float)curarea->winy); 
		fx+= dx; 
	}

	fy= y/dx;
	fy= y-dx*floor(fy);
	

	while(fy< curarea->winy) {
		fdrawline(0.0,  fy,  (float)curarea->winx,  fy); 
		fy+= dx;
	}

}

// not intern, called in editobject for constraint axis too
void make_axis_color(char *col, char *col2, char axis)
{
	if(axis=='x') {
		col2[0]= col[0]>219?255:col[0]+36;
		col2[1]= col[1]<26?0:col[1]-26;
		col2[2]= col[2]<26?0:col[2]-26;
	}
	else if(axis=='y') {
		col2[0]= col[0]<46?0:col[0]-36;
		col2[1]= col[1]>189?255:col[1]+66;
		col2[2]= col[2]<46?0:col[2]-36; 
	}
	else {
		col2[0]= col[0]<26?0:col[0]-26; 
		col2[1]= col[1]<26?0:col[1]-26; 
		col2[2]= col[2]>209?255:col[2]+46;
	}
	
}

static void drawgrid(void)
{
	/* extern short bgpicmode; */
	float wx, wy, x, y, fw, fx, fy, dx;
	float vec4[4];
	char col[3], col2[3];
	
	vec4[0]=vec4[1]=vec4[2]=0.0; 
	vec4[3]= 1.0;
	Mat4MulVec4fl(G.vd->persmat, vec4);
	fx= vec4[0]; 
	fy= vec4[1]; 
	fw= vec4[3];

	wx= (curarea->winx/2.0);	/* because of rounding errors, grid at wrong location */
	wy= (curarea->winy/2.0);

	x= (wx)*fx/fw;
	y= (wy)*fy/fw;

	vec4[0]=vec4[1]=G.vd->grid; 
	vec4[2]= 0.0;
	vec4[3]= 1.0;
	Mat4MulVec4fl(G.vd->persmat, vec4);
	fx= vec4[0]; 
	fy= vec4[1]; 
	fw= vec4[3];

	dx= fabs(x-(wx)*fx/fw);
	if(dx==0) dx= fabs(y-(wy)*fy/fw);
	
	glDepthMask(0);		// disable write in zbuffer

	/* check zoom out */
	BIF_ThemeColor(TH_GRID);
	persp(PERSP_WIN);
	
	if(dx<6.0) {
		G.vd->gridview*= 10.0;
		dx*= 10.0;
		
		if(dx<6.0) {	
			G.vd->gridview*= 10.0;
			dx*= 10.0;
			
			if(dx<6.0) {
				G.vd->gridview*= 10.0;
				dx*=10;
				if(dx<6.0);
				else {
					BIF_ThemeColor(TH_GRID);
					drawgrid_draw(wx, wy, x, y, dx);
				}
			}
			else {	// start blending out
				BIF_ThemeColorBlend(TH_BACK, TH_GRID, dx/60.0);
				drawgrid_draw(wx, wy, x, y, dx);
			
				BIF_ThemeColor(TH_GRID);
				drawgrid_draw(wx, wy, x, y, 10*dx);
			}
		}
		else {	// start blending out (6 < dx < 60)
			BIF_ThemeColorBlend(TH_BACK, TH_GRID, dx/60.0);
			drawgrid_draw(wx, wy, x, y, dx);
			
			BIF_ThemeColor(TH_GRID);
			drawgrid_draw(wx, wy, x, y, 10*dx);
		}
	}
	else {
		if(dx>60.0) {		// start blending in
			G.vd->gridview/= 10.0;
			dx/= 10.0;			
			if(dx>60.0) {		// start blending in
				G.vd->gridview/= 10.0;
				dx/= 10.0;
				if(dx>60.0) {
					BIF_ThemeColor(TH_GRID);
					drawgrid_draw(wx, wy, x, y, dx);
				}
				else {
					BIF_ThemeColorBlend(TH_BACK, TH_GRID, dx/60.0);
					drawgrid_draw(wx, wy, x, y, dx);
					BIF_ThemeColor(TH_GRID);
					drawgrid_draw(wx, wy, x, y, dx*10);
				}
			}
			else {
				BIF_ThemeColorBlend(TH_BACK, TH_GRID, dx/60.0);
				drawgrid_draw(wx, wy, x, y, dx);
				BIF_ThemeColor(TH_GRID);				
				drawgrid_draw(wx, wy, x, y, dx*10);
			}
		}
		else {
			BIF_ThemeColorBlend(TH_BACK, TH_GRID, dx/60.0);
			drawgrid_draw(wx, wy, x, y, dx);
			BIF_ThemeColor(TH_GRID);
			drawgrid_draw(wx, wy, x, y, dx*10);
		}
	}

	x+= (wx); 
	y+= (wy);
	BIF_GetThemeColor3ubv(TH_GRID, col);

	setlinestyle(0);
	
	/* center cross */
	if(G.vd->view==3) make_axis_color(col, col2, 'y');
	else make_axis_color(col, col2, 'x');
	glColor3ubv(col2);
	
	fdrawline(0.0,  y,  (float)curarea->winx,  y); 
	
	if(G.vd->view==7) make_axis_color(col, col2, 'y');
	else make_axis_color(col, col2, 'z');
	glColor3ubv(col2);

	fdrawline(x, 0.0, x, (float)curarea->winy); 

	glDepthMask(1);		// enable write in zbuffer
	persp(PERSP_VIEW);
}


static void drawfloor(void)
{
	View3D *vd;
	float vert[3], grid;
	int a, gridlines;
	char col[3], col2[3];
	short draw_line = 0;
		
	vd= curarea->spacedata.first;

	vert[2]= 0.0;

	if(vd->gridlines<3) return;
	
	if(G.zbuf && G.obedit) glDepthMask(0);	// for zbuffer-select
	
	gridlines= vd->gridlines/2;
	grid= gridlines*vd->grid;
	
	BIF_GetThemeColor3ubv(TH_GRID, col);
	
	/* draw the Y axis and/or grid lines */
	for(a= -gridlines;a<=gridlines;a++) {
		if(a==0) {
			/* check for the 'show Y axis' preference */
			if (vd->gridflag & V3D_SHOW_Y) { 
				make_axis_color(col, col2, 'y');
				glColor3ubv(col2);
				
				draw_line = 1;
			} else if (vd->gridflag & V3D_SHOW_FLOOR) {
				BIF_ThemeColorShade(TH_GRID, -10);
			} else {
				draw_line = 0;
			}
		} else {
			/* check for the 'show grid floor' preference */
			if (vd->gridflag & V3D_SHOW_FLOOR) {
				if( (a % 10)==0) {
					BIF_ThemeColorShade(TH_GRID, -10);
				}
				else BIF_ThemeColorShade(TH_GRID, 10);
				
				draw_line = 1;
			} else {
				draw_line = 0;
			}
		}
		
		if (draw_line) {
			glBegin(GL_LINE_STRIP);
	        vert[0]= a*vd->grid;
	        vert[1]= grid;
	        glVertex3fv(vert);
	        vert[1]= -grid;
	        glVertex3fv(vert);
			glEnd();
		}
	}
	
	/* draw the X axis and/or grid lines */
	for(a= -gridlines;a<=gridlines;a++) {
		if(a==0) {
			/* check for the 'show X axis' preference */
			if (vd->gridflag & V3D_SHOW_X) { 
				make_axis_color(col, col2, 'x');
				glColor3ubv(col2);
				
				draw_line = 1;
			} else if (vd->gridflag & V3D_SHOW_FLOOR) {
				BIF_ThemeColorShade(TH_GRID, -10);
			} else {
				draw_line = 0;
			}
		} else {
			/* check for the 'show grid floor' preference */
			if (vd->gridflag & V3D_SHOW_FLOOR) {
				if( (a % 10)==0) {
					BIF_ThemeColorShade(TH_GRID, -10);
				}
				else BIF_ThemeColorShade(TH_GRID, 10);
				
				draw_line = 1;
			} else {
				draw_line = 0;
			}
		}
		
		if (draw_line) {
			glBegin(GL_LINE_STRIP);
	        vert[1]= a*vd->grid;
	        vert[0]= grid;
	        glVertex3fv(vert );
	        vert[0]= -grid;
	        glVertex3fv(vert);
			glEnd();
		}
	}
	
	/* draw the Z axis line */	
	/* check for the 'show Z axis' preference */
	if (vd->gridflag & V3D_SHOW_Z) {
		make_axis_color(col, col2, 'z');
		glColor3ubv(col2);
		
		glBegin(GL_LINE_STRIP);
		vert[0]= 0;
		vert[1]= 0;
		vert[2]= grid;
		glVertex3fv(vert );
		vert[2]= -grid;
		glVertex3fv(vert);
		glEnd();
	}

	if(G.zbuf && G.obedit) glDepthMask(1);	

}

static void drawcursor(void)
{

	if(G.f & G_PLAYANIM) return;
	
	project_short( give_cursor(), &G.vd->mx);

	G.vd->mxo= G.vd->mx;
	G.vd->myo= G.vd->my;

	if( G.vd->mx!=3200) {
		
		setlinestyle(0); 
		cpack(0xFF);
		circ((float)G.vd->mx, (float)G.vd->my, 10.0);
		setlinestyle(4); 
		cpack(0xFFFFFF);
		circ((float)G.vd->mx, (float)G.vd->my, 10.0);
		setlinestyle(0);
		cpack(0x0);
		
		sdrawline(G.vd->mx-20, G.vd->my, G.vd->mx-5, G.vd->my);
		sdrawline(G.vd->mx+5, G.vd->my, G.vd->mx+20, G.vd->my);
		sdrawline(G.vd->mx, G.vd->my-20, G.vd->mx, G.vd->my-5);
		sdrawline(G.vd->mx, G.vd->my+5, G.vd->mx, G.vd->my+20);
	}
}

static void view3d_get_viewborder_size(View3D *v3d, float size_r[2])
{
	float winmax= MAX2(v3d->area->winx, v3d->area->winy);
	float aspect= (float) (G.scene->r.xsch*G.scene->r.xasp)/(G.scene->r.ysch*G.scene->r.yasp);

	if(aspect>1.0) {
		size_r[0]= winmax;
		size_r[1]= winmax/aspect;
	} else {
		size_r[0]= winmax*aspect;
		size_r[1]= winmax;
	}
}

void calc_viewborder(struct View3D *v3d, rcti *viewborder_r)
{
	float zoomfac, size[2];

	view3d_get_viewborder_size(v3d, size);

		/* magic zoom calculation, no idea what
	     * it signifies, if you find out, tell me! -zr
		 */
	/* simple, its magic dude!
	 * well, to be honest, this gives a natural feeling zooming
	 * with multiple keypad presses (ton)
	 */
	
	zoomfac= (M_SQRT2 + v3d->camzoom/50.0);
	zoomfac= (zoomfac*zoomfac)*0.25;
	
	size[0]= size[0]*zoomfac;
	size[1]= size[1]*zoomfac;

		/* center in window */
	viewborder_r->xmin= 0.5*v3d->area->winx - 0.5*size[0];
	viewborder_r->ymin= 0.5*v3d->area->winy - 0.5*size[1];
	viewborder_r->xmax= viewborder_r->xmin + size[0];
	viewborder_r->ymax= viewborder_r->ymin + size[1];
}

void view3d_set_1_to_1_viewborder(View3D *v3d)
{
	float size[2];
	int im_width= (G.scene->r.size*G.scene->r.xsch)/100;

	view3d_get_viewborder_size(v3d, size);

	v3d->camzoom= (sqrt(4.0*im_width/size[0]) - M_SQRT2)*50.0;
	v3d->camzoom= CLAMPIS(v3d->camzoom, -30, 300);
}

static void drawviewborder(void)
{
	float fac, a;
	float x1, x2, y1, y2;
	float x3, y3, x4, y4;
	rcti viewborder;
	
	calc_viewborder(G.vd, &viewborder);
	x1= viewborder.xmin;
	y1= viewborder.ymin;
	x2= viewborder.xmax;
	y2= viewborder.ymax;

	/* passepartout, in color of backdrop minus 50 */
	if(G.scene->r.scemode & R_PASSEPARTOUT) {
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glEnable(GL_BLEND);
		glColor4ub(0, 0, 0, 50);
		
		if (x1 > 0.0) {
			glRectf(0.0, (float)curarea->winy, x1, 0.0);
			glRectf(x2, (float)curarea->winy, (float)curarea->winx, 0.0);
		}
		if (y1 > 0.0)	{
			glRectf(x1, (float)curarea->winy, x2, y2);
			glRectf(x1, y1, x2, 0.0);
		}
		glDisable(GL_BLEND);
	}
	/* edge */
	setlinestyle(3);
	cpack(0);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); 
	glRectf(x1+1,  y1-1,  x2+1,  y2-1); 
	
	cpack(0xFFFFFF);
	glRectf(x1,  y1,  x2,  y2); 

	/* border */
	if(G.scene->r.mode & R_BORDER) {
		
		cpack(0);
		x3= x1+ G.scene->r.border.xmin*(x2-x1);
		y3= y1+ G.scene->r.border.ymin*(y2-y1);
		x4= x1+ G.scene->r.border.xmax*(x2-x1);
		y4= y1+ G.scene->r.border.ymax*(y2-y1);
		
		glRectf(x3+1,  y3-1,  x4+1,  y4-1); 
		
		cpack(0x4040FF);
		glRectf(x3,  y3,  x4,  y4); 
	}

	/* safety border */

	fac= 0.1;
	
	a= fac*(x2-x1);
	x1+= a; 
	x2-= a;

	a= fac*(y2-y1);
	y1+= a;
	y2-= a;

	cpack(0);
	glRectf(x1+1,  y1-1,  x2+1,  y2-1);
	cpack(0xFFFFFF);
	glRectf(x1,  y1,  x2,  y2);

	setlinestyle(0);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

}


void backdrawview3d(int test)
{
	struct Base *base;

	if(G.f & (G_VERTEXPAINT|G_FACESELECT|G_TEXTUREPAINT|G_WEIGHTPAINT));
	else if(G.obedit && G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT));
	else {
		G.vd->flag &= ~V3D_NEEDBACKBUFDRAW;
		return;
	}

	if( !(G.vd->flag & V3D_NEEDBACKBUFDRAW) ) return;

	if(test) {
		if(qtest()) {
			addafterqueue(curarea->win, BACKBUFDRAW, 1);
			return;
		}
	}
	persp(PERSP_VIEW);

#ifdef __APPLE__
	glDrawBuffer(GL_AUX0);
#endif	
	if(G.vd->drawtype > OB_WIRE) G.zbuf= TRUE;
	curarea->win_swap &= ~WIN_BACK_OK;
	
	glDisable(GL_DITHER);

	glClearColor(0.0, 0.0, 0.0, 0.0); 
	if(G.zbuf) {
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else {
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
	}
	
	G.f |= G_BACKBUFSEL;
	
	base= (G.scene->basact);
	if(base && (base->lay & G.vd->lay)) {
		draw_object_backbufsel(base->object);
	}

	G.vd->flag &= ~V3D_NEEDBACKBUFDRAW;

	G.f &= ~G_BACKBUFSEL;
	G.zbuf= FALSE; 
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_DITHER);

#ifdef __APPLE__
	glDrawBuffer(GL_BACK); /* we were in aux buffers */
#endif

	/* it is important to end a view in a transform compatible with buttons */
	persp(PERSP_WIN);  // set ortho
	bwin_scalematrix(curarea->win, G.vd->blockscale, G.vd->blockscale, G.vd->blockscale);

}

		
void drawname(Object *ob)
{
	cpack(0x404040);
	glRasterPos3f(0.0,  0.0,  0.0);
	
	BMF_DrawString(G.font, " ");
	BMF_DrawString(G.font, ob->id.name+2);
}


static void draw_selected_name(char *name)
{
	char info[128];

	sprintf(info, "(%d) %s", CFRA, name);

	BIF_ThemeColor(TH_TEXT_HI);
	glRasterPos2i(30,  10);
	BMF_DrawString(G.fonts, info);
}


static void draw_view_icon(void)
{
	BIFIconID icon;
	
	if(G.vd->view==7) icon= ICON_AXIS_TOP;
	else if(G.vd->view==1) icon= ICON_AXIS_FRONT;
	else if(G.vd->view==3) icon= ICON_AXIS_SIDE;
	else return ;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); 
	
	glRasterPos2f(5.0, 5.0);
	BIF_draw_icon(icon);
	
	glBlendFunc(GL_ONE,  GL_ZERO); 
	glDisable(GL_BLEND);
}

/* ******************* view3d space & buttons ************** */

static void view3d_change_bgpic_ima(View3D *v3d, Image *newima) {
	if (v3d->bgpic && v3d->bgpic->ima!=newima) {
		if (newima)
			id_us_plus((ID*) newima);
		if (v3d->bgpic->ima)
			v3d->bgpic->ima->id.us--;
		v3d->bgpic->ima= newima;

		if(v3d->bgpic->rect) MEM_freeN(v3d->bgpic->rect);
		v3d->bgpic->rect= NULL;
		
		allqueue(REDRAWVIEW3D, 0);
	}
}
static void view3d_change_bgpic_tex(View3D *v3d, Tex *newtex) {
	if (v3d->bgpic && v3d->bgpic->tex!=newtex) {
		if (newtex)
			id_us_plus((ID*) newtex);
		if (v3d->bgpic->tex)
			v3d->bgpic->tex->id.us--;
		v3d->bgpic->tex= newtex;
		
		allqueue(REDRAWVIEW3D, 0);
	}
}

static void load_bgpic_image(char *name)
{
	Image *ima;
	View3D *vd;
	
	areawinset(curarea->win);
	vd= G.vd;
	if(vd==0 || vd->bgpic==0) return;
	
	ima= add_image(name);
	if(ima) {
		if(vd->bgpic->ima) {
			vd->bgpic->ima->id.us--;
		}
		vd->bgpic->ima= ima;
		
		free_image_buffers(ima);	/* force read again */
		ima->ok= 1;
	}
	allqueue(REDRAWVIEW3D, 0);
	
}

/* this one assumes there is only one global active object in blender...  (for object panel) */
static float ob_eul[4];	// used for quat too....
/* this one assumes there is only one editmode in blender...  (for object panel) */
static float ve_median[4];

/* is used for both read and write... */
static void v3d_editvertex_buts(uiBlock *block, Object *ob, float lim)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	float median[4];
	int tot, totw, totedge;
	
	median[0]= median[1]= median[2]= median[3]= 0.0;
	tot= totw= totedge= 0;
	
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		
		eve= em->verts.first;
		while(eve) {
			if(eve->f & 1) {
				tot++;
				VecAddf(median, median, eve->co);
			}
			eve= eve->next;
		}
		if(me->medge) {
			eed= em->edges.first;
			while(eed) {
				if((eed->f & SELECT)) {
					totedge++;
					median[3]+= eed->crease;
				}
				eed= eed->next;
			}
		}
	}
	else if(ob->type==OB_CURVE || ob->type==OB_SURF) {
		extern ListBase editNurb; /* editcurve.c */
		Nurb *nu;
		BPoint *bp;
		BezTriple *bezt;
		int a;
		
		nu= editNurb.first;
		while(nu) {
			if((nu->type & 7)==1) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(bezt->f2 & 1) {
						VecAddf(median, median, bezt->vec[1]);
						tot++;
					}
					else {
						if(bezt->f1 & 1) {
							VecAddf(median, median, bezt->vec[0]);
							tot++;
						}
						if(bezt->f3 & 1) {
							VecAddf(median, median, bezt->vec[2]);
							tot++;
						}
					}
					bezt++;
				}
			}
			else {
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				while(a--) {
					if(bp->f1 & 1) {
						VecAddf(median, median, bp->vec);
						median[3]+= bp->vec[3];
						totw++;
						tot++;
					}
					bp++;
				}
			}
			nu= nu->next;
		}
	}
	else if(ob->type==OB_LATTICE) {
		BPoint *bp;
		int a;
		
		a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
		bp= editLatt->def;
		while(a--) {
			if(bp->f1 & SELECT) {
				VecAddf(median, median, bp->vec);
				tot++;
			}
			bp++;
		}
	}
	
	if(tot==0) return;

	median[0] /= (float)tot;
	median[1] /= (float)tot;
	median[2] /= (float)tot;
	if(totedge) median[3] /= (float)totedge;
	else median[3] /= (float)tot;
	
	if(G.vd->flag & V3D_GLOBAL_STATS)
		Mat4MulVecfl(ob->obmat, median);
	
	if(block) {	// buttons
	
		
		uiBlockBeginAlign(block);
		uiDefButS(block, TOG|BIT|13, REDRAWVIEW3D, "Global",		160, 150, 70, 19, &G.vd->flag, 0, 0, 0, 0, "Displays global values");
		uiDefButS(block, TOGN|BIT|13, REDRAWVIEW3D, "Local",			230, 150, 70, 19, &G.vd->flag, 0, 0, 0, 0, "Displays local values");
		
		QUATCOPY(ve_median, median);
		
		uiBlockBeginAlign(block);
		if(tot==1) {
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Vertex X:",	10, 110, 290, 19, &(ve_median[0]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Vertex Y:",	10, 90, 290, 19, &(ve_median[1]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Vertex Z:",	10, 70, 290, 19, &(ve_median[2]), -lim, lim, 10, 3, "");
			if(totw==1)
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Vertex W:",	10, 50, 290, 19, &(ve_median[3]), 0.01, 100.0, 10, 3, "");
		}
		else {
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median X:",	10, 110, 290, 19, &(ve_median[0]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median Y:",	10, 90, 290, 19, &(ve_median[1]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median Z:",	10, 70, 290, 19, &(ve_median[2]), -lim, lim, 10, 3, "");
			if(totw==tot)
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median W:",	10, 50, 290, 19, &(ve_median[3]), 0.01, 100.0, 10, 3, "");
		}
		uiBlockEndAlign(block);
		
		if(totedge==1)
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Crease W:",	10, 30, 290, 19, &(ve_median[3]), 0.0, 1.0, 10, 3, "");
		else if(totedge>1)
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median Crease W:",	10, 30, 290, 19, &(ve_median[3]), 0.0, 1.0, 10, 3, "");
		
	}
	else {	// apply
		
		if(G.vd->flag & V3D_GLOBAL_STATS) {
			Mat4Invert(ob->imat, ob->obmat);
			Mat4MulVecfl(ob->imat, median);
			Mat4MulVecfl(ob->imat, ve_median);
		}
		VecSubf(median, ve_median, median);
		median[3]= ve_median[3]-median[3];
		
		if(ob->type==OB_MESH) {
			float diffac= 1.0;
			
			eve= em->verts.first;
			while(eve) {
				if(eve->f & 1) {
					VecAddf(eve->co, eve->co, median);
				}
				eve= eve->next;
			}
			
			/* calculate the differences to squeeze a range smaller when values are close to 1 or 0 */
			/* this way you can edit a median value which is applied on clipped values :) */
			if(totedge>1) {
				float max= 0.0;
				for(eed= em->edges.first; eed; eed= eed->next) {
					if(eed->f & SELECT) {
						if(max < ABS(eed->crease-ve_median[3])) max= ABS(eed->crease-ve_median[3]);
					}
				}
				if(max>0.0) {
					if(ve_median[3]> 0.5) diffac= (1.0-ve_median[3])/max;
					else diffac= (ve_median[3])/max;
					if(diffac>1.0) diffac= 1.0;
				}
			}
			
			for(eed= em->edges.first; eed; eed= eed->next) {
				if(eed->f & SELECT) {
					eed->crease+= median[3];
					eed->crease= ve_median[3] + diffac*(eed->crease-ve_median[3]);
					
					CLAMP(eed->crease, 0.0, 1.0);
				}
			}
			
			recalc_editnormals();
		}
		else if(ob->type==OB_CURVE || ob->type==OB_SURF) {
			extern ListBase editNurb; /* editcurve.c */
			Nurb *nu;
			BPoint *bp;
			BezTriple *bezt;
			int a;
			
			nu= editNurb.first;
			while(nu) {
				if((nu->type & 7)==1) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while(a--) {
						if(bezt->f2 & 1) {
							VecAddf(bezt->vec[0], bezt->vec[0], median);
							VecAddf(bezt->vec[1], bezt->vec[1], median);
							VecAddf(bezt->vec[2], bezt->vec[2], median);
						}
						else {
							if(bezt->f1 & 1) {
								VecAddf(bezt->vec[0], bezt->vec[0], median);
							}
							if(bezt->f3 & 1) {
								VecAddf(bezt->vec[2], bezt->vec[2], median);
							}
						}
						bezt++;
					}
				}
				else {
					bp= nu->bp;
					a= nu->pntsu*nu->pntsv;
					while(a--) {
						if(bp->f1 & 1) {
							VecAddf(bp->vec, bp->vec, median);
							bp->vec[3]+= median[3];
						}
						bp++;
					}
				}
				test2DNurb(nu);
				testhandlesNurb(nu); /* test for bezier too */

				nu= nu->next;
			}
		}
		else if(ob->type==OB_LATTICE) {
			BPoint *bp;
			int a;
			
			a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
			bp= editLatt->def;
			while(a--) {
				if(bp->f1 & SELECT) {
					VecAddf(median, median, bp->vec);
					VecAddf(bp->vec, bp->vec, median);
				}
				bp++;
			}
		}
		
		BIF_undo_push("Transform properties");
	}
}

static void v3d_posearmature_buts(uiBlock *block, Object *ob, float lim)
{
	bArmature *arm;
	Bone *bone;

	arm = get_armature(OBACT);
	if (!arm)
		return;

	bone = get_first_selected_bone();

	if (!bone)
		return;
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "QuatX:",	10, 120, 140, 19, bone->quat+1, -100.0, 100.0, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "QuatZ:",	10, 100, 140, 19, bone->quat+3, -100.0, 100.0, 10, 3, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "QuatY:",	160, 120, 140, 19, bone->quat+2, -100.0, 100.0, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "QuatW:",	160, 100, 140, 19, bone->quat, -100.0, 100.0, 10, 3, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "LocX:",	10, 70, 140, 19, bone->loc, -lim, lim, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "LocY:",	10, 50, 140, 19, bone->loc+1, -lim, lim, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "locZ:",	10, 30, 140, 19, bone->loc+2, -lim, lim, 100, 3, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "SizeX:",	160, 70, 140, 19, bone->size, -lim, lim, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "SizeY:",	160, 50, 140, 19, bone->size+1, -lim, lim, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "SizeZ:",	160, 30, 140, 19, bone->size+2, -lim, lim, 10, 3, "");
	uiBlockEndAlign(block);
}

static void v3d_editarmature_buts(uiBlock *block, Object *ob, float lim)
{
	EditBone *ebone;
	
	ebone= G.edbo.first;

	for (ebone = G.edbo.first; ebone; ebone=ebone->next){
		if (ebone->flag & BONE_SELECTED)
			break;
	}

	if (!ebone)
		return;
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "RootX:",	10, 70, 140, 19, ebone->head, -lim, lim, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "RootY:",	10, 50, 140, 19, ebone->head+1, -lim, lim, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "RootZ:",	10, 30, 140, 19, ebone->head+2, -lim, lim, 100, 3, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "TipX:",	160, 70, 140, 19, ebone->tail, -lim, lim, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "TipY:",	160, 50, 140, 19, ebone->tail+1, -lim, lim, 100, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "TipZ:",	160, 30, 140, 19, ebone->tail+2, -lim, lim, 100, 3, "");
	uiBlockEndAlign(block);
	ob_eul[0]= 180.0*ebone->roll/M_PI;
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "Roll:",	10, 100, 140, 19, ob_eul, -lim, lim, 100, 3, "");

}

static void v3d_editmetaball_buts(uiBlock *block, Object *ob, float lim)
{
	extern MetaElem *lastelem;

	if(lastelem) {
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_RECALCMBALL, "LocX:", 10, 70, 140, 19, &lastelem->x, -lim, lim, 100, 3, "");
		uiDefButF(block, NUM, B_RECALCMBALL, "LocY:", 10, 50, 140, 19, &lastelem->y, -lim, lim, 100, 3, "");
		uiDefButF(block, NUM, B_RECALCMBALL, "LocZ:", 10, 30, 140, 19, &lastelem->z, -lim, lim, 100, 3, "");

		uiBlockBeginAlign(block);
		if(lastelem->type!=MB_BALL)
		uiDefButF(block, NUM, B_RECALCMBALL, "dx:", 160, 70, 140, 19, &lastelem->expx, 0, lim, 100, 3, "");
		if((lastelem->type!=MB_BALL) && (lastelem->type!=MB_TUBE))
		uiDefButF(block, NUM, B_RECALCMBALL, "dy:", 160, 50, 140, 19, &lastelem->expy, 0, lim, 100, 3, "");
		if((lastelem->type==MB_ELIPSOID) || (lastelem->type==MB_CUBE))
		uiDefButF(block, NUM, B_RECALCMBALL, "dz:", 160, 30, 140, 19, &lastelem->expz, 0, lim, 100, 3, "");

		uiBlockEndAlign(block);

		uiDefButF(block, NUM, B_RECALCMBALL, "Stiffness:", 10, 100, 140, 19, &lastelem->s, 0, lim, 100, 3, "");
	}
}

void do_viewbuts(unsigned short event)
{
	View3D *vd;
	Object *ob= OBACT;
	char *name;
	
	vd= G.vd;
	if(vd==0) return;

	switch(event) {
	case B_LOADBGPIC:
		if(vd->bgpic && vd->bgpic->ima) name= vd->bgpic->ima->name;
		else name= G.ima;
		
		if(G.qual==LR_CTRLKEY)
			activate_imageselect(FILE_SPECIAL, "Select Image", name, load_bgpic_image);
		else
			activate_fileselect(FILE_SPECIAL, "Select Image", name, load_bgpic_image);
		break;
		
	case B_BLENDBGPIC:
		if(vd->bgpic && vd->bgpic->rect) setalpha_bgpic(vd->bgpic);
		addqueue(curarea->win, REDRAW, 1);
		break;
		
	case B_BGPICBROWSE:
		if(vd->bgpic) {
			if (vd->menunr==-2) {
				activate_databrowse((ID*) vd->bgpic->ima, ID_IM, 0, B_BGPICBROWSE, &vd->menunr, do_viewbuts);
			} else if (vd->menunr>0) {
				Image *newima= (Image*) BLI_findlink(&G.main->image, vd->menunr-1);

				if (newima)
					view3d_change_bgpic_ima(vd, newima);
			}
		}
		break;
		
	case B_BGPICCLEAR:
		if (vd->bgpic)
			view3d_change_bgpic_ima(vd, NULL);
		break;
		
	case B_BGPICTEX:
		if (vd->bgpic) {
			if (vd->texnr==-2) {
				activate_databrowse((ID*) vd->bgpic->tex, ID_TE, 0, B_BGPICTEX, &vd->texnr, do_viewbuts);
			} else if (vd->texnr>0) {
				Tex *newtex= (Tex*) BLI_findlink(&G.main->tex, vd->texnr-1);
				
				if (newtex)
					view3d_change_bgpic_tex(vd, newtex);
			}
		}
		break;
		
	case B_BGPICTEXCLEAR:
		if (vd->bgpic)
			view3d_change_bgpic_tex(vd, NULL);
		break;
		
	case B_OBJECTPANELROT:
		if(ob) {
			ob->rot[0]= M_PI*ob_eul[0]/180.0;
			ob->rot[1]= M_PI*ob_eul[1]/180.0;
			ob->rot[2]= M_PI*ob_eul[2]/180.0;
			allqueue(REDRAWVIEW3D, 1);
		}
		break;
	
	case B_OBJECTPANELMEDIAN:
		if(ob) {
			v3d_editvertex_buts(NULL, ob, 1.0);
			makeDispList(ob);
			allqueue(REDRAWVIEW3D, 1);
		}
		break;
	case B_OBJECTPANELPARENT:
		if(ob) {
			if( test_parent_loop(ob->parent, ob) ) 
				ob->parent= NULL;
			allqueue(REDRAWVIEW3D, 1);
		}
		break;
		
	case B_ARMATUREPANEL1:
		{
			EditBone *ebone, *child;
			
			ebone= G.edbo.first;
			for (ebone = G.edbo.first; ebone; ebone=ebone->next){
				if (ebone->flag & BONE_SELECTED) break;
			}
			if (ebone) {
				ebone->roll= M_PI*ob_eul[0]/180.0;
				//	Update our parent
				if (ebone->parent && ebone->flag & BONE_IK_TOPARENT){
					VECCOPY (ebone->parent->tail, ebone->head);
				}
			
				//	Update our children if necessary
				for (child = G.edbo.first; child; child=child->next){
					if (child->parent == ebone && child->flag & BONE_IK_TOPARENT){
						VECCOPY (child->head, ebone->tail);
					}
				}
				allqueue(REDRAWVIEW3D, 1);
			}
		}
		break;
	case B_ARMATUREPANEL2:
		{
			bPoseChannel *chan;
			bArmature *arm;
			Bone *bone;

			arm = get_armature(OBACT);
			if (!arm) return;
			bone = get_first_selected_bone();
		
			if (!bone) return;

			/* This is similar to code in special_trans_update */
	
			if (!G.obpose->pose) G.obpose->pose= MEM_callocN(sizeof(bPose), "pose");
			chan = MEM_callocN (sizeof (bPoseChannel), "transPoseChannel");
		
			chan->flag |= POSE_LOC|POSE_ROT|POSE_SIZE;
			memcpy (chan->loc, bone->loc, sizeof (chan->loc));
			memcpy (chan->quat, bone->quat, sizeof (chan->quat));
			memcpy (chan->size, bone->size, sizeof (chan->size));
			strcpy (chan->name, bone->name);
			
			set_pose_channel (G.obpose->pose, chan);

			rebuild_all_armature_displists();

			allqueue(REDRAWVIEW3D, 1);
		}
	}
}


static void view3d_panel_object(short cntrl)	// VIEW3D_HANDLER_OBJECT
{
	uiBlock *block;
	uiBut *bt;
	Object *ob= OBACT;
	float lim;
	
	if(ob==NULL) return;

	block= uiNewBlock(&curarea->uiblocks, "view3d_panel_object", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(VIEW3D_HANDLER_OBJECT);  // for close and esc
	
/* (ton) can't use the rename trick for paint... panel names and settings are stored in the files and
   used to find previous locations when re-open. This causes flipping */

	if(uiNewPanel(curarea, block, "Transform Properties", "View3d", 10, 230, 318, 204)==0) return;
	
	if(ob->id.lib) uiSetButLock(1, "Can't edit library data");
	
	if(G.f & (G_VERTEXPAINT|G_FACESELECT|G_TEXTUREPAINT|G_WEIGHTPAINT)) {
		uiBlockSetFlag(block, UI_BLOCK_FRONTBUFFER);	// force old style frontbuffer draw
	}
	else {
		bt= uiDefBut(block, TEX, B_IDNAME, "OB: ",	10,180,140,20, ob->id.name+2, 0.0, 19.0, 0, 0, "");
		uiButSetFunc(bt, test_idbutton_cb, ob->id.name, NULL);

		uiDefIDPoinBut(block, test_obpoin_but, B_OBJECTPANELPARENT, "Par:", 160, 180, 140, 20, &ob->parent, "Parent Object"); 
	}

	lim= 1000.0*MAX2(1.0, G.vd->grid);

	if(ob==G.obedit) {
		if(ob->type==OB_ARMATURE) v3d_editarmature_buts(block, ob, lim);
		if(ob->type==OB_MBALL) v3d_editmetaball_buts(block, ob, lim);
		else v3d_editvertex_buts(block, ob, lim);
	}
	else if(ob==G.obpose) {
		v3d_posearmature_buts(block, ob, lim);
	}
	else if(G.f & (G_VERTEXPAINT|G_TEXTUREPAINT)) {
		extern VPaint Gvp;         /* from vpaint */
		static float hsv[3], old[3];	// used as temp mem for picker
		uiBlockPickerButtons(block, &Gvp.r, hsv, old, 'f', REDRAWBUTSEDIT);	/* 'f' is for floating panel */
	}
	else {
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, REDRAWVIEW3D, "LocX:",		10, 140, 140, 19, &(ob->loc[0]), -lim, lim, 100, 3, "");
		uiDefButF(block, NUM, REDRAWVIEW3D, "LocY:",		10, 120, 140, 19, &(ob->loc[1]), -lim, lim, 100, 3, "");
		uiDefButF(block, NUM, REDRAWVIEW3D, "LocZ:",		10, 100, 140, 19, &(ob->loc[2]), -lim, lim, 100, 3, "");
		
		ob_eul[0]= 180.0*ob->rot[0]/M_PI;
		ob_eul[1]= 180.0*ob->rot[1]/M_PI;
		ob_eul[2]= 180.0*ob->rot[2]/M_PI;
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_OBJECTPANELROT, "RotX:",	10, 70, 140, 19, &(ob_eul[0]), -lim, lim, 1000, 3, "");
		uiDefButF(block, NUM, B_OBJECTPANELROT, "RotY:",	10, 50, 140, 19, &(ob_eul[1]), -lim, lim, 1000, 3, "");
		uiDefButF(block, NUM, B_OBJECTPANELROT, "RotZ:",	10, 30, 140, 19, &(ob_eul[2]), -lim, lim, 1000, 3, "");
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, REDRAWVIEW3D, "SizeX:",		160, 70, 140, 19, &(ob->size[0]), -lim, lim, 10, 3, "");
		uiDefButF(block, NUM, REDRAWVIEW3D, "SizeY:",		160, 50, 140, 19, &(ob->size[1]), -lim, lim, 10, 3, "");
		uiDefButF(block, NUM, REDRAWVIEW3D, "SizeZ:",		160, 30, 140, 19, &(ob->size[2]), -lim, lim, 10, 3, "");
		uiBlockEndAlign(block);
	}
	uiClearButLock();
}

static void view3d_panel_background(short cntrl)	// VIEW3D_HANDLER_BACKGROUND
{
	uiBlock *block;
	View3D *vd;
	ID *id;
	char *strp;
	
	vd= G.vd;

	block= uiNewBlock(&curarea->uiblocks, "view3d_panel_background", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE  | cntrl);
	uiSetPanelHandler(VIEW3D_HANDLER_BACKGROUND);  // for close and esc
	if(uiNewPanel(curarea, block, "Background Image", "View3d", 340, 10, 318, 204)==0) return;

	if(G.f & (G_VERTEXPAINT|G_FACESELECT|G_TEXTUREPAINT|G_WEIGHTPAINT)) {
		uiBlockSetFlag(block, UI_BLOCK_FRONTBUFFER);	// force old style frontbuffer draw
	}
	
	if(vd->flag & V3D_DISPBGPIC) {
		if(vd->bgpic==0) {
			vd->bgpic= MEM_callocN(sizeof(BGpic), "bgpic");
			vd->bgpic->size= 5.0;
			vd->bgpic->blend= 0.5;
		}
	}
	
	uiDefButBitS(block, TOG, V3D_DISPBGPIC, REDRAWVIEW3D, "Use Background Image", 0, 162, 200, 20, &vd->flag, 0, 0, 0, 0, "Display an image in the background of the 3D View");
	
	uiDefBut(block, LABEL, 1, " ",	206, 162, 84, 20, NULL, 0.0, 0.0, 0, 0, "");
	
	
	if(vd->flag & V3D_DISPBGPIC) {

		/* Background Image */
		uiDefBut(block, LABEL, 1, "Image:",	0, 128, 76, 19, NULL, 0.0, 0.0, 0, 0, "");
		
		uiBlockBeginAlign(block);
		uiDefIconBut(block, BUT, B_LOADBGPIC, ICON_FILESEL,	90, 128, 20, 20, 0, 0, 0, 0, 0, "Open a new background image");

		id= (ID *)vd->bgpic->ima;
		IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->image), id, &(vd->menunr));
		if(strp[0]) {
		
			uiDefButS(block, MENU, B_BGPICBROWSE, strp, 	110, 128, 20, 20, &(vd->menunr), 0, 0, 0, 0, "Select a background image");
		
			if(vd->bgpic->ima)  {
				uiDefBut(block, TEX,	    0,"BG: ",		130, 128, 140, 20, &vd->bgpic->ima->name,0.0,100.0, 0, 0, "The currently selected background image");
				uiDefIconBut(block, BUT, B_BGPICCLEAR, ICON_X, 270, 128, 20, 20, 0, 0, 0, 0, 0, "Remove background image link");
			}
			uiBlockEndAlign(block);
		} else {
			uiBlockEndAlign(block);
		}
		MEM_freeN(strp);


		/* Background texture */
		uiDefBut(block, LABEL, 1, "Texture:",	0, 100, 76, 19, NULL, 0.0, 0.0, 0, 0, "");
		
		id= (ID *)vd->bgpic->tex;
		IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->tex), id, &(vd->texnr));
		if (strp[0])
			uiBlockBeginAlign(block);
			uiDefButS(block, MENU, B_BGPICTEX, strp,			90, 100, 20,20, &(vd->texnr), 0, 0, 0, 0, "Select a texture to use as an animated background image");
		MEM_freeN(strp);
		
		if (id) {
			uiDefBut(block, TEX, B_IDNAME, "TE:",				110, 100, 160, 20, id->name+2, 0.0, 18.0, 0, 0, "");
			uiDefIconBut(block, BUT, B_BGPICTEXCLEAR, ICON_X, 	270, 100, 20, 20, 0, 0, 0, 0, 0, "Remove background texture link");
			uiBlockEndAlign(block);
		} else {
			uiBlockEndAlign(block);
		}

		uiDefButF(block, NUMSLI, B_BLENDBGPIC, "Blend:",	0, 60 , 290, 19, &vd->bgpic->blend, 0.0,1.0, 0, 0, "Set the transparency of the background image");

		uiDefButF(block, NUM, REDRAWVIEW3D, "Size:",		0, 28, 140, 19, &vd->bgpic->size, 0.1, 250.0, 100, 0, "Set the size (width) of the background image");

		uiDefButF(block, NUM, REDRAWVIEW3D, "X Offset:",	0, 6, 140, 19, &vd->bgpic->xof, -20.0,20.0, 10, 2, "Set the horizontal offset of the background image");
		uiDefButF(block, NUM, REDRAWVIEW3D, "Y Offset:",	150, 6, 140, 19, &vd->bgpic->yof, -20.0,20.0, 10, 2, "Set the vertical offset of the background image");

	
		
		// uiDefButF(block, NUM, REDRAWVIEW3D, "Size:", 		160,160,150,20, &vd->bgpic->size, 0.1, 250.0, 100, 0, "Set the size for the width of the BackGroundPic");
		


//		uiDefButF(block, NUMSLI, B_BLENDBGPIC, "Blend:",	120,100,190,20,&vd->bgpic->blend, 0.0,1.0, 0, 0, "Set the BackGroundPic transparency");
		
//		uiDefButF(block, NUM, B_DIFF, "Center X: ",	10,70,140,20,&vd->bgpic->xof, -20.0,20.0, 10, 2, "Set the BackGroundPic X Offset");
//		uiDefButF(block, NUM, B_DIFF, "Center Y: ",	160,70,140,20,&vd->bgpic->yof, -20.0,20.0, 10, 2, "Set the BackGroundPic Y Offset");

	}
}


static void view3d_panel_properties(short cntrl)	// VIEW3D_HANDLER_SETTINGS
{
	uiBlock *block;
	View3D *vd;
	float *curs;
	
	vd= G.vd;

	block= uiNewBlock(&curarea->uiblocks, "view3d_panel_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE  | cntrl);
	uiSetPanelHandler(VIEW3D_HANDLER_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "View Properties", "View3d", 340, 10, 318, 204)==0) return;

	if(G.f & (G_VERTEXPAINT|G_FACESELECT|G_TEXTUREPAINT|G_WEIGHTPAINT)) {
		uiBlockSetFlag(block, UI_BLOCK_FRONTBUFFER);	// force old style frontbuffer draw
	}

	uiDefBut(block, LABEL, 1, "Grid:",					10, 180, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefButF(block, NUM, REDRAWVIEW3D, "Spacing:",		10, 160, 140, 19, &vd->grid, 0.001, 100.0, 10, 0, "Set the distance between grid lines");
	uiDefButS(block, NUM, REDRAWVIEW3D, "Lines:",		10, 136, 140, 19, &vd->gridlines, 0.0, 100.0, 100, 0, "Set the number of grid lines");

	uiDefBut(block, LABEL, 1, "3D Display:",					160, 180, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefButBitS(block, TOG, V3D_SHOW_FLOOR, REDRAWVIEW3D, "Grid Floor",160, 160, 150, 19, &vd->gridflag, 0, 0, 0, 0, "Show the grid floor in free camera mode");
	uiDefButBitS(block, TOG, V3D_SHOW_X, REDRAWVIEW3D, "X Axis",		160, 136, 48, 19, &vd->gridflag, 0, 0, 0, 0, "Show the X Axis line");
	uiDefButBitS(block, TOG, V3D_SHOW_Y, REDRAWVIEW3D, "Y Axis",		212, 136, 48, 19, &vd->gridflag, 0, 0, 0, 0, "Show the Y Axis line");
	uiDefButBitS(block, TOG, V3D_SHOW_Z, REDRAWVIEW3D, "Z Axis",		262, 136, 48, 19, &vd->gridflag, 0, 0, 0, 0, "Show the Z Axis line");

	uiDefBut(block, LABEL, 1, "View Camera:",			10, 110, 140, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefButF(block, NUM, REDRAWVIEW3D, "Lens:",		10, 90, 140, 19, &vd->lens, 10.0, 120.0, 100, 0, "The lens angle in perspective view");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, REDRAWVIEW3D, "Clip Start:",	10, 66, 140, 19, &vd->near, vd->grid/10.0, 100.0, 10, 0, "Set the beginning of the range in which 3D objects are displayed (perspective view)");
	uiDefButF(block, NUM, REDRAWVIEW3D, "Clip End:",	10, 46, 140, 19, &vd->far, 1.0, 1000.0*vd->grid, 100, 0, "Set the end of the range in which 3D objects are displayed (perspective view)");
	uiBlockEndAlign(block);

	uiDefBut(block, LABEL, 1, "3D Cursor:",				160, 110, 140, 19, NULL, 0.0, 0.0, 0, 0, "");

	uiBlockBeginAlign(block);
	curs= give_cursor();
	uiDefButF(block, NUM, REDRAWVIEW3D, "X:",			160, 90, 150, 22, curs, -1000.0*vd->grid, 1000.0*vd->grid, 10, 0, "X co-ordinate of the 3D cursor");
	uiDefButF(block, NUM, REDRAWVIEW3D, "Y:",			160, 68, 150, 22, curs+1, -1000.0*vd->grid, 1000.0*vd->grid, 10, 0, "Y co-ordinate of the 3D cursor");
	uiDefButF(block, NUM, REDRAWVIEW3D, "Z:",			160, 46, 150, 22, curs+2, -1000.0*vd->grid, 1000.0*vd->grid, 10, 0, "Z co-ordinate of the 3D cursor");
	uiBlockEndAlign(block);

	uiDefButBitS(block, TOG, V3D_SELECT_OUTLINE, REDRAWVIEW3D, "Outline Selected Objects", 10, 10, 300, 19, &vd->flag, 0, 0, 0, 0, "Highlight selected objects with an outline, in Solid, Shaded or Textured viewport shading modes");

}



static void view3d_blockhandlers(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	short a;
	
	/* warning; blocks need to be freed each time, handlers dont remove */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);

	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
	
		switch(v3d->blockhandler[a]) {

		case VIEW3D_HANDLER_PROPERTIES:
			view3d_panel_properties(v3d->blockhandler[a+1]);
			break;
		case VIEW3D_HANDLER_BACKGROUND:
			view3d_panel_background(v3d->blockhandler[a+1]);
			break;
		case VIEW3D_HANDLER_OBJECT:
			view3d_panel_object(v3d->blockhandler[a+1]);
		
			break;
		
		}
		/* clear action value for event */
		v3d->blockhandler[a+1]= 0;
	}
	uiDrawBlocksPanels(sa, 0);

}

void drawview3dspace(ScrArea *sa, void *spacedata)
{
	Base *base;
	Object *ob;
	
	setwinmatrixview3d(0);	/* 0= no pick rect */
	setviewmatrixview3d();

	Mat4MulMat4(G.vd->persmat, G.vd->viewmat, curarea->winmat);
	Mat4Invert(G.vd->persinv, G.vd->persmat);
	Mat4Invert(G.vd->viewinv, G.vd->viewmat);

	if(G.vd->drawtype > OB_WIRE) {
		G.zbuf= TRUE;
		glEnable(GL_DEPTH_TEST);
		if(G.f & G_SIMULATION) {
			glClearColor(0.0, 0.0, 0.0, 0.0); 
		}
		else {
			float col[3];
			BIF_GetThemeColor3fv(TH_BACK, col);
			glClearColor(col[0], col[1], col[2], 0.0); 
		}
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glLoadIdentity();
	}
	else {
		float col[3];
		BIF_GetThemeColor3fv(TH_BACK, col);
		glClearColor(col[0], col[1], col[2], 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	
	myloadmatrix(G.vd->viewmat);
	persp(PERSP_STORE);  // store correct view for persp(PERSP_VIEW) calls

	// needs to be done always, gridview is adjusted in drawgrid() now
	G.vd->gridview= G.vd->grid;
	
	if(G.vd->view==0 || G.vd->persp!=0) {
		drawfloor();
		if(G.vd->persp==2) {
			if(G.scene->world) {
				if(G.scene->world->mode & WO_STARS) {
					RE_make_stars(star_stuff_init_func, star_stuff_vertex_func,
								  star_stuff_term_func);
				}
			}
			if(G.vd->flag & V3D_DISPBGPIC) draw_bgpic();
		}
	}
	else {
		drawgrid();

		if(G.vd->flag & V3D_DISPBGPIC) {
			draw_bgpic();
		}
	}
	
#if 0
	/* Lets be a little more selective about when and where we do this,
	 * or else armatures/poses/displists get recalculated all of the
	 * time
	 */
	clear_all_constraints();
#endif

	/* draw set first */
	if(G.scene->set) {
	
		/* patch: color remains constant */ 
		G.f |= G_PICKSEL;

		base= G.scene->set->base.first;
		while(base) {
			if(G.vd->lay & base->lay) {
				where_is_object(base->object);

				cpack(0x404040);
				draw_object(base);

				if(base->object->transflag & OB_DUPLI) {
					extern ListBase duplilist;
					Base tbase;
					
					tbase= *base;
					
					tbase.flag= OB_FROMDUPLI;
					make_duplilist(G.scene->set, base->object);
					ob= duplilist.first;
					while(ob) {
						tbase.object= ob;
						draw_object(&tbase);
						ob= ob->id.next;
					}
					free_duplilist();
					
				}
			}
			base= base->next;
		}
		
		G.f &= ~G_PICKSEL;
	}
	
	/* first calculate positions, we do this in separate loop to make sure displists
	   (mball, deform, etc) are recaluclated based on correct object (parent/children) positions
	*/
	base= G.scene->base.first;
	while(base) {
		if(G.vd->lay & base->lay) where_is_object(base->object);
		base= base->next;
	}
	
	/* then draw not selected and the duplis, but skip editmode object */
	base= G.scene->base.first;
	while(base) {
		
		if(G.vd->lay & base->lay) {
			
			/* dupli drawing temporal off here */
			if(FALSE && base->object->transflag & OB_DUPLI) {
				extern ListBase duplilist;
				Base tbase;

				/* draw original always first because of make_displist */
				draw_object(base);

				/* patch: color remains constant */ 
				G.f |= G_PICKSEL;
				cpack(0x404040);
				
				tbase.flag= OB_FROMDUPLI;
				make_duplilist(G.scene, base->object);

				ob= duplilist.first;
				while(ob) {
					tbase.object= ob;
					draw_object(&tbase);
					ob= ob->id.next;
				}
				free_duplilist();
				
				G.f &= ~G_PICKSEL;				
			}
			else if((base->flag & SELECT)==0) {
				if(base->object!=G.obedit) draw_object(base);
			}
			
		}
		
		base= base->next;
	}
	/* draw selected and editmode */
	base= G.scene->base.first;
	while(base) {
		
		if(G.vd->lay & base->lay) {
			if (base->object==G.obedit || ( base->flag & SELECT) ) 
				draw_object(base);
		}
		
		base= base->next;
	}

	if(G.moving) {
		BIF_drawConstraint();
		if(G.obedit) BIF_drawPropCircle();	// only editmode has proportional edit
	}

	/* duplis, draw as last to make sure the displists are ok */
	base= G.scene->base.first;
	while(base) {
		
		if(G.vd->lay & base->lay) {
			if(base->object->transflag & OB_DUPLI) {
				extern ListBase duplilist;
				Base tbase;

				/* patch: color remains constant */ 
				G.f |= G_PICKSEL;
				cpack(0x404040);
				
				tbase.flag= OB_FROMDUPLI;
				make_duplilist(G.scene, base->object);

				ob= duplilist.first;
				while(ob) {
					tbase.object= ob;
					draw_object(&tbase);
					ob= ob->id.next;
				}
				free_duplilist();
				
				G.f &= ~G_PICKSEL;				
			}
		}
		base= base->next;
	}

	if(G.scene->radio) RAD_drawall(G.vd->drawtype>=OB_SOLID);
	
	BIF_draw_manipulator(sa);
		
	if(G.zbuf) {
		G.zbuf= FALSE;
		glDisable(GL_DEPTH_TEST);
	}

	persp(PERSP_WIN);  // set ortho
	
	if(G.vd->persp>1) drawviewborder();
	drawcursor();
	draw_view_icon();

	ob= OBACT;
	if(ob!=0 && (U.uiflag & USER_DRAWVIEWINFO)) draw_selected_name(ob->id.name+2);
	
	draw_area_emboss(sa);
	
	/* it is important to end a view in a transform compatible with buttons */

	bwin_scalematrix(sa->win, G.vd->blockscale, G.vd->blockscale, G.vd->blockscale);
	view3d_blockhandlers(sa);

	curarea->win_swap= WIN_BACK_OK;
	
	if(G.f & (G_VERTEXPAINT|G_FACESELECT|G_TEXTUREPAINT|G_WEIGHTPAINT)) {
		G.vd->flag |= V3D_NEEDBACKBUFDRAW;
		addafterqueue(curarea->win, BACKBUFDRAW, 1);
	}
	// test for backbuf select
	if(G.obedit && G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
		extern int afterqtest(short win, unsigned short evt);	//editscreen.c

		G.vd->flag |= V3D_NEEDBACKBUFDRAW;
		if(afterqtest(curarea->win, BACKBUFDRAW)==0) {
			addafterqueue(curarea->win, BACKBUFDRAW, 1);
		}
	}

	/* run any view3d draw handler script links */
	if (sa->scriptlink.totscript)
		BPY_do_spacehandlers(sa, 0, SPACEHANDLER_VIEW3D_DRAW);

	/* run scene redraw script links */
	if((G.f & G_DOSCRIPTLINKS) && G.scene->scriptlink.totscript &&
			!during_script()) {
		BPY_do_pyscript((ID *)G.scene, SCRIPT_REDRAW);
	}

}


	/* Called back by rendering system, icky
	 */
void drawview3d_render(struct View3D *v3d)
{
	extern short v3d_windowmode;
	Base *base;
	Object *ob;

	free_all_realtime_images();

	v3d_windowmode= 1;
	setwinmatrixview3d(0);
	v3d_windowmode= 0;
	glMatrixMode(GL_PROJECTION);
	myloadmatrix(R.winmat);
	glMatrixMode(GL_MODELVIEW);
	
	setviewmatrixview3d();
	myloadmatrix(v3d->viewmat);
	Mat4MulMat4(v3d->persmat, v3d->viewmat, R.winmat);
	Mat4Invert(v3d->persinv, v3d->persmat);
	Mat4Invert(v3d->viewinv, v3d->viewmat);

	if(v3d->drawtype > OB_WIRE) {
		G.zbuf= TRUE;
		glEnable(GL_DEPTH_TEST);
	}

	if (v3d->drawtype==OB_TEXTURE && G.scene->world) {
		glClearColor(G.scene->world->horr, G.scene->world->horg, G.scene->world->horb, 0.0); 
	} else {
		float col[3];
		BIF_GetThemeColor3fv(TH_BACK, col);
		glClearColor(col[0], col[1], col[2], 0.0); 
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glLoadIdentity();
	myloadmatrix(v3d->viewmat);
	
	/* abuse! to make sure it doesnt draw the helpstuff */
	G.f |= G_SIMULATION;

	clear_all_constraints();
	do_all_ipos();
	if (G.f & G_DOSCRIPTLINKS) BPY_do_all_scripts(SCRIPT_FRAMECHANGED);
	do_all_keys();
	do_all_actions(NULL);
	do_all_ikas();

	test_all_displists();

	/* not really nice forcing of calc_ipo and where_is */
	ob= G.main->object.first;
	while(ob) {
		ob->ctime= -123.456;
		ob= ob->id.next;
	}

	/* first draw set */
	if(G.scene->set) {
	
		/* patch: color remains constant */ 
		G.f |= G_PICKSEL;
		
		base= G.scene->set->base.first;
		while(base) {
			if(v3d->lay & base->lay) {
				if ELEM3(base->object->type, OB_LAMP, OB_CAMERA, OB_LATTICE);
				else {
					where_is_object(base->object);
	
					cpack(0x404040);
					draw_object(base);
	
					if(base->object->transflag & OB_DUPLI) {
						extern ListBase duplilist;
						Base tbase;
						
						tbase.flag= OB_FROMDUPLI;
						make_duplilist(G.scene->set, base->object);
						ob= duplilist.first;
						while(ob) {
							tbase.object= ob;
							draw_object(&tbase);
							ob= ob->id.next;
						}
						free_duplilist();
					}
				}
			}
			base= base->next;
		}
		
		G.f &= ~G_PICKSEL;
	}

	clear_all_constraints();

	/* first not selected and duplis */
	base= G.scene->base.first;
	while(base) {
		
		if(v3d->lay & base->lay) {
			if ELEM3(base->object->type, OB_LAMP, OB_CAMERA, OB_LATTICE);
			else {
				where_is_object(base->object);
	
				if(base->object->transflag & OB_DUPLI) {
					extern ListBase duplilist;
					Base tbase;
					
					/* always draw original first because of make_displist */
					draw_object(base);
					
					/* patch: color remains constant */ 
					G.f |= G_PICKSEL;
					cpack(0x404040);
					
					tbase.flag= OB_FROMDUPLI;
					make_duplilist(G.scene, base->object);
					ob= duplilist.first;
					while(ob) {
						tbase.object= ob;
						draw_object(&tbase);
						ob= ob->id.next;
					}
					free_duplilist();
					
					G.f &= ~G_PICKSEL;				
				}
				else if((base->flag & SELECT)==0) {
					draw_object(base);
				}
			}
		}
		
		base= base->next;
	}

	/* draw selected */
	base= G.scene->base.first;
	while(base) {
		
		if ( ((base)->flag & SELECT) && ((base)->lay & v3d->lay) ) {
			if ELEM3(base->object->type, OB_LAMP, OB_CAMERA, OB_LATTICE);
			else draw_object(base);
		}
		
		base= base->next;
	}

	if(G.scene->radio) RAD_drawall(G.vd->drawtype>=OB_SOLID);

	if(G.zbuf) {
		G.zbuf= FALSE;
		glDisable(GL_DEPTH_TEST);
	}
	
	G.f &= ~G_SIMULATION;

	glFlush();

	glReadPixels(0, 0, R.rectx, R.recty, GL_RGBA, GL_UNSIGNED_BYTE, R.rectot);
	glLoadIdentity();

	free_all_realtime_images();
}


double tottime = 0.0;

int update_time(void)
{
	static double ltime;
	double time;

	if ((U.mixbufsize)&&(audiostream_pos() != CFRA)&&(G.scene->audio.flag & AUDIO_SYNC)) return 0;

	time = PIL_check_seconds_timer();
	
	tottime += (time - ltime);
	ltime = time;
	return (tottime < 0.0);
}

void inner_play_anim_loop(int init, int mode)
{
	ScrArea *sa;
	static ScrArea *oldsa;
	static double swaptime;
	static int curmode;

	/* init */
	if(init) {
		oldsa= curarea;
		swaptime= 1.0/(float)G.scene->r.frs_sec;
		tottime= 0.0;
		curmode= mode;

		return;
	}

	set_timecursor(CFRA);

	//clear_all_constraints();
	//do_all_ipos();
	//BPY_do_all_scripts(SCRIPT_FRAMECHANGED);
	//do_all_keys();
	//do_all_actions(NULL);
	//do_all_ikas();
	update_for_newframe_muted();

	//test_all_displists();

	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa==oldsa) {
			scrarea_do_windraw(sa);
		}
		else if(curmode & 1) { /* catch modes 1 and 3 */
			if ELEM(sa->spacetype, SPACE_VIEW3D, SPACE_SEQ) {
				scrarea_do_windraw(sa);
			}
		}
		
		sa= sa->next;	
	}
	
	/* make sure that swaptime passed by */
	tottime -= swaptime;
	while (update_time()) PIL_sleep_ms(1);

	if(CFRA>=EFRA) {
		if (tottime > 0.0) tottime = 0.0;
		CFRA= SFRA;
		audiostream_stop();
		audiostream_start( CFRA );
	}
	else {
		if (U.mixbufsize && (G.scene->audio.flag & AUDIO_SYNC)) CFRA = audiostream_pos();
		else CFRA++;
	}
}

/* play_anim: 'mode' defines where to play and if repeat is on:
 * - 0: current area
 * - 1: all view3d and seq areas
 * - 2: current area, no replay
 * - 3: all view3d and seq areas, no replay */
int play_anim(int mode)
{
	Base *base;
	ScrArea *sa, *oldsa;
	int cfraont;
	unsigned short event=0;
	short val;

	/* patch for very very old scenes */
	if(SFRA==0) SFRA= 1;
	if(EFRA==0) EFRA= 250;

	if(SFRA>EFRA) return 0;
	
	update_time();

	/* waitcursor(1); */
	G.f |= G_PLAYANIM;		/* in sequence.c and view.c this is handled */

	cfraont= CFRA;
	oldsa= curarea;

	audiostream_start( CFRA );
	
	inner_play_anim_loop(1, mode);	/* 1==init */
	
	 /* forces all buffers to be OK for current frame (otherwise other windows get redrawn with CFRA+1) */
	curarea->win_swap= WIN_BACK_OK;
	screen_swapbuffers();
	
	while(TRUE) {

		while(qtest()) {
			
			/* we test events first because of MKEY event */
			
			event= extern_qread(&val);
			if(event==ESCKEY) break;
			else if(event==MIDDLEMOUSE) {
				if(U.flag & USER_VIEWMOVE) {
					if(G.qual & LR_SHIFTKEY) viewmove(0);
					else if(G.qual & LR_CTRLKEY) viewmove(2);
					else viewmove(1);
				}
				else {
					if(G.qual & LR_SHIFTKEY) viewmove(1);
					else if(G.qual & LR_CTRLKEY) viewmove(2);
					else viewmove(0);
				}
			}
			else if(event==MKEY) {
				if(val) add_timeline_marker(CFRA-1);
			}
		}
		if(ELEM3(event, ESCKEY, SPACEKEY, RIGHTMOUSE)) break;
		
		inner_play_anim_loop(0, 0);
		screen_swapbuffers();
				
		if((mode > 1) && CFRA==EFRA) break; /* no replay */	
	}

	if(event==SPACEKEY);
	else CFRA= cfraont;

	clear_all_constraints();
	do_all_ipos();
	do_all_keys();
	do_all_actions(NULL);

	if(G.vd) {
		/* set all objects on current frame... test_all_displists() needs it */
		base= G.scene->base.first;
		while(base) {
			if(G.vd->lay & base->lay) where_is_object(base->object);
			base= base->next;
		}
		test_all_displists();
	}
	
	audiostream_stop();

	if(oldsa!=curarea) areawinset(oldsa->win);
	
	/* restore all areas */
	sa= G.curscreen->areabase.first;
	while(sa) {
		if( (mode && sa->spacetype==SPACE_VIEW3D) || sa==curarea) addqueue(sa->win, REDRAW, 1);
		
		sa= sa->next;	
	}
	
	/* groups could have changed ipo */
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWACTION, 0);
	/* for the time being */
	update_for_newframe_muted();

	waitcursor(0);
	G.f &= ~G_PLAYANIM;
	
	if (event==ESCKEY || event==SPACEKEY) return 1;
	else return 0;
}
