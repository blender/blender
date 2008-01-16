/*
 * $Id: multires.c 13015 2007-12-27 07:27:03Z nicholasbishop $
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the multiresolution modeling tools.
 *
 * multires.h
 *
 */

#include "MEM_guardedalloc.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_radialcontrol.h"

#include "BKE_global.h"

#include "mydevice.h"
#include "transform.h"

#include "math.h"

/* Prints the value being edited in the view header */
static void radialcontrol_header(const RadialControl *rc)
{
	if(rc) {
		char str[512];
		const char *name= "";

		if(rc->mode == RADIALCONTROL_SIZE)
			name= "Size";
		else if(rc->mode == RADIALCONTROL_STRENGTH)
			name= "Strength";
		else if(rc->mode == RADIALCONTROL_ROTATION)
			name= "Angle";

		sprintf(str, "%s: %d", name, (int)(rc->new_value));
		headerprint(str);
	}
}

/* Creates, initializes, and returns the control */
RadialControl *radialcontrol_start(const int mode, RadialControlCallback callback,
				   const int original_value, const int max_value,
				   const unsigned int tex)
{
	RadialControl *rc= MEM_callocN(sizeof(RadialControl), "radial control");
	short mouse[2];
		
	getmouseco_areawin(mouse);
	rc->origloc[0]= mouse[0];
	rc->origloc[1]= mouse[1];
		
	if(mode == RADIALCONTROL_SIZE)
		rc->origloc[0]-= original_value;
	else if(mode == RADIALCONTROL_STRENGTH)
		rc->origloc[0]-= 200 - 2*original_value;
	else if(mode == RADIALCONTROL_ROTATION) {
		rc->origloc[0]-= 200 * cos(original_value * M_PI / 180.0);
		rc->origloc[1]-= 200 * sin(original_value * M_PI / 180.0);
	}
		
	rc->callback = callback;
	rc->original_value = original_value;
	rc->max_value = max_value;

	rc->tex = tex;

	/* NumInput is used for keyboard input */
	rc->num = MEM_callocN(sizeof(NumInput), "radialcontrol numinput");
	rc->num->idx_max= 0;

	rc->mode= mode;
	radialcontrol_header(rc);
	
	allqueue(REDRAWVIEW3D, 0);

	return rc;
}


static void radialcontrol_end(RadialControl *rc)
{
	if(rc) {
		rc->callback(rc->mode, rc->new_value);
		BIF_undo_push("Brush property set");

		/* Free everything */
		glDeleteTextures(1, (GLuint*)(&rc->tex));
		MEM_freeN(rc->num);
		MEM_freeN(rc);

		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWHEADERS, 0);
	}
}

void radialcontrol_do_events(RadialControl *rc, unsigned short event)
{
	short mouse[2];
	short tmp[2];
	float dist;
	char valset= 0;

	if(!rc)
		return;
	
	handleNumInput(rc->num, event);
	
	if(hasNumInput(rc->num)) {
		float val;
		applyNumInput(rc->num, &val);
		rc->new_value = val;
		valset= 1;
		allqueue(REDRAWVIEW3D, 0);
	}
	
	switch(event) {
	case MOUSEX:
	case MOUSEY:
		if(!hasNumInput(rc->num)) {
			char ctrl= G.qual & LR_CTRLKEY;
			getmouseco_areawin(mouse);
			tmp[0]= rc->origloc[0]-mouse[0];
			tmp[1]= rc->origloc[1]-mouse[1];
			dist= sqrt(tmp[0]*tmp[0]+tmp[1]*tmp[1]);
			if(rc->mode == RADIALCONTROL_SIZE)
				rc->new_value = dist;
			else if(rc->mode == RADIALCONTROL_STRENGTH) {
				float fin= (200.0f - dist) * 0.5f;
				rc->new_value= fin>=0 ? fin : 0;
			} else if(rc->mode == RADIALCONTROL_ROTATION)
				rc->new_value= ((int)(atan2(tmp[1], tmp[0]) * (180.0 / M_PI)) + 180);

			if(ctrl)
				rc->new_value= (rc->new_value + 5) / 10*10;	

			valset= 1;
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case ESCKEY:
	case RIGHTMOUSE:
		rc->new_value = rc->original_value;
	case LEFTMOUSE:
		while(get_mbut()==L_MOUSE);
	case RETKEY:
	case PADENTER:
		radialcontrol_end(rc);
		return;
	default:
		break;
	};
	
	if(valset) {
		if(rc->new_value > rc->max_value)
			rc->new_value = rc->max_value;
	}
	
	radialcontrol_header(rc);
}

static void rot_line(const short o[2], const float ang)
{
	sdrawXORline(o[0], o[1], o[0] + 200*cos(ang), o[1] + 200*sin(ang));
}

void radialcontrol_draw(RadialControl *rc)
{
	short r1, r2, r3;
	float angle = 0;

	if(rc && rc->mode) {
		if(rc->mode == RADIALCONTROL_SIZE) {
			r1= rc->new_value;
			r2= rc->original_value;
			r3= r1;
		} else if(rc->mode == RADIALCONTROL_STRENGTH) {
			r1= 200 - rc->new_value * 2;
			r2= 200;
			r3= 200;
		} else if(rc->mode == RADIALCONTROL_ROTATION) {
			r1= r2= 200;
			r3= 200;
			angle = rc->new_value;
		}
		
		/* Draw brush with texture */
		glPushMatrix();
		glTranslatef(rc->origloc[0], rc->origloc[1], 0);
		glRotatef(angle, 0, 0, 1);

		if(rc->tex) {
			const float str = rc->mode == RADIALCONTROL_STRENGTH ? (rc->new_value / 200.0 + 0.5) : 1;

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glBindTexture(GL_TEXTURE_2D, rc->tex);

			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glEnable(GL_TEXTURE_2D);
			glBegin(GL_QUADS);
			glColor4f(0,0,0, str);
			glTexCoord2f(0,0);
			glVertex2f(-r3, -r3);
			glTexCoord2f(1,0);
			glVertex2f(r3, -r3);
			glTexCoord2f(1,1);
			glVertex2f(r3, r3);
			glTexCoord2f(0,1);
			glVertex2f(-r3, r3);
			glEnd();
			glDisable(GL_TEXTURE_2D);
		}
			
		glPopMatrix();

		if(r1 != r2)
			fdrawXORcirc(rc->origloc[0], rc->origloc[1], r1);
		fdrawXORcirc(rc->origloc[0], rc->origloc[1], r2);
			
		if(rc->mode == RADIALCONTROL_ROTATION) {
			float ang1= rc->original_value * (M_PI/180.0f);
			float ang2= rc->new_value * (M_PI/180.0f);

			if(rc->new_value > 359)
				ang2 = 0;

			rot_line(rc->origloc, ang1);
			if(ang1 != ang2)
				rot_line(rc->origloc, ang2);
		}
	}
}
