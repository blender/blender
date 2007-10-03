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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h" // only for uvedit_selectionCB() (struct Object)
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_colortools.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_imasel.h"
#include "BIF_interface.h"
#include "BIF_drawimage.h"
#include "BIF_editview.h"
#include "BIF_editsima.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_transform.h"
#include "BIF_writeimage.h"
#include "BIF_editmesh.h"

#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BSE_trans_types.h"

#include "BDR_editobject.h"
#include "BDR_unwrapper.h"

#include "BMF_Api.h"

#include "RE_pipeline.h"

#include "blendef.h"
#include "multires.h"
#include "mydevice.h"
#include "editmesh.h"

/* local prototypes */
void sel_uvco_inside_radius(short , EditFace *efa, MTFace *, int , float *, float *, short);
void uvedit_selectionCB(short , Object *, short *, float ); /* used in edit.c*/ 

void object_uvs_changed(Object *ob)
{
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

void object_tface_flags_changed(Object *ob, int updateButtons)
{
	if (updateButtons) allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}

int is_uv_tface_editing_allowed_silent(void)
{
	if(!EM_texFaceCheck()) return 0;
	if(G.sima->mode!=SI_TEXTURE) return 0;
	if(multires_level1_test()) return 0;	
	return 1;
}

int is_uv_tface_editing_allowed(void)
{
	if(!G.obedit) error("Enter Edit Mode to perform this action");

	return is_uv_tface_editing_allowed_silent();
}

void get_connected_limit_tface_uv(float *limit)
{
	ImBuf *ibuf= BKE_image_get_ibuf(G.sima->image, &G.sima->iuser);
	if(ibuf && ibuf->x > 0 && ibuf->y > 0) {
		limit[0]= 0.05/(float)ibuf->x;
		limit[1]= 0.05/(float)ibuf->y;
	}
	else
		limit[0]= limit[1]= 0.05/256.0;
}

void be_square_tface_uv(EditMesh *em)
{
	EditFace *efa;
	MTFace *tface;
	/* if 1 vertex selected: doit (with the selected vertex) */
	for (efa= em->faces.first; efa; efa= efa->next) {
		if (efa->v4) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tface)) {
				if (SIMA_UVSEL_CHECK(efa, tface, 0)) {
					if( tface->uv[1][0] == tface->uv[2][0] ) {
						tface->uv[1][1]= tface->uv[0][1];
						tface->uv[3][0]= tface->uv[0][0];
					}
					else {	
						tface->uv[1][0]= tface->uv[0][0];
						tface->uv[3][1]= tface->uv[0][1];
					}
					
				}
				if (SIMA_UVSEL_CHECK(efa, tface, 1)) {
					if( tface->uv[2][1] == tface->uv[3][1] ) {
						tface->uv[2][0]= tface->uv[1][0];
						tface->uv[0][1]= tface->uv[1][1];
					}
					else {
						tface->uv[2][1]= tface->uv[1][1];
						tface->uv[0][0]= tface->uv[1][0];
					}

				}
				if (SIMA_UVSEL_CHECK(efa, tface, 2)) {
					if( tface->uv[3][0] == tface->uv[0][0] ) {
						tface->uv[3][1]= tface->uv[2][1];
						tface->uv[1][0]= tface->uv[2][0];
					}
					else {
						tface->uv[3][0]= tface->uv[2][0];
						tface->uv[1][1]= tface->uv[2][1];
					}
				}
				if (SIMA_UVSEL_CHECK(efa, tface, 3)) {
					if( tface->uv[0][1] == tface->uv[1][1] ) {
						tface->uv[0][0]= tface->uv[3][0];
						tface->uv[2][1]= tface->uv[3][1];
					}
					else  {
						tface->uv[0][1]= tface->uv[3][1];
						tface->uv[2][0]= tface->uv[3][0];
					}

				}
			}
		}
	}
}

void transform_aspect_ratio_tface_uv(float *aspx, float *aspy)
{
	int w, h;
	float xuser_asp, yuser_asp;
	
	aspect_sima(G.sima, &xuser_asp, &yuser_asp);
	
	transform_width_height_tface_uv(&w, &h);
	*aspx= (float)w/256.0f * xuser_asp;
	*aspy= (float)h/256.0f * yuser_asp;
}

void transform_width_height_tface_uv(int *width, int *height)
{
	ImBuf *ibuf= BKE_image_get_ibuf(G.sima->image, &G.sima->iuser);

	if(ibuf) {
		*width= ibuf->x;
		*height= ibuf->y;
	}
	else {
		*width= 256;
		*height= 256;
	}
}

void mirror_tface_uv(char mirroraxis)
{
	if (mirroraxis == 'x')
		Mirror(1); /* global x */
	else if (mirroraxis == 'y')
		Mirror(2); /* global y */
}

void mirrormenu_tface_uv(void)
{
	short mode= 0;

	if( is_uv_tface_editing_allowed()==0 ) return;

	mode= pupmenu("Mirror%t|X Axis%x1|Y Axis%x2|");

	if(mode==-1) return;

	if(mode==1) mirror_tface_uv('x');
	else if(mode==2) mirror_tface_uv('y');

	BIF_undo_push("Mirror UV");
}

void weld_align_tface_uv(char tool)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;
	float cent[2];
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	cent_tface_uv(cent, 0);

	if(tool == 'x' || tool == 'w') {
		for (efa= em->faces.first; efa; efa= efa->next) {
			tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tface)) {
				if (SIMA_UVSEL_CHECK(efa, tface, 0))
					tface->uv[0][0]= cent[0];
				if (SIMA_UVSEL_CHECK(efa, tface, 1))
					tface->uv[1][0]= cent[0];
				if (SIMA_UVSEL_CHECK(efa, tface, 2))
					tface->uv[2][0]= cent[0];
				if (efa->v4 && SIMA_UVSEL_CHECK(efa, tface, 3))
					tface->uv[3][0]= cent[0];
			}
		}
	}

	if(tool == 'y' || tool == 'w') {
		for (efa= em->faces.first; efa; efa= efa->next) {
			tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tface)) {
				if (SIMA_UVSEL_CHECK(efa, tface, 0))
					tface->uv[0][1]= cent[1];
				if (SIMA_UVSEL_CHECK(efa, tface, 1))
					tface->uv[1][1]= cent[1];
				if (SIMA_UVSEL_CHECK(efa, tface, 2))
					tface->uv[2][1]= cent[1];
				if (efa->v4 && SIMA_UVSEL_CHECK(efa, tface, 3))
					tface->uv[3][1]= cent[1];
			}
		}
	}

	object_uvs_changed(OBACT);
}

// just for averaging UV's
typedef struct UVVertAverage {
	float uv[2];
	int count;
} UVVertAverage;

void stitch_vert_uv_tface(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	EditVert *eve;
	MTFace *tface;
	int count;
	UVVertAverage *uv_average, *uvav;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	
	// index and count verts
	for (count=0, eve=em->verts.first; eve; count++, eve= eve->next) {
		eve->tmp.l = count;
	}
	
	uv_average = MEM_callocN(sizeof(UVVertAverage) * count, "Stitch");
	
	// gather uv averages per vert
	for (efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
			if (SIMA_UVSEL_CHECK(efa, tface, 0)) {
				uvav = uv_average + efa->v1->tmp.l;
				uvav->count++;
				uvav->uv[0] += tface->uv[0][0];
				uvav->uv[1] += tface->uv[0][1];
			}
			if (SIMA_UVSEL_CHECK(efa, tface, 1)) {
				uvav = uv_average + efa->v2->tmp.l;
				uvav->count++;
				uvav->uv[0] += tface->uv[1][0];
				uvav->uv[1] += tface->uv[1][1];
			}
			if (SIMA_UVSEL_CHECK(efa, tface, 2)) {
				uvav = uv_average + efa->v3->tmp.l;
				uvav->count++;
				uvav->uv[0] += tface->uv[2][0];
				uvav->uv[1] += tface->uv[2][1];
			}
			if (efa->v4 && SIMA_UVSEL_CHECK(efa, tface, 3)) {
				uvav = uv_average + efa->v4->tmp.l;
				uvav->count++;
				uvav->uv[0] += tface->uv[3][0];
				uvav->uv[1] += tface->uv[3][1];
			}
		}
	}
	
	// apply uv welding
	for (efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
			if (SIMA_UVSEL_CHECK(efa, tface, 0)) {
				uvav = uv_average + efa->v1->tmp.l;
				tface->uv[0][0] = uvav->uv[0]/uvav->count;
				tface->uv[0][1] = uvav->uv[1]/uvav->count;
			}
			if (SIMA_UVSEL_CHECK(efa, tface, 1)) {
				uvav = uv_average + efa->v2->tmp.l;
				tface->uv[1][0] = uvav->uv[0]/uvav->count;
				tface->uv[1][1] = uvav->uv[1]/uvav->count;
			}
			if (SIMA_UVSEL_CHECK(efa, tface, 2)) {
				uvav = uv_average + efa->v3->tmp.l;
				tface->uv[2][0] = uvav->uv[0]/uvav->count;
				tface->uv[2][1] = uvav->uv[1]/uvav->count;
			}
			if (efa->v4 && SIMA_UVSEL_CHECK(efa, tface, 3)) {
				uvav = uv_average + efa->v4->tmp.l;
				tface->uv[3][0] = uvav->uv[0]/uvav->count;
				tface->uv[3][1] = uvav->uv[1]/uvav->count;
			}
		}
	}
	MEM_freeN(uv_average);
	object_uvs_changed(OBACT);
}

