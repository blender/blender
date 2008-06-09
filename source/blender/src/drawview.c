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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

//#define NAN_LINEAR_PHYSICS

#include <math.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/times.h>
#else
#include <io.h>
#endif   

#ifdef WIN32
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BMF_Api.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_anim.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_sculpt.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BIF_butspace.h"
#include "BIF_drawimage.h"
#include "BIF_editgroup.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"
#include "BIF_editparticle.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_mywindow.h"
#include "BIF_poseobject.h"
#include "BIF_previewrender.h"
#include "BIF_radialcontrol.h"
#include "BIF_resources.h"
#include "BIF_retopo.h"
#include "BIF_screen.h"
#include "BIF_space.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif

#include "BDR_drawmesh.h"
#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_vpaint.h"
#include "BDR_sculptmode.h"

#include "BSE_drawview.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_seqaudio.h"
#include "BSE_sequence.h"
#include "BSE_trans_types.h"
#include "BSE_time.h"
#include "BSE_view.h"

#include "BPY_extern.h"

#include "RE_render_ext.h"

#include "blendef.h"
#include "mydevice.h"
#include "butspace.h"  // event codes

#include "BIF_transform.h"

#include "RE_pipeline.h"	// make_stars

#include "multires.h"

/* For MULTISAMPLE_ARB #define.
   Note that older systems like irix 
   may not have this, and will need a #ifdef
   to disable it.*/
/* #include "GL/glext.h" Disabled for release, to avoid possibly breaking platforms.
   Instead, the define we need will just be #defined if it's not in the platform opengl.h.
*/

/* Modules used */
#include "radio.h"

/* locals */
void drawname(Object *ob);

static void star_stuff_init_func(void)
{
	cpack(-1);
	glPointSize(1.0);
	glBegin(GL_POINTS);
}
static void star_stuff_vertex_func(float* i)
{
	glVertex3fv(i);
}
static void star_stuff_term_func(void)
{
	glEnd();
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
	float mat_ambient[] = { 0.0, 0.0, 0.0, 0.0 };
	float mat_specular[] = { 0.5, 0.5, 0.5, 1.0 };
	float mat_shininess[] = { 35.0 };
	int a, x, y;
	GLubyte pat[32*32];
	const GLubyte *patc= pat;
	
	
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
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

	/* default on, disable/enable should be local per function */
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	
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
	ImBuf *ibuf= NULL;
	float vec[4], fac, asp, zoomx, zoomy;
	float x1, y1, x2, y2, cx, cy;
	
	bgpic= G.vd->bgpic;
	if(bgpic==NULL) return;
	
	ima= bgpic->ima;
	
	if(ima)
		ibuf= BKE_image_get_ibuf(ima, &bgpic->iuser);
	if(ibuf==NULL || (ibuf->rect==NULL && ibuf->rect_float==NULL) ) 
		return;
	if(ibuf->channels!=4)
		return;
	if(ibuf->rect==NULL)
		IMB_rect_from_float(ibuf);
	
	if(G.vd->persp==2) {
		rctf vb;

		calc_viewborder(G.vd, &vb);

		x1= vb.xmin;
		y1= vb.ymin;
		x2= vb.xmax;
		y2= vb.ymax;
	}
	else {
		float sco[2];

		/* calc window coord */
		initgrabz(0.0, 0.0, 0.0);
		window_to_3d(vec, 1, 0);
		fac= MAX3( fabs(vec[0]), fabs(vec[1]), fabs(vec[1]) );
		fac= 1.0/fac;
	
		asp= ( (float)ibuf->y)/(float)ibuf->x;

		vec[0] = vec[1] = vec[2] = 0.0;
		view3d_project_float(curarea, vec, sco, G.vd->persmat);
		cx = sco[0];
		cy = sco[1];
	
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
	
	zoomx= (x2-x1)/ibuf->x;
	zoomy= (y2-y1)/ibuf->y;
	
	/* for some reason; zoomlevels down refuses to use GL_ALPHA_SCALE */
	if(zoomx < 1.0f || zoomy < 1.0f) {
		float tzoom= MIN2(zoomx, zoomy);
		int mip= 0;
		
		if(ibuf->mipmap[0]==NULL)
			IMB_makemipmap(ibuf, 0);
		
		while(tzoom < 1.0f && mip<8 && ibuf->mipmap[mip]) {
			tzoom*= 2.0f;
			zoomx*= 2.0f;
			zoomy*= 2.0f;
			mip++;
		}
		if(mip>0)
			ibuf= ibuf->mipmap[mip-1];
	}
	
	if(G.vd->zbuf) glDisable(GL_DEPTH_TEST);

	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); 
	 
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	
	glaDefine2DArea(&curarea->winrct);

	glEnable(GL_BLEND);

	glPixelZoom(zoomx, zoomy);
	glColor4f(1.0, 1.0, 1.0, 1.0-bgpic->blend);
	glaDrawPixelsTex(x1, y1, ibuf->x, ibuf->y, GL_UNSIGNED_BYTE, ibuf->rect);
	
	glPixelZoom(1.0, 1.0);
	glPixelTransferf(GL_ALPHA_SCALE, 1.0f);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	
	glDisable(GL_BLEND);
	if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);
	
	areawinset(curarea->win);	// restore viewport / scissor
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
	short sublines = G.vd->gridsubdiv;
	
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
		G.vd->gridview*= sublines;
		dx*= sublines;
		
		if(dx<6.0) {	
			G.vd->gridview*= sublines;
			dx*= sublines;
			
			if(dx<6.0) {
				G.vd->gridview*= sublines;
				dx*=sublines;
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
				drawgrid_draw(wx, wy, x, y, sublines*dx);
			}
		}
		else {	// start blending out (6 < dx < 60)
			BIF_ThemeColorBlend(TH_BACK, TH_GRID, dx/60.0);
			drawgrid_draw(wx, wy, x, y, dx);
			
			BIF_ThemeColor(TH_GRID);
			drawgrid_draw(wx, wy, x, y, sublines*dx);
		}
	}
	else {
		if(dx>60.0) {		// start blending in
			G.vd->gridview/= sublines;
			dx/= sublines;			
			if(dx>60.0) {		// start blending in
				G.vd->gridview/= sublines;
				dx/= sublines;
				if(dx>60.0) {
					BIF_ThemeColor(TH_GRID);
					drawgrid_draw(wx, wy, x, y, dx);
				}
				else {
					BIF_ThemeColorBlend(TH_BACK, TH_GRID, dx/60.0);
					drawgrid_draw(wx, wy, x, y, dx);
					BIF_ThemeColor(TH_GRID);
					drawgrid_draw(wx, wy, x, y, dx*sublines);
				}
			}
			else {
				BIF_ThemeColorBlend(TH_BACK, TH_GRID, dx/60.0);
				drawgrid_draw(wx, wy, x, y, dx);
				BIF_ThemeColor(TH_GRID);				
				drawgrid_draw(wx, wy, x, y, dx*sublines);
			}
		}
		else {
			BIF_ThemeColorBlend(TH_BACK, TH_GRID, dx/60.0);
			drawgrid_draw(wx, wy, x, y, dx);
			BIF_ThemeColor(TH_GRID);
			drawgrid_draw(wx, wy, x, y, dx*sublines);
		}
	}

	x+= (wx); 
	y+= (wy);
	BIF_GetThemeColor3ubv(TH_GRID, col);

	setlinestyle(0);
	
	/* center cross */
	if(G.vd->view==3) make_axis_color(col, col2, 'y');
	else make_axis_color(col, col2, 'x');
	glColor3ubv((GLubyte *)col2);
	
	fdrawline(0.0,  y,  (float)curarea->winx,  y); 
	
	if(G.vd->view==7) make_axis_color(col, col2, 'y');
	else make_axis_color(col, col2, 'z');
	glColor3ubv((GLubyte *)col2);

	fdrawline(x, 0.0, x, (float)curarea->winy); 

	glDepthMask(1);		// enable write in zbuffer
	persp(PERSP_VIEW);
}



