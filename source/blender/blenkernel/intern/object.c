/* object.c
 *
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
#include <math.h>
#include <stdio.h>			

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_ika_types.h"
#include "DNA_ipo_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_deform.h"
#include "BKE_nla.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_utildefines.h"
#include "BKE_bad_level_calls.h"

#include "BKE_main.h"
#include "BKE_global.h"

#include "BKE_object.h"
#include "BKE_blender.h"
#include "BKE_screen.h"
#include "BKE_ipo.h"
#include "BKE_ika.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_curve.h"
#include "BKE_mball.h"
#include "BKE_effect.h"
#include "BKE_sca.h"
#include "BKE_displist.h"
#include "BKE_property.h"
#include "BKE_anim.h"
#include "BKE_group.h"
#include "BKE_lattice.h"
#include "BKE_constraint.h"
#include "BKE_scene.h"

#include "BPY_extern.h"

/* Local function protos */
static void solve_parenting (Object *ob, Object *par, float slowmat[][4], int simul);

float originmat[3][3];	/* after where_is_object(), can be used in other functions (bad!) */
Object workob;

void clear_workob(void)
{
	memset(&workob, 0, sizeof(Object));
	
	workob.size[0]= workob.size[1]= workob.size[2]= 1.0;
	
}

void copy_baseflags()
{
	Base *base= G.scene->base.first;
	
	while(base) {
		base->object->flag= base->flag;
		base= base->next;
	}
}

void copy_objectflags()
{
	Base *base= G.scene->base.first;
	
	while(base) {
		base->flag= base->object->flag;
		base= base->next;
	}
}

void update_base_layer(Object *ob)
{
	Base *base= G.scene->base.first;

	while (base) {
		if (base->object == ob) base->lay= ob->lay;
		base= base->next;
	}
}

/* do not free object itself */
void free_object(Object *ob)
{
	int a;
	
	/* disconnect specific data */
	if(ob->data) {
		ID *id= ob->data;
		id->us--;
		if(id->us==0) {
			if(ob->type==OB_MESH) unlink_mesh(ob->data);
			else if(ob->type==OB_CURVE) unlink_curve(ob->data);
			else if(ob->type==OB_MBALL) unlink_mball(ob->data);
		}
		ob->data= 0;
	}
	
	for(a=0; a<ob->totcol; a++) {
		if(ob->mat[a]) ob->mat[a]->id.us--;
	}
	if(ob->mat) MEM_freeN(ob->mat);
	ob->mat= 0;
	if(ob->bb) MEM_freeN(ob->bb); 
	ob->bb= 0;
	if(ob->path) free_path(ob->path); 
	ob->path= 0;
	if(ob->ipo) ob->ipo->id.us--;
	if(ob->action) ob->action->id.us--;
	if(ob->defbase.first)
		BLI_freelistN(&ob->defbase);
	if(ob->pose) {
		clear_pose(ob->pose);
		MEM_freeN(ob->pose);
	}
	free_effects(&ob->effect);
	BLI_freelistN(&ob->network);
	free_properties(&ob->prop);
	
	free_sensors(&ob->sensors);
	free_controllers(&ob->controllers);
	free_actuators(&ob->actuators);
	
	free_constraints(&ob->constraints);
	free_constraint_channels(&ob->constraintChannels);
	free_nlastrips(&ob->nlastrips);

	freedisplist(&ob->disp);
	
	BPY_free_scriptlink(&ob->scriptlink);
	
	if(ob->pd) MEM_freeN(ob->pd);
}

void unlink_object(Object *ob)
{
	Object *obt;
	Material *mat;
	World *wrld;
	bScreen *sc;
	Scene *sce;
	Curve *cu;
	Tex *tex;
	Group *group;
	int a;

	unlink_controllers(&ob->controllers);
	unlink_actuators(&ob->actuators);
	
	/* check all objects: parents en bevels */
	obt= G.main->object.first;
	while(obt) {
		if(obt->id.lib==0) {
			if(obt->parent==ob) {
				obt->parent= 0;
				if(ob->type==OB_LATTICE) freedisplist(&obt->disp);
			}
			if(obt->track==ob) obt->track= 0;
			if ELEM(obt->type, OB_CURVE, OB_FONT) {
				cu= obt->data;
				if(cu->bevobj==ob) cu->bevobj= 0;
				if(cu->textoncurve==ob) cu->textoncurve= 0;
			}
			if(obt->type==OB_IKA) {
				Ika *ika= obt->data;
				Deform *def= ika->def;
				
				if(ika->parent==ob) ika->parent= 0;
				a= ika->totdef;
				while(a--) {
					if(def->ob==ob) {
						ika->totdef= 0;
						MEM_freeN(ika->def);
						ika->def= 0;
						break;
					}
					def++;
				}
			}
			sca_remove_ob_poin(obt, ob);
		}
		obt= obt->id.next;
	}
	
	/* materials */
	mat= G.main->mat.first;
	while(mat) {
	
		for(a=0; a<8; a++) {
			if(mat->mtex[a] && ob==mat->mtex[a]->object) {
				/* actually, test for lib here... to do */
				mat->mtex[a]->object= 0;
			}
		}

		mat= mat->id.next;
	}
	
	/* textures */
	tex= G.main->tex.first;
	while(tex) {
		if(tex->env) {
			if(tex->env->object == ob) tex->env->object= 0;
		}
		tex= tex->id.next;
	}
	
	/* mballs */
	if(ob->type==OB_MBALL) {
		obt= find_basis_mball(ob);
		if(obt) freedisplist(&obt->disp);
	}
	
	/* worlds */
	wrld= G.main->world.first;
	while(wrld) {
		if(wrld->id.lib==0) {
			for(a=0; a<6; a++) {
				if(wrld->mtex[a] && ob==wrld->mtex[a]->object)
					wrld->mtex[a]->object =0;
			}
		}
		
		wrld= wrld->id.next;
	}
		
	/* scenes */
	sce= G.main->scene.first;
	while(sce) {
		if(sce->id.lib==0) {
			if(sce->camera==ob) sce->camera= 0;
		}
		sce= sce->id.next;
	}
	/* keys */
	
	/* screens */
	sc= G.main->screen.first;
	while(sc) {
		ScrArea *sa= sc->areabase.first;
		while(sa) {
			SpaceLink *sl;

			for (sl= sa->spacedata.first; sl; sl= sl->next) {
				if(sl->spacetype==SPACE_VIEW3D) {
					View3D *v3d= (View3D*) sl;

					if(v3d->camera==ob) {
						v3d->camera= 0;
						if(v3d->persp>1) v3d->persp= 1;
					}
					if(v3d->localvd && v3d->localvd->camera==ob ) {
						v3d->localvd->camera= 0;
						if(v3d->localvd->persp>1) v3d->localvd->persp= 1;
					}
				}
			}

			sa= sa->next;
		}
		sc= sc->id.next;
	}

	/* groups */
	group= G.main->group.first;
	while(group) {
		rem_from_group(group, ob);
		group= group->id.next;
	}
}