void weld_align_menu_tface_uv(void)
{
	short mode= 0;

	if( is_uv_tface_editing_allowed()==0 ) return;

	mode= pupmenu("Weld/Align%t|Weld%x1|Align X%x2|Align Y%x3");

	if(mode==-1) return;
	if(mode==1) weld_align_tface_uv('w');
	else if(mode==2) weld_align_tface_uv('x');
	else if(mode==3) weld_align_tface_uv('y');

	if(mode==1) BIF_undo_push("Weld UV");
	else if(mode==2 || mode==3) BIF_undo_push("Align UV");
}

void select_invert_tface_uv(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;
	
	if( is_uv_tface_editing_allowed()==0 ) return;

	if (G.sima->flag & SI_SYNC_UVSEL) {
		/* Warning, this is not that good (calling editmode stuff from UV),
		TODO look into changing it */
		selectswap_mesh();
		return;
	} else {
		for (efa= em->faces.first; efa; efa= efa->next) {
			tface = CustomData_em_get(&G.editMesh->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tface)) {
				tface->flag ^= TF_SEL1;
				tface->flag ^= TF_SEL2;
				tface->flag ^= TF_SEL3;
				if(efa->v4) tface->flag ^= TF_SEL4;
			}
		}
	}
	BIF_undo_push("Select Inverse UV");

	allqueue(REDRAWIMAGE, 0);
}

void select_swap_tface_uv(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;
	int sel=0;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	
	if (G.sima->flag & SI_SYNC_UVSEL) {
		deselectall_mesh();
		return;
	} else {
			
		for (efa= em->faces.first; efa; efa= efa->next) {
			tface = CustomData_em_get(&G.editMesh->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tface)) {
				if(tface->flag & (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4)) {
					sel= 1;
					break;
				}
			}
		}
	
		for (efa= em->faces.first; efa; efa= efa->next) {
			tface = CustomData_em_get(&G.editMesh->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tface)) {
				if(efa->v4) {
					if(sel) tface->flag &= ~(TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
					else tface->flag |= (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
				}
				else {
					if(sel) tface->flag &= ~(TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
					else tface->flag |= (TF_SEL1+TF_SEL2+TF_SEL3);
				}
			}
		}
	}
	BIF_undo_push("Select swap");

	allqueue(REDRAWIMAGE, 0);
}

static int msel_hit(float *limit, unsigned int *hitarray, unsigned int vertexid, float **uv, float *uv2, int sticky)
{
	int i;
	for(i=0; i< 4; i++) {
		if(hitarray[i] == vertexid) {
			if(sticky == 2) {
				if(fabs(uv[i][0]-uv2[0]) < limit[0] &&
			    fabs(uv[i][1]-uv2[1]) < limit[1])
					return 1;
			}
			else return 1;
		}
	}
	return 0;
}

static void find_nearest_tface(MTFace **nearesttf, EditFace **nearestefa)
{
	EditMesh *em= G.editMesh;
	MTFace *tf;
	EditFace *efa;
	int i, nverts, mindist, dist, fcenter[2], uval[2];
	short mval[2];

	getmouseco_areawin(mval);	

	mindist= 0x7FFFFFF;
	*nearesttf= NULL;
	*nearestefa= NULL;
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tf)) {
			fcenter[0]= fcenter[1]= 0;
			nverts= efa->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				uvco_to_areaco_noclip(tf->uv[i], uval);
				fcenter[0] += uval[0];
				fcenter[1] += uval[1];
			}

			fcenter[0] /= nverts;
			fcenter[1] /= nverts;

			dist= abs(mval[0]- fcenter[0])+ abs(mval[1]- fcenter[1]);
			if (dist < mindist) {
				*nearesttf= tf;
				*nearestefa= efa;
				mindist= dist;
			}
		}
	}
}

static int nearest_uv_between(MTFace *tf, int nverts, int id, short *mval, int *uval)
{
	float m[3], v1[3], v2[3], c1, c2;
	int id1, id2;

	id1= (id+nverts-1)%nverts;
	id2= (id+nverts+1)%nverts;

	m[0] = (float)(mval[0]-uval[0]);
	m[1] = (float)(mval[1]-uval[1]);
	Vec2Subf(v1, tf->uv[id1], tf->uv[id]);
	Vec2Subf(v2, tf->uv[id2], tf->uv[id]);

	/* m and v2 on same side of v-v1? */
	c1= v1[0]*m[1] - v1[1]*m[0];
	c2= v1[0]*v2[1] - v1[1]*v2[0];

	if (c1*c2 < 0.0f)
		return 0;

	/* m and v1 on same side of v-v2? */
	c1= v2[0]*m[1] - v2[1]*m[0];
	c2= v2[0]*v1[1] - v2[1]*v1[0];

	return (c1*c2 >= 0.0f);
}

void find_nearest_uv(MTFace **nearesttf, EditFace **nearestefa, unsigned int *nearestv, int *nearestuv)
{
	EditMesh *em= G.editMesh;
	EditFace *efa;
	MTFace *tf;
	int i, nverts, mindist, dist, uval[2];
	short mval[2];

	getmouseco_areawin(mval);	

	mindist= 0x7FFFFFF;
	if (nearesttf) *nearesttf= NULL;
	if (nearestefa) *nearestefa= NULL;
	
	if (nearestv) {
		EditVert *ev;
		for (i=0, ev=em->verts.first; ev; ev = ev->next, i++)
			ev->tmp.l = i;
	}
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tf)) {
			nverts= efa->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				uvco_to_areaco_noclip(tf->uv[i], uval);
				dist= abs(mval[0]-uval[0]) + abs(mval[1]-uval[1]);

				if (SIMA_UVSEL_CHECK(efa, tf, i))
					dist += 5;

				if(dist<=mindist) {
					if(dist==mindist)
						if (!nearest_uv_between(tf, nverts, i, mval, uval))
							continue;

					mindist= dist;
					*nearestuv= i;
					
					if (nearesttf)		*nearesttf= tf;
					if (nearestefa)		*nearestefa= efa;
					if (nearestv) {
						if (i==0) *nearestv=  efa->v1->tmp.l;
						else if (i==1) *nearestv=  efa->v2->tmp.l;
						else if (i==2) *nearestv=  efa->v3->tmp.l;
						else *nearestv=  efa->v4->tmp.l;
					}
				}
			}
		}
	}
}

