/**
 * $Id: 
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

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_ika_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_radio_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_packedFile_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_packedFile.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BSE_filesel.h"

#include "BIF_gl.h"
#include "BIF_editarmature.h"	
#include "BIF_editconstraint.h"	
#include "BIF_editdeform.h"
#include "BIF_editfont.h"
#include "BIF_editmesh.h"
#include "BIF_editsound.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_scrarea.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_previewrender.h"
#include "BIF_butspace.h"

#include "mydevice.h"
#include "blendef.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_ika.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_sound.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BDR_drawobject.h"
#include "BDR_editcurve.h"
#include "BDR_editface.h"
#include "BDR_editobject.h"
#include "BDR_vpaint.h"

#include "BSE_drawview.h"
#include "BSE_editipo.h"
#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"
#include "BSE_buttons.h"
#include "BSE_seqaudio.h"

#include "LOD_DependKludge.h"
#include "LOD_decimation.h"

#include "butspace.h" // own module

int decim_faces=0;
short degr= 90, step= 9, turn= 1, editbutflag= 1;
float doublimit= 0.001;
float editbutvweight=1;
float extr_offs= 1.0, editbutweight=1.0, editbutsize=0.1, cumapsize= 1.0;




/* *************************** MESH DECIMATE ******************************** */

/* should be removed from this c file (ton) */

static int decimate_count_tria(Object *ob)
{
	int tottria;
	MFace *mface;
	Mesh *me;
	int a;
	
	me= ob->data;
	
	/* count number of trias, since decimator doesnt allow quads */
	tottria= 0;
	mface= me->mface;
	for(a=0; a<me->totface; a++, mface++) {
		if(mface->v4) tottria++;
		if(mface->v3) tottria++;		
	}
	
	return tottria;
}

static void decimate_faces(void)
{
	Object *ob;
	Mesh *me;
	MVert *mvert;
	MFace *mface;
	LOD_Decimation_Info lod; 
	float *vb=NULL;
	float *vnb=NULL;
	int *tib=NULL;
	int a, tottria;
	
	/* we assume the active object being decimated */
	ob= OBACT;
	if(ob==NULL || ob->type!=OB_MESH) return;
	me= ob->data;

	/* add warning for vertex col and tfaces */
	if(me->tface || me->mcol) {
		if(okee("This will remove UV coordinates and vertexcolors")==0) return;
		if(me->tface) MEM_freeN(me->tface);
		if(me->mcol) MEM_freeN(me->mcol);
		me->tface= NULL;
		me->mcol= NULL;
	}
	
	/* count number of trias, since decimator doesnt allow quads */
	tottria= decimate_count_tria(ob);

	if(tottria<3) {
		error("You must have more than 3 input faces selected.");
		return;
	}
	/* allocate and init */
	lod.vertex_buffer= MEM_mallocN(3*sizeof(float)*me->totvert, "vertices");
	lod.vertex_normal_buffer= MEM_mallocN(3*sizeof(float)*me->totvert, "normals");
	lod.triangle_index_buffer= MEM_mallocN(3*sizeof(int)*tottria, "trias");
	lod.vertex_num= me->totvert;
	lod.face_num= tottria;
	
	/* fill vertex buffer */
	vb= lod.vertex_buffer;
	vnb= lod.vertex_normal_buffer;
	mvert= me->mvert;
	for(a=0; a<me->totvert; a++, mvert++, vb+=3, vnb+=3) {
		VECCOPY(vb, mvert->co);
		VECCOPY(vnb, mvert->no);
		Normalise(vnb);
	}
	
	/* fill index buffer */
	mface= me->mface;
	tib= lod.triangle_index_buffer;
	for(a=0; a<me->totface; a++, mface++) {
		if(mface->v4) {
			tib[0]= mface->v1;
			tib[1]= mface->v3;
			tib[2]= mface->v4;
			tib+= 3;
		}
		if(mface->v3) {
			tib[0]= mface->v1;
			tib[1]= mface->v2;
			tib[2]= mface->v3;
			tib+= 3;
		}
	}

	if(LOD_LoadMesh(&lod) ) {
		if( LOD_PreprocessMesh(&lod) ) {
			DispList *dl;
			DispListMesh *dlm;
			MFaceInt *mfaceint;
			
			/* we assume the decim_faces tells how much to reduce */
			
			while(lod.face_num > decim_faces) {
				if( LOD_CollapseEdge(&lod)==0) break;
			}

			/* ok, put back the stuff in a displist */
			freedisplist(&(ob->disp));
			dl= MEM_callocN(sizeof(DispList), "disp");
			BLI_addtail(&ob->disp, dl);
			dl->type= DL_MESH;
			dlm=dl->mesh= MEM_callocN(sizeof(DispListMesh), "dispmesh");
			dlm->mvert= MEM_callocN(lod.vertex_num*sizeof(MVert), "mvert");
			dlm->mface= MEM_callocN(lod.face_num*sizeof(MFaceInt), "mface");
			dlm->totvert= lod.vertex_num;
			dlm->totface= lod.face_num;
			
			mvert= dlm->mvert;
			vb= lod.vertex_buffer;
			for(a=0; a<lod.vertex_num; a++, vb+=3, mvert++) {
				VECCOPY(mvert->co, vb);
			}
			
			mfaceint= dlm->mface;
			tib= lod.triangle_index_buffer;
			for(a=0; a<lod.face_num; a++, mfaceint++, tib+=3) {
				mfaceint->v1= tib[0];
				mfaceint->v2= tib[1];
				mfaceint->v3= tib[2];
			}
		}
		else error("No memory");

		LOD_FreeDecimationData(&lod);
	}
	else error("No manifold Mesh");
	
	MEM_freeN(lod.vertex_buffer);
	MEM_freeN(lod.vertex_normal_buffer);
	MEM_freeN(lod.triangle_index_buffer);
	
	allqueue(REDRAWVIEW3D, 0);
}



static void decimate_cancel(void)
{
	Object *ob;
	
	ob= OBACT;
	if(ob) {
		freedisplist(&ob->disp);
		makeDispList(ob);
	}
	allqueue(REDRAWVIEW3D, 0);
}

static void decimate_apply(void)
{
	Object *ob;
	DispList *dl;
	DispListMesh *dlm;
	Mesh *me;
	MFace *mface;
	MFaceInt *mfaceint;
	int a;
	
	if(G.obedit) return;
	
	ob= OBACT;
	if(ob) {
		dl= ob->disp.first;
		if(dl && dl->mesh) {
			dlm= dl->mesh;
			me= ob->data;
			
			// vertices 
			if(me->mvert) MEM_freeN(me->mvert);
			me->mvert= dlm->mvert;
			dlm->mvert= NULL;
			me->totvert= dlm->totvert;
			
			// faces
			if(me->mface) MEM_freeN(me->mface);
			me->mface= MEM_callocN(dlm->totface*sizeof(MFace), "mface");
			me->totface= dlm->totface;
			mface= me->mface;
			mfaceint= dlm->mface;
			for(a=0; a<me->totface; a++, mface++, mfaceint++) {
				mface->v1= mfaceint->v1;
				mface->v2= mfaceint->v2;
				mface->v3= mfaceint->v3;
				test_index_mface(mface, 3);
			}
			
			freedisplist(&ob->disp);
			
			G.obedit= ob;
			make_editMesh();
			load_editMesh();
			free_editMesh();
			G.obedit= NULL;
			tex_space_mesh(me);
		}
		else error("Not a decimated Mesh");
	}
}

/* *************************** MESH  ******************************** */