int exist_object(Object *obtest)
{
	Object *ob;
	
	ob= G.main->object.first;
	while(ob) {
		if(ob==obtest) return 1;
		ob= ob->id.next;
	}
	return 0;
}

void *add_camera()
{
	Camera *cam;
	
	cam=  alloc_libblock(&G.main->camera, ID_CA, "Camera");

	cam->lens= 35.0f;
	cam->clipsta= 0.1f;
	cam->clipend= 100.0f;
	cam->drawsize= 0.5f;
	
	return cam;
}

Camera *copy_camera(Camera *cam)
{
	Camera *camn;
	
	camn= copy_libblock(cam);
	id_us_plus((ID *)camn->ipo);

	BPY_copy_scriptlink(&camn->scriptlink);
	
	return camn;
}



void make_local_camera(Camera *cam)
{
	Object *ob;
	Camera *camn;
	int local=0, lib=0;

	/* - only lib users: do nothing
	    * - only local users: set flag
	    * - mixed: make copy
	    */
	
	if(cam->id.lib==0) return;
	if(cam->id.us==1) {
		cam->id.lib= 0;
		cam->id.flag= LIB_LOCAL;
		new_id(0, (ID *)cam, 0);
		return;
	}
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->data==cam) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		cam->id.lib= 0;
		cam->id.flag= LIB_LOCAL;
		new_id(0, (ID *)cam, 0);
	}
	else if(local && lib) {
		camn= copy_camera(cam);
		camn->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->data==cam) {
				
				if(ob->id.lib==0) {
					ob->data= camn;
					camn->id.us++;
					cam->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}



void *add_lamp(void)
{
	Lamp *la;
	
	la=  alloc_libblock(&G.main->lamp, ID_LA, "Lamp");
	
	la->r= la->g= la->b= la->k= 1.0;
	la->haint= la->energy= 1.0;
	la->dist= 20.0;
	la->spotsize= 45.0;
	la->spotblend= 0.15;
	la->att2= 1.0;
	la->mode= LA_SHAD;
	la->bufsize= 512;
	la->clipsta= 0.5;
	la->clipend= 40.0;
	la->shadspotsize= 45.0;
	la->samp= 3;
	la->bias= 1.0;
	la->soft= 3.0;
	la->ray_samp= la->ray_sampy= la->ray_sampz= 1; 
	la->area_size=la->area_sizey=la->area_sizez= 1.0; 
	
	return la;
}

Lamp *copy_lamp(Lamp *la)
{
	Lamp *lan;
	int a;
	
	lan= copy_libblock(la);

	for(a=0; a<8; a++) {
		if(lan->mtex[a]) {
			lan->mtex[a]= MEM_mallocN(sizeof(MTex), "copylamptex");
			memcpy(lan->mtex[a], la->mtex[a], sizeof(MTex));
			id_us_plus((ID *)lan->mtex[a]->tex);
		}
	}
	
	id_us_plus((ID *)lan->ipo);

	BPY_copy_scriptlink(&la->scriptlink);
	
	return lan;
}

void make_local_lamp(Lamp *la)
{
	Object *ob;
	Lamp *lan;
	int local=0, lib=0;

	/* - only lib users: do nothing
	    * - only local users: set flag
	    * - mixed: make copy
	    */
	
	if(la->id.lib==0) return;
	if(la->id.us==1) {
		la->id.lib= 0;
		la->id.flag= LIB_LOCAL;
		new_id(0, (ID *)la, 0);
		return;
	}
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->data==la) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		la->id.lib= 0;
		la->id.flag= LIB_LOCAL;
		new_id(0, (ID *)la, 0);
	}
	else if(local && lib) {
		lan= copy_lamp(la);
		lan->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->data==la) {
				
				if(ob->id.lib==0) {
					ob->data= lan;
					lan->id.us++;
					la->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}

void free_camera(Camera *ca)
{
	BPY_free_scriptlink(&ca->scriptlink);
}

void free_lamp(Lamp *la)
{
	MTex *mtex;
	int a;

	/* scriptlinks */
		
	BPY_free_scriptlink(&la->scriptlink);
	
	for(a=0; a<8; a++) {
		mtex= la->mtex[a];
		if(mtex && mtex->tex) mtex->tex->id.us--;
		if(mtex) MEM_freeN(mtex);
	}
	la->ipo= 0;
}

void *add_wave()
{
	return 0;
}


/* *************************************************** */

static void *add_obdata_from_type(int type)
{
	switch (type) {
	case OB_MESH: G.totmesh++; return add_mesh();
	case OB_CURVE: G.totcurve++; return add_curve(OB_CURVE);
	case OB_SURF: G.totcurve++; return add_curve(OB_SURF);
	case OB_FONT: return add_curve(OB_FONT);
	case OB_MBALL: return add_mball();
	case OB_CAMERA: return add_camera();
	case OB_LAMP: G.totlamp++; return add_lamp();
	case OB_IKA: return add_ika();
	case OB_LATTICE: return add_lattice();
	case OB_WAVE: return add_wave();
	case OB_ARMATURE: return add_armature();
	case OB_EMPTY: return NULL;
	default:
		printf("add_obdata_from_type: Internal error, bad type: %d\n", type);
		return NULL;
	}
}

static char *get_obdata_defname(int type)
{
	switch (type) {
	case OB_MESH: return "Mesh";
	case OB_CURVE: return "Curve";
	case OB_SURF: return "Surf";
	case OB_FONT: return "Font";
	case OB_MBALL: return "Mball";
	case OB_CAMERA: return "Camera";
	case OB_LAMP: return "Lamp";
	case OB_IKA: return "Ika";
	case OB_LATTICE: return "Lattice";
	case OB_WAVE: return "Wave";
	case OB_ARMATURE: return "Armature";
	case OB_EMPTY: return "Empty";
	default:
		printf("get_obdata_defname: Internal error, bad type: %d\n", type);
		return "Empty";
	}
}

/* general add: to G.scene, with layer from area and default name */
/* creates minimum required data, but without vertices etc. */
Object *add_object(int type)
{
	Object *ob;
	Base *base;
	char name[32];

	if (G.obpose)
		exit_posemode(1);
	
	strcpy(name, get_obdata_defname(type));
	
	ob= alloc_libblock(&G.main->object, ID_OB, name);
	G.totobj++;
	
	/* default object vars */
	ob->type= type;
	/* ob->transflag= OB_QUAT; */
	
	QuatOne(ob->quat);
	QuatOne(ob->dquat);

	ob->col[0]= ob->col[1]= ob->col[2]= 0.0;
	ob->col[3]= 1.0;
	
	ob->loc[0]= ob->loc[1]= ob->loc[2]= 0.0;
	ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0;
	ob->size[0]= ob->size[1]= ob->size[2]= 1.0;

	Mat4One(ob->parentinv);
	Mat4One(ob->obmat);
	ob->dt= OB_SHADED;
	if(U.flag & USER_MAT_ON_OB) ob->colbits= -1;
	
	if(type==OB_CAMERA || type==OB_LAMP) {
		ob->trackflag= OB_NEGZ;
		ob->upflag= OB_POSY;
	}
	else {
		ob->trackflag= OB_POSY;
		ob->upflag= OB_POSZ;
	}
	ob->ipoflag = OB_OFFS_OB+OB_OFFS_PARENT;
	
	ob->dupon= 1; ob->dupoff= 0;
	ob->dupsta= 1; ob->dupend= 100;

	/* Game engine defaults*/
	ob->mass= ob->inertia= 1.0f;
	ob->formfactor= 0.4f;
	ob->damping= 0.04f;
	ob->rdamping= 0.1f;
	ob->anisotropicFriction[0] = 1.0f;
	ob->anisotropicFriction[1] = 1.0f;
	ob->anisotropicFriction[2] = 1.0f;
	ob->gameflag= OB_PROP;
	
	ob->data= add_obdata_from_type(type);
	
	ob->lay= G.scene->lay;

	base= scene_add_base(G.scene, ob);
	scene_select_base(G.scene, base);

	return ob;
}

void base_init_from_view3d(Base *base, View3D *v3d)
{
	Object *ob= base->object;

	if (v3d->localview) {
		base->lay= ob->lay= v3d->layact + v3d->lay;
		VECCOPY(ob->loc, v3d->cursor);
	} else {
		base->lay= ob->lay= v3d->layact;
		VECCOPY(ob->loc, G.scene->cursor);
	}

	v3d->viewquat[0]= -v3d->viewquat[0];
	if (ob->transflag & OB_QUAT) {
		QUATCOPY(ob->quat, v3d->viewquat);
	} else {
		QuatToEul(v3d->viewquat, ob->rot);
	}
	v3d->viewquat[0]= -v3d->viewquat[0];
}

Object *copy_object(Object *ob)
{
	Object *obn;
	int a;
	bConstraintChannel *actcon;

	obn= copy_libblock(ob);
	
	if(ob->totcol) {
		obn->mat= MEM_dupallocN(ob->mat);
	}
	
	if(ob->bb) obn->bb= MEM_dupallocN(ob->bb);
	obn->path= 0;
	obn->flag &= ~OB_FROMGROUP;
	
	copy_effects(&obn->effect, &ob->effect);
	
	obn->network.first= obn->network.last= 0;
	
	BPY_copy_scriptlink(&ob->scriptlink);
	
	copy_properties(&obn->prop, &ob->prop);
	copy_sensors(&obn->sensors, &ob->sensors);
	copy_controllers(&obn->controllers, &ob->controllers);
	copy_actuators(&obn->actuators, &ob->actuators);
	
	copy_pose(&obn->pose, ob->pose, 1);
	copy_defgroups(&obn->defbase, &ob->defbase);
	copy_nlastrips(&obn->nlastrips, &ob->nlastrips);
	copy_constraints (&obn->constraints, &ob->constraints);

	actcon = clone_constraint_channels (&obn->constraintChannels, &ob->constraintChannels, ob->activecon);
	/* If the active constraint channel was in this list, update it */
	if (actcon)
		obn->activecon = actcon;

	if(ob->pd) obn->pd= MEM_dupallocN(ob->pd);

	/* increase user numbers */
	id_us_plus((ID *)obn->data);
	id_us_plus((ID *)obn->ipo);
	id_us_plus((ID *)obn->action);
	for(a=0; a<obn->totcol; a++) id_us_plus((ID *)obn->mat[a]);
	
	obn->disp.first= obn->disp.last= 0;
	
	return obn;
}

void expand_local_object(Object *ob)
{
	int a;
	
	id_lib_extern((ID *)ob->action);
	id_lib_extern((ID *)ob->ipo);
	id_lib_extern((ID *)ob->data);
	
	for(a=0; a<ob->totcol; a++) {
		id_lib_extern((ID *)ob->mat[a]);
	}
}

void make_local_object(Object *ob)
{
	Object *obn;
	Scene *sce;
	Base *base;
	int local=0, lib=0;

	/* - only lib users: do nothing
	    * - only local users: set flag
	    * - mixed: make copy
	    */
	
	if(ob->id.lib==0) return;
	if(ob->id.us==1) {
		ob->id.lib= 0;
		ob->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ob, 0);

	}
	else {
		sce= G.main->scene.first;
		while(sce) {
			base= sce->base.first;
			while(base) {
				if(base->object==ob) {
					if(sce->id.lib) lib++;
					else local++;
					break;
				}
				base= base->next;
			}
			sce= sce->id.next;
		}
		
		if(local && lib==0) {
			ob->id.lib= 0;
			ob->id.flag= LIB_LOCAL;
			new_id(0, (ID *)ob, 0);
		}
		else if(local && lib) {
			obn= copy_object(ob);
			obn->id.us= 0;
			
			sce= G.main->scene.first;
			while(sce) {
				if(sce->id.lib==0) {
					base= sce->base.first;
					while(base) {
						if(base->object==ob) {
							base->object= obn;
							obn->id.us++;
							ob->id.us--;
						}
						base= base->next;
					}
				}
				sce= sce->id.next;
			}
		}
	}
	
	expand_local_object(ob);
}