void mouse_select_sima(void) /* TODO - SYNCSEL */
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tf, *nearesttf;
	EditFace *nearestefa=NULL;
	int a, selectsticky, actface, nearestuv, i;
	char sticky= 0;
	short flush = 0; /* 0 == dont flush, 1 == sel, -1 == desel;  only use when selection sync is enabled */
	unsigned int hitv[4], nearestv;
	float *hituv[4], limit[2];
	
	if( is_uv_tface_editing_allowed()==0 ) return;

	get_connected_limit_tface_uv(limit);
	
	if (G.sima->flag & SI_SYNC_UVSEL) {
		/* copy from mesh */
		if (G.scene->selectmode == SCE_SELECT_FACE) {
			actface= 1;
			sticky= 0;
		} else {
			actface= (G.qual & LR_ALTKEY || G.sima->flag & SI_SELACTFACE);
			sticky= 2;
		}
	} else {
		/* normal operation */
		actface= (G.qual & LR_ALTKEY || G.sima->flag & SI_SELACTFACE);
		
		switch(G.sima->sticky) {
		case 0:
			sticky=2;
			break;
		case 1:
			sticky=0;
			break;
		case 2:
			if(G.qual & LR_CTRLKEY) {
				sticky=0;
			} else {  
				sticky=1;
			}
			break;
		}
	}

	if(actface) {
		find_nearest_tface(&nearesttf, &nearestefa);
		if(nearesttf==NULL)
			return;
		
		EM_set_actFace(nearestefa);

		for (i=0; i<4; i++)
			hituv[i]= nearesttf->uv[i];

		hitv[0]= nearestefa->v1->tmp.l;
		hitv[1]= nearestefa->v2->tmp.l;
		hitv[2]= nearestefa->v3->tmp.l;
		
		if (nearestefa->v4)	hitv[3]= nearestefa->v4->tmp.l;
		else				hitv[3]= 0xFFFFFFFF;
	}
	else {
		find_nearest_uv(&nearesttf, &nearestefa, &nearestv, &nearestuv);
		if(nearesttf==NULL)
			return;

		if(sticky) {
			for(i=0; i<4; i++)
				hitv[i]= 0xFFFFFFFF;
			hitv[nearestuv]= nearestv;
			hituv[nearestuv]= nearesttf->uv[nearestuv];
		}
	}

	if(G.qual & LR_SHIFTKEY) {
		/* (de)select face */
		if(actface) {
			if(SIMA_FACESEL_CHECK(nearestefa, nearesttf)) {
				SIMA_FACESEL_UNSET(nearestefa, nearesttf);
				selectsticky= 0;
			}
			else {
				SIMA_FACESEL_SET(nearestefa, nearesttf);
				selectsticky= 1;
			}
			flush = -1;
		}
		/* (de)select uv node */
		else {
			if (SIMA_UVSEL_CHECK(nearestefa, nearesttf, nearestuv)) {
				SIMA_UVSEL_UNSET(nearestefa, nearesttf, nearestuv);
				selectsticky= 0;
			}
			else {
				SIMA_UVSEL_SET(nearestefa, nearesttf, nearestuv);
				selectsticky= 1;
			}
			flush = 1;
		}

		/* (de)select sticky uv nodes */
		if(sticky || actface) {
			EditVert *ev;
			
			for (a=0, ev=em->verts.first; ev; ev = ev->next, a++)
				ev->tmp.l = a;
			
			/* deselect */
			if(selectsticky==0) {
				for (efa= em->faces.first; efa; efa= efa->next) {
					tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					if (SIMA_FACEDRAW_CHECK(efa, tf)) {
						/*if(nearesttf && tf!=nearesttf) tf->flag &=~ TF_ACTIVE;*/ /* TODO - deal with editmesh active face */
						if (!sticky) continue;
	
						if(msel_hit(limit, hitv, efa->v1->tmp.l, hituv, tf->uv[0], sticky))
							SIMA_UVSEL_UNSET(efa, tf, 0);
						if(msel_hit(limit, hitv, efa->v2->tmp.l, hituv, tf->uv[1], sticky))
							SIMA_UVSEL_UNSET(efa, tf, 1);
						if(msel_hit(limit, hitv, efa->v3->tmp.l, hituv, tf->uv[2], sticky))
							SIMA_UVSEL_UNSET(efa, tf, 2);
						if (efa->v4)
							if(msel_hit(limit, hitv, efa->v4->tmp.l, hituv, tf->uv[3], sticky))
								SIMA_UVSEL_UNSET(efa, tf, 3);
					}
				}
				flush = -1;
			}
			/* select */
			else {
				for (efa= em->faces.first; efa; efa= efa->next) {
					tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					if (SIMA_FACEDRAW_CHECK(efa, tf)) {
						if (!sticky) continue;
						if(msel_hit(limit, hitv, efa->v1->tmp.l, hituv, tf->uv[0], sticky))
							SIMA_UVSEL_SET(efa, tf, 0);
						if(msel_hit(limit, hitv, efa->v2->tmp.l, hituv, tf->uv[1], sticky))
							SIMA_UVSEL_SET(efa, tf, 1);
						if(msel_hit(limit, hitv, efa->v3->tmp.l, hituv, tf->uv[2], sticky))
							SIMA_UVSEL_SET(efa, tf, 2);
						if (efa->v4)
							if(msel_hit(limit, hitv, efa->v4->tmp.l, hituv, tf->uv[3], sticky))
								SIMA_UVSEL_SET(efa, tf, 3);
					}
				}
				EM_set_actFace(nearestefa);
				flush = 1;
			}			
		}
	}
	else {
		/* select face and deselect other faces */ 
		if(actface) {
			for (efa= em->faces.first; efa; efa= efa->next) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				SIMA_FACESEL_UNSET(efa, tf);
			}
			if(nearesttf) {
				SIMA_FACESEL_SET(nearestefa, nearesttf);
				EM_set_actFace(nearestefa);
			}
				
		}

		/* deselect uvs, and select sticky uvs */
		for (efa= em->faces.first; efa; efa= efa->next) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tf)) {
				if(!actface) SIMA_FACESEL_UNSET(efa, tf);
				if(!sticky) continue;

				if(msel_hit(limit, hitv, efa->v1->tmp.l, hituv, tf->uv[0], sticky))
					SIMA_UVSEL_SET(efa, tf, 0);
				if(msel_hit(limit, hitv, efa->v2->tmp.l, hituv, tf->uv[1], sticky))
					SIMA_UVSEL_SET(efa, tf, 1);
				if(msel_hit(limit, hitv, efa->v3->tmp.l, hituv, tf->uv[2], sticky))
					SIMA_UVSEL_SET(efa, tf, 2);
				if(efa->v4)
					if(msel_hit(limit, hitv, efa->v4->tmp.l, hituv, tf->uv[3], sticky))
						SIMA_UVSEL_SET(efa, tf, 3);
				flush= 1;
			}
		}
		
		if(!actface) {
			SIMA_UVSEL_SET(nearestefa, nearesttf, nearestuv);
			flush= 1;
		}
	}
	
	force_draw(1);
	
	if (G.sima->flag & SI_SYNC_UVSEL) {
		/* flush for mesh selection */
		if (G.scene->selectmode != SCE_SELECT_FACE) {
			if (flush==1)		EM_select_flush();
			else if (flush==-1)	EM_deselect_flush();
		}
		allqueue(REDRAWVIEW3D, 0); /* mesh selection has changed */
	}
	
	BIF_undo_push("Select UV");
	rightmouse_transform();
}

void borderselect_sima(short whichuvs)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;
	rcti rect;
	rctf rectf;
	int val;
	short mval[2];

	if( is_uv_tface_editing_allowed()==0) return;

	val= get_border(&rect, 3);

	if(val) {
		mval[0]= rect.xmin;
		mval[1]= rect.ymin;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

		for (efa= em->faces.first; efa; efa= efa->next) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tface)) {
				if (whichuvs == UV_SELECT_ALL || (G.sima->flag & SI_SYNC_UVSEL) ) {
					/* SI_SYNC_UVSEL - cant do pinned selection */
					if(BLI_in_rctf(&rectf, (float)tface->uv[0][0], (float)tface->uv[0][1])) {
						if(val==LEFTMOUSE)	SIMA_UVSEL_SET(efa, tface, 0);
						else				SIMA_UVSEL_UNSET(efa, tface, 0);
					}
					if(BLI_in_rctf(&rectf, (float)tface->uv[1][0], (float)tface->uv[1][1])) {
						if(val==LEFTMOUSE)	SIMA_UVSEL_SET(efa, tface, 1);
						else				SIMA_UVSEL_UNSET(efa, tface, 1);
					}
					if(BLI_in_rctf(&rectf, (float)tface->uv[2][0], (float)tface->uv[2][1])) {
						if(val==LEFTMOUSE)	SIMA_UVSEL_SET(efa, tface, 2);
						else				SIMA_UVSEL_UNSET(efa, tface, 2);
					}
					if(efa->v4 && BLI_in_rctf(&rectf, (float)tface->uv[3][0], (float)tface->uv[3][1])) {
						if(val==LEFTMOUSE)	SIMA_UVSEL_SET(efa, tface, 3);
						else				SIMA_UVSEL_UNSET(efa, tface, 3);
					}
				} else if (whichuvs == UV_SELECT_PINNED) {
					if ((tface->unwrap & TF_PIN1) && 
						BLI_in_rctf(&rectf, (float)tface->uv[0][0], (float)tface->uv[0][1])) {
						
						if(val==LEFTMOUSE)	SIMA_UVSEL_SET(efa, tface, 0);
						else				SIMA_UVSEL_UNSET(efa, tface, 0);
					}
					if ((tface->unwrap & TF_PIN2) && 
						BLI_in_rctf(&rectf, (float)tface->uv[1][0], (float)tface->uv[1][1])) {
						
						if(val==LEFTMOUSE)	SIMA_UVSEL_SET(efa, tface, 1);
						else				SIMA_UVSEL_UNSET(efa, tface, 1);
					}
					if ((tface->unwrap & TF_PIN3) && 
						BLI_in_rctf(&rectf, (float)tface->uv[2][0], (float)tface->uv[2][1])) {
						
						if(val==LEFTMOUSE)	SIMA_UVSEL_SET(efa, tface, 2);
						else				SIMA_UVSEL_UNSET(efa, tface, 2);
					}
					if ((efa->v4) && (tface->unwrap & TF_PIN4) && BLI_in_rctf(&rectf, (float)tface->uv[3][0], (float)tface->uv[3][1])) {
						if(val==LEFTMOUSE)	SIMA_UVSEL_SET(efa, tface, 3);
						else				SIMA_UVSEL_UNSET(efa, tface, 3);
					}
				}
			}
		}
		
		/* make sure newly selected vert selection is updated*/
		if (G.sima->flag & SI_SYNC_UVSEL) {
			if (G.scene->selectmode != SCE_SELECT_FACE) {
				if (val==LEFTMOUSE)	EM_select_flush();
				else				EM_deselect_flush();
			}
			allqueue(REDRAWVIEW3D, 0); /* mesh selection has changed */
		}
		
		BIF_undo_push("Border select UV");
		scrarea_queue_winredraw(curarea);
	}
}

