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
 * Contributor(s): Blender Foundation 2009.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_utildefines.h"

#include "ED_screen.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

void ui_template_fix_linking()
{
}

/********************** Header Template *************************/

void uiTemplateHeader(uiLayout *layout, bContext *C)
{
	uiBlock *block;
	
	block= uiLayoutFreeBlock(layout);
	ED_area_header_standardbuttons(C, block, 0);
}

/******************* Header ID Template ************************/

typedef struct TemplateHeaderID {
	PointerRNA ptr;
	PropertyRNA *prop;

	int flag;
	short browse;

	char newop[256];
	char openop[256];
	char unlinkop[256];
} TemplateHeaderID;

static void template_header_id_cb(bContext *C, void *arg_litem, void *arg_event)
{
	TemplateHeaderID *template= (TemplateHeaderID*)arg_litem;
	PointerRNA idptr= RNA_property_pointer_get(&template->ptr, template->prop);
	ID *idtest, *id= idptr.data;
	ListBase *lb= wich_libbase(CTX_data_main(C), ID_TXT);
	int nr, event= GET_INT_FROM_POINTER(arg_event);
	
	if(event == UI_ID_BROWSE && template->browse == 32767)
		event= UI_ID_ADD_NEW;
	else if(event == UI_ID_BROWSE && template->browse == 32766)
		event= UI_ID_OPEN;

	switch(event) {
		case UI_ID_BROWSE: {
			if(template->browse== -2) {
				/* XXX implement or find a replacement
				 * activate_databrowse((ID *)G.buts->lockpoin, GS(id->name), 0, B_MESHBROWSE, &template->browse, do_global_buttons); */
				return;
			}
			if(template->browse < 0)
				return;

			for(idtest=lb->first, nr=1; idtest; idtest=idtest->next, nr++) {
				if(nr==template->browse) {
					if(id == idtest)
						return;

					id= idtest;
					RNA_id_pointer_create(id, &idptr);
					RNA_property_pointer_set(&template->ptr, template->prop, idptr);
					RNA_property_update(C, &template->ptr, template->prop);
					/* XXX */

					break;
				}
			}
			break;
		}
#if 0
		case UI_ID_DELETE:
			id= NULL;
			break;
		case UI_ID_FAKE_USER:
			if(id) {
				if(id->flag & LIB_FAKEUSER) id->us++;
				else id->us--;
			}
			else return;
			break;
#endif
		case UI_ID_PIN:
			break;
		case UI_ID_ADD_NEW:
			WM_operator_name_call(C, template->newop, WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case UI_ID_OPEN:
			WM_operator_name_call(C, template->openop, WM_OP_INVOKE_REGION_WIN, NULL);
			break;
#if 0
		case UI_ID_ALONE:
			if(!id || id->us < 1)
				return;
			break;
		case UI_ID_LOCAL:
			if(!id || id->us < 1)
				return;
			break;
		case UI_ID_AUTO_NAME:
			break;
#endif
	}
}

static void template_header_ID(bContext *C, uiBlock *block, TemplateHeaderID *template)
{
	uiBut *but;
	TemplateHeaderID *duptemplate;
	PointerRNA idptr;
	ListBase *lb;
	int x= 0, y= 0;

	idptr= RNA_property_pointer_get(&template->ptr, template->prop);
	lb= wich_libbase(CTX_data_main(C), ID_TXT);

	uiBlockBeginAlign(block);
	if(template->flag & UI_ID_BROWSE) {
		char *extrastr, *str;
		
		if((template->flag & UI_ID_ADD_NEW) && (template->flag && UI_ID_OPEN))
			extrastr= "OPEN NEW %x 32766 |ADD NEW %x 32767";
		else if(template->flag & UI_ID_ADD_NEW)
			extrastr= "ADD NEW %x 32767";
		else if(template->flag & UI_ID_OPEN)
			extrastr= "OPEN NEW %x 32766";
		else
			extrastr= NULL;

		duptemplate= MEM_dupallocN(template);
		IDnames_to_pupstring(&str, NULL, extrastr, lb, idptr.data, &duptemplate->browse);

		but= uiDefButS(block, MENU, 0, str, x, y, UI_UNIT_X, UI_UNIT_Y, &duptemplate->browse, 0, 0, 0, 0, "Browse existing choices, or add new");
		uiButSetNFunc(but, template_header_id_cb, duptemplate, SET_INT_IN_POINTER(UI_ID_BROWSE));
		x+= UI_UNIT_X;
	
		MEM_freeN(str);
	}

	/* text button with name */
	if(idptr.data) {
		char name[64];

		text_idbutton(idptr.data, name);
		but= uiDefButR(block, TEX, 0, name, x, y, UI_UNIT_X*6, UI_UNIT_Y, &idptr, "name", -1, 0, 0, -1, -1, NULL);
		uiButSetNFunc(but, template_header_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_RENAME));
		x += UI_UNIT_X*6;

		/* delete button */
		if(template->flag & UI_ID_DELETE) {
			but= uiDefIconButO(block, BUT, template->unlinkop, WM_OP_EXEC_REGION_WIN, ICON_X, x, y, UI_UNIT_X, UI_UNIT_Y, NULL);
			x += UI_UNIT_X;
		}
	}
	uiBlockEndAlign(block);
}

void uiTemplateHeaderID(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop)
{
	TemplateHeaderID *template;
	uiBlock *block;
	PropertyRNA *prop;

	if(!ptr->data)
		return;

	prop= RNA_struct_find_property(ptr, propname);

	if(!prop) {
		printf("uiTemplateHeaderID: property not found: %s\n", propname);
		return;
	}

	template= MEM_callocN(sizeof(TemplateHeaderID), "TemplateHeaderID");
	template->ptr= *ptr;
	template->prop= prop;
	template->flag= UI_ID_BROWSE|UI_ID_RENAME;

	if(newop) {
		template->flag |= UI_ID_ADD_NEW;
		BLI_strncpy(template->newop, newop, sizeof(template->newop));
	}
	if(openop) {
		template->flag |= UI_ID_OPEN;
		BLI_strncpy(template->openop, openop, sizeof(template->openop));
	}
	if(unlinkop) {
		template->flag |= UI_ID_DELETE;
		BLI_strncpy(template->unlinkop, unlinkop, sizeof(template->unlinkop));
	}

	block= uiLayoutFreeBlock(layout);
	template_header_ID(C, block, template);

	MEM_freeN(template);
}

/************************ Modifier Template *************************/

#define ERROR_LIBDATA_MESSAGE "Can't edit external libdata"

#define B_NOP				0
#define B_MODIFIER_RECALC	1
#define B_MODIFIER_REDRAW	2
#define B_CHANGEDEP			3
#define B_ARM_RECALCDATA	4

#include <string.h>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_bmesh.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_report.h"

#include "UI_resources.h"
#include "ED_util.h"

#include "BLI_arithb.h"
#include "BLI_listbase.h"

#include "ED_object.h"

void do_modifier_panels(bContext *C, void *arg, int event)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);

	switch(event) {
	case B_MODIFIER_REDRAW:
		WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
		break;

	case B_MODIFIER_RECALC:
		WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
		DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
		object_handle_update(scene, ob);
		// XXX countall();
		break;
	}
}

static void modifiers_del(bContext *C, void *ob_v, void *md_v)
{
	ReportList reports;

	BKE_reports_init(&reports, RPT_STORE);

	if(ED_object_modifier_delete(&reports, ob_v, md_v))
		ED_undo_push(C, "Delete modifier");
	else
		uiPupMenuReports(C, &reports);

	BKE_reports_clear(&reports);
}

static void modifiers_moveUp(bContext *C, void *ob_v, void *md_v)
{
	ReportList reports;

	BKE_reports_init(&reports, RPT_STORE);

	if(ED_object_modifier_move_up(&reports, ob_v, md_v))
		ED_undo_push(C, "Move modifier");
	else
		uiPupMenuReports(C, &reports);

	BKE_reports_clear(&reports);
}

static void modifiers_moveDown(bContext *C, void *ob_v, void *md_v)
{
	ReportList reports;

	BKE_reports_init(&reports, RPT_STORE);

	if(ED_object_modifier_move_down(&reports, ob_v, md_v))
		ED_undo_push(C, "Move modifier");
	else
		uiPupMenuReports(C, &reports);

	BKE_reports_clear(&reports);
}

static void modifiers_convertParticles(bContext *C, void *obv, void *mdv)
{
	Scene *scene= CTX_data_scene(C);
	ReportList reports;

	BKE_reports_init(&reports, RPT_STORE);

	if(ED_object_modifier_convert(&reports, scene, obv, mdv))
		ED_undo_push(C, "Convert particles to mesh object(s).");
	else
		uiPupMenuReports(C, &reports);

	BKE_reports_clear(&reports);
}

static void modifiers_applyModifier(bContext *C, void *obv, void *mdv)
{
	Scene *scene= CTX_data_scene(C);
	ReportList reports;

	BKE_reports_init(&reports, RPT_STORE);

	if(ED_object_modifier_apply(&reports, scene, obv, mdv))
		ED_undo_push(C, "Apply modifier");
	else
		uiPupMenuReports(C, &reports);

	BKE_reports_clear(&reports);
}

static void modifiers_copyModifier(bContext *C, void *ob_v, void *md_v)
{
	ReportList reports;

	BKE_reports_init(&reports, RPT_STORE);

	if(ED_object_modifier_copy(&reports, ob_v, md_v))
		ED_undo_push(C, "Copy modifier");
	else
		uiPupMenuReports(C, &reports);

	BKE_reports_clear(&reports);
}

static void modifiers_setOnCage(bContext *C, void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md;
	
	int i, cageIndex = modifiers_getCageIndex(ob, NULL );

	for( i = 0, md=ob->modifiers.first; md; ++i, md=md->next )
		if( md == md_v ) {
			if( i >= cageIndex )
				md->mode ^= eModifierMode_OnCage;
			break;
		}
}

static void modifiers_convertToReal(bContext *C, void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;
	ModifierData *nmd = modifier_new(md->type);

	modifier_copyData(md, nmd);
	nmd->mode &= ~eModifierMode_Virtual;

	BLI_addhead(&ob->modifiers, nmd);

	ob->partype = PAROBJECT;

	ED_undo_push(C, "Modifier convert to real");
}