/* *************** CALC ****************** */

/* there is also a timing calculation in drawobject() */

float bluroffs= 0.0;
int no_speed_curve= 0;

void set_mblur_offs(int blur)
{
	bluroffs= R.r.blurfac*((float)blur);
	bluroffs/= (float)R.r.osa;
}
	
void disable_speed_curve(int val)
{
	no_speed_curve= val;
}

float bsystem_time(Object *ob, Object *par, float cfra, float ofs)
{
	/* returns float ( see frame_to_float in ipo.c) */

	if(no_speed_curve==0) if(ob && ob->ipo) cfra= calc_ipo_time(ob->ipo, cfra);
	
	/* 2nd field */
	if(R.flag & R_SEC_FIELD) {
		if(R.r.mode & R_FIELDSTILL); else cfra+= .5;
	}

	/* motion blur */
	cfra+= bluroffs;

	/* global time */
	cfra*= G.scene->r.framelen;	
	
	/* ofset frames */
	if(ob && (ob->ipoflag & OB_OFFS_PARENT)) {
		if((ob->partype & PARSLOW)==0) cfra-= ob->sf;
	}
	
	cfra-= ofs;

	return cfra;
}

void object_to_mat3(Object *ob, float mat[][3])	/* no parent */
{
	float smat[3][3], vec[3];
	float rmat[3][3];
	float q1[4];
	
	/* size */
	if(ob->ipo) {
		vec[0]= ob->size[0]+ob->dsize[0];
		vec[1]= ob->size[1]+ob->dsize[1];
		vec[2]= ob->size[2]+ob->dsize[2];
		SizeToMat3(vec, smat);
	}
	else {
		SizeToMat3(ob->size, smat);
	}

	/* rot */
	if(ob->transflag & OB_QUAT) {
		if(ob->ipo) {
			QuatMul(q1, ob->quat, ob->dquat);
			QuatToMat3(q1, rmat);
		}
		else {
			QuatToMat3(ob->quat, rmat);
		}
	}
	else {
		if(ob->ipo) {
			vec[0]= ob->rot[0]+ob->drot[0];
			vec[1]= ob->rot[1]+ob->drot[1];
			vec[2]= ob->rot[2]+ob->drot[2];
			EulToMat3(vec, rmat);
		}
		else {
			EulToMat3(ob->rot, rmat);
		}
	}
	Mat3MulMat3(mat, rmat, smat);
}

