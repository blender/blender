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

#include <math.h> 
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_curve_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_oops_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_texture_types.h"
#include "DNA_key_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"

#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_oops.h"
#include "BIF_drawoops.h"

#include "blendef.h"
#include "mydevice.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

static int correct_oops_y(Oops *oops);


Oops *add_oops(void *id)
{
	Oops *oops;
	
	if(G.soops==0) return NULL;
	oops= MEM_callocN(sizeof(Oops), "oops");

	BLI_addtail(&G.soops->oops, oops);
	
	oops->id= id;
	oops->type= GS(oops->id->name);
	
	return oops;
}


Oops *find_oops(ID *id)
{
	Oops *oops;

	/* op zoek naar een oops met dit ID */
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->id == id) {
			/* deze fout kwam een keer voor. beveiliging kan geen kwaad */
			if(oops->type != GS(id->name)) oops->id= 0;
			else break;
		}
		oops= oops->next;
	}
	return oops;
}

int test_oops(Oops *oops)
{
	/* test of de eigen ID block nog bestaat */
	ListBase *lb;
	ID *id;
	
	if(G.soops==0) return 0;
	
	lb= wich_libbase(G.main, oops->type);
	id= lb->first;
	while(id) {
		if(id==oops->id) break;
		id= id->next;
	}
	
	if(id==0) return 0;
	
	
	return 1;
}

void test_oopslinko(OopsLink *ol)
{
	/* test of links bestaan */
	Oops *oops;
	ListBase *lb;
	ID *id, *from;
	
	if(G.soops==0) return;
	
	ol->to= 0;
	from= *ol->idfrom;

	if(from==0) return;
	
	lb= wich_libbase(G.main, ol->type);
	id= lb->first;
	while(id) {
		if(id==from) break;
		id= id->next;
	}
	
	if(id==0) {
		/* ID bestaat niet meer */
		*ol->idfrom= 0;
	}
	else {
		/* op zoek naar een oops met dit ID */
		oops= G.soops->oops.first;
		while(oops) {
			if(oops->id == id) break;
			oops= oops->next;
		}
		
		ol->to= oops;
	}
}

void test_oopslink(OopsLink *ol)
{
	/* test of links bestaan */
	Oops *oops;
	ID *from;
	
	if(G.soops==0) return;
	
	ol->to= 0;
	from= *ol->idfrom;

	if(from==0) return;
	
	/* op zoek naar een oops met dit ID */
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->id == from) break;
		oops= oops->next;
	}
	
	ol->to= oops;
	if(oops) oops->flag |= OOPS_REFER;
}


OopsLink *add_oopslink(char *name, Oops *oops, short type, void *from, float xof, float yof)
{
	OopsLink *ol;
	
	if(G.soops==0) return NULL;
	
	/* testen offie al bestaat: een andere manier mag ook (linkbuffer) */
	/* ol= oops->link.first; */
	/* while(ol) { */
	/* 	if(ol->idfrom == from) { */
	/* 		strncpy(ol->name, name, 11); */
	/* 		ol->type= type; */
	/* 		ol->xof= xof; */
	/* 		ol->yof= yof; */
	/* 		return ol; */
	/* 	} */
	/* 	ol= ol->next; */
	/* } */
	
	if(* ((int *)from) == 0) return NULL;
	
	/* nieuwe maken */
	ol= MEM_callocN(sizeof(OopsLink), "oopslink");

	BLI_addtail(&oops->link, ol);

	ol->type= type;
	ol->idfrom= from;
	ol->xof= xof;
	ol->yof= yof;
	BLI_strncpy(ol->name, name, sizeof(ol->name));
	
	return ol;
}