static void editing_panel_mesh_type(Object *ob, Mesh *me)
{
	uiBlock *block;
	float val;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_type", UI_EMBOSS, UI_HELV, curarea->win);
	if( uiNewPanel(curarea, block, "Mesh", "Editing", 320, 0, 318, 204)==0) return;

	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|5, REDRAWVIEW3D, "Auto Smooth",10,178,130,17, &me->flag, 0, 0, 0, 0, "Treats all faces with angles less than Degr: as 'smooth' during render");
	uiDefButS(block, NUM, B_DIFF, "Degr:",					10,156,130,17, &me->smoothresh, 1, 80, 0, 0, "Defines maximum angle between face normals that 'Auto Smooth' will operate on");
	
	uiBlockBeginAlign(block);
	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|7, B_MAKEDISP, "SubSurf",		10,124,130,17, &me->flag, 0, 0, 0, 0, "Treats the active object as a Catmull-Clark Subdivision Surface");
	uiBlockSetCol(block, TH_AUTO);
	uiDefButS(block, NUM, B_MAKEDISP, "Subdiv:",			10,104,100,18, &me->subdiv, 0, 6, 0, 0, "Defines the level of subdivision to display in real time interactively");
	uiDefButS(block, NUM, B_MAKEDISP, "",					110, 104, 30, 18, &me->subdivr, 0, 6, 0, 0, "Defines the level of subdivision to apply during rendering");
	uiDefButS(block, TOG|BIT|8, B_MAKEDISP, "Optimal",	10,84,130,17, &me->flag, 0, 0, 0, 0, "Only draws optimal wireframe");
	
	
	uiBlockBeginAlign(block);
	if(me->msticky) val= 1.0; else val= 0.0;
	uiDefBut(block, LABEL, 0, "Sticky", 10,55,70,20, 0, val, 0, 0, 0, "");
	if(me->msticky==0) {
		uiDefBut(block, BUT, B_MAKESTICKY, "Make",	80,55,83,19, 0, 0, 0, 0, 0, "Creates Sticky coordinates for the active object from the current camera view background picture");
	}
	else uiDefBut(block, BUT, B_DELSTICKY, "Delete", 80,55,83,19, 0, 0, 0, 0, 0, "Deletes Sticky texture coordinates");

	if(me->mcol) val= 1.0; else val= 0.0;
	uiDefBut(block, LABEL, 0, "VertCol", 10,35,70,20, 0, val, 0, 0, 0, "");
	if(me->mcol==0) {
		uiDefBut(block, BUT, B_MAKEVERTCOL, "Make",	80,35,84,19, 0, 0, 0, 0, 0, "Enables vertex colour painting on active object");
	}
	else uiDefBut(block, BUT, B_DELVERTCOL, "Delete", 80,35,84,19, 0, 0, 0, 0, 0, "Deletes vertex colours on active object");

	if(me->tface) val= 1.0; else val= 0.0;
	uiDefBut(block, LABEL, 0, "TexFace", 10,15,70,20, 0, val, 0, 0, 0, "");
	if(me->tface==0) {
		uiDefBut(block, BUT, B_MAKE_TFACES, "Make",	80,15,84,19, 0, 0, 0, 0, 0, "Enables the active object's faces for UV coordinate mapping");
	}
	else uiDefBut(block, BUT, B_DEL_TFACES, "Delete", 80,15,84,19, 0, 0, 0, 0, 0, "Deletes UV coordinates for active object's faces");
	uiBlockEndAlign(block);
	

	/* decimator */
	if(G.obedit==NULL) {
		int tottria= decimate_count_tria(ob);
		DispList *dl;
	
		// wacko, wait for new displist system (ton)
		if( (dl=ob->disp.first) && dl->mesh);
		else decim_faces= tottria;
	
		uiBlockBeginAlign(block);
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButI(block, NUMSLI,B_DECIM_FACES, "Decimator: ",	173,176,233,19, &decim_faces, 4.0, tottria, 10, 10, "Defines the number of triangular faces to decimate the active Mesh object to");
		uiBlockSetCol(block, TH_AUTO);
		uiDefBut(block, BUT,B_DECIM_APPLY, "Apply",		173,156,115,18, 0, 0, 0, 0, 0, "Applies the decimation to the active Mesh object");
		uiDefBut(block, BUT,B_DECIM_CANCEL, "Cancel",	290,156,116,18, 0, 0, 0, 0, 0, "Restores the Mesh to its original number of faces");
		uiBlockEndAlign(block);
	}

	
	uiDefIDPoinBut(block, test_meshpoin_but, 0, "TexMesh: ",	174,120,234,19, &me->texcomesh, "Enter the name of a Meshblock");

	if(me->key) {
		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_DIFF, "Slurph:",				174,95,100,19, &(me->key->slurph), -500.0, 500.0, 0, 0, "");
		uiDefButS(block, TOG, B_RELKEY, "Relative Keys",		174,75,100,19, &me->key->type, 0, 0, 0, 0, "");
	}

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_SLOWERDRAW,"SlowerDraw",			174,35,100,19, 0, 0, 0, 0, 0, "Displays the active object with all possible edges shown");
	uiDefBut(block, BUT, B_FASTERDRAW,"FasterDraw",			175,15,100,19, 0, 0, 0, 0, 0, "Displays the active object faster by omitting some edges when drawing");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_DOCENTRE, "Centre",					275, 95, 133, 19, 0, 0, 0, 0, 0, "Shifts object data to be centered about object's origin");
	uiDefBut(block, BUT,B_DOCENTRENEW, "Centre New",			275, 75, 133, 19, 0, 0, 0, 0, 0, "Shifts object's origin to center of object data");
	uiDefBut(block, BUT,B_DOCENTRECURSOR, "Centre Cursor",		275, 55, 133, 19, 0, 0, 0, 0, 0, "Shifts object's origin to cursor location");

	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|2, REDRAWVIEW3D, "Double Sided",	275,35,133,19, &me->flag, 0, 0, 0, 0, "Toggles selected faces as doublesided or single-sided");
	uiDefButS(block, TOG|BIT|1, REDRAWVIEW3D, "No V.Normal Flip",275,15,133,19, &me->flag, 0, 0, 0, 0, "Disables flipping of vertexnormals during render");
	uiBlockEndAlign(block);

}



/* *************************** FONT ******************************** */

static short give_vfontnr(VFont *vfont)
{
	VFont *vf;
	short nr= 1;

	vf= G.main->vfont.first;
	while(vf) {
		if(vf==vfont) return nr;
		nr++;
		vf= vf->id.next;
	}
	return -1;
}

static VFont *give_vfontpointer(int nr)	/* nr= button */
{
	VFont *vf;
	short tel= 1;

	vf= G.main->vfont.first;
	while(vf) {
		if(tel==nr) return vf;
		tel++;
		vf= vf->id.next;
	}
	return G.main->vfont.first;
}

static VFont *exist_vfont(char *str)
{
	VFont *vf;

	vf= G.main->vfont.first;
	while(vf) {
		if(strcmp(vf->name, str)==0) return vf;
		vf= vf->id.next;
	}
	return 0;
}

static char *give_vfontbutstr(void)
{
	VFont *vf;
	int len= 0;
	char *str, di[FILE_MAXDIR], fi[FILE_MAXFILE];

	vf= G.main->vfont.first;
	while(vf) {
		strcpy(di, vf->name);
		BLI_splitdirstring(di, fi);
		len+= strlen(fi)+4;
		vf= vf->id.next;
	}
	
	str= MEM_callocN(len+21, "vfontbutstr");
	strcpy(str, "FONTS %t");
	vf= G.main->vfont.first;
	while(vf) {
		
		if(vf->id.us==0) strcat(str, "|0 ");
		else strcat(str, "|   ");
		
		strcpy(di, vf->name);
		BLI_splitdirstring(di, fi);
		
		strcat(str, fi);
		vf= vf->id.next;
	}
	return str;
}

static void load_buts_vfont(char *name)
{
	VFont *vf;
	Curve *cu;
	
	if(OBACT && OBACT->type==OB_FONT) cu= OBACT->data;
	else return;
	
	vf= exist_vfont(name);
	if(vf==0) {
		vf= load_vfont(name);
		if(vf==0) return;
	}
	else id_us_plus((ID *)vf);
	
	if(cu->vfont) cu->vfont->id.us--;
	cu->vfont= vf;
	
	text_to_curve(OBACT, 0);
	makeDispList(OBACT);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}

void do_fontbuts(unsigned short event)
{
	Curve *cu;
	VFont *vf;
	Object *ob;
	ScrArea *sa;
	char str[80];
	
	ob= OBACT;
	
	switch(event) {
	case B_MAKEFONT:
		text_to_curve(ob, 0);
		makeDispList(ob);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_TOUPPER:
		to_upper();
		break;
	case B_LOADFONT:
		vf= give_vfontpointer(G.buts->texnr);
		if(vf && vf->id.prev!=vf->id.next) strcpy(str, vf->name);
		else strcpy(str, U.fontdir);
		
		sa= closest_bigger_area();
		areawinset(sa->win);

		activate_fileselect(FILE_SPECIAL, "SELECT FONT", str, load_buts_vfont);

		break;
	case B_PACKFONT:
		if (ob) {
			cu= ob->data;
			if(cu && cu->vfont) {
				if (cu->vfont->packedfile) {
					if (G.fileflags & G_AUTOPACK) {
						if (okee("Disable AutoPack ?")) {
							G.fileflags &= ~G_AUTOPACK;
						}
					}
					
					if ((G.fileflags & G_AUTOPACK) == 0) {
						if (unpackVFont(cu->vfont, PF_ASK) == RET_OK) {
							text_to_curve(ob, 0);
							makeDispList(ob);
							allqueue(REDRAWVIEW3D, 0);
						}
					}
				} else {
					cu->vfont->packedfile = newPackedFile(cu->vfont->name);
				}
			}
		}
		allqueue(REDRAWHEADERS, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;

	case B_SETFONT:
		if(ob) {
			cu= ob->data;

			vf= give_vfontpointer(G.buts->texnr);
			if(vf) {
				id_us_plus((ID *)vf);
				cu->vfont->id.us--;
				cu->vfont= vf;
				text_to_curve(ob, 0);
				makeDispList(ob);
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWBUTSEDIT, 0);
			}
		}	
		break;
	case B_TEXTONCURVE:
		if(ob) {
			cu= ob->data;
			if(cu->textoncurve && cu->textoncurve->type!=OB_CURVE) {
				error("Only Curve Objects");
				cu->textoncurve= 0;
				allqueue(REDRAWBUTSEDIT, 0);
			}
			text_to_curve(ob, 0);
			makeDispList(ob);
		}
	}
}



static void editing_panel_font_type(Object *ob, Curve *cu)
{
	uiBlock *block;
	char *strp;
	static int packdummy = 0;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_font_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Font", "Editing", 640, 0, 318, 204)==0) return;
	
	uiDefButS(block, ROW,B_MAKEFONT, "Left",		484,139,53,18, &cu->spacemode, 0.0,0.0, 0, 0, "");
	uiDefButS(block, ROW,B_MAKEFONT, "Middle",	604,139,61,18, &cu->spacemode, 0.0,1.0, 0, 0, "");
	uiDefButS(block, ROW,B_MAKEFONT, "Right",		540,139,62,18, &cu->spacemode, 0.0,2.0, 0, 0, "");
	uiDefButS(block, ROW,B_MAKEFONT, "Flush",		665,139,61,18, &cu->spacemode, 0.0,3.0, 0, 0, "");

	uiDefIDPoinBut(block, test_obpoin_but, B_TEXTONCURVE, "TextOnCurve:",	484,115,243,19, &cu->textoncurve, "");
	uiDefBut(block, TEX,REDRAWVIEW3D, "Ob Family:",	484,85,243,19, cu->family, 0.0, 20.0, 0, 0, "");

	uiDefButF(block, NUM,B_MAKEFONT, "Size:",		482,56,121,19, &cu->fsize, 0.1,10.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "Linedist:",	605,56,121,19, &cu->linedist, 0.0,10.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "Spacing:",	482,34,121,19, &cu->spacing, 0.0,10.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "Y offset:",	605,34,121,19, &cu->yof, -50.0,50.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "Shear:",	482,12,121,19, &cu->shear, -1.0,1.0, 10, 0, "");
	uiDefButF(block, NUM,B_MAKEFONT, "X offset:",	605,12,121,19, &cu->xof, -50.0,50.0, 10, 0, "");

	uiDefBut(block, BUT, B_TOUPPER, "ToUpper",		623,163,103,23, 0, 0, 0, 0, 0, "");
	
	G.buts->texnr= give_vfontnr(cu->vfont);
	
	strp= give_vfontbutstr();
	
	uiDefButS(block, MENU, B_SETFONT, strp, 484,191,220,20, &G.buts->texnr, 0, 0, 0, 0, "");
	
	if (cu->vfont->packedfile) {
		packdummy = 1;
	} else {
		packdummy = 0;
	}
	
	uiDefIconButI(block, TOG|BIT|0, B_PACKFONT, ICON_PACKAGE,	706,191,20,20, &packdummy, 0, 0, 0, 0, "Pack/Unpack this Vectorfont");
	
	MEM_freeN(strp);
	
	uiDefBut(block, BUT,B_LOADFONT, "Load Font",	484,163,103,23, 0, 0, 0, 0, 0, "");

}

