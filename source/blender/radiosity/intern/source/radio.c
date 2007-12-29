/* ***************************************
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



    radio.c	nov/dec 1992
			may 1999

    $Id$

    - mainloop
    - interactivity

	
	- PREPROCES
		- collect meshes 
		- spitconnected	(all faces with different color and normals)
		- setedgepointers (nodes pointing to neighbours)

	- EDITING
		- min-max patch en min-max element size
		- using this info patches subdividing
		- lamp subdivide
	
		- if there are too many lamps for subdivide shooting:
			- temporal join patches 
	
	- SUBDIVIDE SHOOTING
		- except for last shooting, this defines patch subdivide
		- if subdivided patches still > 2*minsize : continue
		- at the end create as many elements as possible
		- als store if lamp (can still) cause subdivide.
		
	- REFINEMENT SHOOTING
		- test for overflows (shootpatch subdivide)
		- testen for extreme color transitions:
			- if possible: shootpatch subdivide
			- elements subdivide = start over ?
		- continue itterate until ?
		
	- DEFINITIVE SHOOTING
		- user indicates how many faces maximum and duration of itteration.
		
	- POST PROCESS
		- join element- nodes when nothing happens in it (filter nodes, filter faces)
		- define gamma & mul

 *************************************** */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BLI_blenlib.h"

#include "DNA_object_types.h"
#include "DNA_radio_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "radio.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* locals? This one was already done in radio.h... */
/*  void rad_status_str(char *str); */

RadGlobal RG= {0, 0};

void freeAllRad()
{
	Base *base;
	extern int Ntotvert, Ntotnode, Ntotpatch;
	
	/* clear flag that disables drawing the meshes */
	if(G.scene) {
		base= (G.scene->base.first);
		while(base) {		
			if(base->object->type==OB_MESH) {
				base->flag &= ~OB_RADIO;
			}
			base= base->next;
		}
	}
	
	free_fastAll();	/* verts, nodes, patches */
	RG.patchbase.first= RG.patchbase.last= 0;
	Ntotvert= Ntotnode= Ntotpatch= 0;
	
	closehemiwindows();		/* not real windows anymore... */
	if(RG.elem) MEM_freeN(RG.elem);
	RG.elem= 0;
	if(RG.verts) MEM_freeN(RG.verts);
	RG.verts= 0;
	if(RG.topfactors) MEM_freeN(RG.topfactors);
	RG.topfactors= 0;
	if(RG.sidefactors) MEM_freeN(RG.sidefactors);
	RG.sidefactors= 0;
	if(RG.formfactors) MEM_freeN(RG.formfactors);
	RG.formfactors= 0;
	if(RG.index) MEM_freeN(RG.index);
	RG.index= 0;
	if(RG.facebase) {
		init_face_tab();	/* frees all tables */
		MEM_freeN(RG.facebase);
		RG.facebase= 0;
	}

	if(RG.mfdata) {
		CustomData_free(RG.mfdata, RG.mfdatatot);
		MEM_freeN(RG.mfdata);
		MEM_freeN(RG.mfdatanodes);
		RG.mfdatanodes= NULL;
		RG.mfdata= NULL;
		RG.mfdatatot= 0;
	}
	RG.totelem= RG.totpatch= RG.totvert= RG.totface= RG.totlamp= RG.totmat= 0;	
}

int rad_phase()
{
	int flag= 0;
	
	if(RG.totpatch) flag |= RAD_PHASE_PATCHES;
	if(RG.totface) flag |= RAD_PHASE_FACES;
	
	return flag;
}

void rad_status_str(char *str)
{
	extern int totfastmem;
	int tot;
	char *phase;
	
	tot= (RG.totface*sizeof(Face))/1024;
	tot+= totfastmem/1024;
	
	if(RG.phase==RAD_SHOOTE) phase= "Phase: ELEMENT SUBD,  ";
	else if(RG.phase==RAD_SHOOTP) phase= "Phase: PATCH SUBD,  ";
	else if(RG.phase==RAD_SOLVE) phase= "Phase: SOLVE,  ";
	else if(RG.totpatch==0) phase= "Phase: COLLECT MESHES ";
	else if(RG.totface) phase= "Phase: FINISHED,  ";
	else phase= "Phase: INIT, ";
	
	if(RG.totpatch==0) strcpy(str, phase);
	else sprintf(str, "%s TotPatch: %d TotElem: %d Emit: %d Faces %d Mem: %d k ", phase, RG.totpatch, RG.totelem, RG.totlamp, RG.totface, tot);

	if(RG.phase==RAD_SOLVE) strcat(str, "(press ESC to stop)");
}

void rad_printstatus()
{
	/* actions always are started from a buttonswindow */
// XX	if(curarea) {
//		scrarea_do_windraw(curarea);
//		screen_swapbuffers();
//	}
}