int oops_test_overlap(Oops *test)
{
	Oops *oops;
	rctf rt, ro;
	
	rt.xmin= test->x;
	rt.xmax= (float)(test->x+OOPSX);
	rt.ymin= test->y;
	rt.ymax= (float)(test->y+OOPSY);

	oops= G.soops->oops.first;
	while(oops) {
		if(oops!=test) {	/* niet op hide testen: is slechts tijdelijke flag */
			
			ro.xmin= oops->x;
			ro.xmax= (float)(oops->x+OOPSX);
			ro.ymin= oops->y;
			ro.ymax= (float)(oops->y+OOPSY);

			if( BLI_isect_rctf(&rt, &ro, 0) ) return 1;
			
		}
		oops= oops->next;
	}
	
	return 0;
}

int oops_test_overlaphide(Oops *test)
{
	Oops *oops;
	rctf rt, ro;
	
	rt.xmin= (float)(test->x);
	rt.xmax= (float)(test->x+OOPSX);
	rt.ymin= (float)(test->y);
	rt.ymax= (float)(test->y+OOPSY);

	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0 && oops!=test) {	/* wel op hide testen, niet gebruiken tijdens build_oops */
			
			ro.xmin= oops->x;
			ro.xmax= (float)(oops->x+OOPSX);
			ro.ymin= oops->y;
			ro.ymax= (float)(oops->y+OOPSY);

			if( BLI_isect_rctf(&rt, &ro, 0) ) return 1;
			
		}
		oops= oops->next;
	}
	
	return 0;
}

float oopslink_totlen(Oops *oops)
{
	OopsLink *ol;
	float vec[4], dx, dy, len= 0.0;
	
	
	ol= oops->link.first;
	while(ol) {
		if(ol->to) {
			give_oopslink_line(oops, ol, vec, vec+2);
			
			dx= vec[0]-vec[2];
			dy= vec[1]-vec[3];
			
			len+= (float)sqrt( dx*dx + dy*dy );
		}
		ol= ol->next;
	}
	return len;
}


void add_from_link(Oops *from, Oops *oops)
{
	OopsLink *ol;
	
	ol= MEM_callocN(sizeof(OopsLink), "oopslinktemp");
	BLI_addtail(&oops->link, ol);
	ol->from= from;
	
}

void shuffle_oops()
{
	Oops *o2, *oops;
	OopsLink *ol, *oln;
	float olen, len1, f1, f2;
	int go= 1, tot=0, dir=1, type1, type2;
	
	
	/* we nemen twee oopsen, berkekenen de 'beauty' en de verwisselde beauty */
	
	if(G.soops==0) return;
	
	if(okee("Shuffle oops")==0) return;
	
	waitcursor(1);
	
	/* om de zaak 100% OK en snel te doen: voegen tijdelijk
	 * aan de ooplinklijst - per oops - de 'from' links ook in.
	 * Wel weer vrijgeven!
	 */
	
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {
			ol= oops->link.first;
			while(ol) {
				if(ol->to && ol->to->hide==0) {
					if(ol->to->flag & SELECT) {
						add_from_link(oops, ol->to);
					}
				}
				ol= ol->next;
			}
		}
		oops= oops->next;
	}
	
	while(go) {
		
		go= 0;
		dir= 1-dir;
		tot++;
		
		if(dir) oops= G.soops->oops.last;
		else oops= G.soops->oops.first;
		while(oops) {
			
			if(oops->link.first && oops->hide==0 && (oops->flag & SELECT)) {
				/* vind een goed verwisselbaar paar */
				olen= oopslink_totlen(oops);
				
				if(dir) o2= oops->prev;
				else o2= oops->next;
				
				if ELEM3(oops->type, ID_OB, ID_LI, ID_SCE) type1= 1; else type1= 0;
					

				while(o2) {
					if(o2->hide==0 && (o2->flag & SELECT)) {
					
						if ELEM3(o2->type, ID_OB, ID_LI, ID_SCE) type2= 1; else type2= 0;
						
						if(type1==type2) {
					
							len1= oopslink_totlen(o2);
							
							SWAP(float, oops->x, o2->x);
							SWAP(float, oops->y, o2->y);
							
							f1= oopslink_totlen(oops);
							f2= oopslink_totlen(o2);
							
							if( f1<=olen && f2<len1) {		/* 1 x <= !!! */
								olen= oopslink_totlen(oops);
								go= 1;
							}
							else {
								SWAP(float, oops->x, o2->x);
								SWAP(float, oops->y, o2->y);
							}
						}
					}
					if(dir) o2= o2->prev;
					else o2= o2->next;
				}
			}	
			if(dir) oops= oops->prev;
			else oops= oops->next;
			
			
		}
		if(tot>5) break;
	}
	waitcursor(0);
	
	/* from links vrijgeven */
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {
			ol= oops->link.first;
			while(ol) {
				oln= ol->next;
				if(ol->from) {
					BLI_remlink(&oops->link, ol);
					MEM_freeN(ol);
				}
				ol= oln;
			}
		}
		oops= oops->next;
	}

	allqueue(REDRAWOOPS, 1);
}