/* *************************** CURVE ******************************** */


void do_curvebuts(unsigned short event)
{
	extern Nurb *lastnu;
	extern ListBase editNurb;  /* from editcurve */
	Object *ob;
	Curve *cu;
	Nurb *nu;
	
	ob= OBACT;
	if(ob==0) return;
	
	switch(event) {	

	case B_CONVERTPOLY:
	case B_CONVERTBEZ:
	case B_CONVERTBSPL:
	case B_CONVERTCARD:
	case B_CONVERTNURB:
		if(G.obedit) {
			setsplinetype(event-B_CONVERTPOLY);
			makeDispList(G.obedit);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_UNIFU:
	case B_ENDPU:
	case B_BEZU:
	case B_UNIFV:
	case B_ENDPV:
	case B_BEZV:
		if(G.obedit) {
			nu= editNurb.first;
			while(nu) {
				if(isNurbsel(nu)) {
					if((nu->type & 7)==CU_NURBS) {
						if(event<B_UNIFV) {
							nu->flagu &= 1;
							nu->flagu += ((event-B_UNIFU)<<1);
							makeknots(nu, 1, nu->flagu>>1);
						}
						else if(nu->pntsv>1) {
							nu->flagv &= 1;
							nu->flagv += ((event-B_UNIFV)<<1);
							makeknots(nu, 2, nu->flagv>>1);
						}
					}
				}
				nu= nu->next;
			}
			makeDispList(G.obedit);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SETWEIGHT:
		if(G.obedit) {
			weightflagNurb(1, editbutweight, 0);
			makeDispList(G.obedit);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SETW1:
		editbutweight= 1.0;
		scrarea_queue_winredraw(curarea);
		break;
	case B_SETW2:
		editbutweight= sqrt(2.0)/4.0;
		scrarea_queue_winredraw(curarea);
		break;
	case B_SETW3:
		editbutweight= 0.25;
		scrarea_queue_winredraw(curarea);
		break;
	case B_SETW4:
		editbutweight= sqrt(0.5);
		scrarea_queue_winredraw(curarea);
		break;
	case B_SETORDER:
		if(G.obedit) {
			nu= lastnu;
			if(nu && (nu->type & 7)==CU_NURBS ) {
				if(nu->orderu>nu->pntsu) {
					nu->orderu= nu->pntsu;
					scrarea_queue_winredraw(curarea);
				}
				makeknots(nu, 1, nu->flagu>>1);
				if(nu->orderv>nu->pntsv) {
					nu->orderv= nu->pntsv;
					scrarea_queue_winredraw(curarea);
				}
				makeknots(nu, 2, nu->flagv>>1);
			}
			makeDispList(G.obedit);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_MAKEDISP:
		if(ob->type==OB_FONT) text_to_curve(ob, 0);
		makeDispList(ob);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */
		break;
	
	case B_SUBDIVCURVE:
		subdivideNurb();
		break;
	case B_SPINNURB:
		/* bad bad bad!!! use brackets!!! In case you wondered:
		  {==,!=} goes before & goes before || */
		if( (G.obedit==0) || 
		    (G.obedit->type!=OB_SURF) || 
			((G.obedit->lay & G.vd->lay) == 0) ) return;
		spinNurb(0, 0);
		countall();
		makeDispList(G.obedit);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_CU3D:	    /* allow 3D curve */
		if(G.obedit) {
			cu= G.obedit->data;
			nu= editNurb.first;
			while(nu) {
				nu->type &= ~CU_2D;
				if((cu->flag & CU_3D)==0) nu->type |= CU_2D;
				test2DNurb(nu);
				nu= nu->next;
			}
		}
		if(ob->type==OB_CURVE) {
			cu= ob->data;
			nu= cu->nurb.first;
			while(nu) {
				nu->type &= ~CU_2D;
				if((cu->flag & CU_3D)==0) nu->type |= CU_2D;
				test2DNurb(nu);
				nu= nu->next;
			}
		}
		makeDispList(G.obedit);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_SETRESOLU:
		if(ob->type==OB_CURVE) {
			cu= ob->data;
			if(ob==G.obedit) nu= editNurb.first;
			else nu= cu->nurb.first;
			
			while(nu) {
				nu->resolu= cu->resolu;
				nu= nu->next;
			}
		}
		else if(ob->type==OB_FONT) text_to_curve(ob, 0);
		
		makeDispList(ob);
		allqueue(REDRAWVIEW3D, 0);

		break;
	}
}

static void editing_panel_curve_tools(Object *ob, Curve *cu)
{
	Nurb *nu;
	extern ListBase editNurb;  /* from editcurve */
	extern Nurb *lastnu;
	uiBlock *block;
	short *sp;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_curve_tools", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Curve Tools", "Editing", 640, 0, 318, 204)==0) return;
	
	if(ob->type==OB_CURVE) {
		uiDefBut(block, LABEL, 0, "Convert",	463,173,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTPOLY,"Poly",		467,152,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTBEZ,"Bezier",	467,132,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTBSPL,"Bspline",	467,112,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTCARD,"Cardinal",	467,92,72, 18, 0, 0, 0, 0, 0, "");
		uiDefBut(block, BUT,B_CONVERTNURB,"Nurb",		467,72,72, 18, 0, 0, 0, 0, 0, "");
	}
	uiDefBut(block, LABEL, 0, "Make Knots",562,173,102, 18, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_UNIFU,"Uniform U",	565,152,102, 18, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_ENDPU,"Endpoint U",	565,132,102, 18, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_BEZU,"Bezier U",	565,112,102, 18, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_UNIFV,"V",		670,152,50, 18, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_ENDPV,"V",		670,132,50, 18, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_BEZV,"V",		670,112,50, 18, 0, 0, 0, 0, 0, "");

	uiDefBut(block, BUT,B_SETWEIGHT,"Set Weight",	465,11,95,49, 0, 0, 0, 0, 0, "");

	uiDefButF(block, NUM,0,"Weight:",	564,36,102,22, &editbutweight, 0.01, 10.0, 10, 0, "");
	uiDefBut(block, BUT,B_SETW1,"1.0",		669,36,50,22, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_SETW2,"sqrt(2)/4",564,11,57,20, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_SETW3,"0.25",		621,11,43,20, 0, 0, 0, 0, 0, "");
	uiDefBut(block, BUT,B_SETW4,"sqrt(0.5)",664,11,57,20, 0, 0, 0, 0, 0, "");
	
	if(ob==G.obedit) {
		nu= lastnu;
		if(nu==NULL) nu= editNurb.first;
		if(nu) {
			sp= &(nu->orderu); 
			uiDefButS(block, NUM, B_SETORDER, "Order U:", 565,91,102, 18, sp, 2.0, 6.0, 0, 0, "");
			sp= &(nu->orderv); 
			uiDefButS(block, NUM, B_SETORDER, "V:",	 670,91,50, 18, sp, 2.0, 6.0, 0, 0, "");
			sp= &(nu->resolu); 
			uiDefButS(block, NUM, B_MAKEDISP, "Resol U:", 565,70,102, 18, sp, 1.0, 128.0, 0, 0, "");
			sp= &(nu->resolv); 
			uiDefButS(block, NUM, B_MAKEDISP, "V:", 670,70,50, 18, sp, 1.0, 128.0, 0, 0, "");
		}
	}
	

}

static void editing_panel_curve_tools1(Object *ob, Curve *cu)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_curve_tools1", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Curve Tools1", "Editing", 960, 0, 318, 204)==0) return;
	
	uiDefBut(block, BUT, B_SUBDIVCURVE, "Subdivide", 400,180,150,20, 0, 0, 0, 0, 0, "");
	if(ob->type==OB_SURF) {
		uiDefBut(block, BUT, B_SPINNURB, "Spin",	 400,160,150,20, 0, 0, 0, 0, 0, "");
	}
	
	uiDefBut(block, BUT,B_HIDE,		"Hide",			400,120,150,18, 0, 0, 0, 0, 0, "Hides selected faces");
	uiDefBut(block, BUT,B_REVEAL,	"Reveal",		400,100,150,18, 0, 0, 0, 0, 0, "Reveals selected faces");
	uiDefBut(block, BUT,B_SELSWAP,	"Select Swap",	400,80,150,18, 0, 0, 0, 0, 0, "Selects unselected faces, and deselects selected faces");

	uiDefButF(block, NUM,	REDRAWVIEW3D, "NSize:",	400, 40, 150, 19, &editbutsize, 0.001, 1.0, 10, 0, "");
}