int snap_uv_sel_to_curs(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;
	short change = 0;

	for (efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
			if (SIMA_UVSEL_CHECK(efa, tface, 0))		VECCOPY2D(tface->uv[0], G.v2d->cursor);
			if (SIMA_UVSEL_CHECK(efa, tface, 1))		VECCOPY2D(tface->uv[1], G.v2d->cursor);
			if (SIMA_UVSEL_CHECK(efa, tface, 2))		VECCOPY2D(tface->uv[2], G.v2d->cursor);
			if (efa->v4)
				if (SIMA_UVSEL_CHECK(efa, tface, 3))	VECCOPY2D(tface->uv[3], G.v2d->cursor);
			change = 1;
		}
	}
	return change;
}

int snap_uv_sel_to_adj_unsel(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	EditVert *eve;
	MTFace *tface;
	short change = 0;
	int count = 0;
	float *coords;
	short *usercount, users;
	
	/* set all verts to -1 : an unused index*/
	for (eve= em->verts.first; eve; eve= eve->next)
		eve->tmp.l=-1;
	
	/* index every vert that has a selected UV using it, but only once so as to
	 * get unique indicies and to count how much to malloc */
	for (efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
			if (SIMA_UVSEL_CHECK(efa, tface, 0) && efa->v1->tmp.l==-1)		efa->v1->tmp.l= count++;
			if (SIMA_UVSEL_CHECK(efa, tface, 1) && efa->v2->tmp.l==-1)		efa->v2->tmp.l= count++;
			if (SIMA_UVSEL_CHECK(efa, tface, 2) && efa->v3->tmp.l==-1)		efa->v3->tmp.l= count++;
			if (efa->v4)
				if (SIMA_UVSEL_CHECK(efa, tface, 3) && efa->v4->tmp.l==-1)	efa->v4->tmp.l= count++;
			change = 1;
			
			/* optional speedup */
			efa->tmp.p = tface;
		} else {
			efa->tmp.p = NULL;
		}
	}
	
	coords = MEM_callocN(sizeof(float)*count*2, "snap to adjacent coords");
	usercount = MEM_callocN(sizeof(short)*count, "snap to adjacent counts");
	
	/* add all UV coords from visible, unselected UV coords as well as counting them to average later */
	for (efa= em->faces.first; efa; efa= efa->next) {
//		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
//		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
		if ((tface=(MTFace *)efa->tmp.p)) {
			
			/* is this an unselected UV we can snap to? */
			if (efa->v1->tmp.l >= 0 && (!SIMA_UVSEL_CHECK(efa, tface, 0))) {
				coords[efa->v1->tmp.l*2] +=		tface->uv[0][0];
				coords[(efa->v1->tmp.l*2)+1] +=	tface->uv[0][1];
				usercount[efa->v1->tmp.l]++;
				change = 1;
			}
			if (efa->v2->tmp.l >= 0 && (!SIMA_UVSEL_CHECK(efa, tface, 1))) {
				coords[efa->v2->tmp.l*2] +=		tface->uv[1][0];
				coords[(efa->v2->tmp.l*2)+1] +=	tface->uv[1][1];
				usercount[efa->v2->tmp.l]++;
				change = 1;
			}
			if (efa->v3->tmp.l >= 0 && (!SIMA_UVSEL_CHECK(efa, tface, 2))) {
				coords[efa->v3->tmp.l*2] +=		tface->uv[2][0];
				coords[(efa->v3->tmp.l*2)+1] +=	tface->uv[2][1];
				usercount[efa->v3->tmp.l]++;
				change = 1;
			}
			
			if (efa->v4) {
				if (efa->v4->tmp.l >= 0 && (!SIMA_UVSEL_CHECK(efa, tface, 3))) {
					coords[efa->v4->tmp.l*2] +=		tface->uv[3][0];
					coords[(efa->v4->tmp.l*2)+1] +=	tface->uv[3][1];
					usercount[efa->v4->tmp.l]++;
					change = 1;
				}
			}
		}
	}
	
	/* no other verts selected, bail out */
	if (!change) {
		MEM_freeN(coords);
		MEM_freeN(usercount);
		return change;
	}
	
	/* copy the averaged unselected UVs back to the selected UVs */
	for (efa= em->faces.first; efa; efa= efa->next) {
//		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
//		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
		if ((tface=(MTFace *)efa->tmp.p)) {
			
			if (	SIMA_UVSEL_CHECK(efa, tface, 0) &&
					efa->v1->tmp.l >= 0 &&
					(users = usercount[efa->v1->tmp.l])
			) {
				tface->uv[0][0] = coords[efa->v1->tmp.l*2]		/ users;
				tface->uv[0][1] = coords[(efa->v1->tmp.l*2)+1]	/ users;
			}

			if (	SIMA_UVSEL_CHECK(efa, tface, 1) &&
					efa->v2->tmp.l >= 0 &&
					(users = usercount[efa->v2->tmp.l])
			) {
				tface->uv[1][0] = coords[efa->v2->tmp.l*2]		/ users;
				tface->uv[1][1] = coords[(efa->v2->tmp.l*2)+1]	/ users;
			}
			
			if (	SIMA_UVSEL_CHECK(efa, tface, 2) &&
					efa->v3->tmp.l >= 0 &&
					(users = usercount[efa->v3->tmp.l])
			) {
				tface->uv[2][0] = coords[efa->v3->tmp.l*2]		/ users;
				tface->uv[2][1] = coords[(efa->v3->tmp.l*2)+1]	/ users;
			}
			
			if (efa->v4) {
				if (	SIMA_UVSEL_CHECK(efa, tface, 3) &&
						efa->v4->tmp.l >= 0 &&
						(users = usercount[efa->v4->tmp.l])
				) {
					tface->uv[3][0] = coords[efa->v4->tmp.l*2]		/ users;
					tface->uv[3][1] = coords[(efa->v4->tmp.l*2)+1]	/ users;
				}
			}
		}
	}
	
	MEM_freeN(coords);
	MEM_freeN(usercount);
	return change;
}

void snap_coord_to_pixel(float *uvco, float w, float h)
{
	uvco[0] = ((float) ((int)((uvco[0]*w) + 0.5))) / w;  
	uvco[1] = ((float) ((int)((uvco[1]*h) + 0.5))) / h;  
}

int snap_uv_sel_to_pixels(void) /* warning, sanity checks must alredy be done */
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;
	int wi, hi;
	float w, h;
	short change = 0;

	transform_width_height_tface_uv(&wi, &hi);
	w = (float)wi;
	h = (float)hi;
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
			if (SIMA_UVSEL_CHECK(efa, tface, 0)) snap_coord_to_pixel(tface->uv[0], w, h);
			if (SIMA_UVSEL_CHECK(efa, tface, 1)) snap_coord_to_pixel(tface->uv[1], w, h);
			if (SIMA_UVSEL_CHECK(efa, tface, 2)) snap_coord_to_pixel(tface->uv[2], w, h);
			if (efa->v4)
				if (SIMA_UVSEL_CHECK(efa, tface, 3)) snap_coord_to_pixel(tface->uv[3], w, h);
			change = 1;
		}
	}
	return change;
}

void snap_uv_curs_to_pixels(void)
{
	int wi, hi;
	float w, h;

	transform_width_height_tface_uv(&wi, &hi);
	w = (float)wi;
	h = (float)hi;
	snap_coord_to_pixel(G.v2d->cursor, w, h);
}

int snap_uv_curs_to_sel(void)
{
	if( is_uv_tface_editing_allowed()==0 ) return 0;
	return cent_tface_uv(G.v2d->cursor, 0);
}