void shrink_oops()
{
	Oops *oops;
	OopsLink *ol;
	float vec[4];
	int /*  go= 1,  */tot=4;
	
	
	if(G.soops==0) return;
	
	if(okee("Shrink oops")==0) return;
	
	waitcursor(1);
	
	/* clear */
	oops= G.soops->oops.first;
	while(oops) {
		oops->dx= oops->dy= 0.0;
		oops= oops->next;
	}
	
	while(tot) {
		tot--;

		/* shrink */
		oops= G.soops->oops.first;
		while(oops) {
			if(oops->link.first && oops->hide==0 && (oops->flag & SELECT)) {
				
				ol= oops->link.first;
				while(ol) {
					if(ol->to && ol->to->hide==0) {
						
						give_oopslink_line(oops, ol, vec, vec+2);
						
						oops->dx= (float)(.8*oops->dx + .2*( vec[2]-vec[0]));
						oops->dy= (float)(.8*oops->dy + .2*( vec[3]-vec[1]));
						
						if(ol->to->flag & SELECT) {
							ol->to->dx= (float)(.8*ol->to->dx + .2*( vec[0]-vec[2]));
							ol->to->dy= (float)(.8*ol->to->dy + .2*( vec[1]-vec[3]));
						}
					}
					
					ol= ol->next;
				}
			}	
			oops= oops->next;
		}
			
		/* apply */
		oops= G.soops->oops.first;
		while(oops) {
			if(oops->hide==0 && (oops->flag & SELECT)) {
			
				/* shrink */
				oops->x+= oops->dx;
				oops->y+= oops->dy;
				
				if(oops_test_overlaphide(oops)) {
					oops->x-= oops->dx;
					oops->y-= oops->dy;
				}
			
				oops->dx= oops->dy= 0.0;
			}	
			oops= oops->next;
		}
	}
	waitcursor(0);
	
	allqueue(REDRAWOOPS, 1);
}

#define LIMSCE	-20.0
#define LIMOB	14.0
#define LIMDATA	24.0

static int correct_oops_y(Oops *oops)
{
	float y;
	
	y= oops->y;
	
	switch(oops->type) {
	case ID_SCE:
	case ID_LI:
		if(oops->y > LIMSCE-OOPSY) oops->y= (float)(LIMSCE-OOPSY);
		break;
	case ID_OB:
		CLAMP(oops->y, LIMSCE, LIMOB);
		break;
	case ID_IP:
	case ID_MA:
	case ID_TE:
		if(oops->y < LIMDATA+OOPSY) oops->y= (float)(LIMDATA+OOPSY);
		break;
	default:
		CLAMP(oops->y, (float)(LIMOB+OOPSY), LIMDATA);
		break;
	}
	
	if(y==oops->y) return 0;
	else return 1;
}

float oopslastx=0.0, oopslasty= 0.0;

