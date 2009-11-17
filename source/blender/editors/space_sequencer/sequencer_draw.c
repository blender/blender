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
 * Contributor(s): Blender Foundation, 2003-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "IMB_imbuf_types.h"

#include "DNA_gpencil_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_plugin_types.h"
#include "BKE_sequence.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
 
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_anim_api.h"
#include "ED_space_api.h"
#include "ED_sequencer.h"
#include "ED_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

/* own include */
#include "sequencer_intern.h"

#define SEQ_LEFTHANDLE		1
#define SEQ_RIGHTHANDLE	2


/* Note, Dont use WHILE_SEQ while drawing! - it messes up transform, - Campbell */

int no_rightbox=0, no_leftbox= 0;
static void draw_shadedstrip(Sequence *seq, char *col, float x1, float y1, float x2, float y2);


static void draw_cfra_seq(View2D *v2d, int cfra)
{
	glColor3ub(0x30, 0x90, 0x50);
	glLineWidth(2.0);
	glBegin(GL_LINES);
	glVertex2f(cfra, v2d->cur.ymin);
	glVertex2f(cfra, v2d->cur.ymax);
	glEnd();
	glLineWidth(1.0);
}

static void get_seq_color3ubv(Scene *curscene, Sequence *seq, char *col)
{
	char blendcol[3];
	float hsv[3], rgb[3];
	SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;

	switch(seq->type) {
	case SEQ_IMAGE:
		UI_GetThemeColor3ubv(TH_SEQ_IMAGE, col);
		break;
	case SEQ_META:
		UI_GetThemeColor3ubv(TH_SEQ_META, col);
		break;
	case SEQ_MOVIE:
		UI_GetThemeColor3ubv(TH_SEQ_MOVIE, col);
		break;
	case SEQ_SCENE:
		UI_GetThemeColor3ubv(TH_SEQ_SCENE, col);
		
		if(seq->scene==curscene) {
			UI_GetColorPtrBlendShade3ubv(col, col, col, 1.0, 20);
		}
		break;

	/* transitions */
	case SEQ_CROSS:
	case SEQ_GAMCROSS:
	case SEQ_WIPE:
		/* slightly offset hue to distinguish different effects */
		UI_GetThemeColor3ubv(TH_SEQ_TRANSITION, col);
		
		rgb[0] = col[0]/255.0; rgb[1] = col[1]/255.0; rgb[2] = col[2]/255.0; 
		rgb_to_hsv(rgb[0], rgb[1], rgb[2], hsv, hsv+1, hsv+2);
		
		if (seq->type == SEQ_CROSS)		hsv[0]+= 0.04;
		if (seq->type == SEQ_GAMCROSS)	hsv[0]+= 0.08;
		if (seq->type == SEQ_WIPE)		hsv[0]+= 0.12;
		
		if(hsv[0]>1.0) hsv[0]-=1.0; else if(hsv[0]<0.0) hsv[0]+= 1.0;
		hsv_to_rgb(hsv[0], hsv[1], hsv[2], rgb, rgb+1, rgb+2);
		col[0] = (char)(rgb[0]*255); col[1] = (char)(rgb[1]*255); col[2] = (char)(rgb[2]*255); 
		break;
		
	/* effects */
	case SEQ_TRANSFORM:
	case SEQ_SPEED:
	case SEQ_ADD:
	case SEQ_SUB:
	case SEQ_MUL:
	case SEQ_ALPHAOVER:
	case SEQ_ALPHAUNDER:
	case SEQ_OVERDROP:
	case SEQ_GLOW:
		/* slightly offset hue to distinguish different effects */
		UI_GetThemeColor3ubv(TH_SEQ_EFFECT, col);
		
		rgb[0] = col[0]/255.0; rgb[1] = col[1]/255.0; rgb[2] = col[2]/255.0; 
		rgb_to_hsv(rgb[0], rgb[1], rgb[2], hsv, hsv+1, hsv+2);
		
		if (seq->type == SEQ_ADD)		hsv[0]+= 0.04;
		if (seq->type == SEQ_SUB)		hsv[0]+= 0.08;
		if (seq->type == SEQ_MUL)		hsv[0]+= 0.12;
		if (seq->type == SEQ_ALPHAOVER)	hsv[0]+= 0.16;
		if (seq->type == SEQ_ALPHAUNDER)	hsv[0]+= 0.20;
		if (seq->type == SEQ_OVERDROP)	hsv[0]+= 0.24;
		if (seq->type == SEQ_GLOW)		hsv[0]+= 0.28;
		if (seq->type == SEQ_TRANSFORM)		hsv[0]+= 0.36;

		if(hsv[0]>1.0) hsv[0]-=1.0; else if(hsv[0]<0.0) hsv[0]+= 1.0;
		hsv_to_rgb(hsv[0], hsv[1], hsv[2], rgb, rgb+1, rgb+2);
		col[0] = (char)(rgb[0]*255); col[1] = (char)(rgb[1]*255); col[2] = (char)(rgb[2]*255); 
		break;
	case SEQ_COLOR:
		if (colvars->col) {
			col[0]= (char)(colvars->col[0]*255);
			col[1]= (char)(colvars->col[1]*255);
			col[2]= (char)(colvars->col[2]*255);
		} else {
			col[0] = col[1] = col[2] = 128;
		}
		break;
	case SEQ_PLUGIN:
		UI_GetThemeColor3ubv(TH_SEQ_PLUGIN, col);
		break;
	case SEQ_SOUND:
		UI_GetThemeColor3ubv(TH_SEQ_AUDIO, col);
		blendcol[0] = blendcol[1] = blendcol[2] = 128;
		if(seq->flag & SEQ_MUTE) UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.5, 20);
		break;
	default:
		col[0] = 10; col[1] = 255; col[2] = 40;
	}
}

