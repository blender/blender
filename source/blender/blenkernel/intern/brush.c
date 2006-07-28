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

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"

#include "BKE_brush.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"

Brush *add_brush(char *name)
{
	Brush *brush;

	brush= alloc_libblock(&G.main->brush, ID_BR, name);

	brush->rgb[0]= 1.0f;
	brush->rgb[1]= 1.0f;
	brush->rgb[2]= 1.0f;
	brush->alpha= 0.2f;
	brush->size= 25;
	brush->spacing= 10.0f;
	brush->rate= 0.1f;
	brush->innerradius= 0.5f;
	brush->clone.alpha= 0.5;

	/* enable fake user by default */
	brush_toggle_fake_user(brush);
	
	return brush;	
}

Brush *copy_brush(Brush *brush)
{
	Brush *brushn;
	
	brushn= copy_libblock(brush);

	/* enable fake user by default */
	if (!(brushn->id.flag & LIB_FAKEUSER))
		brush_toggle_fake_user(brushn);
	
	return brushn;
}

/* not brush itself */
void free_brush(Brush *brush)
{
}

void make_local_brush(Brush *brush)
{
	/* don't forget: add stuff texture make local once texture bruses are added*/

	/* - only lib users: do nothing
	    * - only local users: set flag
	    * - mixed: make copy
	    */
	
	Brush *brushn;
	Scene *scene;
	int local= 0, lib= 0;

	if(brush->id.lib==0) return;

	if(brush->clone.image) {
    	/* special case: ima always local immediately */
		brush->clone.image->id.lib= 0;
		brush->clone.image->id.flag= LIB_LOCAL;
		new_id(0, (ID *)brush->clone.image, 0);
    }

	for(scene= G.main->scene.first; scene; scene=scene->id.next)
		if(scene->toolsettings->imapaint.brush==brush) {
			if(scene->id.lib) lib= 1;
			else local= 1;
		}
	
	if(local && lib==0) {
		brush->id.lib= 0;
		brush->id.flag= LIB_LOCAL;
		new_id(0, (ID *)brush, 0);

		/* enable fake user by default */
		if (!(brush->id.flag & LIB_FAKEUSER))
			brush_toggle_fake_user(brush);
	}
	else if(local && lib) {
		brushn= copy_brush(brush);
		brushn->id.us= 1; /* only keep fake user */
		brushn->id.flag |= LIB_FAKEUSER;
		
		for(scene= G.main->scene.first; scene; scene=scene->id.next)
			if(scene->toolsettings->imapaint.brush==brush)
				if(scene->id.lib==0) {
					scene->toolsettings->imapaint.brush= brushn;
					brushn->id.us++;
					brush->id.us--;
				}
	}
}

static void brush_blend_mix(char *cp, char *cp1, char *cp2, int fac)
{
	/* this and other blending modes previously used >>8 instead of /255. both
	   are not equivalent (>>8 is /256), and the former results in rounding
	   errors that can turn colors black fast */
	int mfac= 255-fac;
	cp[0]= (mfac*cp1[0]+fac*cp2[0])/255;
	cp[1]= (mfac*cp1[1]+fac*cp2[1])/255;
	cp[2]= (mfac*cp1[2]+fac*cp2[2])/255;
}

static void brush_blend_add(char *cp, char *cp1, char *cp2, int fac)
{
	int temp;

	temp= cp1[0] + ((fac*cp2[0])/255);
	if(temp>254) cp[0]= 255; else cp[0]= temp;
	temp= cp1[1] + ((fac*cp2[1])/255);
	if(temp>254) cp[1]= 255; else cp[1]= temp;
	temp= cp1[2] + ((fac*cp2[2])/255);
	if(temp>254) cp[2]= 255; else cp[2]= temp;
}

static void brush_blend_sub(char *cp, char *cp1, char *cp2, int fac)
{
	int temp;

	temp= cp1[0] - ((fac*cp2[0])/255);
	if(temp<0) cp[0]= 0; else cp[0]= temp;
	temp= cp1[1] - ((fac*cp2[1])/255);
	if(temp<0) cp[1]= 0; else cp[1]= temp;
	temp= cp1[2] - ((fac*cp2[2])/255);
	if(temp<0) cp[2]= 0; else cp[2]= temp;
}