#if 0
static void modifiers_clearHookOffset(bContext *C, void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;
	HookModifierData *hmd = (HookModifierData*) md;
	
	if (hmd->object) {
		Mat4Invert(hmd->object->imat, hmd->object->obmat);
		Mat4MulSerie(hmd->parentinv, hmd->object->imat, ob->obmat, NULL, NULL, NULL, NULL, NULL, NULL);
		ED_undo_push(C, "Clear hook offset");
	}
}

static void modifiers_cursorHookCenter(bContext *C, void *ob_v, void *md_v)
{
	/* XXX 
	Object *ob = ob_v;
	ModifierData *md = md_v;
	HookModifierData *hmd = (HookModifierData*) md;

	if(G.vd) {
		float *curs = give_cursor();
		float bmat[3][3], imat[3][3];

		where_is_object(ob);
	
		Mat3CpyMat4(bmat, ob->obmat);
		Mat3Inv(imat, bmat);

		curs= give_cursor();
		hmd->cent[0]= curs[0]-ob->obmat[3][0];
		hmd->cent[1]= curs[1]-ob->obmat[3][1];
		hmd->cent[2]= curs[2]-ob->obmat[3][2];
		Mat3MulVecfl(imat, hmd->cent);

		ED_undo_push(C, "Hook cursor center");
	}*/
}

static void modifiers_selectHook(bContext *C, void *ob_v, void *md_v)
{
	/* XXX ModifierData *md = md_v;
	HookModifierData *hmd = (HookModifierData*) md;

	hook_select(hmd);*/
}

static void modifiers_reassignHook(bContext *C, void *ob_v, void *md_v)
{
	/* XXX ModifierData *md = md_v;
	HookModifierData *hmd = (HookModifierData*) md;
	float cent[3];
	int *indexar, tot, ok;
	char name[32];
		
	ok= hook_getIndexArray(&tot, &indexar, name, cent);

	if (!ok) {
		uiPupMenuError(C, "Requires selected vertices or active Vertex Group");
	} else {
		if (hmd->indexar) {
			MEM_freeN(hmd->indexar);
		}

		VECCOPY(hmd->cent, cent);
		hmd->indexar = indexar;
		hmd->totindex = tot;
	}*/
}

static void modifiers_bindMeshDeform(bContext *C, void *ob_v, void *md_v)
{
	Scene *scene= CTX_data_scene(C);
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md_v;
	Object *ob = (Object*)ob_v;

	if(mmd->bindcos) {
		if(mmd->bindweights) MEM_freeN(mmd->bindweights);
		if(mmd->bindcos) MEM_freeN(mmd->bindcos);
		if(mmd->dyngrid) MEM_freeN(mmd->dyngrid);
		if(mmd->dyninfluences) MEM_freeN(mmd->dyninfluences);
		if(mmd->dynverts) MEM_freeN(mmd->dynverts);
		mmd->bindweights= NULL;
		mmd->bindcos= NULL;
		mmd->dyngrid= NULL;
		mmd->dyninfluences= NULL;
		mmd->dynverts= NULL;
		mmd->totvert= 0;
		mmd->totcagevert= 0;
		mmd->totinfluence= 0;
	}
	else {
		DerivedMesh *dm;
		int mode= mmd->modifier.mode;

		/* force modifier to run, it will call binding routine */
		mmd->needbind= 1;
		mmd->modifier.mode |= eModifierMode_Realtime;

		if(ob->type == OB_MESH) {
			dm= mesh_create_derived_view(scene, ob, 0);
			dm->release(dm);
		}
		else if(ob->type == OB_LATTICE) {
			lattice_calc_modifiers(scene, ob);
		}
		else if(ob->type==OB_MBALL) {
			makeDispListMBall(scene, ob);
		}
		else if(ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
			makeDispListCurveTypes(scene, ob, 0);
		}

		mmd->needbind= 0;
		mmd->modifier.mode= mode;
	}
}

void modifiers_explodeFacepa(bContext *C, void *arg1, void *arg2)
{
	ExplodeModifierData *emd=arg1;

	emd->flag |= eExplodeFlag_CalcFaces;
}

void modifiers_explodeDelVg(bContext *C, void *arg1, void *arg2)
{
	ExplodeModifierData *emd=arg1;
	emd->vgroup = 0;
}
#endif

static int modifier_is_fluid_particles(ModifierData *md)
{
	if(md->type == eModifierType_ParticleSystem) {
		if(((ParticleSystemModifierData *)md)->psys->part->type == PART_FLUID)
			return 1;
	}
	return 0;
}