static void drawmeta_contents(Scene *scene, Sequence *seqm, float x1, float y1, float x2, float y2)
{
	/* Note, this used to use WHILE_SEQ, but it messes up the seq->depth value, (needed by transform when doing overlap checks)
	 * so for now, just use the meta's immediate children, could be fixed but its only drawing - Campbell */
	Sequence *seq;
	float dx;
	int nr;
	char col[3];
	
	nr= BLI_countlist(&seqm->seqbase);

	dx= (x2-x1)/nr;

	if (seqm->flag & SEQ_MUTE) {
		glEnable(GL_POLYGON_STIPPLE);
		glPolygonStipple(stipple_halftone);
		
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(1, 0x8888);
	}
	
	for (seq= seqm->seqbase.first; seq; seq= seq->next) {
		get_seq_color3ubv(scene, seq, col);
		
		glColor3ubv((GLubyte *)col);

		glRectf(x1,  y1,  x1+0.9*dx,  y2);
		
		UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -30);
		glColor3ubv((GLubyte *)col);

		fdrawbox(x1,  y1,  x1+0.9*dx,  y2);
		
		x1+= dx;
	}
	
	if (seqm->flag & SEQ_MUTE) {
		glDisable(GL_POLYGON_STIPPLE);
		glDisable(GL_LINE_STIPPLE);
	}
}