void new_oops_location(Oops *new)
{
	float dirvec[4][2];
	static int cnt=0;
	int a, b, rc= 1, tel=1, ok=0;
	
	if(G.soops==0) return;
	
	if(G.soops->oops.first==G.soops->oops.last) {
		oopslastx= oopslasty= 0.0;
	}

	cnt++;
	
	new->x= oopslastx;
	new->y= oopslasty;
	
	correct_oops_y(new);
	
	/* vanuit centrum vrije plek vinden */
	dirvec[cnt & 3][0]= 1.2*OOPSX;
	dirvec[cnt & 3][1]= 0;
	cnt++;
	dirvec[cnt & 3][0]= 0;
	dirvec[cnt & 3][1]= (float)(-1.2*OOPSY);
	cnt++;
	dirvec[cnt & 3][0]= -1.2*OOPSX;
	dirvec[cnt & 3][1]= 0;
	cnt++;
	dirvec[cnt & 3][0]= 0;
	dirvec[cnt & 3][1]= (float)(1.2*OOPSY);
	cnt++;

	
	new->x+= dirvec[ (rc-2) & 3][0];
	new->y+= dirvec[ (rc-2) & 3][1];
	rc+= correct_oops_y(new);

	if( oops_test_overlap(new)==0 ) {
		ok= 1;
	}

	rc++;
	
	if(ok==0) {		
		new->x+= dirvec[ (rc-1) & 3][0];
		new->y+= dirvec[ (rc-1) & 3][1];
		rc+= correct_oops_y(new);
		
		if(oops_test_overlap(new)==0 ) {
			ok= 1;
		}
		rc++;
	}
	
	
	while(ok==0) {
		
		for(a=0;a<2;a++) {
			for(b=0;b<tel;b++) {

				if( oops_test_overlap(new)==0 ) {
					ok= 1;
					break;
				}
				
				rc &= 3;
				new->x += dirvec[rc][0];
				new->y += dirvec[rc][1];
				rc+= correct_oops_y(new);
			}
			rc++;
			
			
			
			if(ok) break;
		}
		if(ok || tel>100) break;
		tel++;
	}
	oopslastx= new->x;
	oopslasty= new->y;

}


void free_oops(Oops *oops)	/* ook oops zelf */
{
	BLI_freelistN(&oops->link);
	MEM_freeN(oops);
}

void free_oopspace(SpaceOops *so)
{
	Oops *oops;

	while( (oops= so->oops.first) ) {
		BLI_remlink(&so->oops, oops);
		free_oops(oops);
	}
}

void add_material_oopslinks(Material *ma, Oops *oops, short flag)
{
	int a;
	
	if(flag & OOPS_TE) {
		for(a=0; a<8; a++) {
			if(ma->mtex[a]) add_oopslink("tex", oops, ID_TE, &(ma->mtex[a]->tex), (float)(0.5*OOPSX), (float)OOPSY);
		}
	}
	if(flag & OOPS_OB) {
		for(a=0; a<8; a++) {
			if(ma->mtex[a]) add_oopslink("ob", oops, ID_OB, &(ma->mtex[a]->object), 0.0, (float)(0.2*OOPSY));
		}
	}
	if(flag & OOPS_IP) {
		if(ma->ipo) add_oopslink("ipo", oops, ID_IP, &(ma->ipo), OOPSX, (float)(0.5*OOPSY));
	}
}


