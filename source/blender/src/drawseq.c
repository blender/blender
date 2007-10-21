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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "IMB_imbuf_types.h"

#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_global.h"
#include "BKE_plugin_types.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
 
#include "BIF_cursors.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_drawseq.h"
#include "BIF_editseq.h"
#include "BIF_glutil.h"
#include "BIF_resources.h"
#include "BIF_space.h"
#include "BIF_interface.h"

#include "BSE_view.h"
#include "BSE_drawipo.h"
#include "BSE_sequence.h"
#include "BSE_seqeffects.h"
#include "BSE_seqscopes.h"
#include "BSE_seqaudio.h"
#include "BSE_time.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "blendef.h"	/* CFRA */
#include "mydevice.h"	/* REDRAWSEQ */
#include "interface.h"
#include "winlay.h"

#define SEQ_LEFTHANDLE		1
#define SEQ_RIGHTHANDLE	2

#define SEQ_STRIP_OFSBOTTOM		0.2
#define SEQ_STRIP_OFSTOP		0.8

int no_rightbox=0, no_leftbox= 0;
static void draw_seq_handle(Sequence *seq, SpaceSeq *sseq, short direction);
static void draw_seq_extensions(Sequence *seq, SpaceSeq *sseq);
static void draw_seq_text(Sequence *seq, float x1, float x2, float y1, float y2);
static void draw_shadedstrip(Sequence *seq, char *col, float x1, float y1, float x2, float y2);
static void draw_seq_strip(struct Sequence *seq, struct ScrArea *sa, struct SpaceSeq *sseq);

static char *give_seqname(Sequence *seq)
{
	if(seq->type==SEQ_META) return "Meta";
	else if(seq->type==SEQ_IMAGE) return "Image";
	else if(seq->type==SEQ_SCENE) return "Scene";
	else if(seq->type==SEQ_MOVIE) return "Movie";
	else if(seq->type==SEQ_RAM_SOUND) return "Audio (RAM)";
	else if(seq->type==SEQ_HD_SOUND) return "Audio (HD)";
	else if(seq->type<SEQ_EFFECT) return seq->strip->dir;
	else if(seq->type==SEQ_CROSS) return "Cross";
	else if(seq->type==SEQ_GAMCROSS) return "Gamma Cross";
	else if(seq->type==SEQ_ADD) return "Add";
	else if(seq->type==SEQ_SUB) return "Sub";
	else if(seq->type==SEQ_MUL) return "Mul";
	else if(seq->type==SEQ_ALPHAOVER) return "Alpha Over";
	else if(seq->type==SEQ_ALPHAUNDER) return "Alpha Under";
	else if(seq->type==SEQ_OVERDROP) return "Over Drop";
	else if(seq->type==SEQ_WIPE) return "Wipe";
	else if(seq->type==SEQ_GLOW) return "Glow";
	else if(seq->type==SEQ_TRANSFORM) return "Transform";
	else if(seq->type==SEQ_COLOR) return "Color";
	else if(seq->type==SEQ_SPEED) return "Speed";
	else if(seq->type==SEQ_PLUGIN) {
		if(!(seq->flag & SEQ_EFFECT_NOT_LOADED) &&
		   seq->plugin && seq->plugin->doit) return seq->plugin->pname;
		return "Plugin";
	}
	else return "Effect";

}
static void draw_cfra_seq(void)
{
	glColor3ub(0x30, 0x90, 0x50);
	glLineWidth(2.0);
	glBegin(GL_LINES);
	glVertex2f(G.scene->r.cfra, G.v2d->cur.ymin);
	glVertex2f(G.scene->r.cfra, G.v2d->cur.ymax);
	glEnd();
	glLineWidth(1.0);
}

static void get_seq_color3ubv(Sequence *seq, char *col)
{
	char blendcol[3];
	float hsv[3], rgb[3];
	SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;

	switch(seq->type) {
	case SEQ_IMAGE:
		BIF_GetThemeColor3ubv(TH_SEQ_IMAGE, col);
		break;
	case SEQ_META:
		BIF_GetThemeColor3ubv(TH_SEQ_META, col);
		break;
	case SEQ_MOVIE:
		BIF_GetThemeColor3ubv(TH_SEQ_MOVIE, col);
		break;
	case SEQ_SCENE:
		BIF_GetThemeColor3ubv(TH_SEQ_SCENE, col);
		
		if(seq->scene==G.scene) {
			BIF_GetColorPtrBlendShade3ubv(col, col, col, 1.0, 20);
		}
		break;

	/* transitions */
	case SEQ_CROSS:
	case SEQ_GAMCROSS:
	case SEQ_WIPE:
		/* slightly offset hue to distinguish different effects */
		BIF_GetThemeColor3ubv(TH_SEQ_TRANSITION, col);
		
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
		BIF_GetThemeColor3ubv(TH_SEQ_EFFECT, col);
		
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
		BIF_GetThemeColor3ubv(TH_SEQ_PLUGIN, col);
		break;
	case SEQ_HD_SOUND:
	case SEQ_RAM_SOUND:
		BIF_GetThemeColor3ubv(TH_SEQ_AUDIO, col);
		blendcol[0] = blendcol[1] = blendcol[2] = 128;
		if(seq->flag & SEQ_MUTE) BIF_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.5, 20);
		break;
	default:
		col[0] = 10; col[1] = 255; col[2] = 40;
	}
}

static void drawmeta_contents(Sequence *seqm, float x1, float y1, float x2, float y2)
{
	Sequence *seq;
	float dx;
	int nr;
	char col[3];
	
	nr= 0;
	WHILE_SEQ(&seqm->seqbase) {
		nr++;
	}
	END_SEQ

	dx= (x2-x1)/nr;

	WHILE_SEQ(&seqm->seqbase) {
		get_seq_color3ubv(seq, col);
		
		glColor3ubv((GLubyte *)col);

		glRectf(x1,  y1,  x1+0.9*dx,  y2);
		
		BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -30);
		glColor3ubv((GLubyte *)col);

		fdrawbox(x1,  y1,  x1+0.9*dx,  y2);
		
		x1+= dx;
	}
	END_SEQ
}