/* draw a handle, for each end of a sequence strip */
static void draw_seq_handle(View2D *v2d, Sequence *seq, float pixelx, short direction)
{
	float v1[2], v2[2], v3[2], rx1=0, rx2=0; //for triangles and rect
	float x1, x2, y1, y2;
	float handsize;
	float minhandle, maxhandle;
	char str[32];
	unsigned int whichsel=0;
	
	x1= seq->startdisp;
	x2= seq->enddisp;
	
	y1= seq->machine+SEQ_STRIP_OFSBOTTOM;
	y2= seq->machine+SEQ_STRIP_OFSTOP;
	
	/* clamp handles to defined size in pixel space */
	handsize = seq->handsize;
	minhandle = 7;
	maxhandle = 40;
	CLAMP(handsize, minhandle*pixelx, maxhandle*pixelx);
	
	/* set up co-ordinates/dimensions for either left or right handle */
	if (direction == SEQ_LEFTHANDLE) {	
		rx1 = x1;
		rx2 = x1+handsize*0.75;
		
		v1[0]= x1+handsize/4; v1[1]= y1+( ((y1+y2)/2.0 - y1)/2);
		v2[0]= x1+handsize/4; v2[1]= y2-( ((y1+y2)/2.0 - y1)/2);
		v3[0]= v2[0] + handsize/4; v3[1]= (y1+y2)/2.0;
		
		whichsel = SEQ_LEFTSEL;
	} else if (direction == SEQ_RIGHTHANDLE) {	
		rx1 = x2-handsize*0.75;
		rx2 = x2;
		
		v1[0]= x2-handsize/4; v1[1]= y1+( ((y1+y2)/2.0 - y1)/2);
		v2[0]= x2-handsize/4; v2[1]= y2-( ((y1+y2)/2.0 - y1)/2);
		v3[0]= v2[0] - handsize/4; v3[1]= (y1+y2)/2.0;
		
		whichsel = SEQ_RIGHTSEL;
	}
	
	/* draw! */
	if(seq->type < SEQ_EFFECT || 
	   get_sequence_effect_num_inputs(seq->type) == 0) {
		glEnable( GL_BLEND );
		
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		if(seq->flag & whichsel) glColor4ub(0, 0, 0, 80);
		else if (seq->flag & SELECT) glColor4ub(255, 255, 255, 30);
		else glColor4ub(0, 0, 0, 22);
		
		glRectf(rx1, y1, rx2, y2);
		
		if(seq->flag & whichsel) glColor4ub(255, 255, 255, 200);
		else glColor4ub(0, 0, 0, 50);
		
		glEnable( GL_POLYGON_SMOOTH );
		glBegin(GL_TRIANGLES);
		glVertex2fv(v1); glVertex2fv(v2); glVertex2fv(v3);
		glEnd();
		
		glDisable( GL_POLYGON_SMOOTH );
		glDisable( GL_BLEND );
	}
	
	if(G.moving || (seq->flag & whichsel)) {
		cpack(0xFFFFFF);
		if (direction == SEQ_LEFTHANDLE) {
			sprintf(str, "%d", seq->startdisp);
			x1= rx1;
			y1 -= 0.45;
		} else {
			sprintf(str, "%d", seq->enddisp - 1);
			x1= x2 - handsize*0.75;
			y1= y2 + 0.05;
		}
		UI_view2d_text_cache_add(v2d, x1, y1, str);
	}	
}

static void draw_seq_extensions(Scene *scene, SpaceSeq *sseq, Sequence *seq)
{
	float x1, x2, y1, y2, pixely, a;
	char col[3], blendcol[3];
	View2D *v2d;
	
	if(seq->type >= SEQ_EFFECT) return;

	x1= seq->startdisp;
	x2= seq->enddisp;
	
	y1= seq->machine+SEQ_STRIP_OFSBOTTOM;
	y2= seq->machine+SEQ_STRIP_OFSTOP;
	
	v2d = &sseq->v2d;
	pixely = (v2d->cur.ymax - v2d->cur.ymin)/(v2d->mask.ymax - v2d->mask.ymin);
	
	blendcol[0] = blendcol[1] = blendcol[2] = 120;

	if(seq->startofs) {
		glEnable( GL_BLEND );
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		get_seq_color3ubv(scene, seq, col);
		
		if (seq->flag & SELECT) {
			UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.3, -40);
			glColor4ub(col[0], col[1], col[2], 170);
		} else {
			UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.6, 0);
			glColor4ub(col[0], col[1], col[2], 110);
		}
		
		glRectf((float)(seq->start), y1-SEQ_STRIP_OFSBOTTOM, x1, y1);
		
		if (seq->flag & SELECT) glColor4ub(col[0], col[1], col[2], 255);
		else glColor4ub(col[0], col[1], col[2], 160);

		fdrawbox((float)(seq->start), y1-SEQ_STRIP_OFSBOTTOM, x1, y1);	//outline
		
		glDisable( GL_BLEND );
	}
	if(seq->endofs) {
		glEnable( GL_BLEND );
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		get_seq_color3ubv(scene, seq, col);
		
		if (seq->flag & SELECT) {
			UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.3, -40);
			glColor4ub(col[0], col[1], col[2], 170);
		} else {
			UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.6, 0);
			glColor4ub(col[0], col[1], col[2], 110);
		}
		
		glRectf(x2, y2, (float)(seq->start+seq->len), y2+SEQ_STRIP_OFSBOTTOM);
		
		if (seq->flag & SELECT) glColor4ub(col[0], col[1], col[2], 255);
		else glColor4ub(col[0], col[1], col[2], 160);

		fdrawbox(x2, y2, (float)(seq->start+seq->len), y2+SEQ_STRIP_OFSBOTTOM);	//outline
		
		glDisable( GL_BLEND );
	}
	if(seq->startstill) {
		get_seq_color3ubv(scene, seq, col);
		UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.75, 40);
		glColor3ubv((GLubyte *)col);
		
		draw_shadedstrip(seq, col, x1, y1, (float)(seq->start), y2);
		
		/* feint pinstripes, helps see exactly which is extended and which isn't,
		* especially when the extension is very small */ 
		if (seq->flag & SELECT) UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 24);
		else UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -16);
		
		glColor3ubv((GLubyte *)col);
		
		for(a=y1; a< y2; a+= pixely*2.0 ) {
			fdrawline(x1,  a,  (float)(seq->start),  a);
		}
	}
	if(seq->endstill) {
		get_seq_color3ubv(scene, seq, col);
		UI_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.75, 40);
		glColor3ubv((GLubyte *)col);
		
		draw_shadedstrip(seq, col, (float)(seq->start+seq->len), y1, x2, y2);
		
		/* feint pinstripes, helps see exactly which is extended and which isn't,
		* especially when the extension is very small */ 
		if (seq->flag & SELECT) UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 24);
		else UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -16);
		
		glColor3ubv((GLubyte *)col);
		
		for(a=y1; a< y2; a+= pixely*2.0 ) {
			fdrawline((float)(seq->start+seq->len),  a,  x2,  a);
		}
	}
}