void snap_menu_sima(void)
{
	short event;
	if( is_uv_tface_editing_allowed()==0 || !G.v2d) return; /* !G.v2d should never happen */
	
	event = pupmenu("Snap %t|Selection -> Pixels%x1|Selection -> Cursor%x2|Selection -> Adjacent Unselected%x3|Cursor -> Pixel%x4|Cursor -> Selection%x5");
	switch (event) {
		case 1:
		    if (snap_uv_sel_to_pixels()) {
		    	BIF_undo_push("Snap UV Selection to Pixels");
		    	object_uvs_changed(OBACT);
		    }
		    break;
		case 2:
		    if (snap_uv_sel_to_curs()) {
		    	BIF_undo_push("Snap UV Selection to Cursor");
		    	object_uvs_changed(OBACT);
		    }
		    break;
		case 3:
		    if (snap_uv_sel_to_adj_unsel()) {
		    	BIF_undo_push("Snap UV Selection to Cursor");
		    	object_uvs_changed(OBACT);
		    }
		    break;
		case 4:
		    snap_uv_curs_to_pixels();
		    scrarea_queue_winredraw(curarea);
		    break;
		case 5:
		    if (snap_uv_curs_to_sel())
		    	allqueue(REDRAWIMAGE, 0);
		    break;
	}
}


/** This is an ugly function to set the Tface selection flags depending
  * on whether its UV coordinates are inside the normalized 
  * area with radius rad and offset offset. These coordinates must be
  * normalized to 1.0 
  * Just for readability...
  */

void sel_uvco_inside_radius(short sel, EditFace *efa, MTFace *tface, int index, float *offset, float *ell, short select_index)
{
	// normalized ellipse: ell[0] = scaleX,
	//                        [1] = scaleY

	float *uv = tface->uv[index];
	float x, y, r2;

	x = (uv[0] - offset[0]) * ell[0];
	y = (uv[1] - offset[1]) * ell[1];

	r2 = x * x + y * y;
	if (r2 < 1.0) {
		if (sel == LEFTMOUSE)	SIMA_UVSEL_SET(efa, tface, select_index);
		else					SIMA_UVSEL_UNSET(efa, tface, select_index);
	}
}

// see below:
/** gets image dimensions of the 2D view 'v' */
static void getSpaceImageDimension(SpaceImage *sima, float *xy)
{
	ImBuf *ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);

	if (ibuf) {
		xy[0] = ibuf->x * sima->zoom;
		xy[1] = ibuf->y * sima->zoom;
	} else {
		xy[0] = 256 * sima->zoom;
		xy[1] = 256 * sima->zoom;
	}
}

/** Callback function called by circle_selectCB to enable 
  * brush select in UV editor.
  */

void uvedit_selectionCB(short selecting, Object *editobj, short *mval, float rad) 
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	float offset[2];
	MTFace *tface;
	float ellipse[2]; // we need to deal with ellipses, as
	                  // non square textures require for circle
					  // selection. this ellipse is normalized; r = 1.0

	getSpaceImageDimension(curarea->spacedata.first, ellipse);
	ellipse[0] /= rad;
	ellipse[1] /= rad;

	areamouseco_to_ipoco(G.v2d, mval, &offset[0], &offset[1]);
	
	if (selecting) {
		for (efa= em->faces.first; efa; efa= efa->next) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			sel_uvco_inside_radius(selecting, efa, tface, 0, offset, ellipse, 0);
			sel_uvco_inside_radius(selecting, efa, tface, 1, offset, ellipse, 1);
			sel_uvco_inside_radius(selecting, efa, tface, 2, offset, ellipse, 2);
			if (efa->v4)
				sel_uvco_inside_radius(selecting, efa, tface, 3, offset, ellipse, 3);
		}

		if(G.f & G_DRAWFACES) { /* full redraw only if necessary */
			draw_sel_circle(0, 0, 0, 0, 0); /* signal */
			force_draw(0);
		}
		else { /* force_draw() is no good here... */
			glDrawBuffer(GL_FRONT);
			draw_uvs_sima();
			bglFlush();
			glDrawBuffer(GL_BACK);
		}
		
		
		if (selecting == LEFTMOUSE)	EM_select_flush();
		else						EM_deselect_flush();
		
		if (G.sima->lock && (G.sima->flag & SI_SYNC_UVSEL))
			force_draw_plus(SPACE_VIEW3D, 0);
	}
}


void mouseco_to_curtile(void)
{
	float fx, fy;
	short mval[2];
	
	if( is_uv_tface_editing_allowed()==0) return;

	if(G.sima->image && G.sima->image->tpageflag & IMA_TILES) {
		
		G.sima->flag |= SI_EDITTILE;
		
		while(get_mbut()&L_MOUSE) {
			
			calc_image_view(G.sima, 'f');
			
			getmouseco_areawin(mval);
			areamouseco_to_ipoco(G.v2d, mval, &fx, &fy);

			if(fx>=0.0 && fy>=0.0 && fx<1.0 && fy<1.0) {
			
				fx= (fx)*G.sima->image->xrep;
				fy= (fy)*G.sima->image->yrep;
				
				mval[0]= fx;
				mval[1]= fy;
				
				G.sima->curtile= mval[1]*G.sima->image->xrep + mval[0];
			}

			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		}
		
		G.sima->flag &= ~SI_EDITTILE;

		image_set_tile(G.sima, 2);

		allqueue(REDRAWVIEW3D, 0);
		scrarea_queue_winredraw(curarea);
	}
}

/* Could be used for other 2D views also */
void mouseco_to_cursor_sima(void)
{
	short mval[2];
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &G.v2d->cursor[0], &G.v2d->cursor[1]);
	scrarea_queue_winredraw(curarea);
}

void stitch_limit_uv_tface(void)
{
	MTFace *tf;
	int a, vtot;
	float newuv[2], limit[2];
	UvMapVert *vlist, *iterv;
	EditMesh *em = G.editMesh;
	EditVert *ev;
	EditFace *efa;
	
	struct UvVertMap *vmap;
	
	
	if(is_uv_tface_editing_allowed()==0)
		return;
	if(G.sima->flag & SI_SYNC_UVSEL) {
		error("Can't stitch when Sync Mesh Selection is enabled");
		return;
	}
	
	limit[0]= limit[1]= 20.0;
	add_numbut(0, NUM|FLO, "Limit:", 0.1, 1000.0, &limit[0], NULL);
	if (!do_clever_numbuts("Stitch UVs", 1, REDRAW))
		return;

	limit[0]= limit[1]= limit[0]/256.0;
	if(G.sima->image) {
		ImBuf *ibuf= BKE_image_get_ibuf(G.sima->image, &G.sima->iuser);

		if(ibuf && ibuf->x > 0 && ibuf->y > 0) {
			limit[1]= limit[0]/(float)ibuf->y;
			limit[0]= limit[0]/(float)ibuf->x;
		}
	}

	/*vmap= make_uv_vert_map(me->mface, tf, me->totface, me->totvert, 1, limit);*/
	EM_init_index_arrays(0, 0, 1);
	vmap= make_uv_vert_map_EM(1, 0, limit);
	if(vmap == NULL)
		return;

	for(a=0, ev= em->verts.first; ev; a++, ev= ev->next) {
		vlist= get_uv_map_vert_EM(vmap, a);

		while(vlist) {
			newuv[0]= 0; newuv[1]= 0;
			vtot= 0;

			for(iterv=vlist; iterv; iterv=iterv->next) {
				if((iterv != vlist) && iterv->separate)
					break;
				efa = EM_get_face_for_index(iterv->f);
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				
				if (tf[iterv->f].flag & TF_SEL_MASK(iterv->tfindex)) {
					newuv[0] += tf->uv[iterv->tfindex][0];
					newuv[1] += tf->uv[iterv->tfindex][1];
					vtot++;
				}
			}

			if (vtot > 1) {
				newuv[0] /= vtot; newuv[1] /= vtot;

				for(iterv=vlist; iterv; iterv=iterv->next) {
					if((iterv != vlist) && iterv->separate)
						break;
					efa = EM_get_face_for_index(iterv->f);
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					if (tf->flag & TF_SEL_MASK(iterv->tfindex)) {
						tf->uv[iterv->tfindex][0]= newuv[0];
						tf->uv[iterv->tfindex][1]= newuv[1];
					}
				}
			}
			vlist= iterv;
		}
	}

	free_uv_vert_map_EM(vmap);
	EM_free_index_arrays();
	
	if(G.sima->flag & SI_BE_SQUARE) be_square_tface_uv(em);

	BIF_undo_push("Stitch UV");

	object_uvs_changed(OBACT);
}