void object_to_mat4(Object *ob, float mat[][4])
{
	float tmat[3][3];
	
	object_to_mat3(ob, tmat);
	
	Mat4CpyMat3(mat, tmat);
	
	VECCOPY(mat[3], ob->loc);
	if(ob->ipo) {
		mat[3][0]+= ob->dloc[0];
		mat[3][1]+= ob->dloc[1];
		mat[3][2]+= ob->dloc[2];
	}
}

int enable_cu_speed= 1;

void ob_parcurve(Object *ob, Object *par, float mat[][4])
{
	Curve *cu;
	float q[4], vec[4], dir[3], *quat, x1, ctime;
	
	Mat4One(mat);
	
	cu= par->data;
	if(cu->path==0 || cu->path->data==0) calc_curvepath(par);
	if(cu->path==0) return;
	
	/* catch exceptions: curve paths used as a duplicator */
	if(enable_cu_speed) {
		ctime= bsystem_time(ob, par, (float)G.scene->r.cfra, 0.0);
		
		if(calc_ipo_spec(cu->ipo, CU_SPEED, &ctime)==0) {
			ctime /= cu->pathlen;
			CLAMP(ctime, 0.0, 1.0);
		}
	}
	else {
		ctime= G.scene->r.cfra - ob->sf;
		ctime /= cu->pathlen;
		
		CLAMP(ctime, 0.0, 1.0);
	}

 	if( where_on_path(par, ctime, vec, dir) ) {

		if(cu->flag & CU_FOLLOW) {
			quat= vectoquat(dir, ob->trackflag, ob->upflag);

			Normalise(dir);
			q[0]= (float)cos(0.5*vec[3]);
			x1= (float)sin(0.5*vec[3]);
			q[1]= -x1*dir[0];
			q[2]= -x1*dir[1];
			q[3]= -x1*dir[2];
			QuatMul(quat, q, quat);
			
			QuatToMat4(quat, mat);
		}
		
		VECCOPY(mat[3], vec);
		
	}
}