static uiLayout *draw_modifier(uiLayout *layout, Object *ob, ModifierData *md, int index, int cageIndex, int lastCageIndex)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	uiBut *but;
	uiBlock *block;
	uiLayout *column, *row, *result= NULL;
	int isVirtual = md->mode&eModifierMode_Virtual;
	int x = 0, y = 0; // XXX , color = md->error?TH_REDALERT:TH_BUT_NEUTRAL;
	short width = 295, buttonWidth = width-120-10;
	char str[128];

	column= uiLayoutColumn(layout, 1);

	/* rounded header */
	/* XXX uiBlockSetCol(block, color); */
		/* roundbox 4 free variables: corner-rounding, nop, roundbox type, shade */
	block= uiLayoutFreeBlock(uiLayoutBox(column));
	uiBlockSetHandleFunc(block, do_modifier_panels, NULL);

	//uiDefBut(block, ROUNDBOX, 0, "", x-10, y-4, width, 25, NULL, 7.0, 0.0, 
	//		 (!isVirtual && (md->mode&eModifierMode_Expanded))?3:15, 20, ""); 
	/* XXX uiBlockSetCol(block, TH_AUTO); */
	
	/* open/close icon */
	if (!isVirtual) {
		uiBlockSetEmboss(block, UI_EMBOSSN);
		uiDefIconButBitI(block, ICONTOG, eModifierMode_Expanded, B_MODIFIER_REDRAW, ICON_TRIA_RIGHT, x-10, y-2, 20, 20, &md->mode, 0.0, 0.0, 0.0, 0.0, "Collapse/Expand Modifier");
	}

	uiBlockSetEmboss(block, UI_EMBOSS);
	
	if (isVirtual) {
		sprintf(str, "%s parent deform", md->name);
		uiDefBut(block, LABEL, 0, str, x+10, y-1, width-110, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Modifier name"); 

		but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Make Real", x+width-100, y, 80, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Convert virtual modifier to a real modifier");
		uiButSetFunc(but, modifiers_convertToReal, ob, md);
	} else {
		uiBlockBeginAlign(block);
		uiDefBut(block, TEX, B_MODIFIER_REDRAW, "", x+10, y-1, buttonWidth-60, 19, md->name, 0.0, sizeof(md->name)-1, 0.0, 0.0, "Modifier name"); 

		/* Softbody not allowed in this situation, enforce! */
		if (((md->type!=eModifierType_Softbody && md->type!=eModifierType_Collision) || !(ob->pd && ob->pd->deflect)) && (md->type!=eModifierType_Surface)) {
			uiDefIconButBitI(block, TOG, eModifierMode_Render, B_MODIFIER_RECALC, ICON_SCENE, x+10+buttonWidth-60, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during rendering");
			but= uiDefIconButBitI(block, TOG, eModifierMode_Realtime, B_MODIFIER_RECALC, ICON_VIEW3D, x+10+buttonWidth-40, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during interactive display");
			if (mti->flags&eModifierTypeFlag_SupportsEditmode) {
				uiDefIconButBitI(block, TOG, eModifierMode_Editmode, B_MODIFIER_RECALC, ICON_EDITMODE_HLT, x+10+buttonWidth-20, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during Editmode (only if enabled for display)");
			}
		}
		uiBlockEndAlign(block);

		/* XXX uiBlockSetEmboss(block, UI_EMBOSSR); */

		if (ob->type==OB_MESH && modifier_couldBeCage(md) && index<=lastCageIndex) {
			int icon; //, color;

			if (index==cageIndex) {
				// XXX color = TH_BUT_SETTING;
				icon = VICON_EDITMODE_HLT;
			} else if (index<cageIndex) {
				// XXX color = TH_BUT_NEUTRAL;
				icon = VICON_EDITMODE_DEHLT;
			} else {
				// XXX color = TH_BUT_NEUTRAL;
				icon = ICON_BLANK1;
			}
			/* XXX uiBlockSetCol(block, color); */
			but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, icon, x+width-105, y, 16, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Apply modifier to editing cage during Editmode");
			uiButSetFunc(but, modifiers_setOnCage, ob, md);
			/* XXX uiBlockSetCol(block, TH_AUTO); */
		}

		/* XXX uiBlockSetCol(block, TH_BUT_ACTION); */

		but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, VICON_MOVE_UP, x+width-75, y, 16, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Move modifier up in stack");
		uiButSetFunc(but, modifiers_moveUp, ob, md);

		but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, VICON_MOVE_DOWN, x+width-75+20, y, 16, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Move modifier down in stack");
		uiButSetFunc(but, modifiers_moveDown, ob, md);
		
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		// deletion over the deflection panel
		// fluid particle modifier can't be deleted here
		if(md->type!=eModifierType_Fluidsim && md->type!=eModifierType_Collision && md->type!=eModifierType_Surface && !modifier_is_fluid_particles(md))
		{
			but = uiDefIconBut(block, BUT, B_MODIFIER_RECALC, VICON_X, x+width-70+40, y, 16, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Delete modifier");
			uiButSetFunc(but, modifiers_del, ob, md);
		}
		/* XXX uiBlockSetCol(block, TH_AUTO); */
	}

	uiBlockSetEmboss(block, UI_EMBOSS);

	if(!isVirtual && (md->mode&eModifierMode_Expanded)) {
		int cy = y - 8;
		int lx = x + width - 60 - 15;
		uiLayout *box;

		box= uiLayoutBox(column);
		row= uiLayoutRow(box, 1);

		y -= 18;

		if (!isVirtual && (md->type!=eModifierType_Collision) && (md->type!=eModifierType_Surface)) {
			uiBlockSetButLock(block, object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE); /* only here obdata, the rest of modifiers is ob level */

						if (md->type==eModifierType_ParticleSystem) {
		    	ParticleSystem *psys= ((ParticleSystemModifierData *)md)->psys;

	    		if(!(G.f & G_PARTICLEEDIT)) {
					if(ELEM3(psys->part->draw_as, PART_DRAW_PATH, PART_DRAW_GR, PART_DRAW_OB) && psys->pathcache) {
						but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Convert",	lx,(cy-=19),60,19, 0, 0, 0, 0, 0, "Convert the current particles to a mesh object");
						uiButSetFunc(but, modifiers_convertParticles, ob, md);
					}
				}
			}
			else{
				but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Apply",	lx,(cy-=19),60,19, 0, 0, 0, 0, 0, "Apply the current modifier and remove from the stack");
				uiButSetFunc(but, modifiers_applyModifier, ob, md);
			}
			
			uiBlockClearButLock(block);
			uiBlockSetButLock(block, ob && ob->id.lib, ERROR_LIBDATA_MESSAGE);

			if (md->type!=eModifierType_Fluidsim && md->type!=eModifierType_Softbody && md->type!=eModifierType_ParticleSystem && (md->type!=eModifierType_Cloth)) {
				but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Copy",	lx,(cy-=19),60,19, 0, 0, 0, 0, 0, "Duplicate the current modifier at the same position in the stack");
				uiButSetFunc(but, modifiers_copyModifier, ob, md);
			}
		}

		result= uiLayoutColumn(box, 0);
		block= uiLayoutFreeBlock(box);

		lx = x + 10;
		cy = y + 10 - 1;
		// else if (md->type==eModifierType_Surface) {
		//	uiDefBut(block, LABEL, 1, "See Fields panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
	}

	if (md->error) {

		row = uiLayoutRow(uiLayoutBox(column), 0);

		/* XXX uiBlockSetCol(block, color); */
		uiItemL(row, md->error, ICON_ERROR);
		/* XXX uiBlockSetCol(block, TH_AUTO); */
	}

	return result;
}

uiLayout *uiTemplateModifier(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob;
	ModifierData *md, *vmd;
	int i, lastCageIndex, cageIndex;

	/* verify we have valid data */
	if(!RNA_struct_is_a(ptr->type, &RNA_Modifier)) {
		printf("uiTemplateModifier: expected modifier on object.\n");
		return NULL;
	}

	ob= ptr->id.data;
	md= ptr->data;

	if(!ob || !(GS(ob->id.name) == ID_OB)) {
		printf("uiTemplateModifier: expected modifier on object.\n");
		return NULL;
	}
	
	uiBlockSetButLock(uiLayoutGetBlock(layout), (ob && ob->id.lib), ERROR_LIBDATA_MESSAGE);
	
	/* find modifier and draw it */
	cageIndex = modifiers_getCageIndex(ob, &lastCageIndex);

	// XXX virtual modifiers are not accesible for python
	vmd = modifiers_getVirtualModifierList(ob);

	for(i=0; vmd; i++, vmd=vmd->next) {
		if(md == vmd)
			return draw_modifier(layout, ob, md, i, cageIndex, lastCageIndex);
		else if(vmd->mode&eModifierMode_Virtual)
			i--;
	}

	return NULL;
}

/************************ Constraint Template *************************/

#include "DNA_action_types.h"
#include "DNA_constraint_types.h"

#include "BKE_action.h"
#include "BKE_constraint.h"

#define REDRAWIPO					1
#define REDRAWNLA					2
#define REDRAWBUTSOBJECT			3		
#define REDRAWACTION				4
#define B_CONSTRAINT_TEST			5
#define B_CONSTRAINT_CHANGETARGET	6
#define B_CONSTRAINT_INF			7
#define REMAKEIPO					8
#define B_DIFF						9

void do_constraint_panels(bContext *C, void *arg, int event)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	
	switch(event) {
	case B_CONSTRAINT_TEST:
		// XXX allqueue(REDRAWVIEW3D, 0);
		// XXX allqueue(REDRAWBUTSOBJECT, 0);
		// XXX allqueue(REDRAWBUTSEDIT, 0);
		break;  // no handling
	case B_CONSTRAINT_INF:
		/* influence; do not execute actions for 1 dag_flush */
		if (ob->pose)
			ob->pose->flag |= (POSE_LOCKED|POSE_DO_UNLOCK);
		break;
	case B_CONSTRAINT_CHANGETARGET:
		if (ob->pose) ob->pose->flag |= POSE_RECALC;	// checks & sorts pose channels
		DAG_scene_sort(scene);
		break;
	default:
		break;
	}

	object_test_constraints(ob);
	
	if(ob->pose) update_pose_constraint_flags(ob->pose);
	
	if(ob->type==OB_ARMATURE) DAG_object_flush_update(scene, ob, OB_RECALC_DATA|OB_RECALC_OB);
	else DAG_object_flush_update(scene, ob, OB_RECALC_OB);
	
	// XXX allqueue(REDRAWVIEW3D, 0);
	// XXX allqueue(REDRAWBUTSOBJECT, 0);
	// XXX allqueue(REDRAWBUTSEDIT, 0);
}

static void constraint_active_func(bContext *C, void *ob_v, void *con_v)
{
	ED_object_constraint_set_active(ob_v, con_v);
}

static void del_constraint_func (bContext *C, void *ob_v, void *con_v)
{
	if(ED_object_constraint_delete(NULL, ob_v, con_v))
		ED_undo_push(C, "Delete Constraint");
}

static void verify_constraint_name_func (bContext *C, void *con_v, void *name_v)
{
	Object *ob= CTX_data_active_object(C);
	bConstraint *con= con_v;
	char oldname[32];	
	
	if (!con)
		return;
	
	/* put on the stack */
	BLI_strncpy(oldname, (char *)name_v, 32);
	
	ED_object_constraint_rename(ob, con, oldname);
	ED_object_constraint_set_active(ob, con);
	// XXX allqueue(REDRAWACTION, 0); 
}

static void constraint_moveUp(bContext *C, void *ob_v, void *con_v)
{
	if(ED_object_constraint_move_up(NULL, ob_v, con_v))
		ED_undo_push(C, "Move Constraint");
}

static void constraint_moveDown(bContext *C, void *ob_v, void *con_v)
{
	if(ED_object_constraint_move_down(NULL, ob_v, con_v))
		ED_undo_push(C, "Move Constraint");
}

/* some commonly used macros in the constraints drawing code */
#define is_armature_target(target) (target && target->type==OB_ARMATURE)
#define is_armature_owner(ob) ((ob->type == OB_ARMATURE) && (ob->flag & OB_POSEMODE))
#define is_geom_target(target) (target && (ELEM(target->type, OB_MESH, OB_LATTICE)) )

/* Helper function for draw constraint - draws constraint space stuff 
 * This function should not be called if no menus are required 
 * owner/target: -1 = don't draw menu; 0= not posemode, 1 = posemode 
 */
static void draw_constraint_spaceselect (uiBlock *block, bConstraint *con, short xco, short yco, short owner, short target)
{
	short tarx, ownx, iconx;
	short bwidth;
	short iconwidth = 20;
	
	/* calculate sizes and placement of menus */
	if (owner == -1) {
		bwidth = 125;
		tarx = 120;
		ownx = 0;
	}
	else if (target == -1) {
		bwidth = 125;
		tarx = 0;
		ownx = 120;
	}
	else {
		bwidth = 100;
		tarx = 85;
		iconx = tarx + bwidth + 5;
		ownx = tarx + bwidth + iconwidth + 10;
	}
	
	
	uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Convert:", xco, yco, 80,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

	/* Target-Space */
	if (target == 1) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Pose Space %x2|Local with Parent %x3|Local Space %x1", 
												tarx, yco, bwidth, 18, &con->tarspace, 0, 0, 0, 0, "Choose space that target is evaluated in");	
	}
	else if (target == 0) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Local (Without Parent) Space %x1", 
										tarx, yco, bwidth, 18, &con->tarspace, 0, 0, 0, 0, "Choose space that target is evaluated in");	
	}
	
	if ((target != -1) && (owner != -1))
		uiDefIconBut(block, LABEL, B_NOP, ICON_ARROW_LEFTRIGHT,
			iconx, yco, 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "");
	
	/* Owner-Space */
	if (owner == 1) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Owner Space %t|World Space %x0|Pose Space %x2|Local with Parent %x3|Local Space %x1", 
												ownx, yco, bwidth, 18, &con->ownspace, 0, 0, 0, 0, "Choose space that owner is evaluated in");	
	}
	else if (owner == 0) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Owner Space %t|World Space %x0|Local (Without Parent) Space %x1", 
										ownx, yco, bwidth, 18, &con->ownspace, 0, 0, 0, 0, "Choose space that owner is evaluated in");	
	}
}