void select_linked_tface_uv(int mode) /* TODO */
{
	EditMesh *em= G.editMesh;
	EditFace *efa, *nearestefa=NULL;
	MTFace *tf, *nearesttf=NULL;
	UvVertMap *vmap;
	UvMapVert *vlist, *iterv, *startv;
	unsigned int *stack, stacksize= 0, nearestv;
	char *flag;
	int a, nearestuv, i, nverts, j;
	float limit[2];
	if(is_uv_tface_editing_allowed()==0)
		return;

	if(G.sima->flag & SI_SYNC_UVSEL) {
		error("Can't select linked when Sync Mesh Selection is enabled");
		return;
	}
	
	if (mode == 2) {
		nearesttf= NULL;
		nearestuv= 0;
	}
	if (mode!=2) {
		find_nearest_uv(&nearesttf, &nearestefa, &nearestv, &nearestuv);
		if(nearesttf==NULL)
			return;
	}

	get_connected_limit_tface_uv(limit);
	vmap= make_uv_vert_map_EM(1, 1, limit);
	if(vmap == NULL)
		return;

	stack= MEM_mallocN(sizeof(*stack)* BLI_countlist(&em->faces), "UvLinkStack");
	flag= MEM_callocN(sizeof(*flag)*BLI_countlist(&em->faces), "UvLinkFlag");

	if (mode == 2) {
		for (a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tf)) {
				if(tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)) {
					stack[stacksize]= a;
					stacksize++;
					flag[a]= 1;
				}
			}
		}
	} else {
		for (a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(tf == nearesttf) {
				stack[stacksize]= a;
				stacksize++;
				flag[a]= 1;
				break;
			}
		}
	}

	while(stacksize > 0) {
		stacksize--;
		a= stack[stacksize];
		
		for (j=0, efa= em->faces.first; efa; efa= efa->next, j++) {
			if (j==a) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				break;
			}
		}

		nverts= efa->v4? 4: 3;

		for(i=0; i<nverts; i++) {
			/* make_uv_vert_map_EM sets verts tmp.l to the indicies */
			vlist= get_uv_map_vert_EM(vmap, (*(&efa->v1 + i))->tmp.l);
			
			startv= vlist;

			for(iterv=vlist; iterv; iterv=iterv->next) {
				if(iterv->separate)
					startv= iterv;
				if(iterv->f == a)
					break;
			}

			for(iterv=startv; iterv; iterv=iterv->next) {
				if((startv != iterv) && (iterv->separate))
					break;
				else if(!flag[iterv->f]) {
					flag[iterv->f]= 1;
					stack[stacksize]= iterv->f;;
					stacksize++;
				}
			}
		}
	}

	if(mode==0 || mode==2) {
		for (a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(flag[a])
				tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
			else
				tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
		}
	}
	else if(mode==1) {
		for (a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			if(flag[a]) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if (efa->v4) {
					if((tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)))
						break;
				}
				else if(tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
					break;
			}
		}

		if (efa) {
			for (a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
				if(flag[a]) {
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
		else {
			for (a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
				if(flag[a]) {
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
	}
	
	MEM_freeN(stack);
	MEM_freeN(flag);
	free_uv_vert_map_EM(vmap);

	BIF_undo_push("Select linked UV");
	scrarea_queue_winredraw(curarea);
}

void unlink_selection(void)
{
	EditMesh *em= G.editMesh;
	EditFace *efa;
	MTFace *tface;

	if( is_uv_tface_editing_allowed()==0 ) return;

	if(G.sima->flag & SI_SYNC_UVSEL) {
		error("Can't select unlinked when Sync Mesh Selection is enabled");
		return;
	}
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
			if(efa->v4) {
				if(~tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4))
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
			} else {
				if(~tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3);
			}
		}
	}
	
	BIF_undo_push("Unlink UV selection");
	scrarea_queue_winredraw(curarea);
}

/*
void toggle_uv_select(int mode)
{
	switch(mode){
	case 'f':
		G.sima->flag ^= SI_SELACTFACE;
		break;
	case 's':
		G.sima->flag &= ~SI_LOCALSTICKY;
		G.sima->flag |= SI_STICKYUVS;
		break;
	case 'l':
		G.sima->flag &= ~SI_STICKYUVS;
		G.sima->flag &= ~SI_LOCALSTICKY;
		break;
	case 'o':
		 G.sima->flag &= ~SI_STICKYUVS;
		 G.sima->flag |= SI_LOCALSTICKY;
		break;
	}
	allqueue(REDRAWIMAGE, 0);
}
*/

void pin_tface_uv(int mode)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
			if(mode ==1) {
				if(SIMA_UVSEL_CHECK(efa, tface, 0)) tface->unwrap |= TF_PIN1;
				if(SIMA_UVSEL_CHECK(efa, tface, 1)) tface->unwrap |= TF_PIN2;
				if(SIMA_UVSEL_CHECK(efa, tface, 2)) tface->unwrap |= TF_PIN3;
				if(efa->v4)
					if(SIMA_UVSEL_CHECK(efa, tface, 3)) tface->unwrap |= TF_PIN4;
			}
			else if (mode ==0) {
				if(SIMA_UVSEL_CHECK(efa, tface, 0)) tface->unwrap &= ~TF_PIN1;
				if(SIMA_UVSEL_CHECK(efa, tface, 1)) tface->unwrap &= ~TF_PIN2;
				if(SIMA_UVSEL_CHECK(efa, tface, 2)) tface->unwrap &= ~TF_PIN3;
				if(efa->v4)
					if(SIMA_UVSEL_CHECK(efa, tface, 3)) tface->unwrap &= ~TF_PIN4;
			}
		}
	}
	
	BIF_undo_push("Pin UV");
	scrarea_queue_winredraw(curarea);
}

void select_pinned_tface_uv(void)
{
	EditMesh *em= G.editMesh;
	EditFace *efa;
	MTFace *tface;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tface)) {
			if (tface->unwrap & TF_PIN1) SIMA_UVSEL_SET(efa, tface, 0);
			if (tface->unwrap & TF_PIN2) SIMA_UVSEL_SET(efa, tface, 1);
			if (tface->unwrap & TF_PIN3) SIMA_UVSEL_SET(efa, tface, 2);
			if(efa->v4) {
				if (tface->unwrap & TF_PIN4) SIMA_UVSEL_SET(efa, tface, 3);
			}
			
		}
	}
	
	if (G.sima->flag & SI_SYNC_UVSEL) {
		allqueue(REDRAWVIEW3D, 0); /* mesh selection has changed */
	}
	
	BIF_undo_push("Select Pinned UVs");
	scrarea_queue_winredraw(curarea);
}

int minmax_tface_uv(float *min, float *max)
{
	EditMesh *em= G.editMesh;
	EditFace *efa;
	MTFace *tf;
	int sel;
	
	if( is_uv_tface_editing_allowed()==0 ) return 0;

	INIT_MINMAX2(min, max);

	sel= 0;
	for (efa= em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if (SIMA_FACEDRAW_CHECK(efa, tf)) {
			if (SIMA_UVSEL_CHECK(efa, tf, 0))				DO_MINMAX2(tf->uv[0], min, max);
			if (SIMA_UVSEL_CHECK(efa, tf, 1))				DO_MINMAX2(tf->uv[1], min, max);
			if (SIMA_UVSEL_CHECK(efa, tf, 2))				DO_MINMAX2(tf->uv[2], min, max);
			if (efa->v4 && (SIMA_UVSEL_CHECK(efa, tf, 3)))	DO_MINMAX2(tf->uv[3], min, max);
			sel = 1;
		}
	}
	return sel;
}

int cent_tface_uv(float *cent, int mode)
{
	float min[2], max[2];
	short change= 0;
	
	if (mode==0) {
		if (minmax_tface_uv(min, max))
			change = 1;

	} else if (mode==1) {
		EditFace *efa;
		MTFace *tf;
		INIT_MINMAX2(min, max);
		
		for (efa= G.editMesh->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&G.editMesh->fdata, efa->data, CD_MTFACE);
			if (SIMA_FACEDRAW_CHECK(efa, tf)) {
				if (SIMA_UVSEL_CHECK(efa, tf, 0))				{ DO_MINMAX2(tf->uv[0], min, max);	change= 1;}
				if (SIMA_UVSEL_CHECK(efa, tf, 1))				{ DO_MINMAX2(tf->uv[1], min, max);	change= 1;}
				if (SIMA_UVSEL_CHECK(efa, tf, 2))				{ DO_MINMAX2(tf->uv[2], min, max);	change= 1;}
				if (efa->v4 && (SIMA_UVSEL_CHECK(efa, tf, 3)))	{ DO_MINMAX2(tf->uv[3], min, max);	change= 1;}
			}
		}
	}
	
	if (change) {
		cent[0]= (min[0]+max[0])/2.0;
		cent[1]= (min[1]+max[1])/2.0;
		return 1;
	}
	return 0;
}

