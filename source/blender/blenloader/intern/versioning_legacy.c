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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenloader/intern/readfile_pre250.c
 *  \ingroup blenloader
 */


#include "zlib.h"

#include <limits.h>

#ifndef WIN32
#  include <unistd.h> // for read close
#else
#  include <io.h> // for open close read
#  include "winsock2.h"
#  include "BLI_winstuff.h"
#endif

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_armature_types.h"
#include "DNA_actuator_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_nla_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim.h" // NT
#include "DNA_object_types.h"
#include "DNA_property_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_sensor_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_vfont_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"

#include "BKE_armature.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_fcurve.h"
#include "BKE_global.h" // for G
#include "BKE_image.h"
#include "BKE_lattice.h"
#include "BKE_main.h" // for Main
#include "BKE_mesh.h" // for ME_ defines (patching)
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_property.h" // for get_ob_property
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_utildefines.h" // SWITCH_INT DATA ENDB DNA1 O_BINARY GLOB USER TEST REND

#include "IMB_imbuf.h"  // for proxy / timecode versioning stuff

#include "NOD_socket.h"

//XXX #include "BIF_butspace.h" // badlevel, for do_versions, patching event codes
//XXX #include "BIF_filelist.h" // badlevel too, where to move this? - elubie
//XXX #include "BIF_previewrender.h" // bedlelvel, for struct RenderInfo
#include "BLO_readfile.h"
#include "BLO_undofile.h"

#include "RE_engine.h"

#include "readfile.h"

#include "PIL_time.h"

#include <errno.h>

static void vcol_to_fcol(Mesh *me)
{
	MFace *mface;
	unsigned int *mcol, *mcoln, *mcolmain;
	int a;

	if (me->totface==0 || me->mcol==NULL) return;

	mcoln= mcolmain= MEM_mallocN(4*sizeof(int)*me->totface, "mcoln");
	mcol = (unsigned int *)me->mcol;
	mface= me->mface;
	for (a=me->totface; a>0; a--, mface++) {
		mcoln[0]= mcol[mface->v1];
		mcoln[1]= mcol[mface->v2];
		mcoln[2]= mcol[mface->v3];
		mcoln[3]= mcol[mface->v4];
		mcoln+= 4;
	}

	MEM_freeN(me->mcol);
	me->mcol= (MCol *)mcolmain;
}

static int map_223_keybd_code_to_224_keybd_code(int code)
{
	switch (code) {
		case 312:	return 311; /* F12KEY */
		case 159:	return 161; /* PADSLASHKEY */
		case 161:	return 150; /* PAD0 */
		case 154:	return 151; /* PAD1 */
		case 150:	return 152; /* PAD2 */
		case 155:	return 153; /* PAD3 */
		case 151:	return 154; /* PAD4 */
		case 156:	return 155; /* PAD5 */
		case 152:	return 156; /* PAD6 */
		case 157:	return 157; /* PAD7 */
		case 153:	return 158; /* PAD8 */
		case 158:	return 159; /* PAD9 */
		default: return code;
	}
}

static void do_version_bone_head_tail_237(Bone *bone)
{
	Bone *child;
	float vec[3];

	/* head */
	copy_v3_v3(bone->arm_head, bone->arm_mat[3]);

	/* tail is in current local coord system */
	copy_v3_v3(vec, bone->arm_mat[1]);
	mul_v3_fl(vec, bone->length);
	add_v3_v3v3(bone->arm_tail, bone->arm_head, vec);

	for (child= bone->childbase.first; child; child= child->next)
		do_version_bone_head_tail_237(child);
}

static void bone_version_238(ListBase *lb)
{
	Bone *bone;
	
	for (bone= lb->first; bone; bone= bone->next) {
		if (bone->rad_tail==0.0f && bone->rad_head==0.0f) {
			bone->rad_head= 0.25f*bone->length;
			bone->rad_tail= 0.1f*bone->length;
			
			bone->dist-= bone->rad_head;
			if (bone->dist<=0.0f) bone->dist= 0.0f;
		}
		bone_version_238(&bone->childbase);
	}
}

static void bone_version_239(ListBase *lb)
{
	Bone *bone;
	
	for (bone= lb->first; bone; bone= bone->next) {
		if (bone->layer==0)
			bone->layer= 1;
		bone_version_239(&bone->childbase);
	}
}

static void ntree_version_241(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree->type==NTREE_COMPOSIT) {
		for (node= ntree->nodes.first; node; node= node->next) {
			if (node->type==CMP_NODE_BLUR) {
				if (node->storage==NULL) {
					NodeBlurData *nbd= MEM_callocN(sizeof(NodeBlurData), "node blur patch");
					nbd->sizex= node->custom1;
					nbd->sizey= node->custom2;
					nbd->filtertype= R_FILTER_QUAD;
					node->storage= nbd;
				}
			}
			else if (node->type==CMP_NODE_VECBLUR) {
				if (node->storage==NULL) {
					NodeBlurData *nbd= MEM_callocN(sizeof(NodeBlurData), "node blur patch");
					nbd->samples= node->custom1;
					nbd->maxspeed= node->custom2;
					nbd->fac= 1.0f;
					node->storage= nbd;
				}
			}
		}
	}
}

static void ntree_version_242(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree->type==NTREE_COMPOSIT) {
		for (node= ntree->nodes.first; node; node= node->next) {
			if (node->type==CMP_NODE_HUE_SAT) {
				if (node->storage) {
					NodeHueSat *nhs= node->storage;
					if (nhs->val==0.0f) nhs->val= 1.0f;
				}
			}
		}
	}
	else if (ntree->type==NTREE_SHADER) {
		for (node= ntree->nodes.first; node; node= node->next)
			if (node->type == SH_NODE_GEOMETRY && node->storage == NULL)
				node->storage= MEM_callocN(sizeof(NodeGeometry), "NodeGeometry");
	}
	
}

static void ntree_version_245(FileData *fd, Library *lib, bNodeTree *ntree)
{
	bNode *node;
	NodeTwoFloats *ntf;
	ID *nodeid;
	Image *image;
	ImageUser *iuser;

	if (ntree->type==NTREE_COMPOSIT) {
		for (node= ntree->nodes.first; node; node= node->next) {
			if (node->type == CMP_NODE_ALPHAOVER) {
				if (!node->storage) {
					ntf= MEM_callocN(sizeof(NodeTwoFloats), "NodeTwoFloats");
					node->storage= ntf;
					if (node->custom1)
						ntf->x= 1.0f;
				}
			}
			
			/* fix for temporary flag changes during 245 cycle */
			nodeid= blo_do_versions_newlibadr(fd, lib, node->id);
			if (node->storage && nodeid && GS(nodeid->name) == ID_IM) {
				image= (Image*)nodeid;
				iuser= node->storage;
				if (iuser->flag & IMA_OLD_PREMUL) {
					iuser->flag &= ~IMA_OLD_PREMUL;
					iuser->flag |= IMA_DO_PREMUL;
				}
				if (iuser->flag & IMA_DO_PREMUL) {
					image->flag &= ~IMA_OLD_PREMUL;
					image->flag |= IMA_DO_PREMUL;
				}
			}
		}
	}
}

static void idproperties_fix_groups_lengths_recurse(IDProperty *prop)
{
	IDProperty *loop;
	int i;
	
	for (loop=prop->data.group.first, i=0; loop; loop=loop->next, i++) {
		if (loop->type == IDP_GROUP) idproperties_fix_groups_lengths_recurse(loop);
	}
	
	if (prop->len != i) {
		printf("Found and fixed bad id property group length.\n");
		prop->len = i;
	}
}

static void idproperties_fix_group_lengths(ListBase idlist)
{
	ID *id;
	
	for (id=idlist.first; id; id=id->next) {
		if (id->properties) {
			idproperties_fix_groups_lengths_recurse(id->properties);
		}
	}
}

static void alphasort_version_246(FileData *fd, Library *lib, Mesh *me)
{
	Material *ma;
	MFace *mf;
	MTFace *tf;
	int a, b, texalpha;

	/* verify we have a tface layer */
	for (b=0; b<me->fdata.totlayer; b++)
		if (me->fdata.layers[b].type == CD_MTFACE)
			break;
	
	if (b == me->fdata.totlayer)
		return;

	/* if we do, set alpha sort if the game engine did it before */
	for (a=0, mf=me->mface; a<me->totface; a++, mf++) {
		if (mf->mat_nr < me->totcol) {
			ma= blo_do_versions_newlibadr(fd, lib, me->mat[mf->mat_nr]);
			texalpha = 0;

			/* we can't read from this if it comes from a library,
			 * because direct_link might not have happened on it,
			 * so ma->mtex is not pointing to valid memory yet */
			if (ma && ma->id.lib)
				ma= NULL;

			for (b=0; ma && b<MAX_MTEX; b++)
				if (ma->mtex && ma->mtex[b] && ma->mtex[b]->mapto & MAP_ALPHA)
					texalpha = 1;
		}
		else {
			ma= NULL;
			texalpha = 0;
		}

		for (b=0; b<me->fdata.totlayer; b++) {
			if (me->fdata.layers[b].type == CD_MTFACE) {
				tf = ((MTFace*)me->fdata.layers[b].data) + a;

				tf->mode &= ~TF_ALPHASORT;
				if (ma && (ma->mode & MA_ZTRANSP))
					if (ELEM(tf->transp, TF_ALPHA, TF_ADD) || (texalpha && (tf->transp != TF_CLIP)))
						tf->mode |= TF_ALPHASORT;
			}
		}
	}
}

static void customdata_version_242(Mesh *me)
{
	CustomDataLayer *layer;
	MTFace *mtf;
	MCol *mcol;
	TFace *tf;
	int a, mtfacen, mcoln;

	if (!me->vdata.totlayer) {
		CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, me->mvert, me->totvert);

		if (me->msticky)
			CustomData_add_layer(&me->vdata, CD_MSTICKY, CD_ASSIGN, me->msticky, me->totvert);
		if (me->dvert)
			CustomData_add_layer(&me->vdata, CD_MDEFORMVERT, CD_ASSIGN, me->dvert, me->totvert);
	}

	if (!me->edata.totlayer)
		CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, me->medge, me->totedge);
	
	if (!me->fdata.totlayer) {
		CustomData_add_layer(&me->fdata, CD_MFACE, CD_ASSIGN, me->mface, me->totface);

		if (me->tface) {
			if (me->mcol)
				MEM_freeN(me->mcol);

			me->mcol= CustomData_add_layer(&me->fdata, CD_MCOL, CD_CALLOC, NULL, me->totface);
			me->mtface= CustomData_add_layer(&me->fdata, CD_MTFACE, CD_CALLOC, NULL, me->totface);

			mtf= me->mtface;
			mcol= me->mcol;
			tf= me->tface;

			for (a=0; a < me->totface; a++, mtf++, tf++, mcol+=4) {
				memcpy(mcol, tf->col, sizeof(tf->col));
				memcpy(mtf->uv, tf->uv, sizeof(tf->uv));

				mtf->flag= tf->flag;
				mtf->unwrap= tf->unwrap;
				mtf->mode= tf->mode;
				mtf->tile= tf->tile;
				mtf->tpage= tf->tpage;
				mtf->transp= tf->transp;
			}

			MEM_freeN(me->tface);
			me->tface= NULL;
		}
		else if (me->mcol) {
			me->mcol= CustomData_add_layer(&me->fdata, CD_MCOL, CD_ASSIGN, me->mcol, me->totface);
		}
	}

	if (me->tface) {
		MEM_freeN(me->tface);
		me->tface= NULL;
	}

	for (a=0, mtfacen=0, mcoln=0; a < me->fdata.totlayer; a++) {
		layer= &me->fdata.layers[a];

		if (layer->type == CD_MTFACE) {
			if (layer->name[0] == 0) {
				if (mtfacen == 0) strcpy(layer->name, "UVMap");
				else BLI_snprintf(layer->name, sizeof(layer->name), "UVMap.%.3d", mtfacen);
			}
			mtfacen++;
		}
		else if (layer->type == CD_MCOL) {
			if (layer->name[0] == 0) {
				if (mcoln == 0) strcpy(layer->name, "Col");
				else BLI_snprintf(layer->name, sizeof(layer->name), "Col.%.3d", mcoln);
			}
			mcoln++;
		}
	}

	mesh_update_customdata_pointers(me, TRUE);
}

/*only copy render texface layer from active*/
static void customdata_version_243(Mesh *me)
{
	CustomDataLayer *layer;
	int a;

	for (a=0; a < me->fdata.totlayer; a++) {
		layer= &me->fdata.layers[a];
		layer->active_rnd = layer->active;
	}
}

/* struct NodeImageAnim moved to ImageUser, and we make it default available */
static void do_version_ntree_242_2(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree->type==NTREE_COMPOSIT) {
		for (node= ntree->nodes.first; node; node= node->next) {
			if (ELEM3(node->type, CMP_NODE_IMAGE, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				/* only image had storage */
				if (node->storage) {
					NodeImageAnim *nia= node->storage;
					ImageUser *iuser= MEM_callocN(sizeof(ImageUser), "ima user node");

					iuser->frames= nia->frames;
					iuser->sfra= nia->sfra;
					iuser->offset= nia->nr-1;
					iuser->cycl= nia->cyclic;
					iuser->fie_ima= 2;
					iuser->ok= 1;
					
					node->storage= iuser;
					MEM_freeN(nia);
				}
				else {
					ImageUser *iuser= node->storage= MEM_callocN(sizeof(ImageUser), "node image user");
					iuser->sfra= 1;
					iuser->fie_ima= 2;
					iuser->ok= 1;
				}
			}
		}
	}
}

