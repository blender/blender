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
 * i.t.t. wat de naam doet vermoeden: ook algemene lattice (calc) functies
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BIF_editlattice.h"
#include "BIF_editmode_undo.h"
#include "BIF_editkey.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BSE_edit.h"

#include "BDR_editobject.h"
#include "BDR_drawobject.h"

#include "blendef.h"
#include "mydevice.h"

#include "BKE_armature.h"

/* ***************************** */

void free_editLatt(void)
{
	if(editLatt) {
		if(editLatt->def) MEM_freeN(editLatt->def);
		if(editLatt->dvert) 
			free_dverts(editLatt->dvert, editLatt->pntsu*editLatt->pntsv*editLatt->pntsw);
		
		MEM_freeN(editLatt);
		editLatt= NULL;
	}
}


static void setflagsLatt(int flag)
{
	BPoint *bp;
	int a;
	
	bp= editLatt->def;
	
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	
	while(a--) {
		if(bp->hide==0) {
			bp->f1= flag;
		}
		bp++;
	}
}



void make_editLatt(void)
{
	Lattice *lt;
	KeyBlock *actkey;
	
	free_editLatt();
	
	lt= G.obedit->data;

	actkey = ob_get_keyblock(G.obedit);
	if(actkey) {
		strcpy(G.editModeTitleExtra, "(Key) ");
		key_to_latt(actkey, lt);
	}

	editLatt= MEM_dupallocN(lt);
	editLatt->def= MEM_dupallocN(lt->def);
	
	if(lt->dvert) {
		int tot= lt->pntsu*lt->pntsv*lt->pntsw;
		editLatt->dvert = MEM_mallocN (sizeof (MDeformVert)*tot, "Lattice MDeformVert");
		copy_dverts(editLatt->dvert, lt->dvert, tot);
	}
	
	BIF_undo_push("Original");
}


void load_editLatt(void)
{
	Lattice *lt;
	KeyBlock *actkey;
	BPoint *bp;
	float *fp;
	int tot;
	
	lt= G.obedit->data;
	
	actkey = ob_get_keyblock(G.obedit);
	if(actkey) {
		/* active key: vertices */
		tot= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
		
		if(actkey->data) MEM_freeN(actkey->data);
		
		fp=actkey->data= MEM_callocN(lt->key->elemsize*tot, "actkey->data");
		actkey->totelem= tot;
	
		bp= editLatt->def;
		while(tot--) {
			VECCOPY(fp, bp->vec);
			fp+= 3;
			bp++;
		}
	}
	else {

		MEM_freeN(lt->def);
	
		lt->def= MEM_dupallocN(editLatt->def);

		lt->flag= editLatt->flag;

		lt->pntsu= editLatt->pntsu;
		lt->pntsv= editLatt->pntsv;
		lt->pntsw= editLatt->pntsw;
		
		lt->typeu= editLatt->typeu;
		lt->typev= editLatt->typev;
		lt->typew= editLatt->typew;
	}
	
	if(lt->dvert) {
		free_dverts(lt->dvert, lt->pntsu*lt->pntsv*lt->pntsw);
		lt->dvert= NULL;
	}
	
	if(editLatt->dvert) {
		int tot= lt->pntsu*lt->pntsv*lt->pntsw;
		
		lt->dvert = MEM_mallocN (sizeof (MDeformVert)*tot, "Lattice MDeformVert");
		copy_dverts(lt->dvert, editLatt->dvert, tot);
	}
	
}

void remake_editLatt(void)
{
	if(okee("Reload original data")==0) return;
	
	make_editLatt();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);

	BIF_undo_push("Reload original");
}


void deselectall_Latt(void)
{
	BPoint *bp;
	int a;
	
	bp= editLatt->def;
	
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	
	allqueue(REDRAWVIEW3D, 0);
	
	while(a--) {
		if(bp->hide==0) {
			if(bp->f1) {
				setflagsLatt(0);
				countall();
				BIF_undo_push("(De)select all");
				return;
			}
		}
		bp++;
	}
	setflagsLatt(1);
	countall();
	BIF_undo_push("(De)select all");
}

static void findnearestLattvert__doClosest(void *userData, BPoint *bp, int x, int y)
{
	struct { BPoint *bp; short dist, select, mval[2]; } *data = userData;
	float temp = abs(data->mval[0]-x) + abs(data->mval[1]-y);
	
	if ((bp->f1 & SELECT)==data->select) temp += 5;
	if (temp<data->dist) {
		data->dist = temp;

		data->bp = bp;
	}
}
static BPoint *findnearestLattvert(int sel)
{
		/* sel==1: selected gets a disadvantage */
		/* in nurb and bezt or bp the nearest is written */
		/* return 0 1 2: handlepunt */
	struct { BPoint *bp; short dist, select, mval[2]; } data = {0};

	data.dist = 100;
	data.select = sel;
	getmouseco_areawin(data.mval);

	lattice_foreachScreenVert(findnearestLattvert__doClosest, &data);

	return data.bp;
}


void mouse_lattice(void)
{
	BPoint *bp=0;

	bp= findnearestLattvert(1);

	if(bp) {
		if((G.qual & LR_SHIFTKEY)==0) {
		
			setflagsLatt(0);
			bp->f1 |= SELECT;

			allqueue(REDRAWVIEW3D, 0);
		}
		else {
			bp->f1 ^= SELECT; /* swap */
			allqueue(REDRAWVIEW3D, 0);
		}

		countall();
		BIF_undo_push("Select");
	}

	rightmouse_transform();
	
}


/* **************** undo for lattice object ************** */

typedef struct UndoLattice {
	BPoint *def;
	int pntsu, pntsv, pntsw;
} UndoLattice;

static void undoLatt_to_editLatt(void *data)
{
	UndoLattice *ult= (UndoLattice*)data;
	int a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;

	memcpy(editLatt->def, ult->def, a*sizeof(BPoint));
}

static void *editLatt_to_undoLatt(void)
{
	UndoLattice *ult= MEM_callocN(sizeof(UndoLattice), "UndoLattice");
	ult->def= MEM_dupallocN(editLatt->def);
	ult->pntsu= editLatt->pntsu;
	ult->pntsv= editLatt->pntsv;
	ult->pntsw= editLatt->pntsw;
	
	return ult;
}

static void free_undoLatt(void *data)
{
	UndoLattice *ult= (UndoLattice*)data;

	if(ult->def) MEM_freeN(ult->def);
	MEM_freeN(ult);
}

static int validate_undoLatt(void *data)
{
	UndoLattice *ult= (UndoLattice*)data;

	return (ult->pntsu == editLatt->pntsu &&
	        ult->pntsv == editLatt->pntsv &&
	        ult->pntsw == editLatt->pntsw);
}

/* and this is all the undo system needs to know */
void undo_push_lattice(char *name)
{
	undo_editmode_push(name, free_undoLatt, undoLatt_to_editLatt, editLatt_to_undoLatt, validate_undoLatt);
}



/***/