static void sima_show_info(int channels, int x, int y, char *cp, float *fp, int *zp, float *zpf)
{
	short ofs;
	char str[256];
	
	ofs= sprintf(str, "X: %d Y: %d ", x, y);
	if(cp)
		ofs+= sprintf(str+ofs, "| R: %d G: %d B: %d A: %d ", cp[0], cp[1], cp[2], cp[3]);
	if(fp) {
		if(channels==4)
			ofs+= sprintf(str+ofs, "| R: %.3f G: %.3f B: %.3f A: %.3f ", fp[0], fp[1], fp[2], fp[3]);
		else if(channels==1)
			ofs+= sprintf(str+ofs, "| Val: %.3f ", fp[0]);
		else if(channels==3)
			ofs+= sprintf(str+ofs, "| R: %.3f G: %.3f B: %.3f ", fp[0], fp[1], fp[2]);
	}
	if(zp)
		ofs+= sprintf(str+ofs, "| Z: %.4f ", 0.5+0.5*( ((float)*zp)/(float)0x7fffffff));
	if(zpf)
		ofs+= sprintf(str+ofs, "| Z: %.3f ", *zpf);
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	glColor4f(.0,.0,.0,.25);
	glRectf(0.0, 0.0, curarea->winx, 30.0);
	glDisable(GL_BLEND);
	
	glColor3ub(255, 255, 255);
	glRasterPos2i(10, 10);
	
	BMF_DrawString(G.fonts, str);

}

void sima_sample_color(void)
{
	ImBuf *ibuf= BKE_image_get_ibuf(G.sima->image, &G.sima->iuser);
	float fx, fy;
	short mval[2], mvalo[2], firsttime=1;
	
	if(ibuf==NULL)
		return;
	
	calc_image_view(G.sima, 'f');
	getmouseco_areawin(mvalo);
	
	while(get_mbut() & L_MOUSE) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || firsttime) {
			firsttime= 0;
			areamouseco_to_ipoco(G.v2d, mval, &fx, &fy);
			
			if(fx>=0.0 && fy>=0.0 && fx<1.0 && fy<1.0) {
				float *fp= NULL, *zpf= NULL;
				int *zp= NULL;
				char *cp= NULL;
				
				int x= (int) (fx*ibuf->x);
				int y= (int) (fy*ibuf->y);
				
				if(x>=ibuf->x) x= ibuf->x-1;
				if(y>=ibuf->y) y= ibuf->y-1;
				
				if(ibuf->rect)
					cp= (char *)(ibuf->rect + y*ibuf->x + x);
				if(ibuf->zbuf)
					zp= ibuf->zbuf + y*ibuf->x + x;
				if(ibuf->zbuf_float)
					zpf= ibuf->zbuf_float + y*ibuf->x + x;
				if(ibuf->rect_float)
					fp= (ibuf->rect_float + (ibuf->channels)*(y*ibuf->x + x));
					
				if(G.sima->cumap) {
					float vec[3];
					if(fp==NULL) {
						fp= vec;
						vec[0]= (float)cp[0]/255.0f;
						vec[1]= (float)cp[1]/255.0f;
						vec[2]= (float)cp[2]/255.0f;
					}
					
					if(ibuf->channels==4) {
						if(G.qual & LR_CTRLKEY) {
							curvemapping_set_black_white(G.sima->cumap, NULL, fp);
							curvemapping_do_ibuf(G.sima->cumap, ibuf);
						}
						else if(G.qual & LR_SHIFTKEY) {
							curvemapping_set_black_white(G.sima->cumap, fp, NULL);
							curvemapping_do_ibuf(G.sima->cumap, ibuf);
						}
					}
				}
				
				scrarea_do_windraw(curarea);
				myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);
				glLoadIdentity();
				sima_show_info(ibuf->channels, x, y, cp, fp, zp, zpf);
				screen_swapbuffers();
			}
			
		}
		BIF_wait_for_statechange();
	}
	
	scrarea_queue_winredraw(curarea);
}

/* Image functions */

static void load_image_filesel(char *str)	/* called from fileselect */
{
	Image *ima= NULL;

	ima= BKE_add_image_file(str);
	if(ima) {
		BKE_image_signal(ima, &G.sima->iuser, IMA_SIGNAL_RELOAD);
		image_changed(G.sima, ima);
	}
	BIF_undo_push("Load image UV");
	allqueue(REDRAWIMAGE, 0);
}

static void replace_image_filesel(char *str)		/* called from fileselect */
{
	if (!G.sima->image)
		return;
	
	strncpy(G.sima->image->name, str, sizeof(G.sima->image->name)-1); /* we cant do much if the str is longer then 240 :/ */
	BKE_image_signal(G.sima->image, &G.sima->iuser, IMA_SIGNAL_RELOAD);
	BIF_undo_push("Replace image UV");
	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWVIEW3D, 0);
}


static void save_image_doit(char *name)
{
	Image *ima= G.sima->image;
	ImBuf *ibuf= BKE_image_get_ibuf(ima, &G.sima->iuser);
	int len;
	char str[FILE_MAXDIR+FILE_MAXFILE];

	if (ibuf) {
		BLI_strncpy(str, name, sizeof(str));

		BLI_convertstringcode(str, G.sce, G.scene->r.cfra);
		
		if(G.scene->r.scemode & R_EXTENSION) 
			BKE_add_image_extension(str, G.sima->imtypenr);
		
		if (saveover(str)) {
			
			/* enforce user setting for RGB or RGBA, but skip BW */
			if(G.scene->r.planes==32)
				ibuf->depth= 32;
			else if(G.scene->r.planes==24)
				ibuf->depth= 24;
			
			waitcursor(1);
			if(G.sima->imtypenr==R_MULTILAYER) {
				RenderResult *rr= BKE_image_get_renderresult(ima);
				if(rr) {
					RE_WriteRenderResult(rr, str, G.scene->r.quality);
					
					BLI_strncpy(ima->name, name, sizeof(ima->name));
					BLI_strncpy(ibuf->name, str, sizeof(ibuf->name));
					
					/* should be function? nevertheless, saving only happens here */
					for(ibuf= ima->ibufs.first; ibuf; ibuf= ibuf->next)
						ibuf->userflags &= ~IB_BITMAPDIRTY;
					
				}
				else error("Did not write, no Multilayer Image");
			}
			else if (BKE_write_ibuf(ibuf, str, G.sima->imtypenr, G.scene->r.subimtype, G.scene->r.quality)) {
				BLI_strncpy(ima->name, name, sizeof(ima->name));
				BLI_strncpy(ibuf->name, str, sizeof(ibuf->name));
				
				ibuf->userflags &= ~IB_BITMAPDIRTY;
				
				/* change type? */
				if( ELEM(ima->source, IMA_SRC_GENERATED, IMA_SRC_VIEWER)) {
					ima->source= IMA_SRC_FILE;
					ima->type= IMA_TYPE_IMAGE;
				}
				if(ima->type==IMA_TYPE_R_RESULT)
					ima->type= IMA_TYPE_IMAGE;
				
				/* name image as how we saved it */
				len= strlen(str);
				while (len > 0 && str[len - 1] != '/' && str[len - 1] != '\\') len--;
				rename_id(&ima->id, str+len);
			} 
			else {
				error("Couldn't write image: %s", str);
			}

			allqueue(REDRAWHEADERS, 0);
			allqueue(REDRAWBUTSSHADING, 0);

			waitcursor(0);
		}
	}
}

void open_image_sima(short imageselect)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];

	if(G.sima->image)
		strcpy(name, G.sima->image->name);
	else
		strcpy(name, U.textudir);

	if(imageselect)
		activate_imageselect(FILE_SPECIAL, "Open Image", name, load_image_filesel);
	else
		activate_fileselect(FILE_SPECIAL, "Open Image", name, load_image_filesel);
}

void replace_image_sima(short imageselect)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];

	if(G.sima->image)
		strcpy(name, G.sima->image->name);
	else
		strcpy(name, U.textudir);
	
	if(imageselect)
		activate_imageselect(FILE_SPECIAL, "Replace Image", name, replace_image_filesel);
	else
		activate_fileselect(FILE_SPECIAL, "Replace Image", name, replace_image_filesel);
}


static char *filesel_imagetype_string(Image *ima)
{
	char *strp, *str= MEM_callocN(14*32, "menu for filesel");
	
	strp= str;
	str += sprintf(str, "Save Image as: %%t|");
	str += sprintf(str, "Targa %%x%d|", R_TARGA);
	str += sprintf(str, "Targa Raw %%x%d|", R_RAWTGA);
	str += sprintf(str, "PNG %%x%d|", R_PNG);
	str += sprintf(str, "BMP %%x%d|", R_BMP);
	str += sprintf(str, "Jpeg %%x%d|", R_JPEG90);
	str += sprintf(str, "Iris %%x%d|", R_IRIS);
	if(G.have_libtiff)
		str += sprintf(str, "Tiff %%x%d|", R_TIFF);
	str += sprintf(str, "Radiance HDR %%x%d|", R_RADHDR);
	str += sprintf(str, "Cineon %%x%d|", R_CINEON);
	str += sprintf(str, "DPX %%x%d|", R_DPX);
#ifdef WITH_OPENEXR
	str += sprintf(str, "OpenEXR %%x%d|", R_OPENEXR);
	/* saving sequences of multilayer won't work, they copy buffers  */
	if(ima->source==IMA_SRC_SEQUENCE && ima->type==IMA_TYPE_MULTILAYER);
	else str += sprintf(str, "MultiLayer %%x%d|", R_MULTILAYER);
#endif	
	return strp;
}