static void do_version_free_effect_245(Effect *eff)
{
	PartEff *paf;

	if (eff->type==EFF_PARTICLE) {
		paf= (PartEff *)eff;
		if (paf->keys) MEM_freeN(paf->keys);
	}
	MEM_freeN(eff);
}

static void do_version_free_effects_245(ListBase *lb)
{
	Effect *eff;

	eff= lb->first;
	while (eff) {
		BLI_remlink(lb, eff);
		do_version_free_effect_245(eff);
		eff= lb->first;
	}
}

PartEff *blo_do_version_give_parteff_245(Object *ob)
{
	PartEff *paf;

	paf= ob->effect.first;
	while (paf) {
		if (paf->type==EFF_PARTICLE) return paf;
		paf= paf->next;
	}
	return NULL;
}

/* NOTE: this version patch is intended for versions < 2.52.2, but was initially introduced in 2.27 already */
void blo_do_version_old_trackto_to_constraints(Object *ob)
{
	/* create new trackto constraint from the relationship */
	if (ob->track) {
		bConstraint *con= add_ob_constraint(ob, "AutoTrack", CONSTRAINT_TYPE_TRACKTO);
		bTrackToConstraint *data = con->data;
		
		/* copy tracking settings from the object */
		data->tar = ob->track;
		data->reserved1 = ob->trackflag;
		data->reserved2 = ob->upflag;
	}
	
	/* clear old track setting */
	ob->track = NULL;
}