void rad_setlimits()
{
	Radio *rad= G.scene->radio;
	float fac;
	
	fac= 0.0005*rad->pama;
	RG.patchmax= RG.maxsize*fac;
	RG.patchmax*= RG.patchmax;
	fac= 0.0005*rad->pami;
	RG.patchmin= RG.maxsize*fac;
	RG.patchmin*= RG.patchmin;

	fac= 0.0005*rad->elma;
	RG.elemmax= RG.maxsize*fac;
	RG.elemmax*= RG.elemmax;
	fac= 0.0005*rad->elmi;
	RG.elemmin= RG.maxsize*fac;
	RG.elemmin*= RG.elemmin;
}

void set_radglobal()
{
	/* always call before any action is performed */
	Radio *rad= G.scene->radio;

	if(RG.radio==0) {
		/* firsttime and to be sure */
		memset(&RG, 0, sizeof(RadGlobal));
	}
	
	if(rad==0) return;

	if(rad != RG.radio) {
		if(RG.radio) freeAllRad();
		memset(&RG, 0, sizeof(RadGlobal));
		RG.radio= rad;
	}
	
	RG.hemires= rad->hemires & 0xFFF0;
	RG.drawtype= rad->drawtype;
	RG.flag= rad->flag;
	RG.subshootp= rad->subshootp;
	RG.subshoote= rad->subshoote;
	RG.nodelim= rad->nodelim;
	RG.maxsublamp= rad->maxsublamp;
	RG.maxnode= 2*rad->maxnode;		/* in button:max elem, subdividing! */
	RG.convergence= rad->convergence/1000.0;
	RG.radfac= rad->radfac;
	RG.gamma= rad->gamma;
	RG.maxiter= rad->maxiter;
	
	RG.re= NULL;	/* struct render, for when call it from render engine */
	
	rad_setlimits();
}

/* called from buttons.c */
void add_radio()
{
	Radio *rad;
	
	if(G.scene->radio) MEM_freeN(G.scene->radio);
	rad= G.scene->radio= MEM_callocN(sizeof(Radio), "radio");

	rad->hemires= 300;
	rad->convergence= 0.1;
	rad->radfac= 30.0;
	rad->gamma= 2.0;
	rad->drawtype= DTSOLID;
	rad->subshootp= 1;
	rad->subshoote= 2;
	rad->maxsublamp= 0;
	
	rad->pama= 500;
	rad->pami= 200;
	rad->elma= 100;
	rad->elmi= 20;
	rad->nodelim= 0;
	rad->maxnode= 10000;
	rad->maxiter= 120;	// arbitrary
	rad->flag= 2;
	set_radglobal();
}

void delete_radio()
{
	freeAllRad();
	if(G.scene->radio) MEM_freeN(G.scene->radio);
	G.scene->radio= 0;

	RG.radio= 0;
}

int rad_go(void)	/* return 0 when user escapes */
{
	double stime= PIL_check_seconds_timer();
	int retval;
	
	if(RG.totface) return 0;

	G.afbreek= 0;
	
	set_radglobal();
	initradiosity();	/* LUT's */
	inithemiwindows();	/* views */
	
	maxsizePatches();

	setnodelimit(RG.patchmin);
	RG.phase= RAD_SHOOTP;
	subdivideshootPatches(RG.subshootp);

	setnodelimit(RG.elemmin);
	RG.phase= RAD_SHOOTE;
	subdivideshootElements(RG.subshoote);

	setnodelimit(RG.patchmin);
	subdividelamps();

	setnodelimit(RG.elemmin);

	RG.phase= RAD_SOLVE;
	subdiv_elements();

	progressiverad();

	removeEqualNodes(RG.nodelim);

	make_face_tab();	/* now anchored */

	closehemiwindows();
	RG.phase= 0;

	stime= PIL_check_seconds_timer()-stime;
	printf("Radiosity solving time: %dms\n", (int) (stime*1000));

	if(G.afbreek==1) retval= 1;
	else retval= 0;
	
	G.afbreek= 0;
	
	return retval;
}

void rad_subdivshootpatch()
{
	
	if(RG.totface) return;

	G.afbreek= 0;

	set_radglobal();
	initradiosity();	/* LUT's */
	inithemiwindows();	/* views */
	
	subdivideshootPatches(1);

	removeEqualNodes(RG.nodelim);
	closehemiwindows();
	
// XXX	allqueue(REDRAWVIEW3D, 1);
}

void rad_subdivshootelem(void)
{
	
	if(RG.totface) return;

	G.afbreek= 0;

	set_radglobal();
	initradiosity();	/* LUT's */
	inithemiwindows();	/* views */
	
	subdivideshootElements(1);

	removeEqualNodes(RG.nodelim);
	closehemiwindows();
	
// XXX	allqueue(REDRAWVIEW3D, 1);
}

void rad_limit_subdivide()
{

	if(G.scene->radio==0) return;

	set_radglobal();

	if(RG.totpatch==0) {
		/* printf("exit: no relevant data\n"); */
		return;
	}
	
	maxsizePatches();

	init_face_tab();	/* free faces */
}