static void drawseqwave(Sequence *seq, float x1, float y1, float x2, float y2, int winx)
{
	/*
	x1 is the starting x value to draw the wave,
	x2 the end x value, same for y1 and y2
	winx is the zoom level.
	*/
	
	float
	f, /* floating point value used to store the X draw location for the wave lines when openGL drawing*/
	midy, /* fast access to the middle location (y1+y2)/2 */
	clipxmin, /* the minimum X value, clip this with the window */
	clipxmax, /* the maximum X value, clip this with the window */
	sample_step, /* steps to move per sample, floating value must later translate into an int */
	fsofs, /* steps to move per sample, floating value must later translate into an int */
	feofs_sofs, /*  */
	sound_width, /* convenience: x2-x1 */
	wavemulti; /* scale the samples by this value when GL_LINE drawing so it renders the right height */
	
	int
	offset, /* initial offset value for the wave drawing */
	offset_next, /* when in the wave drawing loop this value is the samples intil the next vert */
	sofs, /* Constrained offset value (~3) for the wave, start */
	eofs, /* ditto, end */
	wavesample, /* inner loop storage if the current wave sample value, used to make the 2 values below */
	wavesamplemin, /* used for finding the min and max wave peaks */
	wavesamplemax, /* ditto */
	subsample_step=4; /* when the sample step is 4 every sample of
	the wave is evaluated for min and max values used to draw the wave,
	however this is slow ehrn zoomed out so when the sample step is above
	1 (the larger the further out the zoom is) so not evaluate all samples, only some. */
	
	signed short* s;
	bSound *sound;
	Uint8 *stream;
	
	audio_makestream(seq->sound);
	if(seq->sound==NULL || seq->sound->stream==NULL) return;
	
	if (seq->flag & SEQ_MUTE) glColor3ub(0x70, 0x80, 0x80); else glColor3ub(0x70, 0xc0, 0xc0);
	
	sofs = ((int)( FRA2TIME(seq->startdisp-seq->start)*(float)G.scene->audio.mixrate*4.0 )) & (~3);
	eofs = ((int)( FRA2TIME(seq->enddisp-seq->start)*(float)G.scene->audio.mixrate*4.0 )) & (~3);
	
	/* clip the drawing area to the screen bounds to save time */
	sample_step= (G.v2d->cur.xmax - G.v2d->cur.xmin)/winx;
	clipxmin= MAX2(x1, G.v2d->cur.xmin);
	clipxmax= MIN2(x2, G.v2d->cur.xmax);
	
	if (sample_step > 1)
		subsample_step= ((int)(subsample_step*sample_step*8)) & (~3);
	
	/* for speedy access */
	midy = (y1+y2)/2;
	fsofs= (float)sofs;
	feofs_sofs= (float)(eofs-sofs);
	sound_width= x2-x1;
	sound = seq->sound;
	stream = sound->stream;
	wavemulti = (y2-y1)/196605; /*y2-y1 is the height*/
	wavesample=0;
	
	/* we need to get the starting offset value, excuse the duplicate code */
	f=clipxmin;
	offset= (int) (fsofs + ((f-x1)/sound_width) * feofs_sofs) & (~3);
	
	/* start the loop, draw a line per sample_step -sample_step is about 1 line drawn per pixel */
	glBegin(GL_LINES);
	for (f=x1+sample_step; f<=clipxmax; f+=sample_step) {
		
		offset_next = (int) (fsofs + ((f-x1)/sound_width) * feofs_sofs) & (~3);
		if (f > G.v2d->cur.xmin) {
			/* if this is close to the last sample just exit */
			if (offset_next >= sound->streamlen) break;
			
			wavesamplemin = 131070;
			wavesamplemax = -131070;
			
			/*find with high and low of the waveform for this draw,
			evaluate small samples to find this range */
			while (offset < offset_next) {
				s = (signed short*)(stream+offset);
				
				wavesample = s[0]*2 + s[1];
				if (wavesamplemin>wavesample)
					wavesamplemin=wavesample;
				if (wavesamplemax<wavesample)
					wavesamplemax=wavesample;
				offset+=subsample_step;
			}
			/* draw the wave line, looks good up close and zoomed out */
			glVertex2f(f,  midy-(wavemulti*wavesamplemin) );
			glVertex2f(f,  midy-(wavemulti*wavesamplemax) );
		} else {
			while (offset < offset_next) offset+=subsample_step;
		}
		
		offset=offset_next;
	}
	glEnd();
}

/* draw a handle, for each end of a sequence strip */
static void draw_seq_handle(Sequence *seq, SpaceSeq *sseq, short direction)
{
	float v1[2], v2[2], v3[2], rx1=0, rx2=0; //for triangles and rect
	float x1, x2, y1, y2;
	float pixelx;
	float handsize;
	float minhandle, maxhandle;
	char str[120];
	unsigned int whichsel=0;
	View2D *v2d;
	
	x1= seq->startdisp;
	x2= seq->enddisp;
	
	y1= seq->machine+SEQ_STRIP_OFSBOTTOM;
	y2= seq->machine+SEQ_STRIP_OFSTOP;
	
	v2d = &sseq->v2d;
	pixelx = (v2d->cur.xmax - v2d->cur.xmin)/(v2d->mask.xmax - v2d->mask.xmin);
	
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
			glRasterPos3f(rx1,  y1-0.15, 0.0);
		} else {
			sprintf(str, "%d", seq->enddisp - 1);
			glRasterPos3f((x2-BMF_GetStringWidth(G.font, str)*pixelx),  y2+0.05, 0.0);
		}
		BMF_DrawString(G.font, str);
	}	
}

static void draw_seq_extensions(Sequence *seq, SpaceSeq *sseq)
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
		
		get_seq_color3ubv(seq, col);
		
		if (seq->flag & SELECT) {
			BIF_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.3, -40);
			glColor4ub(col[0], col[1], col[2], 170);
		} else {
			BIF_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.6, 0);
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
		
		get_seq_color3ubv(seq, col);
		
		if (seq->flag & SELECT) {
			BIF_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.3, -40);
			glColor4ub(col[0], col[1], col[2], 170);
		} else {
			BIF_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.6, 0);
			glColor4ub(col[0], col[1], col[2], 110);
		}
		
		glRectf(x2, y2, (float)(seq->start+seq->len), y2+SEQ_STRIP_OFSBOTTOM);
		
		if (seq->flag & SELECT) glColor4ub(col[0], col[1], col[2], 255);
		else glColor4ub(col[0], col[1], col[2], 160);

		fdrawbox(x2, y2, (float)(seq->start+seq->len), y2+SEQ_STRIP_OFSBOTTOM);	//outline
		
		glDisable( GL_BLEND );
	}
	if(seq->startstill) {
		get_seq_color3ubv(seq, col);
		BIF_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.75, 40);
		glColor3ubv((GLubyte *)col);
		
		draw_shadedstrip(seq, col, x1, y1, (float)(seq->start), y2);
		
		/* feint pinstripes, helps see exactly which is extended and which isn't,
		* especially when the extension is very small */ 
		if (seq->flag & SELECT) BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 24);
		else BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -16);
		
		glColor3ubv((GLubyte *)col);
		
		for(a=y1; a< y2; a+= pixely*2.0 ) {
			fdrawline(x1,  a,  (float)(seq->start),  a);
		}
	}
	if(seq->endstill) {
		get_seq_color3ubv(seq, col);
		BIF_GetColorPtrBlendShade3ubv(col, blendcol, col, 0.75, 40);
		glColor3ubv((GLubyte *)col);
		
		draw_shadedstrip(seq, col, (float)(seq->start+seq->len), y1, x2, y2);
		
		/* feint pinstripes, helps see exactly which is extended and which isn't,
		* especially when the extension is very small */ 
		if (seq->flag & SELECT) BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 24);
		else BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -16);
		
		glColor3ubv((GLubyte *)col);
		
		for(a=y1; a< y2; a+= pixely*2.0 ) {
			fdrawline((float)(seq->start+seq->len),  a,  x2,  a);
		}
	}
}