/* for curve, surf and font! */
static void editing_panel_curve_type(Object *ob, Curve *cu)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_curve_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Curve and Surface", "Editing", 320, 0, 318, 204)==0) return;
	
	uiDefButS(block, TOG|BIT|5, 0, "UV Orco",					600,160,150,19, &cu->flag, 0, 0, 0, 0, "");
	if(ob->type==OB_SURF) 
		uiDefButS(block, TOG|BIT|6, REDRAWVIEW3D, "No Puno Flip",	600,140,150,19, &cu->flag, 0, 0, 0, 0, "");

	uiDefBut(block, BUT,B_DOCENTRE, "Centre",					600, 115, 150, 19, 0, 0, 0, 0, 0, "Shifts object data to be centered about object's origin");
	uiDefBut(block, BUT,B_DOCENTRENEW, "Centre New",			600, 95, 150, 19, 0, 0, 0, 0, 0, "Shifts object's origin to center of object data");
	uiDefBut(block, BUT,B_DOCENTRECURSOR, "Centre Cursor",		600, 75, 150, 19, 0, 0, 0, 0, 0, "Shifts object's origin to cursor location");

	if(ob->type==OB_SURF) {
		if(cu->key) {
			/* uiDefButS(block, NUM, B_DIFF, "Slurph:",			600,25,140,19, &(cu->key->slurph), -500.0, 500.0,0,0); ,""*/
			uiDefButS(block, TOG, B_RELKEY, "Relative Keys",	600,45,140,19, &cu->key->type, 0, 0, 0, 0, "");
		}
	}

	if(ob->type!=OB_SURF) {
		
		if(ob->type==OB_CURVE) {
			static float prlen;
			char str[32];
			uiDefButS(block, NUM, B_RECALCPATH, "PathLen:",			600,50,150,19, &cu->pathlen, 1.0, 9000.0, 0, 0, "");
			uiDefButS(block, TOG|BIT|3, B_RECALCPATH, "CurvePath",	600,30,75,19 , &cu->flag, 0, 0, 0, 0, "");
			uiDefButS(block, TOG|BIT|4, REDRAWVIEW3D, "CurveFollow",675,30,75,19, &cu->flag, 0, 0, 0, 0, "");
			sprintf(str, "%.4f", prlen);
			uiDefBut(block, BUT, B_PRINTLEN,		"PrintLen",	600,10,75,19, 0, 0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, str,						675,10,75,19, 0, 1.0, 0, 0, 0, "");
		}
		uiDefButS(block, NUM, B_MAKEDISP, "DefResolU:",	760,160,120,19, &cu->resolu, 1.0, 128.0, 0, 0, "");
		uiDefBut(block, BUT, B_SETRESOLU, "Set",		880,160,30,19, 0, 0, 0, 0, 0, "");
		
		uiDefButS(block, NUM, B_MAKEDISP, "BevResol:",	760,30,150,19, &cu->bevresol, 0.0, 10.0, 0, 0, "");

		uiDefIDPoinBut(block, test_obcurpoin_but, B_MAKEDISP, "BevOb:",		760,10,150,19, &cu->bevobj, "");
		uiDefButF(block, NUM, B_MAKEDISP, "Width:",		760,90,150,19, &cu->width, 0.0, 2.0, 1, 0, "");
		uiDefButF(block, NUM, B_MAKEDISP, "Ext1:",		760,70,150,19, &cu->ext1, 0.0, 5.0, 10, 0, "");
		uiDefButF(block, NUM, B_MAKEDISP, "Ext2:",		760,50,150,19, &cu->ext2, 0.0, 2.0, 1, 0, "");

		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButS(block, TOG|BIT|2, B_MAKEDISP, "Back",	760,130,50,19, &cu->flag, 0, 0, 0, 0, "");
		uiDefButS(block, TOG|BIT|1, B_MAKEDISP, "Front",810,130,50,19, &cu->flag, 0, 0, 0, 0, "");
		uiDefButS(block, TOG|BIT|0, B_CU3D, "3D",		860,130,50,19, &cu->flag, 0, 0, 0, 0, "");

		
	}

}


/* *************************** CAMERA ******************************** */


static void editing_panel_camera_type(Object *ob, Camera *cam)
{
	uiBlock *block;
	float grid=0.0;
	
	if(G.vd) grid= G.vd->grid; 
	if(grid<1.0) grid= 1.0;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_camera_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Camera", "Editing", 320, 0, 318, 204)==0) return;


	uiDefButF(block, NUM,REDRAWVIEW3D, "Lens:", 470,178,160,20, &cam->lens, 1.0, 250.0, 100, 0, "Specify the lens of the camera");
	uiDefButF(block, NUM,REDRAWVIEW3D, "ClipSta:", 470,147,160,20, &cam->clipsta, 0.001*grid, 100.0*grid, 10, 0, "Specify the startvalue of the the field of view");
	uiDefButF(block, NUM,REDRAWVIEW3D, "ClipEnd:", 470,125,160,20, &cam->clipend, 1.0, 5000.0*grid, 100, 0, "Specify the endvalue of the the field of view");
	uiDefButF(block, NUM,REDRAWVIEW3D, "DrawSize:", 470,90,160,20, &cam->drawsize, 0.1*grid, 10.0, 10, 0, "Specify the drawsize of the camera");

	uiDefButS(block, TOG, REDRAWVIEW3D, "Ortho", 470,49,61,40, &cam->type, 0, 0, 0, 0, "Render orthogonally");

	uiDefButS(block, TOG|BIT|0,REDRAWVIEW3D, "ShowLimits", 533,69,97,20, &cam->flag, 0, 0, 0, 0, "Draw the field of view");
	uiDefButS(block, TOG|BIT|1,REDRAWVIEW3D, "Show Mist", 533,49,97,20, &cam->flag, 0, 0, 0, 0, "Draw a line that indicates the mist area");
	
	if(G.special1 & G_HOLO) {
		if(cam->netend==0.0) cam->netend= EFRA;
		uiDefButF(block, NUM, REDRAWVIEW3D, "Anim len",		670,80,100,20, &cam->netend, 1.0, 2500.0, 0, 0, "");
		uiDefButF(block, NUM, REDRAWVIEW3D, "Path len:",		670,160,100,20, &cam->hololen, 0.1, 25.0, 10, 0, "");
		uiDefButF(block, NUM, REDRAWVIEW3D, "Shear fac:",		670,140,100,20, &cam->hololen1, 0.1, 5.0, 10, 0, "");
		uiDefButS(block, TOG|BIT|4, REDRAWVIEW3D, "Holo 1",	670,120,100,20, &cam->flag, 0.0, 0.0, 0, 0, "");
		uiDefButS(block, TOG|BIT|5, REDRAWVIEW3D, "Holo 2",	670,100,100,20, &cam->flag, 0.0, 0.0, 0, 0, "");
		
	}

}

/* *************************** MBALL ******************************** */

void do_mballbuts(unsigned short event)
{
	switch(event) {
	case B_RECALCMBALL:
		makeDispList(OBACT);
		allqueue(REDRAWVIEW3D, 0);
		break;
	}
}

static void editing_panel_mball_type(Object *ob, MetaBall *mb)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mball_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "MetaBall", "Editing", 320, 0, 318, 204)==0) return;
	
	if (ob==find_basis_mball(ob)) {
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "Wiresize:",	470,178,250,19, &mb->wiresize, 0.05, 1.0, 0, 0, "");
		uiDefButF(block, NUMSLI, 0, "Rendersize:",			470,158,250,19, &mb->rendersize, 0.05, 1.0, 0, 0, "");
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "Threshold:", 470,138,250,19, &mb->thresh, 0.0001, 5.0, 0, 0, "");

		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefBut(block, LABEL, 0, "Update:",		471,108,120,19, 0, 0, 0, 0, 0, "");
		uiDefButS(block, ROW, B_DIFF, "Always",	471, 85, 120, 19, &mb->flag, 0.0, 0.0, 0, 0, "");
		uiDefButS(block, ROW, B_DIFF, "Half Res",	471, 65, 120, 19, &mb->flag, 0.0, 1.0, 0, 0, "");
		uiDefButS(block, ROW, B_DIFF, "Fast",		471, 45, 120, 19, &mb->flag, 0.0, 2.0, 0, 0, "");
	}
	
}

static void editing_panel_mball_tools(Object *ob, MetaBall *mb)
{
	extern MetaElem *lastelem;
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mball_tools", UI_EMBOSS, UI_HELV, curarea->win);
	if( uiNewPanel(curarea, block, "MetaBall tools", "Editing", 640, 0, 318, 204)==0) return;
	
	if(ob==G.obedit && lastelem) {
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "Stiffness:", 750,178,250,19, &lastelem->s, 0.0, 10.0, 0, 0, "");
		if(lastelem->type!=MB_BALL)
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "dx:",		750,158,250,19, &lastelem->expx, 0.0, 20.0, 0, 0, "");
		if((lastelem->type!=MB_BALL)&&(lastelem->type!=MB_TUBE))
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "dy:",		750,138,250,19, &lastelem->expy, 0.0, 20.0, 0, 0, "");

		if((lastelem->type==MB_CUBE)||(lastelem->type==MB_ELIPSOID))
		uiDefButF(block, NUMSLI, B_RECALCMBALL, "dz:",		750,118,250,19, &lastelem->expz, 0.0, 20.0, 0, 0, "");

		uiDefButS(block, TOG|BIT|1, B_RECALCMBALL, "Negative",753,16,60,19, &lastelem->flag, 0, 0, 0, 0, "");

		uiDefButS(block, ROW, B_RECALCMBALL, "Ball",			753,83,60,19, &lastelem->type, 1.0, 0.0, 0, 0, "");
		uiDefButS(block, ROW, B_RECALCMBALL, "Tube",			753,62,60,19, &lastelem->type, 1.0, 4.0, 0, 0, "");
		uiDefButS(block, ROW, B_RECALCMBALL, "Plane",			814,62,60,19, &lastelem->type, 1.0, 5.0, 0, 0, "");
		uiDefButS(block, ROW, B_RECALCMBALL, "Elipsoid",		876,62,60,19, &lastelem->type, 1.0, 6.0, 0, 0, "");
		uiDefButS(block, ROW, B_RECALCMBALL, "Cube",			938,62,60,19, &lastelem->type, 1.0, 7.0, 0, 0, "");
	}

}


/* *************************** LATTICE ******************************** */

void do_latticebuts(unsigned short event)
{
	Object *ob;
	Lattice *lt;
	
	ob= OBACT;
	if(ob->type!=OB_LATTICE) return;
	
	switch(event) {
	case B_RESIZELAT:
		if(ob) {
			if(ob==G.obedit) resizelattice(editLatt);
			else resizelattice(ob->data);
		}
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_DRAWLAT:
		if(ob==G.obedit) calc_lattverts_ext();
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_LATTCHANGED:
		
		lt= ob->data;
		if(lt->flag & LT_OUTSIDE) outside_lattice(lt);
		
		make_displists_by_parent(ob);

		allqueue(REDRAWVIEW3D, 0);
		
		break;
	}
}