void add_object_oopslinks(Object *ob, Oops *oops, short flag)
{
	ID *id;
	
	if(ob->parent) add_oopslink("parent", oops, ID_OB, &ob->parent, (float)(.6*OOPSX), (float)OOPSY);
	if(ob->track) add_oopslink("parent", oops, ID_OB, &ob->track, (float)(.4*OOPSX), (float)OOPSY);

	id= ob->data;
	if(id) {
		switch( GS(id->name) ) {
		case ID_ME:
			if(flag & OOPS_ME) add_oopslink("data", oops, ID_ME, &ob->data, (float)(.5*OOPSX), (float)OOPSY);
			break;
		case ID_CU:
			if(flag & OOPS_CU) add_oopslink("data", oops, ID_CU, &ob->data, (float)(.5*OOPSX), (float)OOPSY);
			break;
		case ID_MB:
			if(flag & OOPS_MB) add_oopslink("data", oops, ID_MB, &ob->data, (float)(.5*OOPSX), (float)OOPSY);
			break;
		case ID_LT:
			if(flag & OOPS_LT) add_oopslink("data", oops, ID_LT, &ob->data, (float)(.5*OOPSX), (float)OOPSY);
			break;
		case ID_LA:
			if(flag & OOPS_LA) add_oopslink("data", oops, ID_LA, &ob->data, (float)(.5*OOPSX), (float)OOPSY);
			break;
		}
	}
	
	if(flag & OOPS_MA) {
		short a;
		
		for(a=0; a<ob->totcol; a++) {
			if(ob->mat[a]) {
				add_oopslink("mat", oops, ID_MA, ob->mat+a, 0, (float)(0.5*OOPSY));
			}
		}
	}
	
	if(flag & OOPS_IP) add_oopslink("ipo", oops, ID_IP, &ob->ipo, OOPSX, (float)(0.5*OOPSY));
}

void add_mesh_oopslinks(Mesh *me, Oops *oops, short flag)
{
	int a;
	
	if(flag & OOPS_MA) {
		for(a=0; a<me->totcol; a++) {
			if(me->mat[a]) {
				add_oopslink("ma", oops, ID_MA, me->mat+a, 0.0, (float)(0.5*OOPSY));
			}
		}
	}
	if(flag & OOPS_IP) {
		if(me->key) add_oopslink("ipo", oops, ID_IP, &me->key->ipo, OOPSX, (float)(0.5*OOPSY));
	}
}

void add_curve_oopslinks(Curve *cu, Oops *oops, short flag)
{
	int a;
	
	if(flag & OOPS_MA) {
		for(a=0; a<cu->totcol; a++) {
			if(cu->mat[a]) {
				add_oopslink("ma", oops, ID_MA, cu->mat+a, 0.0, (float)(0.5*OOPSY));
			}
		}
	}
	if(flag & OOPS_IP) {
		add_oopslink("speed", oops, ID_IP, &cu->ipo, OOPSX, (float)(0.5*OOPSY));
		if(cu->key) add_oopslink("ipo", oops, ID_IP, &cu->key->ipo, OOPSX, (float)(0.5*OOPSY));
	}
	
}

void add_mball_oopslinks(MetaBall *mb, Oops *oops, short flag)
{
	int a;
	
	if(flag & OOPS_MA) {
		for(a=0; a<mb->totcol; a++) {
			if(mb->mat[a]) {
				add_oopslink("ma", oops, ID_MA, mb->mat+a, 0.0, (float)(0.5*OOPSY));
			}
		}
	}
}