/* draw info text on a sequence strip */
static void draw_seq_text(Sequence *seq, float x1, float x2, float y1, float y2)
{
	float v1[2], v2[2];
	int len, size;
	char str[32 + FILE_MAXDIR+FILE_MAXFILE], *strp;
	short mval[2];
	
	v1[1]= y1;
	v2[1]= y2;
	
	v1[0]= x1;
	ipoco_to_areaco_noclip(G.v2d, v1, mval);
	x1= mval[0];
	v2[0]= x2;
	ipoco_to_areaco_noclip(G.v2d, v2, mval);
	x2= mval[0];
	size= x2-x1;
	
	if(seq->name[2]) {
		sprintf(str, "%d | %s: %s", seq->len, give_seqname(seq), seq->name+2);
	}else{
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
		else if (seq->type == SEQ_RAM_SOUND) {
			sprintf(str, "%d | %s", seq->len, seq->strip->stripdata->name);
		}
		else if (seq->type == SEQ_HD_SOUND) {
			sprintf(str, "%d | %s", seq->len, seq->strip->stripdata->name);
		}
		else if (seq->type == SEQ_MOVIE) {
			sprintf(str, "%d | %s%s", seq->len, seq->strip->dir, seq->strip->stripdata->name);
		}
	}
	
	strp= str;
	
	while( (len= BMF_GetStringWidth(G.font, strp)) > size) {
		if(len < 10) break;
		if(strp[1]==0) break;
		strp++;
	}
	
	mval[0]= (x1+x2-len+1)/2;
	mval[1]= 1;
	areamouseco_to_ipoco(G.v2d, mval, &x1, &x2);
	
	if(seq->flag & SELECT) cpack(0xFFFFFF);
	else cpack(0);
	glRasterPos3f(x1,  y1+SEQ_STRIP_OFSBOTTOM, 0.0);
	BMF_DrawString(G.font, strp);
}

/* draws a shaded strip, made from gradient + flat color + gradient */
static void draw_shadedstrip(Sequence *seq, char *col, float x1, float y1, float x2, float y2)
{
	float ymid1, ymid2;
	
	ymid1 = (y2-y1)*0.25 + y1;
	ymid2 = (y2-y1)*0.65 + y1;
	
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);
	
	if(seq->flag & SELECT) BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -50);
	else BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 0);
	
	glColor3ubv((GLubyte *)col);
	
	glVertex2f(x1,y1);
	glVertex2f(x2,y1);
	
	if(seq->flag & SELECT) BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 5);
	else BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -5);

	glColor3ubv((GLubyte *)col);
	
	glVertex2f(x2,ymid1);
	glVertex2f(x1,ymid1);
	
	glEnd();
	
	glRectf(x1,  ymid1,  x2,  ymid2);
	
	glBegin(GL_QUADS);
	
	glVertex2f(x1,ymid2);
	glVertex2f(x2,ymid2);
	
	if(seq->flag & SELECT) BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -15);
	else BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 25);
	
	glColor3ubv((GLubyte *)col);
	
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);
	
	glEnd();
	
}

/*
Draw a sequence strip, bounds check alredy made
ScrArea is currently only used to get the windows width in pixels
so wave file sample drawing precission is zoom adjusted
*/
static void draw_seq_strip(Sequence *seq, ScrArea *sa, SpaceSeq *sseq)
{
	float x1, x2, y1, y2;
	char col[3], is_single_image;
	Sequence *last_seq = get_last_seq();

	/* we need to know if this is a single image or not for drawing */
	is_single_image = (char)check_single_image_seq(seq);
	
	/* body */
	if(seq->startstill) x1= seq->start;
	else x1= seq->startdisp;
	y1= seq->machine+SEQ_STRIP_OFSBOTTOM;
	if(seq->endstill) x2= seq->start+seq->len;
	else x2= seq->enddisp;
	y2= seq->machine+SEQ_STRIP_OFSTOP;
	
	
	/* get the correct color per strip type*/
	get_seq_color3ubv(seq, col);
	
	/* draw the main strip body */
	if (is_single_image) /* single image */
		draw_shadedstrip(seq, col, seq_tx_get_final_left(seq), y1, seq_tx_get_final_right(seq), y2);
	else /* normal operation */
		draw_shadedstrip(seq, col, x1, y1, x2, y2);
	
	/* draw additional info and controls */
	if (seq->type == SEQ_RAM_SOUND)
		drawseqwave(seq, x1, y1, x2, y2, sa->winx);
	
	if (!is_single_image)
		draw_seq_extensions(seq, sseq);
	
	draw_seq_handle(seq, sseq, SEQ_LEFTHANDLE);
	draw_seq_handle(seq, sseq, SEQ_RIGHTHANDLE);
	
	/* draw the strip outline */
	x1= seq->startdisp;
	x2= seq->enddisp;
	
	get_seq_color3ubv(seq, col);
	if (G.moving && (seq->flag & SELECT)) {
		if(seq->flag & SEQ_OVERLAP) {
			col[0]= 255; col[1]= col[2]= 40;
		} else BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 120);
	}
	else if (seq == last_seq) BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, 120);
	else if (seq->flag & SELECT) BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -150);
	else BIF_GetColorPtrBlendShade3ubv(col, col, col, 0.0, -60);

	glColor3ubv((GLubyte *)col);
	gl_round_box_shade(GL_LINE_LOOP, x1, y1, x2, y2, 0.0, 0.1, 0.0);

	
	/* calculate if seq is long enough to print a name */
	x1= seq->startdisp+seq->handsize;
	x2= seq->enddisp-seq->handsize;

	/* but first the contents of a meta */
	if(seq->type==SEQ_META) drawmeta_contents(seq, x1, y1+0.15, x2, y2-0.15);

	/* info text on the strip */
	if(x1<G.v2d->cur.xmin) x1= G.v2d->cur.xmin;
	else if(x1>G.v2d->cur.xmax) x1= G.v2d->cur.xmax;
	if(x2<G.v2d->cur.xmin) x2= G.v2d->cur.xmin;
	else if(x2>G.v2d->cur.xmax) x2= G.v2d->cur.xmax;

	/* nice text here would require changing the view matrix for texture text */
	if(x1 != x2) {
		draw_seq_text(seq, x1, x2, y1, y2);
	}
}