void ob_parbone(Object *ob, Object *par, float mat[][4])
{	
	Bone *bone;
	bArmature *arm;

	Mat4One(mat);
	arm=get_armature(par);
	if (!arm)
		return;

	/* Make sure the bone is still valid */
	bone = get_named_bone(arm, ob->parsubstr);
	if (!bone){
		printf ("Lost bone %s\n", ob->parsubstr);
		return;
	}

	apply_pose_armature(arm, par->pose, 1);	/* Hopefully can set doit parameter in the future */
	where_is_bone (par, bone); 

	/* Translate by negative bone */
	get_objectspace_bone_matrix(bone, mat, 0, 1);

}

void ob_parlimb(Object *ob, Object *par, float mat[][4])
{	
	Ika *ika;
	Limb *li;
	float ang=0.0;
	int cur=0;
	
	/* in local ob space */
	Mat4One(mat);
	
	ika= par->data;
	li= ika->limbbase.first;
	while(li) {
		ang+= li->alpha;
		if(cur==ob->par1 || li->next==0) break;
		
		cur++;
		li= li->next;
	}
	
	mat[0][0]= (float)cos(ang);
	mat[1][0]= (float)-sin(ang);
	mat[0][1]= (float)sin(ang);
	mat[1][1]= (float)cos(ang);
	
	mat[3][0]= li->eff[0];
	mat[3][1]= li->eff[1];
	
}

void give_parvert(Object *par, int nr, float *vec)
{
	EditMesh *em = G.editMesh;
	Mesh *me;
	EditVert *eve;
/*  	extern ListBase editNurb; already in bad lev calls */
	Nurb *nu;
	Curve *cu;
	BPoint *bp;
	DispList *dl;
	BezTriple *bezt;
	float *fp;
	int a, count;
	
	vec[0]=vec[1]=vec[2]= 0.0;
	
	if(par->type==OB_MESH) {
		if(par==G.obedit) {
			if(nr >= G.totvert) nr= 0;

			count= 0;
			eve= em->verts.first;
			while(eve) {
				if(count==nr) {
					memcpy(vec, eve->co, 12);
					break;
				}
				eve= eve->next;
				count++;
			}
		}
		else {
			me= par->data;
			if(me->totvert) {
				if(nr >= me->totvert) nr= 0;
				
				/* is there a deform */
				dl= find_displist(&par->disp, DL_VERTS);
				if(dl) {
					fp= dl->verts+3*nr;
					VECCOPY(vec, fp);
				}
				else {
					MVert *mvert= me->mvert + nr;
					VECCOPY(vec, mvert->co);
				}
			}
		}
	}
	else if ELEM(par->type, OB_CURVE, OB_SURF) {

		cu= par->data;
		nu= cu->nurb.first;
		if(par==G.obedit) nu= editNurb.first;
		
		count= 0;
		while(nu) {
			if((nu->type & 7)==CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(count==nr) {
						VECCOPY(vec, bezt->vec[1]);
						break;
					}
					count++;
					bezt++;
				}
			}
			else {
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				while(a--) {
					if(count==nr) {
						memcpy(vec, bp->vec, 12);
						break;
					}
					count++;
					bp++;
				}
			}
			nu= nu->next;
		}

	}
	else if(par->type==OB_IKA) {
		Ika *ika= par->data;
		Limb *li= ika->limbbase.first;
		int cur= 1;
		if(nr) {
			while(li) {
				if(cur==nr || li->next==0) break;
				cur++;
				li= li->next;
			}
			vec[0]= li->eff[0];
			vec[1]= li->eff[1];
		}
	}
	else return;
}

void ob_parvert3(Object *ob, Object *par, float mat[][4])
{
	float cmat[3][3], v1[3], v2[3], v3[3], q[4];

	/* in local ob space */
	Mat4One(mat);
	
	if ELEM3(par->type, OB_MESH, OB_SURF, OB_CURVE) {
		
		give_parvert(par, ob->par1, v1);
		give_parvert(par, ob->par2, v2);
		give_parvert(par, ob->par3, v3);
				
		triatoquat(v1, v2, v3, q);
		QuatToMat3(q, cmat);
		Mat4CpyMat3(mat, cmat);
		
		if(ob->type==OB_CURVE) {
			VECCOPY(mat[3], v1);
		}
		else {
			VecAddf(mat[3], v1, v2);
			VecAddf(mat[3], mat[3], v3);
			VecMulf(mat[3], 0.3333333f);
		}
	}
}