/* draw panel showing settings for a constraint */
static uiLayout *draw_constraint(uiLayout *layout, Object *ob, bConstraint *con)
{
	bPoseChannel *pchan= get_active_posechannel(ob);
	bConstraintTypeInfo *cti;
	uiBlock *block;
	uiLayout *result= NULL, *col, *box;
	uiBut *but;
	char typestr[32];
	short width = 265;
	short proxy_protected, xco=0, yco=0;
	int rb_col;

	/* get constraint typeinfo */
	cti= constraint_get_typeinfo(con);
	if (cti == NULL) {
		/* exception for 'Null' constraint - it doesn't have constraint typeinfo! */
		if (con->type == CONSTRAINT_TYPE_NULL)
			strcpy(typestr, "Null");
		else
			strcpy(typestr, "Unknown");
	}
	else
		strcpy(typestr, cti->name);
		
	/* determine whether constraint is proxy protected or not */
	if (proxylocked_constraints_owner(ob, pchan))
		proxy_protected= (con->flag & CONSTRAINT_PROXY_LOCAL)==0;
	else
		proxy_protected= 0;

	/* unless button has own callback, it adds this callback to button */
	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_constraint_panels, NULL);
	uiBlockSetFunc(block, constraint_active_func, ob, con);

	col= uiLayoutColumn(layout, 1);
	box= uiLayoutBox(col);

	block= uiLayoutFreeBlock(box);
		
	/* Draw constraint header */
	uiBlockSetEmboss(block, UI_EMBOSSN);
	
	/* rounded header */
	rb_col= (con->flag & CONSTRAINT_ACTIVE)?50:20;
	
	/* open/close */
	uiDefIconButBitS(block, ICONTOG, CONSTRAINT_EXPAND, B_CONSTRAINT_TEST, ICON_TRIA_RIGHT, xco-10, yco, 20, 20, &con->flag, 0.0, 0.0, 0.0, 0.0, "Collapse/Expand Constraint");
	
	/* name */	
	if ((con->flag & CONSTRAINT_EXPAND) && (proxy_protected==0)) {
		/* XXX if (con->flag & CONSTRAINT_DISABLE)
			uiBlockSetCol(block, TH_REDALERT);*/
		
		uiBlockSetEmboss(block, UI_EMBOSS);
		
		uiDefBut(block, LABEL, B_CONSTRAINT_TEST, typestr, xco+10, yco, 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
		
		but = uiDefBut(block, TEX, B_CONSTRAINT_TEST, "", xco+120, yco, 85, 18, con->name, 0.0, 29.0, 0.0, 0.0, "Constraint name"); 
		uiButSetFunc(but, verify_constraint_name_func, con, NULL);
	}	
	else {
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		/* XXX if (con->flag & CONSTRAINT_DISABLE)
			uiBlockSetCol(block, TH_REDALERT);*/
		
		uiDefBut(block, LABEL, B_CONSTRAINT_TEST, typestr, xco+10, yco, 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
		
		uiDefBut(block, LABEL, B_CONSTRAINT_TEST, con->name, xco+120, yco-1, 135, 19, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
	}

	// XXX uiBlockSetCol(block, TH_AUTO);	
	
	/* proxy-protected constraints cannot be edited, so hide up/down + close buttons */
	if (proxy_protected) {
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		/* draw a ghost icon (for proxy) and also a lock beside it, to show that constraint is "proxy locked" */
		uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, ICON_GHOST, xco+244, yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Proxy Protected");
		uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, ICON_LOCKED, xco+262, yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Proxy Protected");
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	else {
		short prev_proxylock, show_upbut, show_downbut;
		
		/* Up/Down buttons: 
		 *	Proxy-constraints are not allowed to occur after local (non-proxy) constraints
		 *	as that poses problems when restoring them, so disable the "up" button where
		 *	it may cause this situation. 
		 *
		 * 	Up/Down buttons should only be shown (or not greyed - todo) if they serve some purpose. 
		 */
		if (proxylocked_constraints_owner(ob, pchan)) {
			if (con->prev) {
				prev_proxylock= (con->prev->flag & CONSTRAINT_PROXY_LOCAL) ? 0 : 1;
			}
			else
				prev_proxylock= 0;
		}
		else
			prev_proxylock= 0;
			
		show_upbut= ((prev_proxylock == 0) && (con->prev));
		show_downbut= (con->next) ? 1 : 0;
		
		if (show_upbut || show_downbut) {
			uiBlockBeginAlign(block);
				uiBlockSetEmboss(block, UI_EMBOSS);
				
				if (show_upbut) {
					but = uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, VICON_MOVE_UP, xco+width-50, yco, 16, 18, NULL, 0.0, 0.0, 0.0, 0.0, "Move constraint up in constraint stack");
					uiButSetFunc(but, constraint_moveUp, ob, con);
				}
				
				if (show_downbut) {
					but = uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, VICON_MOVE_DOWN, xco+width-50+18, yco, 16, 18, NULL, 0.0, 0.0, 0.0, 0.0, "Move constraint down in constraint stack");
					uiButSetFunc(but, constraint_moveDown, ob, con);
				}
			uiBlockEndAlign(block);
		}
		
		
		/* Close 'button' - emboss calls here disable drawing of 'button' behind X */
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		but = uiDefIconBut(block, BUT, B_CONSTRAINT_CHANGETARGET, ICON_X, xco+262, yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Delete constraint");
		uiButSetFunc(but, del_constraint_func, ob, con);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	
	/* Set but-locks for protected settings (magic numbers are used here!) */
	if (proxy_protected)
		uiBlockSetButLock(block, 1, "Cannot edit Proxy-Protected Constraint");
	
	/* Draw constraint data */
	if ((con->flag & CONSTRAINT_EXPAND) == 0) {
		(yco) -= 21;
	}
	else {
		box= uiLayoutBox(col);
		block= uiLayoutFreeBlock(box);

		switch (con->type) {
#ifndef DISABLE_PYTHON
		case CONSTRAINT_TYPE_PYTHON:
			{
				bPythonConstraint *data = con->data;
				bConstraintTarget *ct;
				// uiBut *but2;
				int tarnum, theight;
				// static int pyconindex=0;
				// char *menustr;
				
				theight = (data->tarnum)? (data->tarnum * 38) : (38);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Script:", xco+60, yco-24, 55, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* do the scripts menu */
				/* XXX menustr = buildmenu_pyconstraints(data->text, &pyconindex);
				but2 = uiDefButI(block, MENU, B_CONSTRAINT_TEST, menustr,
				      xco+120, yco-24, 150, 20, &pyconindex,
				      0, 0, 0, 0, "Set the Script Constraint to use");
				uiButSetFunc(but2, validate_pyconstraint_cb, data, &pyconindex);
				MEM_freeN(menustr);	 */
				
				/* draw target(s) */
				if (data->flag & PYCON_USETARGETS) {
					/* Draw target parameters */ 
					for (ct=data->targets.first, tarnum=1; ct; ct=ct->next, tarnum++) {
						char tarstr[32];
						short yoffset= ((tarnum-1) * 38);
	
						/* target label */
						sprintf(tarstr, "Target %d:", tarnum);
						uiDefBut(block, LABEL, B_CONSTRAINT_TEST, tarstr, xco+45, yco-(48+yoffset), 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
						
						/* target space-selector - per target */
						if (is_armature_target(ct->tar)) {
							uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Pose Space %x3|Local with Parent %x4|Local Space %x1", 
															xco+10, yco-(66+yoffset), 100, 18, &ct->space, 0, 0, 0, 0, "Choose space that target is evaluated in");	
						}
						else {
							uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Local (Without Parent) Space %x1", 
															xco+10, yco-(66+yoffset), 100, 18, &ct->space, 0, 0, 0, 0, "Choose space that target is evaluated in");	
						}
						
						uiBlockBeginAlign(block);
							/* target object */
							uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-(48+yoffset), 150, 18, &ct->tar, "Target Object"); 
							
							/* subtarget */
							if (is_armature_target(ct->tar)) {
								but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-(66+yoffset),150,18, &ct->subtarget, 0, 24, 0, 0, "Subtarget Bone");
								uiButSetCompleteFunc(but, autocomplete_bone, (void *)ct->tar);
							}
							else if (is_geom_target(ct->tar)) {
								but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-(66+yoffset),150,18, &ct->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
								uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ct->tar);
							}
							else {
								strcpy(ct->subtarget, "");
							}
						uiBlockEndAlign(block);
					}
				}
				else {
					/* Draw indication that no target needed */
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+60, yco-48, 55, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Not Applicable", xco+120, yco-48, 150, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				}
				
				/* settings */
				uiBlockBeginAlign(block);
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Options", xco, yco-(52+theight), (width/2),18, NULL, 0, 24, 0, 0, "Change some of the constraint's settings.");
					// XXX uiButSetFunc(but, BPY_pyconstraint_settings, data, NULL);
					
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Refresh", xco+((width/2)+10), yco-(52+theight), (width/2),18, NULL, 0, 24, 0, 0, "Force constraint to refresh it's settings");
				uiBlockEndAlign(block);
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, xco, yco-(73+theight), is_armature_owner(ob), -1);
			}
			break;
#endif /* DISABLE_PYTHON */
		case CONSTRAINT_TYPE_ACTION:
			{
				bActionConstraint *data = con->data;
				float minval, maxval;
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				
				uiBlockEndAlign(block);
				
				/* Draw action/type buttons */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_actionpoin_but, ID_AC, B_CONSTRAINT_TEST, "AC:",	xco+((width/2)-117), yco-64, 78, 18, &data->act, "Action containing the keyed motion for this bone"); 
					uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Key on%t|Loc X%x20|Loc Y%x21|Loc Z%x22|Rot X%x0|Rot Y%x1|Rot Z%x2|Size X%x10|Size Y%x11|Size Z%x12", xco+((width/2)-117), yco-84, 78, 18, &data->type, 0, 24, 0, 0, "Specify which transformation channel from the target is used to key the action");
				uiBlockEndAlign(block);
				
				/* Draw start/end frame buttons */
				uiBlockBeginAlign(block);
					uiDefButI(block, NUM, B_CONSTRAINT_TEST, "Start:", xco+((width/2)-36), yco-64, 78, 18, &data->start, 1, MAXFRAME, 0.0, 0.0, "Starting frame of the keyed motion"); 
					uiDefButI(block, NUM, B_CONSTRAINT_TEST, "End:", xco+((width/2)-36), yco-84, 78, 18, &data->end, 1, MAXFRAME, 0.0, 0.0, "Ending frame of the keyed motion"); 
				uiBlockEndAlign(block);
				
				/* Draw minimum/maximum transform range buttons */
				uiBlockBeginAlign(block);
					if (data->type < 10) { /* rotation */
						minval = -180.0f;
						maxval = 180.0f;
					}
					else if (data->type < 20) { /* scaling */
						minval = 0.0001f;
						maxval = 1000.0f;
					}
					else { /* location */
						minval = -1000.0f;
						maxval = 1000.0f;
					}
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Min:", xco+((width/2)+45), yco-64, 78, 18, &data->min, minval, maxval, 0, 0, "Minimum value for target channel range");
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Max:", xco+((width/2)+45), yco-84, 78, 18, &data->max, minval, maxval, 0, 0, "Maximum value for target channel range");
				uiBlockEndAlign(block);
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, xco, yco-104, -1, is_armature_target(data->tar));
			}
			break;
		case CONSTRAINT_TYPE_CHILDOF:
			{
				bChildOfConstraint *data = con->data;
				short normButWidth = (width/3);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Parent:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object to use as Parent"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone to use as Parent");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				/* Draw triples of channel toggles */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Use Channel(s):", xco+65, yco-64, 150, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				uiBlockBeginAlign(block); 
					uiDefButBitI(block, TOG, CHILDOF_LOCX, B_CONSTRAINT_TEST, "Loc X", xco, yco-84, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects x-location"); 
					uiDefButBitI(block, TOG, CHILDOF_LOCY, B_CONSTRAINT_TEST, "Loc Y", xco+normButWidth, yco-84, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects y-location"); 
					uiDefButBitI(block, TOG, CHILDOF_LOCZ, B_CONSTRAINT_TEST, "Loc Z", xco+(normButWidth * 2), yco-84, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects z-location"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitI(block, TOG, CHILDOF_ROTX, B_CONSTRAINT_TEST, "Rot X", xco, yco-105, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects x-rotation"); 
					uiDefButBitI(block, TOG, CHILDOF_ROTY, B_CONSTRAINT_TEST, "Rot Y", xco+normButWidth, yco-105, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects y-rotation"); 
					uiDefButBitI(block, TOG, CHILDOF_ROTZ, B_CONSTRAINT_TEST, "Rot Z", xco+(normButWidth * 2), yco-105, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects z-rotation"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitI(block, TOG, CHILDOF_SIZEX, B_CONSTRAINT_TEST, "Scale X", xco, yco-126, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects x-scaling"); 
					uiDefButBitI(block, TOG, CHILDOF_SIZEY, B_CONSTRAINT_TEST, "Scale Y", xco+normButWidth, yco-126, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects y-scaling"); 
					uiDefButBitI(block, TOG, CHILDOF_SIZEZ, B_CONSTRAINT_TEST, "Scale Z", xco+(normButWidth * 2), yco-126, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects z-scaling"); 
				uiBlockEndAlign(block);
				
				
				/* Inverse options */
				uiBlockBeginAlign(block);
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Set Offset", xco, yco-151, (width/2),18, NULL, 0, 24, 0, 0, "Calculate current Parent-Inverse Matrix (i.e. restore offset from parent)");
					// XXX uiButSetFunc(but, childof_const_setinv, con, NULL);
					
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Clear Offset", xco+((width/2)+10), yco-151, (width/2),18, NULL, 0, 24, 0, 0, "Clear Parent-Inverse Matrix (i.e. clear offset from parent)");
					// XXX uiButSetFunc(but, childof_const_clearinv, con, NULL);
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
			{
				bLocateLikeConstraint *data = con->data;
				
				result= uiLayoutColumn(box, 0);

				/* constraint space settings */
				block= uiLayoutFreeBlock(box);
				draw_constraint_spaceselect(block, con, 0, 0, is_armature_owner(ob), is_armature_target(data->tar));
			}
			break;
		case CONSTRAINT_TYPE_ROTLIKE:
			{
				bRotateLikeConstraint *data = con->data;
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) { 
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				/* Draw XYZ toggles */
				uiBlockBeginAlign(block);
					uiDefButBitI(block, TOG, ROTLIKE_X, B_CONSTRAINT_TEST, "X", xco+((width/2)-48), yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy X component");
					uiDefButBitI(block, TOG, ROTLIKE_X_INVERT, B_CONSTRAINT_TEST, "-", xco+((width/2)-16), yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Invert X component");
					uiDefButBitI(block, TOG, ROTLIKE_Y, B_CONSTRAINT_TEST, "Y", xco+((width/2)+16), yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Y component");
					uiDefButBitI(block, TOG, ROTLIKE_Y_INVERT, B_CONSTRAINT_TEST, "-", xco+((width/2)+48), yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Invert Y component");
					uiDefButBitI(block, TOG, ROTLIKE_Z, B_CONSTRAINT_TEST, "Z", xco+((width/2)+96), yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Z component");
					uiDefButBitI(block, TOG, ROTLIKE_Z_INVERT, B_CONSTRAINT_TEST, "-", xco+((width/2)+128), yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Invert Z component");
				uiBlockEndAlign(block);
				
				/* draw offset toggle */
				uiDefButBitI(block, TOG, ROTLIKE_OFFSET, B_CONSTRAINT_TEST, "Offset", xco, yco-64, 80, 18, &data->flag, 0, 24, 0, 0, "Add original rotation onto copied rotation");
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, xco, yco-94, is_armature_owner(ob), is_armature_target(data->tar));
			}
			break;
		case CONSTRAINT_TYPE_SIZELIKE:
			{
				bSizeLikeConstraint *data = con->data;
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				/* Draw XYZ toggles */
				uiBlockBeginAlign(block);
					uiDefButBitI(block, TOG, SIZELIKE_X, B_CONSTRAINT_TEST, "X", xco+((width/2)-48), yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy X component");
					uiDefButBitI(block, TOG, SIZELIKE_Y, B_CONSTRAINT_TEST, "Y", xco+((width/2)-16), yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Y component");
					uiDefButBitI(block, TOG, SIZELIKE_Z, B_CONSTRAINT_TEST, "Z", xco+((width/2)+16), yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Z component");
				uiBlockEndAlign(block);
				
				/* draw offset toggle */
				uiDefButBitI(block, TOG, SIZELIKE_OFFSET, B_CONSTRAINT_TEST, "Offset", xco, yco-64, 80, 18, &data->flag, 0, 24, 0, 0, "Add original scaling onto copied scaling");
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, xco, yco-94, is_armature_owner(ob), is_armature_target(data->tar));
			}
 			break;
		case CONSTRAINT_TYPE_KINEMATIC:
			{
				bKinematicConstraint *data = con->data;

				/* IK Target */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco, yco-24, 80, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco, yco-44, 137, 19, &data->tar, "Target Object"); 

				if (is_armature_target(data->tar)) {
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco, yco-62,137,19, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
					uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
				}
				else if (is_geom_target(data->tar)) {
					but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco, yco-62,137,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
					uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
				}
				else {
					strcpy (data->subtarget, "");
				}
				
				uiBlockEndAlign(block);
				
				/* Settings */
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, CONSTRAINT_IK_TIP, B_CONSTRAINT_TEST, "Use Tail", xco, yco-92, 137, 19, &data->flag, 0, 0, 0, 0, "Include Bone's tail also last element in Chain");
				uiDefButS(block, NUM, B_CONSTRAINT_TEST, "ChainLen:", xco, yco-112,137,19, &data->rootbone, 0, 255, 0, 0, "If not zero, the amount of bones in this chain");
				
				uiBlockBeginAlign(block);
				uiDefButF(block, NUMSLI, B_CONSTRAINT_TEST, "PosW ", xco+147, yco-92, 137, 19, &data->weight, 0.01, 1.0, 2, 2, "For Tree-IK: weight of position control for this target");
				uiDefButBitS(block, TOG, CONSTRAINT_IK_ROT, B_CONSTRAINT_TEST, "Rot", xco+147, yco-112, 40,19, &data->flag, 0, 0, 0, 0, "Chain follows rotation of target");
				uiDefButF(block, NUMSLI, B_CONSTRAINT_TEST, "W ", xco+187, yco-112, 97, 19, &data->orientweight, 0.01, 1.0, 2, 2, "For Tree-IK: Weight of orientation control for this target");
				
				uiBlockBeginAlign(block);
				
				uiDefButBitS(block, TOG, CONSTRAINT_IK_STRETCH, B_CONSTRAINT_TEST, "Stretch", xco, yco-137,137,19, &data->flag, 0, 0, 0, 0, "Enable IK stretching");
				uiBlockBeginAlign(block);
				uiDefButS(block, NUM, B_CONSTRAINT_TEST, "Iterations:", xco+147, yco-137, 137, 19, &data->iterations, 1, 10000, 0, 0, "Maximum number of solving iterations"); 
				uiBlockEndAlign(block);
				
				/* Pole Vector */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Pole Target:", xco+147, yco-24, 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+147, yco-44, 137, 19, &data->poletar, "Pole Target Object"); 
				if (is_armature_target(data->poletar)) {
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+147, yco-62,137,19, &data->polesubtarget, 0, 24, 0, 0, "Pole Subtarget Bone");
					uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->poletar);
				}
				else if (is_geom_target(data->poletar)) {
					but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+147, yco-62,137,18, &data->polesubtarget, 0, 24, 0, 0, "Name of Vertex Group defining pole 'target' points");
					uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->poletar);
				}
				else {
					strcpy(data->polesubtarget, "");
				}
				
				if (data->poletar) {
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Pole Offset ", xco, yco-167, 137, 19, &data->poleangle, -180.0, 180.0, 0, 0, "Pole rotation offset");
				}
			}
			break;
		case CONSTRAINT_TYPE_TRACKTO:
			{
				bTrackToConstraint *data = con->data;
					
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Align:", xco+5, yco-42, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
					
					uiDefButBitI(block, TOG, 1, B_CONSTRAINT_TEST, "TargetZ", xco+60, yco-42, 50, 18, &data->flags, 0, 1, 0, 0, "Target Z axis, not world Z axis, will constrain up direction");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "To:", xco+12, yco-64, 25, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"X",	xco+39, yco-64,17,18, &data->reserved1, 12.0, 0.0, 0, 0, "X axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Y",	xco+56, yco-64,17,18, &data->reserved1, 12.0, 1.0, 0, 0, "Y axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Z",	xco+73, yco-64,17,18, &data->reserved1, 12.0, 2.0, 0, 0, "Z axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"-X",	xco+90, yco-64,24,18, &data->reserved1, 12.0, 3.0, 0, 0, "-X axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"-Y",	xco+114, yco-64,24,18, &data->reserved1, 12.0, 4.0, 0, 0, "-Y axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"-Z",	xco+138, yco-64,24,18, &data->reserved1, 12.0, 5.0, 0, 0, "-Z axis points to the target object");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Up:", xco+174, yco-64, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
					
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"X",	xco+204, yco-64,17,18, &data->reserved2, 13.0, 0.0, 0, 0, "X axis points upward");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Y",	xco+221, yco-64,17,18, &data->reserved2, 13.0, 1.0, 0, 0, "Y axis points upward");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Z",	xco+238, yco-64,17,18, &data->reserved2, 13.0, 2.0, 0, 0, "Z axis points upward");
				uiBlockEndAlign(block);
				
				if (is_armature_target(data->tar)) {
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Head/Tail:", xco, yco-94, 241, 18, &con->headtail, 0.0, 1, 0.1, 0.1, "Target along length of bone: Head=0, Tail=1");
					
					/* constraint space settings */
					draw_constraint_spaceselect(block, con, xco, yco-116, is_armature_owner(ob), is_armature_target(data->tar));
				}
				else {
					/* constraint space settings */
					draw_constraint_spaceselect(block, con, xco, yco-94, is_armature_owner(ob), is_armature_target(data->tar));
				}
			}
			break;
		case CONSTRAINT_TYPE_MINMAX:
			{
				bMinMaxConstraint *data = con->data;
					
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Offset:", xco, yco-44, 100, 18, &data->offset, -100, 100, 100.0, 0.0, "Offset from the position of the object center"); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiDefButBitI(block, TOG, MINMAX_STICKY, B_CONSTRAINT_TEST, "Sticky", xco, yco-24, 44, 18, &data->flag, 0, 24, 0, 0, "Immobilize object while constrained");
				uiDefButBitI(block, TOG, MINMAX_USEROT, B_CONSTRAINT_TEST, "Use Rot", xco+44, yco-24, 64, 18, &data->flag, 0, 24, 0, 0, "Use target object rotation");
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Max/Min:", xco-8, yco-64, 54, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				uiBlockBeginAlign(block);			
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	xco+51, yco-64,17,18, &data->minmaxflag, 12.0, 0.0, 0, 0, "Will not pass below X of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	xco+67, yco-64,17,18, &data->minmaxflag, 12.0, 1.0, 0, 0, "Will not pass below Y of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	xco+85, yco-64,17,18, &data->minmaxflag, 12.0, 2.0, 0, 0, "Will not pass below Z of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-X",	xco+102, yco-64,24,18, &data->minmaxflag, 12.0, 3.0, 0, 0, "Will not pass above X of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Y",	xco+126, yco-64,24,18, &data->minmaxflag, 12.0, 4.0, 0, 0, "Will not pass above Y of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Z",	xco+150, yco-64,24,18, &data->minmaxflag, 12.0, 5.0, 0, 0, "Will not pass above Z of target");
				uiBlockEndAlign(block);
				
				if (is_armature_target(data->tar)) {
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Head/Tail:", xco, yco-86, 241, 18, &con->headtail, 0.0, 1, 0.1, 0.1, "Target along length of bone: Head=0, Tail=1");
				}
				
			}
			break;
		case CONSTRAINT_TYPE_LOCKTRACK:
			{
				bLockTrackConstraint *data = con->data;
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "To:", xco+12, yco-64, 25, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	xco+39, yco-64,17,18, &data->trackflag, 12.0, 0.0, 0, 0, "X axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	xco+56, yco-64,17,18, &data->trackflag, 12.0, 1.0, 0, 0, "Y axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	xco+73, yco-64,17,18, &data->trackflag, 12.0, 2.0, 0, 0, "Z axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-X",	xco+90, yco-64,24,18, &data->trackflag, 12.0, 3.0, 0, 0, "-X axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Y",	xco+114, yco-64,24,18, &data->trackflag, 12.0, 4.0, 0, 0, "-Y axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Z",	xco+138, yco-64,24,18, &data->trackflag, 12.0, 5.0, 0, 0, "-Z axis points to the target object");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Lock:", xco+166, yco-64, 38, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
					
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	xco+204, yco-64,17,18, &data->lockflag, 13.0, 0.0, 0, 0, "X axis is locked");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	xco+221, yco-64,17,18, &data->lockflag, 13.0, 1.0, 0, 0, "Y axis is locked");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	xco+238, yco-64,17,18, &data->lockflag, 13.0, 2.0, 0, 0, "Z axis is locked");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_FOLLOWPATH:
			{
				bFollowPathConstraint *data = con->data;
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
				
				/* Draw Curve Follow toggle */
				uiDefButBitI(block, TOG, 1, B_CONSTRAINT_TEST, "CurveFollow", xco+39, yco-44, 100, 18, &data->followflag, 0, 24, 0, 0, "Object will follow the heading and banking of the curve");
				
				/* Draw Offset number button */
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Offset:", xco+155, yco-44, 100, 18, &data->offset, -MAXFRAMEF, MAXFRAMEF, 100.0, 0.0, "Offset from the position corresponding to the time frame"); 
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Fw:", xco+12, yco-64, 27, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	xco+39, yco-64,17,18, &data->trackflag, 12.0, 0.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	xco+56, yco-64,17,18, &data->trackflag, 12.0, 1.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	xco+73, yco-64,17,18, &data->trackflag, 12.0, 2.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-X",	xco+90, yco-64,24,18, &data->trackflag, 12.0, 3.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Y",	xco+114, yco-64,24,18, &data->trackflag, 12.0, 4.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Z",	xco+138, yco-64,24,18, &data->trackflag, 12.0, 5.0, 0, 0, "The axis that points forward along the path");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Up:", xco+174, yco-64, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
					
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	xco+204, yco-64,17,18, &data->upflag, 13.0, 0.0, 0, 0, "The axis that points upward");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	xco+221, yco-64,17,18, &data->upflag, 13.0, 1.0, 0, 0, "The axis that points upward");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	xco+238, yco-64,17,18, &data->upflag, 13.0, 2.0, 0, 0, "The axis that points upward");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_STRETCHTO:
			{
				bStretchToConstraint *data = con->data;
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					if (is_armature_target(data->tar)) {
						uiDefButF(block, BUTM, B_CONSTRAINT_TEST, "R", xco, yco-60, 20, 18, &data->orglength, 0.0, 0, 0, 0, "Recalculate RLength");
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Rest Length:", xco+18, yco-60,139,18, &data->orglength, 0.0, 100, 0.5, 0.5, "Length at Rest Position");
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Head/Tail:", xco+155, yco-60,98,18, &con->headtail, 0.0, 1, 0.1, 0.1, "Target along length of bone: Head=0, Tail=1");
					}
					else {
						uiDefButF(block, BUTM, B_CONSTRAINT_TEST, "R", xco, yco-60, 20, 18, &data->orglength, 0.0, 0, 0, 0, "Recalculate RLength");
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Rest Length:", xco+18, yco-60, 237, 18, &data->orglength, 0.0, 100, 0.5, 0.5, "Length at Rest Position");
					}
				uiBlockEndAlign(block);
				
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Volume Variation:", xco+18, yco-82, 237, 18, &data->bulge, 0.0, 100, 0.5, 0.5, "Factor between volume variation and stretching");
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Vol:",xco+14, yco-104,30,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"XZ",	 xco+44, yco-104,30,18, &data->volmode, 12.0, 0.0, 0, 0, "Keep Volume: Scaling X & Z");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"X",	 xco+74, yco-104,20,18, &data->volmode, 12.0, 1.0, 0, 0, "Keep Volume: Scaling X");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Z",	 xco+94, yco-104,20,18, &data->volmode, 12.0, 2.0, 0, 0, "Keep Volume: Scaling Z");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"NONE", xco+114, yco-104,50,18, &data->volmode, 12.0, 3.0, 0, 0, "Ignore Volume");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST,"Plane:",xco+175, yco-104,40,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"X",	  xco+215, yco-104,20,18, &data->plane, 12.0, 0.0, 0, 0, "Keep X axis");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Z",	  xco+235, yco-104,20,18, &data->plane, 12.0, 2.0, 0, 0, "Keep Z axis");
				uiBlockEndAlign(block);
				}
			break;
		case CONSTRAINT_TYPE_LOCLIMIT:
			{
				bLocLimitConstraint *data = con->data;
				
				int togButWidth = 50;
				int textButWidth = ((width/2)-togButWidth);
				
				/* Draw Pairs of LimitToggle+LimitValue */
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XMIN, B_CONSTRAINT_TEST, "minX", xco, yco-28, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum x value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-28, (textButWidth-5), 18, &(data->xmin), -1000, 1000, 0.1,0.5,"Lowest x value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XMAX, B_CONSTRAINT_TEST, "maxX", xco+(width-(textButWidth-5)-togButWidth), yco-28, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum x value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-28, (textButWidth-5), 18, &(data->xmax), -1000, 1000, 0.1,0.5,"Highest x value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YMIN, B_CONSTRAINT_TEST, "minY", xco, yco-50, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum y value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-50, (textButWidth-5), 18, &(data->ymin), -1000, 1000, 0.1,0.5,"Lowest y value to allow"); 
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YMAX, B_CONSTRAINT_TEST, "maxY", xco+(width-(textButWidth-5)-togButWidth), yco-50, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum y value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-50, (textButWidth-5), 18, &(data->ymax), -1000, 1000, 0.1,0.5,"Highest y value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZMIN, B_CONSTRAINT_TEST, "minZ", xco, yco-72, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum z value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-72, (textButWidth-5), 18, &(data->zmin), -1000, 1000, 0.1,0.5,"Lowest z value to allow"); 
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZMAX, B_CONSTRAINT_TEST, "maxZ", xco+(width-(textButWidth-5)-togButWidth), yco-72, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum z value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-72, (textButWidth-5), 18, &(data->zmax), -1000, 1000, 0.1,0.5,"Highest z value to allow"); 
				uiBlockEndAlign(block);
				
				/* special option(s) */
				uiDefButBitS(block, TOG, LIMIT_TRANSFORM, B_CONSTRAINT_TEST, "For Transform", xco+(width/4), yco-100, (width/2), 18, &data->flag2, 0, 24, 0, 0, "Transforms are affected by this constraint as well"); 
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, xco, yco-130, is_armature_owner(ob), -1);
			}
			break;
		case CONSTRAINT_TYPE_ROTLIMIT:
			{
				bRotLimitConstraint *data = con->data;
				int normButWidth = (width/3);
				
				/* Draw Pairs of LimitToggle+LimitValue */
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XROT, B_CONSTRAINT_TEST, "LimitX", xco, yco-28, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Limit rotation on x-axis"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min:", xco+normButWidth, yco-28, normButWidth, 18, &(data->xmin), -360, 360, 0.1,0.5,"Lowest x value to allow"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max:", xco+(normButWidth * 2), yco-28, normButWidth, 18, &(data->xmax), -360, 360, 0.1,0.5,"Highest x value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YROT, B_CONSTRAINT_TEST, "LimitY", xco, yco-50, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Limit rotation on y-axis"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min:", xco+normButWidth, yco-50, normButWidth, 18, &(data->ymin), -360, 360, 0.1,0.5,"Lowest y value to allow"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max:", xco+(normButWidth * 2), yco-50, normButWidth, 18, &(data->ymax), -360, 360, 0.1,0.5,"Highest y value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZROT, B_CONSTRAINT_TEST, "LimitZ", xco, yco-72, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Limit rotation on z-axis"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min:", xco+normButWidth, yco-72, normButWidth, 18, &(data->zmin), -360, 360, 0.1,0.5,"Lowest z value to allow"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max:", xco+(normButWidth * 2), yco-72, normButWidth, 18, &(data->zmax), -360, 360, 0.1,0.5,"Highest z value to allow"); 
				uiBlockEndAlign(block); 
				
				/* special option(s) */
				uiDefButBitS(block, TOG, LIMIT_TRANSFORM, B_CONSTRAINT_TEST, "For Transform", xco+(width/4), yco-100, (width/2), 18, &data->flag2, 0, 24, 0, 0, "Transforms are affected by this constraint as well"); 
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, xco, yco-130, is_armature_owner(ob), -1);
			}
			break;
		case CONSTRAINT_TYPE_SIZELIMIT:
			{
				bSizeLimitConstraint *data = con->data;
				
				int togButWidth = 50;
				int textButWidth = ((width/2)-togButWidth);
					
				/* Draw Pairs of LimitToggle+LimitValue */
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XMIN, B_CONSTRAINT_TEST, "minX", xco, yco-28, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum x value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-28, (textButWidth-5), 18, &(data->xmin), 0.0001, 1000, 0.1,0.5,"Lowest x value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XMAX, B_CONSTRAINT_TEST, "maxX", xco+(width-(textButWidth-5)-togButWidth), yco-28, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum x value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-28, (textButWidth-5), 18, &(data->xmax), 0.0001, 1000, 0.1,0.5,"Highest x value to allow"); 
				uiBlockEndAlign(block); 
				
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YMIN, B_CONSTRAINT_TEST, "minY", xco, yco-50, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum y value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-50, (textButWidth-5), 18, &(data->ymin), 0.0001, 1000, 0.1,0.5,"Lowest y value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YMAX, B_CONSTRAINT_TEST, "maxY", xco+(width-(textButWidth-5)-togButWidth), yco-50, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum y value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-50, (textButWidth-5), 18, &(data->ymax), 0.0001, 1000, 0.1,0.5,"Highest y value to allow"); 
				uiBlockEndAlign(block); 
				
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZMIN, B_CONSTRAINT_TEST, "minZ", xco, yco-72, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum z value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-72, (textButWidth-5), 18, &(data->zmin), 0.0001, 1000, 0.1,0.5,"Lowest z value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZMAX, B_CONSTRAINT_TEST, "maxZ", xco+(width-(textButWidth-5)-togButWidth), yco-72, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum z value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-72, (textButWidth-5), 18, &(data->zmax), 0.0001, 1000, 0.1,0.5,"Highest z value to allow"); 
				uiBlockEndAlign(block);
				
				/* special option(s) */
				uiDefButBitS(block, TOG, LIMIT_TRANSFORM, B_CONSTRAINT_TEST, "For Transform", xco+(width/4), yco-100, (width/2), 18, &data->flag2, 0, 24, 0, 0, "Transforms are affected by this constraint as well"); 
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, xco, yco-130, is_armature_owner(ob), -1);
			}
			break;
		case CONSTRAINT_TYPE_DISTLIMIT:
			{
				bDistLimitConstraint *data = con->data;
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					if (is_armature_target(data->tar)) {
						uiDefButF(block, BUTM, B_CONSTRAINT_TEST, "R", xco, yco-60, 20, 18, &data->dist, 0, 0, 0, 0, "Recalculate distance"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Distance:", xco+18, yco-60,139,18, &data->dist, 0.0, 100, 0.5, 0.5, "Radius of limiting sphere");
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Head/Tail:", xco+155, yco-60,100,18, &con->headtail, 0.0, 1, 0.1, 0.1, "Target along length of bone: Head=0, Tail=1");
					}
					else {
						uiDefButF(block, BUTM, B_CONSTRAINT_TEST, "R", xco, yco-60, 20, 18, &data->dist, 0, 0, 0, 0, "Recalculate distance"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Distance:", xco+18, yco-60, 237, 18, &data->dist, 0.0, 100, 0.5, 0.5, "Radius of limiting sphere");
					}
					
					/* disabled soft-distance controls... currently it doesn't work yet. It was intended to be used for soft-ik (see xsi-blog for details) */
#if 0
					uiDefButBitS(block, TOG, LIMITDIST_USESOFT, B_CONSTRAINT_TEST, "Soft", xco, yco-82, 50, 18, &data->flag, 0, 24, 0, 0, "Enables soft-distance");
					if (data->flag & LIMITDIST_USESOFT)
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Soft-Distance:", xco+50, yco-82, 187, 18, &data->soft, 0.0, 100, 0.5, 0.5, "Distance surrounding radius when transforms should get 'delayed'");
#endif
				uiBlockEndAlign(block);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Clamp Region:",xco+((width/2)-110), yco-104,100,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Limit Mode%t|Inside %x0|Outside %x1|Surface %x2", xco+(width/2), yco-104, 100, 18, &data->mode, 0, 24, 0, 0, "Distances in relation to sphere of influence to allow");
			}
			break;
		case CONSTRAINT_TYPE_RIGIDBODYJOINT:
			{
				bRigidBodyJointConstraint *data = con->data;
				float extremeLin = 999.f;
				float extremeAngX = 180.f;
				float extremeAngY = 45.f;
				float extremeAngZ = 45.f;
				int togButWidth = 70;
				int offsetY = 150;
				int textButWidth = ((width/2)-togButWidth);
				
				uiDefButI(block, MENU, B_CONSTRAINT_TEST, "Joint Types%t|Ball%x1|Hinge%x2|Generic 6DOF%x12",//|Extra Force%x6",
				//uiDefButI(block, MENU, B_CONSTRAINT_TEST, "Joint Types%t|Ball%x1|Hinge%x2|Cone Twist%x4|Generic 6DOF%x12",//|Extra Force%x6",
												xco, yco-25, 150, 18, &data->type, 0, 0, 0, 0, "Choose the joint type");

				uiDefButBitS(block, TOG, CONSTRAINT_DISABLE_LINKED_COLLISION, B_CONSTRAINT_TEST, "No Collision", xco+155, yco-25, 111, 18, &data->flag, 0, 24, 0, 0, "Disable Collision Between Linked Bodies");


				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "toObject:", xco, yco-50, 130, 18, &data->tar, "Child Object");
				uiDefButBitS(block, TOG, CONSTRAINT_DRAW_PIVOT, B_CONSTRAINT_TEST, "ShowPivot", xco+135, yco-50, 130, 18, &data->flag, 0, 24, 0, 0, "Show pivot position and rotation"); 				
				
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Pivot X:", xco, yco-75, 130, 18, &data->pivX, -1000, 1000, 100, 0.0, "Offset pivot on X");
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Pivot Y:", xco, yco-100, 130, 18, &data->pivY, -1000, 1000, 100, 0.0, "Offset pivot on Y");
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Pivot Z:", xco, yco-125, 130, 18, &data->pivZ, -1000, 1000, 100, 0.0, "Offset pivot on z");
				
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Ax X:", xco+135, yco-75, 130, 18, &data->axX, -360, 360, 1500, 0.0, "Rotate pivot on X Axis (in degrees)");
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Ax Y:", xco+135, yco-100, 130, 18, &data->axY, -360, 360, 1500, 0.0, "Rotate pivot on Y Axis (in degrees)");
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Ax Z:", xco+135, yco-125, 130, 18, &data->axZ, -360, 360, 1500, 0.0, "Rotate pivot on Z Axis (in degrees)");
				
				if (data->type==CONSTRAINT_RB_GENERIC6DOF) {
					/* Draw Pairs of LimitToggle+LimitValue */
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 1, B_CONSTRAINT_TEST, "LinMinX", xco, yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum x limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-offsetY, (textButWidth-5), 18, &(data->minLimit[0]), -extremeLin, extremeLin, 0.1,0.5,"min x limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 1, B_CONSTRAINT_TEST, "LinMaxX", xco+(width-(textButWidth-5)-togButWidth), yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum x limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-offsetY, (textButWidth), 18, &(data->maxLimit[0]), -extremeLin, extremeLin, 0.1,0.5,"max x limit"); 
					uiBlockEndAlign(block);
					
					offsetY += 20;
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 2, B_CONSTRAINT_TEST, "LinMinY", xco, yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum y limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-offsetY, (textButWidth-5), 18, &(data->minLimit[1]), -extremeLin, extremeLin, 0.1,0.5,"min y limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 2, B_CONSTRAINT_TEST, "LinMaxY", xco+(width-(textButWidth-5)-togButWidth), yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum y limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-offsetY, (textButWidth), 18, &(data->maxLimit[1]), -extremeLin, extremeLin, 0.1,0.5,"max y limit"); 
					uiBlockEndAlign(block);
					
					offsetY += 20;
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 4, B_CONSTRAINT_TEST, "LinMinZ", xco, yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum z limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-offsetY, (textButWidth-5), 18, &(data->minLimit[2]), -extremeLin, extremeLin, 0.1,0.5,"min z limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 4, B_CONSTRAINT_TEST, "LinMaxZ", xco+(width-(textButWidth-5)-togButWidth), yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum z limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-offsetY, (textButWidth), 18, &(data->maxLimit[2]), -extremeLin, extremeLin, 0.1,0.5,"max z limit"); 
					uiBlockEndAlign(block);
					offsetY += 20;
				}
				if ((data->type==CONSTRAINT_RB_GENERIC6DOF) || (data->type==CONSTRAINT_RB_CONETWIST)) {
					/* Draw Pairs of LimitToggle+LimitValue */
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 8, B_CONSTRAINT_TEST, "AngMinX", xco, yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum x limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-offsetY, (textButWidth-5), 18, &(data->minLimit[3]), -extremeAngX, extremeAngX, 0.1,0.5,"min x limit"); 
					uiBlockEndAlign(block);
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 8, B_CONSTRAINT_TEST, "AngMaxX", xco+(width-(textButWidth-5)-togButWidth), yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum x limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-offsetY, (textButWidth), 18, &(data->maxLimit[3]), -extremeAngX, extremeAngX, 0.1,0.5,"max x limit"); 
					uiBlockEndAlign(block);
					
					offsetY += 20;
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 16, B_CONSTRAINT_TEST, "AngMinY", xco, yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum y limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-offsetY, (textButWidth-5), 18, &(data->minLimit[4]), -extremeAngY, extremeAngY, 0.1,0.5,"min y limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 16, B_CONSTRAINT_TEST, "AngMaxY", xco+(width-(textButWidth-5)-togButWidth), yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum y limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-offsetY, (textButWidth), 18, &(data->maxLimit[4]), -extremeAngY, extremeAngY, 0.1,0.5,"max y limit"); 
					uiBlockEndAlign(block);
					
					offsetY += 20;
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 32, B_CONSTRAINT_TEST, "AngMinZ", xco, yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum z limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+togButWidth, yco-offsetY, (textButWidth-5), 18, &(data->minLimit[5]), -extremeAngZ, extremeAngZ, 0.1,0.5,"min z limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 32, B_CONSTRAINT_TEST, "AngMaxZ", xco+(width-(textButWidth-5)-togButWidth), yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum z limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+(width-textButWidth-5), yco-offsetY, (textButWidth), 18, &(data->maxLimit[5]), -extremeAngZ, extremeAngZ, 0.1,0.5,"max z limit"); 
					uiBlockEndAlign(block);
				}
				
			}
			break;
		case CONSTRAINT_TYPE_CLAMPTO:
			{
				bClampToConstraint *data = con->data;
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object"); 
				
				/* Draw XYZ toggles */
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Main Axis:", xco, yco-64, 90, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButI(block, ROW, B_CONSTRAINT_TEST, "Auto", xco+100, yco-64, 50, 18, &data->flag, 12.0, CLAMPTO_AUTO, 0, 0, "Automatically determine main-axis of movement");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST, "X", xco+150, yco-64, 32, 18, &data->flag, 12.0, CLAMPTO_X, 0, 0, "Main axis of movement is x-axis");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST, "Y", xco+182, yco-64, 32, 18, &data->flag, 12.0, CLAMPTO_Y, 0, 0, "Main axis of movement is y-axis");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST, "Z", xco+214, yco-64, 32, 18, &data->flag, 12.0, CLAMPTO_Z, 0, 0, "Main axis of movement is z-axis");
				uiBlockEndAlign(block);
				
				/* Extra Options Controlling Behaviour */
				//uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Options:", xco, yco-88, 90, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButBitI(block, TOG, CLAMPTO_CYCLIC, B_CONSTRAINT_TEST, "Cyclic", xco+((width/2)), yco-88,60,19, &data->flag2, 0, 0, 0, 0, "Treat curve as cyclic curve (no clamping to curve bounding box)");
				//uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_TRANSFORM:
			{
				bTransformConstraint *data = con->data;
				float fmin, fmax, tmin, tmax;
				
				/* Draw target parameters */			
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->tar, "Target Object to use as Parent"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone to use as Parent");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				/* Extrapolate Ranges? */
				uiDefButBitC(block, TOG, 1, B_CONSTRAINT_TEST, "Extrapolate", xco-10, yco-42,80,19, &data->expo, 0, 0, 0, 0, "Extrapolate ranges");
				
				/* Draw options for source motion */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Source:", xco-10, yco-62, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* 	draw Loc/Rot/Size toggles 	*/
				uiBlockBeginAlign(block);
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Loc", xco-5, yco-82, 45, 18, &data->from, 12.0, 0, 0, 0, "Use Location transform channels from Target");
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Rot", xco+40, yco-82, 45, 18, &data->from, 12.0, 1, 0, 0, "Use Rotation transform channels from Target");
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Scale", xco+85, yco-82, 45, 18, &data->from, 12.0, 2, 0, 0, "Use Scale transform channels from Target");
				uiBlockEndAlign(block);
				
				/* Draw Pairs of Axis: Min/Max Value*/
				if (data->from == 2) {
					fmin= 0.0001;
					fmax= 1000.0;
				}
				else if (data->from == 1) {
					fmin= -360.0;
					fmax= 360.0;
				}
				else {
					fmin = -1000.0;
					fmax= 1000.0;
				}
				
				uiBlockBeginAlign(block); 
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "X:", xco-10, yco-107, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", xco+20, yco-107, 55, 18, &data->from_min[0], fmin, fmax, 0, 0, "Bottom of range of x-axis source motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", xco+75, yco-107, 55, 18, &data->from_max[0], fmin, fmax, 0, 0, "Top of range of x-axis source motion for source->target mapping"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Y:", xco-10, yco-127, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", xco+20, yco-127, 55, 18, &data->from_min[1], fmin, fmax, 0, 0, "Bottom of range of y-axis source motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", xco+75, yco-127, 55, 18, &data->from_max[1], fmin, fmax, 0, 0, "Top of range of y-axis source motion for source->target mapping"); 
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block); 
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Z:", xco-10, yco-147, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", xco+20, yco-147, 55, 18, &data->from_min[2], fmin, fmax, 0, 0, "Bottom of range of z-axis source motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", xco+75, yco-147, 55, 18, &data->from_max[2], fmin, fmax, 0, 0, "Top of range of z-axis source motion for source->target mapping"); 
				uiBlockEndAlign(block); 
				
				
				/* Draw options for target motion */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Destination:", xco+150, yco-62, 150, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* 	draw Loc/Rot/Size toggles 	*/
				uiBlockBeginAlign(block);
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Loc", xco+150, yco-82, 45, 18, &data->to, 12.0, 0, 0, 0, "Use as Location transform");
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Rot", xco+195, yco-82, 45, 18, &data->to, 12.0, 1, 0, 0, "Use as Rotation transform");
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Scale", xco+245, yco-82, 45, 18, &data->to, 12.0, 2, 0, 0, "Use as Scale transform");
				uiBlockEndAlign(block);
				
				/* Draw Pairs of Source-Axis: Min/Max Value*/
				if (data->to == 2) {
					tmin= 0.0001;
					tmax= 1000.0;
				}
				else if (data->to == 1) {
					tmin= -360.0;
					tmax= 360.0;
				}
				else {
					tmin = -1000.0;
					tmax= 1000.0;
				}
				
				uiBlockBeginAlign(block); 
					uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Axis Mapping%t|X->X%x0|Y->X%x1|Z->X%x2", xco+150, yco-107, 40, 18, &data->map[0], 0, 24, 0, 0, "Specify which source axis the x-axis destination uses");
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", xco+175, yco-107, 50, 18, &data->to_min[0], tmin, tmax, 0, 0, "Bottom of range of x-axis destination motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", xco+240, yco-107, 50, 18, &data->to_max[0], tmin, tmax, 0, 0, "Top of range of x-axis destination motion for source->target mapping"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Axis Mapping%t|X->Y%x0|Y->Y%x1|Z->Y%x2", xco+150, yco-127, 40, 18, &data->map[1], 0, 24, 0, 0, "Specify which source axis the y-axis destination uses");
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", xco+175, yco-127, 50, 18, &data->to_min[1], tmin, tmax, 0, 0, "Bottom of range of y-axis destination motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", xco+240, yco-127, 50, 18, &data->to_max[1], tmin, tmax, 0, 0, "Top of range of y-axis destination motion for source->target mapping"); 
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block); 
					uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Axis Mapping%t|X->Z%x0|Y->Z%x1|Z->Z%x2", xco+150, yco-147, 40, 18, &data->map[2], 0, 24, 0, 0, "Specify which source axis the z-axis destination uses");
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", xco+175, yco-147, 50, 18, &data->to_min[2], tmin, tmax, 0, 0, "Bottom of range of z-axis destination motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", xco+240, yco-147, 50, 18, &data->to_max[2], tmin, tmax, 0, 0, "Top of range of z-axis destination motion for source->target mapping"); 
				uiBlockEndAlign(block); 
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, xco, yco-170, is_armature_owner(ob), is_armature_target(data->tar));
			}
			break;
		case CONSTRAINT_TYPE_NULL:
			{
				uiItemL(box, "", 0);
			}
			break;

		case CONSTRAINT_TYPE_SHRINKWRAP:
			{
				bShrinkwrapConstraint *data = con->data;

				/* Draw parameters */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+65, yco-24, 90, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				uiDefIDPoinBut(block, test_meshobpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-24, 135, 18, &data->target, "Target Object");

				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Dist:", xco + 75, yco-42, 90, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", xco+120, yco-42, 135, 18, &data->dist, -100.0f, 100.0f, 1.0f, 0.0f, "Distance to target");

				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Type:", xco + 70, yco-60, 90, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				uiDefButS(block, MENU, B_MODIFIER_RECALC, "Shrinkwrap Type%t|Nearest Surface Point %x0|Projection %x1|Nearest Vertex %x2", xco+120, yco-60, 135, 18, &data->shrinkType, 0, 0, 0, 0, "Selects type of shrinkwrap algorithm for target position.");

				if(data->shrinkType == MOD_SHRINKWRAP_PROJECT)
				{
					/* Draw XYZ toggles */
					uiDefBut(block, LABEL,B_CONSTRAINT_TEST, "Axis:", xco+ 75, yco-78, 90, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButBitC(block, TOG, MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS, B_CONSTRAINT_TEST, "X",xco+120, yco-78, 45, 18, &data->projAxis, 0, 0, 0, 0, "Projection over X axis");
					uiDefButBitC(block, TOG, MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS, B_CONSTRAINT_TEST, "Y",xco+165, yco-78, 45, 18, &data->projAxis, 0, 0, 0, 0, "Projection over Y axis");
					uiDefButBitC(block, TOG, MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS, B_CONSTRAINT_TEST, "Z",xco+210, yco-78, 45, 18, &data->projAxis, 0, 0, 0, 0, "Projection over Z axis");
				}
			}
			break;
		default:
			result= box;
			break;
		}
	}

	if (ELEM(con->type, CONSTRAINT_TYPE_NULL, CONSTRAINT_TYPE_RIGIDBODYJOINT)==0) {
		box= uiLayoutBox(col);
		block= uiLayoutFreeBlock(box);
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_CONSTRAINT_INF, "Influence ", xco, yco, width, 20, &(con->enforce), 0.0, 1.0, 0.0, 0.0, "Amount of influence this constraint will have on the final solution");
		uiBlockEndAlign(block);

		// XXX Show/Key buttons, functionaly can be replaced with right click menu?
	} 
	
	/* clear any locks set up for proxies/lib-linking */
	uiBlockClearButLock(block);

	return result;
}

uiLayout *uiTemplateConstraint(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob;
	bConstraint *con;

	/* verify we have valid data */
	if(!RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
		printf("uiTemplateConstraint: expected constraint on object.\n");
		return NULL;
	}

	ob= ptr->id.data;
	con= ptr->data;

	if(!ob || !(GS(ob->id.name) == ID_OB)) {
		printf("uiTemplateConstraint: expected constraint on object.\n");
		return NULL;
	}
	
	uiBlockSetButLock(uiLayoutGetBlock(layout), (ob && ob->id.lib), ERROR_LIBDATA_MESSAGE);

	/* hrms, the temporal constraint should not draw! */
	if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
		bKinematicConstraint *data= con->data;
		if(data->flag & CONSTRAINT_IK_TEMP)
			return NULL;
	}

	return draw_constraint(layout, ob, con);
}