static Sequence *special_seq_update= 0;

void set_special_seq_update(int val)
{
	int x;

	/* if mouse over a sequence && LEFTMOUSE */
	if(val) {
		special_seq_update= find_nearest_seq(&x);
	}
	else special_seq_update= 0;
}


static void draw_image_seq(ScrArea *sa)
{
	SpaceSeq *sseq;
	StripElem *se;
	struct ImBuf *ibuf;
	int x1, y1, rectx, recty;
	int free_ibuf = 0;
	static int recursive= 0;
	float zoom;

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	sseq= sa->spacedata.first;
	if(sseq==0) return;

	rectx= (G.scene->r.size*G.scene->r.xsch)/100;
	recty= (G.scene->r.size*G.scene->r.ysch)/100;

	/* BIG PROBLEM: the give_ibuf_seq() can call a rendering, which in turn calls redraws...
	   this shouldn't belong in a window drawing....
	   So: solve this once event based. 
	   Now we check for recursion, space type and active area again (ton) */
	
	if(recursive)
		return;
	else {
		recursive= 1;
		if (!U.prefetchframes || (G.f & G_PLAYANIM) == 0) {
			ibuf= (ImBuf *)give_ibuf_seq(rectx, recty, (G.scene->r.cfra), sseq->chanshown);
		} else {
			ibuf= (ImBuf *)give_ibuf_threaded(rectx, recty, (G.scene->r.cfra), sseq->chanshown);
		}
		recursive= 0;
		
		/* HURMF! the give_ibuf_seq can call image display in this window */
		if(sa->spacetype!=SPACE_SEQ)
			return;
		if(sa!=curarea) {
			areawinset(sa->win);
		}
	}
	
	if(special_seq_update) {
		se = special_seq_update->curelem;
		if(se) {
			if(se->ok==2) {
				if(se->se1)
					ibuf= se->se1->ibuf;
			}
			else ibuf= se->ibuf;
		}
	}
	if(ibuf==NULL) 
		return;
	if(ibuf->rect_float && ibuf->rect==NULL)
		IMB_rect_from_float(ibuf);
	if(ibuf->rect==NULL) 
		return;

	if (sseq->mainb == SEQ_DRAW_IMG_WAVEFORM) {
		ibuf = make_waveform_view_from_ibuf(ibuf);
		free_ibuf = 1;
	} else if (sseq->mainb == SEQ_DRAW_IMG_VECTORSCOPE) {
		ibuf = make_vectorscope_view_from_ibuf(ibuf);
		free_ibuf = 1;
	}

	if (sseq->zoom > 0) {
		zoom = sseq->zoom;
	} else if (sseq->zoom == 0) {
		zoom = 1.0;
	} else {
		zoom = -1.0/sseq->zoom;
	}

	/* calc location */
	x1= (sa->winx-zoom*ibuf->x)/2 + sseq->xof;
	y1= (sa->winy-zoom*ibuf->y)/2 + sseq->yof;

	/* needed for gla draw */
	glaDefine2DArea(&curarea->winrct);
	glPixelZoom(zoom, zoom);

	glaDrawPixelsSafe(x1, y1, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
	
	glPixelZoom(1.0, 1.0);

	if (free_ibuf) {
		IMB_freeImBuf(ibuf);
	}

	sa->win_swap= WIN_BACK_OK;
}

static void draw_extra_seqinfo(void)
{
	Sequence *last_seq = get_last_seq();
	StripElem *se, *last;
	float xco, xfac, yco, yfac;
	int sta, end;
	char str[256];

	if(last_seq==0) return;

	/* xfac: size of 1 pixel */
	xfac= G.v2d->cur.xmax - G.v2d->cur.xmin;
	xfac/= (float)(G.v2d->mask.xmax-G.v2d->mask.xmin);
	xco= G.v2d->cur.xmin+10*xfac;

	yfac= G.v2d->cur.ymax - G.v2d->cur.ymin;
	yfac/= (float)(G.v2d->mask.ymax-G.v2d->mask.ymin);
	yco= G.v2d->cur.ymin+40*yfac;
	
	BIF_ThemeColor(TH_TEXT_HI);

	/* NAME */
	glRasterPos3f(xco,  yco, 0.0);
	strncpy(str, give_seqname(last_seq), 255);
	BMF_DrawString(G.font, str);
	xco += xfac*BMF_GetStringWidth(G.font, str) +10.0*xfac;

	if(last_seq->type==SEQ_SCENE && last_seq->scene) {
		glRasterPos3f(xco,  yco, 0.0);
		BMF_DrawString(G.font, last_seq->scene->id.name+2);
		xco += xfac*BMF_GetStringWidth(G.font, last_seq->scene->id.name+2) +30.0*xfac;
	}

	/* LEN, dont bother with single images */
	if (check_single_image_seq(last_seq)==0) {
		if(last_seq->type & SEQ_EFFECT)
			sprintf(str, "len: %d   From %d - %d", last_seq->len, last_seq->startdisp, last_seq->enddisp-1);
		else
			sprintf(str, "len: %d (%d)", last_seq->enddisp-last_seq->startdisp, last_seq->len);
		
		glRasterPos3f(xco,  yco, 0.0);
	
		BMF_DrawString(G.font, str);
		xco += xfac*BMF_GetStringWidth(G.font, str) +10.0*xfac;
	}


	if(last_seq->type==SEQ_IMAGE) {
		if (last_seq->len > 1) {
			/* CURRENT */
			se= (StripElem *)give_stripelem(last_seq,  (G.scene->r.cfra));
			if(se) {
				sprintf(str, "Cur: %s", se->name);
				glRasterPos3f(xco,  yco, 0.0);
				BMF_DrawString(G.font, str);
				xco += xfac*BMF_GetStringWidth(G.font, str) +10.0*xfac;
			}
	
			/* FIRST AND LAST */
	
			if(last_seq->strip) {
				se= last_seq->strip->stripdata;
				last= se+last_seq->len-1;
				if(last_seq->startofs) se+= last_seq->startofs;
				if(last_seq->endofs) last-= last_seq->endofs;
	
				sprintf(str, "First: %s at %d   Last: %s at %d", se->name, last_seq->startdisp, last->name, last_seq->enddisp-1);
				glRasterPos3f(xco,  yco, 0.0);
				BMF_DrawString(G.font, str);
				xco += xfac*BMF_GetStringWidth(G.font, str) +30.0*xfac;
	
				/* orig size */
				sprintf(str, "OrigSize: %d x %d", last_seq->strip->orx, last_seq->strip->ory);
				glRasterPos3f(xco,  yco, 0.0);
				BMF_DrawString(G.font, str);
				xco += xfac*BMF_GetStringWidth(G.font, str) +30.0*xfac;
			}
		} else { /* single image */
			if (last_seq->strip) {
				sprintf(str, "Single: %s   len: %d", last_seq->strip->stripdata->name, last_seq->enddisp-last_seq->startdisp);
				glRasterPos3f(xco,  yco, 0.0);
				BMF_DrawString(G.font, str);
				xco += xfac*BMF_GetStringWidth(G.font, str) +30.0*xfac;
			}
		}
	}
	else if(last_seq->type==SEQ_MOVIE) {

		sta= last_seq->startofs;
		end= last_seq->len-1-last_seq->endofs;

		sprintf(str, "%s   %s%s  First: %d at %d   Last: %d at %d   Cur: %d",
				last_seq->name+2, last_seq->strip->dir, last_seq->strip->stripdata->name,
				sta, last_seq->startdisp, end, last_seq->enddisp-1,  (G.scene->r.cfra)-last_seq->startdisp);

		glRasterPos3f(xco,  yco, 0.0);
		BMF_DrawString(G.font, str);
	}
	else if(last_seq->type==SEQ_SCENE) {
		se= (StripElem *)give_stripelem(last_seq,  (G.scene->r.cfra));
		if(se && last_seq->scene) {
			sprintf(str, "Cur: %d  First: %d  Last: %d", last_seq->sfra+se->nr, last_seq->sfra, last_seq->sfra+last_seq->len-1); 
			glRasterPos3f(xco,  yco, 0.0);
			BMF_DrawString(G.font, str);
		}
	}
	else if(last_seq->type==SEQ_RAM_SOUND
		|| last_seq->type == SEQ_HD_SOUND) {

		sta= last_seq->startofs;
		end= last_seq->len-1-last_seq->endofs;

		sprintf(str, "%s   %s%s  First: %d at %d   Last: %d at %d   Cur: %d   Gain: %.2f dB   Pan: %.2f",
				last_seq->name+2, last_seq->strip->dir, last_seq->strip->stripdata->name,
				sta, last_seq->startdisp, end, last_seq->enddisp-1,  (G.scene->r.cfra)-last_seq->startdisp,
				last_seq->level, last_seq->pan);

		glRasterPos3f(xco,  yco, 0.0);
		BMF_DrawString(G.font, str);
	}
}

void seq_reset_imageofs(SpaceSeq *sseq)
{
	sseq->xof = sseq->yof = sseq->zoom = 0;
}

void seq_viewmove(SpaceSeq *sseq)
{	
	ScrArea *sa;
	short mval[2], mvalo[2];
	short rectx, recty, xmin, xmax, ymin, ymax, pad;
	int oldcursor;
	Window *win;
	
	sa = sseq->area;
	rectx= (G.scene->r.size*G.scene->r.xsch)/100;
	recty= (G.scene->r.size*G.scene->r.ysch)/100;
	
	pad = 10;
	xmin = -(sa->winx/2) - rectx/2 + pad;
	xmax = sa->winx/2 + rectx/2 - pad;
	ymin = -(sa->winy/2) - recty/2 + pad;
	ymax = sa->winy/2 + recty/2 - pad;
	
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
			
			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		}
		else BIF_wait_for_statechange();
	}
	window_set_cursor(win, oldcursor);
}