static void editing_panel_lattice_type(Object *ob, Lattice *lt)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_lattice_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Latice", "Editing", 320, 0, 318, 204)==0) return;
	

	uiSetButLock(lt->key!=0, "Not with VertexKeys");
	uiSetButLock(ob==G.obedit, "Unable to perform function in EditMode");
	uiDefButS(block, NUM, B_RESIZELAT,	"U:",			470,178,100,19, &lt->pntsu, 1.0, 64.0, 0, 0, "");
	uiDefButS(block, NUM, B_RESIZELAT,	"V:",			470,158,100,19, &lt->pntsv, 1.0, 64.0, 0, 0, "");
	uiDefButS(block, NUM, B_RESIZELAT,	"W:",			470,138,100,19, &lt->pntsw, 1.0, 64.0, 0, 0, "");
	uiClearButLock();
	
	uiDefButC(block, ROW, B_LATTCHANGED,		"Lin",		572, 178, 40, 19, &lt->typeu, 1.0, (float)KEY_LINEAR, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Card",		612, 178, 40, 19, &lt->typeu, 1.0, (float)KEY_CARDINAL, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"B",		652, 178, 40, 19, &lt->typeu, 1.0, (float)KEY_BSPLINE, 0, 0, "");

	uiDefButC(block, ROW, B_LATTCHANGED,		"Lin",		572, 158, 40, 19, &lt->typev, 2.0, (float)KEY_LINEAR, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Card",		612, 158, 40, 19, &lt->typev, 2.0, (float)KEY_CARDINAL, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"B",		652, 158, 40, 19, &lt->typev, 2.0, (float)KEY_BSPLINE, 0, 0, "");

	uiDefButC(block, ROW, B_LATTCHANGED,		"Lin",		572, 138, 40, 19, &lt->typew, 3.0, (float)KEY_LINEAR, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"Card",		612, 138, 40, 19, &lt->typew, 3.0, (float)KEY_CARDINAL, 0, 0, "");
	uiDefButC(block, ROW, B_LATTCHANGED,		"B",		652, 138, 40, 19, &lt->typew, 3.0, (float)KEY_BSPLINE, 0, 0, "");
	
	uiDefBut(block, BUT, B_RESIZELAT,	"Make Regular",		470,101,99,32, 0, 0, 0, 0, 0, "");

	uiDefButS(block, TOG|BIT|1, B_LATTCHANGED, "Outside",	571,101,120,31, &lt->flag, 0, 0, 0, 0, "");

	if(lt->key) {
		uiDefButS(block, NUM, B_DIFF, "Slurph:",			470,60,120,19, &(lt->key->slurph), -500.0, 500.0, 0, 0, "");
		uiDefButS(block, TOG, B_RELKEY, "Relative Keys",	470,40,120,19, &lt->key->type, 0, 0, 0, 0, "");
	}

}

/* *************************** ARMATURE ******************************** */



static int editbone_to_parnr (EditBone *bone)
{
	EditBone *ebone;
	int	index;

	for (ebone=G.edbo.first, index=0; ebone; ebone=ebone->next, index++){
		if (ebone==bone)
			return index;
	}

	return -1;
}



static void attach_bone_to_parent(EditBone *bone)
{
	EditBone *curbone;

	if (bone->flag & BONE_IK_TOPARENT) {

	/* See if there are any other bones that refer to the same parent and disconnect them */
		for (curbone = G.edbo.first; curbone; curbone=curbone->next){
			if (curbone!=bone){
				if (curbone->parent && (curbone->parent == bone->parent) && (curbone->flag & BONE_IK_TOPARENT))
					curbone->flag &= ~BONE_IK_TOPARENT;
			}
		}

	/* Attach this bone to its parent */
		VECCOPY(bone->head, bone->parent->tail);
	}

}

static void attach_bone_to_parent_cb(void *bonev, void *arg2_unused)
{
	EditBone *curBone= bonev;
	attach_bone_to_parent(curBone);
}

static void parnr_to_editbone(EditBone *bone)
{
	if (bone->parNr == -1){
		bone->parent = NULL;
		bone->flag &= ~BONE_IK_TOPARENT;
	}
	else{
		bone->parent = BLI_findlink(&G.edbo, bone->parNr);
		attach_bone_to_parent(bone);
	}
}

static void parnr_to_editbone_cb(void *bonev, void *arg2_unused)
{
	EditBone *curBone= bonev;
	parnr_to_editbone(curBone);
}


static void build_bonestring (char *string, EditBone *bone){
	EditBone *curBone;
	EditBone *pBone;
	int		skip=0;
	int		index;

	sprintf (string, "Parent%%t| %%x%d", -1);	/* That space is there for a reason */
	
	for (curBone = G.edbo.first, index=0; curBone; curBone=curBone->next, index++){
		/* Make sure this is a valid child */
		if (curBone != bone){
			skip=0;
			for (pBone=curBone->parent; pBone; pBone=pBone->parent){
				if (pBone==bone){
					skip=1;
					break;
				}
			}
			
			if (skip)
				continue;
			
			sprintf (string, "%s|%s%%x%d", string, curBone->name, index);
		}
	}
}

static void validate_editbonebutton(EditBone *eBone){
	EditBone	*prev;
	bAction		*act=NULL;
	bActionChannel *chan;
	Base *base;

	/* Separate the bone from the G.edbo */
	prev=eBone->prev;
	BLI_remlink (&G.edbo, eBone);

	/*	Validate the name */
	unique_editbone_name (eBone->name);

	/* Re-insert the bone */
	if (prev)
		BLI_insertlink(&G.edbo, prev, eBone);
	else
		BLI_addhead (&G.edbo, eBone);

	/* Rename channel if necessary */
	if (G.obedit)
		act = G.obedit->action;

	if (act && !act->id.lib){
		//	Find the appropriate channel
		for (chan = act->chanbase.first; chan; chan=chan->next){
			if (!strcmp (chan->name, eBone->oldname)){
				strcpy (chan->name, eBone->name);
			}
		}
		allqueue(REDRAWACTION, 0);
	}

	/* Update the parenting info of any users */
	/*	Yes, I know this is the worst thing you have ever seen. */

	for (base = G.scene->base.first; base; base=base->next){
		Object *ob = base->object;

		/* See if an object is parented to this armature */
		if (ob->parent && ob->partype==PARBONE && (ob->parent->type==OB_ARMATURE) && (ob->parent->data == G.obedit->data)){
			if (!strcmp(ob->parsubstr, eBone->oldname))
				strcpy(ob->parsubstr, eBone->name);
		}
	}

	exit_editmode(0);	/* To ensure new names make it to the edit armature */

}

static void validate_editbonebutton_cb(void *bonev, void *arg2_unused)
{
	EditBone *curBone= bonev;
	validate_editbonebutton(curBone);
}


static void editing_panel_armature_type(Object *ob, bArmature *arm)
{
	uiBlock		*block;
	int			bx=148, by=100;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_armature_type", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Armature", "Editing", 320, 0, 318, 204)==0) return;
		
	uiDefButI(block, TOG|BIT|ARM_RESTPOSBIT,REDRAWVIEW3D, "Rest Pos", bx,by,97,20, &arm->flag, 0, 0, 0, 0, "Disable all animation for this object");
	uiDefButI(block, TOG|BIT|ARM_DRAWAXESBIT,REDRAWVIEW3D, "Draw Axes", bx,by-46,97,20, &arm->flag, 0, 0, 0, 0, "Draw bone axes");
	uiDefButI(block, TOG|BIT|ARM_DRAWNAMESBIT,REDRAWVIEW3D, "Draw Names", bx,by-69,97,20, &arm->flag, 0, 0, 0, 0, "Draw bone names");
	uiDefButI(block, TOG|BIT|ARM_DRAWXRAYBIT,REDRAWVIEW3D, "X-Ray", bx,by-92,97,20, &arm->flag, 0, 0, 0, 0, "Draw armature in front of shaded objects");
	
	

}

static void editing_panel_armature_bones(Object *ob, bArmature *arm)
{
	uiBlock		*block;
	EditBone	*curBone;
	uiBut		*but;
	char		*boneString=NULL;
	int			bx=148, by=180;
	int			index;

	/* Draw the bone name block */

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_armature_bones", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Armature Bones", "Editing", 640, 0, 318, 204)==0) return;

	/* this is a variable height panel, newpanel doesnt force new size on existing panels */
	/* so first we make it default height */
	uiNewPanelHeight(block, 204);


	uiDefBut(block, LABEL, 0, "Selected Bones",						bx,by,158,18, 0, 0, 0, 0, 0, "");
	by-=20;
	for (curBone=G.edbo.first, index=0; curBone; curBone=curBone->next, index++){
		if (curBone->flag & (BONE_SELECTED)) {

			/* Hide in posemode flag */
			uiDefButI(block, TOG|BIT|BONE_HIDDENBIT, REDRAWVIEW3D, "Hide", bx-55,by,45,18, &curBone->flag, 0, 0, 0, 0, "Toggles display of this bone in posemode");
			
			/*	Bone naming button */
			strcpy (curBone->oldname, curBone->name);
			but=uiDefBut(block, TEX, REDRAWVIEW3D, "BO:", bx-10,by,117,18, &curBone->name, 0, 24, 0, 0, "Change the bone name");
			uiButSetFunc(but, validate_editbonebutton_cb, curBone, NULL);
			
			uiDefBut(block, LABEL, 0, "child of", bx+107,by,73,18, NULL, 0.0, 0.0, 0.0, 0.0, "");

			boneString = malloc((BLI_countlist(&G.edbo) * 64)+64);
			build_bonestring (boneString, curBone);
			
			curBone->parNr = editbone_to_parnr(curBone->parent);
			but = uiDefButI(block, MENU,REDRAWVIEW3D, boneString, bx+180,by,120,18, &curBone->parNr, 0.0, 0.0, 0.0, 0.0, "Parent");
			uiButSetFunc(but, parnr_to_editbone_cb, curBone, NULL);

			free(boneString);

			/* IK to parent flag */
			if (curBone->parent){
				but=uiDefButI(block, TOG|BIT|BONE_IK_TOPARENTBIT, REDRAWVIEW3D, "IK", bx+300,by,32,18, &curBone->flag, 0.0, 0.0, 0.0, 0.0, "IK link to parent");
				uiButSetFunc(but, attach_bone_to_parent_cb, curBone, NULL);
			}

			/* Dist and weight buttons */
			but=uiDefButS(block, MENU, REDRAWVIEW3D,
							"Skinnable %x0|"
							"Unskinnable %x1|"
							"Head %x2|"
							"Neck %x3|"
							"Back %x4|"
							"Shoulder %x5|"
							"Arm %x6|"
							"Hand %x7|"
							"Finger %x8|"
							"Thumb %x9|"
							"Pelvis %x10|"
							"Leg %x11|"
							"Foot %x12|"
							"Toe %x13|"
							"Tentacle %x14",
							bx-10,by-19,117,18,
							&curBone->boneclass,
							0.0, 0.0, 0.0, 0.0, 
							"Classification of armature element");
			
			/* Dist and weight buttons */
			uiDefButF(block, NUM,REDRAWVIEW3D, "Dist:", bx+110, by-19, 
						105, 18, &curBone->dist, 0.0, 1000.0, 10.0, 0.0, 
						"Bone deformation distance");
			uiDefButF(block, NUM,REDRAWVIEW3D, "Weight:", bx+223, by-19, 
						110, 18, &curBone->weight, 0.0F, 1000.0F, 
						10.0F, 0.0F, "Bone deformation weight");
			
			by-=42;	
		}
	}
	
	if(by<0) {
		uiNewPanelHeight(block, 204 - by);
	}
	
}


