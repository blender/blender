/*  deform.c   June 2001
 *  
 *  support for deformation groups
 * 
 *	Reevan McKay
 *
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
#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "DNA_object_types.h"
#include "BKE_deform.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void color_temperature (float input, unsigned char *r, unsigned char *g, unsigned char *b)
{
	
	/* blue to red */
	
	float fr = (float)(*r);
	float fg = (float)(*g);
	float fb = (float)(*b);

	if (input < 0.0F)
		input = 0.0F;

	if (input > 1.0F)
		input = 1.0F;

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

	(*r) = (unsigned char)(fr * ((input/2.0f)+0.5f));
	(*g) = (unsigned char)(fg * ((input/2.0f)+0.5f));
	(*b) = (unsigned char)(fb * ((input/2.0f)+0.5f));


};

void copy_defgroups(ListBase *outbase, ListBase *inbase)
{
	bDeformGroup *defgroup, *defgroupn;

	outbase->first= outbase->last= 0;

	for (defgroup = inbase->first; defgroup; defgroup=defgroup->next){
		defgroupn= copy_defgroup(defgroup);
		BLI_addtail(outbase, defgroupn);
	}
}

bDeformGroup* copy_defgroup (bDeformGroup *ingroup)
{
	bDeformGroup *outgroup;

	if (!ingroup)
		return NULL;

	outgroup=MEM_callocN(sizeof(bDeformGroup), "deformGroup");
	
	/* For now, just copy everything over. */
	memcpy (outgroup, ingroup, sizeof(bDeformGroup));

	outgroup->next=outgroup->prev=NULL;

	return outgroup;
}