void blo_do_versions_pre250(FileData *fd, Library *lib, Main *main)
{
	/* WATCH IT!!!: pointers from libdata have not been converted */

	if (main->versionfile == 100) {
		/* tex->extend and tex->imageflag have changed: */
		Tex *tex = main->tex.first;
		while (tex) {
			if (tex->id.flag & LIB_NEEDLINK) {

				if (tex->extend==0) {
					if (tex->xrepeat || tex->yrepeat) tex->extend= TEX_REPEAT;
					else {
						tex->extend= TEX_EXTEND;
						tex->xrepeat= tex->yrepeat= 1;
					}
				}

			}
			tex= tex->id.next;
		}
	}
	if (main->versionfile <= 101) {
		/* frame mapping */
		Scene *sce = main->scene.first;
		while (sce) {
			sce->r.framapto= 100;
			sce->r.images= 100;
			sce->r.framelen= 1.0;
			sce= sce->id.next;
		}
	}
	if (main->versionfile <= 102) {
		/* init halo's at 1.0 */
		Material *ma = main->mat.first;
		while (ma) {
			ma->add= 1.0;
			ma= ma->id.next;
		}
	}
	if (main->versionfile <= 103) {
		/* new variable in object: colbits */
		Object *ob = main->object.first;
		int a;
		while (ob) {
			ob->colbits= 0;
			if (ob->totcol) {
				for (a=0; a<ob->totcol; a++) {
					if (ob->mat[a]) ob->colbits |= (1<<a);
				}
			}
			ob= ob->id.next;
		}
	}
	if (main->versionfile <= 104) {
		/* timeoffs moved */
		Object *ob = main->object.first;
		while (ob) {
			if (ob->transflag & 1) {
				ob->transflag -= 1;
				//ob->ipoflag |= OB_OFFS_OB;
			}
			ob= ob->id.next;
		}
	}
	if (main->versionfile <= 105) {
		Object *ob = main->object.first;
		while (ob) {
			ob->dupon= 1; ob->dupoff= 0;
			ob->dupsta= 1; ob->dupend= 100;
			ob= ob->id.next;
		}
	}
	if (main->versionfile <= 106) {
		/* mcol changed */
		Mesh *me = main->mesh.first;
		while (me) {
			if (me->mcol) vcol_to_fcol(me);
			me= me->id.next;
		}

	}
	if (main->versionfile <= 107) {
		Object *ob;
		Scene *sce = main->scene.first;
		while (sce) {
			sce->r.mode |= R_GAMMA;
			sce= sce->id.next;
		}
		ob= main->object.first;
		while (ob) {
			//ob->ipoflag |= OB_OFFS_PARENT;
			if (ob->dt==0) ob->dt= OB_SOLID;
			ob= ob->id.next;
		}

	}
	if (main->versionfile <= 109) {
		/* new variable: gridlines */
		bScreen *sc = main->screen.first;
		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;

						if (v3d->gridlines==0) v3d->gridlines= 20;
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}
	if (main->versionfile <= 113) {
		Material *ma = main->mat.first;
		while (ma) {
			if (ma->flaresize==0.0f) ma->flaresize= 1.0f;
			ma->subsize= 1.0f;
			ma->flareboost= 1.0f;
			ma= ma->id.next;
		}
	}

	if (main->versionfile <= 134) {
		Tex *tex = main->tex.first;
		while (tex) {
			if ((tex->rfac == 0.0f) &&
				(tex->gfac == 0.0f) &&
				(tex->bfac == 0.0f)) {
				tex->rfac = 1.0f;
				tex->gfac = 1.0f;
				tex->bfac = 1.0f;
				tex->filtersize = 1.0f;
			}
			tex = tex->id.next;
		}
	}
	if (main->versionfile <= 140) {
		/* r-g-b-fac in texture */
		Tex *tex = main->tex.first;
		while (tex) {
			if ((tex->rfac == 0.0f) &&
				(tex->gfac == 0.0f) &&
				(tex->bfac == 0.0f)) {
				tex->rfac = 1.0f;
				tex->gfac = 1.0f;
				tex->bfac = 1.0f;
				tex->filtersize = 1.0f;
			}
			tex = tex->id.next;
		}
	}
	if (main->versionfile <= 153) {
		Scene *sce = main->scene.first;
		while (sce) {
			if (sce->r.blurfac==0.0f) sce->r.blurfac= 1.0f;
			sce= sce->id.next;
		}
	}
	if (main->versionfile <= 163) {
		Scene *sce = main->scene.first;
		while (sce) {
			if (sce->r.frs_sec==0) sce->r.frs_sec= 25;
			sce= sce->id.next;
		}
	}
	if (main->versionfile <= 164) {
		Mesh *me= main->mesh.first;
		while (me) {
			me->smoothresh= 30;
			me= me->id.next;
		}
	}
	if (main->versionfile <= 165) {
		Mesh *me= main->mesh.first;
		TFace *tface;
		int nr;
		char *cp;

		while (me) {
			if (me->tface) {
				nr= me->totface;
				tface= me->tface;
				while (nr--) {
					cp= (char *)&tface->col[0];
					if (cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if (cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if (cp[3]>126) cp[3]= 255; else cp[3]*=2;
					cp= (char *)&tface->col[1];
					if (cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if (cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if (cp[3]>126) cp[3]= 255; else cp[3]*=2;
					cp= (char *)&tface->col[2];
					if (cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if (cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if (cp[3]>126) cp[3]= 255; else cp[3]*=2;
					cp= (char *)&tface->col[3];
					if (cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if (cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if (cp[3]>126) cp[3]= 255; else cp[3]*=2;

					tface++;
				}
			}
			me= me->id.next;
		}
	}

	if (main->versionfile <= 169) {
		Mesh *me= main->mesh.first;
		while (me) {
			if (me->subdiv==0) me->subdiv= 1;
			me= me->id.next;
		}
	}

	if (main->versionfile <= 169) {
		bScreen *sc= main->screen.first;
		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_IPO) {
						SpaceIpo *sipo= (SpaceIpo*) sl;
						sipo->v2d.max[0]= 15000.0;
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}

	if (main->versionfile <= 170) {
		Object *ob = main->object.first;
		PartEff *paf;
		while (ob) {
			paf = blo_do_version_give_parteff_245(ob);
			if (paf) {
				if (paf->staticstep == 0) {
					paf->staticstep= 5;
				}
			}
			ob = ob->id.next;
		}
	}

	if (main->versionfile <= 171) {
		bScreen *sc= main->screen.first;
		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_TEXT) {
						SpaceText *st= (SpaceText*) sl;
						st->lheight= 12;
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}

	if (main->versionfile <= 173) {
		int a, b;
		Mesh *me= main->mesh.first;
		while (me) {
			if (me->tface) {
				TFace *tface= me->tface;
				for (a=0; a<me->totface; a++, tface++) {
					for (b=0; b<4; b++) {
						tface->uv[b][0]/= 32767.0f;
						tface->uv[b][1]/= 32767.0f;
					}
				}
			}
			me= me->id.next;
		}
	}

	if (main->versionfile <= 191) {
		Object *ob= main->object.first;
		Material *ma = main->mat.first;

		/* let faces have default add factor of 0.0 */
		while (ma) {
		  if (!(ma->mode & MA_HALO)) ma->add = 0.0;
		  ma = ma->id.next;
		}

		while (ob) {
			ob->mass= 1.0f;
			ob->damping= 0.1f;
			/*ob->quat[1]= 1.0f;*/ /* quats arnt used yet */
			ob= ob->id.next;
		}
	}

	if (main->versionfile <= 193) {
		Object *ob= main->object.first;
		while (ob) {
			ob->inertia= 1.0f;
			ob->rdamping= 0.1f;
			ob= ob->id.next;
		}
	}

	if (main->versionfile <= 196) {
		Mesh *me= main->mesh.first;
		int a, b;
		while (me) {
			if (me->tface) {
				TFace *tface= me->tface;
				for (a=0; a<me->totface; a++, tface++) {
					for (b=0; b<4; b++) {
						tface->mode |= TF_DYNAMIC;
						tface->mode &= ~TF_INVISIBLE;
					}
				}
			}
			me= me->id.next;
		}
	}

	if (main->versionfile <= 200) {
		Object *ob= main->object.first;
		while (ob) {
			ob->scaflag = ob->gameflag & (OB_DO_FH|OB_ROT_FH|OB_ANISOTROPIC_FRICTION|OB_GHOST|OB_RIGID_BODY|OB_BOUNDS);
				/* 64 is do_fh */
			ob->gameflag &= ~(OB_ROT_FH|OB_ANISOTROPIC_FRICTION|OB_GHOST|OB_RIGID_BODY|OB_BOUNDS);
			ob = ob->id.next;
		}
	}

	if (main->versionfile <= 201) {
		/* add-object + end-object are joined to edit-object actuator */
		Object *ob = main->object.first;
		bProperty *prop;
		bActuator *act;
		bIpoActuator *ia;
		bEditObjectActuator *eoa;
		bAddObjectActuator *aoa;
		while (ob) {
			act = ob->actuators.first;
			while (act) {
				if (act->type==ACT_IPO) {
					ia= act->data;
					prop= get_ob_property(ob, ia->name);
					if (prop) {
						ia->type= ACT_IPO_FROM_PROP;
					}
				}
				else if (act->type==ACT_ADD_OBJECT) {
					aoa= act->data;
					eoa= MEM_callocN(sizeof(bEditObjectActuator), "edit ob act");
					eoa->type= ACT_EDOB_ADD_OBJECT;
					eoa->ob= aoa->ob;
					eoa->time= aoa->time;
					MEM_freeN(aoa);
					act->data= eoa;
					act->type= act->otype= ACT_EDIT_OBJECT;
				}
				else if (act->type==ACT_END_OBJECT) {
					eoa= MEM_callocN(sizeof(bEditObjectActuator), "edit ob act");
					eoa->type= ACT_EDOB_END_OBJECT;
					act->data= eoa;
					act->type= act->otype= ACT_EDIT_OBJECT;
				}
				act= act->next;
			}
			ob = ob->id.next;
		}
	}

	if (main->versionfile <= 202) {
		/* add-object and end-object are joined to edit-object
		 * actuator */
		Object *ob= main->object.first;
		bActuator *act;
		bObjectActuator *oa;
		while (ob) {
			act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_OBJECT) {
					oa= act->data;
					oa->flag &= ~(ACT_TORQUE_LOCAL|ACT_DROT_LOCAL);		/* this actuator didn't do local/glob rot before */
				}
				act= act->next;
			}
			ob= ob->id.next;
		}
	}

	if (main->versionfile <= 204) {
		/* patches for new physics */
		Object *ob= main->object.first;
		bActuator *act;
		bObjectActuator *oa;
		bSound *sound;
		while (ob) {

			/* please check this for demo20 files like
			 * original Egypt levels etc.  converted
			 * rotation factor of 50 is not workable */
			act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_OBJECT) {
					oa= act->data;

					oa->forceloc[0]*= 25.0f;
					oa->forceloc[1]*= 25.0f;
					oa->forceloc[2]*= 25.0f;

					oa->forcerot[0]*= 10.0f;
					oa->forcerot[1]*= 10.0f;
					oa->forcerot[2]*= 10.0f;
				}
				act= act->next;
			}
			ob= ob->id.next;
		}

		sound = main->sound.first;
		while (sound) {
			if (sound->volume < 0.01f) {
				sound->volume = 1.0f;
			}
			sound = sound->id.next;
		}
	}

	if (main->versionfile <= 205) {
		/* patches for new physics */
		Object *ob= main->object.first;
		bActuator *act;
		bSensor *sens;
		bEditObjectActuator *oa;
		bRaySensor *rs;
		bCollisionSensor *cs;
		while (ob) {
			/* Set anisotropic friction off for old objects,
			 * values to 1.0.  */
			ob->gameflag &= ~OB_ANISOTROPIC_FRICTION;
			ob->anisotropicFriction[0] = 1.0;
			ob->anisotropicFriction[1] = 1.0;
			ob->anisotropicFriction[2] = 1.0;

			act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_EDIT_OBJECT) {
					/* Zero initial velocity for newly
					 * added objects */
					oa= act->data;
					oa->linVelocity[0] = 0.0;
					oa->linVelocity[1] = 0.0;
					oa->linVelocity[2] = 0.0;
					oa->localflag = 0;
				}
				act= act->next;
			}

			sens= ob->sensors.first;
			while (sens) {
				/* Extra fields for radar sensors. */
				if (sens->type == SENS_RADAR) {
					bRadarSensor *s = sens->data;
					s->range = 10000.0;
				}

				/* Pulsing: defaults for new sensors. */
				if (sens->type != SENS_ALWAYS) {
					sens->pulse = 0;
					sens->freq = 0;
				}
				else {
					sens->pulse = 1;
				}

				/* Invert: off. */
				sens->invert = 0;

				/* Collision and ray: default = trigger
				 * on property. The material field can
				 * remain empty. */
				if (sens->type == SENS_COLLISION) {
					cs = (bCollisionSensor*) sens->data;
					cs->mode = 0;
				}
				if (sens->type == SENS_RAY) {
					rs = (bRaySensor*) sens->data;
					rs->mode = 0;
				}
				sens = sens->next;
			}
			ob= ob->id.next;
		}
		/* have to check the exact multiplier */
	}

	if (main->versionfile <= 211) {
		/* Render setting: per scene, the applicable gamma value
		 * can be set. Default is 1.0, which means no
		 * correction.  */
		bActuator *act;
		bObjectActuator *oa;
		Object *ob;

		/* added alpha in obcolor */
		ob= main->object.first;
		while (ob) {
			ob->col[3]= 1.0;
			ob= ob->id.next;
		}

		/* added alpha in obcolor */
		ob= main->object.first;
		while (ob) {
			act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_OBJECT) {
					/* multiply velocity with 50 in old files */
					oa= act->data;
					if (fabsf(oa->linearvelocity[0]) >= 0.01f)
						oa->linearvelocity[0] *= 50.0f;
					if (fabsf(oa->linearvelocity[1]) >= 0.01f)
						oa->linearvelocity[1] *= 50.0f;
					if (fabsf(oa->linearvelocity[2]) >= 0.01f)
						oa->linearvelocity[2] *= 50.0f;
					if (fabsf(oa->angularvelocity[0])>=0.01f)
						oa->angularvelocity[0] *= 50.0f;
					if (fabsf(oa->angularvelocity[1])>=0.01f)
						oa->angularvelocity[1] *= 50.0f;
					if (fabsf(oa->angularvelocity[2])>=0.01f)
						oa->angularvelocity[2] *= 50.0f;
				}
				act= act->next;
			}
			ob= ob->id.next;
		}
	}

	if (main->versionfile <= 212) {

		bSound* sound;
		bProperty *prop;
		Object *ob;
		Mesh *me;

		sound = main->sound.first;
		while (sound) {
			sound->max_gain = 1.0;
			sound->min_gain = 0.0;
			sound->distance = 1.0;

			if (sound->attenuation > 0.0f)
				sound->flags |= SOUND_FLAGS_3D;
			else
				sound->flags &= ~SOUND_FLAGS_3D;

			sound = sound->id.next;
		}

		ob = main->object.first;

		while (ob) {
			prop= ob->prop.first;
			while (prop) {
				if (prop->type == GPROP_TIME) {
					// convert old GPROP_TIME values from int to float
					*((float *)&prop->data) = (float) prop->data;
				}

				prop= prop->next;
			}
			ob = ob->id.next;
		}

			/* me->subdiv changed to reflect the actual reparametization
			 * better, and smeshes were removed - if it was a smesh make
			 * it a subsurf, and reset the subdiv level because subsurf
			 * takes a lot more work to calculate.
			 */
		for (me= main->mesh.first; me; me= me->id.next) {
			if (me->flag&ME_SMESH) {
				me->flag&= ~ME_SMESH;
				me->flag|= ME_SUBSURF;

				me->subdiv= 1;
			}
			else {
				if (me->subdiv<2)
					me->subdiv= 1;
				else
					me->subdiv--;
			}
		}
	}

	if (main->versionfile <= 220) {
		Object *ob;
		Mesh *me;

		ob = main->object.first;

		/* adapt form factor in order to get the 'old' physics
		 * behavior back...*/

		while (ob) {
			/* in future, distinguish between different
			 * object bounding shapes */
			ob->formfactor = 0.4f;
			/* patch form factor, note that inertia equiv radius
			 * of a rotation symmetrical obj */
			if (ob->inertia != 1.0f) {
				ob->formfactor /= ob->inertia * ob->inertia;
			}
			ob = ob->id.next;
		}

			/* Began using alpha component of vertex colors, but
			 * old file vertex colors are undefined, reset them
			 * to be fully opaque. -zr
			 */
		for (me= main->mesh.first; me; me= me->id.next) {
			if (me->mcol) {
				int i;

				for (i=0; i<me->totface*4; i++) {
					MCol *mcol= &me->mcol[i];
					mcol->a= 255;
				}
			}
			if (me->tface) {
				int i, j;

				for (i=0; i<me->totface; i++) {
					TFace *tf= &((TFace*) me->tface)[i];

					for (j=0; j<4; j++) {
						char *col= (char*) &tf->col[j];

						col[0]= 255;
					}
				}
			}
		}
	}
	if (main->versionfile <= 221) {
		Scene *sce= main->scene.first;

		// new variables for std-alone player and runtime
		while (sce) {

			sce->r.xplay= 640;
			sce->r.yplay= 480;
			sce->r.freqplay= 60;

			sce= sce->id.next;
		}

	}
	if (main->versionfile <= 222) {
		Scene *sce= main->scene.first;

		// new variables for std-alone player and runtime
		while (sce) {

			sce->r.depth= 32;

			sce= sce->id.next;
		}
	}


	if (main->versionfile <= 223) {
		VFont *vf;
		Image *ima;
		Object *ob;

		for (vf= main->vfont.first; vf; vf= vf->id.next) {
			if (strcmp(vf->name+strlen(vf->name)-6, ".Bfont")==0) {
				strcpy(vf->name, FO_BUILTIN_NAME);
			}
		}

		/* Old textures animate at 25 FPS */
		for (ima = main->image.first; ima; ima=ima->id.next) {
			ima->animspeed = 25;
		}

			/* Zr remapped some keyboard codes to be linear (stupid zr) */
		for (ob= main->object.first; ob; ob= ob->id.next) {
			bSensor *sens;

			for (sens= ob->sensors.first; sens; sens= sens->next) {
				if (sens->type==SENS_KEYBOARD) {
					bKeyboardSensor *ks= sens->data;

					ks->key= map_223_keybd_code_to_224_keybd_code(ks->key);
					ks->qual= map_223_keybd_code_to_224_keybd_code(ks->qual);
					ks->qual2= map_223_keybd_code_to_224_keybd_code(ks->qual2);
				}
			}
		}
	}
	if (main->versionfile <= 224) {
		bSound* sound;
		Scene *sce;
		Mesh *me;
		bScreen *sc;

		for (sound=main->sound.first; sound; sound=sound->id.next) {
			if (sound->packedfile) {
				if (sound->newpackedfile == NULL) {
					sound->newpackedfile = sound->packedfile;
				}
				sound->packedfile = NULL;
			}
		}
		/* Make sure that old subsurf meshes don't have zero subdivision level for rendering */
		for (me=main->mesh.first; me; me=me->id.next) {
			if ((me->flag & ME_SUBSURF) && (me->subdivr==0))
				me->subdivr=me->subdiv;
		}

		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->r.stereomode = 1;  // no stereo
		}

			/* some oldfile patch, moved from set_func_space */
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;

			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_IPO) {
						SpaceSeq *sseq= (SpaceSeq*) sl;
						sseq->v2d.keeptot= 0;
					}
				}
			}
		}

	}


	if (main->versionfile <= 225) {
		World *wo;
		/* Use Sumo for old games */
		for (wo = main->world.first; wo; wo= wo->id.next) {
			wo->physicsEngine = 2;
		}
	}

	if (main->versionfile <= 227) {
		Scene *sce;
		Material *ma;
		bScreen *sc;
		Object *ob;

		/* As of now, this insures that the transition from the old Track system
		 * to the new full constraint Track is painless for everyone. - theeth
		 */
		ob = main->object.first;

		while (ob) {
			ListBase *list;
			list = &ob->constraints;

			/* check for already existing TrackTo constraint
			 * set their track and up flag correctly */

			if (list) {
				bConstraint *curcon;
				for (curcon = list->first; curcon; curcon=curcon->next) {
					if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
						bTrackToConstraint *data = curcon->data;
						data->reserved1 = ob->trackflag;
						data->reserved2 = ob->upflag;
					}
				}
			}

			if (ob->type == OB_ARMATURE) {
				if (ob->pose) {
					bConstraint *curcon;
					bPoseChannel *pchan;
					for (pchan = ob->pose->chanbase.first;
						 pchan; pchan=pchan->next) {
						for (curcon = pchan->constraints.first;
							 curcon; curcon=curcon->next) {
							if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
								bTrackToConstraint *data = curcon->data;
								data->reserved1 = ob->trackflag;
								data->reserved2 = ob->upflag;
							}
						}
					}
				}
			}

			/* Change Ob->Track in real TrackTo constraint */
			blo_do_version_old_trackto_to_constraints(ob);
			
			ob = ob->id.next;
		}


		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->audio.mixrate = 44100;
			sce->audio.flag |= AUDIO_SCRUB;
			sce->r.mode |= R_ENVMAP;
		}
		// init new shader vars
		for (ma= main->mat.first; ma; ma= ma->id.next) {
			ma->refrac= 4.0f;
			ma->roughness= 0.5f;
			ma->param[0]= 0.5f;
			ma->param[1]= 0.1f;
			ma->param[2]= 0.1f;
			ma->param[3]= 0.05f;
		}
		// patch for old wrong max view2d settings, allows zooming out more
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;

			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_ACTION) {
						SpaceAction *sac= (SpaceAction *) sl;
						sac->v2d.max[0]= 32000;
					}
					else if (sl->spacetype==SPACE_NLA) {
						SpaceNla *sla= (SpaceNla *) sl;
						sla->v2d.max[0]= 32000;
					}
				}
			}
		}
	}
	if (main->versionfile <= 228) {
		Scene *sce;
		bScreen *sc;
		Object *ob;


		/* As of now, this insures that the transition from the old Track system
		 * to the new full constraint Track is painless for everyone.*/
		ob = main->object.first;

		while (ob) {
			ListBase *list;
			list = &ob->constraints;

			/* check for already existing TrackTo constraint
			 * set their track and up flag correctly */

			if (list) {
				bConstraint *curcon;
				for (curcon = list->first; curcon; curcon=curcon->next) {
					if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
						bTrackToConstraint *data = curcon->data;
						data->reserved1 = ob->trackflag;
						data->reserved2 = ob->upflag;
					}
				}
			}

			if (ob->type == OB_ARMATURE) {
				if (ob->pose) {
					bConstraint *curcon;
					bPoseChannel *pchan;
					for (pchan = ob->pose->chanbase.first;
						 pchan; pchan=pchan->next) {
						for (curcon = pchan->constraints.first;
							 curcon; curcon=curcon->next) {
							if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
								bTrackToConstraint *data = curcon->data;
								data->reserved1 = ob->trackflag;
								data->reserved2 = ob->upflag;
							}
						}
					}
				}
			}

			ob = ob->id.next;
		}

		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->r.mode |= R_ENVMAP;
		}

		// convert old mainb values for new button panels
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;

			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_BUTS) {
						SpaceButs *sbuts= (SpaceButs *) sl;

						sbuts->v2d.maxzoom= 1.2f;
						sbuts->align= 1;	/* horizontal default */
					
						if (sbuts->mainb==BUTS_LAMP) {
							sbuts->mainb= CONTEXT_SHADING;
							//sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_LAMP;
						}
						else if (sbuts->mainb==BUTS_MAT) {
							sbuts->mainb= CONTEXT_SHADING;
							//sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_MAT;
						}
						else if (sbuts->mainb==BUTS_TEX) {
							sbuts->mainb= CONTEXT_SHADING;
							//sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_TEX;
						}
						else if (sbuts->mainb==BUTS_ANIM) {
							sbuts->mainb= CONTEXT_OBJECT;
						}
						else if (sbuts->mainb==BUTS_WORLD) {
							sbuts->mainb= CONTEXT_SCENE;
							//sbuts->tab[CONTEXT_SCENE]= TAB_SCENE_WORLD;
						}
						else if (sbuts->mainb==BUTS_RENDER) {
							sbuts->mainb= CONTEXT_SCENE;
							//sbuts->tab[CONTEXT_SCENE]= TAB_SCENE_RENDER;
						}
						else if (sbuts->mainb==BUTS_GAME) {
							sbuts->mainb= CONTEXT_LOGIC;
						}
						else if (sbuts->mainb==BUTS_FPAINT) {
							sbuts->mainb= CONTEXT_EDITING;
						}
						else if (sbuts->mainb==BUTS_RADIO) {
							sbuts->mainb= CONTEXT_SHADING;
							//sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_RAD;
						}
						else if (sbuts->mainb==BUTS_CONSTRAINT) {
							sbuts->mainb= CONTEXT_OBJECT;
						}
						else if (sbuts->mainb==BUTS_SCRIPT) {
							sbuts->mainb= CONTEXT_OBJECT;
						}
						else if (sbuts->mainb==BUTS_EDIT) {
							sbuts->mainb= CONTEXT_EDITING;
						}
						else sbuts->mainb= CONTEXT_SCENE;
					}
				}
			}
		}
	}
	/* ton: made this 230 instead of 229,
	 * to be sure (tuho files) and this is a reliable check anyway
	 * nevertheless, we might need to think over a fitness (initialize)
	 * check apart from the do_versions() */

	if (main->versionfile <= 230) {
		bScreen *sc;

		// new variable blockscale, for panels in any area
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;

			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->blockscale==0.0f) sl->blockscale= 0.7f;
					/* added: 5x better zoom in for action */
					if (sl->spacetype==SPACE_ACTION) {
						SpaceAction *sac= (SpaceAction *)sl;
						sac->v2d.maxzoom= 50;
					}
				}
			}
		}
	}
	if (main->versionfile <= 231) {
		/* new bit flags for showing/hiding grid floor and axes */
		bScreen *sc = main->screen.first;
		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;

						if (v3d->gridflag==0) {
							v3d->gridflag |= V3D_SHOW_X;
							v3d->gridflag |= V3D_SHOW_Y;
							v3d->gridflag |= V3D_SHOW_FLOOR;
							v3d->gridflag &= ~V3D_SHOW_Z;
						}
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}
	if (main->versionfile <= 231) {
		Material *ma= main->mat.first;
		bScreen *sc = main->screen.first;
		Scene *sce;
		Lamp *la;
		World *wrld;

		/* introduction of raytrace */
		while (ma) {
			if (ma->fresnel_tra_i==0.0f) ma->fresnel_tra_i= 1.25f;
			if (ma->fresnel_mir_i==0.0f) ma->fresnel_mir_i= 1.25f;

			ma->ang= 1.0;
			ma->ray_depth= 2;
			ma->ray_depth_tra= 2;
			ma->fresnel_tra= 0.0;
			ma->fresnel_mir= 0.0;

			ma= ma->id.next;
		}
		sce= main->scene.first;
		while (sce) {
			if (sce->r.gauss==0.0f) sce->r.gauss= 1.0f;
			sce= sce->id.next;
		}
		la= main->lamp.first;
		while (la) {
			if (la->k==0.0f) la->k= 1.0;
			if (la->ray_samp==0) la->ray_samp= 1;
			if (la->ray_sampy==0) la->ray_sampy= 1;
			if (la->ray_sampz==0) la->ray_sampz= 1;
			if (la->area_size==0.0f) la->area_size= 1.0f;
			if (la->area_sizey==0.0f) la->area_sizey= 1.0f;
			if (la->area_sizez==0.0f) la->area_sizez= 1.0f;
			la= la->id.next;
		}
		wrld= main->world.first;
		while (wrld) {
			if (wrld->range==0.0f) {
				wrld->range= 1.0f/wrld->exposure;
			}
			wrld= wrld->id.next;
		}

		/* new bit flags for showing/hiding grid floor and axes */

		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;

						if (v3d->gridflag==0) {
							v3d->gridflag |= V3D_SHOW_X;
							v3d->gridflag |= V3D_SHOW_Y;
							v3d->gridflag |= V3D_SHOW_FLOOR;
							v3d->gridflag &= ~V3D_SHOW_Z;
						}
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}
	if (main->versionfile <= 232) {
		Tex *tex= main->tex.first;
		World *wrld= main->world.first;
		bScreen *sc;
		Scene *sce;

		while (tex) {
			if ((tex->flag & (TEX_CHECKER_ODD+TEX_CHECKER_EVEN))==0) {
				tex->flag |= TEX_CHECKER_ODD;
			}
			/* copied from kernel texture.c */
			if (tex->ns_outscale==0.0f) {
				/* musgrave */
				tex->mg_H = 1.0f;
				tex->mg_lacunarity = 2.0f;
				tex->mg_octaves = 2.0f;
				tex->mg_offset = 1.0f;
				tex->mg_gain = 1.0f;
				tex->ns_outscale = 1.0f;
				/* distnoise */
				tex->dist_amount = 1.0f;
				/* voronoi */
				tex->vn_w1 = 1.0f;
				tex->vn_mexp = 2.5f;
			}
			tex= tex->id.next;
		}

		while (wrld) {
			if (wrld->aodist==0.0f) {
				wrld->aodist= 10.0f;
				wrld->aobias= 0.05f;
			}
			if (wrld->aosamp==0) wrld->aosamp= 5;
			if (wrld->aoenergy==0.0f) wrld->aoenergy= 1.0f;
			wrld= wrld->id.next;
		}


		// new variable blockscale, for panels in any area, do again because new
		// areas didnt initialize it to 0.7 yet
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->blockscale==0.0f) sl->blockscale= 0.7f;

					/* added: 5x better zoom in for nla */
					if (sl->spacetype==SPACE_NLA) {
						SpaceNla *snla= (SpaceNla *)sl;
						snla->v2d.maxzoom= 50;
					}
				}
			}
		}
		sce= main->scene.first;
		while (sce) {
			if (sce->r.ocres==0) sce->r.ocres= 64;
			sce= sce->id.next;
		}

	}
	if (main->versionfile <= 233) {
		bScreen *sc;
		Material *ma= main->mat.first;
		/* Object *ob= main->object.first; */
		
		while (ma) {
			if (ma->rampfac_col==0.0f) ma->rampfac_col= 1.0;
			if (ma->rampfac_spec==0.0f) ma->rampfac_spec= 1.0;
			if (ma->pr_lamp==0) ma->pr_lamp= 3;
			ma= ma->id.next;
		}
		
		/* this should have been done loooong before! */
#if 0   /* deprecated in 2.5+ */
		while (ob) {
			if (ob->ipowin==0) ob->ipowin= ID_OB;
			ob= ob->id.next;
		}
#endif
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D *)sl;
						v3d->flag |= V3D_SELECT_OUTLINE;
					}
				}
			}
		}
	}


	

	if (main->versionfile <= 234) {
		World *wo;
		bScreen *sc;
		
		// force sumo engine to be active
		for (wo = main->world.first; wo; wo= wo->id.next) {
			if (wo->physicsEngine==0) wo->physicsEngine = 2;
		}
		
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D *)sl;
						v3d->flag |= V3D_ZBUF_SELECT;
					}
					else if (sl->spacetype==SPACE_TEXT) {
						SpaceText *st= (SpaceText *)sl;
						if (st->tabnumber==0) st->tabnumber= 2;
					}
				}
			}
		}
	}
	if (main->versionfile <= 235) {
		Tex *tex= main->tex.first;
		Scene *sce= main->scene.first;
		Sequence *seq;
		Editing *ed;
		
		while (tex) {
			if (tex->nabla==0.0f) tex->nabla= 0.025f;
			tex= tex->id.next;
		}
		while (sce) {
			ed= sce->ed;
			if (ed) {
				SEQ_BEGIN (sce->ed, seq)
				{
					if (seq->type==SEQ_IMAGE || seq->type==SEQ_MOVIE)
						seq->flag |= SEQ_MAKE_PREMUL;
				}
				SEQ_END
			}
			
			sce= sce->id.next;
		}
	}
	if (main->versionfile <= 236) {
		Object *ob;
		Camera *cam= main->camera.first;
		Material *ma;
		bScreen *sc;

		while (cam) {
			if (cam->ortho_scale==0.0f) {
				cam->ortho_scale= 256.0f/cam->lens;
				if (cam->type==CAM_ORTHO) printf("NOTE: ortho render has changed, tweak new Camera 'scale' value.\n");
			}
			cam= cam->id.next;
		}
		/* set manipulator type */
		/* force oops draw if depgraph was set*/
		/* set time line var */
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D *)sl;
						if (v3d->twtype==0) v3d->twtype= V3D_MANIP_TRANSLATE;
					}
				}
			}
		}
		// init new shader vars
		for (ma= main->mat.first; ma; ma= ma->id.next) {
			if (ma->darkness==0.0f) {
				ma->rms=0.1f;
				ma->darkness=1.0f;
			}
		}
		
		/* softbody init new vars */
		for (ob= main->object.first; ob; ob= ob->id.next) {
			if (ob->soft) {
				if (ob->soft->defgoal==0.0f) ob->soft->defgoal= 0.7f;
				if (ob->soft->physics_speed==0.0f) ob->soft->physics_speed= 1.0f;
				
				if (ob->soft->interval==0) {
					ob->soft->interval= 2;
					ob->soft->sfra= 1;
					ob->soft->efra= 100;
				}
			}
			if (ob->soft && ob->soft->vertgroup==0) {
				bDeformGroup *locGroup = defgroup_find_name(ob, "SOFTGOAL");
				if (locGroup) {
					/* retrieve index for that group */
					ob->soft->vertgroup =  1 + BLI_findindex(&ob->defbase, locGroup);
				}
			}
		}
	}
	if (main->versionfile <= 237) {
		bArmature *arm;
		bConstraint *con;
		Object *ob;
		Bone *bone;
		
		// armature recode checks 
		for (arm= main->armature.first; arm; arm= arm->id.next) {
			BKE_armature_where_is(arm);

			for (bone= arm->bonebase.first; bone; bone= bone->next)
				do_version_bone_head_tail_237(bone);
		}
		for (ob= main->object.first; ob; ob= ob->id.next) {
			if (ob->parent) {
				Object *parent= blo_do_versions_newlibadr(fd, lib, ob->parent);
				if (parent && parent->type==OB_LATTICE)
					ob->partype = PARSKEL;
			}

			// btw. BKE_pose_rebuild is further only called on leave editmode
			if (ob->type==OB_ARMATURE) {
				if (ob->pose)
					ob->pose->flag |= POSE_RECALC;
				ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;	// cannot call stuff now (pointers!), done in setup_app_data

				/* new generic xray option */
				arm= blo_do_versions_newlibadr(fd, lib, ob->data);
				if (arm->flag & ARM_DRAWXRAY) {
					ob->dtx |= OB_DRAWXRAY;
				}
			}
			else if (ob->type==OB_MESH) {
				Mesh *me = blo_do_versions_newlibadr(fd, lib, ob->data);
				
				if ((me->flag&ME_SUBSURF)) {
					SubsurfModifierData *smd = (SubsurfModifierData*) modifier_new(eModifierType_Subsurf);
					
					smd->levels = MAX2(1, me->subdiv);
					smd->renderLevels = MAX2(1, me->subdivr);
					smd->subdivType = me->subsurftype;
					
					smd->modifier.mode = 0;
					if (me->subdiv!=0)
						smd->modifier.mode |= 1;
					if (me->subdivr!=0)
						smd->modifier.mode |= 2;
					if (me->flag&ME_OPT_EDGES)
						smd->flags |= eSubsurfModifierFlag_ControlEdges;
					
					BLI_addtail(&ob->modifiers, smd);
					
					modifier_unique_name(&ob->modifiers, (ModifierData*)smd);
				}
			}
			
			// follow path constraint needs to set the 'path' option in curves...
			for (con=ob->constraints.first; con; con= con->next) {
				if (con->type==CONSTRAINT_TYPE_FOLLOWPATH) {
					bFollowPathConstraint *data = con->data;
					Object *obc= blo_do_versions_newlibadr(fd, lib, data->tar);
					
					if (obc && obc->type==OB_CURVE) {
						Curve *cu= blo_do_versions_newlibadr(fd, lib, obc->data);
						if (cu) cu->flag |= CU_PATH;
					}
				}
			}
		}
	}
	if (main->versionfile <= 238) {
		Lattice *lt;
		Object *ob;
		bArmature *arm;
		Mesh *me;
		Key *key;
		Scene *sce= main->scene.first;

		while (sce) {
			if (sce->toolsettings == NULL) {
				sce->toolsettings = MEM_callocN(sizeof(struct ToolSettings), "Tool Settings Struct");
				sce->toolsettings->cornertype=0;
				sce->toolsettings->degr = 90; 
				sce->toolsettings->step = 9;
				sce->toolsettings->turn = 1; 				
				sce->toolsettings->extr_offs = 1; 
				sce->toolsettings->doublimit = 0.001f;
				sce->toolsettings->segments = 32;
				sce->toolsettings->rings = 32;
				sce->toolsettings->vertices = 32;
			}
			sce= sce->id.next;	
		}

		for (lt=main->latt.first; lt; lt=lt->id.next) {
			if (lt->fu==0.0f && lt->fv==0.0f && lt->fw==0.0f) {
				calc_lat_fudu(lt->flag, lt->pntsu, &lt->fu, &lt->du);
				calc_lat_fudu(lt->flag, lt->pntsv, &lt->fv, &lt->dv);
				calc_lat_fudu(lt->flag, lt->pntsw, &lt->fw, &lt->dw);
			}
		}

		for (ob=main->object.first; ob; ob= ob->id.next) {
			ModifierData *md;
			PartEff *paf;

			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Subsurf) {
					SubsurfModifierData *smd = (SubsurfModifierData*) md;

					smd->flags &= ~(eSubsurfModifierFlag_Incremental|eSubsurfModifierFlag_DebugIncr);
				}
			}

			if ((ob->softflag&OB_SB_ENABLE) && !modifiers_findByType(ob, eModifierType_Softbody)) {
				if (ob->softflag&OB_SB_POSTDEF) {
					md = ob->modifiers.first;

					while (md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform) {
						md = md->next;
					}

					BLI_insertlinkbefore(&ob->modifiers, md, modifier_new(eModifierType_Softbody));
				}
				else {
					BLI_addhead(&ob->modifiers, modifier_new(eModifierType_Softbody));
				}

				ob->softflag &= ~OB_SB_ENABLE;
			}
			if (ob->pose) {
				bPoseChannel *pchan;
				bConstraint *con;
				for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
					// note, pchan->bone is also lib-link stuff
					if (pchan->limitmin[0] == 0.0f && pchan->limitmax[0] == 0.0f) {
						pchan->limitmin[0]= pchan->limitmin[1]= pchan->limitmin[2]= -180.0f;
						pchan->limitmax[0]= pchan->limitmax[1]= pchan->limitmax[2]= 180.0f;
						
						for (con= pchan->constraints.first; con; con= con->next) {
							if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
								bKinematicConstraint *data = (bKinematicConstraint*)con->data;
								data->weight = 1.0f;
								data->orientweight = 1.0f;
								data->flag &= ~CONSTRAINT_IK_ROT;
								
								/* enforce conversion from old IK_TOPARENT to rootbone index */
								data->rootbone= -1;
								
								/* update_pose_etc handles rootbone==-1 */
								ob->pose->flag |= POSE_RECALC;
							}	
						}
					}
				}
			}

			paf = blo_do_version_give_parteff_245(ob);
			if (paf) {
				if (paf->disp == 0)
					paf->disp = 100;
				if (paf->speedtex == 0)
					paf->speedtex = 8;
				if (paf->omat == 0)
					paf->omat = 1;
			}
		}
		
		for (arm=main->armature.first; arm; arm= arm->id.next) {
			bone_version_238(&arm->bonebase);
			arm->deformflag |= ARM_DEF_VGROUP;
		}

		for (me=main->mesh.first; me; me= me->id.next) {
			if (!me->medge) {
				make_edges(me, 1);	/* 1 = use mface->edcode */
			}
			else {
				mesh_strip_loose_faces(me);
			}
		}
		
		for (key= main->key.first; key; key= key->id.next) {
			KeyBlock *kb;
			int index = 1;

			for (kb= key->block.first; kb; kb= kb->next) {
				if (kb==key->refkey) {
					if (kb->name[0]==0)
						strcpy(kb->name, "Basis");
				}
				else {
					if (kb->name[0]==0) {
						BLI_snprintf(kb->name, sizeof(kb->name), "Key %d", index);
					}
					index++;
				}
			}
		}
	}
	if (main->versionfile <= 239) {
		bArmature *arm;
		Object *ob;
		Scene *sce= main->scene.first;
		Camera *cam= main->camera.first;
		Material *ma= main->mat.first;
		int set_passepartout= 0;
		
		/* deformflag is local in modifier now */
		for (ob=main->object.first; ob; ob= ob->id.next) {
			ModifierData *md;
			
			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Armature) {
					ArmatureModifierData *amd = (ArmatureModifierData*) md;
					if (amd->object && amd->deformflag==0) {
						Object *oba= blo_do_versions_newlibadr(fd, lib, amd->object);
						arm= blo_do_versions_newlibadr(fd, lib, oba->data);
						amd->deformflag= arm->deformflag;
					}
				}
			}
		}
		
		/* updating stepsize for ghost drawing */
		for (arm= main->armature.first; arm; arm= arm->id.next) {
			if (arm->ghostsize==0) arm->ghostsize=1;
			bone_version_239(&arm->bonebase);
			if (arm->layer==0) arm->layer= 1;
		}
		
		for (;sce;sce= sce->id.next) {
			/* make 'innervert' the default subdivide type, for backwards compat */
			sce->toolsettings->cornertype=1;
		
			if (sce->r.scemode & R_PASSEPARTOUT) {
				set_passepartout= 1;
				sce->r.scemode &= ~R_PASSEPARTOUT;
			}
			/* gauss is filter variable now */
			if (sce->r.mode & R_GAUSS) {
				sce->r.filtertype= R_FILTER_GAUSS;
				sce->r.mode &= ~R_GAUSS;
			}
		}
		
		for (;cam; cam= cam->id.next) {
			if (set_passepartout)
				cam->flag |= CAM_SHOWPASSEPARTOUT;
			
			/* make sure old cameras have title safe on */
			if (!(cam->flag & CAM_SHOWTITLESAFE))
				cam->flag |= CAM_SHOWTITLESAFE;
			
			/* set an appropriate camera passepartout alpha */
			if (!(cam->passepartalpha)) cam->passepartalpha = 0.2f;
		}
		
		for (; ma; ma= ma->id.next) {
			if (ma->strand_sta==0.0f) {
				ma->strand_sta= ma->strand_end= 1.0f;
				ma->mode |= MA_TANGENT_STR;
			}
			if (ma->mode & MA_TRACEBLE) ma->mode |= MA_SHADBUF;
		}
	}
	
	if (main->versionfile <= 241) {
		Object *ob;
		Tex *tex;
		Scene *sce;
		World *wo;
		Lamp *la;
		Material *ma;
		bArmature *arm;
		bNodeTree *ntree;
		
		for (wo = main->world.first; wo; wo= wo->id.next) {
			/* Migrate to Bullet for games, except for the NaN versions */
			/* People can still explicitly choose for Sumo (after 2.42 is out) */
			if (main->versionfile > 225)
				wo->physicsEngine = WOPHY_BULLET;
			if (WO_AODIST == wo->aomode)
				wo->aocolor= WO_AOPLAIN;
		}
		
		/* updating layers still */
		for (arm= main->armature.first; arm; arm= arm->id.next) {
			bone_version_239(&arm->bonebase);
			if (arm->layer==0) arm->layer= 1;
		}
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->audio.mixrate==0) sce->audio.mixrate= 44100;

			if (sce->r.xparts<2) sce->r.xparts= 4;
			if (sce->r.yparts<2) sce->r.yparts= 4;
			/* adds default layer */
			if (sce->r.layers.first==NULL)
				BKE_scene_add_render_layer(sce, NULL);
			else {
				SceneRenderLayer *srl;
				/* new layer flag for sky, was default for solid */
				for (srl= sce->r.layers.first; srl; srl= srl->next) {
					if (srl->layflag & SCE_LAY_SOLID)
						srl->layflag |= SCE_LAY_SKY;
					srl->passflag &= (SCE_PASS_COMBINED|SCE_PASS_Z|SCE_PASS_NORMAL|SCE_PASS_VECTOR);
				}
			}
			
			/* node version changes */
			if (sce->nodetree)
				ntree_version_241(sce->nodetree);

			/* uv calculation options moved to toolsettings */
			if (sce->toolsettings->uvcalc_radius == 0.0f) {
				sce->toolsettings->uvcalc_radius = 1.0f;
				sce->toolsettings->uvcalc_cubesize = 1.0f;
				sce->toolsettings->uvcalc_mapdir = 1;
				sce->toolsettings->uvcalc_mapalign = 1;
				sce->toolsettings->uvcalc_flag = UVCALC_FILLHOLES;
				sce->toolsettings->unwrapper = 1;
			}

			if (sce->r.mode & R_PANORAMA) {
				/* all these checks to ensure saved files with svn version keep working... */
				if (sce->r.xsch < sce->r.ysch) {
					Object *obc= blo_do_versions_newlibadr(fd, lib, sce->camera);
					if (obc && obc->type==OB_CAMERA) {
						Camera *cam= blo_do_versions_newlibadr(fd, lib, obc->data);
						if (cam->lens>=10.0f) {
							sce->r.xsch*= sce->r.xparts;
							cam->lens*= (float)sce->r.ysch/(float)sce->r.xsch;
						}
					}
				}
			}
		}
		
		for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next)
			ntree_version_241(ntree);
		
		for (la= main->lamp.first; la; la= la->id.next)
			if (la->buffers==0)
				la->buffers= 1;
		
		for (tex= main->tex.first; tex; tex= tex->id.next) {
			if (tex->env && tex->env->viewscale==0.0f)
				tex->env->viewscale= 1.0f;
//			tex->imaflag |= TEX_GAUSS_MIP;
		}
		
		/* for empty drawsize and drawtype */
		for (ob=main->object.first; ob; ob= ob->id.next) {
			if (ob->empty_drawsize==0.0f) {
				ob->empty_drawtype = OB_ARROWS;
				ob->empty_drawsize = 1.0;
			}
		}
		
		for (ma= main->mat.first; ma; ma= ma->id.next) {
			/* stucci returns intensity from now on */
			int a;
			for (a=0; a<MAX_MTEX; a++) {
				if (ma->mtex[a] && ma->mtex[a]->tex) {
					tex= blo_do_versions_newlibadr(fd, lib, ma->mtex[a]->tex);
					if (tex && tex->type==TEX_STUCCI)
						ma->mtex[a]->mapto &= ~(MAP_COL|MAP_SPEC|MAP_REF);
				}
			}
			/* transmissivity defaults */
			if (ma->tx_falloff==0.0f) ma->tx_falloff= 1.0f;
		}
		
		/* during 2.41 images with this name were used for viewer node output, lets fix that */
		if (main->versionfile == 241) {
			Image *ima;
			for (ima= main->image.first; ima; ima= ima->id.next)
				if (strcmp(ima->name, "Compositor")==0) {
					strcpy(ima->id.name+2, "Viewer Node");
					strcpy(ima->name, "Viewer Node");
				}
		}
	}
		
	if (main->versionfile <= 242) {
		Scene *sce;
		bScreen *sc;
		Object *ob;
		Curve *cu;
		Material *ma;
		Mesh *me;
		Group *group;
		Nurb *nu;
		BezTriple *bezt;
		BPoint *bp;
		bNodeTree *ntree;
		int a;
		
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;
						if (v3d->gridsubdiv == 0)
							v3d->gridsubdiv = 10;
					}
				}
				sa = sa->next;
			}
		}
		
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->toolsettings->select_thresh == 0.0f)
				sce->toolsettings->select_thresh= 0.01f;
			if (sce->toolsettings->clean_thresh == 0.0f) 
				sce->toolsettings->clean_thresh = 0.1f;
				
			if (sce->r.threads==0) {
				if (sce->r.mode & R_THREADS)
					sce->r.threads= 2;
				else
					sce->r.threads= 1;
			}
			if (sce->nodetree)
				ntree_version_242(sce->nodetree);
		}
		
		for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next)
			ntree_version_242(ntree);
		
		/* add default radius values to old curve points */
		for (cu= main->curve.first; cu; cu= cu->id.next) {
			for (nu= cu->nurb.first; nu; nu= nu->next) {
				if (nu) {
					if (nu->bezt) {
						for (bezt=nu->bezt, a=0; a<nu->pntsu; a++, bezt++) {
							if (!bezt->radius) bezt->radius= 1.0;
						}
					}
					else if (nu->bp) {
						for (bp=nu->bp, a=0; a<nu->pntsu*nu->pntsv; a++, bp++) {
							if (!bp->radius) bp->radius= 1.0;
						}
					}
				}
			}
		}
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			ModifierData *md;
			ListBase *list;
			list = &ob->constraints;

			/* check for already existing MinMax (floor) constraint
			 * and update the sticky flagging */

			if (list) {
				bConstraint *curcon;
				for (curcon = list->first; curcon; curcon=curcon->next) {
					switch (curcon->type) {
						case CONSTRAINT_TYPE_MINMAX:
						{
							bMinMaxConstraint *data = curcon->data;
							if (data->sticky==1) 
								data->flag |= MINMAX_STICKY;
							else 
								data->flag &= ~MINMAX_STICKY;
						}
							break;
						case CONSTRAINT_TYPE_ROTLIKE:
						{
							bRotateLikeConstraint *data = curcon->data;
							
							/* version patch from buttons_object.c */
							if (data->flag==0)
								data->flag = ROTLIKE_X|ROTLIKE_Y|ROTLIKE_Z;
						}
							break;
					}
				}
			}

			if (ob->type == OB_ARMATURE) {
				if (ob->pose) {
					bConstraint *curcon;
					bPoseChannel *pchan;
					for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
						for (curcon = pchan->constraints.first; curcon; curcon=curcon->next) {
							switch (curcon->type) {
								case CONSTRAINT_TYPE_MINMAX:
								{
									bMinMaxConstraint *data = curcon->data;
									if (data->sticky==1) 
										data->flag |= MINMAX_STICKY;
									else 
										data->flag &= ~MINMAX_STICKY;
								}
									break;
								case CONSTRAINT_TYPE_KINEMATIC:
								{
									bKinematicConstraint *data = curcon->data;
									if (!(data->flag & CONSTRAINT_IK_POS)) {
										data->flag |= CONSTRAINT_IK_POS;
										data->flag |= CONSTRAINT_IK_STRETCH;
									}
								}
									break;
								case CONSTRAINT_TYPE_ROTLIKE:
								{
									bRotateLikeConstraint *data = curcon->data;
									
									/* version patch from buttons_object.c */
									if (data->flag==0)
										data->flag = ROTLIKE_X|ROTLIKE_Y|ROTLIKE_Z;
								}
									break;
							}
						}
					}
				}
			}
			
			/* copy old object level track settings to curve modifers */
			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Curve) {
					CurveModifierData *cmd = (CurveModifierData*) md;

					if (cmd->defaxis == 0) cmd->defaxis = ob->trackflag+1;
				}
			}
			
		}
		
		for (ma = main->mat.first; ma; ma= ma->id.next) {
			if (ma->shad_alpha==0.0f)
				ma->shad_alpha= 1.0f;
			if (ma->nodetree)
				ntree_version_242(ma->nodetree);
		}

		for (me=main->mesh.first; me; me=me->id.next)
			customdata_version_242(me);
		
		for (group= main->group.first; group; group= group->id.next)
			if (group->layer==0)
				group->layer= (1<<20)-1;

		/* now, subversion control! */
		if (main->subversionfile < 3) {
			Image *ima;
			Tex *tex;
			
			/* Image refactor initialize */
			for (ima= main->image.first; ima; ima= ima->id.next) {
				ima->source= IMA_SRC_FILE;
				ima->type= IMA_TYPE_IMAGE;
				
				ima->gen_x= 256; ima->gen_y= 256;
				ima->gen_type= 1;
				
				if (0==strncmp(ima->id.name+2, "Viewer Node", sizeof(ima->id.name)-2)) {
					ima->source= IMA_SRC_VIEWER;
					ima->type= IMA_TYPE_COMPOSITE;
				}
				if (0==strncmp(ima->id.name+2, "Render Result", sizeof(ima->id.name)-2)) {
					ima->source= IMA_SRC_VIEWER;
					ima->type= IMA_TYPE_R_RESULT;
				}
				
			}
			for (tex= main->tex.first; tex; tex= tex->id.next) {
				if (tex->type==TEX_IMAGE && tex->ima) {
					ima= blo_do_versions_newlibadr(fd, lib, tex->ima);
					if (tex->imaflag & TEX_ANIM5_)
						ima->source= IMA_SRC_MOVIE;
					if (tex->imaflag & TEX_FIELDS_)
						ima->flag |= IMA_FIELDS;
					if (tex->imaflag & TEX_STD_FIELD_)
						ima->flag |= IMA_STD_FIELD;
				}
				tex->iuser.frames= tex->frames;
				tex->iuser.fie_ima= (char)tex->fie_ima;
				tex->iuser.offset= tex->offset;
				tex->iuser.sfra= tex->sfra;
				tex->iuser.cycl= (tex->imaflag & TEX_ANIMCYCLIC_)!=0;
			}
			for (sce= main->scene.first; sce; sce= sce->id.next) {
				if (sce->nodetree)
					do_version_ntree_242_2(sce->nodetree);
			}
			for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next)
				do_version_ntree_242_2(ntree);
			for (ma = main->mat.first; ma; ma= ma->id.next)
				if (ma->nodetree)
					do_version_ntree_242_2(ma->nodetree);
			
			for (sc= main->screen.first; sc; sc= sc->id.next) {
				ScrArea *sa;
				for (sa= sc->areabase.first; sa; sa= sa->next) {
					SpaceLink *sl;
					for (sl= sa->spacedata.first; sl; sl= sl->next) {
						if (sl->spacetype==SPACE_IMAGE)
							((SpaceImage *)sl)->iuser.fie_ima= 2;
						else if (sl->spacetype==SPACE_VIEW3D) {
							View3D *v3d= (View3D *)sl;
							BGpic *bgpic;
							for (bgpic= v3d->bgpicbase.first; bgpic; bgpic= bgpic->next)
								bgpic->iuser.fie_ima= 2;
						}
					}
				}
			}
		}
		
		if (main->subversionfile < 4) {
			for (sce= main->scene.first; sce; sce= sce->id.next) {
				sce->r.bake_mode= 1;	/* prevent to include render stuff here */
				sce->r.bake_filter= 2;
				sce->r.bake_osa= 5;
				sce->r.bake_flag= R_BAKE_CLEAR;
			}
		}

		if (main->subversionfile < 5) {
			for (sce= main->scene.first; sce; sce= sce->id.next) {
				/* improved triangle to quad conversion settings */
				if (sce->toolsettings->jointrilimit==0.0f)
					sce->toolsettings->jointrilimit= 0.8f;
			}
		}
	}
	if (main->versionfile <= 243) {
		Object *ob= main->object.first;
		Material *ma;

		for (ma=main->mat.first; ma; ma= ma->id.next) {
			if (ma->sss_scale==0.0f) {
				ma->sss_radius[0]= 1.0f;
				ma->sss_radius[1]= 1.0f;
				ma->sss_radius[2]= 1.0f;
				ma->sss_col[0]= 0.8f;
				ma->sss_col[1]= 0.8f;
				ma->sss_col[2]= 0.8f;
				ma->sss_error= 0.05f;
				ma->sss_scale= 0.1f;
				ma->sss_ior= 1.3f;
				ma->sss_colfac= 1.0f;
				ma->sss_texfac= 0.0f;
			}
			if (ma->sss_front==0 && ma->sss_back==0) {
				ma->sss_front= 1.0f;
				ma->sss_back= 1.0f;
			}
			if (ma->sss_col[0]==0 && ma->sss_col[1]==0 && ma->sss_col[2]==0) {
				ma->sss_col[0]= ma->r;
				ma->sss_col[1]= ma->g;
				ma->sss_col[2]= ma->b;
			}
		}
		
		for (; ob; ob= ob->id.next) {
			bDeformGroup *curdef;
			
			for (curdef= ob->defbase.first; curdef; curdef=curdef->next) {
				/* replace an empty-string name with unique name */
				if (curdef->name[0] == '\0') {
					defgroup_unique_name(curdef, ob);
				}
			}

			if (main->versionfile < 243 || main->subversionfile < 1) {
				ModifierData *md;

				/* translate old mirror modifier axis values to new flags */
				for (md=ob->modifiers.first; md; md=md->next) {
					if (md->type==eModifierType_Mirror) {
						MirrorModifierData *mmd = (MirrorModifierData*) md;

						switch (mmd->axis) {
							case 0:
								mmd->flag |= MOD_MIR_AXIS_X;
								break;
							case 1:
								mmd->flag |= MOD_MIR_AXIS_Y;
								break;
							case 2:
								mmd->flag |= MOD_MIR_AXIS_Z;
								break;
						}

						mmd->axis = 0;
					}
				}
			}
		}
		
		/* render layer added, this is not the active layer */
		if (main->versionfile <= 243 || main->subversionfile < 2) {
			Mesh *me;
			for (me=main->mesh.first; me; me=me->id.next)
				customdata_version_243(me);
		}		

	}
	
	if (main->versionfile <= 244) {
		Scene *sce;
		bScreen *sc;
		Lamp *la;
		World *wrld;
		
		if (main->versionfile != 244 || main->subversionfile < 2) {
			for (sce= main->scene.first; sce; sce= sce->id.next)
				sce->r.mode |= R_SSS;

			/* correct older action editors - incorrect scrolling */
			for (sc= main->screen.first; sc; sc= sc->id.next) {
				ScrArea *sa;
				sa= sc->areabase.first;
				while (sa) {
					SpaceLink *sl;

					for (sl= sa->spacedata.first; sl; sl= sl->next) {
						if (sl->spacetype==SPACE_ACTION) {
							SpaceAction *saction= (SpaceAction*) sl;
							
							saction->v2d.tot.ymin = -1000.0;
							saction->v2d.tot.ymax = 0.0;
							
							saction->v2d.cur.ymin = -75.0;
							saction->v2d.cur.ymax = 5.0;
						}
					}
					sa = sa->next;
				}
			}
		}
		if (main->versionfile != 244 || main->subversionfile < 3) {	
			/* constraints recode version patch used to be here. Moved to 245 now... */
			
			
			for (wrld=main->world.first; wrld; wrld= wrld->id.next) {
				if (wrld->mode & WO_AMB_OCC)
					wrld->ao_samp_method = WO_AOSAMP_CONSTANT;
				else
					wrld->ao_samp_method = WO_AOSAMP_HAMMERSLEY;
				
				wrld->ao_adapt_thresh = 0.005f;
			}
			
			for (la=main->lamp.first; la; la= la->id.next) {
				if (la->type == LA_AREA)
					la->ray_samp_method = LA_SAMP_CONSTANT;
				else
					la->ray_samp_method = LA_SAMP_HALTON;
				
				la->adapt_thresh = 0.001f;
			}
		}
	}
	if (main->versionfile <= 245) {
		Scene *sce;
		Object *ob;
		Image *ima;
		Lamp *la;
		Material *ma;
		ParticleSettings *part;
		World *wrld;
		Mesh *me;
		bNodeTree *ntree;
		Tex *tex;
		ModifierData *md;
		ParticleSystem *psys;
		
		/* unless the file was created 2.44.3 but not 2.45, update the constraints */
		if ( !(main->versionfile==244 && main->subversionfile==3) &&
			 ((main->versionfile<245) || (main->versionfile==245 && main->subversionfile==0)) ) 
		{
			for (ob = main->object.first; ob; ob= ob->id.next) {
				ListBase *list;
				list = &ob->constraints;
				
				/* fix up constraints due to constraint recode changes (originally at 2.44.3) */
				if (list) {
					bConstraint *curcon;
					for (curcon = list->first; curcon; curcon=curcon->next) {
						/* old CONSTRAINT_LOCAL check -> convert to CONSTRAINT_SPACE_LOCAL */
						if (curcon->flag & 0x20) {
							curcon->ownspace = CONSTRAINT_SPACE_LOCAL;
							curcon->tarspace = CONSTRAINT_SPACE_LOCAL;
						}
						
						switch (curcon->type) {
							case CONSTRAINT_TYPE_LOCLIMIT:
							{
								bLocLimitConstraint *data= (bLocLimitConstraint *)curcon->data;
								
								/* old limit without parent option for objects */
								if (data->flag2)
									curcon->ownspace = CONSTRAINT_SPACE_LOCAL;
							}
								break;
						}	
					}
				}
				
				/* correctly initialize constinv matrix */
				unit_m4(ob->constinv);
				
				if (ob->type == OB_ARMATURE) {
					if (ob->pose) {
						bConstraint *curcon;
						bPoseChannel *pchan;
						
						for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
							/* make sure constraints are all up to date */
							for (curcon = pchan->constraints.first; curcon; curcon=curcon->next) {
								/* old CONSTRAINT_LOCAL check -> convert to CONSTRAINT_SPACE_LOCAL */
								if (curcon->flag & 0x20) {
									curcon->ownspace = CONSTRAINT_SPACE_LOCAL;
									curcon->tarspace = CONSTRAINT_SPACE_LOCAL;
								}
								
								switch (curcon->type) {
									case CONSTRAINT_TYPE_ACTION:
									{
										bActionConstraint *data= (bActionConstraint *)curcon->data;
										
										/* 'data->local' used to mean that target was in local-space */
										if (data->local)
											curcon->tarspace = CONSTRAINT_SPACE_LOCAL;
									}							
										break;
								}
							}
							
							/* correctly initialize constinv matrix */
							unit_m4(pchan->constinv);
						}
					}
				}
			}
		}
		
		/* fix all versions before 2.45 */
		if (main->versionfile != 245) {

			/* repair preview from 242 - 244*/
			for (ima= main->image.first; ima; ima= ima->id.next) {
				ima->preview = NULL;
			}
		}

		/* add point caches */
		for (ob=main->object.first; ob; ob=ob->id.next) {
			if (ob->soft && !ob->soft->pointcache)
				ob->soft->pointcache= BKE_ptcache_add(&ob->soft->ptcaches);

			for (psys=ob->particlesystem.first; psys; psys=psys->next) {
				if (psys->pointcache) {
					if (psys->pointcache->flag & PTCACHE_BAKED && (psys->pointcache->flag & PTCACHE_DISK_CACHE)==0) {
						printf("Old memory cache isn't supported for particles, so re-bake the simulation!\n");
						psys->pointcache->flag &= ~PTCACHE_BAKED;
					}
				}
				else
					psys->pointcache= BKE_ptcache_add(&psys->ptcaches);
			}

			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Cloth) {
					ClothModifierData *clmd = (ClothModifierData*) md;
					if (!clmd->point_cache)
						clmd->point_cache= BKE_ptcache_add(&clmd->ptcaches);
				}
			}
		}

		/* Copy over old per-level multires vertex data
		 * into a single vertex array in struct Multires */
		for (me = main->mesh.first; me; me=me->id.next) {
			if (me->mr && !me->mr->verts) {
				MultiresLevel *lvl = me->mr->levels.last;
				if (lvl) {
					me->mr->verts = lvl->verts;
					lvl->verts = NULL;
					/* Don't need the other vert arrays */
					for (lvl = lvl->prev; lvl; lvl = lvl->prev) {
						MEM_freeN(lvl->verts);
						lvl->verts = NULL;
					}
				}
			}
		}
		
		if (main->versionfile != 245 || main->subversionfile < 1) {
			for (la=main->lamp.first; la; la= la->id.next) {
				if (la->mode & LA_QUAD) la->falloff_type = LA_FALLOFF_SLIDERS;
				else la->falloff_type = LA_FALLOFF_INVLINEAR;
				
				if (la->curfalloff == NULL) {
					la->curfalloff = curvemapping_add(1, 0.0f, 1.0f, 1.0f, 0.0f);
					curvemapping_initialize(la->curfalloff);
				}
			}
		}		
		
		for (ma=main->mat.first; ma; ma= ma->id.next) {
			if (ma->samp_gloss_mir == 0) {
				ma->gloss_mir = ma->gloss_tra= 1.0f;
				ma->aniso_gloss_mir = 1.0f;
				ma->samp_gloss_mir = ma->samp_gloss_tra= 18;
				ma->adapt_thresh_mir = ma->adapt_thresh_tra = 0.005f;
				ma->dist_mir = 0.0f;
				ma->fadeto_mir = MA_RAYMIR_FADETOSKY;
			}

			if (ma->strand_min == 0.0f)
				ma->strand_min= 1.0f;
		}

		for (part=main->particle.first; part; part=part->id.next) {
			if (part->ren_child_nbr==0)
				part->ren_child_nbr= part->child_nbr;

			if (part->simplify_refsize==0) {
				part->simplify_refsize= 1920;
				part->simplify_rate= 1.0f;
				part->simplify_transition= 0.1f;
				part->simplify_viewport= 0.8f;
			}
		}

		for (wrld=main->world.first; wrld; wrld= wrld->id.next) {
			if (wrld->ao_approx_error == 0.0f)
				wrld->ao_approx_error= 0.25f;
		}

		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->nodetree)
				ntree_version_245(fd, lib, sce->nodetree);

			if (sce->r.simplify_shadowsamples == 0) {
				sce->r.simplify_subsurf= 6;
				sce->r.simplify_particles= 1.0f;
				sce->r.simplify_shadowsamples= 16;
				sce->r.simplify_aosss= 1.0f;
			}

			if (sce->r.cineongamma == 0) {
				sce->r.cineonblack= 95;
				sce->r.cineonwhite= 685;
				sce->r.cineongamma= 1.7f;
			}
		}

		for (ntree=main->nodetree.first; ntree; ntree= ntree->id.next)
			ntree_version_245(fd, lib, ntree);

		/* fix for temporary flag changes during 245 cycle */
		for (ima= main->image.first; ima; ima= ima->id.next) {
			if (ima->flag & IMA_OLD_PREMUL) {
				ima->flag &= ~IMA_OLD_PREMUL;
				ima->flag |= IMA_DO_PREMUL;
			}
		}

		for (tex=main->tex.first; tex; tex=tex->id.next) {
			if (tex->iuser.flag & IMA_OLD_PREMUL) {
				tex->iuser.flag &= ~IMA_OLD_PREMUL;
				tex->iuser.flag |= IMA_DO_PREMUL;

			}

			ima= blo_do_versions_newlibadr(fd, lib, tex->ima);
			if (ima && (tex->iuser.flag & IMA_DO_PREMUL)) {
				ima->flag &= ~IMA_OLD_PREMUL;
				ima->flag |= IMA_DO_PREMUL;
			}
		}
	}
	
	/* sanity check for skgen
	 * */
	{
		Scene *sce;
		for (sce=main->scene.first; sce; sce = sce->id.next) {
			if (sce->toolsettings->skgen_subdivisions[0] == sce->toolsettings->skgen_subdivisions[1] ||
				sce->toolsettings->skgen_subdivisions[0] == sce->toolsettings->skgen_subdivisions[2] ||
				sce->toolsettings->skgen_subdivisions[1] == sce->toolsettings->skgen_subdivisions[2])
			{
					sce->toolsettings->skgen_subdivisions[0] = SKGEN_SUB_CORRELATION;
					sce->toolsettings->skgen_subdivisions[1] = SKGEN_SUB_LENGTH;
					sce->toolsettings->skgen_subdivisions[2] = SKGEN_SUB_ANGLE;
			}
		}
	}
	

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 2)) {
		Image *ima;

		/* initialize 1:1 Aspect */
		for (ima= main->image.first; ima; ima= ima->id.next) {
			ima->aspx = ima->aspy = 1.0f;				
		}

	}

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 4)) {
		bArmature *arm;
		ModifierData *md;
		Object *ob;
		
		for (arm= main->armature.first; arm; arm= arm->id.next)
			arm->deformflag |= ARM_DEF_B_BONE_REST;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Armature)
					((ArmatureModifierData*)md)->deformflag |= ARM_DEF_B_BONE_REST;
			}
		}
	}

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 5)) {
		/* foreground color needs to be something other then black */
		Scene *sce;
		for (sce= main->scene.first; sce; sce=sce->id.next) {
			sce->r.fg_stamp[0] = sce->r.fg_stamp[1] = sce->r.fg_stamp[2] = 0.8f;
			sce->r.fg_stamp[3] = 1.0f; /* don't use text alpha yet */
			sce->r.bg_stamp[3] = 0.25f; /* make sure the background has full alpha */
		}
	}

	
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 6)) {
		Scene *sce;
		/* fix frs_sec_base */
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->r.frs_sec_base == 0) {
				sce->r.frs_sec_base = 1;
			}
		}
	}
	
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 7)) {
		Object *ob;
		bPoseChannel *pchan;
		bConstraint *con;
		bConstraintTarget *ct;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->pose) {
				for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
					for (con=pchan->constraints.first; con; con=con->next) {
						if (con->type == CONSTRAINT_TYPE_PYTHON) {
							bPythonConstraint *data= (bPythonConstraint *)con->data;
							if (data->tar) {
								/* version patching needs to be done */
								ct= MEM_callocN(sizeof(bConstraintTarget), "PyConTarget");
								
								ct->tar = data->tar;
								BLI_strncpy(ct->subtarget, data->subtarget, sizeof(ct->subtarget));
								ct->space = con->tarspace;
								
								BLI_addtail(&data->targets, ct);
								data->tarnum++;
								
								/* clear old targets to avoid problems */
								data->tar = NULL;
								data->subtarget[0]= '\0';
							}
						}
						else if (con->type == CONSTRAINT_TYPE_LOCLIKE) {
							bLocateLikeConstraint *data= (bLocateLikeConstraint *)con->data;
							
							/* new headtail functionality makes Bone-Tip function obsolete */
							if (data->flag & LOCLIKE_TIP)
								con->headtail = 1.0f;
						}
					}
				}
			}
			
			for (con=ob->constraints.first; con; con=con->next) {
				if (con->type==CONSTRAINT_TYPE_PYTHON) {
					bPythonConstraint *data= (bPythonConstraint *)con->data;
					if (data->tar) {
						/* version patching needs to be done */
						ct= MEM_callocN(sizeof(bConstraintTarget), "PyConTarget");
						
						ct->tar = data->tar;
						BLI_strncpy(ct->subtarget, data->subtarget, sizeof(ct->subtarget));
						ct->space = con->tarspace;
						
						BLI_addtail(&data->targets, ct);
						data->tarnum++;
						
						/* clear old targets to avoid problems */
						data->tar = NULL;
						data->subtarget[0]= '\0';
					}
				}
				else if (con->type == CONSTRAINT_TYPE_LOCLIKE) {
					bLocateLikeConstraint *data= (bLocateLikeConstraint *)con->data;
					
					/* new headtail functionality makes Bone-Tip function obsolete */
					if (data->flag & LOCLIKE_TIP)
						con->headtail = 1.0f;
				}
			}

			if (ob->soft && ob->soft->keys) {
				SoftBody *sb = ob->soft;
				int k;

				for (k=0; k<sb->totkey; k++) {
					if (sb->keys[k])
						MEM_freeN(sb->keys[k]);
				}

				MEM_freeN(sb->keys);

				sb->keys = NULL;
				sb->totkey = 0;
			}
		}
	}

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 8)) {
		Scene *sce;
		Object *ob;
		PartEff *paf=NULL;

		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->soft && ob->soft->keys) {
				SoftBody *sb = ob->soft;
				int k;

				for (k=0; k<sb->totkey; k++) {
					if (sb->keys[k])
						MEM_freeN(sb->keys[k]);
				}

				MEM_freeN(sb->keys);

				sb->keys = NULL;
				sb->totkey = 0;
			}

			/* convert old particles to new system */
			if ((paf = blo_do_version_give_parteff_245(ob))) {
				ParticleSystem *psys;
				ModifierData *md;
				ParticleSystemModifierData *psmd;
				ParticleSettings *part;

				/* create new particle system */
				psys = MEM_callocN(sizeof(ParticleSystem), "particle_system");
				psys->pointcache = BKE_ptcache_add(&psys->ptcaches);

				part = psys->part = psys_new_settings("ParticleSettings", main);
				
				/* needed for proper libdata lookup */
				blo_do_versions_oldnewmap_insert(fd->libmap, psys->part, psys->part, 0);
				part->id.lib= ob->id.lib;

				part->id.us--;
				part->id.flag |= (ob->id.flag & LIB_NEEDLINK);
				
				psys->totpart=0;
				psys->flag= PSYS_ENABLED|PSYS_CURRENT;

				BLI_addtail(&ob->particlesystem, psys);

				md= modifier_new(eModifierType_ParticleSystem);
				BLI_snprintf(md->name, sizeof(md->name), "ParticleSystem %i", BLI_countlist(&ob->particlesystem));
				psmd= (ParticleSystemModifierData*) md;
				psmd->psys=psys;
				BLI_addtail(&ob->modifiers, md);

				/* convert settings from old particle system */
				/* general settings */
				part->totpart = MIN2(paf->totpart, 100000);
				part->sta = paf->sta;
				part->end = paf->end;
				part->lifetime = paf->lifetime;
				part->randlife = paf->randlife;
				psys->seed = paf->seed;
				part->disp = paf->disp;
				part->omat = paf->mat[0];
				part->hair_step = paf->totkey;

				part->eff_group = paf->group;

				/* old system didn't interpolate between keypoints at render time */
				part->draw_step = part->ren_step = 0;

				/* physics */
				part->normfac = paf->normfac * 25.0f;
				part->obfac = paf->obfac;
				part->randfac = paf->randfac * 25.0f;
				part->dampfac = paf->damp;
				copy_v3_v3(part->acc, paf->force);

				/* flags */
				if (paf->stype & PAF_VECT) {
					if (paf->flag & PAF_STATIC) {
						/* new hair lifetime is always 100.0f */
						float fac = paf->lifetime / 100.0f;

						part->draw_as = PART_DRAW_PATH;
						part->type = PART_HAIR;
						psys->recalc |= PSYS_RECALC_REDO;

						part->normfac *= fac;
						part->randfac *= fac;
					}
					else {
						part->draw_as = PART_DRAW_LINE;
						part->draw |= PART_DRAW_VEL_LENGTH;
						part->draw_line[1] = 0.04f;
					}
				}

				part->rotmode = PART_ROT_VEL;
				
				part->flag |= (paf->flag & PAF_BSPLINE) ? PART_HAIR_BSPLINE : 0;
				part->flag |= (paf->flag & PAF_TRAND) ? PART_TRAND : 0;
				part->flag |= (paf->flag & PAF_EDISTR) ? PART_EDISTR : 0;
				part->flag |= (paf->flag & PAF_UNBORN) ? PART_UNBORN : 0;
				part->flag |= (paf->flag & PAF_DIED) ? PART_DIED : 0;
				part->from |= (paf->flag & PAF_FACE) ? PART_FROM_FACE : 0;
				part->draw |= (paf->flag & PAF_SHOWE) ? PART_DRAW_EMITTER : 0;

				psys->vgroup[PSYS_VG_DENSITY] = paf->vertgroup;
				psys->vgroup[PSYS_VG_VEL] = paf->vertgroup_v;
				psys->vgroup[PSYS_VG_LENGTH] = paf->vertgroup_v;

				/* dupliobjects */
				if (ob->transflag & OB_DUPLIVERTS) {
					Object *dup = main->object.first;

					for (; dup; dup= dup->id.next) {
						if (ob == blo_do_versions_newlibadr(fd, lib, dup->parent)) {
							part->dup_ob = dup;
							ob->transflag |= OB_DUPLIPARTS;
							ob->transflag &= ~OB_DUPLIVERTS;

							part->draw_as = PART_DRAW_OB;

							/* needed for proper libdata lookup */
							blo_do_versions_oldnewmap_insert(fd->libmap, dup, dup, 0);
						}
					}
				}

				
				{
					FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
					if (fluidmd && fluidmd->fss && fluidmd->fss->type == OB_FLUIDSIM_PARTICLE)
						part->type = PART_FLUID;
				}

				do_version_free_effects_245(&ob->effect);

				printf("Old particle system converted to new system.\n");
			}
		}

		for (sce= main->scene.first; sce; sce=sce->id.next) {
			ParticleEditSettings *pset= &sce->toolsettings->particle;
			int a;

			if (pset->brush[0].size == 0) {
				pset->flag= PE_KEEP_LENGTHS|PE_LOCK_FIRST|PE_DEFLECT_EMITTER;
				pset->emitterdist= 0.25f;
				pset->totrekey= 5;
				pset->totaddkey= 5;
				pset->brushtype= PE_BRUSH_NONE;

				for (a=0; a<PE_TOT_BRUSH; a++) {
					pset->brush[a].strength= 50;
					pset->brush[a].size= 50;
					pset->brush[a].step= 10;
				}

				pset->brush[PE_BRUSH_CUT].strength= 100;
			}
		}
	}
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 9)) {
		Material *ma;
		int a;

		for (ma=main->mat.first; ma; ma= ma->id.next)
			if (ma->mode & MA_NORMAP_TANG)
				for (a=0; a<MAX_MTEX; a++)
					if (ma->mtex[a] && ma->mtex[a]->tex)
						ma->mtex[a]->normapspace = MTEX_NSPACE_TANGENT;
	}
	
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 10)) {
		Object *ob;
		
		/* dupliface scale */
		for (ob= main->object.first; ob; ob= ob->id.next)
			ob->dupfacesca = 1.0f;
	}
	
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 11)) {
		Object *ob;
		bActionStrip *strip;
		
		/* nla-strips - scale */		
		for (ob= main->object.first; ob; ob= ob->id.next) {
			for (strip= ob->nlastrips.first; strip; strip= strip->next) {
				float length, actlength, repeat;
				
				if (strip->flag & ACTSTRIP_USESTRIDE)
					repeat= 1.0f;
				else
					repeat= strip->repeat;
				
				length = strip->end-strip->start;
				if (length == 0.0f) length= 1.0f;
				actlength = strip->actend-strip->actstart;
				
				strip->scale = length / (repeat * actlength);
				if (strip->scale == 0.0f) strip->scale= 1.0f;
			}	
			if (ob->soft) {
				ob->soft->inpush =  ob->soft->inspring;
				ob->soft->shearstiff = 1.0f; 
			}
		}
	}

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 14)) {
		Scene *sce;
		Sequence *seq;
		
		for (sce=main->scene.first; sce; sce=sce->id.next) {
			SEQ_BEGIN (sce->ed, seq)
			{
				if (seq->blend_mode == 0)
					seq->blend_opacity = 100.0f;
			}
			SEQ_END
		}
	}
	
	/*fix broken group lengths in id properties*/
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 15)) {
		idproperties_fix_group_lengths(main->scene);
		idproperties_fix_group_lengths(main->library);
		idproperties_fix_group_lengths(main->object);
		idproperties_fix_group_lengths(main->mesh);
		idproperties_fix_group_lengths(main->curve);
		idproperties_fix_group_lengths(main->mball);
		idproperties_fix_group_lengths(main->mat);
		idproperties_fix_group_lengths(main->tex);
		idproperties_fix_group_lengths(main->image);
		idproperties_fix_group_lengths(main->latt);
		idproperties_fix_group_lengths(main->lamp);
		idproperties_fix_group_lengths(main->camera);
		idproperties_fix_group_lengths(main->ipo);
		idproperties_fix_group_lengths(main->key);
		idproperties_fix_group_lengths(main->world);
		idproperties_fix_group_lengths(main->screen);
		idproperties_fix_group_lengths(main->script);
		idproperties_fix_group_lengths(main->vfont);
		idproperties_fix_group_lengths(main->text);
		idproperties_fix_group_lengths(main->sound);
		idproperties_fix_group_lengths(main->group);
		idproperties_fix_group_lengths(main->armature);
		idproperties_fix_group_lengths(main->action);
		idproperties_fix_group_lengths(main->nodetree);
		idproperties_fix_group_lengths(main->brush);
		idproperties_fix_group_lengths(main->particle);		
	}

	/* sun/sky */
	if (main->versionfile < 246) {
		Object *ob;
		bActuator *act;

		/* dRot actuator change direction in 2.46 */
		for (ob = main->object.first; ob; ob= ob->id.next) {
			for (act= ob->actuators.first; act; act= act->next) {
				if (act->type == ACT_OBJECT) {
					bObjectActuator *ba= act->data;

					ba->drot[0] = -ba->drot[0];
					ba->drot[1] = -ba->drot[1];
					ba->drot[2] = -ba->drot[2];
				}
			}
		}
	}
	
	// convert fluids to modifier
	if (main->versionfile < 246 || (main->versionfile == 246 && main->subversionfile < 1)) {
		Object *ob;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->fluidsimSettings) {
				FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifier_new(eModifierType_Fluidsim);
				BLI_addhead(&ob->modifiers, (ModifierData *)fluidmd);
				
				MEM_freeN(fluidmd->fss);
				fluidmd->fss = MEM_dupallocN(ob->fluidsimSettings);
				fluidmd->fss->ipo = blo_do_versions_newlibadr_us(fd, ob->id.lib, ob->fluidsimSettings->ipo);
				MEM_freeN(ob->fluidsimSettings);
				
				fluidmd->fss->lastgoodframe = INT_MAX;
				fluidmd->fss->flag = 0;
				fluidmd->fss->meshVelocities = NULL;
			}
		}
	}
	

	if (main->versionfile < 246 || (main->versionfile == 246 && main->subversionfile < 1)) {
		Mesh *me;

		for (me=main->mesh.first; me; me= me->id.next)
			alphasort_version_246(fd, lib, me);
	}
	
	if (main->versionfile < 246 || (main->versionfile == 246 && main->subversionfile < 1)) {
		Object *ob;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->pd && (ob->pd->forcefield == PFIELD_WIND))
				ob->pd->f_noise = 0.0f;
		}
	}

	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 2)) {
		Object *ob;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			ob->gameflag |= OB_COLLISION;
			ob->margin = 0.06f;
		}
	}

	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 3)) {
		Object *ob;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			// Starting from subversion 3, ACTOR is a separate feature.
			// Before it was conditioning all the other dynamic flags
			if (!(ob->gameflag & OB_ACTOR))
				ob->gameflag &= ~(OB_GHOST|OB_DYNAMIC|OB_RIGID_BODY|OB_SOFT_BODY|OB_COLLISION_RESPONSE);
			/* suitable default for older files */
		}
	}

	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 5)) {
		Lamp *la= main->lamp.first;
		for (; la; la= la->id.next) {
			la->skyblendtype= MA_RAMP_ADD;
			la->skyblendfac= 1.0f;
		}
	}
	
	/* set the curve radius interpolation to 2.47 default - easy */
	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 6)) {
		Curve *cu;
		Nurb *nu;
		
		for (cu= main->curve.first; cu; cu= cu->id.next) {
			for (nu= cu->nurb.first; nu; nu= nu->next) {
				if (nu) {
					nu->radius_interp = 3;
					
					/* resolu and resolv are now used differently for surfaces
					 * rather than using the resolution to define the entire number of divisions,
					 * use it for the number of divisions per segment
					 */
					if (nu->pntsv > 1) {
						nu->resolu = MAX2( 1, (int)(((float)nu->resolu / (float)nu->pntsu)+0.5f) );
						nu->resolv = MAX2( 1, (int)(((float)nu->resolv / (float)nu->pntsv)+0.5f) );
					}
				}
			}
		}
	}
	/* direction constraint actuators were always local in previous version */
	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 7)) {
		bActuator *act;
		Object *ob;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			for (act= ob->actuators.first; act; act= act->next) {
				if (act->type == ACT_CONSTRAINT) {
					bConstraintActuator *coa = act->data;
					if (coa->type == ACT_CONST_TYPE_DIST) {
						coa->flag |= ACT_CONST_LOCAL;
					}
				}
			}
		}
	}

	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 9)) {
		Lamp *la= main->lamp.first;
		for (; la; la= la->id.next) {
			la->sky_exposure= 1.0f;
		}
	}
	
	/* BGE message actuators needed OB prefix, very confusing */
	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 10)) {
		bActuator *act;
		Object *ob;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			for (act= ob->actuators.first; act; act= act->next) {
				if (act->type == ACT_MESSAGE) {
					bMessageActuator *msgAct = (bMessageActuator *) act->data;
					if (BLI_strnlen(msgAct->toPropName, 3) > 2) {
						/* strip first 2 chars, would have only worked if these were OB anyway */
						memmove(msgAct->toPropName, msgAct->toPropName + 2, sizeof(msgAct->toPropName) - 2);
					}
					else {
						msgAct->toPropName[0] = '\0';
					}
				}
			}
		}
	}

	if (main->versionfile < 248) {
		Lamp *la;

		for (la=main->lamp.first; la; la= la->id.next) {
			if (la->atm_turbidity == 0.0f) {
				la->sun_effect_type = 0;
				la->horizon_brightness = 1.0f;
				la->spread = 1.0f;
				la->sun_brightness = 1.0f;
				la->sun_size = 1.0f;
				la->backscattered_light = 1.0f;
				la->atm_turbidity = 2.0f;
				la->atm_inscattering_factor = 1.0f;
				la->atm_extinction_factor = 1.0f;
				la->atm_distance_factor = 1.0f;
				la->sun_intensity = 1.0f;
			}
		}
	}

	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 2)) {
		Scene *sce;
		
		/* Note, these will need to be added for painting */
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->toolsettings->imapaint.seam_bleed = 2;
			sce->toolsettings->imapaint.normal_angle = 80;

			/* initialize skeleton generation toolsettings */
			sce->toolsettings->skgen_resolution = 250;
			sce->toolsettings->skgen_threshold_internal 	= 0.1f;
			sce->toolsettings->skgen_threshold_external 	= 0.1f;
			sce->toolsettings->skgen_angle_limit			= 30.0f;
			sce->toolsettings->skgen_length_ratio			= 1.3f;
			sce->toolsettings->skgen_length_limit			= 1.5f;
			sce->toolsettings->skgen_correlation_limit		= 0.98f;
			sce->toolsettings->skgen_symmetry_limit			= 0.1f;
			sce->toolsettings->skgen_postpro = SKGEN_SMOOTH;
			sce->toolsettings->skgen_postpro_passes = 3;
			sce->toolsettings->skgen_options = SKGEN_FILTER_INTERNAL|SKGEN_FILTER_EXTERNAL|SKGEN_FILTER_SMART|SKGEN_SUB_CORRELATION|SKGEN_HARMONIC;
			sce->toolsettings->skgen_subdivisions[0] = SKGEN_SUB_CORRELATION;
			sce->toolsettings->skgen_subdivisions[1] = SKGEN_SUB_LENGTH;
			sce->toolsettings->skgen_subdivisions[2] = SKGEN_SUB_ANGLE;

			
			sce->toolsettings->skgen_retarget_angle_weight = 1.0f;
			sce->toolsettings->skgen_retarget_length_weight = 1.0f;
			sce->toolsettings->skgen_retarget_distance_weight = 1.0f;
	
			/* Skeleton Sketching */
			sce->toolsettings->bone_sketching = 0;
			sce->toolsettings->skgen_retarget_roll = SK_RETARGET_ROLL_VIEW;
		}
	}
	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 3)) {
		bScreen *sc;
		
		/* adjust default settings for Animation Editors */
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			
			for (sa= sc->areabase.first; sa; sa= sa->next) { 
				SpaceLink *sl;
				
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					switch (sl->spacetype) {
						case SPACE_ACTION:
						{
							SpaceAction *sact= (SpaceAction *)sl;
							
							sact->mode= SACTCONT_DOPESHEET;
							sact->autosnap= SACTSNAP_FRAME;
						}
							break;
						case SPACE_IPO:
						{
							SpaceIpo *sipo= (SpaceIpo *)sl;
							sipo->autosnap= SACTSNAP_FRAME;
						}
							break;
						case SPACE_NLA:
						{
							SpaceNla *snla= (SpaceNla *)sl;
							snla->autosnap= SACTSNAP_FRAME;
						}
							break;
					}
				}
			}
		}
	}

	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 3)) {
		Object *ob;

		/* Adjustments needed after Bullets update */
		for (ob = main->object.first; ob; ob= ob->id.next) {
			ob->damping *= 0.635f;
			ob->rdamping = 0.1f + (0.8f * ob->rdamping);
		}
	}
	
	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 4)) {
		Scene *sce;
		World *wrld;

		/*  Dome (Fisheye) default parameters  */
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->r.domeangle = 180;
			sce->r.domemode = 1;
			sce->r.domeres = 4;
			sce->r.domeresbuf = 1.0f;
			sce->r.dometilt = 0;
		}
		/* DBVT culling by default */
		for (wrld=main->world.first; wrld; wrld= wrld->id.next) {
			wrld->mode |= WO_DBVT_CULLING;
			wrld->occlusionRes = 128;
		}
	}

	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 5)) {
		Object *ob;
		World *wrld;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			ob->m_contactProcessingThreshold = 1.0f; //pad3 is used for m_contactProcessingThreshold
			if (ob->parent) {
				/* check if top parent has compound shape set and if yes, set this object
				 * to compound shaper as well (was the behavior before, now it's optional) */
				Object *parent= blo_do_versions_newlibadr(fd, lib, ob->parent);
				while (parent && parent != ob &&  parent->parent != NULL) {
					parent = blo_do_versions_newlibadr(fd, lib, parent->parent);
				}
				if (parent) {
					if (parent->gameflag & OB_CHILD)
						ob->gameflag |= OB_CHILD;
				}
			}
		}
		for (wrld=main->world.first; wrld; wrld= wrld->id.next) {
			wrld->ticrate = 60;
			wrld->maxlogicstep = 5;
			wrld->physubstep = 1;
			wrld->maxphystep = 5;
		}
	}
	
	// correct introduce of seed for wind force
	if (main->versionfile < 249 && main->subversionfile < 1) {
		Object *ob;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->pd)
				ob->pd->seed = ((unsigned int)(ceil(PIL_check_seconds_timer()))+1) % 128;
		}
	
	}

	if (main->versionfile < 249 && main->subversionfile < 2) {
		Scene *sce= main->scene.first;
		Sequence *seq;
		Editing *ed;
		
		while (sce) {
			ed= sce->ed;
			if (ed) {
				SEQP_BEGIN (ed, seq)
				{
					if (seq->strip && seq->strip->proxy) {
						seq->strip->proxy->quality =90;
					}
				}
				SEQ_END
			}
			
			sce= sce->id.next;
		}

	}
	
	/* WATCH IT!!!: pointers from libdata have not been converted yet here! */
	/* WATCH IT 2!: Userdef struct init has to be in editors/interface/resources.c! */

	/* don't forget to set version number in blender.c! */
}