/* always opens fileselect */
void save_as_image_sima(void)
{
	Image *ima = G.sima->image;
	ImBuf *ibuf= BKE_image_get_ibuf(ima, &G.sima->iuser);
	char name[FILE_MAXDIR+FILE_MAXFILE];

	if (ima) {
		strcpy(name, ima->name);

		if (ibuf) {
			char *strp;
			
			strp= filesel_imagetype_string(ima);
			
			/* cant save multilayer sequence, ima->rr isn't valid for a specific frame */
			if(ima->rr && !(ima->source==IMA_SRC_SEQUENCE && ima->type==IMA_TYPE_MULTILAYER))
				G.sima->imtypenr= R_MULTILAYER;
			else if(ima->type==IMA_TYPE_R_RESULT)
				G.sima->imtypenr= G.scene->r.imtype;
			else G.sima->imtypenr= BKE_ftype_to_imtype(ibuf->ftype);
			
			activate_fileselect_menu(FILE_SPECIAL, "Save Image", name, strp, &G.sima->imtypenr, save_image_doit);
		}
	}
}

/* if exists, saves over without fileselect */
void save_image_sima(void)
{
	Image *ima = G.sima->image;
	ImBuf *ibuf= BKE_image_get_ibuf(ima, &G.sima->iuser);
	char name[FILE_MAXDIR+FILE_MAXFILE];

	if (ima) {
		strcpy(name, ima->name);

		if (ibuf) {
			if (BLI_exists(ibuf->name)) {
				if(BKE_image_get_renderresult(ima)) 
					G.sima->imtypenr= R_MULTILAYER;
				else 
					G.sima->imtypenr= BKE_ftype_to_imtype(ibuf->ftype);
				
				save_image_doit(ibuf->name);
			}
			else
				save_as_image_sima();
		}
	}
}

void save_image_sequence_sima(void)
{
	ImBuf *ibuf;
	int tot= 0;
	char di[FILE_MAX], fi[FILE_MAX];
	
	if(G.sima->image==NULL)
		return;
	if(G.sima->image->source!=IMA_SRC_SEQUENCE)
		return;
	if(G.sima->image->type==IMA_TYPE_MULTILAYER) {
		error("Cannot save Multilayer Sequences");
		return;
	}
	
	/* get total */
	for(ibuf= G.sima->image->ibufs.first; ibuf; ibuf= ibuf->next) 
		if(ibuf->userflags & IB_BITMAPDIRTY)
			tot++;
	
	if(tot==0) {
		notice("No Images have been changed");
		return;
	}
	/* get a filename for menu */
	for(ibuf= G.sima->image->ibufs.first; ibuf; ibuf= ibuf->next) 
		if(ibuf->userflags & IB_BITMAPDIRTY)
			break;
	
	BLI_strncpy(di, ibuf->name, FILE_MAX);
	BLI_splitdirstring(di, fi);
	
	sprintf(fi, "%d Image(s) will be saved in %s", tot, di);
	if(okee(fi)) {
		
		for(ibuf= G.sima->image->ibufs.first; ibuf; ibuf= ibuf->next) {
			if(ibuf->userflags & IB_BITMAPDIRTY) {
				char name[FILE_MAX];
				BLI_strncpy(name, ibuf->name, sizeof(name));
				
				BLI_convertstringcode(name, G.sce, 0);

				if(0 == IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat)) {
					error("Could not write image", name);
					break;
				}
				printf("Saved: %s\n", ibuf->name);
				ibuf->userflags &= ~IB_BITMAPDIRTY;
			}
		}
	}
}

void reload_image_sima(void)
{
	if (G.sima ) {
		BKE_image_signal(G.sima->image, &G.sima->iuser, IMA_SIGNAL_RELOAD);
		/* image_changed(G.sima, 0); - do we really need this? */
	}

	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWVIEW3D, 0);
	BIF_preview_changed(ID_TE);
}

void new_image_sima(void)
{
	static int width= 256, height= 256;
	static short uvtestgrid= 0;
	static float color[] = {0, 0, 0, 1};
	char name[22];
	Image *ima;
	
	strcpy(name, "Untitled");

	add_numbut(0, TEX, "Name:", 0, 21, name, NULL);
	add_numbut(1, NUM|INT, "Width:", 1, 5000, &width, NULL);
	add_numbut(2, NUM|INT, "Height:", 1, 5000, &height, NULL);
	add_numbut(3, COL, "", 0, 0, &color, NULL);
	add_numbut(4, NUM|FLO, "Alpha:", 0.0, 1.0, &color[3], NULL);
	add_numbut(5, TOG|SHO, "UV Test Grid", 0, 0, &uvtestgrid, NULL);
	if (!do_clever_numbuts("New Image", 6, REDRAW))
 		return;

	ima = BKE_add_image_size(width, height, name, uvtestgrid, color);
	image_changed(G.sima, ima);
	BKE_image_signal(G.sima->image, &G.sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
	BIF_undo_push("Add image");

	allqueue(REDRAWIMAGE, 0);
	allqueue(REDRAWVIEW3D, 0);
}

void pack_image_sima()
{
	Image *ima = G.sima->image;

	if (ima) {
		if(ima->source!=IMA_SRC_SEQUENCE && ima->source!=IMA_SRC_MOVIE) {
			if (ima->packedfile) {
				if (G.fileflags & G_AUTOPACK)
					if (okee("Disable AutoPack?"))
						G.fileflags &= ~G_AUTOPACK;
				
				if ((G.fileflags & G_AUTOPACK) == 0) {
					unpackImage(ima, PF_ASK);
					BIF_undo_push("Unpack image");
				}
			}
			else {
				ImBuf *ibuf= BKE_image_get_ibuf(ima, &G.sima->iuser);
				if (ibuf && (ibuf->userflags & IB_BITMAPDIRTY)) {
					if(okee("Can't pack painted image. Use Repack as PNG?"))
						BKE_image_memorypack(ima);
				}
				else {
					ima->packedfile = newPackedFile(ima->name);
					BIF_undo_push("Pack image");
				}
			}

			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWHEADERS, 0);
		}
	}
}



/* goes over all ImageUsers, and sets frame numbers if auto-refresh is set */
void BIF_image_update_frame(void)
{
	Tex *tex;
	
	/* texture users */
	for(tex= G.main->tex.first; tex; tex= tex->id.next) {
		if(tex->type==TEX_IMAGE && tex->ima)
			if(ELEM(tex->ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE))
				if(tex->iuser.flag & IMA_ANIM_ALWAYS)
					BKE_image_user_calc_imanr(&tex->iuser, G.scene->r.cfra, 0);
		
	}
	/* image window, compo node users */
	if(G.curscreen) {
		ScrArea *sa;
		for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
			if(sa->spacetype==SPACE_VIEW3D) {
				View3D *v3d= sa->spacedata.first;
				if(v3d->bgpic)
					if(v3d->bgpic->iuser.flag & IMA_ANIM_ALWAYS)
						BKE_image_user_calc_imanr(&v3d->bgpic->iuser, G.scene->r.cfra, 0);
			}
			else if(sa->spacetype==SPACE_IMAGE) {
				SpaceImage *sima= sa->spacedata.first;
				if(sima->iuser.flag & IMA_ANIM_ALWAYS)
					BKE_image_user_calc_imanr(&sima->iuser, G.scene->r.cfra, 0);
			}
			else if(sa->spacetype==SPACE_NODE) {
				SpaceNode *snode= sa->spacedata.first;
				if((snode->treetype==NTREE_COMPOSIT) && (snode->nodetree)) {
					bNode *node;
					for(node= snode->nodetree->nodes.first; node; node= node->next) {
						if(node->id && node->type==CMP_NODE_IMAGE) {
							Image *ima= (Image *)node->id;
							ImageUser *iuser= node->storage;
							if(ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE))
								if(iuser->flag & IMA_ANIM_ALWAYS)
									BKE_image_user_calc_imanr(iuser, G.scene->r.cfra, 0);
						}
					}
				}
			}
		}
	}
}

void aspect_sima(SpaceImage *sima, float *x, float *y)
{
	*x = *y = 1.0;
	
	if(		(sima->image == 0) ||
			(sima->image->type == IMA_TYPE_R_RESULT) ||
			(sima->image->type == IMA_TYPE_COMPOSITE) ||
			(sima->image->tpageflag & IMA_TILES) ||
			(sima->image->aspx==0.0 || sima->image->aspy==0.0)
	) {
		return;
	}
	
	/* x is always 1 */
	*y = sima->image->aspy / sima->image->aspx;
}