/* *************************** MESH ******************************** */



void do_meshbuts(unsigned short event)
{
	void decimate_faces(void);
	void decimate_cancel(void);
	void decimate_apply(void);
	Object *ob;
	Mesh *me;
	float fac;
	short randfac;

	ob= OBACT;
	if(ob && ob->type==OB_MESH) {
		
		me= get_mesh(ob);
		if(me==0) return;
		
		switch(event) {
		case B_AUTOVGROUP:
			if (!get_armature(ob->parent)){
				error ("Mesh must be the child of an armature");
				break;
			}
				/* Verify that there are vertex groups for bones in armature */
				/* Remove selected vertices from all defgroups */
				/* Perform assignment for selected vertices */

			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_NEWVGROUP:
			add_defgroup (G.obedit);
			scrarea_queue_winredraw(curarea);
			break;
		case B_DELVGROUP:
			del_defgroup (G.obedit);
			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_ASSIGNVGROUP:
			undo_push_mesh("Assign to vertex group");
			assign_verts_defgroup ();
			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_REMOVEVGROUP:
			undo_push_mesh("Remove from vertex group");
			remove_verts_defgroup (0);
			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_SELVGROUP:
			sel_verts_defgroup(1);
			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_DESELVGROUP:
			sel_verts_defgroup(0);
			allqueue (REDRAWVIEW3D, 1);
			break;
		case B_DELSTICKY:
		
			if(me->msticky) MEM_freeN(me->msticky);
			me->msticky= 0;
			allqueue(REDRAWBUTSEDIT, 0);
			break;
		case B_MAKESTICKY:
			make_sticky();
			break;
		case B_MAKEVERTCOL:
			make_vertexcol();
			break;
		case B_DELVERTCOL:
			if(me->mcol) MEM_freeN(me->mcol);
			me->mcol= 0;
			G.f &= ~G_VERTEXPAINT;
			freedisplist(&(ob->disp));
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWVIEW3D, 0);
			break;

		case B_MAKE_TFACES:
			make_tfaces(me);
			allqueue(REDRAWBUTSEDIT, 0);
			break;

		case B_DEL_TFACES:
			if(me->tface) MEM_freeN(me->tface);
			me->tface= 0;
			G.f &= ~G_FACESELECT;
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIMAGE, 0);
			break;
			
		case B_FLIPNORM:
			if(G.obedit) {
				flip_editnormals();
			}
			else flipnorm_mesh( get_mesh(ob) );
			
			allqueue(REDRAWVIEW3D, 0);
			break;

		case B_DECIM_FACES:
			decimate_faces();
			break;
		case B_DECIM_CANCEL:
			decimate_cancel();
			break;
		case B_DECIM_APPLY:
			decimate_apply();
			break;

		case B_SLOWERDRAW:
			slowerdraw();
			break;
		case B_FASTERDRAW:
			fasterdraw();
			break;
		}
	}
	
	if(G.obedit==0 || (G.obedit->type!=OB_MESH)) return;
	
	switch(event) {
	case B_SPIN:
		if( select_area(SPACE_VIEW3D)) spin_mesh(step, degr, 0, 0);
		break;
	case B_SPINDUP:
		if( select_area(SPACE_VIEW3D)) spin_mesh(step, degr, 0, 1);
		break;
	case B_EXTR:
		G.f |= G_DISABLE_OK;
		if( select_area(SPACE_VIEW3D)) extrude_mesh();
		G.f -= G_DISABLE_OK;
		break;
	case B_SCREW:
		if( select_area(SPACE_VIEW3D)) screw_mesh(step, turn);
		break;
	case B_EXTREP:
		if( select_area(SPACE_VIEW3D)) extrude_repeat_mesh(step, extr_offs);
		break;
	case B_SPLIT:
		G.f |= G_DISABLE_OK;
		split_mesh();
		G.f -= G_DISABLE_OK;
		break;
	case B_REMDOUB:
		undo_push_mesh("Rem Doubles");
		notice("Removed: %d", removedoublesflag(1, doublimit));
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_SUBDIV:
		waitcursor(1);
		undo_push_mesh("Subdivide");
		subdivideflag(1, 0.0, editbutflag & B_BEAUTY);
		countall();
		waitcursor(0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_FRACSUBDIV:
		randfac= 10;
		if(button(&randfac, 1, 100, "Rand fac:")==0) return;
		waitcursor(1);
		undo_push_mesh("Fractal Subdivide");
		fac= -( (float)randfac )/100;
		subdivideflag(1, fac, editbutflag & B_BEAUTY);
		countall();
		waitcursor(0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_XSORT:
		if( select_area(SPACE_VIEW3D)) xsortvert_flag(1);
		break;
	case B_HASH:
		hashvert_flag(1);
		break;
	case B_TOSPHERE:
		vertices_to_sphere();
		break;
	case B_VERTEXNOISE:
		vertexnoise();
		break;
	case B_VERTEXSMOOTH:
		vertexsmooth();
		break;
	}
	/* WATCH IT: previous events only in editmode! */
}

static void editing_panel_mesh_tools(Object *ob, Mesh *me)
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_tools", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Mesh Tools", "Editing", 640, 0, 318, 204)==0) return;
	
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|2, 0, "Beauty",		477,195,80,19, &editbutflag, 0, 0, 0, 0, "Causes 'Subdivide' to split faces in halves instead of quarters");
	uiDefBut(block, BUT,B_SUBDIV,"Subdivide",		557,195,80,19, 0, 0, 0, 0, 0, "Splits selected faces into halves or quarters");
	uiDefBut(block, BUT,B_FRACSUBDIV, "Fract Subd",	637,195,85,19, 0, 0, 0, 0, 0, "Subdivides selected faces with a random factor");

	uiDefBut(block, BUT,B_VERTEXNOISE,"Noise",		477,175,80,19, 0, 0, 0, 0, 0, "Use vertex coordinate as texture coordinate");
	uiDefBut(block, BUT,B_HASH,"Hash",				557,175,80,19, 0, 0, 0, 0, 0, "Randomizes selected vertice sequence data");
	uiDefBut(block, BUT,B_XSORT,"Xsort",			637,175,85,19, 0, 0, 0, 0, 0, "Sorts selected vertice data in the X direction");

	uiDefBut(block, BUT,B_TOSPHERE,"To Sphere",		477,155,80,19, 0, 0, 0, 0, 0, "Moves selected vertices outwards into a spherical shape");
	uiDefBut(block, BUT,B_VERTEXSMOOTH,"Smooth",	557,155,80,19, 0, 0, 0, 0, 0, "Flattens angles of selected faces");
	uiDefBut(block, BUT,B_SPLIT,"Split",			637,155,85,19, 0, 0, 0, 0, 0, "Flattens angles of selected faces");

	uiDefBut(block, BUT,B_FLIPNORM,"Flip Normals",	477,135,80,19, 0, 0, 0, 0, 0, "Toggles the direction of the selected face's normals");
	uiDefBut(block, BUT,B_REMDOUB,"Rem Doubles",	557,135,80,19, 0, 0, 0, 0, 0, "Removes duplicates from selected vertices");
	uiDefButF(block, NUM, B_DIFF, "Limit:",			637,135,85,19, &doublimit, 0.0001, 1.0, 10, 0, "Specifies the max distance 'Rem Doubles' will consider vertices as 'doubled'");
	uiBlockEndAlign(block);

	uiDefBut(block, BUT,B_EXTR,"Extrude",			477,105,249,24, 0, 0, 0, 0, 0, "Converts selected edges to faces and selects the new vertices");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_SCREW,"Screw",			477,75,79,24, 0, 0, 0, 0, 0, "Activates the screw tool");  // Bish - This could use some more definition
	uiDefBut(block, BUT,B_SPIN, "Spin",				558,75,78,24, 0, 0, 0, 0, 0, "Extrudes the selected vertices in a circle around the cursor in the indicated viewport");
	uiDefBut(block, BUT,B_SPINDUP,"Spin Dup",		639,75,87,24, 0, 0, 0, 0, 0, "Creates copies of the selected vertices in a circle around the cursor in the indicated viewport");

	uiDefButS(block, NUM, B_DIFF, "Degr:",		477,55,78,19, &degr,10.0,360.0, 0, 0, "Specifies the number of degrees 'Spin' revolves");
	uiDefButS(block, NUM, B_DIFF, "Steps:",		558,55,78,19, &step,1.0,180.0, 0, 0, "Specifies the total number of 'Spin' slices");
	uiDefButS(block, NUM, B_DIFF, "Turns:",		639,55,86,19, &turn,1.0,360.0, 0, 0, "Specifies the number of revolutions the screw turns");
	uiDefButS(block, TOG|BIT|1, B_DIFF, "Keep Original",477,35,156,19, &editbutflag, 0, 0, 0, 0, "Keeps a copy of the original vertices and faces after executing tools");
	uiDefButS(block, TOG|BIT|0, B_DIFF, "Clockwise",	639,35,86,19, &editbutflag, 0, 0, 0, 0, "Specifies the direction for 'Screw' and 'Spin'");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_EXTREP, "Extrude Dup",	477,15,128,19, 0, 0, 0, 0, 0, "Creates copies of the selected vertices in a straight line away from the current viewport");
	uiDefButF(block, NUM, B_DIFF, "Offset:",		608,15,117,19, &extr_offs, 0.01, 10.0, 100, 0, "Sets the distance between each copy for 'Extrude Dup'");
	uiBlockEndAlign(block);


}

static void verify_vertexgroup_name_func(void *datav, void *data2_unused)
{
	unique_vertexgroup_name((bDeformGroup*)datav, OBACT);
}