static int no_parent_ipo=0;
void set_no_parent_ipo(int val)
{
	no_parent_ipo= val;
}

static float timefac= 1.0;		/* 50 Hz, dtime:2 */
void set_dtime(int dtime)
{
	timefac= ((float)(dtime-1))/2.0f;
}

static int during_script_flag=0;
void disable_where_script(short on)
{
	during_script_flag= on;	
}

int during_script(void) {
	return during_script_flag;
}

void where_is_object_time(Object *ob, float ctime)
{
	Object *par;
	float *fp1, *fp2, slowmat[4][4] = MAT4_UNITY;
	float stime, fac1, fac2, vec[3];
	int a;
	int pop; 
	
	/* new version: correct parent+vertexparent and track+parent */
	/* this one only calculates direct attached parent and track */
	/* hij is sneller, maar moet wel de timeoffs in de gaten houden */
	
	if(ob==0) return;

	if( ctime != ob->ctime) {
		ob->ctime= ctime;
		
		if(ob->ipo) {
			
			stime= bsystem_time(ob, 0, ctime, 0.0);

			calc_ipo(ob->ipo, stime);
			execute_ipo((ID *)ob, ob->ipo);
		}			
	}


	if(ob->type==OB_IKA) {
		Ika *ika= ob->data;
		if(ika->parent) where_is_object_time(ika->parent, ctime);
	}

	if(ob->parent) {
		par= ob->parent;

		if(ob->ipoflag & OB_OFFS_PARENT) ctime-= ob->sf;
		
		pop= 0;
		if(no_parent_ipo==0 && ctime != par->ctime) {
		
			// alleen voor ipo systemen? 
			pushdata(par, sizeof(Object));
			pop= 1;
			
			where_is_object_time(par, ctime);
		}
		
		solve_parenting(ob, par, slowmat, 0);

		if(pop) {
			poplast(par);
		}
		
		if(ob->partype & PARSLOW) {
			// framerate meetellen 

			fac1= (float)(timefac/(1.0+ fabs(ob->sf)));
			if(fac1>=1.0) return;
			fac2= 1.0f-fac1;
			
			fp1= ob->obmat[0];
			fp2= slowmat[0];
			for(a=0; a<16; a++, fp1++, fp2++) {
				fp1[0]= fac1*fp1[0] + fac2*fp2[0];
			}
		}
	
	}
	else {
		object_to_mat4(ob, ob->obmat);
	}

	/* Handle tracking */
	if(ob->track) {
		if( ctime != ob->track->ctime) where_is_object_time(ob->track, ctime);
		solve_tracking (ob, ob->track->obmat);
		
	}

	solve_constraints (ob, TARGET_OBJECT, NULL, ctime);

	if(ob->scriptlink.totscript && !during_script()) {
		BPY_do_pyscript((ID *)ob, SCRIPT_REDRAW);
	}
	
	/* set negative scale flag in object */
	Crossf(vec, ob->obmat[0], ob->obmat[1]);
	if( Inpf(vec, ob->obmat[2]) < 0.0 ) ob->transflag |= OB_NEG_SCALE;
	else ob->transflag &= ~OB_NEG_SCALE;
}

static void solve_parenting (Object *ob, Object *par, float slowmat[][4], int simul)
{
	float totmat[4][4];
	float tmat[4][4];
	float obmat[4][4];
	float vec[3];
	int ok;

	object_to_mat4(ob, obmat);
	
	if(ob->partype & PARSLOW) Mat4CpyMat4(slowmat, ob->obmat);
	

	switch(ob->partype & PARTYPE) {
	case PAROBJECT:
		ok= 0;
		if(par->type==OB_CURVE) {
			if( ((Curve *)par->data)->flag & CU_PATH ) {
				ob_parcurve(ob, par, tmat);
				ok= 1;
			}
		}
		
		if(ok) Mat4MulSerie(totmat, par->obmat, tmat, 
			NULL, NULL, NULL, NULL, NULL, NULL);
		else Mat4CpyMat4(totmat, par->obmat);
		
		break;
	case PARBONE:
		ob_parbone(ob, par, tmat);
		Mat4MulSerie(totmat, par->obmat, tmat,         
			NULL, NULL, NULL, NULL, NULL, NULL);
		break;
	case PARLIMB:	
		ob_parlimb(ob, par, tmat);
		Mat4MulSerie(totmat, par->obmat, tmat,         
			NULL, NULL, NULL, NULL, NULL, NULL);
		break;
		
	case PARVERT1:
		Mat4One(totmat);
		if (simul){
			VECCOPY(totmat[3], par->obmat[3]);
		}
		else{
			give_parvert(par, ob->par1, vec);
			VecMat4MulVecfl(totmat[3], par->obmat, vec);
		}
		break;
	case PARVERT3:
		ob_parvert3(ob, par, tmat);
		
		Mat4MulSerie(totmat, par->obmat, tmat,         
			NULL, NULL, NULL, NULL, NULL, NULL);
		break;
		
	case PARSKEL:
#if 0
		if (ob!=G.obedit)
			Mat4One(totmat);
		else
			Mat4CpyMat4(totmat, par->obmat);
		break;
#else
			Mat4CpyMat4(totmat, par->obmat);
#endif
	}
	
	// totaal 
	Mat4MulSerie(tmat, totmat, ob->parentinv,         
		NULL, NULL, NULL, NULL, NULL, NULL);
	Mat4MulSerie(ob->obmat, tmat, obmat,         
		NULL, NULL, NULL, NULL, NULL, NULL);
	
	if (simul){

	}
	else{
		// >>>>>>>>>>>>>>>>>>
		// dit is een extern bruikbare originmat 
		Mat3CpyMat4(originmat, tmat);
		
		// origin, voor hulplijntje 
		if( (ob->partype & 15)==PARSKEL ) {
			VECCOPY(ob->orig, par->obmat[3]);
		}
		else {
			VECCOPY(ob->orig, totmat[3]);
		}
	}

}
void solve_tracking (Object *ob, float targetmat[][4])
{
	float *quat;
	float vec[3];
	float totmat[3][3];
	float tmat[4][4];
	
	VecSubf(vec, ob->obmat[3], targetmat[3]);
	quat= vectoquat(vec, ob->trackflag, ob->upflag);
	QuatToMat3(quat, totmat);
	
	if(ob->parent && (ob->transflag & OB_POWERTRACK)) {
		/* 'temporal' : clear parent info */
		object_to_mat4(ob, tmat);
		tmat[0][3]= ob->obmat[0][3];
		tmat[1][3]= ob->obmat[1][3];
		tmat[2][3]= ob->obmat[2][3];
		tmat[3][0]= ob->obmat[3][0];
		tmat[3][1]= ob->obmat[3][1];
		tmat[3][2]= ob->obmat[3][2];
		tmat[3][3]= ob->obmat[3][3];
	}
	else Mat4CpyMat4(tmat, ob->obmat);
	
	Mat4MulMat34(ob->obmat, totmat, tmat);

}