/* draw info text on a sequence strip */
static void draw_seq_text(View2D *v2d, Sequence *seq, float x1, float x2, float y1, float y2, char *background_col)
{
	rctf rect;
	char str[32 + FILE_MAXDIR+FILE_MAXFILE];
	
	if(seq->name[2]) {
		sprintf(str, "%d | %s: %s", seq->len, give_seqname(seq), seq->name+2);
	}
	else{
		if(seq->type == SEQ_META) {
			sprintf(str, "%d | %s", seq->len, give_seqname(seq));
		}
		else if(seq->type == SEQ_SCENE) {
			if(seq->scene) sprintf(str, "%d | %s: %s", seq->len, give_seqname(seq), seq->scene->id.name+2);
			else sprintf(str, "%d | %s", seq->len, give_seqname(seq));
			
		}
		else if(seq->type == SEQ_IMAGE) {
			sprintf(str, "%d | %s%s", seq->len, seq->strip->dir, seq->strip->stripdata->name);
		}
		else if(seq->type & SEQ_EFFECT) {
			int can_float = (seq->type != SEQ_PLUGIN)
				|| (seq->plugin && seq->plugin->version >= 4);

			if(seq->seq3!=seq->seq2 && seq->seq1!=seq->seq3)
				sprintf(str, "%d | %s: %d>%d (use %d)%s", seq->len, give_seqname(seq), seq->seq1->machine, seq->seq2->machine, seq->seq3->machine, can_float ? "" : " No float, upgrade plugin!");
			else if (seq->seq1 && seq->seq2)
				sprintf(str, "%d | %s: %d>%d%s", seq->len, give_seqname(seq), seq->seq1->machine, seq->seq2->machine, can_float ? "" : " No float, upgrade plugin!");
			else 
				sprintf(str, "%d | %s", seq->len, give_seqname(seq));
		}
		else if (seq->type == SEQ_SOUND) {
			sprintf(str, "%d | %s", seq->len, seq->sound->name);
		}
		else if (seq->type == SEQ_MOVIE) {
			sprintf(str, "%d | %s%s", seq->len, seq->strip->dir, seq->strip->stripdata->name);
		}
	}
	
	if(seq->flag & SELECT){
		cpack(0xFFFFFF);
	}else if ((((int)background_col[0] + (int)background_col[1] + (int)background_col[2]) / 3) < 50){
		cpack(0x505050); /* use lighter text colour for dark background */
	}else{
		cpack(0);
	}
	
	rect.xmin= x1;
	rect.ymin= y1;
	rect.xmax= x2;
	rect.ymax= y2;
	UI_view2d_text_cache_rectf(v2d, &rect, str);
}