static void editing_panel_mesh_tools1(Object *ob, Mesh *me)
{
	uiBlock *block;


	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_tools1", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Mesh Tools 1", "Editing", 960, 0, 318, 204)==0) return;

	uiDefBut(block, BUT,B_DOCENTRE, "Centre",				1091, 200, 100, 19, 0, 0, 0, 0, 0, "Shifts object data to be centered about object's origin");
	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_HIDE,		"Hide",		1091,155,77,24, 0, 0, 0, 0, 0, "Hides selected faces");
	uiDefBut(block, BUT,B_REVEAL,	"Reveal",	1171,155,86,24, 0, 0, 0, 0, 0, "Reveals selected faces");
	uiBlockEndAlign(block);

	uiDefBut(block, BUT,B_SELSWAP,	"Select Swap",	1091,124,166,24, 0, 0, 0, 0, 0, "Selects unselected faces, and deselects selected faces");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,		  REDRAWVIEW3D, "NSize:",		1090, 90, 164, 19, &editbutsize, 0.001, 2.0, 10, 0, "Sets the length to use when displaying face normals");
	uiDefButI(block, TOG|BIT|6, REDRAWVIEW3D, "Draw Normals",	1090,70,164,19, &G.f, 0, 0, 0, 0, "Displays face normals as lines");
	uiDefButI(block, TOG|BIT|7, REDRAWVIEW3D, "Draw Faces",	1090,50,164,19, &G.f, 0, 0, 0, 0, "Displays all faces as shades");
	uiDefButI(block, TOG|BIT|18, REDRAWVIEW3D, "Draw Edges", 1090,30,164,19, &G.f, 0, 0, 0, 0, "Displays selected edges using hilights");
	uiDefButI(block, TOG|BIT|11, 0, "All edges",				1090,10,164,19, &G.f, 0, 0, 0, 0, "Displays all edges in object mode without optimization");
	uiBlockEndAlign(block);

}


static void editing_panel_links(Object *ob)
{
	uiBlock *block;
	ID *id, *idfrom;
	int *poin, xco=143;
	float min;
	Material *ma;
	char str[64];
	uiBut *but;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_links", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Link and Materials", "Editing", 0, 0, 318, 204)==0) return;

	buttons_active_id(&id, &idfrom);
	
	if(id) {
		int alone= 0;
		int local= 0;
		int browse= B_EDITBROWSE;

		if(ob->type==OB_MESH) {
			browse= B_MESHBROWSE;
			alone= B_MESHALONE;
			local= B_MESHLOCAL;
			uiSetButLock(G.obedit!=0, "Unable to perform function in EditMode");
		}
		else if(ob->type==OB_MBALL) {
			alone= B_MBALLALONE;
			local= B_MBALLLOCAL;
		}
		else if ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF) {
			alone= B_CURVEALONE;
			local= B_CURVELOCAL;
		}
		else if(ob->type==OB_CAMERA) {
			alone= B_CAMERAALONE;
			local= B_CAMERALOCAL;
		}
		else if(ob->type==OB_LAMP) {
			alone= B_LAMPALONE;
			local= B_LAMPLOCAL;
		}
		else if (ob->type==OB_ARMATURE){
			alone = B_ARMALONE;
			local = B_ARMLOCAL;
		}
		else if(ob->type==OB_LATTICE) {
			alone= B_LATTALONE;
			local= B_LATTLOCAL;
		}
		uiBlockSetCol(block, TH_BUT_SETTING2);
		xco= std_libbuttons(block, 143, 180, 0, NULL, browse, id, idfrom, &(G.buts->menunr), alone, local, 0, 0, B_KEEPDATA);
		uiBlockSetCol(block, TH_AUTO);	
	}
	if(ob) {
		but = uiDefBut(block, TEX, B_IDNAME, "OB:",	xco, 180, 454-xco, YIC, ob->id.name+2, 0.0, 19.0, 0, 0, "Displays Active Object name. Click to change.");
		uiButSetFunc(but, test_idbutton_cb, ob->id.name, NULL);
	}



	/* to be sure */
	if ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL);
	else return;
	
	
	if(ob->type==OB_MESH) poin= &( ((Mesh *)ob->data)->texflag );
	else if(ob->type==OB_MBALL) poin= &( ((MetaBall *)ob->data)->texflag );
	else poin= &( ((Curve *)ob->data)->texflag );
	uiDefButI(block, TOG|BIT|0, B_AUTOTEX, "AutoTexSpace",	143,15,130,19, poin, 0, 0, 0, 0, "Adjusts active object's texture space automatically when transforming object");

	sprintf(str,"%d Mat:", ob->totcol);
	if(ob->totcol) min= 1.0; else min= 0.0;
	ma= give_current_material(ob, ob->actcol);
	
	if(ma) {
		uiDefButF(block, COL, 0, "",			291,123,24,30, &(ma->r), 0, 0, 0, 0, "");
		uiDefBut(block, LABEL, 0, ma->id.name+2, 318,153, 103, 20, 0, 0, 0, 0, 0, "");
	}
	uiDefButC(block, NUM, B_REDR,	str,		318,123,103,30, &ob->actcol, min, (float)(ob->totcol), 0, 0, "Displays total number of material indices and the current index");
	uiDefBut(block, BUT,B_MATWICH,	"?",		423,123,31,30, 0, 0, 0, 0, 0, "In EditMode, sets the active material index from selected faces");
	
	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_MATNEW,	"New",		292,101,80,20, 0, 0, 0, 0, 0, "Adds a new Material index");
	uiDefBut(block, BUT,B_MATDEL,	"Delete",	374,101,80,20, 0, 0, 0, 0, 0, "Deletes this Material index");
	uiDefBut(block, BUT,B_MATSEL,	"Select",	292,76,80,22, 0, 0, 0, 0, 0, "In EditMode, selects faces that have the active index");
	uiDefBut(block, BUT,B_MATDESEL,	"Deselect",	374,76,80,22, 0, 0, 0, 0, 0, "Deselects everything with current indexnumber");
	uiDefBut(block, BUT,B_MATASS,	"Assign",	292,47,162,26, 0, 0, 0, 0, 0, "In EditMode, assigns the active index to selected faces");

	uiBlockBeginAlign(block);
	uiDefBut(block, BUT,B_SETSMOOTH,"Set Smooth",	291,15,80,20, 0, 0, 0, 0, 0, "In EditMode, sets 'smooth' rendering of selected faces");
	uiDefBut(block, BUT,B_SETSOLID,	"Set Solid",	373,15,80,20, 0, 0, 0, 0, 0, "In EditMode, sets 'solid' rendering of selected faces");
	uiBlockEndAlign(block);

	/* vertex group... partially editmode... */
	{
		uiBut *but;
		int	defCount;
		bDeformGroup	*defGroup;
		char *s, *menustr;
		bDeformGroup *dg;
		int min, index;
		
		uiDefBut(block, LABEL,0,"Vertex Groups",	143,153,130,20, 0, 0, 0, 0, 0, "");

		defCount=BLI_countlist(&ob->defbase);

		if (!defCount) min=0;
		else min=1;
		
		s= menustr = MEM_callocN((32 * defCount)+20, "menustr");

		for (index = 1, dg = ob->defbase.first; dg; index++, dg=dg->next) {
			int cnt= sprintf (s, "%s%%x%d|", dg->name, index);
			
			if (cnt>0) s+= cnt;
		}
		
		uiBlockBeginAlign(block);
		if (defCount) uiDefButS(block, MENU, REDRAWBUTSEDIT, menustr,	143, 132,18,21, &ob->actdef, min, defCount, 0, 0, "Browses available vertex groups");

		MEM_freeN (menustr);

		if (ob->actdef){
			defGroup = BLI_findlink(&ob->defbase, ob->actdef-1);
			but= uiDefBut(block, TEX,REDRAWBUTSEDIT,"",		161,132,140-18,21, defGroup->name, 0, 32, 0, 0, "Displays current vertex group name. Click to change. (Match bone name for deformation.)");
			uiButSetFunc(but, verify_vertexgroup_name_func, defGroup, NULL);

			uiDefButF(block, NUM, REDRAWVIEW3D, "Weight:",		143, 111, 140, 21, &editbutvweight, 0, 1, 10, 0, "Sets the current vertex group's bone deformation strength");
		}
		uiBlockEndAlign(block);

		if (G.obedit && G.obedit==ob){
			uiBlockBeginAlign(block);
	/*		uiDefBut(block, BUT,B_AUTOVGROUP,"Auto Weight",			740,by-=22,93,18, 0, 0, 0, 0, 0, "Automatically assigns deformation groups"); */
			uiDefBut(block, BUT,B_NEWVGROUP,"New",			143,90,70,21, 0, 0, 0, 0, 0, "Creates a new vertex group");
			uiDefBut(block, BUT,B_DELVGROUP,"Delete",		213,90,70,21, 0, 0, 0, 0, 0, "Removes the current vertex group");
	
			uiDefBut(block, BUT,B_ASSIGNVGROUP,"Assign",	143,69,70,21, 0, 0, 0, 0, 0, "Assigns selected vertices to the current vertex group");
			uiDefBut(block, BUT,B_REMOVEVGROUP,"Remove",	213,69,70,21, 0, 0, 0, 0, 0, "Removes selected vertices from the current vertex group");
	
			uiDefBut(block, BUT,B_SELVGROUP,"Select",		143,48,70,21, 0, 0, 0, 0, 0, "Selects vertices belonging to the current vertex group");
			uiDefBut(block, BUT,B_DESELVGROUP,"Desel.",		213,48,70,21, 0, 0, 0, 0, 0, "Deselects vertices belonging to the current vertex group");
			uiBlockEndAlign(block);
		}
	}

	
}

/* *************************** FACE/PAINT *************************** */