void where_is_object(Object *ob)
{
	
	/* these have been mem copied */
	if(ob->flag & OB_FROMDUPLI) return;
	
	where_is_object_time(ob, (float)G.scene->r.cfra);
}


void where_is_object_simul(Object *ob)
/* was written for the old game engine (until 2.04) */
/* It seems that this function is only called
for a lamp that is the child of another object */
{
	Object *par;
	Ipo *ipo;
	float *fp1, *fp2;
	float slowmat[4][4];
	float fac1, fac2;
	int a;
	
	/* NO TIMEOFFS */
	
	/* no ipo! (because of dloc and realtime-ipos) */
	ipo= ob->ipo;
	ob->ipo= NULL;

	if(ob->parent) {
		par= ob->parent;
		
		solve_parenting(ob, par, slowmat, 1);

		if(ob->partype & PARSLOW) {

			fac1= (float)(1.0/(1.0+ fabs(ob->sf)));
			fac2= 1.0f-fac1;
			fp1= ob->obmat[0];
			fp2= slowmat[0];
			for(a=0; a<16; a++, fp1++, fp2++) {
				fp1[0]= fac1*fp1[0] + fac2*fp2[0];
			}
		}
		
	}
	else {
		object_to_mat4(ob, ob->obmat);
	}
	
	if(ob->track) 
		solve_tracking(ob, ob->track->obmat);

	solve_constraints(ob, TARGET_OBJECT, NULL, G.scene->r.cfra);
	
	/*  WATCH IT!!! */
	ob->ipo= ipo;
	
}
extern void Mat4BlendMat4(float out[][4], float dst[][4], float src[][4], float srcweight);

void solve_constraints (Object *ob, short obtype, void *obdata, float ctime)
{
	bConstraint *con;
	float tmat[4][4], focusmat[4][4], lastmat[4][4];
	int i, clear=1, tot=0;
	float	a=0;
	float	aquat[4], quat[4];
	float	aloc[3], loc[3];
	float	asize[3], size[3];
	float	oldmat[4][4];
	float	smat[3][3], rmat[3][3], mat[3][3];
	float enf;

	for (con = ob->constraints.first; con; con=con->next){
		/* Clear accumulators if necessary*/
		if (clear){
			clear=0;
			a=0;
			tot=0;
			memset(aquat, 0, sizeof(float)*4);
			memset(aloc, 0, sizeof(float)*3);
			memset(asize, 0, sizeof(float)*3);
		}
		
		/* Check this constraint only if it has some enforcement */
		if (!(con->flag & CONSTRAINT_DISABLE))
		{
			if (con->enforce==0)
				enf = 0.001f;
				enf = con->enforce;

			/* Get the targetmat */
			get_constraint_target(con, obtype, obdata, tmat, size, ctime);
			
			Mat4CpyMat4(focusmat, tmat);
			
			/* Extract the components & accumulate */
			Mat4ToQuat(focusmat, quat);
			VECCOPY(loc, focusmat[3]);
			Mat3CpyMat4(mat, focusmat);
			Mat3ToSize(mat, size);
			
			a+=enf;
			tot++;
			
			for(i=0; i<3; i++){
				aquat[i+1]+=(quat[i+1]) * enf;
				aloc[i]+=(loc[i]) * enf;
				asize[i]+=(size[i]-1.0f) * enf;
			}
			aquat[0]+=(quat[0])*enf;
			Mat4CpyMat4(lastmat, focusmat);
		}
		
		/* If the next constraint is not the same type (or there isn't one),
		 *	then evaluate the accumulator & request a clear */
		if ((!con->next)||(con->next && con->next->type!=con->type))
		{
			clear=1;
			Mat4CpyMat4(oldmat, ob->obmat);

			/*	If we have several inputs, do a blend of them */
			if (tot){
				if (tot>1){
					if (a){
						for (i=0; i<3; i++){
							asize[i]=1.0f + (asize[i]/(a));
							aloc[i]=(aloc[i]/a);
						}
						
						NormalQuat(aquat);
						
						QuatToMat3(aquat, rmat);
						SizeToMat3(asize, smat);
						Mat3MulMat3(mat, rmat, smat);
						Mat4CpyMat3(focusmat, mat);
						VECCOPY(focusmat[3], aloc);

						evaluate_constraint(con, ob, obtype, obdata, focusmat);
					}
					
				}	
				/* If we only have one, blend with the current obmat */
				else{
					float solution[4][4];
					float delta[4][4];
					float imat[4][4];
					float identity[4][4];
					float worldmat[4][4];
					
					if (con->type!=CONSTRAINT_TYPE_KINEMATIC){
						/* If we're not an IK constraint, solve the constraint then blend it to the previous one */
						evaluate_constraint(con, ob, obtype, obdata, lastmat);
						
						Mat4CpyMat4 (solution, ob->obmat);

						/* Interpolate the enforcement */					
						Mat4Invert (imat, oldmat);
						Mat4MulMat4 (delta, solution, imat);
						
						if (a<1.0) {
							Mat4One(identity);
							Mat4BlendMat4(delta, identity, delta, a);
						}
						Mat4MulMat4 (ob->obmat, delta, oldmat);

					}
					else{
						/* Interpolate the target between the chain's unconstrained endpoint and the effector loc */
						if (obtype==TARGET_BONE){
							get_objectspace_bone_matrix(obdata, oldmat, 1, 1);
							
							Mat4MulMat4(worldmat, oldmat, ob->parent->obmat);

							Mat4BlendMat4(focusmat, worldmat, lastmat, a);
							
							evaluate_constraint(con, ob, obtype, obdata, focusmat);
						}
					}
				}
			}
		}
	}	
}