/* draws a shaded strip, made from gradient + flat color + gradient */
static void draw_shadedstrip(Sequence *seq, char *col, float x1, float y1, float x2, float y2)
{
	float ymid1, ymid2;
	
	if (seq->flag & SEQ_MUTE) {
		glEnable(GL_POLYGON_STIPPLE);
		glPolygonStipple(stipple_halftone);
	}
	
	ymid1 = (y2-y1)*0.25 + y1;
	ymid2 = (y2-y1)*0.65 + y1;
	
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);
	
	if(seq->flag & SELECT) UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -50);
	else UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 0);
	
	glColor3ubv((GLubyte *)col);
	
	glVertex2f(x1,y1);
	glVertex2f(x2,y1);
	
	if(seq->flag & SELECT) UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 5);
	else UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -5);

	glColor3ubv((GLubyte *)col);
	
	glVertex2f(x2,ymid1);
	glVertex2f(x1,ymid1);
	
	glEnd();
	
	glRectf(x1,  ymid1,  x2,  ymid2);
	
	glBegin(GL_QUADS);
	
	glVertex2f(x1,ymid2);
	glVertex2f(x2,ymid2);
	
	if(seq->flag & SELECT) UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -15);
	else UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 25);
	
	glColor3ubv((GLubyte *)col);
	
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);
	
	glEnd();
	
	if (seq->flag & SEQ_MUTE) {
		glDisable(GL_POLYGON_STIPPLE);
	}
}

/*
Draw a sequence strip, bounds check alredy made
ARegion is currently only used to get the windows width in pixels
so wave file sample drawing precission is zoom adjusted
*/
static void draw_seq_strip(Scene *scene, ARegion *ar, SpaceSeq *sseq, Sequence *seq, int outline_tint, float pixelx)
{
	// XXX
	extern void gl_round_box_shade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadetop, float shadedown);
	View2D *v2d= &ar->v2d;
	float x1, x2, y1, y2;
	char col[3], background_col[3], is_single_image;

	/* we need to know if this is a single image/color or not for drawing */
	is_single_image = (char)check_single_seq(seq);
	
	/* body */
	if(seq->startstill) x1= seq->start;
	else x1= seq->startdisp;
	y1= seq->machine+SEQ_STRIP_OFSBOTTOM;
	if(seq->endstill) x2= seq->start+seq->len;
	else x2= seq->enddisp;
	y2= seq->machine+SEQ_STRIP_OFSTOP;
	
	
	/* get the correct color per strip type*/
	//get_seq_color3ubv(scene, seq, col);
	get_seq_color3ubv(scene, seq, background_col);
	
	/* draw the main strip body */
	if (is_single_image) /* single image */
		draw_shadedstrip(seq, background_col, seq_tx_get_final_left(seq, 0), y1, seq_tx_get_final_right(seq, 0), y2);
	else /* normal operation */
		draw_shadedstrip(seq, background_col, x1, y1, x2, y2);
	
	/* draw additional info and controls */
	if (!is_single_image)
		draw_seq_extensions(scene, sseq, seq);
	
	draw_seq_handle(v2d, seq, pixelx, SEQ_LEFTHANDLE);
	draw_seq_handle(v2d, seq, pixelx, SEQ_RIGHTHANDLE);
	
	/* draw the strip outline */
	x1= seq->startdisp;
	x2= seq->enddisp;
	
	get_seq_color3ubv(scene, seq, col);
	if (G.moving && (seq->flag & SELECT)) {
		if(seq->flag & SEQ_OVERLAP) {
			col[0]= 255; col[1]= col[2]= 40;
		} else UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 120);
	}

	UI_GetColorPtrBlendShade3ubv(col, col, col, 0.0, outline_tint);
	
	glColor3ubv((GLubyte *)col);
	
	if (seq->flag & SEQ_MUTE) {
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(1, 0x8888);
	}
	
	gl_round_box_shade(GL_LINE_LOOP, x1, y1, x2, y2, 0.0, 0.1, 0.0);
	
	if (seq->flag & SEQ_MUTE) {
		glDisable(GL_LINE_STIPPLE);
	}
	
	/* calculate if seq is long enough to print a name */
	x1= seq->startdisp+seq->handsize;
	x2= seq->enddisp-seq->handsize;

	/* but first the contents of a meta */
	if(seq->type==SEQ_META) drawmeta_contents(scene, seq, x1, y1+0.15, x2, y2-0.15);

	/* info text on the strip */
	if(x1<v2d->cur.xmin) x1= v2d->cur.xmin;
	else if(x1>v2d->cur.xmax) x1= v2d->cur.xmax;
	if(x2<v2d->cur.xmin) x2= v2d->cur.xmin;
	else if(x2>v2d->cur.xmax) x2= v2d->cur.xmax;

	/* nice text here would require changing the view matrix for texture text */
	if( (x2-x1) / pixelx > 32) {
		draw_seq_text(v2d, seq, x1, x2, y1, y2, background_col);
	}
}