#define SEQ_BUT_PLUGIN	1
#define SEQ_BUT_RELOAD	2
#define SEQ_BUT_EFFECT	3
#define SEQ_BUT_RELOAD_ALL 4

void do_seqbuttons(short val)
{
	Sequence *last_seq = get_last_seq();

	switch(val) {
	case SEQ_BUT_PLUGIN:
	case SEQ_BUT_EFFECT:
		update_changed_seq_and_deps(last_seq, 0, 1);
		break;

	case SEQ_BUT_RELOAD:
	case SEQ_BUT_RELOAD_ALL:
		update_seq_ipo_rect(last_seq);
		update_seq_icu_rects(last_seq);

		free_imbuf_seq();	// frees all

		break;
	}

	if (val == SEQ_BUT_RELOAD_ALL) {
		allqueue(REDRAWALL, 0);
	} else {
		allqueue(REDRAWSEQ, 0);
	}
}

static void seq_panel_properties(short cntrl)	// SEQ_HANDLER_PROPERTIES
{
	Sequence *last_seq = get_last_seq();
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "seq_panel_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(SEQ_HANDLER_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "Strip Properties", "Seq", 10, 230, 318, 204)==0) return;

	if(last_seq==NULL) return;

	if(last_seq->type==SEQ_PLUGIN) {
		PluginSeq *pis;
		VarStruct *varstr;
		int a, xco, yco;

		get_sequence_effect(last_seq);/* make sure, plugin is loaded */

		uiDefBut(block, LABEL, 0, "Type: Plugin", 10,50,70,20, 0, 0, 0, 0, 0, "");

		pis= last_seq->plugin;
		if(pis->vars==0) return;

		varstr= pis->varstr;
		if(varstr) {
			for(a=0; a<pis->vars; a++, varstr++) {
				xco= 150*(a/6)+10;
				yco= 125 - 20*(a % 6)+1;
				uiDefBut(block, varstr->type, SEQ_BUT_PLUGIN, varstr->name, xco,yco,150,19, &(pis->data[a]), varstr->min, varstr->max, 100, 0, varstr->tip);

			}
		}
		uiDefButBitS(block, TOG, SEQ_IPO_FRAME_LOCKED,
			     SEQ_BUT_RELOAD_ALL, "IPO Frame locked",
			     10,-40,150,19, &last_seq->flag,
			     0.0, 1.0, 0, 0,
			     "Lock the IPO coordinates to the "
			     "global frame counter.");

	}
	else if(last_seq->type==SEQ_IMAGE) {

		uiDefBut(block, LABEL, 0, "Type: Image", 10,160,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, B_NOP, "Name: ", 10,140,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");
		
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, SEQ_MAKE_PREMUL, SEQ_BUT_RELOAD, "Convert to Premul", 10,110,150,19, &last_seq->flag, 0.0, 21.0, 100, 0, "Converts RGB values to become premultiplied with Alpha");
		uiDefButBitS(block, TOG, SEQ_FILTERY, SEQ_BUT_RELOAD, "FilterY",	10,90,150,19, &last_seq->flag, 0.0, 21.0, 100, 0, "For video movies to remove fields");
		
		uiDefButBitS(block, TOG, SEQ_FLIPX, SEQ_BUT_RELOAD, "FlipX",	10,70,75,19, &last_seq->flag, 0.0, 21.0, 100, 0, "Flip on the X axis");
		uiDefButBitS(block, TOG, SEQ_FLIPY, SEQ_BUT_RELOAD, "FlipY",	85,70,75,19, &last_seq->flag, 0.0, 21.0, 100, 0, "Flip on the Y axis");
		
		uiDefButF(block, NUM, SEQ_BUT_RELOAD, "Mul:",			10,50,150,19, &last_seq->mul, 0.001, 5.0, 100, 0, "Multiply colors");
		uiDefButS(block, TOG|BIT|7, SEQ_BUT_RELOAD, "Reverse Frames", 10,30,150,19, &last_seq->flag, 0.0, 21.0, 100, 0, "Reverse frame order");
		uiDefButF(block, NUM, SEQ_BUT_RELOAD, "Strobe:",			10,10,150,19, &last_seq->strobe, 1.0, 30.0, 100, 0, "Only display every nth frame");
		uiBlockEndAlign(block);
	}
	else if(last_seq->type==SEQ_META) {

		uiDefBut(block, LABEL, 0, "Type: Meta", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, B_NOP, "Name: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");

	}
	else if(last_seq->type==SEQ_SCENE) {

		uiDefBut(block, LABEL, 0, "Type: Scene", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, B_NOP, "Name: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");
		uiDefButS(block, TOG|BIT|7, SEQ_BUT_RELOAD, "Reverse Frames", 10,90,150,19, &last_seq->flag, 0.0, 21.0, 100, 0, "Reverse frame order");
	}
	else if(last_seq->type==SEQ_MOVIE) {

		if(last_seq->mul==0.0) last_seq->mul= 1.0;

		uiDefBut(block, LABEL, 0, "Type: Movie", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, B_NOP, "Name: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");
		
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, SEQ_MAKE_PREMUL, SEQ_BUT_RELOAD, "Make Premul Alpha ", 10,90,150,19, &last_seq->flag, 0.0, 21.0, 100, 0, "Converts RGB values to become premultiplied with Alpha");
		uiDefButBitS(block, TOG, SEQ_FILTERY, SEQ_BUT_RELOAD, "FilterY ",	10,70,150,19, &last_seq->flag, 0.0, 21.0, 100, 0, "For video movies to remove fields");
		uiDefButF(block, NUM, SEQ_BUT_RELOAD, "Mul:",			10,50,150,19, &last_seq->mul, 0.001, 5.0, 100, 0, "Multiply colors");
		
		uiDefButS(block, TOG|BIT|7, SEQ_BUT_RELOAD, "Reverse Frames", 10,30,150,19, &last_seq->flag, 0.0, 21.0, 100, 0, "Reverse frame order");
		uiDefButF(block, NUM, SEQ_BUT_RELOAD, "Strobe:",			10,10,150,19, &last_seq->strobe, 1.0, 30.0, 100, 0, "Only display every nth frame");
		uiDefButI(block, NUM, SEQ_BUT_RELOAD, "Preseek:",			10,-10,150,19, &last_seq->anim_preseek, 0.0, 50.0, 100, 0, "On MPEG-seeking preseek this many frames");
		uiBlockEndAlign(block);
	}
	else if(last_seq->type==SEQ_RAM_SOUND || 
		last_seq->type==SEQ_HD_SOUND) {

		uiDefBut(block, LABEL, 0, "Type: Audio", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, 0, "Name: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");
		
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, SEQ_IPO_FRAME_LOCKED,
			     SEQ_BUT_RELOAD_ALL, "IPO Frame locked",
			     10,90,150,19, &last_seq->flag, 
			     0.0, 1.0, 0, 0, 
			     "Lock the IPO coordinates to the "
			     "global frame counter.");

		uiDefButBitS(block, TOG, SEQ_MUTE, B_NOP, "Mute", 10,70,120,19, &last_seq->flag, 0.0, 21.0, 100, 0, "");
		uiDefButF(block, NUM, SEQ_BUT_RELOAD, "Gain (dB):", 10,50,150,19, &last_seq->level, -96.0, 6.0, 100, 0, "");
		uiDefButF(block, NUM, SEQ_BUT_RELOAD, "Pan:", 	10,30,150,19, &last_seq->pan, -1.0, 1.0, 100, 0, "");
		uiBlockEndAlign(block);
	}
	else if(last_seq->type>=SEQ_EFFECT) {
		uiDefBut(block, LABEL, 0, "Type: Effect", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, B_NOP, "Name: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");
		
		uiDefButBitS(block, TOG, SEQ_IPO_FRAME_LOCKED,
			     SEQ_BUT_RELOAD_ALL, "IPO Frame locked",
			     10,90,150,19, &last_seq->flag, 
			     0.0, 1.0, 0, 0, 
			     "Lock the IPO coordinates to the "
			     "global frame counter.");
		
		uiBlockBeginAlign(block);
		if(last_seq->type==SEQ_WIPE){
			WipeVars *wipe = (WipeVars *)last_seq->effectdata;
			char formatstring[256];
			
			strncpy(formatstring, "Transition Type %t|Single Wipe%x0|Double Wipe %x1|Iris Wipe %x4|Clock Wipe %x5", 255);
			uiDefButS(block, MENU,SEQ_BUT_EFFECT, formatstring,	10,65,220,22, &wipe->wipetype, 0, 0, 0, 0, "What type of wipe should be performed");
			uiDefButF(block, NUM,SEQ_BUT_EFFECT,"Blur:",	10,40,220,22, &wipe->edgeWidth,0.0,1.0, 1, 2, "The percent width of the blur edge");
			switch(wipe->wipetype){ /*Skip Types that do not require angle*/
				case DO_IRIS_WIPE:
				case DO_CLOCK_WIPE:
				break;
				
				default:
					uiDefButF(block, NUM,SEQ_BUT_EFFECT,"Angle:",	10,15,220,22, &wipe->angle,-90.0,90.0, 1, 2, "The Angle of the Edge");
			}
			uiDefButS(block, TOG,SEQ_BUT_EFFECT,"Wipe In",  10,-10,220,22, &wipe->forward,0,0, 0, 0, "Controls Primary Direction of Wipe");				
		}
		else if(last_seq->type==SEQ_GLOW){
			GlowVars *glow = (GlowVars *)last_seq->effectdata;

			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "Threshold:", 	10,70,150,19, &glow->fMini, 0.0, 1.0, 0, 0, "Trigger Intensity");
			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "Clamp:",			10,50,150,19, &glow->fClamp, 0.0, 1.0, 0, 0, "Brightness limit of intensity");
			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "Boost factor:", 	10,30,150,19, &glow->fBoost, 0.0, 10.0, 0, 0, "Brightness multiplier");
			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "Blur distance:", 	10,10,150,19, &glow->dDist, 0.5, 20.0, 0, 0, "Radius of glow effect");
			uiDefButI(block, NUM, B_NOP, "Quality:", 10,-5,150,19, &glow->dQuality, 1.0, 5.0, 0, 0, "Accuracy of the blur effect");
			uiDefButI(block, TOG, B_NOP, "Only boost", 10,-25,150,19, &glow->bNoComp, 0.0, 0.0, 0, 0, "Show the glow buffer only");
		}
		else if(last_seq->type==SEQ_TRANSFORM){
			TransformVars *transform = (TransformVars *)last_seq->effectdata;

			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "xScale Start:", 	10,70,150,19, &transform->ScalexIni, 0.0, 10.0, 0, 0, "X Scale Start");
			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "xScale End:", 	160,70,150,19, &transform->ScalexFin, 0.0, 10.0, 0, 0, "X Scale End");
			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "yScale Start:",	10,50,150,19, &transform->ScaleyIni, 0.0, 10.0, 0, 0, "Y Scale Start");
			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "yScale End:", 	160,50,150,19, &transform->ScaleyFin, 0.0, 10.0, 0, 0, "Y Scale End");
			
			uiDefButI(block, ROW, SEQ_BUT_EFFECT, "Percent", 10, 30, 150, 19, &transform->percent, 0.0, 1.0, 0.0, 0.0, "Percent Translate");
			uiDefButI(block, ROW, SEQ_BUT_EFFECT, "Pixels", 160, 30, 150, 19, &transform->percent, 0.0, 0.0, 0.0, 0.0, "Pixels Translate");
			if(transform->percent==1){
				uiDefButF(block, NUM, SEQ_BUT_EFFECT, "x Start:", 	10,10,150,19, &transform->xIni, -500.0, 500.0, 0, 0, "X Position Start");
				uiDefButF(block, NUM, SEQ_BUT_EFFECT, "x End:", 	160,10,150,19, &transform->xFin, -500.0, 500.0, 0, 0, "X Position End");
				uiDefButF(block, NUM, SEQ_BUT_EFFECT, "y Start:", 	10,-10,150,19, &transform->yIni, -500.0, 500.0, 0, 0, "Y Position Start");
				uiDefButF(block, NUM, SEQ_BUT_EFFECT, "y End:", 	160,-10,150,19, &transform->yFin, -500.0, 500.0, 0, 0, "Y Position End");
			}else{
				uiDefButF(block, NUM, SEQ_BUT_EFFECT, "x Start:", 	10,10,150,19, &transform->xIni, -10000.0, 10000.0, 0, 0, "X Position Start");
				uiDefButF(block, NUM, SEQ_BUT_EFFECT, "x End:", 	160,10,150,19, &transform->xFin, -10000.0, 10000.0, 0, 0, "X Position End");
				uiDefButF(block, NUM, SEQ_BUT_EFFECT, "y Start:", 	10,-10,150,19, &transform->yIni, -10000.0, 10000.0, 0, 0, "Y Position Start");
				uiDefButF(block, NUM, SEQ_BUT_EFFECT, "y End:", 	160,-10,150,19, &transform->yFin, -10000.0, 10000.0, 0, 0, "Y Position End");

			}
			

			
			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "rot Start:",10,-30,150,19, &transform->rotIni, 0.0, 360.0, 0, 0, "Rotation Start");
			uiDefButF(block, NUM, SEQ_BUT_EFFECT, "rot End:",160,-30,150,19, &transform->rotFin, 0.0, 360.0, 0, 0, "Rotation End");

			uiDefButI(block, ROW, SEQ_BUT_EFFECT, "No Interpolat", 10, -50, 100, 19, &transform->interpolation, 0.0, 0.0, 0.0, 0.0, "No interpolation");
			uiDefButI(block, ROW, SEQ_BUT_EFFECT, "Bilinear", 101, -50, 100, 19, &transform->interpolation, 0.0, 1.0, 0.0, 0.0, "Bilinear interpolation");
			uiDefButI(block, ROW, SEQ_BUT_EFFECT, "Bicubic", 202, -50, 100, 19, &transform->interpolation, 0.0, 2.0, 0.0, 0.0, "Bicubic interpolation");
		} else if(last_seq->type==SEQ_COLOR) {
			SolidColorVars *colvars = (SolidColorVars *)last_seq->effectdata;
			uiDefButF(block, COL, SEQ_BUT_RELOAD, "",10,90,150,19, colvars->col, 0, 0, 0, 0, "");
		} else if(last_seq->type==SEQ_SPEED){
			SpeedControlVars *sp = 
				(SpeedControlVars *)last_seq->effectdata;

			uiDefButF(block, NUM, SEQ_BUT_RELOAD, "Global Speed:", 	10,70,150,19, &sp->globalSpeed, 0.0, 100.0, 0, 0, "Global Speed");

			uiDefButBitI(block, TOG, SEQ_SPEED_INTEGRATE,
				     SEQ_BUT_RELOAD, 
				     "IPO is velocity",
				     10,50,150,19, &sp->flags, 
				     0.0, 1.0, 0, 0, 
				     "Interpret the IPO value as a "
				     "velocity instead of a frame number");

			uiDefButBitI(block, TOG, SEQ_SPEED_BLEND,
				     SEQ_BUT_RELOAD, 
				     "Enable frame blending",
				     10,30,150,19, &sp->flags, 
				     0.0, 1.0, 0, 0, 
				     "Blend two frames into the "
				     "target for a smoother result");

			uiDefButBitI(block, TOG, SEQ_SPEED_COMPRESS_IPO_Y,
				     SEQ_BUT_RELOAD, 
				     "IPO value runs from [0..1]",
				     10,10,150,19, &sp->flags, 
				     0.0, 1.0, 0, 0, 
				     "Scale IPO value to get the "
				     "target frame number.");
		}

		uiBlockEndAlign(block);
	}
}