Oops *add_test_oops(void *id)	/* incl links */
{
	Oops *oops;
	Object *ob;
	Lamp *la;
	Tex *tex;
	
	if(id==0) return NULL;
	
	/* eerst test ofie al bestaat */
	oops= find_oops(id);
	
	if(oops) {
		oops->hide= 0;
	}
	else {
		oops= add_oops(id);
		new_oops_location(oops);
		if(G.soops->flag & SO_NEWSELECTED) {
			oops->flag |= SELECT;
		}
	}
	
	switch( GS( ((ID *)id)->name)) {
	case ID_SCE:
		add_oopslink("set", oops, ID_SCE, &((Scene *)id)->set, (float)(.5*OOPSX), (float)OOPSY);
		break;
	case ID_OB:
		ob= (Object *)id;
		if(ob->flag & SELECT) oops->flag |= SELECT;
		else oops->flag &= ~SELECT;		
		add_object_oopslinks(ob, oops, G.soops->visiflag);
		break;
	case ID_ME:
		add_mesh_oopslinks((Mesh *)id, oops, G.soops->visiflag);
		break;
	case ID_CU:
		add_curve_oopslinks((Curve *)id, oops, G.soops->visiflag);
		break;
	case ID_MB:
		add_mball_oopslinks((MetaBall *)id, oops, G.soops->visiflag);
		break;
	case ID_LA:
		/* textures nog doen */
		la= (Lamp *)id;
		if(la->ipo) if(G.soops->visiflag & OOPS_IP) add_oopslink("ipo", oops, ID_IP, &la->ipo, OOPSX, (float)(0.3*OOPSY));
		break;	 
	case ID_IP:

		break;
	case ID_MA:
		add_material_oopslinks((Material *)id, oops, G.soops->visiflag);
		break;
	case ID_TE:
		tex= (Tex *)id;
		if(tex->ima) if(G.soops->visiflag & OOPS_IM) add_oopslink("image", oops, ID_IM, &tex->ima, OOPSX, (float)(0.3*OOPSY));
	}
	
	return oops;
}

void add_texture_oops(Material *ma)
{
	int a;
	
	for(a=0; a<8; a++) {
		if(ma->mtex[a]) {
			add_test_oops(ma->mtex[a]->tex);
			if(ma->mtex[a]->tex) if(G.soops->visiflag & OOPS_IM) add_test_oops(ma->mtex[a]->tex->ima);
		}
	}
}