void do_fpaintbuts(unsigned short event)
{
	Mesh *me;
	Object *ob;
	extern TFace *lasttface; /* caches info on tface bookkeeping ?*/
	
	ob= OBACT;
	if(ob==0) return;

	switch(event) { 
		
	case B_VPGAMMA:
		vpaint_dogamma();
		break;
	case B_COPY_TF_MODE:
	case B_COPY_TF_UV:
	case B_COPY_TF_COL:
	case B_COPY_TF_TEX:
		me= get_mesh(ob);
		if(me && me->tface) {
/*  			extern TFace *lasttface; */
			TFace *tface= me->tface;
			int a= me->totface;
			
			set_lasttface();
			if(lasttface) {
			
				while(a--) {
					if(tface!=lasttface && (tface->flag & TF_SELECT)) {
						if(event==B_COPY_TF_MODE) {
							tface->mode= lasttface->mode;
							tface->transp= lasttface->transp;
						}
						else if(event==B_COPY_TF_UV) {
							memcpy(tface->uv, lasttface->uv, sizeof(tface->uv));
							tface->tpage= lasttface->tpage;
							tface->tile= lasttface->tile;
							
							if(lasttface->mode & TF_TILES) tface->mode |= TF_TILES;
							else tface->mode &= ~TF_TILES;
							
						}
						else if(event==B_COPY_TF_TEX) {
							tface->tpage= lasttface->tpage;
							tface->tile= lasttface->tile;

							if(lasttface->mode & TF_TILES) tface->mode |= TF_TILES;
							else tface->mode &= ~TF_TILES;
						}
						else if(event==B_COPY_TF_COL) memcpy(tface->col, lasttface->col, sizeof(tface->col));
					}
					tface++;
				}
			}
			do_shared_vertexcol(me);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	case B_SET_VCOL:
		clear_vpaint_selectedfaces();
		break;
	case B_REDR_3D_IMA:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		break;
	case B_ASSIGNMESH:
		
		test_object_materials(ob->data);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
		
	case B_TFACE_HALO:
		set_lasttface();
		if(lasttface) {
			lasttface->mode &= ~TF_BILLBOARD2;
			allqueue(REDRAWBUTSEDIT, 0);
		}
		break;

	case B_TFACE_BILLB:
		set_lasttface();
		if(lasttface) {
			lasttface->mode &= ~TF_BILLBOARD;
			allqueue(REDRAWBUTSEDIT, 0);
		}
		break;
	}	
}


/* -------------------- MODE: vpaint faceselect ------------------- */

static void editing_panel_mesh_paint(void)
{
	extern VPaint Gvp;         /* from vpaint */
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_paint", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Paint", "Editing", 640, 0, 318, 204)==0) return;
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, 0, "R ",			979,160,194,19, &Gvp.r, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of red used for painting");
	uiDefButF(block, NUMSLI, 0, "G ",			979,140,194,19, &Gvp.g, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of green used for painting");
	uiDefButF(block, NUMSLI, 0, "B ",			979,120,194,19, &Gvp.b, 0.0, 1.0, B_VPCOLSLI, 0, "The amount of blue used for painting");
	uiBlockEndAlign(block);

	uiDefButF(block, NUMSLI, 0, "Opacity ",		979,100,194,19, &Gvp.a, 0.0, 1.0, 0, 0, "The amount of pressure on the brush");
	uiDefButF(block, NUMSLI, 0, "Size ",		979,80,194,19, &Gvp.size, 2.0, 64.0, 0, 0, "The size of the brush");

	uiDefButF(block, COL, B_VPCOLSLI, "",		1176,100,28,80, &(Gvp.r), 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_DIFF, "Mix",			1212,160,63,19, &Gvp.mode, 1.0, 0.0, 0, 0, "Mix the vertex colours");
	uiDefButS(block, ROW, B_DIFF, "Add",			1212,140,63,19, &Gvp.mode, 1.0, 1.0, 0, 0, "Add the vertex colour");
	uiDefButS(block, ROW, B_DIFF, "Sub",			1212, 120,63,19, &Gvp.mode, 1.0, 2.0, 0, 0, "Subtract from the vertex colour");
	uiDefButS(block, ROW, B_DIFF, "Mul",			1212, 100,63,19, &Gvp.mode, 1.0, 3.0, 0, 0, "Multiply the vertex colour");
	uiDefButS(block, ROW, B_DIFF, "Filter",		1212, 80,63,19, &Gvp.mode, 1.0, 4.0, 0, 0, "Mix the colours with an alpha factor");

	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|1, 0, "Area", 		980,50,80,19, &Gvp.flag, 0, 0, 0, 0, "Set the area the brush covers");
	uiDefButS(block, TOG|BIT|2, 0, "Soft", 		1061,50,112,19, &Gvp.flag, 0, 0, 0, 0, "Use a soft brush");
	uiDefButS(block, TOG|BIT|3, 0, "Normals", 	1174,50,102,19, &Gvp.flag, 0, 0, 0, 0, "Use vertex normal for painting");
	
	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_VPGAMMA, "Set", 	980,30,80,19, 0, 0, 0, 0, 0, "Apply Mul and Gamma to vertex colours");
	uiDefButF(block, NUM, B_DIFF, "Mul:", 		1061,30,112,19, &Gvp.mul, 0.1, 50.0, 10, 0, "Set the number to multiply vertex colours with");
	uiDefButF(block, NUM, B_DIFF, "Gamma:", 	1174,30,102,19, &Gvp.gamma, 0.1, 5.0, 10, 0, "Change the clarity of the vertex colours");
	uiBlockEndAlign(block);
	
	uiDefBut(block, BUT, B_SET_VCOL, "Set VertCol",	980,5,80,20, 0, 0, 0, 0, 0, "Set Vertex colour of selection to current (Shift+K)");

}


static void editing_panel_mesh_texface(void)
{
	uiBlock *block;
	extern TFace *lasttface;
	
	block= uiNewBlock(&curarea->uiblocks, "editing_panel_mesh_texface", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Texture face", "Editing", 960, 0, 318, 204)==0) return;

	set_lasttface();	// checks for ob type
	if(lasttface) {
		
		uiDefButS(block, TOG|BIT|2, B_REDR_3D_IMA, "Tex",	600,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Render face with texture");
		uiDefButS(block, TOG|BIT|7, B_REDR_3D_IMA, "Tiles",	660,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Use tilemode for face");
		uiDefButS(block, TOG|BIT|4, REDRAWVIEW3D, "Light",	720,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Use light for face");
		uiDefButS(block, TOG|BIT|10, REDRAWVIEW3D, "Invisible",780,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Make face invisible");
		uiDefButS(block, TOG|BIT|0, REDRAWVIEW3D, "Collision", 840,160,60,19, &lasttface->mode, 0, 0, 0, 0, "Use face for collision detection");

		uiDefButS(block, TOG|BIT|6, REDRAWVIEW3D, "Shared",	600,140,60,19, &lasttface->mode, 0, 0, 0, 0, "Blend vertex colours across face when vertices are shared");
		uiDefButS(block, TOG|BIT|9, REDRAWVIEW3D, "Twoside",	660,140,60,19, &lasttface->mode, 0, 0, 0, 0, "Render face twosided");
		uiDefButS(block, TOG|BIT|11, REDRAWVIEW3D, "ObColor",720,140,60,19, &lasttface->mode, 0, 0, 0, 0, "Use ObColor instead of vertex colours");

		uiDefButS(block, TOG|BIT|8, B_TFACE_HALO, "Halo",	600,120,60,19, &lasttface->mode, 0, 0, 0, 0, "Screen aligned billboard");
		uiDefButS(block, TOG|BIT|12, B_TFACE_BILLB, "Billboard",660,120,60,19, &lasttface->mode, 0, 0, 0, 0, "Billboard with Z-axis constraint");
		uiDefButS(block, TOG|BIT|13, REDRAWVIEW3D, "Shadow", 720,120,60,19, &lasttface->mode, 0, 0, 0, 0, "Face is used for shadow");
		uiDefButS(block, TOG|BIT|14, REDRAWVIEW3D, "Text", 780,120,60,19, &lasttface->mode, 0, 0, 0, 0, "Enable bitmap text on face");

		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButC(block, ROW, REDRAWVIEW3D, "Opaque",	600,100,60,19, &lasttface->transp, 2.0, 0.0, 0, 0, "Render colour of textured face as colour");
		uiDefButC(block, ROW, REDRAWVIEW3D, "Add",		660,100,60,19, &lasttface->transp, 2.0, 1.0, 0, 0, "Render face transparent and add colour of face");
		uiDefButC(block, ROW, REDRAWVIEW3D, "Alpha",		720,100,60,19, &lasttface->transp, 2.0, 2.0, 0, 0, "Render polygon transparent, depending on alpha channel of the texture");
		/* uiDefButC(block, ROW, REDRAWVIEW3D, "Sub",	780,100,60,19, &lasttface->transp, 2.0, 3.0, 0, 0); ,""*/
		uiBlockSetCol(block, TH_AUTO);
		
		uiDefBut(block, BUT, B_COPY_TF_MODE, "Copy DrawMode", 600,7,117,28, 0, 0, 0, 0, 0, "Copy the drawmode");
		uiDefBut(block, BUT, B_COPY_TF_UV, "Copy UV+tex",	  721,7,85,28, 0, 0, 0, 0, 0, "Copy UV information and textures");
		uiDefBut(block, BUT, B_COPY_TF_COL, "Copy VertCol",	  809,7,103,28, 0, 0, 0, 0, 0, "Copy vertex colours");
	}

}


/* this is a mode context sensitive system */

void editing_panels()
{
	Object *ob;
	Curve *cu;
	MetaBall *mb;
	Lattice *lt;
	bArmature *arm;
	Camera *cam;
	
	ob= OBACT;
	if(ob==NULL) return;
	
	switch(ob->type) {
	case OB_MESH:
		editing_panel_links(ob); // no editmode!
		editing_panel_mesh_type(ob, ob->data);	// no editmode!
		/* modes */
		if(G.obedit) {
			editing_panel_mesh_tools(ob, ob->data); // no editmode!
			editing_panel_mesh_tools1(ob, ob->data); // no editmode!
		}
		else {
			if(G.f & G_FACESELECT)
				editing_panel_mesh_texface();
			
			if(G.f & (G_VERTEXPAINT | G_TEXTUREPAINT | G_WEIGHTPAINT) )
				editing_panel_mesh_paint();
		}
		break;
		
	case OB_CURVE:
	case OB_SURF:
		cu= ob->data;
		editing_panel_links(ob); // no editmode!
		editing_panel_curve_type(ob, cu);
		if(G.obedit) {
			editing_panel_curve_tools(ob, cu);
			editing_panel_curve_tools1(ob, cu);
		}
		break;

	case OB_MBALL:
		mb= ob->data;
		editing_panel_links(ob); // no editmode!
		editing_panel_mball_type(ob, mb);
		if(G.obedit) {
			editing_panel_mball_tools(ob, mb);
		}
		break;

	case OB_FONT:
		cu= ob->data;
		editing_panel_links(ob); // no editmode!
		editing_panel_curve_type(ob, cu);
		editing_panel_font_type(ob, cu);
		break;

	case OB_LATTICE:
		lt= ob->data;
		editing_panel_links(ob); // no editmode!
		editing_panel_lattice_type(ob, lt);
		break;

	case OB_LAMP:
		editing_panel_links(ob); // no editmode!
		break;
		
	case OB_EMPTY:
		editing_panel_links(ob); // no editmode!
		break;

	case OB_CAMERA:
		cam= ob->data;
		editing_panel_links(ob); // no editmode!
		editing_panel_camera_type(ob, cam);
		break;
		
	case OB_ARMATURE:
		arm= ob->data;
		editing_panel_links(ob); // no editmode!
		editing_panel_armature_type(ob, arm);
		if(G.obedit) {
			editing_panel_armature_bones(ob, arm);
		}
		break;
	}

}