static void drawfloor(void)
{
	View3D *vd;
	float vert[3], grid;
	int a, gridlines, emphasise;
	char col[3], col2[3];
	short draw_line = 0;
		
	vd= curarea->spacedata.first;

	vert[2]= 0.0;

	if(vd->gridlines<3) return;
	
	if(G.vd->zbuf && G.obedit) glDepthMask(0);	// for zbuffer-select
	
	gridlines= vd->gridlines/2;
	grid= gridlines*vd->grid;
	
	BIF_GetThemeColor3ubv(TH_GRID, col);
	BIF_GetThemeColor3ubv(TH_BACK, col2);
	
	/* emphasise division lines lighter instead of darker, if background is darker than grid */
	if ( ((col[0]+col[1]+col[2])/3+10) > (col2[0]+col2[1]+col2[2])/3 )
		emphasise = 20;
	else
		emphasise = -10;
	
	/* draw the Y axis and/or grid lines */
	for(a= -gridlines;a<=gridlines;a++) {
		if(a==0) {
			/* check for the 'show Y axis' preference */
			if (vd->gridflag & V3D_SHOW_Y) { 
				make_axis_color(col, col2, 'y');
				glColor3ubv((GLubyte *)col2);
				
				draw_line = 1;
			} else if (vd->gridflag & V3D_SHOW_FLOOR) {
				BIF_ThemeColorShade(TH_GRID, emphasise);
			} else {
				draw_line = 0;
			}
		} else {
			/* check for the 'show grid floor' preference */
			if (vd->gridflag & V3D_SHOW_FLOOR) {
				if( (a % 10)==0) {
					BIF_ThemeColorShade(TH_GRID, emphasise);
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
				glColor3ubv((GLubyte *)col2);
				
				draw_line = 1;
			} else if (vd->gridflag & V3D_SHOW_FLOOR) {
				BIF_ThemeColorShade(TH_GRID, emphasise);
			} else {
				draw_line = 0;
			}
		} else {
			/* check for the 'show grid floor' preference */
			if (vd->gridflag & V3D_SHOW_FLOOR) {
				if( (a % 10)==0) {
					BIF_ThemeColorShade(TH_GRID, emphasise);
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
		glColor3ubv((GLubyte *)col2);
		
		glBegin(GL_LINE_STRIP);
		vert[0]= 0;
		vert[1]= 0;
		vert[2]= grid;
		glVertex3fv(vert );
		vert[2]= -grid;
		glVertex3fv(vert);
		glEnd();
	}

	if(G.vd->zbuf && G.obedit) glDepthMask(1);	

}

static void drawcursor(View3D *v3d)
{
	short mx,my,co[2];
	int flag;
	
	/* we dont want the clipping for cursor */
	flag= v3d->flag;
	v3d->flag= 0;
	project_short( give_cursor(), co);
	v3d->flag= flag;
	
	mx = co[0];
	my = co[1];

	if(mx!=IS_CLIPPED) {
		setlinestyle(0); 
		cpack(0xFF);
		circ((float)mx, (float)my, 10.0);
		setlinestyle(4); 
		cpack(0xFFFFFF);
		circ((float)mx, (float)my, 10.0);
		setlinestyle(0);
		cpack(0x0);
		
		sdrawline(mx-20, my, mx-5, my);
		sdrawline(mx+5, my, mx+20, my);
		sdrawline(mx, my-20, mx, my-5);
		sdrawline(mx, my+5, mx, my+20);
	}
}

/* ********* custom clipping *********** */

static void view3d_draw_clipping(View3D *v3d)
{
	BoundBox *bb= v3d->clipbb;
	
	BIF_ThemeColorShade(TH_BACK, -8);
	
	glBegin(GL_QUADS);

	glVertex3fv(bb->vec[0]); glVertex3fv(bb->vec[1]); glVertex3fv(bb->vec[2]); glVertex3fv(bb->vec[3]);
	glVertex3fv(bb->vec[0]); glVertex3fv(bb->vec[4]); glVertex3fv(bb->vec[5]); glVertex3fv(bb->vec[1]);
	glVertex3fv(bb->vec[4]); glVertex3fv(bb->vec[7]); glVertex3fv(bb->vec[6]); glVertex3fv(bb->vec[5]);
	glVertex3fv(bb->vec[7]); glVertex3fv(bb->vec[3]); glVertex3fv(bb->vec[2]); glVertex3fv(bb->vec[6]);
	glVertex3fv(bb->vec[1]); glVertex3fv(bb->vec[5]); glVertex3fv(bb->vec[6]); glVertex3fv(bb->vec[2]);
	glVertex3fv(bb->vec[7]); glVertex3fv(bb->vec[4]); glVertex3fv(bb->vec[0]); glVertex3fv(bb->vec[3]);
	
	glEnd();
}

void view3d_set_clipping(View3D *v3d)
{
	double plane[4];
	int a;
	
	for(a=0; a<4; a++) {
		QUATCOPY(plane, v3d->clip[a]);
		glClipPlane(GL_CLIP_PLANE0+a, plane);
		glEnable(GL_CLIP_PLANE0+a);
	}
}

void view3d_clr_clipping(void)
{
	int a;
	
	for(a=0; a<4; a++) {
		glDisable(GL_CLIP_PLANE0+a);
	}
}

int view3d_test_clipping(View3D *v3d, float *vec)
{
	/* vec in world coordinates, returns 1 if clipped */
	float view[3];
	
	VECCOPY(view, vec);
	
	if(0.0f < v3d->clip[0][3] + INPR(view, v3d->clip[0]))
		if(0.0f < v3d->clip[1][3] + INPR(view, v3d->clip[1]))
			if(0.0f < v3d->clip[2][3] + INPR(view, v3d->clip[2]))
				if(0.0f < v3d->clip[3][3] + INPR(view, v3d->clip[3]))
					return 0;

	return 1;
}

/* ********* end custom clipping *********** */

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

void calc_viewborder(struct View3D *v3d, rctf *viewborder_r)
{
	float zoomfac, size[2];
	float dx= 0.0f, dy= 0.0f;
	
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
	
	dx= v3d->area->winx*G.vd->camdx*zoomfac*2.0f;
	dy= v3d->area->winy*G.vd->camdy*zoomfac*2.0f;
	
	/* apply offset */
	viewborder_r->xmin-= dx;
	viewborder_r->ymin-= dy;
	viewborder_r->xmax-= dx;
	viewborder_r->ymax-= dy;
	
	if(v3d->camera && v3d->camera->type==OB_CAMERA) {
		Camera *cam= v3d->camera->data;
		float w = viewborder_r->xmax - viewborder_r->xmin;
		float h = viewborder_r->ymax - viewborder_r->ymin;
		float side = MAX2(w, h);

		viewborder_r->xmin+= cam->shiftx*side;
		viewborder_r->xmax+= cam->shiftx*side;
		viewborder_r->ymin+= cam->shifty*side;
		viewborder_r->ymax+= cam->shifty*side;
	}
}

void view3d_set_1_to_1_viewborder(View3D *v3d)
{
	float size[2];
	int im_width= (G.scene->r.size*G.scene->r.xsch)/100;

	view3d_get_viewborder_size(v3d, size);

	v3d->camzoom= (sqrt(4.0*im_width/size[0]) - M_SQRT2)*50.0;
	v3d->camzoom= CLAMPIS(v3d->camzoom, -30, 300);
}


static void drawviewborder_flymode(void)	
{
	/* draws 4 edge brackets that frame the safe area where the
	mouse can move during fly mode without spinning the view */
	float x1, x2, y1, y2;
	
	x1= 0.45*(float)curarea->winx;
	y1= 0.45*(float)curarea->winy;
	x2= 0.55*(float)curarea->winx;
	y2= 0.55*(float)curarea->winy;
	cpack(0);
	
	
	glBegin(GL_LINES);
	/* bottom left */
	glVertex2f(x1,y1); 
	glVertex2f(x1,y1+5);
	
	glVertex2f(x1,y1); 
	glVertex2f(x1+5,y1);

	/* top right */
	glVertex2f(x2,y2); 
	glVertex2f(x2,y2-5);
	
	glVertex2f(x2,y2); 
	glVertex2f(x2-5,y2);
	
	/* top left */
	glVertex2f(x1,y2); 
	glVertex2f(x1,y2-5);
	
	glVertex2f(x1,y2); 
	glVertex2f(x1+5,y2);
	
	/* bottom right */
	glVertex2f(x2,y1); 
	glVertex2f(x2,y1+5);
	
	glVertex2f(x2,y1); 
	glVertex2f(x2-5,y1);
	glEnd();	
}


static void drawviewborder(void)
{
	extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);          // interface_panel.c
	float fac, a;
	float x1, x2, y1, y2;
	float x3, y3, x4, y4;
	rctf viewborder;
	Camera *ca= NULL;

	if(G.vd->camera==NULL)
		return;
	if(G.vd->camera->type==OB_CAMERA)
		ca = G.vd->camera->data;
	
	calc_viewborder(G.vd, &viewborder);
	x1= viewborder.xmin;
	y1= viewborder.ymin;
	x2= viewborder.xmax;
	y2= viewborder.ymax;

	/* passepartout, specified in camera edit buttons */
	if (ca && (ca->flag & CAM_SHOWPASSEPARTOUT) && ca->passepartalpha > 0.000001) {
		if (ca->passepartalpha == 1.0) {
			glColor3f(0, 0, 0);
		} else {
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glEnable(GL_BLEND);
			glColor4f(0, 0, 0, ca->passepartalpha);
		}
		if (x1 > 0.0)
			glRectf(0.0, (float)curarea->winy, x1, 0.0);
		if (x2 < (float)curarea->winx)
			glRectf(x2, (float)curarea->winy, (float)curarea->winx, 0.0);
		if (y2 < (float)curarea->winy)
			glRectf(x1, (float)curarea->winy, x2, y2);
		if (y2 > 0.0) 
			glRectf(x1, y1, x2, 0.0);

		glDisable(GL_BLEND);
	}
	
	/* edge */
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); 
	
	setlinestyle(0);
	BIF_ThemeColor(TH_BACK);
	glRectf(x1, y1, x2, y2);
	
	setlinestyle(3);
	BIF_ThemeColor(TH_WIRE);
	glRectf(x1, y1, x2, y2);
		
	/* camera name - draw in highlighted text color */
	if (ca && (ca->flag & CAM_SHOWNAME)) {
		BIF_ThemeColor(TH_TEXT_HI);
		glRasterPos2f(x1, y1-15);
		
		BMF_DrawString(G.font, G.vd->camera->id.name+2);
		BIF_ThemeColor(TH_WIRE);
	}


	/* border */
	if(G.scene->r.mode & R_BORDER) {
		
		cpack(0);
		x3= x1+ G.scene->r.border.xmin*(x2-x1);
		y3= y1+ G.scene->r.border.ymin*(y2-y1);
		x4= x1+ G.scene->r.border.xmax*(x2-x1);
		y4= y1+ G.scene->r.border.ymax*(y2-y1);
		
		cpack(0x4040FF);
		glRectf(x3,  y3,  x4,  y4); 
	}

	/* safety border */
	if (ca && (ca->flag & CAM_SHOWTITLESAFE)) {
		fac= 0.1;
		
		a= fac*(x2-x1);
		x1+= a; 
		x2-= a;
	
		a= fac*(y2-y1);
		y1+= a;
		y2-= a;
	
		BIF_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);
		
		uiSetRoundBox(15);
		gl_round_box(GL_LINE_LOOP, x1, y1, x2, y2, 12.0);
	}
	
	setlinestyle(0);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

}

void backdrawview3d(int test)
{
	struct Base *base;

/*for 2.43 release, don't use glext and just define the constant.
  this to avoid possibly breaking platforms before release.*/
#ifndef GL_MULTISAMPLE_ARB
	#define GL_MULTISAMPLE_ARB	0x809D
#endif

#ifdef GL_MULTISAMPLE_ARB
	int m;
#endif

	if(G.f & G_VERTEXPAINT || G.f & G_WEIGHTPAINT || G.f & G_TEXTUREPAINT);
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

	/*Disable FSAA for backbuffer selection.  
	
	Only works if GL_MULTISAMPLE_ARB is defined by the header
	file, which is should be for every OS that supports FSAA.*/

#ifdef GL_MULTISAMPLE_ARB
	m = glIsEnabled(GL_MULTISAMPLE_ARB);
	if (m) glDisable(GL_MULTISAMPLE_ARB);
#endif

#ifdef __APPLE__
	glDrawBuffer(GL_AUX0);
#endif	
	if(G.vd->drawtype > OB_WIRE) G.vd->zbuf= TRUE;
	curarea->win_swap &= ~WIN_BACK_OK;
	
	glDisable(GL_DITHER);

	glClearColor(0.0, 0.0, 0.0, 0.0); 
	if(G.vd->zbuf) {
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else {
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
	}
	
	if(G.vd->flag & V3D_CLIPPING)
		view3d_set_clipping(G.vd);
	
	G.f |= G_BACKBUFSEL;
	
	base= (G.scene->basact);
	if(base && (base->lay & G.vd->lay)) {
		draw_object_backbufsel(base->object);
	}

	G.vd->flag &= ~V3D_NEEDBACKBUFDRAW;

	G.f &= ~G_BACKBUFSEL;
	G.vd->zbuf= FALSE; 
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_DITHER);

#ifdef __APPLE__
	glDrawBuffer(GL_BACK); /* we were in aux buffers */
#endif

	if(G.vd->flag & V3D_CLIPPING)
		view3d_clr_clipping();

#ifdef GL_MULTISAMPLE_ARB
	if (m) glEnable(GL_MULTISAMPLE_ARB);
#endif

	/* it is important to end a view in a transform compatible with buttons */
	persp(PERSP_WIN);  // set ortho
	bwin_scalematrix(curarea->win, G.vd->blockscale, G.vd->blockscale, G.vd->blockscale);

}

void check_backbuf(void)
{
	if(G.vd->flag & V3D_NEEDBACKBUFDRAW)
		backdrawview3d(0);
}

/* samples a single pixel (copied from vpaint) */
unsigned int sample_backbuf(int x, int y)
{
	unsigned int col;
	
	if(x>=curarea->winx || y>=curarea->winy) return 0;
	x+= curarea->winrct.xmin;
	y+= curarea->winrct.ymin;
	
	check_backbuf(); // actually not needed for apple

#ifdef __APPLE__
	glReadBuffer(GL_AUX0);
#endif
	glReadPixels(x,  y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,  &col);
	glReadBuffer(GL_BACK);	
	
	if(G.order==B_ENDIAN) SWITCH_INT(col);
	
	return framebuffer_to_index(col);
}

/* reads full rect, converts indices */
ImBuf *read_backbuf(short xmin, short ymin, short xmax, short ymax)
{
	unsigned int *dr, *rd;
	struct ImBuf *ibuf, *ibuf1;
	int a;
	short xminc, yminc, xmaxc, ymaxc, xs, ys;
	
	/* clip */
	if(xmin<0) xminc= 0; else xminc= xmin;
	if(xmax>=curarea->winx) xmaxc= curarea->winx-1; else xmaxc= xmax;
	if(xminc > xmaxc) return NULL;

	if(ymin<0) yminc= 0; else yminc= ymin;
	if(ymax>=curarea->winy) ymaxc= curarea->winy-1; else ymaxc= ymax;
	if(yminc > ymaxc) return NULL;
	
	ibuf= IMB_allocImBuf((xmaxc-xminc+1), (ymaxc-yminc+1), 32, IB_rect,0);

	check_backbuf(); // actually not needed for apple
	
#ifdef __APPLE__
	glReadBuffer(GL_AUX0);
#endif
	glReadPixels(curarea->winrct.xmin+xminc, curarea->winrct.ymin+yminc, (xmaxc-xminc+1), (ymaxc-yminc+1), GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
	glReadBuffer(GL_BACK);	

	if(G.order==B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);

	a= (xmaxc-xminc+1)*(ymaxc-yminc+1);
	dr= ibuf->rect;
	while(a--) {
		if(*dr) *dr= framebuffer_to_index(*dr);
		dr++;
	}
	
	/* put clipped result back, if needed */
	if(xminc==xmin && xmaxc==xmax && yminc==ymin && ymaxc==ymax) 
		return ibuf;
	
	ibuf1= IMB_allocImBuf( (xmax-xmin+1),(ymax-ymin+1),32,IB_rect,0);
	rd= ibuf->rect;
	dr= ibuf1->rect;
		
	for(ys= ymin; ys<=ymax; ys++) {
		for(xs= xmin; xs<=xmax; xs++, dr++) {
			if( xs>=xminc && xs<=xmaxc && ys>=yminc && ys<=ymaxc) {
				*dr= *rd;
				rd++;
			}
		}
	}
	IMB_freeImBuf(ibuf);
	return ibuf1;
}

/* smart function to sample a rect spiralling outside, nice for backbuf selection */
unsigned int sample_backbuf_rect(short mval[2], int size, unsigned int min, unsigned int max, int *dist, short strict, unsigned int (*indextest)(unsigned int index))
{
	struct ImBuf *buf;
	unsigned int *bufmin, *bufmax, *tbuf;
	int minx, miny;
	int a, b, rc, nr, amount, dirvec[4][2];
	int distance=0;
	unsigned int index = 0;
	short indexok = 0;	

	amount= (size-1)/2;

	minx = mval[0]-(amount+1);
	miny = mval[1]-(amount+1);
	buf = read_backbuf(minx, miny, minx+size-1, miny+size-1);
	if (!buf) return 0;

	rc= 0;
	
	dirvec[0][0]= 1; dirvec[0][1]= 0;
	dirvec[1][0]= 0; dirvec[1][1]= -size;
	dirvec[2][0]= -1; dirvec[2][1]= 0;
	dirvec[3][0]= 0; dirvec[3][1]= size;
	
	bufmin = buf->rect;
	tbuf = buf->rect;
	bufmax = buf->rect + size*size;
	tbuf+= amount*size+ amount;
	
	for(nr=1; nr<=size; nr++) {
		
		for(a=0; a<2; a++) {
			for(b=0; b<nr; b++, distance++) {
				if (*tbuf && *tbuf>=min && *tbuf<max) { //we got a hit
					if(strict){
						indexok =  indextest(*tbuf - min+1);
						if(indexok){
							*dist= (short) sqrt( (float)distance   );
							index = *tbuf - min+1;
							goto exit; 
						}						
					}
					else{
						*dist= (short) sqrt( (float)distance ); // XXX, this distance is wrong - 
						index = *tbuf - min+1; // messy yah, but indices start at 1
						goto exit;
					}			
				}
				
				tbuf+= (dirvec[rc][0]+dirvec[rc][1]);
				
				if(tbuf<bufmin || tbuf>=bufmax) {
					goto exit;
				}
			}
			rc++;
			rc &= 3;
		}
	}

exit:
	IMB_freeImBuf(buf);
	return index;
}

void drawname(Object *ob)
{
	cpack(0x404040);
	glRasterPos3f(0.0,  0.0,  0.0);
	
	BMF_DrawString(G.font, " ");
	BMF_DrawString(G.font, ob->id.name+2);
}


static void draw_selected_name(Object *ob)
{
	char info[128];
	short offset=30;

	if(ob->type==OB_ARMATURE) {
		bArmature *arm= ob->data;
		char *name= NULL;
		
		if(ob==G.obedit) {
			EditBone *ebo;
			for (ebo=G.edbo.first; ebo; ebo=ebo->next){
				if ((ebo->flag & BONE_ACTIVE) && (ebo->layer & arm->layer)) {
					name= ebo->name;
					break;
				}
			}
		}
		else if(ob->pose && (ob->flag & OB_POSEMODE)) {
			bPoseChannel *pchan;
			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				if((pchan->bone->flag & BONE_ACTIVE) && (pchan->bone->layer & arm->layer)) {
					name= pchan->name;
					break;
				}
			}
		}
		if(name)
			sprintf(info, "(%d) %s %s", CFRA, ob->id.name+2, name);
		else
			sprintf(info, "(%d) %s", CFRA, ob->id.name+2);
	}
	else if(ob->type==OB_MESH) {
		Key *key= NULL;
		KeyBlock *kb = NULL;
		char shapes[75];
		
		shapes[0] = 0;
		key = ob_get_key(ob);
		if(key){
			kb = BLI_findlink(&key->block, ob->shapenr-1);
			if(kb){
				sprintf(shapes, ": %s ", kb->name);		
				if(ob->shapeflag == OB_SHAPE_LOCK){
					sprintf(shapes, "%s (Pinned)",shapes);
				}
			}
		}
		sprintf(info, "(%d) %s %s", CFRA, ob->id.name+2, shapes);
	}
	else sprintf(info, "(%d) %s", CFRA, ob->id.name+2);

	BIF_ThemeColor(TH_TEXT_HI);
	if (U.uiflag & USER_SHOW_ROTVIEWICON)
		offset = 14 + (U.rvisize * 2);

	glRasterPos2i(offset,  10);
	BMF_DrawString(G.fonts, info);
}


/* Draw a live substitute of the view icon, which is always shown */
static void draw_view_axis(void)
{
	const float k = U.rvisize;   /* axis size */
	const float toll = 0.5;      /* used to see when view is quasi-orthogonal */
	const float start = k + 1.0; /* axis center in screen coordinates, x=y */
	float ydisp = 0.0;          /* vertical displacement to allow obj info text */
	
	/* rvibright ranges approx. from original axis icon color to gizmo color */
	float bright = U.rvibright / 15.0f;
	
	unsigned char col[3];
	unsigned char gridcol[3];
	float colf[3];
	
	float vec[4];
	float dx, dy;
	float h, s, v;
	
	/* thickness of lines is proportional to k */
	/*	(log(k)-1) gives a more suitable thickness, but fps decreased by about 3 fps */
	glLineWidth(k / 10);
	//glLineWidth(log(k)-1); // a bit slow
	
	BIF_GetThemeColor3ubv(TH_GRID, (char *)gridcol);
	
	/* X */
	vec[0] = vec[3] = 1;
	vec[1] = vec[2] = 0;
	QuatMulVecf(G.vd->viewquat, vec);
	
	make_axis_color((char *)gridcol, (char *)col, 'x');
	rgb_to_hsv(col[0]/255.0f, col[1]/255.0f, col[2]/255.0f, &h, &s, &v);
	s = s<0.5 ? s+0.5 : 1.0;
	v = 0.3;
	v = (v<1.0-(bright) ? v+bright : 1.0);
	hsv_to_rgb(h, s, v, colf, colf+1, colf+2);
	glColor3fv(colf);
	
	dx = vec[0] * k;
	dy = vec[1] * k;
	fdrawline(start, start + ydisp, start + dx, start + dy + ydisp);
	if (fabs(dx) > toll || fabs(dy) > toll) {
		glRasterPos2i(start + dx + 2, start + dy + ydisp + 2);
		BMF_DrawString(G.fonts, "x");
	}
	
	/* Y */
	vec[1] = vec[3] = 1;
	vec[0] = vec[2] = 0;
	QuatMulVecf(G.vd->viewquat, vec);
	
	make_axis_color((char *)gridcol, (char *)col, 'y');
	rgb_to_hsv(col[0]/255.0f, col[1]/255.0f, col[2]/255.0f, &h, &s, &v);
	s = s<0.5 ? s+0.5 : 1.0;
	v = 0.3;
	v = (v<1.0-(bright) ? v+bright : 1.0);
	hsv_to_rgb(h, s, v, colf, colf+1, colf+2);
	glColor3fv(colf);
	
	dx = vec[0] * k;
	dy = vec[1] * k;
	fdrawline(start, start + ydisp, start + dx, start + dy + ydisp);
	if (fabs(dx) > toll || fabs(dy) > toll) {
		glRasterPos2i(start + dx + 2, start + dy + ydisp + 2);
		BMF_DrawString(G.fonts, "y");
	}
	
	/* Z */
	vec[2] = vec[3] = 1;
	vec[1] = vec[0] = 0;
	QuatMulVecf(G.vd->viewquat, vec);
	
	make_axis_color((char *)gridcol, (char *)col, 'z');
	rgb_to_hsv(col[0]/255.0f, col[1]/255.0f, col[2]/255.0f, &h, &s, &v);
	s = s<0.5 ? s+0.5 : 1.0;
	v = 0.5;
	v = (v<1.0-(bright) ? v+bright : 1.0);
	hsv_to_rgb(h, s, v, colf, colf+1, colf+2);
	glColor3fv(colf);
	
	dx = vec[0] * k;
	dy = vec[1] * k;
	fdrawline(start, start + ydisp, start + dx, start + dy + ydisp);
	if (fabs(dx) > toll || fabs(dy) > toll) {
		glRasterPos2i(start + dx + 2, start + dy + ydisp + 2);
		BMF_DrawString(G.fonts, "z");
	}
	
	/* restore line-width */
	glLineWidth(1.0);
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
	
	BIF_icon_draw(5.0, 5.0, icon);
	
	glDisable(GL_BLEND);
}

static void draw_viewport_name(ScrArea *sa)
{
	char *name = NULL;
	char *printable = NULL;
	
	switch(G.vd->view) {
		case 1:
			if (G.vd->persp == V3D_ORTHO)
				name = (G.vd->flag2 & V3D_OPP_DIRECTION_NAME) ? "Back Ortho" : "Front Ortho";
			else
				name = (G.vd->flag2 & V3D_OPP_DIRECTION_NAME) ? "Back Persp" : "Front Persp";
			break;
		case 3:
			if (G.vd->persp == V3D_ORTHO)
				name = (G.vd->flag2 & V3D_OPP_DIRECTION_NAME) ? "Left Ortho" : "Right Ortho";
			else
				name = (G.vd->flag2 & V3D_OPP_DIRECTION_NAME) ? "Left Persp" : "Right Persp";
			break;
		case 7:
			if (G.vd->persp == V3D_ORTHO)
				name = (G.vd->flag2 & V3D_OPP_DIRECTION_NAME) ? "Bottom Ortho" : "Top Ortho";
			else
				name = (G.vd->flag2 & V3D_OPP_DIRECTION_NAME) ? "Bottom Persp" : "Top Persp";
			break;
		default:
			if (G.vd->persp==V3D_CAMOB) {
				if ((G.vd->camera) && (G.vd->camera->type == OB_CAMERA)) {
					Camera *cam;
					cam = G.vd->camera->data;
					name = (cam->type != CAM_ORTHO) ? "Camera Persp" : "Camera Ortho";
				} else {
					name = "Object as Camera";
				}
			} else { 
				name = (G.vd->persp == V3D_ORTHO) ? "User Ortho" : "User Persp";
			}
	}
	
	if (G.vd->localview) {
		printable = malloc(strlen(name) + strlen(" (Local)_")); /* '_' gives space for '\0' */
		strcpy(printable, name);
		strcat(printable, " (Local)");
	} else {
		printable = name;
	}

	if (printable) {
		BIF_ThemeColor(TH_TEXT_HI);
		glRasterPos2i(10,  sa->winy-20);
		BMF_DrawString(G.fonts, printable);
	}

	if (G.vd->localview) {
		free(printable);
	}
}

/* ******************* view3d space & buttons ************** */


/* temporal struct for storing transform properties */
typedef struct {
	float ob_eul[4];	// used for quat too....
	float ob_scale[3]; // need temp space due to linked values
	float ob_dims[3];
	short link_scale;
	float ve_median[5];
	int curdef;
	float *defweightp;
} TransformProperties;

/* is used for both read and write... */
static void v3d_editvertex_buts(uiBlock *block, Object *ob, float lim)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *evedef=NULL;
	EditEdge *eed;
	MDeformVert *dvert=NULL;
	TransformProperties *tfp= G.vd->properties_storage;
	float median[5], ve_median[5];
	int tot, totw, totweight, totedge;
	char defstr[320];
	
	median[0]= median[1]= median[2]= median[3]= median[4]= 0.0;
	tot= totw= totweight= totedge= 0;
	defstr[0]= 0;

	if(ob->type==OB_MESH) {		
		eve= em->verts.first;
		while(eve) {
			if(eve->f & SELECT) {
				evedef= eve;
				tot++;
				VecAddf(median, median, eve->co);
			}
			eve= eve->next;
		}
		eed= em->edges.first;
		while(eed) {
			if((eed->f & SELECT)) {
				totedge++;
				median[3]+= eed->crease;
			}
			eed= eed->next;
		}

		/* check for defgroups */
		if(evedef)
			dvert= CustomData_em_get(&em->vdata, evedef->data, CD_MDEFORMVERT);
		if(tot==1 && dvert && dvert->totweight) {
			bDeformGroup *dg;
			int i, max=1, init=1;
			char str[320];
			
			for (i=0; i<dvert->totweight; i++){
				dg = BLI_findlink (&ob->defbase, dvert->dw[i].def_nr);
				if(dg) {
					max+= snprintf(str, sizeof(str), "%s %%x%d|", dg->name, dvert->dw[i].def_nr); 
					if(max<320) strcat(defstr, str);
				}
				else printf("oh no!\n");
				if(tfp->curdef==dvert->dw[i].def_nr) {
					init= 0;
					tfp->defweightp= &dvert->dw[i].weight;
				}
			}
			
			if(init) {	// needs new initialized 
				tfp->curdef= dvert->dw[0].def_nr;
				tfp->defweightp= &dvert->dw[0].weight;
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
			if((nu->type & 7)==CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(bezt->f2 & SELECT) {
						VecAddf(median, median, bezt->vec[1]);
						tot++;
						median[4]+= bezt->weight;
						totweight++;
					}
					else {
						if(bezt->f1 & SELECT) {
							VecAddf(median, median, bezt->vec[0]);
							tot++;
						}
						if(bezt->f3 & SELECT) {
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
					if(bp->f1 & SELECT) {
						VecAddf(median, median, bp->vec);
						median[3]+= bp->vec[3];
						totw++;
						tot++;
						median[4]+= bp->weight;
						totweight++;
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
				median[4]+= bp->weight;
				totweight++;
			}
			bp++;
		}
	}
	
	if(tot==0) return;

	median[0] /= (float)tot;
	median[1] /= (float)tot;
	median[2] /= (float)tot;
	if(totedge) median[3] /= (float)totedge;
	else if(totw) median[3] /= (float)totw;
	if(totweight) median[4] /= (float)totweight;
	
	if(G.vd->flag & V3D_GLOBAL_STATS)
		Mat4MulVecfl(ob->obmat, median);
	
	if(block) {	// buttons
		int but_y;
		if((ob->parent) && (ob->partype == PARBONE))	but_y = 135;
		else											but_y = 150;
		
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, V3D_GLOBAL_STATS, REDRAWVIEW3D, "Global",		160, but_y, 70, 19, &G.vd->flag, 0, 0, 0, 0, "Displays global values");
		uiDefButBitS(block, TOGN, V3D_GLOBAL_STATS, REDRAWVIEW3D, "Local",		230, but_y, 70, 19, &G.vd->flag, 0, 0, 0, 0, "Displays local values");
		uiBlockEndAlign(block);
		
		memcpy(tfp->ve_median, median, sizeof(tfp->ve_median));
		
		uiBlockBeginAlign(block);
		if(tot==1) {
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Vertex X:",	10, 110, 290, 19, &(tfp->ve_median[0]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Vertex Y:",	10, 90, 290, 19, &(tfp->ve_median[1]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Vertex Z:",	10, 70, 290, 19, &(tfp->ve_median[2]), -lim, lim, 10, 3, "");
			if(totw==1)
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Vertex W:",	10, 50, 290, 19, &(tfp->ve_median[3]), 0.01, 100.0, 10, 3, "");
			uiBlockEndAlign(block);
	
			if(defstr[0]) {
				uiDefBut(block, LABEL, 1, "Vertex Deform Groups",		10, 40, 290, 20, NULL, 0.0, 0.0, 0, 0, "");

				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, B_NOP, "Weight:",			10, 20, 150, 19, tfp->defweightp, 0.0f, 1.0f, 10, 3, "Weight value");
				uiDefButI(block, MENU, REDRAWVIEW3D, defstr,	160, 20, 140, 19, &tfp->curdef, 0.0, 0.0, 0, 0, "Current Vertex Group");
				uiBlockEndAlign(block);
			}
			else if(totweight)
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	10, 20, 290, 19, &(tfp->ve_median[4]), 0.0, 1.0, 10, 3, "");

		}
		else {
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median X:",	10, 110, 290, 19, &(tfp->ve_median[0]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median Y:",	10, 90, 290, 19, &(tfp->ve_median[1]), -lim, lim, 10, 3, "");
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median Z:",	10, 70, 290, 19, &(tfp->ve_median[2]), -lim, lim, 10, 3, "");
			if(totw==tot)
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median W:",	10, 50, 290, 19, &(tfp->ve_median[3]), 0.01, 100.0, 10, 3, "");
			uiBlockEndAlign(block);
			if(totweight)
				uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Weight:",	10, 20, 290, 19, &(tfp->ve_median[4]), 0.0, 1.0, 10, 3, "Weight is used for SoftBody Goal");
		}
		
		if(ob->type==OB_CURVE && (totw==0)) { /* bez curves have no w */
			uiBlockBeginAlign(block);
			uiDefBut(block, BUT,B_SETPT_AUTO,"Auto",	10, 44, 72, 19, 0, 0, 0, 0, 0, "Auto handles (Shift H)");
			uiDefBut(block, BUT,B_SETPT_VECTOR,"Vector",82, 44, 73, 19, 0, 0, 0, 0, 0, "Vector handles (V)");
			uiDefBut(block, BUT,B_SETPT_ALIGN,"Align",155, 44, 73, 19, 0, 0, 0, 0, 0, "Align handles (H Toggles)");
			uiDefBut(block, BUT,B_SETPT_FREE,"Free",	227, 44, 72, 19, 0, 0, 0, 0, 0, "Align handles (H Toggles)");
			uiBlockEndAlign(block);
		}
		
		if(totedge==1)
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Crease W:",	10, 30, 290, 19, &(tfp->ve_median[3]), 0.0, 1.0, 10, 3, "");
		else if(totedge>1)
			uiDefButF(block, NUM, B_OBJECTPANELMEDIAN, "Median Crease W:",	10, 30, 290, 19, &(tfp->ve_median[3]), 0.0, 1.0, 10, 3, "");
		
	}
	else {	// apply
		memcpy(ve_median, tfp->ve_median, sizeof(tfp->ve_median));
		
		if(G.vd->flag & V3D_GLOBAL_STATS) {
			Mat4Invert(ob->imat, ob->obmat);
			Mat4MulVecfl(ob->imat, median);
			Mat4MulVecfl(ob->imat, ve_median);
		}
		VecSubf(median, ve_median, median);
		median[3]= ve_median[3]-median[3];
		median[4]= ve_median[4]-median[4];
		
		if(ob->type==OB_MESH) {
			
			eve= em->verts.first;
			while(eve) {
				if(eve->f & SELECT) {
					VecAddf(eve->co, eve->co, median);
				}
				eve= eve->next;
			}
			
			for(eed= em->edges.first; eed; eed= eed->next) {
				if(eed->f & SELECT) {
					/* ensure the median can be set to zero or one */
					if(ve_median[3]==0.0f) eed->crease= 0.0f;
					else if(ve_median[3]==1.0f) eed->crease= 1.0f;
					else {
						eed->crease+= median[3];
						CLAMP(eed->crease, 0.0, 1.0);
					}
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
						if(bezt->f2 & SELECT) {
							VecAddf(bezt->vec[0], bezt->vec[0], median);
							VecAddf(bezt->vec[1], bezt->vec[1], median);
							VecAddf(bezt->vec[2], bezt->vec[2], median);
							bezt->weight+= median[4];
						}
						else {
							if(bezt->f1 & SELECT) {
								VecAddf(bezt->vec[0], bezt->vec[0], median);
							}
							if(bezt->f3 & SELECT) {
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
						if(bp->f1 & SELECT) {
							VecAddf(bp->vec, bp->vec, median);
							bp->vec[3]+= median[3];
							bp->weight+= median[4];
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
					VecAddf(bp->vec, bp->vec, median);
					bp->weight+= median[4];
				}
				bp++;
			}
		}
		
		BIF_undo_push("Transform properties");
	}
}

/* assumes armature active */
static void validate_bonebutton_cb(void *bonev, void *namev)
{
	Object *ob= OBACT;
	
	if(ob && ob->type==OB_ARMATURE) {
		Bone *bone= bonev;
		char oldname[32], newname[32];
		
		/* need to be on the stack */
		BLI_strncpy(newname, bone->name, 32);
		BLI_strncpy(oldname, (char *)namev, 32);
		/* restore */
		BLI_strncpy(bone->name, oldname, 32);
		
		armature_bone_rename(ob->data, oldname, newname); // editarmature.c
		allqueue(REDRAWALL, 0);
	}
}

static void v3d_posearmature_buts(uiBlock *block, Object *ob, float lim)
{
	uiBut *but;
	bArmature *arm;
	bPoseChannel *pchan;
	Bone *bone= NULL;
	TransformProperties *tfp= G.vd->properties_storage;

	arm = get_armature(OBACT);
	if (!arm || !ob->pose) return;

	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		bone = pchan->bone;
		if(bone && (bone->flag & BONE_ACTIVE) && (bone->layer & arm->layer))
			break;
	}
	if (!pchan || !bone) return;

	if((ob->parent) && (ob->partype == PARBONE))
		but= uiDefBut (block, TEX, B_DIFF, "Bone:",				160, 130, 140, 19, bone->name, 1, 31, 0, 0, "");
	else
		but= uiDefBut(block, TEX, B_DIFF, "Bone:",				160, 140, 140, 19, bone->name, 1, 31, 0, 0, "");
	uiButSetFunc(but, validate_bonebutton_cb, bone, NULL);
	
	QuatToEul(pchan->quat, tfp->ob_eul);
	tfp->ob_eul[0]*= 180.0/M_PI;
	tfp->ob_eul[1]*= 180.0/M_PI;
	tfp->ob_eul[2]*= 180.0/M_PI;
	
	uiBlockBeginAlign(block);
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_LOCX, REDRAWVIEW3D, ICON_UNLOCKED,	10,140,20,19, &(pchan->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "LocX:",	30, 140, 120, 19, pchan->loc, -lim, lim, 100, 3, "");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_LOCY, REDRAWVIEW3D, ICON_UNLOCKED,	10,120,20,19, &(pchan->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "LocY:",	30, 120, 120, 19, pchan->loc+1, -lim, lim, 100, 3, "");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_LOCZ, REDRAWVIEW3D, ICON_UNLOCKED,	10,100,20,19, &(pchan->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "LocZ:",	30, 100, 120, 19, pchan->loc+2, -lim, lim, 100, 3, "");

	uiBlockBeginAlign(block);
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTX, REDRAWVIEW3D, ICON_UNLOCKED,	10,70,20,19, &(pchan->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
	uiDefButF(block, NUM, B_ARMATUREPANEL3, "RotX:",	30, 70, 120, 19, tfp->ob_eul, -1000.0, 1000.0, 100, 3, "");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTY, REDRAWVIEW3D, ICON_UNLOCKED,	10,50,20,19, &(pchan->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
	uiDefButF(block, NUM, B_ARMATUREPANEL3, "RotY:",	30, 50, 120, 19, tfp->ob_eul+1, -1000.0, 1000.0, 100, 3, "");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTZ, REDRAWVIEW3D, ICON_UNLOCKED,	10,30,20,19, &(pchan->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
	uiDefButF(block, NUM, B_ARMATUREPANEL3, "RotZ:",	30, 30, 120, 19, tfp->ob_eul+2, -1000.0, 1000.0, 100, 3, "");
	
	uiBlockBeginAlign(block);
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_SCALEX, REDRAWVIEW3D, ICON_UNLOCKED,	160,70,20,19, &(pchan->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "ScaleX:",	180, 70, 120, 19, pchan->size, -lim, lim, 10, 3, "");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_SCALEY, REDRAWVIEW3D, ICON_UNLOCKED,	160,50,20,19, &(pchan->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "ScaleY:",	180, 50, 120, 19, pchan->size+1, -lim, lim, 10, 3, "");
	uiDefIconButBitS(block, ICONTOG, OB_LOCK_SCALEZ, REDRAWVIEW3D, ICON_UNLOCKED,	160,30,20,19, &(pchan->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
	uiDefButF(block, NUM, B_ARMATUREPANEL2, "ScaleZ:",	180, 30, 120, 19, pchan->size+2, -lim, lim, 10, 3, "");
	uiBlockEndAlign(block);
}

static void v3d_editarmature_buts(uiBlock *block, Object *ob, float lim)
{
	bArmature *arm= G.obedit->data;
	EditBone *ebone;
	uiBut *but;
	TransformProperties *tfp= G.vd->properties_storage;
	
	ebone= G.edbo.first;

	for (ebone = G.edbo.first; ebone; ebone=ebone->next){
		if ((ebone->flag & BONE_ACTIVE) && (ebone->layer & arm->layer))
			break;
	}

	if (!ebone)
		return;
	
	if((ob->parent) && (ob->partype == PARBONE))
		but= uiDefBut(block, TEX, B_DIFF, "Bone:", 160, 130, 140, 19, ebone->name, 1, 31, 0, 0, "");
	else
		but= uiDefBut(block, TEX, B_DIFF, "Bone:",			160, 150, 140, 19, ebone->name, 1, 31, 0, 0, "");
	uiButSetFunc(but, validate_editbonebutton_cb, ebone, NULL);

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "HeadX:",	10, 70, 140, 19, ebone->head, -lim, lim, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "HeadY:",	10, 50, 140, 19, ebone->head+1, -lim, lim, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "HeadZ:",	10, 30, 140, 19, ebone->head+2, -lim, lim, 10, 3, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "TailX:",	160, 70, 140, 19, ebone->tail, -lim, lim, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "TailY:",	160, 50, 140, 19, ebone->tail+1, -lim, lim, 10, 3, "");
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "TailZ:",	160, 30, 140, 19, ebone->tail+2, -lim, lim, 10, 3, "");
	uiBlockEndAlign(block);
	
	tfp->ob_eul[0]= 180.0*ebone->roll/M_PI;
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "Roll:",	10, 100, 140, 19, tfp->ob_eul, -lim, lim, 1000, 3, "");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_ARMATUREPANEL1, "TailRadius:",	10, 150, 140, 19, &ebone->rad_tail, 0, lim, 10, 3, "");
	if (ebone->parent && ebone->flag & BONE_CONNECTED )
		uiDefButF(block, NUM, B_ARMATUREPANEL1, "HeadRadius:",	10, 130, 140, 19, &ebone->parent->rad_tail, 0, lim, 10, 3, "");
	else
		uiDefButF(block, NUM, B_ARMATUREPANEL1, "HeadRadius:",	10, 130, 140, 19, &ebone->rad_head, 0, lim, 10, 3, "");
	uiBlockEndAlign(block);
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

		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_RECALCMBALL, "Radius:", 10, 120, 140, 19, &lastelem->rad, 0, lim, 100, 3, "Size of the active metaball");
		uiDefButF(block, NUM, B_RECALCMBALL, "Stiffness:", 10, 100, 140, 19, &lastelem->s, 0, 10, 100, 3, "Stiffness of the active metaball");
		uiBlockEndAlign(block);
		
		uiDefButS(block, MENU, B_RECALCMBALL, "Type%t|Ball%x0|Tube%x4|Plane%x5|Elipsoid%x6|Cube%x7", 160, 120, 140, 19, &lastelem->type, 0.0, 0.0, 0, 0, "Set active element type");
		
	}
}

void do_viewbuts(unsigned short event)
{
	BoundBox *bb;
	View3D *vd;
	Object *ob= OBACT;
	TransformProperties *tfp= G.vd->properties_storage;
	
	vd= G.vd;
	if(vd==NULL) return;

	switch(event) {
		
	case B_OBJECTPANEL:
		DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
		allqueue(REDRAWVIEW3D, 1);
		break;
		
	case B_OBJECTPANELROT:
		if(ob) {
			ob->rot[0]= M_PI*tfp->ob_eul[0]/180.0;
			ob->rot[1]= M_PI*tfp->ob_eul[1]/180.0;
			ob->rot[2]= M_PI*tfp->ob_eul[2]/180.0;
			DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
			allqueue(REDRAWVIEW3D, 1);
		}
		break;

	case B_OBJECTPANELSCALE:
		if(ob) {

			/* link scale; figure out which axis changed */
			if (tfp->link_scale) {
				float ratio, tmp, max = 0.0;
				int axis;
				
				axis = 0;
				max = fabs(tfp->ob_scale[0] - ob->size[0]);
				tmp = fabs(tfp->ob_scale[1] - ob->size[1]);
				if (tmp > max) {
					axis = 1;
					max = tmp;
				}
				tmp = fabs(tfp->ob_scale[2] - ob->size[2]);
				if (tmp > max) {
					axis = 2;
					max = tmp;
				}
			
				if (ob->size[axis] != tfp->ob_scale[axis]) {
					if (fabs(ob->size[axis]) > FLT_EPSILON) {
						ratio = tfp->ob_scale[axis] / ob->size[axis];
						ob->size[0] *= ratio;
						ob->size[1] *= ratio;
						ob->size[2] *= ratio;
					}
				}
			}
			else {
				VECCOPY(ob->size, tfp->ob_scale);
				
			}
			DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
			allqueue(REDRAWVIEW3D, 1);
		}
		break;

	case B_OBJECTPANELDIMS:
		bb= object_get_boundbox(ob);
		if(bb) {
			float old_dims[3], scale[3], ratio, len[3];
			int axis;

			Mat4ToSize(ob->obmat, scale);

			len[0] = bb->vec[4][0] - bb->vec[0][0];
			len[1] = bb->vec[2][1] - bb->vec[0][1];
			len[2] = bb->vec[1][2] - bb->vec[0][2];

			old_dims[0] = fabs(scale[0]) * len[0];
			old_dims[1] = fabs(scale[1]) * len[1];
			old_dims[2] = fabs(scale[2]) * len[2];

			/* for each axis changed */
			for (axis = 0; axis<3; axis++) {
				if (fabs(old_dims[axis] - tfp->ob_dims[axis]) > 0.0001) {
					if (old_dims[axis] > 0.0) {
						ratio = tfp->ob_dims[axis] / old_dims[axis]; 
						if (tfp->link_scale) {
							ob->size[0] *= ratio;
							ob->size[1] *= ratio;
							ob->size[2] *= ratio;
							break;
						}
						else {
							ob->size[axis] *= ratio;
						}
					}
					else {
						if (len[axis] > 0) {
							ob->size[axis] = tfp->ob_dims[axis] / len[axis];
						}
					}
				}
			}
			
			/* prevent multiple B_OBJECTPANELDIMS events to keep scaling, cycling with TAB on buttons can cause that */
			VECCOPY(tfp->ob_dims, old_dims);
			
			DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
			allqueue(REDRAWVIEW3D, 1);
		}
		break;
	
	case B_OBJECTPANELMEDIAN:
		if(ob) {
			v3d_editvertex_buts(NULL, ob, 1.0);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 1);
		}
		break;
		
		/* note; this case also used for parbone */
	case B_OBJECTPANELPARENT:
		if(ob) {
			if(ob->id.lib || test_parent_loop(ob->parent, ob) ) 
				ob->parent= NULL;
			else {
				DAG_scene_sort(G.scene);
				DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
			}
			allqueue(REDRAWVIEW3D, 1);
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWOOPS, 0);
		}
		break;
		
	case B_ARMATUREPANEL1:
		{
			bArmature *arm= G.obedit->data;
			EditBone *ebone, *child;
			
			for (ebone = G.edbo.first; ebone; ebone=ebone->next){
				if ((ebone->flag & BONE_ACTIVE) && (ebone->layer & arm->layer))
					break;
			}
			if (ebone) {
				ebone->roll= M_PI*tfp->ob_eul[0]/180.0;
				//	Update our parent
				if (ebone->parent && ebone->flag & BONE_CONNECTED){
					VECCOPY (ebone->parent->tail, ebone->head);
				}
			
				//	Update our children if necessary
				for (child = G.edbo.first; child; child=child->next){
					if (child->parent == ebone && (child->flag & BONE_CONNECTED)){
						VECCOPY (child->head, ebone->tail);
					}
				}
				if(arm->flag & ARM_MIRROR_EDIT) {
					EditBone *eboflip= armature_bone_get_mirrored(ebone);
					if(eboflip) {
						eboflip->roll= -ebone->roll;
						eboflip->head[0]= -ebone->head[0];
						eboflip->tail[0]= -ebone->tail[0];
						
						//	Update our parent
						if (eboflip->parent && eboflip->flag & BONE_CONNECTED){
							VECCOPY (eboflip->parent->tail, eboflip->head);
						}
						
						//	Update our children if necessary
						for (child = G.edbo.first; child; child=child->next){
							if (child->parent == eboflip && (child->flag & BONE_CONNECTED)){
								VECCOPY (child->head, eboflip->tail);
							}
						}
					}
				}
				
				allqueue(REDRAWVIEW3D, 1);
			}
		}
		break;
	case B_ARMATUREPANEL3:  // rotate button on channel
		{
			bArmature *arm;
			bPoseChannel *pchan;
			Bone *bone;
			float eul[3];
			
			arm = get_armature(OBACT);
			if (!arm || !ob->pose) return;
				
			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				bone = pchan->bone;
				if(bone && (bone->flag & BONE_ACTIVE) && (bone->layer & arm->layer))
					break;
			}
			if (!pchan) return;
			
			/* make a copy to eul[3], to allow TAB on buttons to work */
			eul[0]= M_PI*tfp->ob_eul[0]/180.0;
			eul[1]= M_PI*tfp->ob_eul[1]/180.0;
			eul[2]= M_PI*tfp->ob_eul[2]/180.0;
			EulToQuat(eul, pchan->quat);
		}
		/* no break, pass on */
	case B_ARMATUREPANEL2:
		{
			ob->pose->flag |= (POSE_LOCKED|POSE_DO_UNLOCK);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 1);
		}
		break;
	case B_TRANSFORMSPACEADD:
		BIF_manageTransformOrientation(1, 0);
		allqueue(REDRAWVIEW3D, 1);
		break;
	case B_TRANSFORMSPACECLEAR:
		BIF_clearTransformOrientation();
		allqueue(REDRAWVIEW3D, 1);
	}
}