static Sequence *special_seq_update= 0;

void set_special_seq_update(int val)
{
//	int x;

	/* if mouse over a sequence && LEFTMOUSE */
	if(val) {
// XXX		special_seq_update= find_nearest_seq(&x);
	}
	else special_seq_update= 0;
}


static void draw_image_seq(Scene *scene, ARegion *ar, SpaceSeq *sseq)
{
	extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);
	struct ImBuf *ibuf;
	int x1, y1, rectx, recty;
	int free_ibuf = 0;
	static int recursive= 0;
	float zoom;
	float zoomx, zoomy;
	float render_size = 0.0;
	float proxy_size = 100.0;

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	render_size = sseq->render_size;
	if (render_size == 0) {
		render_size = scene->r.size;
	} else {
		proxy_size = render_size;
	}
	if (render_size < 0) {
		return;
	}

	rectx= (render_size*scene->r.xsch)/100;
	recty= (render_size*scene->r.ysch)/100;

	/* BIG PROBLEM: the give_ibuf_seq() can call a rendering, which in turn calls redraws...
	   this shouldn't belong in a window drawing....
	   So: solve this once event based. 
	   Now we check for recursion, space type and active area again (ton) */
	
	if(recursive)
		return;
	else {
		recursive= 1;
		if (special_seq_update) {
			ibuf= give_ibuf_seq_direct(scene, rectx, recty, (scene->r.cfra), proxy_size, special_seq_update);
		} 
		else if (!U.prefetchframes) { // XXX || (G.f & G_PLAYANIM) == 0) {
			ibuf= (ImBuf *)give_ibuf_seq(scene, rectx, recty, (scene->r.cfra), sseq->chanshown, proxy_size);
		} 
		else {
			ibuf= (ImBuf *)give_ibuf_seq_threaded(scene, rectx, recty, (scene->r.cfra), sseq->chanshown, proxy_size);
		}
		recursive= 0;
		
		/* XXX HURMF! the give_ibuf_seq can call image display in this window */
//		if(sa->spacetype!=SPACE_SEQ)
//			return;
//		if(sa!=curarea) {
//			areawinset(sa->win);
//		}
	}
	
	if(ibuf==NULL) 
		return;

	if(ibuf->rect==NULL && ibuf->rect_float == NULL) 
		return;
	
	switch(sseq->mainb) {
	case SEQ_DRAW_IMG_IMBUF:
		if (sseq->zebra != 0) {
			ibuf = make_zebra_view_from_ibuf(ibuf, sseq->zebra);
			free_ibuf = 1;
		}
		break;
	case SEQ_DRAW_IMG_WAVEFORM:
		if ((sseq->flag & SEQ_DRAW_COLOR_SEPERATED) != 0) {
			ibuf = make_sep_waveform_view_from_ibuf(ibuf);
		} else {
			ibuf = make_waveform_view_from_ibuf(ibuf);
		}
		free_ibuf = 1;
		break;
	case SEQ_DRAW_IMG_VECTORSCOPE:
		ibuf = make_vectorscope_view_from_ibuf(ibuf);
		free_ibuf = 1;
		break;
	case SEQ_DRAW_IMG_HISTOGRAM:
		ibuf = make_histogram_view_from_ibuf(ibuf);
		free_ibuf = 1;
		break;
	}

	if(ibuf->rect_float && ibuf->rect==NULL)
		IMB_rect_from_float(ibuf);
	
	/* needed for gla draw */
	glaDefine2DArea(&ar->winrct);
	
	zoom= SEQ_ZOOM_FAC(sseq->zoom);
	if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
		zoom /= proxy_size / 100.0;
		zoomx = zoom * ((float)scene->r.xasp / (float)scene->r.yasp);
		zoomy = zoom;
	} else {
		zoomx = zoomy = zoom;
	}

	/* calc location */
	x1= (ar->winx-zoomx*ibuf->x)/2 + sseq->xof;
	y1= (ar->winy-zoomy*ibuf->y)/2 + sseq->yof;
	
	glPixelZoom(zoomx, zoomy);
	
	glaDrawPixelsSafe(x1, y1, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
	
	glPixelZoom(1.0, 1.0);

	/* safety border */
	if (sseq->mainb == SEQ_DRAW_IMG_IMBUF && 
	    (sseq->flag & SEQ_DRAW_SAFE_MARGINS) != 0) {
		float fac= 0.1;
		float x2 = x1 + ibuf->x * zoomx;
		float y2 = y1 + ibuf->y * zoomy;
		
		float a= fac*(x2-x1);
		x1+= a; 
		x2-= a;
	
		a= fac*(y2-y1);
		y1+= a;
		y2-= a;
	
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); 
		setlinestyle(3);

		UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 1.0, 0);
		
		uiSetRoundBox(15);
		gl_round_box(GL_LINE_LOOP, x1, y1, x2, y2, 12.0);

		setlinestyle(0);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	
	/* draw grease-pencil (image aligned) */
