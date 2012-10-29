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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/bullet.c
 *  \ingroup bke
 */


#include "MEM_guardedalloc.h"

/* types */
#include "DNA_object_force.h"	/* here is the softbody struct */

#include "BKE_bullet.h"


/* ************ Object level, exported functions *************** */

/* allocates and initializes general main data */
BulletSoftBody *bsbNew(void)
{
	BulletSoftBody *bsb;
	
	bsb = MEM_callocN(sizeof(BulletSoftBody), "bulletsoftbody");
		
	bsb->flag = OB_BSB_BENDING_CONSTRAINTS | OB_BSB_SHAPE_MATCHING | OB_BSB_AERO_VPOINT;
	bsb->linStiff = 0.5f;
	bsb->angStiff = 1.0f;
	bsb->volume = 1.0f;

	
	bsb->viterations	=	0;
	bsb->piterations	=	2;
	bsb->diterations	=	0;
	bsb->citerations	=	4;
	
	bsb->kSRHR_CL		=	0.1f;
	bsb->kSKHR_CL		=	1.f;
	bsb->kSSHR_CL		=	0.5f;
	bsb->kSR_SPLT_CL	=	0.5f;
	
	bsb->kSK_SPLT_CL	=	0.5f;
	bsb->kSS_SPLT_CL	=	0.5f;
	bsb->kVCF			=	1;
	bsb->kDP			=	0;

	bsb->kDG			=	0;
	bsb->kLF			=	0;
	bsb->kPR			=	0;
	bsb->kVC			=	0;

	bsb->kDF			=	0.2f;
	bsb->kMT			=	0.05;
	bsb->kCHR			=	1.0f;
	bsb->kKHR			=	0.1f;

	bsb->kSHR			=	1.0f;
	bsb->kAHR			=	0.7f;
	
	bsb->collisionflags = 0;
	//bsb->collisionflags = OB_BSB_COL_CL_RS + OB_BSB_COL_CL_SS;
	bsb->numclusteriterations = 64;
	bsb->welding = 0.f;

	return bsb;
}

/* frees all */
void bsbFree(BulletSoftBody *bsb)
{
	/* no internal data yet */
	MEM_freeN(bsb);
}