void removeTransformOrientation_func(void *target, void *unused)
{
	BIF_removeTransformOrientation((TransformOrientation *) target);
}

void selectTransformOrientation_func(void *target, void *unused)
{
	BIF_selectTransformOrientation((TransformOrientation *) target);
}

static void view3d_panel_transform_spaces(short cntrl)
{
	ListBase *transform_spaces = &G.scene->transform_spaces;
	TransformOrientation *ts = transform_spaces->first;
	uiBlock *block;
	uiBut *but;
	int xco = 20, yco = 70, height = 140;
	int index;

	block= uiNewBlock(&curarea->uiblocks, "view3d_panel_transform", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE  | cntrl);
	uiSetPanelHandler(VIEW3D_HANDLER_TRANSFORM);  // for close and esc

	if(uiNewPanel(curarea, block, "Transform Orientations", "View3d", 10, 230, 318, height)==0) return;

	uiNewPanelHeight(block, height);

	uiBlockBeginAlign(block);
	
	if (G.obedit)
		uiDefBut(block, BUT, B_TRANSFORMSPACEADD, "Add", xco,120,80,20, 0, 0, 0, 0, 0, "Add the selected element as a Transform Orientation");
	else
		uiDefBut(block, BUT, B_TRANSFORMSPACEADD, "Add", xco,120,80,20, 0, 0, 0, 0, 0, "Add the active object as a Transform Orientation");

	uiDefBut(block, BUT, B_TRANSFORMSPACECLEAR, "Clear", xco + 80,120,80,20, 0, 0, 0, 0, 0, "Removal all Transform Orientations");
	
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	
	uiDefButS(block, ROW, REDRAWHEADERS, "Global",	xco, 		90, 40,20, &G.vd->twmode, 5.0, (float)V3D_MANIP_GLOBAL,0, 0, "Global Transform Orientation");
	uiDefButS(block, ROW, REDRAWHEADERS, "Local",	xco + 40,	90, 40,20, &G.vd->twmode, 5.0, (float)V3D_MANIP_LOCAL, 0, 0, "Local Transform Orientation");
	uiDefButS(block, ROW, REDRAWHEADERS, "Normal",	xco + 80,	90, 40,20, &G.vd->twmode, 5.0, (float)V3D_MANIP_NORMAL,0, 0, "Normal Transform Orientation");
	uiDefButS(block, ROW, REDRAWHEADERS, "View",		xco + 120,	90, 40,20, &G.vd->twmode, 5.0, (float)V3D_MANIP_VIEW,	0, 0, "View Transform Orientation");
	
	for (index = V3D_MANIP_CUSTOM, ts = transform_spaces->first ; ts ; ts = ts->next, index++) {

		BIF_ThemeColor(TH_BUT_ACTION);
		if (G.vd->twmode == index) {
			but = uiDefIconButS(block,ROW, REDRAWHEADERS, ICON_CHECKBOX_HLT, xco,yco,XIC,YIC, &G.vd->twmode, 5.0, (float)index, 0, 0, "Use this Custom Transform Orientation");
		}
		else {
			but = uiDefIconButS(block,ROW, REDRAWHEADERS, ICON_CHECKBOX_DEHLT, xco,yco,XIC,YIC, &G.vd->twmode, 5.0, (float)index, 0, 0, "Use this Custom Transform Orientation");
		}
		uiButSetFunc(but, selectTransformOrientation_func, ts, NULL);
		uiDefBut(block, TEX, 0, "", xco+=XIC, yco,100+XIC,20, &ts->name, 0, 30, 0, 0, "Edits the name of this Transform Orientation");
		but = uiDefIconBut(block, BUT, REDRAWVIEW3D, ICON_X, xco+=100+XIC,yco,XIC,YIC, 0, 0, 0, 0, 0, "Deletes this Transform Orientation");
		uiButSetFunc(but, removeTransformOrientation_func, ts, NULL);

		xco = 20;
		yco -= 25;
	}
	uiBlockEndAlign(block);
	
	if(yco < 0) uiNewPanelHeight(block, height-yco);
}