//	if (sseq->flag & SEQ_DRAW_GPENCIL)
// XXX		draw_gpencil_2dimage(sa, ibuf);

	if (free_ibuf) {
		IMB_freeImBuf(ibuf);
	} 
	
	/* draw grease-pencil (screen aligned) */
//	if (sseq->flag & SEQ_DRAW_GPENCIL)
// XXX		draw_gpencil_2dview(sa, 0);
	
	/* ortho at pixel level sa */
// XXX	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);
	
}

void seq_reset_imageofs(SpaceSeq *sseq)
{
	sseq->xof = sseq->yof = sseq->zoom = 0;
}


#if 0
/* XXX - these should really be made to use View2D instead of so wacko private system - Aligorith */

void seq_viewzoom(SpaceSeq *sseq, unsigned short event, int invert)
{

	if(event==PAD1)
		sseq->zoom= 1.0;
	else if(event==PAD2)
		sseq->zoom= (invert)? 2.0: 0.5;
	else if(event==PAD4)
		sseq->zoom= (invert)? 4.0: 0.25;
	else if(event==PAD8)
		sseq->zoom= (invert)? 8.0: 0.125;
	
	/* ensure pixel exact locations for draw */
	sseq->xof= (int)sseq->xof;
	sseq->yof= (int)sseq->yof;
}

void seq_viewmove(Scene *scene, ARegion *ar, SpaceSeq *sseq)
{	
	short mval[2], mvalo[2];
	short rectx, recty, xmin, xmax, ymin, ymax, pad;
	int oldcursor;
	Window *win;
	
	sa = sseq->area;
	rectx= (scene->r.size*scene->r.xsch)/100;
	recty= (scene->r.size*scene->r.ysch)/100;
	
	pad = 10;
	xmin = -(ar->winx/2) - rectx/2 + pad;
	xmax = ar->winx/2 + rectx/2 - pad;
	ymin = -(ar->winy/2) - recty/2 + pad;
	ymax = ar->winy/2 + recty/2 - pad;
	
	getmouseco_sc(mvalo);

	oldcursor=get_cursor();
	win=winlay_get_active_window();
	
	SetBlenderCursor(BC_NSEW_SCROLLCURSOR);
	
	while(get_mbut()&(L_MOUSE|M_MOUSE)) {
		
		getmouseco_sc(mval);
		
		if(mvalo[0]!=mval[0] || mvalo[1]!=mval[1]) {

			sseq->xof -= (mvalo[0]-mval[0]);
			sseq->yof -= (mvalo[1]-mval[1]);
			
			/* prevent dragging image outside of the window and losing it! */
			CLAMP(sseq->xof, xmin, xmax);
			CLAMP(sseq->yof, ymin, ymax);
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
		}
	}
}
#endif