void what_does_parent1(Object *par, int partype, int par1, int par2, int par3)
{

	clear_workob();
	Mat4One(workob.parentinv);
	workob.parent= par;
	if(par) 
		workob.track= par->track;	/* WATCH IT: THATS NOT NICE CODE */
	workob.partype= partype;
	workob.par1= par1;
	workob.par2= par2;
	workob.par3= par3;

	if (par){
		workob.constraints.first = par->constraints.first;
		workob.constraints.last = par->constraints.last;
	}

	where_is_object(&workob);
}


void what_does_parent(Object *ob)
{

	clear_workob();
	Mat4One(workob.obmat);
	Mat4One(workob.parentinv);
	workob.parent= ob->parent;
	workob.track= ob->track;

	workob.trackflag= ob->trackflag;
	workob.upflag= ob->upflag;
	
	workob.partype= ob->partype;
	workob.par1= ob->par1;
	workob.par2= ob->par2;
	workob.par3= ob->par3;

	workob.constraints.first = ob->constraints.first;
	workob.constraints.last = ob->constraints.last;

	strcpy (workob.parsubstr, ob->parsubstr); 

	where_is_object(&workob);
}

BoundBox *unit_boundbox()
{
	BoundBox *bb;
	
	bb= MEM_mallocN(sizeof(BoundBox), "bb");

	bb->vec[0][0]=bb->vec[1][0]=bb->vec[2][0]=bb->vec[3][0]= -1.0;
	bb->vec[4][0]=bb->vec[5][0]=bb->vec[6][0]=bb->vec[7][0]= 1.0;
	
	bb->vec[0][1]=bb->vec[1][1]=bb->vec[4][1]=bb->vec[5][1]= -1.0;
	bb->vec[2][1]=bb->vec[3][1]=bb->vec[6][1]=bb->vec[7][1]= 1.0;

	bb->vec[0][2]=bb->vec[3][2]=bb->vec[4][2]=bb->vec[7][2]= -1.0;
	bb->vec[1][2]=bb->vec[2][2]=bb->vec[5][2]=bb->vec[6][2]= 1.0;
	
	return bb;
}

void minmax_object(Object *ob, float *min, float *max)
{
	BoundBox bb;
	Mesh *me;
	Curve *cu;
	float vec[3];
	int a;
	
	switch(ob->type) {
		
	case OB_CURVE:
	case OB_FONT:
	case OB_SURF:
		cu= ob->data;
		
		if(cu->bb==0) tex_space_curve(cu);
		bb= *(cu->bb);
		
		for(a=0; a<8; a++) {
			Mat4MulVecfl(ob->obmat, bb.vec[a]);
			DO_MINMAX(bb.vec[a], min, max);
		}
		break;

	case OB_MESH:
		me= get_mesh(ob);
		
		if(me) {
			if(me->bb==0) tex_space_mesh(me);
			bb= *(me->bb);
			
			for(a=0; a<8; a++) {
				Mat4MulVecfl(ob->obmat, bb.vec[a]);
				DO_MINMAX(bb.vec[a], min, max);
			}
		}
		if(min[0] < max[0] ) break;
		
		/* else here no break!!!, mesh can be zero sized */
		
	default:
		DO_MINMAX(ob->obmat[3], min, max);

		VECCOPY(vec, ob->obmat[3]);
		VecAddf(vec, vec, ob->size);
		DO_MINMAX(vec, min, max);

		VECCOPY(vec, ob->obmat[3]);
		VecSubf(vec, vec, ob->size);
		DO_MINMAX(vec, min, max);
		break;
	}
}