static void view3d_panel_object(short cntrl)	// VIEW3D_HANDLER_OBJECT
{
	uiBlock *block;
	uiBut *bt;
	Object *ob= OBACT;
	TransformProperties *tfp;
	float lim;
	static char hexcol[128];
	
	if(ob==NULL) return;

	/* make sure we got storage */
	if(G.vd->properties_storage==NULL)
		G.vd->properties_storage= MEM_callocN(sizeof(TransformProperties), "TransformProperties");
	tfp= G.vd->properties_storage;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_panel_object", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(VIEW3D_HANDLER_OBJECT);  // for close and esc

	if((G.f & G_SCULPTMODE) && !G.obedit) {
		if(!uiNewPanel(curarea, block, "Transform Properties", "View3d", 10, 230, 318, 234))
			return;
	} else if(G.f & G_PARTICLEEDIT && !G.obedit){
		if(!uiNewPanel(curarea, block, "Transform Properties", "View3d", 10, 230, 318, 234))
			return;
	} else {
		if(!uiNewPanel(curarea, block, "Transform Properties", "View3d", 10, 230, 318, 204))
			return;
	}

	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	if(G.f & (G_VERTEXPAINT|G_TEXTUREPAINT|G_WEIGHTPAINT)) {
		uiBlockSetFlag(block, UI_BLOCK_FRONTBUFFER);	// force old style frontbuffer draw
	}
	else {
		bt= uiDefBut(block, TEX, B_IDNAME, "OB: ",	10,180,140,20, ob->id.name+2, 0.0, 21.0, 0, 0, "");
#ifdef WITH_VERSE
		if(ob->vnode) uiButSetFunc(bt, test_and_send_idbutton_cb, ob, ob->id.name);
		else uiButSetFunc(bt, test_idbutton_cb, ob->id.name, NULL);
#else
		uiButSetFunc(bt, test_idbutton_cb, ob->id.name, NULL);
#endif

		if((G.f & G_PARTICLEEDIT)==0) {
			uiBlockBeginAlign(block);
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_OBJECTPANELPARENT, "Par:", 160, 180, 140, 20, &ob->parent, "Parent Object"); 
			if((ob->parent) && (ob->partype == PARBONE)) {
				bt= uiDefBut(block, TEX, B_OBJECTPANELPARENT, "ParBone:", 160, 160, 140, 20, ob->parsubstr, 0, 30, 0, 0, "");
				uiButSetCompleteFunc(bt, autocomplete_bone, (void *)ob->parent);
			}
			else {
				strcpy(ob->parsubstr, "");
			}
			uiBlockEndAlign(block);
		}
	}

	lim= 10000.0f*MAX2(1.0, G.vd->grid);

	if(ob==G.obedit) {
		if(ob->type==OB_ARMATURE) v3d_editarmature_buts(block, ob, lim);
		if(ob->type==OB_MBALL) v3d_editmetaball_buts(block, ob, lim);
		else v3d_editvertex_buts(block, ob, lim);
	}
	else if(ob->flag & OB_POSEMODE) {
		v3d_posearmature_buts(block, ob, lim);
	}
	else if(G.f & G_WEIGHTPAINT) {
		uiNewPanelTitle(block, "Weight Paint Properties");
		weight_paint_buttons(block);
	}
	else if(G.f & (G_VERTEXPAINT|G_TEXTUREPAINT)) {
		extern VPaint Gvp;         /* from vpaint */
		static float hsv[3], old[3];	// used as temp mem for picker
		float *rgb= NULL;
		ToolSettings *settings= G.scene->toolsettings;

		if(G.f & G_VERTEXPAINT) rgb= &Gvp.r;
		else if(settings->imapaint.brush) rgb= settings->imapaint.brush->rgb;
		
		uiNewPanelTitle(block, "Paint Properties");
		if (rgb)
			/* 'f' is for floating panel */
			uiBlockPickerButtons(block, rgb, hsv, old, hexcol, 'f', REDRAWBUTSEDIT);
	}
	else if(G.f & G_SCULPTMODE) {
		uiNewPanelTitle(block, "Sculpt Properties");
		sculptmode_draw_interface_tools(block,10,150);
	} else if(G.f & G_PARTICLEEDIT){
		uiNewPanelTitle(block, "Particle Edit Properties");
		particle_edit_buttons(block);
	} else {
		BoundBox *bb = NULL;

		uiBlockBeginAlign(block);
		uiDefIconButBitS(block, ICONTOG, OB_LOCK_LOCX, REDRAWVIEW3D, ICON_UNLOCKED,	10,150,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
		uiDefButF(block, NUM, B_OBJECTPANEL, "LocX:",		30, 150, 120, 19, &(ob->loc[0]), -lim, lim, 100, 3, "");
		uiDefIconButBitS(block, ICONTOG, OB_LOCK_LOCY, REDRAWVIEW3D, ICON_UNLOCKED,	10,130,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
		uiDefButF(block, NUM, B_OBJECTPANEL, "LocY:",		30, 130, 120, 19, &(ob->loc[1]), -lim, lim, 100, 3, "");
		uiDefIconButBitS(block, ICONTOG, OB_LOCK_LOCZ, REDRAWVIEW3D, ICON_UNLOCKED,	10,110,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
		uiDefButF(block, NUM, B_OBJECTPANEL, "LocZ:",		30, 110, 120, 19, &(ob->loc[2]), -lim, lim, 100, 3, "");
		
		tfp->ob_eul[0]= 180.0*ob->rot[0]/M_PI;
		tfp->ob_eul[1]= 180.0*ob->rot[1]/M_PI;
		tfp->ob_eul[2]= 180.0*ob->rot[2]/M_PI;
		
		uiBlockBeginAlign(block);
		if ((ob->parent) && (ob->partype == PARBONE)) {
			uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTX, REDRAWVIEW3D, ICON_UNLOCKED,	160,130,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
			uiDefButF(block, NUM, B_OBJECTPANELROT, "RotX:",	180, 130, 120, 19, &(tfp->ob_eul[0]), -lim, lim, 1000, 3, "");
			uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTY, REDRAWVIEW3D, ICON_UNLOCKED,	160,110,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
			uiDefButF(block, NUM, B_OBJECTPANELROT, "RotY:",	180, 110, 120, 19, &(tfp->ob_eul[1]), -lim, lim, 1000, 3, "");
			uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTZ, REDRAWVIEW3D, ICON_UNLOCKED,	160,90,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
			uiDefButF(block, NUM, B_OBJECTPANELROT, "RotZ:",	180, 90, 120, 19, &(tfp->ob_eul[2]), -lim, lim, 1000, 3, "");

		}
		else {
			uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTX, REDRAWVIEW3D, ICON_UNLOCKED,	160,150,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
			uiDefButF(block, NUM, B_OBJECTPANELROT, "RotX:",	180, 150, 120, 19, &(tfp->ob_eul[0]), -lim, lim, 1000, 3, "");
			uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTY, REDRAWVIEW3D, ICON_UNLOCKED,	160,130,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
			uiDefButF(block, NUM, B_OBJECTPANELROT, "RotY:",	180, 130, 120, 19, &(tfp->ob_eul[1]), -lim, lim, 1000, 3, "");
			uiDefIconButBitS(block, ICONTOG, OB_LOCK_ROTZ, REDRAWVIEW3D, ICON_UNLOCKED,	160,110,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
			uiDefButF(block, NUM, B_OBJECTPANELROT, "RotZ:",	180, 110, 120, 19, &(tfp->ob_eul[2]), -lim, lim, 1000, 3, "");
		}

		tfp->ob_scale[0]= ob->size[0];
		tfp->ob_scale[1]= ob->size[1];
		tfp->ob_scale[2]= ob->size[2];

		uiBlockBeginAlign(block);
		uiDefIconButBitS(block, ICONTOG, OB_LOCK_SCALEX, REDRAWVIEW3D, ICON_UNLOCKED,	10,80,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
		uiDefButF(block, NUM, B_OBJECTPANELSCALE, "ScaleX:",		30, 80, 120, 19, &(tfp->ob_scale[0]), -lim, lim, 10, 3, "");
		uiDefIconButBitS(block, ICONTOG, OB_LOCK_SCALEY, REDRAWVIEW3D, ICON_UNLOCKED,	10,60,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
		uiDefButF(block, NUM, B_OBJECTPANELSCALE, "ScaleY:",		30, 60, 120, 19, &(tfp->ob_scale[1]), -lim, lim, 10, 3, "");
		uiDefIconButBitS(block, ICONTOG, OB_LOCK_SCALEZ, REDRAWVIEW3D, ICON_UNLOCKED,	10,40,20,19, &(ob->protectflag), 0, 0, 0, 0, "Protects this value from being Transformed");
		uiDefButF(block, NUM, B_OBJECTPANELSCALE, "ScaleZ:",		30, 40, 120, 19, &(tfp->ob_scale[2]), -lim, lim, 10, 3, "");
		uiBlockEndAlign(block);
		
		uiDefButS(block, TOG, REDRAWVIEW3D, "Link Scale",		10, 10, 140, 19, &(tfp->link_scale), 0, 1, 0, 0, "Scale values vary proportionally in all directions");

		bb= object_get_boundbox(ob);
		if (bb) {
			float scale[3];

			Mat4ToSize(ob->obmat, scale);

			tfp->ob_dims[0] = fabs(scale[0]) * (bb->vec[4][0] - bb->vec[0][0]);
			tfp->ob_dims[1] = fabs(scale[1]) * (bb->vec[2][1] - bb->vec[0][1]);
			tfp->ob_dims[2] = fabs(scale[2]) * (bb->vec[1][2] - bb->vec[0][2]);

			uiBlockBeginAlign(block);
			if ((ob->parent) && (ob->partype == PARBONE)) {
				uiDefButF(block, NUM, B_OBJECTPANELDIMS, "DimX:",		160, 60, 140, 19, &(tfp->ob_dims[0]), 0.0, lim, 10, 3, "Manipulate bounding box size");
				uiDefButF(block, NUM, B_OBJECTPANELDIMS, "DimY:",		160, 40, 140, 19, &(tfp->ob_dims[1]), 0.0, lim, 10, 3, "Manipulate bounding box size");
				uiDefButF(block, NUM, B_OBJECTPANELDIMS, "DimZ:",		160, 20, 140, 19, &(tfp->ob_dims[2]), 0.0, lim, 10, 3, "Manipulate bounding box size");

			}
			else {
				uiDefButF(block, NUM, B_OBJECTPANELDIMS, "DimX:",		160, 80, 140, 19, &(tfp->ob_dims[0]), 0.0, lim, 10, 3, "Manipulate bounding box size");
				uiDefButF(block, NUM, B_OBJECTPANELDIMS, "DimY:",		160, 60, 140, 19, &(tfp->ob_dims[1]), 0.0, lim, 10, 3, "Manipulate bounding box size");
				uiDefButF(block, NUM, B_OBJECTPANELDIMS, "DimZ:",		160, 40, 140, 19, &(tfp->ob_dims[2]), 0.0, lim, 10, 3, "Manipulate bounding box size");
			}

			uiBlockEndAlign(block);
		}
	}
	uiClearButLock();
}

static void view3d_panel_background(short cntrl)	// VIEW3D_HANDLER_BACKGROUND
{
	uiBlock *block;
	View3D *vd;
	
	vd= G.vd;

	block= uiNewBlock(&curarea->uiblocks, "view3d_panel_background", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE  | cntrl);
	uiSetPanelHandler(VIEW3D_HANDLER_BACKGROUND);  // for close and esc
	if(uiNewPanel(curarea, block, "Background Image", "View3d", 340, 10, 318, 204)==0) return;

	if(G.f & G_VERTEXPAINT || G.f & G_WEIGHTPAINT || G.f & G_TEXTUREPAINT) {
		uiBlockSetFlag(block, UI_BLOCK_FRONTBUFFER);	// force old style frontbuffer draw
	}
	
	if(vd->flag & V3D_DISPBGPIC) {
		if(vd->bgpic==NULL) {
			vd->bgpic= MEM_callocN(sizeof(BGpic), "bgpic");
			vd->bgpic->size= 5.0;
			vd->bgpic->blend= 0.5;
			vd->bgpic->iuser.fie_ima= 2;
			vd->bgpic->iuser.ok= 1;
		}
	}
	
	if(!(vd->flag & V3D_DISPBGPIC)) {
		uiDefButBitS(block, TOG, V3D_DISPBGPIC, B_REDR, "Use Background Image", 10, 180, 150, 20, &vd->flag, 0, 0, 0, 0, "Display an image in the background of this 3D View");
		uiDefBut(block, LABEL, 1, " ",	160, 180, 150, 20, NULL, 0.0, 0.0, 0, 0, "");
	}
	else {
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, V3D_DISPBGPIC, B_REDR, "Use", 10, 225, 50, 20, &vd->flag, 0, 0, 0, 0, "Display an image in the background of this 3D View");
		uiDefButF(block, NUMSLI, B_REDR, "Blend:",	60, 225, 150, 20, &vd->bgpic->blend, 0.0,1.0, 0, 0, "Set the transparency of the background image");
		uiDefButF(block, NUM, B_REDR, "Size:",		210, 225, 100, 20, &vd->bgpic->size, 0.1, 250.0*vd->grid, 100, 0, "Set the size (width) of the background image");

		uiDefButF(block, NUM, B_REDR, "X Offset:",	10, 205, 150, 20, &vd->bgpic->xof, -250.0*vd->grid,250.0*vd->grid, 10, 2, "Set the horizontal offset of the background image");
		uiDefButF(block, NUM, B_REDR, "Y Offset:",	160, 205, 150, 20, &vd->bgpic->yof, -250.0*vd->grid,250.0*vd->grid, 10, 2, "Set the vertical offset of the background image");
		
		uiblock_image_panel(block, &vd->bgpic->ima, &vd->bgpic->iuser, B_REDR, B_REDR);
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
	if(uiNewPanel(curarea, block, "View Properties", "View3d", 340, 30, 318, 254)==0) return;

	/* to force height */
	uiNewPanelHeight(block, 264);

	if(G.f & (G_VERTEXPAINT|G_FACESELECT|G_TEXTUREPAINT|G_WEIGHTPAINT)) {
		uiBlockSetFlag(block, UI_BLOCK_FRONTBUFFER);	// force old style frontbuffer draw
	}

	uiDefBut(block, LABEL, 1, "Grid:",					10, 220, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, REDRAWVIEW3D, "Spacing:",		10, 200, 140, 19, &vd->grid, 0.001, 100.0, 10, 0, "Set the distance between grid lines");
	uiDefButS(block, NUM, REDRAWVIEW3D, "Lines:",		10, 180, 140, 19, &vd->gridlines, 0.0, 100.0, 100, 0, "Set the number of grid lines in perspective view");
	uiDefButS(block, NUM, REDRAWVIEW3D, "Divisions:",		10, 160, 140, 19, &vd->gridsubdiv, 1.0, 100.0, 100, 0, "Set the number of grid lines");
	uiBlockEndAlign(block);

	uiDefBut(block, LABEL, 1, "3D Display:",							160, 220, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefButBitS(block, TOG, V3D_SHOW_FLOOR, REDRAWVIEW3D, "Grid Floor",160, 200, 150, 19, &vd->gridflag, 0, 0, 0, 0, "Show the grid floor in free camera mode");
	uiDefButBitS(block, TOG, V3D_SHOW_X, REDRAWVIEW3D, "X Axis",		160, 176, 48, 19, &vd->gridflag, 0, 0, 0, 0, "Show the X Axis line");
	uiDefButBitS(block, TOG, V3D_SHOW_Y, REDRAWVIEW3D, "Y Axis",		212, 176, 48, 19, &vd->gridflag, 0, 0, 0, 0, "Show the Y Axis line");
	uiDefButBitS(block, TOG, V3D_SHOW_Z, REDRAWVIEW3D, "Z Axis",		262, 176, 48, 19, &vd->gridflag, 0, 0, 0, 0, "Show the Z Axis line");

	uiDefBut(block, LABEL, 1, "View Camera:",			10, 140, 140, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefButF(block, NUM, REDRAWVIEW3D, "Lens:",		10, 120, 140, 19, &vd->lens, 10.0, 120.0, 100, 0, "The lens angle in perspective view");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, REDRAWVIEW3D, "Clip Start:",	10, 96, 140, 19, &vd->near, vd->grid/100.0, 100.0, 10, 0, "Set the beginning of the range in which 3D objects are displayed (perspective view)");
	uiDefButF(block, NUM, REDRAWVIEW3D, "Clip End:",	10, 76, 140, 19, &vd->far, 1.0, 10000.0*vd->grid, 100, 0, "Set the end of the range in which 3D objects are displayed (perspective view)");
	uiBlockEndAlign(block);

	uiDefBut(block, LABEL, 1, "3D Cursor:",				160, 150, 140, 19, NULL, 0.0, 0.0, 0, 0, "");

	uiBlockBeginAlign(block);
	curs= give_cursor();
	uiDefButF(block, NUM, REDRAWVIEW3D, "X:",			160, 130, 150, 22, curs, -10000.0*vd->grid, 10000.0*vd->grid, 10, 0, "X co-ordinate of the 3D cursor");
	uiDefButF(block, NUM, REDRAWVIEW3D, "Y:",			160, 108, 150, 22, curs+1, -10000.0*vd->grid, 10000.0*vd->grid, 10, 0, "Y co-ordinate of the 3D cursor");
	uiDefButF(block, NUM, REDRAWVIEW3D, "Z:",			160, 86, 150, 22, curs+2, -10000.0*vd->grid, 10000.0*vd->grid, 10, 0, "Z co-ordinate of the 3D cursor");
	uiBlockEndAlign(block);

	uiDefBut(block, LABEL, 1, "Display:",				10, 50, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, V3D_SELECT_OUTLINE, REDRAWVIEW3D, "Outline Selected", 10, 30, 140, 19, &vd->flag, 0, 0, 0, 0, "Highlight selected objects with an outline, in Solid, Shaded or Textured viewport shading modes");
	uiDefButBitS(block, TOG, V3D_DRAW_CENTERS, REDRAWVIEW3D, "All Object Centers", 10, 10, 140, 19, &vd->flag, 0, 0, 0, 0, "Draw the center points on all objects");
	uiDefButBitS(block, TOGN, V3D_HIDE_HELPLINES, REDRAWVIEW3D, "Relationship Lines", 10, -10, 140, 19, &vd->flag, 0, 0, 0, 0, "Draw dashed lines indicating Parent, Constraint, or Hook relationships");
	uiDefButBitS(block, TOG, V3D_SOLID_TEX, REDRAWVIEW3D, "Solid Tex", 10, -30, 140, 19, &vd->flag2, 0, 0, 0, 0, "Display textures in Solid draw type (Shift T)");
	uiBlockEndAlign(block);

	uiDefBut(block, LABEL, 1, "View Locking:",				160, 50, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefIDPoinBut(block, test_obpoin_but, ID_OB, REDRAWVIEW3D, "Object:", 160, 30, 140, 19, &vd->ob_centre, "Lock view to center to this Object"); 
	uiDefBut(block, TEX, REDRAWVIEW3D, "Bone:",						160, 10, 140, 19, vd->ob_centre_bone, 1, 31, 0, 0, "If view locked to Object, use this Bone to lock to view to");

}

static void view3d_panel_preview(ScrArea *sa, short cntrl)	// VIEW3D_HANDLER_PREVIEW
{
	uiBlock *block;
	View3D *v3d= sa->spacedata.first;
	int ofsx, ofsy;
	
	block= uiNewBlock(&sa->uiblocks, "view3d_panel_preview", UI_EMBOSS, UI_HELV, sa->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | UI_PNL_SCALE | cntrl);
	uiSetPanelHandler(VIEW3D_HANDLER_PREVIEW);  // for close and esc
	
	ofsx= -150+(sa->winx/2)/v3d->blockscale;
	ofsy= -100+(sa->winy/2)/v3d->blockscale;
	if(uiNewPanel(sa, block, "Preview", "View3d", ofsx, ofsy, 300, 200)==0) return;

	uiBlockSetDrawExtraFunc(block, BIF_view3d_previewdraw);
	
	if(G.scene->recalc & SCE_PRV_CHANGED) {
		G.scene->recalc &= ~SCE_PRV_CHANGED;
		//printf("found recalc\n");
		BIF_view3d_previewrender_free(sa->spacedata.first);
		BIF_preview_changed(0);
	}
}


static void view3d_blockhandlers(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	short a;
	
	/* warning; blocks need to be freed each time, handlers dont remove */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);
	
	/*uv face-sel and wp mode when mixed with wire leave depth enabled causing
	models to draw over the UI */
	glDisable(GL_DEPTH_TEST); 
	
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
		case VIEW3D_HANDLER_PREVIEW:
			view3d_panel_preview(sa, v3d->blockhandler[a+1]);
			break;			
		case VIEW3D_HANDLER_TRANSFORM:
			view3d_panel_transform_spaces(v3d->blockhandler[a+1]);
 			break;			
		}
		/* clear action value for event */
		v3d->blockhandler[a+1]= 0;
	}
	uiDrawBlocksPanels(sa, 0);

}

/* ****************** View3d afterdraw *************** */

typedef struct View3DAfter {
	struct View3DAfter *next, *prev;
	struct Base *base;
	int type, flag;
} View3DAfter;

/* temp storage of Objects that need to be drawn as last */
void add_view3d_after(View3D *v3d, Base *base, int type, int flag)
{
	View3DAfter *v3da= MEM_callocN(sizeof(View3DAfter), "View 3d after");

	BLI_addtail(&v3d->afterdraw, v3da);
	v3da->base= base;
	v3da->type= type;
	v3da->flag= flag;
}

/* clears zbuffer and draws it over */
static void view3d_draw_xray(View3D *v3d)
{
	View3DAfter *v3da, *next;
	int doit= 0;
	
	for(v3da= v3d->afterdraw.first; v3da; v3da= v3da->next)
		if(v3da->type==V3D_XRAY) doit= 1;
	
	if(doit) {
		if(v3d->zbuf) glClear(GL_DEPTH_BUFFER_BIT);
		v3d->xray= TRUE;
		
		for(v3da= v3d->afterdraw.first; v3da; v3da= next) {
			next= v3da->next;
			if(v3da->type==V3D_XRAY) {
				draw_object(v3da->base, v3da->flag);
				BLI_remlink(&v3d->afterdraw, v3da);
				MEM_freeN(v3da);
			}
		}
		v3d->xray= FALSE;
	}
}

/* disables write in zbuffer and draws it over */
static void view3d_draw_transp(View3D *v3d)
{
	View3DAfter *v3da, *next;

	glDepthMask(0);
	v3d->transp= TRUE;
		
	for(v3da= v3d->afterdraw.first; v3da; v3da= next) {
		next= v3da->next;
		if(v3da->type==V3D_TRANSP) {
			draw_object(v3da->base, v3da->flag);
			BLI_remlink(&v3d->afterdraw, v3da);
			MEM_freeN(v3da);
		}
	}
	v3d->transp= FALSE;

	glDepthMask(1);

}

/* *********************** */

/*
	In most cases call draw_dupli_objects,
	draw_dupli_objects_color was added because when drawing set dupli's
	we need to force the color
*/
static void draw_dupli_objects_color(View3D *v3d, Base *base, int color)
{	
	ListBase *lb;
	DupliObject *dob;
	Base tbase;
	BoundBox *bb= NULL;
	GLuint displist=0;
	short transflag, use_displist= -1;	/* -1 is initialize */
	char dt, dtx;
	
	if (base->object->restrictflag & OB_RESTRICT_VIEW) return;
	
	tbase.flag= OB_FROMDUPLI|base->flag;
	lb= object_duplilist(G.scene, base->object);

	for(dob= lb->first; dob; dob= dob->next) {
		if(dob->no_draw);
		else {
			tbase.object= dob->ob;
			
			/* extra service: draw the duplicator in drawtype of parent */
			dt= tbase.object->dt; tbase.object->dt= base->object->dt;
			dtx= tbase.object->dtx; tbase.object->dtx= base->object->dtx;
			
			/* negative scale flag has to propagate */
			transflag= tbase.object->transflag;
			if(base->object->transflag & OB_NEG_SCALE)
				tbase.object->transflag ^= OB_NEG_SCALE;
			
			BIF_ThemeColorBlend(color, TH_BACK, 0.5);
			
			/* generate displist, test for new object */
			if(use_displist==1 && dob->prev && dob->prev->ob!=dob->ob) {
				use_displist= -1;
				glDeleteLists(displist, 1);
			}
			/* generate displist */
			if(use_displist == -1) {
				
				/* lamp drawing messes with matrices, could be handled smarter... but this works */
				if(dob->ob->type==OB_LAMP || dob->type==OB_DUPLIGROUP)
					use_displist= 0;
				else {
					/* disable boundbox check for list creation */
					object_boundbox_flag(dob->ob, OB_BB_DISABLED, 1);
					/* need this for next part of code */
					bb= object_get_boundbox(dob->ob);
					
					Mat4One(dob->ob->obmat);	/* obmat gets restored */
					
					displist= glGenLists(1);
					glNewList(displist, GL_COMPILE);
					draw_object(&tbase, DRAW_CONSTCOLOR);
					glEndList();
					
					use_displist= 1;
					object_boundbox_flag(dob->ob, OB_BB_DISABLED, 0);
				}
			}
			if(use_displist) {
				mymultmatrix(dob->mat);
				if(boundbox_clip(dob->mat, bb))
				   glCallList(displist);
				myloadmatrix(G.vd->viewmat);
			}
			else {
				Mat4CpyMat4(dob->ob->obmat, dob->mat);
				draw_object(&tbase, DRAW_CONSTCOLOR);
			}
			
			tbase.object->dt= dt;
			tbase.object->dtx= dtx;
			tbase.object->transflag= transflag;
		}
	}
	
	/* Transp afterdraw disabled, afterdraw only stores base pointers, and duplis can be same obj */
	
	free_object_duplilist(lb);	/* does restore */
	
	if(use_displist)
		glDeleteLists(displist, 1);
}

static void draw_dupli_objects(View3D *v3d, Base *base)
{
	/* define the color here so draw_dupli_objects_color can be called
	 * from the set loop */
	
	int color= (base->flag & SELECT)?TH_SELECT:TH_WIRE;
	/* debug */
	if(base->object->dup_group && base->object->dup_group->id.us<1)
		color= TH_REDALERT;
	
	draw_dupli_objects_color(v3d, base, color);
}

void view3d_update_depths(View3D *v3d)
{
	/* Create storage for, and, if necessary, copy depth buffer */
	if(!v3d->depths) v3d->depths= MEM_callocN(sizeof(ViewDepths),"ViewDepths");
	if(v3d->depths) {
		ViewDepths *d= v3d->depths;
		if(d->w != v3d->area->winx ||
		   d->h != v3d->area->winy ||
		   !d->depths) {
			d->w= v3d->area->winx;
			d->h= v3d->area->winy;
			if(d->depths)
				MEM_freeN(d->depths);
			d->depths= MEM_mallocN(sizeof(float)*d->w*d->h,"View depths");
			d->damaged= 1;
		}
		
		if(d->damaged) {
			glReadPixels(v3d->area->winrct.xmin,v3d->area->winrct.ymin,d->w,d->h,
				     GL_DEPTH_COMPONENT,GL_FLOAT, d->depths);
			
			glGetDoublev(GL_DEPTH_RANGE,d->depth_range);
			
			d->damaged= 0;
		}
	}
}

/* Enable sculpting in wireframe mode by drawing sculpt object only to the depth buffer */
static void draw_sculpt_depths(View3D *v3d)
{
	Object *ob = OBACT;

	int dt= MIN2(v3d->drawtype, ob->dt);
	if(v3d->zbuf==0 && dt>OB_WIRE)
		dt= OB_WIRE;
	if(dt == OB_WIRE) {
		GLboolean depth_on;
		int orig_vdt = v3d->drawtype;
		int orig_zbuf = v3d->zbuf;
		int orig_odt = ob->dt;

		glGetBooleanv(GL_DEPTH_TEST, &depth_on);
		v3d->drawtype = ob->dt = OB_SOLID;
		v3d->zbuf = 1;

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glEnable(GL_DEPTH_TEST);
		draw_object(BASACT, 0);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		if(!depth_on)
			glDisable(GL_DEPTH_TEST);

		v3d->drawtype = orig_vdt;
		v3d->zbuf = orig_zbuf;
		ob->dt = orig_odt;
	}
}

void draw_depth(ScrArea *sa, void *spacedata)
{
	View3D *v3d= spacedata;
	Base *base;
	Scene *sce;
	short zbuf, flag;
	float glalphaclip;
	/* temp set drawtype to solid */
	
	/* Setting these temporarily is not nice */
	zbuf = v3d->zbuf;
	flag = v3d->flag;
	glalphaclip = U.glalphaclip;
	
	U.glalphaclip = 0.5; /* not that nice but means we wont zoom into billboards */
	v3d->flag &= ~V3D_SELECT_OUTLINE;

	setwinmatrixview3d(sa->winx, sa->winy, NULL);	/* 0= no pick rect */
	setviewmatrixview3d();	/* note: calls where_is_object for camera... */
	
	Mat4MulMat4(v3d->persmat, v3d->viewmat, sa->winmat);
	Mat4Invert(v3d->persinv, v3d->persmat);
	Mat4Invert(v3d->viewinv, v3d->viewmat);
	
	glClear(GL_DEPTH_BUFFER_BIT);
	
	myloadmatrix(v3d->viewmat);
	persp(PERSP_STORE);  // store correct view for persp(PERSP_VIEW) calls
	
	if(v3d->flag & V3D_CLIPPING) {
		view3d_set_clipping(v3d);
	}
	
	v3d->zbuf= TRUE;
	glEnable(GL_DEPTH_TEST);
	
	/* draw set first */
	if(G.scene->set) {
		for(SETLOOPER(G.scene->set, base)) {
			if(v3d->lay & base->lay) {
				draw_object(base, 0);
				if(base->object->transflag & OB_DUPLI) {
					draw_dupli_objects_color(v3d, base, TH_WIRE);
				}
			}
		}
	}
	
	for(base= G.scene->base.first; base; base= base->next) {
		if(v3d->lay & base->lay) {
			
			/* dupli drawing */
			if(base->object->transflag & OB_DUPLI) {
				draw_dupli_objects(v3d, base);
			}
			draw_object(base, 0);
		}
	}
	
	/* this isnt that nice, draw xray objects as if they are normal */
	if (v3d->afterdraw.first) {
		View3DAfter *v3da, *next;
		int num = 0;
		v3d->xray= TRUE;
		
		glDepthFunc(GL_ALWAYS); /* always write into the depth bufer, overwriting front z values */
		for(v3da= v3d->afterdraw.first; v3da; v3da= next) {
			next= v3da->next;
			if(v3da->type==V3D_XRAY) {
				draw_object(v3da->base, 0);
				num++;
			}
			/* dont remove this time */
		}
		v3d->xray= FALSE;
		
		glDepthFunc(GL_LEQUAL); /* Now write the depth buffer normally */
		for(v3da= v3d->afterdraw.first; v3da; v3da= next) {
			next= v3da->next;
			if(v3da->type==V3D_XRAY) {
				v3d->xray= TRUE; v3d->transp= FALSE;  
			} else if (v3da->type==V3D_TRANSP) {
				v3d->xray= FALSE; v3d->transp= TRUE;
			}
			
			draw_object(v3da->base, 0); /* Draw Xray or Transp objects normally */
			BLI_remlink(&v3d->afterdraw, v3da);
			MEM_freeN(v3da);
		}
		v3d->xray= FALSE;
		v3d->transp= FALSE;
	}
	
	v3d->zbuf = zbuf;
	U.glalphaclip = glalphaclip;
	v3d->flag = flag;
}

static void draw_viewport_fps(ScrArea *sa);


void drawview3dspace(ScrArea *sa, void *spacedata)
{
	View3D *v3d= spacedata;
	Base *base;
	Object *ob;
	Scene *sce;
	char retopo, sculptparticle;
	Object *obact = OBACT;
	
	/* update all objects, ipos, matrices, displists, etc. Flags set by depgraph or manual, 
	   no layer check here, gets correct flushed */
	/* sets first, we allow per definition current scene to have dependencies on sets */
	if(G.scene->set) {
		for(SETLOOPER(G.scene->set, base))
			object_handle_update(base->object);   // bke_object.h
	}

	for(base= G.scene->base.first; base; base= base->next)
		object_handle_update(base->object);   // bke_object.h
	
	setwinmatrixview3d(sa->winx, sa->winy, NULL);	/* 0= no pick rect */
	setviewmatrixview3d();	/* note: calls where_is_object for camera... */

	Mat4MulMat4(v3d->persmat, v3d->viewmat, sa->winmat);
	Mat4Invert(v3d->persinv, v3d->persmat);
	Mat4Invert(v3d->viewinv, v3d->viewmat);

	/* calculate pixelsize factor once, is used for lamps and obcenters */
	{
		float len1, len2, vec[3];

		VECCOPY(vec, v3d->persinv[0]);
		len1= Normalize(vec);
		VECCOPY(vec, v3d->persinv[1]);
		len2= Normalize(vec);
		
		v3d->pixsize= 2.0f*(len1>len2?len1:len2);
		
		/* correct for window size */
		if(sa->winx > sa->winy) v3d->pixsize/= (float)sa->winx;
		else v3d->pixsize/= (float)sa->winy;
	}
	
	if(v3d->drawtype > OB_WIRE) {
		if(G.f & G_SIMULATION)
			glClearColor(0.0, 0.0, 0.0, 0.0); 
		else {
			float col[3];
			BIF_GetThemeColor3fv(TH_BACK, col);
			glClearColor(col[0], col[1], col[2], 0.0); 
		}
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		
		glLoadIdentity();
	}
	else {
		float col[3];
		BIF_GetThemeColor3fv(TH_BACK, col);
		glClearColor(col[0], col[1], col[2], 0.0);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	}
	
	myloadmatrix(v3d->viewmat);
	persp(PERSP_STORE);  // store correct view for persp(PERSP_VIEW) calls

	if(v3d->flag & V3D_CLIPPING)
		view3d_draw_clipping(v3d);
	
	/* set zbuffer after we draw clipping region */
	if(v3d->drawtype > OB_WIRE) {
		v3d->zbuf= TRUE;
		glEnable(GL_DEPTH_TEST);
	}
	
	// needs to be done always, gridview is adjusted in drawgrid() now
	v3d->gridview= v3d->grid;
	
	if(v3d->view==0 || v3d->persp!=0) {
		drawfloor();
		if(v3d->persp==2) {
			if(G.scene->world) {
				if(G.scene->world->mode & WO_STARS) {
					RE_make_stars(NULL, star_stuff_init_func, star_stuff_vertex_func,
								  star_stuff_term_func);
				}
			}
			if(v3d->flag & V3D_DISPBGPIC) draw_bgpic();
		}
	}
	else {
		drawgrid();

		if(v3d->flag & V3D_DISPBGPIC) {
			draw_bgpic();
		}
	}
	
	if(v3d->flag & V3D_CLIPPING)
		view3d_set_clipping(v3d);
	
	/* draw set first */
	if(G.scene->set) {
		for(SETLOOPER(G.scene->set, base)) {
			
			if(v3d->lay & base->lay) {
				
				BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.6f);
				draw_object(base, DRAW_CONSTCOLOR);

				if(base->object->transflag & OB_DUPLI) {
					draw_dupli_objects_color(v3d, base, TH_WIRE);
				}
			}
		}

		/* Transp and X-ray afterdraw stuff for sets is done later */
	}
	
	/* then draw not selected and the duplis, but skip editmode object */
	for(base= G.scene->base.first; base; base= base->next) {
		if(v3d->lay & base->lay) {
			
			/* dupli drawing */
			if(base->object->transflag & OB_DUPLI) {
				draw_dupli_objects(v3d, base);
			}
			if((base->flag & SELECT)==0) {
				if(base->object!=G.obedit) draw_object(base, 0);
			}
		}
	}

	retopo= retopo_mesh_check() || retopo_curve_check();
	sculptparticle= (G.f & (G_SCULPTMODE|G_PARTICLEEDIT)) && !G.obedit;
	if(retopo)
		view3d_update_depths(v3d);

	/* draw selected and editmode */
	for(base= G.scene->base.first; base; base= base->next) {
		if(v3d->lay & base->lay) {
			if (base->object==G.obedit || ( base->flag & SELECT) ) 
				draw_object(base, 0);
		}
	}

	if(!retopo && sculptparticle && !(obact && (obact->dtx & OB_DRAWXRAY))) {
		if(G.f & G_SCULPTMODE)
			draw_sculpt_depths(v3d);
		view3d_update_depths(v3d);
	}

	if(G.moving) {
		BIF_drawConstraint();
		if(G.obedit || (G.f & G_PARTICLEEDIT))
			BIF_drawPropCircle(); // only editmode and particles have proportional edit
		BIF_drawSnap();
	}

	if(G.scene->radio) RAD_drawall(v3d->drawtype>=OB_SOLID);
	
	/* Transp and X-ray afterdraw stuff */
	view3d_draw_xray(v3d);	// clears zbuffer if it is used!
	view3d_draw_transp(v3d);

	if(!retopo && sculptparticle && (obact && (OBACT->dtx & OB_DRAWXRAY))) {
		if(G.f & G_SCULPTMODE)
			draw_sculpt_depths(v3d);
		view3d_update_depths(v3d);
	}
	
	if(v3d->flag & V3D_CLIPPING)
		view3d_clr_clipping();
	
	BIF_draw_manipulator(sa);
		
	if(v3d->zbuf) {
		v3d->zbuf= FALSE;
		glDisable(GL_DEPTH_TEST);
	}

	persp(PERSP_WIN);  // set ortho

	/* Draw Sculpt Mode brush */
	if(!G.obedit && (G.f & G_SCULPTMODE) && area_is_active_area(v3d->area) && sculpt_session()) {
		RadialControl *rc= sculpt_session()->radialcontrol;

		if(sculpt_data()->flags & SCULPT_INPUT_SMOOTH)
			sculpt_stroke_draw();

		if(rc)
			radialcontrol_draw(rc);
		else if(sculpt_data()->flags & SCULPT_DRAW_BRUSH) {
			short csc[2], car[2];
			getmouseco_sc(csc);
			getmouseco_areawin(car);
			if(csc[0] > v3d->area->winrct.xmin && 
			   csc[1] > v3d->area->winrct.ymin &&
			   csc[0] < v3d->area->winrct.xmax &&
			   csc[1] < v3d->area->winrct.ymax)
				fdrawXORcirc((float)car[0], (float)car[1], sculptmode_brush()->size);
		}
	}

	retopo_paint_view_update(v3d);
	retopo_draw_paint_lines();

	if(!G.obedit && OBACT && G.f&G_PARTICLEEDIT && area_is_active_area(v3d->area)){
		ParticleSystem *psys = PE_get_current(OBACT);
		ParticleEditSettings *pset = PE_settings();

		short c[2];
		if(*PE_radialcontrol())
			radialcontrol_draw(*PE_radialcontrol());
		else if(psys && psys->edit && pset->brushtype>=0) {
			getmouseco_areawin(c);
			fdrawXORcirc((float)c[0], (float)c[1], (float)pset->brush[pset->brushtype].size);
		}
	}

	if(v3d->persp>1) drawviewborder();
	if(v3d->flag2 & V3D_FLYMODE) drawviewborder_flymode();
	if(!(G.f & G_PLAYANIM)) drawcursor(v3d);
	if(U.uiflag & USER_SHOW_ROTVIEWICON)
		draw_view_axis();
	else	
		draw_view_icon();
	
	if(U.uiflag & USER_SHOW_FPS && G.f & G_PLAYANIM) {
		draw_viewport_fps(sa);
	} else if(U.uiflag & USER_SHOW_VIEWPORTNAME) {
		draw_viewport_name(sa);
	}

	ob= OBACT;
	if(ob && (U.uiflag & USER_DRAWVIEWINFO)) 
		draw_selected_name(ob);
	
	draw_area_emboss(sa);
	
	/* it is important to end a view in a transform compatible with buttons */

	bwin_scalematrix(sa->win, v3d->blockscale, v3d->blockscale, v3d->blockscale);
	view3d_blockhandlers(sa);

	sa->win_swap= WIN_BACK_OK;
	
	if(G.f & G_VERTEXPAINT || G.f & G_WEIGHTPAINT || G.f & G_TEXTUREPAINT) {
		v3d->flag |= V3D_NEEDBACKBUFDRAW;
		addafterqueue(sa->win, BACKBUFDRAW, 1);
	}
	// test for backbuf select
	if(G.obedit && v3d->drawtype>OB_WIRE && (v3d->flag & V3D_ZBUF_SELECT)) {
		extern int afterqtest(short win, unsigned short evt);	//editscreen.c

		v3d->flag |= V3D_NEEDBACKBUFDRAW;
		if(afterqtest(sa->win, BACKBUFDRAW)==0) {
			addafterqueue(sa->win, BACKBUFDRAW, 1);
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


void drawview3d_render(struct View3D *v3d, int winx, int winy, float winmat[][4])
{
	Base *base;
	Scene *sce;
	float v3dwinmat[4][4];
	
	if(!winmat)
		setwinmatrixview3d(winx, winy, NULL);

	setviewmatrixview3d();
	myloadmatrix(v3d->viewmat);

	/* when winmat is not NULL, it overrides the regular window matrix */
	glMatrixMode(GL_PROJECTION);
	if(winmat)
		myloadmatrix(winmat);
	mygetmatrix(v3dwinmat);
	glMatrixMode(GL_MODELVIEW);

	Mat4MulMat4(v3d->persmat, v3d->viewmat, v3dwinmat);
	Mat4Invert(v3d->persinv, v3d->persmat);
	Mat4Invert(v3d->viewinv, v3d->viewmat);

	free_all_realtime_images();
	reshadeall_displist();
	
	if(v3d->drawtype > OB_WIRE) {
		v3d->zbuf= TRUE;
		glEnable(GL_DEPTH_TEST);
	}

	if(v3d->flag & V3D_CLIPPING)
		view3d_set_clipping(v3d);

	if (v3d->drawtype==OB_TEXTURE && G.scene->world) {
		glClearColor(G.scene->world->horr, G.scene->world->horg, G.scene->world->horb, 0.0); 
	} else {
		float col[3];
		BIF_GetThemeColor3fv(TH_BACK, col);
		glClearColor(col[0], col[1], col[2], 0.0); 
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* abuse! to make sure it doesnt draw the helpstuff */
	G.f |= G_SIMULATION;

	/* first draw set */
	if(G.scene->set) {
	
		for(SETLOOPER(G.scene->set, base)) {
			if(v3d->lay & base->lay) {
				if ELEM3(base->object->type, OB_LAMP, OB_CAMERA, OB_LATTICE);
				else {
					where_is_object(base->object);
	
					BIF_ThemeColorBlend(TH_WIRE, TH_BACK, 0.6f);
					draw_object(base, DRAW_CONSTCOLOR);
	
					if(base->object->transflag & OB_DUPLI) {
						draw_dupli_objects(v3d, base);
					}
				}
			}
		}
		
		/* Transp and X-ray afterdraw stuff for sets is done later */
	}

	/* first not selected and duplis */
	base= G.scene->base.first;
	while(base) {
		
		if(v3d->lay & base->lay) {
			if ELEM3(base->object->type, OB_LAMP, OB_CAMERA, OB_LATTICE);
			else {
	
				if(base->object->transflag & OB_DUPLI) {
					draw_dupli_objects(v3d, base);
				}
				else if((base->flag & SELECT)==0) {
					draw_object(base, 0);
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
			else draw_object(base, 0);
		}
		
		base= base->next;
	}

	if(G.scene->radio) RAD_drawall(v3d->drawtype>=OB_SOLID);

	/* Transp and X-ray afterdraw stuff */
	view3d_draw_xray(v3d);	// clears zbuffer if it is used!
	view3d_draw_transp(v3d);
	
	if(v3d->flag & V3D_CLIPPING)
		view3d_clr_clipping();
	
	if(v3d->zbuf) {
		v3d->zbuf= FALSE;
		glDisable(GL_DEPTH_TEST);
	}
	
	G.f &= ~G_SIMULATION;

	glFlush();

	glLoadIdentity();

	free_all_realtime_images();
}


double tottime = 0.0;
static ScrArea *oldsa;
static double swaptime;
static int curmode;

/* used for fps display */
#define REDRAW_FRAME_AVERAGE 8
static double redrawtime;
static double lredrawtime;
static float redrawtimes_fps[REDRAW_FRAME_AVERAGE];
static short redrawtime_index;


int update_time(int cfra)
{
	static double ltime;
	double time;

	if ((audiostream_pos() != cfra)
	    && (G.scene->audio.flag & AUDIO_SYNC)) {
		return 0;
	}

	time = PIL_check_seconds_timer();
	
	tottime += (time - ltime);
	ltime = time;
	return (tottime < 0.0);
}

static void draw_viewport_fps(ScrArea *sa)
{
	float fps;
	char printable[16];
	int i, tot;
	
	if (!lredrawtime || !redrawtime)
		return;
	
	printable[0] = '\0';
	
#if 0
	/* this is too simple, better do an average */
	fps = (float)(1.0/(lredrawtime-redrawtime))
#else
	redrawtimes_fps[redrawtime_index] = (float)(1.0/(lredrawtime-redrawtime));
	
	for (i=0, tot=0, fps=0.0f ; i < REDRAW_FRAME_AVERAGE ; i++) {
		if (redrawtimes_fps[i]) {
			fps += redrawtimes_fps[i];
			tot++;
		}
	}
	if (tot) {
		redrawtime_index++;
		if (redrawtime_index >= REDRAW_FRAME_AVERAGE)
			redrawtime_index = 0;
		
		fps = fps / tot;
	}
#endif
	
	/* is this more then half a frame behind? */
	if (fps+0.5 < FPS) {
		BIF_ThemeColor(TH_REDALERT);
		sprintf(printable, "fps: %.2f", (float)fps);
	} else {
		BIF_ThemeColor(TH_TEXT_HI);
		sprintf(printable, "fps: %i", (int)(fps+0.5));
	}
	
	glRasterPos2i(10,  sa->winy-20);
	BMF_DrawString(G.fonts, printable);
}

static void inner_play_prefetch_frame(int mode, int cfra)
{
	ScrArea *sa;
	int oldcfra = CFRA;
	ScrArea *oldcurarea = curarea;

	if (!U.prefetchframes) {
		return;
	}

	CFRA = cfra;

	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa==oldsa) {
			scrarea_do_winprefetchdraw(sa);
		}
		else if(mode & 1) { /* all view3d and seq spaces */
			if ELEM(sa->spacetype, SPACE_VIEW3D, SPACE_SEQ) {
				scrarea_do_winprefetchdraw(sa);
			}
		}
		else if(mode & 4) { /* all seq spaces */
			if (sa->spacetype == SPACE_SEQ) {
				scrarea_do_winprefetchdraw(sa);
			}
		}		
		
		sa= sa->next;	
	}

	CFRA = oldcfra;
	curarea = oldcurarea;
}

static void inner_play_prefetch_startup(int mode)
{
	int i;

	if (!U.prefetchframes) {
		return;
	}

	seq_start_threads();

	for (i = 0; i <= U.prefetchframes; i++) {
		int cfra = CFRA + i;
		inner_play_prefetch_frame(mode, cfra);
	}

	seq_wait_for_prefetch_ready();
}

static void inner_play_prefetch_shutdown(int mode)
{
	if (!U.prefetchframes) {
		return;
	}
	seq_stop_threads();
}

static int cached_dynamics(int sfra, int efra)
{
	Base *base = G.scene->base.first;
	Object *ob;
	ParticleSystem *psys;
	int i, cached=1;
	PTCacheID pid;

	while(base && cached) {
		ob = base->object;
		if(ob->softflag & OB_SB_ENABLE && ob->soft) {
			BKE_ptcache_id_from_softbody(&pid, ob, ob->soft);

			for(i=sfra; i<=efra && cached; i++)
				cached &= BKE_ptcache_id_exist(&pid, i);
		}

		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			if(psys->part->type==PART_HAIR) {
				if(psys->softflag & OB_SB_ENABLE && psys->soft) {
					BKE_ptcache_id_from_softbody(&pid, ob, psys->soft);

					for(i=sfra; i<=efra && cached; i++)
						cached &= BKE_ptcache_id_exist(&pid, i);
				}
			}
		}
		
		base = base->next;
	}

	return cached;
}
void inner_play_anim_loop(int init, int mode)
{
	ScrArea *sa;
	static int last_cfra = -1;
	static int cached = 0;

	/* init */
	if(init) {
		oldsa= curarea;
		swaptime= 1.0/FPS;
		tottime= 0.0;
		curmode= mode;
		last_cfra = -1;
		cached = cached_dynamics(PSFRA,PEFRA);
		
		redrawtime = 0.0;
		
		redrawtime_index = REDRAW_FRAME_AVERAGE;
		while(redrawtime_index--) {
			redrawtimes_fps[redrawtime_index] = 0.0;
		}
		redrawtime_index = 0;
		lredrawtime = 0.0;
		return;
	}

	if (CFRA != last_cfra) {
		int pf;
		set_timecursor(CFRA);
	
		update_for_newframe_nodraw(1);	/* adds no events in UI */

		sa= G.curscreen->areabase.first;
		while(sa) {
			if(sa==oldsa) {
				scrarea_do_windraw(sa);
			}
			else if(curmode & 1) { /* all view3d and seq spaces */
				if ELEM(sa->spacetype, SPACE_VIEW3D, SPACE_SEQ) {
					scrarea_do_windraw(sa);
				}
			}
			else if(curmode & 4) { /* all seq spaces */
				if (sa->spacetype == SPACE_SEQ) {
					scrarea_do_windraw(sa);
				}
			}		
		
			sa= sa->next;	
		}

		if (last_cfra == -1) {
			last_cfra = CFRA - 1;
		}
		
		if (U.prefetchframes) {
			pf = last_cfra;

			if (CFRA - last_cfra >= U.prefetchframes || 
			    CFRA - last_cfra < 0) {
				pf = CFRA - U.prefetchframes;
				fprintf(stderr, 
					"SEQ-THREAD: Lost sync, "
					"stopping threads, "
					"back to skip mode...\n");
				seq_stop_threads();
			} else {
				while (pf < CFRA) {
					int c;
					pf++;
					c = pf + U.prefetchframes;
					if (c >= PEFRA) {
						c -= PEFRA;
						c += PSFRA;
					}

					inner_play_prefetch_frame(curmode, c);
				}
			}
			
		}
	}

	last_cfra = CFRA;

	/* make sure that swaptime passed by */
	tottime -= swaptime;
	while (update_time(CFRA)) {
		PIL_sleep_ms(1);
	}
	
	if (CFRA >= PEFRA) {
		if (tottime > 0.0) {
			tottime = 0.0;
		}
		CFRA = PSFRA;
		audiostream_stop();
		audiostream_start( CFRA );
		cached = cached_dynamics(PSFRA,PEFRA);
	} else {
		if (cached
		    && (G.scene->audio.flag & AUDIO_SYNC)) {
			CFRA = audiostream_pos();
		} else {
			CFRA++;
		}
		if (CFRA < last_cfra) {
			fprintf(stderr, 
				"SEQ-THREAD: CFRA running backwards: %d\n",
				CFRA);
		}
	}

}

/* play_anim: 'mode' defines where to play and if repeat is on (now bitfield):
 * - mode & 1 : All view3d and seq areas
 * - mode & 2 : No replay 
 * - mode & 4 : All seq areas
 */
int play_anim(int mode)
{
	ScrArea *sa, *oldsa;
	int cfraont;
	unsigned short event=0;
	short val = 0; /* its possible qtest() wont run and val must be initialized */

	/* patch for very very old scenes */
	if(SFRA==0) SFRA= 1;
	if(EFRA==0) EFRA= 250;

	if(PSFRA>PEFRA) return 0;
	
	/* waitcursor(1); */
	G.f |= G_PLAYANIM;		/* in sequence.c and view.c this is handled */

	cfraont= CFRA;
	oldsa= curarea;

	if (curarea && curarea->spacetype == SPACE_SEQ) {
		SpaceSeq *sseq = curarea->spacedata.first;
		if (sseq->mainb == 0) mode |= 4;
	}

	inner_play_prefetch_startup(mode);

	update_time(CFRA);
	
	inner_play_anim_loop(1, mode);	/* 1==init */

	audiostream_start( CFRA );
	
	 /* forces all buffers to be OK for current frame (otherwise other windows get redrawn with CFRA+1) */
	curarea->win_swap= WIN_BACK_OK;
	screen_swapbuffers();

	while(TRUE) {
		
		if  (U.uiflag & USER_SHOW_FPS)
			lredrawtime = PIL_check_seconds_timer();
		
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
			} else if (event==WHEELDOWNMOUSE || (val && event==PADMINUS)) { /* copied from persptoetsen */
				if (G.vd) { /* when using the sequencer this can be NULL */
					/* this min and max is also in viewmove() */ 
					if(G.vd->persp==V3D_CAMOB) {
						G.vd->camzoom-= 10;
						if(G.vd->camzoom<-30) G.vd->camzoom= -30;
					}
					else if(G.vd->dist<10.0*G.vd->far) G.vd->dist*=1.2f;
				}
			} else if (event==WHEELUPMOUSE || (val && event==PADPLUSKEY)) { /* copied from persptoetsen */
				if (G.vd) {
					if(G.vd->persp==V3D_CAMOB) {
						G.vd->camzoom+= 10;
						if(G.vd->camzoom>300) G.vd->camzoom= 300;
					}
					else if(G.vd->dist> 0.001*G.vd->grid) G.vd->dist*=.83333f;
				}
			} else if(event==MKEY) {
				if(val) add_marker(CFRA-1);
			}
		}
		if(val && ELEM3(event, ESCKEY, SPACEKEY, RIGHTMOUSE)) break;
		
		inner_play_anim_loop(0, 0);
		 
		
		screen_swapbuffers();
		
		if (U.uiflag & USER_SHOW_FPS)
			redrawtime = lredrawtime;
		
		if((mode & 2) && CFRA==PEFRA) break; /* no replay */	
	}

	if(event==SPACEKEY);
	else CFRA= cfraont;
	
	inner_play_prefetch_shutdown(mode);
	audiostream_stop();

	if(oldsa!=curarea) areawinset(oldsa->win);
	
	/* restore all areas */
	sa= G.curscreen->areabase.first;
	while(sa) {
		if( ((mode & 1) && sa->spacetype==SPACE_VIEW3D) || sa==curarea) addqueue(sa->win, REDRAW, 1);
		sa= sa->next;	
	}
	
	/* groups could have changed ipo */
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWACTION, 0);
	
	/* restore for cfra */
	update_for_newframe_muted();

	waitcursor(0);
	G.f &= ~G_PLAYANIM;
	
	if (event==ESCKEY || event==SPACEKEY) return 1;
	else return 0;
}