void drawprefetchseqspace(Scene *scene, ARegion *ar, SpaceSeq *sseq)
{
	int rectx, recty;
	int render_size = sseq->render_size;
	int proxy_size = 100.0; 
	if (render_size == 0) {
		render_size = scene->r.size;
	} else {
		proxy_size = render_size;
	}
	if (render_size < 0) {
		return;
	}

	rectx= (render_size*scene->r.xsch)/100;
	recty= (render_size*scene->r.ysch)/100;

	if(sseq->mainb != SEQ_DRAW_SEQUENCE) {
		give_ibuf_prefetch_request(
			rectx, recty, (scene->r.cfra), sseq->chanshown,
			proxy_size);
	}
}

void drawseqspace(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	Editing *ed= seq_give_editing(scene, FALSE);
	Sequence *seq;
	float col[3];
	int i;

	if(sseq->mainb != SEQ_DRAW_SEQUENCE) {
		draw_image_seq(scene, ar, sseq);
		return;
	}

	UI_GetThemeColor3fv(TH_BACK, col);
	if(ed && ed->metastack.first) glClearColor(col[0], col[1], col[2]-0.1, 0.0);
	else glClearColor(col[0], col[1], col[2], 0.0);

	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(C, v2d);
	
	UI_ThemeColorShade(TH_BACK, -20);
	glRectf(v2d->cur.xmin,  0.0,  v2d->cur.xmax,  1.0);

	boundbox_seq(scene, &v2d->tot);
	
	/* Alternating horizontal stripes */
	i= MAX2(1, ((int)v2d->cur.ymin)-1);

	glBegin(GL_QUADS);
	while (i<v2d->cur.ymax) {
		if (((int)i) & 1)
			UI_ThemeColorShade(TH_BACK, -15);
		else
			UI_ThemeColorShade(TH_BACK, -25);
		
		glVertex2f(v2d->cur.xmax, i);
		glVertex2f(v2d->cur.xmin, i);
		glVertex2f(v2d->cur.xmin, i+1);
		glVertex2f(v2d->cur.xmax, i+1);
		i+=1.0;
	}
	glEnd();
	
	/* Force grid lines */
	i= MAX2(1, ((int)v2d->cur.ymin)-1);
	glBegin(GL_LINES);

	while (i<v2d->cur.ymax) {
		UI_ThemeColor(TH_GRID);
		glVertex2f(v2d->cur.xmax, i);
		glVertex2f(v2d->cur.xmin, i);
		i+=1.0;
	}
	glEnd();
	
	UI_view2d_constant_grid_draw(C, v2d);

	draw_cfra_seq(v2d, scene->r.cfra);

	/* sequences: first deselect */
	if(ed) {
		Sequence *last_seq = active_seq_get(scene);
		int sel = 0, j;
		int outline_tint;
		float pixelx = (v2d->cur.xmax - v2d->cur.xmin)/(v2d->mask.xmax - v2d->mask.xmin);
		/* loop through twice, first unselected, then selected */
		for (j=0; j<2; j++) {
			seq= ed->seqbasep->first;
			if (j==0)	outline_tint = -150;
			else		outline_tint = -60;
			
			while(seq) { /* bound box test, dont draw outside the view */
				if (  ((seq->flag & SELECT) == sel) ||
						seq == last_seq ||
						MIN2(seq->startdisp, seq->start) > v2d->cur.xmax ||
						MAX2(seq->enddisp, seq->start+seq->len) < v2d->cur.xmin ||
						seq->machine+1.0 < v2d->cur.ymin ||
						seq->machine > v2d->cur.ymax)
				{
					/* dont draw */
				} else {
					draw_seq_strip(scene, ar, sseq, seq, outline_tint, pixelx);
				}
				seq= seq->next;
			}
			sel= SELECT; /* draw selected next time round */
		}
		/* draw the last selected last, removes some overlapping error */
		if (last_seq) {
			draw_seq_strip(scene, ar, sseq, last_seq, 120, pixelx);
		}
	}

	/* text draw cached, in pixelspace now */
	UI_view2d_text_cache_draw(ar);

	/* Draw markers */
//	draw_markers_timespace(SCE_MARKERS, DRAW_MARKERS_LINES);
	
	/* preview range */
	UI_view2d_view_ortho(C, v2d);
	ANIM_draw_previewrange(C, v2d);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_UNIT_SECONDSSEQ, V2D_GRID_CLAMP, V2D_UNIT_VALUES, V2D_GRID_CLAMP);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}