void build_oops()
{
	Oops *oops;
	OopsLink *ol;
	ID *id;
	Base *base;
	Object *ob;
	short a, type;
	
	/* altijd alles bouwen! */

	if(G.soops==0) return;	
	
	/* set hide flags */
	oops= G.soops->oops.first;
	while(oops) {
		oops->hide= 1;
		oops->flag &= ~OOPS_REFER;
		
		BLI_freelistN(&oops->link);	/* veel veiliger */
		
		oops= oops->next;
	}

	/* oopsen maken,is ook testen of ie al bestaat */

	/* altijd */	
	if(G.soops->visiflag & OOPS_LI) {
		Library *li= G.main->library.first;
		while(li) {
			oops= add_test_oops(li);
			li= li->id.next;
		}
	}
	
	/* rest op twee manieren: of alles (OOPS_SCE) of alleen gebruikt in deze scene */
	
	if(G.soops->visiflag & OOPS_SCE) {
		Scene *sce= G.main->scene.first;
		
		while(sce) {
		
			oops= add_test_oops(sce);

			if(G.soops->visiflag & OOPS_OB) {
				base= sce->base.first;
				while(base) {
					
					add_oopslink("object", oops, ID_OB, &base->object, (float)(.5*OOPSX), (float)OOPSY);
					base= base->next;
				}
			}
			
			sce= sce->id.next;
		}
		
		if(G.soops->visiflag & OOPS_OB) {
			Object *ob= G.main->object.first;

			while(ob) {
				oops= add_test_oops(ob);
				ob= ob->id.next;
			}
		}
		if(G.soops->visiflag & OOPS_ME) {
			Mesh *me= G.main->mesh.first;
			while(me) {
				oops= add_test_oops(me);
				me= me->id.next;
			}
		}
	
		if(G.soops->visiflag & OOPS_CU) {
			Curve *cu= G.main->curve.first;
			while(cu) {
				oops= add_test_oops(cu);
				cu= cu->id.next;
			}
		}

		if(G.soops->visiflag & OOPS_MB) {
			MetaBall *mb= G.main->mball.first;
			while(mb) {
				oops= add_test_oops(mb);
				mb= mb->id.next;
			}
		}
	
		if(G.soops->visiflag & OOPS_LA) {
			Lamp *la= G.main->lamp.first;
			while(la) {
				oops= add_test_oops(la);
				la= la->id.next;
			}
		}
		
		if(G.soops->visiflag & OOPS_IP) {
			Ipo *ipo= G.main->ipo.first;
			while(ipo) {
				oops= add_test_oops(ipo);
				ipo= ipo->id.next;
			}
		}
		
		if(G.soops->visiflag & OOPS_MA) {
			Material *ma= G.main->mat.first;
			while(ma) {
				oops= add_test_oops(ma);
				ma= ma->id.next;
			}
		}
		if(G.soops->visiflag & OOPS_TE) {
			Tex *tex= G.main->tex.first;
			while(tex) {
				oops= add_test_oops(tex);
				tex= tex->id.next;
			}
		}
		if(G.soops->visiflag & OOPS_IM) {
			Image *ima= G.main->image.first;
			while(ima) {
				oops= add_test_oops(ima);
				ima= ima->id.next;
			}
		}
		
	}
	else {
		
		/* alleen blokken uit huidige scene */
		
		base= FIRSTBASE;
		while(base) {
			
			/* layer? */
			if( (G.soops->visiflag & OOPS_LAY)==0 || (base->lay & G.scene->lay)) {
				ob= base->object;
				
				if(G.soops->visiflag & OOPS_OB) {
					oops= add_test_oops(ob);
				}
				if(G.soops->visiflag & OOPS_MA) {
					for(a=0; a<ob->totcol; a++) {
						if(ob->mat[a]) {
							oops= add_test_oops(ob->mat[a]);
							if(G.soops->visiflag & OOPS_TE) add_texture_oops(ob->mat[a]);
						}
					}
				}
				if(G.soops->visiflag & OOPS_IP) oops= add_test_oops(ob->ipo);
				
				id= ob->data;
				if(id) {
					type= GS(id->name);
					
					if(type==ID_ME && G.soops->visiflag & OOPS_ME) {
						Mesh *me= ob->data;
						oops= add_test_oops(ob->data);
						
						if(G.soops->visiflag & OOPS_MA) {
							for(a=0; a<me->totcol; a++) {
								if(me->mat[a]) {
									oops= add_test_oops(me->mat[a]);
									if(G.soops->visiflag & OOPS_TE) add_texture_oops(me->mat[a]);
								}
							}
						}
						if(G.soops->visiflag & OOPS_IP) {
							if(me->key) oops= add_test_oops(me->key->ipo);
						}
					}
					else if(type==ID_CU && G.soops->visiflag & OOPS_CU) {
						Curve *cu= ob->data;
						oops= add_test_oops(ob->data);
						
						if(G.soops->visiflag & OOPS_MA) {
							for(a=0; a<cu->totcol; a++) {
								if(cu->mat[a]) {
									oops= add_test_oops(cu->mat[a]);
									if(G.soops->visiflag & OOPS_TE) add_texture_oops(cu->mat[a]);
								}
							}
						}
						if(G.soops->visiflag & OOPS_IP) {
							if(cu->ipo) oops= add_test_oops(cu->ipo);
							if(cu->key) oops= add_test_oops(cu->key->ipo);
						}
					}
					else if(type==ID_MB && G.soops->visiflag & OOPS_MB) {
						oops= add_test_oops(ob->data);
						
						if(G.soops->visiflag & OOPS_MA) {
							MetaBall *mb= ob->data;
							for(a=0; a<mb->totcol; a++) {
								if(mb->mat[a]) {
									oops= add_test_oops(mb->mat[a]);
									if(G.soops->visiflag & OOPS_TE) add_texture_oops(mb->mat[a]);
								}
							}
						}
					}
					else if(type==ID_LA && G.soops->visiflag & OOPS_LA) {
						oops= add_test_oops(ob->data);
					}
				}
			}
			base= base->next;
		}
	}
	
	


	/* links testen */
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			ol= oops->link.first;
			while(ol) {
				test_oopslink(ol);
				ol= ol->next;
			}
		}
		oops= oops->next;
	}
	
	G.soops->flag &= ~SO_NEWSELECTED;
}