static void seq_blockhandlers(ScrArea *sa)
{
	SpaceSeq *sseq= sa->spacedata.first;
	short a;

	/* warning; blocks need to be freed each time, handlers dont remove (for ipo moved to drawipospace) */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);

	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		switch(sseq->blockhandler[a]) {

		case SEQ_HANDLER_PROPERTIES:
			seq_panel_properties(sseq->blockhandler[a+1]);
			break;

		}
		/* clear action value for event */
		sseq->blockhandler[a+1]= 0;
	}
	uiDrawBlocksPanels(sa, 0);

}

void drawprefetchseqspace(ScrArea *sa, void *spacedata)
{
	SpaceSeq *sseq= sa->spacedata.first;
	int rectx, recty;

	rectx= (G.scene->r.size*G.scene->r.xsch)/100;
	recty= (G.scene->r.size*G.scene->r.ysch)/100;

	if(sseq->mainb) {
		give_ibuf_prefetch_request(
			rectx, recty, (G.scene->r.cfra), sseq->chanshown);
	}
}

void drawseqspace(ScrArea *sa, void *spacedata)
{
	SpaceSeq *sseq= sa->spacedata.first;
	View2D *v2d= &sseq->v2d;
	Editing *ed;
	Sequence *seq;
	float col[3];
	int ofsx, ofsy;
	int i;

	ed= G.scene->ed;

	if(sseq->mainb) {
		draw_image_seq(sa);
		return;
	}

	bwin_clear_viewmat(sa->win);	/* clear buttons view */
	glLoadIdentity();

	BIF_GetThemeColor3fv(TH_BACK, col);
	if(ed && ed->metastack.first) glClearColor(col[0], col[1], col[2]-0.1, 0.0);
	else glClearColor(col[0], col[1], col[2], 0.0);

	glClear(GL_COLOR_BUFFER_BIT);

	calc_scrollrcts(sa, v2d, sa->winx, sa->winy);

	if(sa->winx>SCROLLB+10 && sa->winy>SCROLLH+10) {
		if(v2d->scroll) {
			ofsx= sa->winrct.xmin;	/* because of mywin */
			ofsy= sa->winrct.ymin;
			glViewport(ofsx+v2d->mask.xmin,  ofsy+v2d->mask.ymin, ( ofsx+v2d->mask.xmax-1)-(ofsx+v2d->mask.xmin)+1, ( ofsy+v2d->mask.ymax-1)-( ofsy+v2d->mask.ymin)+1);
			glScissor(ofsx+v2d->mask.xmin,  ofsy+v2d->mask.ymin, ( ofsx+v2d->mask.xmax-1)-(ofsx+v2d->mask.xmin)+1, ( ofsy+v2d->mask.ymax-1)-( ofsy+v2d->mask.ymin)+1);
		}
	}


	myortho2(v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);

	BIF_ThemeColorShade(TH_BACK, -20);
	glRectf(v2d->cur.xmin,  0.0,  v2d->cur.xmax,  1.0);

	
	boundbox_seq();
	calc_ipogrid();
	
	i= MAX2(1, ((int)G.v2d->cur.ymin)-1);

	glBegin(GL_QUADS);
	while (i<v2d->cur.ymax) {
		if (((int)i) & 1)
			BIF_ThemeColorShade(TH_BACK, -15);
		else
			BIF_ThemeColorShade(TH_BACK, -25);
		
		glVertex2f(v2d->cur.xmax, i);
		glVertex2f(v2d->cur.xmin, i);
		glVertex2f(v2d->cur.xmin, i+1);
		glVertex2f(v2d->cur.xmax, i+1);
		i+=1.0;
	}
	glEnd();
	
	/* Force grid lines */
	i= MAX2(1, ((int)G.v2d->cur.ymin)-1);
	glBegin(GL_LINES);

	while (i<G.v2d->cur.ymax) {
		BIF_ThemeColor(TH_GRID);
		glVertex2f(G.v2d->cur.xmax, i);
		glVertex2f(G.v2d->cur.xmin, i);
		i+=1.0;
	}
	glEnd();
	
	
	draw_ipogrid();
	draw_cfra_seq();


	/* sequences: first deselect */
	if(ed) {
		seq= ed->seqbasep->first;
		while(seq) { /* bound box test, dont draw outside the view */
			if (seq->flag & SELECT ||
					MIN2(seq->startdisp, seq->start) > v2d->cur.xmax ||
					MAX2(seq->enddisp, seq->start+seq->len) < v2d->cur.xmin ||
					seq->machine+1.0 < v2d->cur.ymin ||
					seq->machine > v2d->cur.ymax)
			{
				/* dont draw */
			} else {
				draw_seq_strip(seq, sa, sseq);
			}
			seq= seq->next;
		}
	}
	ed= G.scene->ed;
	if(ed) {
		seq= ed->seqbasep->first;
		while(seq) { /* bound box test, dont draw outside the view */
			if (!(seq->flag & SELECT) ||
					MIN2(seq->startdisp, seq->start) > v2d->cur.xmax ||
					MAX2(seq->enddisp, seq->start+seq->len) < v2d->cur.xmin ||
					seq->machine+1.0 < v2d->cur.ymin ||
					seq->machine > v2d->cur.ymax)
			{
				/* dont draw */
			} else {
				draw_seq_strip(seq, sa, sseq);
			}
			seq= seq->next;
		}
	}

	draw_extra_seqinfo();

	/* Draw markers */
	draw_markers_timespace(1);
	
	/* restore viewport */
	mywinset(sa->win);

	/* ortho at pixel level sa */
	myortho2(-0.375, sa->winx-0.375, -0.375, sa->winy-0.375);

	if(sa->winx>SCROLLB+10 && sa->winy>SCROLLH+10) {
		if(v2d->scroll) {
			drawscroll(0);
		}
	}

	draw_area_emboss(sa);

	if(sseq->mainb==0) {
		/* it is important to end a view in a transform compatible with buttons */
		bwin_scalematrix(sa->win, sseq->blockscale, sseq->blockscale, sseq->blockscale);
		seq_blockhandlers(sa);
	}

	sa->win_swap= WIN_BACK_OK;
}