static void brush_blend_mul(char *cp, char *cp1, char *cp2, int fac)
{
	int mfac= 255-fac;
	
	/* first mul, then blend the fac */
	cp[0]= (mfac*cp1[0] + fac*((cp2[0]*cp1[0])/255))/255;
	cp[1]= (mfac*cp1[1] + fac*((cp2[1]*cp1[1])/255))/255;
	cp[2]= (mfac*cp1[2] + fac*((cp2[2]*cp1[2])/255))/255;
}

static void brush_blend_lighten(char *cp, char *cp1, char *cp2, int fac)
{
	/* See if are lighter, if so mix, else dont do anything.
	if the paint col is darker then the original, then ignore */
	if (cp1[0]+cp1[1]+cp1[2] > cp2[0]+cp2[1]+cp2[2]) {
		cp[0]= cp1[0];
		cp[1]= cp1[1];
		cp[2]= cp1[2];
	}
	else
		brush_blend_mix(cp, cp1, cp2, fac);
}

static void brush_blend_darken(char *cp, char *cp1, char *cp2, int fac)
{
	/* See if were darker, if so mix, else dont do anything.
	if the paint col is brighter then the original, then ignore */
	if (cp1[0]+cp1[1]+cp1[2] < cp2[0]+cp2[1]+cp2[2]) {
		cp[0]= cp1[0];
		cp[1]= cp1[1];
		cp[2]= cp1[2];
	}
	else
		brush_blend_mix(cp, cp1, cp2, fac);
}

void brush_blend_rgb(char *outcol, char *col1, char *col2, int fac, short mode)
{
	if (fac==0) {
		outcol[0]= col1[0];
		outcol[1]= col1[1];
		outcol[2]= col1[2];
	}
	else {
		switch (mode) {
			case BRUSH_BLEND_MIX:
				brush_blend_mix(outcol, col1, col2, fac); break;
			case BRUSH_BLEND_ADD:
				brush_blend_add(outcol, col1, col2, fac); break;
			case BRUSH_BLEND_SUB:
				brush_blend_sub(outcol, col1, col2, fac); break;
			case BRUSH_BLEND_MUL:
				brush_blend_mul(outcol, col1, col2, fac); break;
			case BRUSH_BLEND_LIGHTEN:
				brush_blend_lighten(outcol, col1, col2, fac); break;
			case BRUSH_BLEND_DARKEN:
				brush_blend_darken(outcol, col1, col2, fac); break;
			default:
				brush_blend_mix(outcol, col1, col2, fac); break;
		}
	}
}

int brush_set_nr(Brush **current_brush, int nr)
{
	ID *idtest, *id;
	
	id= (ID*)(*current_brush);
	idtest= (ID*)BLI_findlink(&G.main->brush, nr-1);
	
	if(idtest==0) { /* new brush */
		if(id) idtest= (ID *)copy_brush((Brush *)id);
		else idtest= (ID *)add_brush("Brush");
		idtest->us--;
	}
	if(idtest!=id) {
		brush_delete(current_brush);
		*current_brush= (Brush *)idtest;
		id_us_plus(idtest);

		return 1;
	}

	return 0;
}

int brush_delete(Brush **current_brush)
{
	if (*current_brush) {
		(*current_brush)->id.us--;
		*current_brush= NULL;
		return 1;
	}

	return 0;
}

void brush_toggle_fake_user(Brush *brush)
{
	ID *id= (ID*)brush;
	if(id) {
		if(id->flag & LIB_FAKEUSER) {
			id->flag -= LIB_FAKEUSER;
			id->us--;
		} else {
			id->flag |= LIB_FAKEUSER;
			id_us_plus(id);
		}
	}
}

int brush_clone_image_set_nr(Brush *brush, int nr)
{
	if(brush && nr > 0) {
		Image *ima= (Image*)BLI_findlink(&G.main->image, nr-1);

		if(ima) {
			brush_clone_image_delete(brush);
			brush->clone.image= ima;
			id_us_plus(&ima->id);
			brush->clone.offset[0]= brush->clone.offset[1]= 0.0f;

			return 1;
		}
	}

	return 0;
}

int brush_clone_image_delete(Brush *brush)
{
	if (brush && brush->clone.image) {
		brush->clone.image->id.us--;
		brush->clone.image= NULL;
		return 1;
	}

	return 0;
}

void brush_check_exists(Brush **brush)
{
	if(*brush==NULL)
		brush_set_nr(brush, 1);
}

