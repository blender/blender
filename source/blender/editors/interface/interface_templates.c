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

static void modifier_testLatticeObj(bContext *C, char *name, ID **idpp)
{
	Main *bmain= CTX_data_main(C);
	ID *id;

	for (id= bmain->object.first; id; id= id->next) {
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_LATTICE) {
				uiPupMenuError(C, "Lattice deform object must be a lattice");
				break;
			} 
			*idpp= id;
			return;
		}
	}
	*idpp= 0;
}

static void modifier_testCurveObj(bContext *C, char *name, ID **idpp)
{
	Main *bmain= CTX_data_main(C);
	ID *id;

	for (id= bmain->object.first; id; id= id->next) {
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_CURVE) {
				uiPupMenuError(C, "Curve deform object must be a curve");
				break;
			} 
			*idpp= id;
			return;
		}
	}
	*idpp= 0;
}

static void modifier_testMeshObj(bContext *C, char *name, ID **idpp)
{
	Main *bmain= CTX_data_main(C);
	Object *obact= CTX_data_active_object(C);
	ID *id;

	for (id= bmain->object.first; id; id= id->next) {
		/* no boolean on its own object */
		if(id != (ID *)obact) {
			if( strcmp(name, id->name+2)==0 ) {
				if (((Object *)id)->type != OB_MESH) {
					uiPupMenuError(C, "Boolean modifier object must be a mesh");
					break;
				} 
				*idpp= id;
				return;
			}
		}
	}
	*idpp= NULL;
}

static void modifier_testArmatureObj(bContext *C, char *name, ID **idpp)
{
	Main *bmain= CTX_data_main(C);
	ID *id;

	for (id= bmain->object.first; id; id= id->next) {
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_ARMATURE) {
				uiPupMenuError(C, "Armature deform object must be an armature");
				break;
			} 
			*idpp= id;
			return;
		}
	}
	*idpp= 0;
}

static void modifier_testTexture(bContext *C, char *name, ID **idpp)
{
	Main *bmain= CTX_data_main(C);
	ID *id;

	for(id = bmain->tex.first; id; id = id->next) {
		if(strcmp(name, id->name + 2) == 0) {
			*idpp = id;
			/* texture gets user, objects not: delete object = clear modifier */
			id_us_plus(id);
			return;
		}
	}
	*idpp = 0;
}

#if 0 /* this is currently unused, but could be useful in the future */
static void modifier_testMaterial(bContext *C, char *name, ID **idpp)
{
	Main *bmain= CTX_data_main(C);
	ID *id;

	for(id = bmain->mat.first; id; id = id->next) {
		if(strcmp(name, id->name + 2) == 0) {
			*idpp = id;
			return;
		}
	}
	*idpp = 0;
}
#endif

static void modifier_testImage(bContext *C, char *name, ID **idpp)
{
	Main *bmain= CTX_data_main(C);
	ID *id;

	for(id = bmain->image.first; id; id = id->next) {
		if(strcmp(name, id->name + 2) == 0) {
			*idpp = id;
			return;
		}
	}
	*idpp = 0;
}

/* autocomplete callback for ID buttons */
void autocomplete_image(bContext *C, char *str, void *arg_v)
{
	Main *bmain= CTX_data_main(C);

	/* search if str matches the beginning of an ID struct */
	if(str[0]) {
		AutoComplete *autocpl = autocomplete_begin(str, 22);
		ID *id;

		for(id = bmain->image.first; id; id = id->next)
			autocomplete_do_name(autocpl, id->name+2);

		autocomplete_end(autocpl, str);
	}
}

/* autocomplete callback for ID buttons */
void autocomplete_meshob(bContext *C, char *str, void *arg_v)
{
	Main *bmain= CTX_data_main(C);

	/* search if str matches the beginning of an ID struct */
	if(str[0]) {
		AutoComplete *autocpl = autocomplete_begin(str, 22);
		ID *id;

		for(id = bmain->object.first; id; id = id->next)
			if(((Object *)id)->type == OB_MESH)
				autocomplete_do_name(autocpl, id->name+2);

		autocomplete_end(autocpl, str);
	}
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

static void build_uvlayer_menu_vars(CustomData *data, char **menu_string,
                                    int *uvlayer_tmp, char *uvlayer_name)
{
	char strtmp[38];
	int totuv, i;
	CustomDataLayer *layer
	            = &data->layers[CustomData_get_layer_index(data, CD_MTFACE)];

	*uvlayer_tmp = -1;

	totuv = CustomData_number_of_layers(data, CD_MTFACE);

	*menu_string = MEM_callocN(sizeof(**menu_string) * (totuv * 38 + 10),
	                           "menu_string");
	sprintf(*menu_string, "UV Layer%%t");
	for(i = 0; i < totuv; i++) {
		/* assign first layer as uvlayer_name if uvlayer_name is null. */
		if(strcmp(layer->name, uvlayer_name) == 0) *uvlayer_tmp = i + 1;
		sprintf(strtmp, "|%s%%x%d", layer->name, i + 1);
		strcat(*menu_string, strtmp);
		layer++;
	}

	/* there is no uvlayer defined, or else it was deleted. Assign active
	 * layer, then recalc modifiers.
	 */
	if(*uvlayer_tmp == -1) {
		if(CustomData_get_active_layer_index(data, CD_MTFACE) != -1) {
			*uvlayer_tmp = 1;
			layer = data->layers;
			for(i = 0; i < CustomData_get_active_layer_index(data, CD_MTFACE);
			    i++, layer++) {
				if(layer->type == CD_MTFACE) (*uvlayer_tmp)++;
			}
			strcpy(uvlayer_name, layer->name);

			/* update the modifiers */
			/* XXX do_modifier_panels(B_MODIFIER_RECALC);*/
		} else {
			/* ok we have no uv layers, so make sure menu button knows that.*/
			*uvlayer_tmp = 0;
		}
	}
}

void set_wave_uvlayer(bContext *C, void *arg1, void *arg2)
{
	WaveModifierData *wmd=arg1;
	CustomDataLayer *layer = arg2;

	/*check we have UV layers*/
	if (wmd->uvlayer_tmp < 1) return;
	layer = layer + (wmd->uvlayer_tmp-1);
	
	strcpy(wmd->uvlayer_name, layer->name);
}

void set_displace_uvlayer(bContext *C, void *arg1, void *arg2)
{
	DisplaceModifierData *dmd=arg1;
	CustomDataLayer *layer = arg2;

	/*check we have UV layers*/
	if (dmd->uvlayer_tmp < 1) return;
	layer = layer + (dmd->uvlayer_tmp-1);
	
	strcpy(dmd->uvlayer_name, layer->name);
}

void set_uvproject_uvlayer(bContext *C, void *arg1, void *arg2)
{
	UVProjectModifierData *umd=arg1;
	CustomDataLayer *layer = arg2;

	/*check we have UV layers*/
	if (umd->uvlayer_tmp < 1) return;
	layer = layer + (umd->uvlayer_tmp-1);
	
	strcpy(umd->uvlayer_name, layer->name);
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

static int modifier_is_fluid_particles(ModifierData *md)
{
	if(md->type == eModifierType_ParticleSystem) {
		if(((ParticleSystemModifierData *)md)->psys->part->type == PART_FLUID)
			return 1;
	}
	return 0;
}

static uiLayout *draw_modifier(bContext *C, uiLayout *layout, Object *ob, ModifierData *md, int index, int cageIndex, int lastCageIndex)
{
	Object *obedit= CTX_data_edit_object(C);
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	uiBut *but;
	uiBlock *block;
	uiLayout *column, *row, *result= NULL;
	int isVirtual = md->mode&eModifierMode_Virtual;
	int x = 0, y = 0; // XXX , color = md->error?TH_REDALERT:TH_BUT_NEUTRAL;
	int editing = (obedit==ob);
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
		uiDefIconButBitI(block, ICONTOG, eModifierMode_Expanded, B_MODIFIER_REDRAW, VICON_DISCLOSURE_TRI_RIGHT, x-10, y-2, 20, 20, &md->mode, 0.0, 0.0, 0.0, 0.0, "Collapse/Expand Modifier");
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
			but= uiDefIconButBitI(block, TOG, eModifierMode_Realtime, B_MODIFIER_RECALC, VICON_VIEW3D, x+10+buttonWidth-40, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during interactive display");
			if (mti->flags&eModifierTypeFlag_SupportsEditmode) {
				uiDefIconButBitI(block, TOG, eModifierMode_Editmode, B_MODIFIER_RECALC, VICON_EDIT, x+10+buttonWidth-20, y-1, 19, 19,&md->mode, 0, 0, 1, 0, "Enable modifier during Editmode (only if enabled for display)");
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

			uiBlockBeginAlign(block);
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
			uiBlockEndAlign(block);
		}

		result= uiLayoutColumn(box, 0);
		block= uiLayoutFreeBlock(box);

		lx = x + 10;
		cy = y + 10 - 1;
		uiBlockBeginAlign(block);
		if (md->type==eModifierType_Subsurf) {
			SubsurfModifierData *smd = (SubsurfModifierData*) md;
			char subsurfmenu[]="Subsurf Type%t|Catmull-Clark%x0|Simple Subdiv.%x1";
			uiDefButS(block, MENU, B_MODIFIER_RECALC, subsurfmenu,		lx,(cy-=19),buttonWidth,19, &smd->subdivType, 0, 0, 0, 0, "Selects type of subdivision algorithm.");
			uiDefButS(block, NUM, B_MODIFIER_RECALC, "Levels:",		lx, (cy-=19), buttonWidth,19, &smd->levels, 1, 6, 0, 0, "Number subdivisions to perform");
			uiDefButS(block, NUM, B_MODIFIER_REDRAW, "Render Levels:",		lx, (cy-=19), buttonWidth,19, &smd->renderLevels, 1, 6, 0, 0, "Number subdivisions to perform when rendering");

			/* Disabled until non-EM DerivedMesh implementation is complete */

			/*
			uiDefButBitS(block, TOG, eSubsurfModifierFlag_Incremental, B_MODIFIER_RECALC, "Incremental", lx, (cy-=19),90,19,&smd->flags, 0, 0, 0, 0, "Use incremental calculation, even outside of mesh mode");
			uiDefButBitS(block, TOG, eSubsurfModifierFlag_DebugIncr, B_MODIFIER_RECALC, "Debug", lx+90, cy,buttonWidth-90,19,&smd->flags, 0, 0, 0, 0, "Visualize the subsurf incremental calculation, for debugging effect of other modifiers");
			*/

			uiDefButBitS(block, TOG, eSubsurfModifierFlag_ControlEdges, B_MODIFIER_RECALC, "Optimal Draw", lx, (cy-=19), buttonWidth,19,&smd->flags, 0, 0, 0, 0, "Skip drawing/rendering of interior subdivided edges");
			uiDefButBitS(block, TOG, eSubsurfModifierFlag_SubsurfUv, B_MODIFIER_RECALC, "Subsurf UV", lx, (cy-=19),buttonWidth,19,&smd->flags, 0, 0, 0, 0, "Use subsurf to subdivide UVs");
		} else if (md->type==eModifierType_Lattice) {
			LatticeModifierData *lmd = (LatticeModifierData*) md;
			uiDefIDPoinBut(block, modifier_testLatticeObj, ID_OB, B_CHANGEDEP, "Ob: ",	lx, (cy-=19), buttonWidth,19, &lmd->object, "Lattice object to deform with");
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-=19), buttonWidth,19, &lmd->name, 0.0, 31.0, 0, 0, "Vertex Group name");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
		} else if (md->type==eModifierType_Curve) {
			CurveModifierData *cmd = (CurveModifierData*) md;
			uiDefIDPoinBut(block, modifier_testCurveObj, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &cmd->object, "Curve object to deform with");
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-=19), buttonWidth,19, &cmd->name, 0.0, 31.0, 0, 0, "Vertex Group name");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"X",		lx, (cy-=19), 19,19, &cmd->defaxis, 12.0, MOD_CURVE_POSX, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"Y",		(lx+buttonWidth/6), cy, 19,19, &cmd->defaxis, 12.0, MOD_CURVE_POSY, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"Z",		(lx+2*buttonWidth/6), cy, 19,19, &cmd->defaxis, 12.0, MOD_CURVE_POSZ, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"-X",		(lx+3*buttonWidth/6), cy, 24,19, &cmd->defaxis, 12.0, MOD_CURVE_NEGX, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"-Y",		(lx+4*buttonWidth/6), cy, 24,19, &cmd->defaxis, 12.0, MOD_CURVE_NEGY, 0, 0, "The axis that the curve deforms along");
			uiDefButS(block, ROW,B_MODIFIER_RECALC,"-Z",		(lx+buttonWidth-buttonWidth/6), cy, 24,19, &cmd->defaxis, 12.0, MOD_CURVE_NEGZ, 0, 0, "The axis that the curve deforms along");
		} else if (md->type==eModifierType_Build) {
			BuildModifierData *bmd = (BuildModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Start:", lx, (cy-=19), buttonWidth,19, &bmd->start, 1.0, MAXFRAMEF, 100, 0, "Specify the start frame of the effect");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Length:", lx, (cy-=19), buttonWidth,19, &bmd->length, 1.0, MAXFRAMEF, 100, 0, "Specify the total time the build effect requires");
			uiDefButI(block, TOG, B_MODIFIER_RECALC, "Randomize", lx, (cy-=19), buttonWidth,19, &bmd->randomize, 0, 0, 1, 0, "Randomize the faces or edges during build.");
			uiDefButI(block, NUM, B_MODIFIER_RECALC, "Seed:", lx, (cy-=19), buttonWidth,19, &bmd->seed, 1.0, MAXFRAMEF, 100, 0, "Specify the seed for random if used.");
		} else if (md->type==eModifierType_Mirror) {
			MirrorModifierData *mmd = (MirrorModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Merge Limit:", lx, (cy-=19), buttonWidth,19, &mmd->tolerance, 0.0, 1.0, 10, 10, "Distance from axis within which mirrored vertices are merged");
			uiDefButBitS(block, TOG, MOD_MIR_AXIS_X, B_MODIFIER_RECALC, "X",	lx,(cy-=19),20,19, &mmd->flag, 0, 0, 0, 0, "Enable X axis mirror");
			uiDefButBitS(block, TOG, MOD_MIR_AXIS_Y, B_MODIFIER_RECALC, "Y",	lx+20,cy,20,19,    &mmd->flag, 0, 0, 0, 0, "Enable Y axis mirror");
			uiDefButBitS(block, TOG, MOD_MIR_AXIS_Z, B_MODIFIER_RECALC, "Z",	lx+40,cy,20,19,    &mmd->flag, 0, 0, 0, 0, "Enable Z axis mirror");
			uiDefButBitS(block, TOG, MOD_MIR_CLIPPING, B_MODIFIER_RECALC, "Do Clipping",	lx+60, cy, buttonWidth-60,19, &mmd->flag, 1, 2, 0, 0, "Prevents during Transform vertices to go through Mirror");
			uiDefButBitS(block, TOG, MOD_MIR_VGROUP, B_MODIFIER_RECALC, "Mirror Vgroups",	lx, (cy-=19), buttonWidth,19, &mmd->flag, 1, 2, 0, 0, "Mirror vertex groups (e.g. .R->.L)");
			uiDefButBitS(block, TOG, MOD_MIR_MIRROR_U, B_MODIFIER_RECALC,
			             "Mirror U",
			             lx, (cy-=19), buttonWidth/2, 19,
			             &mmd->flag, 0, 0, 0, 0,
			             "Mirror the U texture coordinate around "
			             "the 0.5 point");
			uiDefButBitS(block, TOG, MOD_MIR_MIRROR_V, B_MODIFIER_RECALC,
			             "Mirror V",
			             lx + buttonWidth/2 + 1, cy, buttonWidth/2, 19,
			             &mmd->flag, 0, 0, 0, 0,
			             "Mirror the V texture coordinate around "
			             "the 0.5 point");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
			               "Ob: ", lx, (cy -= 19), buttonWidth, 19,
			               &mmd->mirror_ob,
			               "Object to use as mirror");
		} else if (md->type==eModifierType_Bevel) {
			BevelModifierData *bmd = (BevelModifierData*) md;
			/*uiDefButS(block, ROW, B_MODIFIER_RECALC, "Distance",
					  lx, (cy -= 19), (buttonWidth/2), 19, &bmd->val_flags,
					  11.0, 0, 0, 0,
					  "Interpret bevel value as a constant distance from each edge");
			uiDefButS(block, ROW, B_MODIFIER_RECALC, "Radius",
					  (lx+buttonWidth/2), cy, (buttonWidth - buttonWidth/2), 19, &bmd->val_flags,
					  11.0, BME_BEVEL_RADIUS, 0, 0,
					  "Interpret bevel value as a radius - smaller angles will be beveled more");*/
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Width: ",
					  lx, (cy -= 19), buttonWidth, 19, &bmd->value,
					  0.0, 0.5, 5, 4,
					  "Bevel value/amount");
			/*uiDefButI(block, NUM, B_MODIFIER_RECALC, "Recurs",
					  lx, (cy -= 19), buttonWidth, 19, &bmd->res,
					  1, 4, 5, 2,
					  "Number of times to bevel");*/
			uiDefButBitS(block, TOG, BME_BEVEL_VERT,
					  B_MODIFIER_RECALC, "Only Vertices",
					  lx, (cy -= 19), buttonWidth, 19,
					  &bmd->flags, 0, 0, 0, 0,
					  "Bevel only verts/corners; not edges");
			uiBlockEndAlign(block);
					  
			uiDefBut(block, LABEL, 1, "Limit using:",	lx, (cy-=25), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
			uiBlockBeginAlign(block);
			uiDefButS(block, ROW, B_MODIFIER_RECALC, "None",
					  lx, (cy -= 19), (buttonWidth/3), 19, &bmd->lim_flags,
					  12.0, 0, 0, 0,
					  "Bevel the entire mesh by a constant amount");
			uiDefButS(block, ROW, B_MODIFIER_RECALC, "Angle",
					  (lx+buttonWidth/3), cy, (buttonWidth/3), 19, &bmd->lim_flags,
					  12.0, BME_BEVEL_ANGLE, 0, 0,
					  "Only bevel edges with sharp enough angles between faces");
			uiDefButS(block, ROW, B_MODIFIER_RECALC, "BevWeight",
					  lx+(2*buttonWidth/3), cy, buttonWidth-2*(buttonWidth/3), 19, &bmd->lim_flags,
					  12.0, BME_BEVEL_WEIGHT, 0, 0,
					  "Use bevel weights to determine how much bevel is applied; apply them separately in vert/edge select mode");
			if ((bmd->lim_flags & BME_BEVEL_WEIGHT) && !(bmd->flags & BME_BEVEL_VERT)) {
				uiDefButS(block, ROW, B_MODIFIER_RECALC, "Min",
					  lx, (cy -= 19), (buttonWidth/3), 19, &bmd->e_flags,
					  13.0, BME_BEVEL_EMIN, 0, 0,
					  "The sharpest edge's weight is used when weighting a vert");
				uiDefButS(block, ROW, B_MODIFIER_RECALC, "Average",
					  (lx+buttonWidth/3), cy, (buttonWidth/3), 19, &bmd->e_flags,
					  13.0, 0, 0, 0,
					  "The edge weights are averaged when weighting a vert");
				uiDefButS(block, ROW, B_MODIFIER_RECALC, "Max",
					  (lx+2*(buttonWidth/3)), cy, buttonWidth-2*(buttonWidth/3), 19, &bmd->e_flags,
					  13.0, BME_BEVEL_EMAX, 0, 0,
					  "The largest edge's wieght is used when weighting a vert");
			}
			else if (bmd->lim_flags & BME_BEVEL_ANGLE) {
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Angle:",
					  lx, (cy -= 19), buttonWidth, 19, &bmd->bevel_angle,
					  0.0, 180.0, 100, 2,
					  "Angle above which to bevel edges");
			}
		} else if (md->type==eModifierType_EdgeSplit) {
			EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;
			uiDefButBitI(block, TOG, MOD_EDGESPLIT_FROMANGLE,
			             B_MODIFIER_RECALC, "From Edge Angle",
			             lx, (cy -= 19), buttonWidth, 19,
			             &emd->flags, 0, 0, 0, 0,
			             "Split edges with high angle between faces");
			if(emd->flags & MOD_EDGESPLIT_FROMANGLE) {
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Split Angle:",
				          lx, (cy -= 19), buttonWidth, 19, &emd->split_angle,
				          0.0, 180.0, 100, 2,
				          "Angle above which to split edges");
			}
			uiDefButBitI(block, TOG, MOD_EDGESPLIT_FROMFLAG,
			             B_MODIFIER_RECALC, "From Marked As Sharp",
			             lx, (cy -= 19), buttonWidth, 19,
			             &emd->flags, 0, 0, 0, 0,
			             "Split edges that are marked as sharp");
		} else if (md->type==eModifierType_Displace) {
			DisplaceModifierData *dmd = (DisplaceModifierData*) md;
			but = uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",
			               lx, (cy -= 19), buttonWidth, 19,
			               &dmd->defgrp_name, 0.0, 31.0, 0, 0,
			               "Name of vertex group to displace"
			               " (displace whole mesh if blank)");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			uiDefIDPoinBut(block, modifier_testTexture, ID_TE, B_CHANGEDEP,
			               "Texture: ", lx, (cy -= 19), buttonWidth, 19,
			               &dmd->texture,
			               "Texture to use as displacement input");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Midlevel:",
			          lx, (cy -= 19), buttonWidth, 19, &dmd->midlevel,
			          0, 1, 10, 3,
			          "Material value that gives no displacement");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Strength:",
			          lx, (cy -= 19), buttonWidth, 19, &dmd->strength,
			          -1000, 1000, 10, 0.1,
			          "Strength of displacement");
			sprintf(str, "Direction%%t|Normal%%x%d|RGB -> XYZ%%x%d|"
			        "Z%%x%d|Y%%x%d|X%%x%d",
			        MOD_DISP_DIR_NOR, MOD_DISP_DIR_RGB_XYZ,
			        MOD_DISP_DIR_Z, MOD_DISP_DIR_Y, MOD_DISP_DIR_X);
			uiDefButI(block, MENU, B_MODIFIER_RECALC, str,
			          lx, (cy -= 19), buttonWidth, 19, &dmd->direction,
			          0.0, 1.0, 0, 0, "Displace direction");
			sprintf(str, "Texture Coordinates%%t"
			        "|Local%%x%d|Global%%x%d|Object%%x%d|UV%%x%d",
			        MOD_DISP_MAP_LOCAL, MOD_DISP_MAP_GLOBAL,
			        MOD_DISP_MAP_OBJECT, MOD_DISP_MAP_UV);
			uiDefButI(block, MENU, B_MODIFIER_RECALC, str,
			          lx, (cy -= 19), buttonWidth, 19, &dmd->texmapping,
			          0.0, 1.0, 0, 0,
			          "Texture coordinates used for displacement input");
			if (dmd->texmapping == MOD_DISP_MAP_UV) {
				char *strtmp;
				int i;
				Mesh *me= (Mesh*)ob->data;
				CustomData *fdata = obedit? &me->edit_mesh->fdata: &me->fdata;
				build_uvlayer_menu_vars(fdata, &strtmp, &dmd->uvlayer_tmp,
				                        dmd->uvlayer_name);
				but = uiDefButI(block, MENU, B_MODIFIER_RECALC, strtmp,
				      lx, (cy -= 19), buttonWidth, 19, &dmd->uvlayer_tmp,
				      0.0, 1.0, 0, 0, "Set the UV layer to use");
				MEM_freeN(strtmp);
				i = CustomData_get_layer_index(fdata, CD_MTFACE);
				uiButSetFunc(but, set_displace_uvlayer, dmd,
				             &fdata->layers[i]);
			}
			if(dmd->texmapping == MOD_DISP_MAP_OBJECT) {
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
				               "Ob: ", lx, (cy -= 19), buttonWidth, 19,
				               &dmd->map_object,
				               "Object to get texture coordinates from");
			}
		} else if (md->type==eModifierType_UVProject) {
			UVProjectModifierData *umd = (UVProjectModifierData *) md;
			int i;
			char *strtmp;
			Mesh *me= (Mesh*)ob->data;
			CustomData *fdata = obedit? &me->edit_mesh->fdata: &me->fdata;
			build_uvlayer_menu_vars(fdata, &strtmp, &umd->uvlayer_tmp,
			                        umd->uvlayer_name);
			but = uiDefButI(block, MENU, B_MODIFIER_RECALC, strtmp,
			      lx, (cy -= 19), buttonWidth, 19, &umd->uvlayer_tmp,
			      0.0, 1.0, 0, 0, "Set the UV layer to use");
			i = CustomData_get_layer_index(fdata, CD_MTFACE);
			uiButSetFunc(but, set_uvproject_uvlayer, umd, &fdata->layers[i]);
			MEM_freeN(strtmp);
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "AspX:",
			          lx, (cy -= 19), buttonWidth / 2, 19, &umd->aspectx,
			          1, 1000, 100, 2,
			          "Horizontal Aspect Ratio");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "AspY:",
			          lx + (buttonWidth / 2) + 1, cy, buttonWidth / 2, 19,
			          &umd->aspecty,
			          1, 1000, 100, 2,
			          "Vertical Aspect Ratio");
			uiDefButI(block, NUM, B_MODIFIER_RECALC, "Projectors:",
			          lx, (cy -= 19), buttonWidth, 19, &umd->num_projectors,
			          1, MOD_UVPROJECT_MAXPROJECTORS, 0, 0,
			          "Number of objects to use as projectors");
			for(i = 0; i < umd->num_projectors; ++i) {
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
				               "Ob: ", lx, (cy -= 19), buttonWidth, 19,
				               &umd->projectors[i],
				               "Object to use as projector");
			}
			uiDefIDPoinBut(block, modifier_testImage, ID_IM, B_CHANGEDEP,
			               "Image: ", lx, (cy -= 19), buttonWidth, 19,
			               &umd->image,
			               "Image to project (only faces with this image "
			               "will be altered");
			uiButSetCompleteFunc(but, autocomplete_image, (void *)ob);
			uiDefButBitI(block, TOG, MOD_UVPROJECT_OVERRIDEIMAGE,
			             B_MODIFIER_RECALC, "Override Image",
			             lx, (cy -= 19), buttonWidth, 19,
			             &umd->flags, 0, 0, 0, 0,
			             "Override faces' current images with the "
			             "given image");
		} else if (md->type==eModifierType_Decimate) {
			DecimateModifierData *dmd = (DecimateModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Ratio:",	lx,(cy-=19),buttonWidth,19, &dmd->percent, 0.0, 1.0, 10, 0, "Defines the percentage of triangles to reduce to");
			sprintf(str, "Face Count: %d", dmd->faceCount);
			uiDefBut(block, LABEL, 1, str,	lx, (cy-=19), 160,19, NULL, 0.0, 0.0, 0, 0, "Displays the current number of faces in the decimated mesh");
		} else if (md->type==eModifierType_Mask) {
			MaskModifierData *mmd = (MaskModifierData *)md;
			
			sprintf(str, "Mask Mode%%t|Vertex Group%%x%d|Selected Bones%%x%d|",
			        MOD_MASK_MODE_VGROUP,MOD_MASK_MODE_ARM);
			uiDefButI(block, MENU, B_MODIFIER_RECALC, str,
			        lx, (cy -= 19), buttonWidth, 19, &mmd->mode,
			        0.0, 1.0, 0, 0, "How masking region is defined");
					  
			if (mmd->mode == MOD_MASK_MODE_ARM) {
				uiDefIDPoinBut(block, modifier_testArmatureObj, ID_OB, B_CHANGEDEP,
				    "Ob: ", lx, (cy -= 19), buttonWidth, 19, &mmd->ob_arm,
				    "Armature to use as source of bones to mask");
			}
			else {
				but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",	
					lx, (cy-=19), buttonWidth, 19, &mmd->vgroup, 
					0.0, 31.0, 0, 0, "Vertex Group name");
				uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			}
			
			uiDefButBitI(block, TOG, MOD_MASK_INV, B_MODIFIER_RECALC, "Inverse",		
				lx, (cy-=19), buttonWidth, 19, &mmd->flag, 
				0, 0, 0, 0, "Use vertices that are not part of region defined");
		} else if (md->type==eModifierType_Smooth) {
			SmoothModifierData *smd = (SmoothModifierData*) md;

			uiDefButBitS(block, TOG, MOD_SMOOTH_X, B_MODIFIER_RECALC, "X",		lx,(cy-=19),45,19, &smd->flag, 0, 0, 0, 0, "Enable X axis smoothing");
			uiDefButBitS(block, TOG, MOD_SMOOTH_Y, B_MODIFIER_RECALC, "Y",		lx+45,cy,45,19, &smd->flag, 0, 0, 0, 0, "Enable Y axis smoothing");
			uiDefButBitS(block, TOG, MOD_SMOOTH_Z, B_MODIFIER_RECALC, "Z",		lx+90,cy,45,19, &smd->flag, 0, 0, 0, 0, "Enable Z axis smoothing");

			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Factor:",	lx,(cy-=19),buttonWidth, 19, &smd->fac, -10.0, 10.0, 0.5, 0, "Define the amount of smoothing, from 0.0 to 1.0 (lower / higher values can deform the mesh)");
			uiDefButS(block, NUM, B_MODIFIER_RECALC, "Repeat:",	lx,(cy-=19),buttonWidth, 19, &smd->repeat, 0.0, 30.0, 1, 0, "Number of smoothing iterations");
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-=19), buttonWidth,19, &smd->defgrp_name, 0.0, 31.0, 0, 0, "Vertex Group name to define which vertices are affected");
		} else if (md->type==eModifierType_Cast) {
			CastModifierData *cmd = (CastModifierData*) md;

			char casttypemenu[]="Projection Type%t|Sphere%x0|Cylinder%x1|Cuboid%x2";
			uiDefButS(block, MENU, B_MODIFIER_RECALC, casttypemenu,		lx,(cy-=19),buttonWidth - 30,19, &cmd->type, 0, 0, 0, 0, "Projection type to apply");
			uiDefButBitS(block, TOG, MOD_CAST_X, B_MODIFIER_RECALC, "X",		lx,(cy-=19),45,19, &cmd->flag, 0, 0, 0, 0, "Enable (local) X axis deformation");
			uiDefButBitS(block, TOG, MOD_CAST_Y, B_MODIFIER_RECALC, "Y",		lx+45,cy,45,19, &cmd->flag, 0, 0, 0, 0, "Enable (local) Y axis deformation");
			if (cmd->type != MOD_CAST_TYPE_CYLINDER) {
				uiDefButBitS(block, TOG, MOD_CAST_Z, B_MODIFIER_RECALC, "Z",		lx+90,cy,45,19, &cmd->flag, 0, 0, 0, 0, "Enable (local) Z axis deformation");
			}
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Factor:",	lx,(cy-=19),buttonWidth, 19, &cmd->fac, -10.0, 10.0, 5, 0, "Define the amount of deformation");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Radius:",	lx,(cy-=19),buttonWidth, 19, &cmd->radius, 0.0, 100.0, 10.0, 0, "Only deform vertices within this distance from the center of the effect (leave as 0 for infinite)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Size:",	lx,(cy-=19),buttonWidth, 19, &cmd->size, 0.0, 100.0, 10.0, 0, "Size of projection shape (leave as 0 for auto)");
			uiDefButBitS(block, TOG, MOD_CAST_SIZE_FROM_RADIUS, B_MODIFIER_RECALC, "From radius",		lx+buttonWidth,cy,80,19, &cmd->flag, 0, 0, 0, 0, "Use radius as size of projection shape (0 = auto)");
			if (ob->type == OB_MESH) {
				but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-=19), buttonWidth,19, &cmd->defgrp_name, 0.0, 31.0, 0, 0, "Vertex Group name to define which vertices are affected");
			}
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP, "Ob: ", lx,(cy-=19), buttonWidth,19, &cmd->object, "Control object: if available, its location determines the center of the effect");
			if(cmd->object) {
				uiDefButBitS(block, TOG, MOD_CAST_USE_OB_TRANSFORM, B_MODIFIER_RECALC, "Use transform",		lx+buttonWidth,cy,80,19, &cmd->flag, 0, 0, 0, 0, "Use object transform to control projection shape");
			}
		} else if (md->type==eModifierType_Wave) {
			WaveModifierData *wmd = (WaveModifierData*) md;
			uiDefButBitS(block, TOG, MOD_WAVE_X, B_MODIFIER_RECALC, "X",		lx,(cy-=19),45,19, &wmd->flag, 0, 0, 0, 0, "Enable X axis motion");
			uiDefButBitS(block, TOG, MOD_WAVE_Y, B_MODIFIER_RECALC, "Y",		lx+45,cy,45,19, &wmd->flag, 0, 0, 0, 0, "Enable Y axis motion");
			uiDefButBitS(block, TOG, MOD_WAVE_CYCL, B_MODIFIER_RECALC, "Cycl",	lx+90,cy,buttonWidth-90,19, &wmd->flag, 0, 0, 0, 0, "Enable cyclic wave effect");
			uiDefButBitS(block, TOG, MOD_WAVE_NORM, B_MODIFIER_RECALC, "Normals",	lx,(cy-=19),buttonWidth,19, &wmd->flag, 0, 0, 0, 0, "Displace along normals");
			if (wmd->flag & MOD_WAVE_NORM){
				if (ob->type==OB_MESH) {
					uiDefButBitS(block, TOG, MOD_WAVE_NORM_X, B_MODIFIER_RECALC, "X",	lx,(cy-=19),buttonWidth/3,19, &wmd->flag, 0, 0, 0, 0, "Enable displacement along the X normal");
					uiDefButBitS(block, TOG, MOD_WAVE_NORM_Y, B_MODIFIER_RECALC, "Y",	lx+(buttonWidth/3),cy,buttonWidth/3,19, &wmd->flag, 0, 0, 0, 0, "Enable displacement along the Y normal");
					uiDefButBitS(block, TOG, MOD_WAVE_NORM_Z, B_MODIFIER_RECALC, "Z",	lx+(buttonWidth/3)*2,cy,buttonWidth/3,19, &wmd->flag, 0, 0, 0, 0, "Enable displacement along the Z normal");
				}
				else
					uiDefBut(block, LABEL, 1, "Meshes Only",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");				
			}

			uiBlockBeginAlign(block);
			if(wmd->speed >= 0)
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Time sta:",	lx,(cy-=19),buttonWidth,19, &wmd->timeoffs, -MAXFRAMEF, MAXFRAMEF, 100, 0, "Specify starting frame of the wave");
			else
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Time end:",	lx,(cy-=19),buttonWidth,19, &wmd->timeoffs, -MAXFRAMEF, MAXFRAMEF, 100, 0, "Specify ending frame of the wave");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Lifetime:",	lx,(cy-=19),buttonWidth,19, &wmd->lifetime,  -MAXFRAMEF, MAXFRAMEF, 100, 0, "Specify the lifespan of the wave");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Damptime:",	lx,(cy-=19),buttonWidth,19, &wmd->damp,  -MAXFRAMEF, MAXFRAMEF, 100, 0, "Specify the dampingtime of the wave");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Falloff:",	lx,(cy-=19),buttonWidth,19, &wmd->falloff,  0, 100, 100, 0, "Specify the falloff radius of the waves");

			cy -= 9;
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Sta x:",		lx,(cy-=19),113,19, &wmd->startx, -100.0, 100.0, 100, 0, "Starting position for the X axis");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Sta y:",		lx+115,cy,105,19, &wmd->starty, -100.0, 100.0, 100, 0, "Starting position for the Y axis");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_MODIFIER_RECALC, "Ob: ", lx, (cy-=19), 220,19, &wmd->objectcenter, "Object to use as Starting Position (leave blank to disable)");
			uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",lx, (cy -= 19), 220, 19,&wmd->defgrp_name, 0.0, 31.0, 0, 0, "Name of vertex group with which to modulate displacement");
			uiDefIDPoinBut(block, modifier_testTexture, ID_TE, B_CHANGEDEP,"Texture: ", lx, (cy -= 19), 220, 19, &wmd->texture,"Texture with which to modulate wave");
			sprintf(str, "Texture Coordinates%%t"
			        "|Local%%x%d|Global%%x%d|Object%%x%d|UV%%x%d",
			        MOD_WAV_MAP_LOCAL, MOD_WAV_MAP_GLOBAL,
			        MOD_WAV_MAP_OBJECT, MOD_WAV_MAP_UV);
			uiDefButI(block, MENU, B_MODIFIER_RECALC, str,
			          lx, (cy -= 19), 220, 19, &wmd->texmapping,
			          0.0, 1.0, 0, 0,
			          "Texture coordinates used for modulation input");
			if (wmd->texmapping == MOD_WAV_MAP_UV) {
				char *strtmp;
				int i;
				Mesh *me = (Mesh*)ob->data;
				CustomData *fdata = obedit? &me->edit_mesh->fdata: &me->fdata;
				build_uvlayer_menu_vars(fdata, &strtmp, &wmd->uvlayer_tmp,
				                        wmd->uvlayer_name);
				but = uiDefButI(block, MENU, B_MODIFIER_RECALC, strtmp,
				      lx, (cy -= 19), 220, 19, &wmd->uvlayer_tmp,
				      0.0, 1.0, 0, 0, "Set the UV layer to use");
				MEM_freeN(strtmp);
				i = CustomData_get_layer_index(fdata, CD_MTFACE);
				uiButSetFunc(but, set_wave_uvlayer, wmd,
				             &fdata->layers[i]);
			}
			if(wmd->texmapping == MOD_DISP_MAP_OBJECT) {
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
				               "Ob: ", lx, (cy -= 19), 220, 19,
				               &wmd->map_object,
				               "Object to get texture coordinates from");
			}
			cy -= 9;
			uiBlockBeginAlign(block);
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Speed:",	lx,(cy-=19),220,19, &wmd->speed, -2.0, 2.0, 0, 0, "Specify the wave speed");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Height:",	lx,(cy-=19),220,19, &wmd->height, -2.0, 2.0, 0, 0, "Specify the amplitude of the wave");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Width:",	lx,(cy-=19),220,19, &wmd->width, 0.0, 5.0, 0, 0, "Specify the width of the wave");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Narrow:",	lx,(cy-=19),220,19, &wmd->narrow, 0.0, 10.0, 0, 0, "Specify how narrow the wave follows");
		} else if (md->type==eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData*) md;
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Falloff: ",		lx, (cy-=19), buttonWidth,19, &hmd->falloff, 0.0, 100.0, 100, 0, "If not zero, the distance from hook where influence ends");
			uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "Force: ",		lx, (cy-=19), buttonWidth,19, &hmd->force, 0.0, 1.0, 100, 0, "Set relative force of hook");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &hmd->object, "Parent Object for hook, also recalculates and clears offset"); 
			if(hmd->indexar==NULL) {
				but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",		lx, (cy-=19), buttonWidth,19, &hmd->name, 0.0, 31.0, 0, 0, "Vertex Group name");
				uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			}
			uiBlockBeginAlign(block);
			but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Reset", 		lx, (cy-=19), 80,19,			NULL, 0.0, 0.0, 0, 0, "Recalculate and clear offset (transform) of hook");
			uiButSetFunc(but, modifiers_clearHookOffset, ob, md);
			but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Recenter", 	lx+80, cy, buttonWidth-80,19,	NULL, 0.0, 0.0, 0, 0, "Sets hook center to cursor position");
			uiButSetFunc(but, modifiers_cursorHookCenter, ob, md);

			if (editing) {
				but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Select", 		lx, (cy-=19), 80,19, NULL, 0.0, 0.0, 0, 0, "Selects effected vertices on mesh");
				uiButSetFunc(but, modifiers_selectHook, ob, md);
				but = uiDefBut(block, BUT, B_MODIFIER_RECALC, "Reassign", 		lx+80, cy, buttonWidth-80,19, NULL, 0.0, 0.0, 0, 0, "Reassigns selected vertices to hook");
				uiButSetFunc(but, modifiers_reassignHook, ob, md);
			}
		} else if (md->type==eModifierType_Softbody) {
			uiDefBut(block, LABEL, 1, "See Soft Body panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_Cloth) {
			uiDefBut(block, LABEL, 1, "See Cloth panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");

		} else if (md->type==eModifierType_Collision) {
			uiDefBut(block, LABEL, 1, "See Collision panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_Surface) {
			uiDefBut(block, LABEL, 1, "See Fields panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_Fluidsim) {
			uiDefBut(block, LABEL, 1, "See Fluidsim panel.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_Boolean) {
			BooleanModifierData *bmd = (BooleanModifierData*) md;
			uiDefButI(block, MENU, B_MODIFIER_RECALC, "Operation%t|Intersect%x0|Union%x1|Difference%x2",	lx,(cy-=19),buttonWidth,19, &bmd->operation, 0.0, 1.0, 0, 0, "Boolean operation to perform");
			uiDefIDPoinBut(block, modifier_testMeshObj, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &bmd->object, "Mesh object to use for boolean operation");
		} else if (md->type==eModifierType_Array) {
			ArrayModifierData *amd = (ArrayModifierData*) md;
			float range = 10000;
			int cytop, halfwidth = (width - 5)/2 - 15;
			int halflx = lx + halfwidth + 10;

			// XXX uiBlockSetEmboss(block, UI_EMBOSSX);
			uiBlockEndAlign(block);

			/* length parameters */
			uiBlockBeginAlign(block);
			sprintf(str, "Length Fit%%t|Fixed Count%%x%d|Fixed Length%%x%d"
			        "|Fit To Curve Length%%x%d",
			        MOD_ARR_FIXEDCOUNT, MOD_ARR_FITLENGTH, MOD_ARR_FITCURVE);
			uiDefButI(block, MENU, B_MODIFIER_RECALC, str,
			          lx, (cy-=19), buttonWidth, 19, &amd->fit_type,
			          0.0, 1.0, 0, 0, "Array length calculation method");
			switch(amd->fit_type)
			{
			case MOD_ARR_FIXEDCOUNT:
				uiDefButI(block, NUM, B_MODIFIER_RECALC, "Count:",
				          lx, (cy -= 19), buttonWidth, 19, &amd->count,
				          1, 1000, 0, 0, "Number of duplicates to make");
				break;
			case MOD_ARR_FITLENGTH:
				uiDefButF(block, NUM, B_MODIFIER_RECALC, "Length:",
				          lx, (cy -= 19), buttonWidth, 19, &amd->length,
				          0, range, 10, 2,
				          "Length to fit array within");
				break;
			case MOD_ARR_FITCURVE:
				uiDefIDPoinBut(block, modifier_testCurveObj, ID_OB,
				               B_CHANGEDEP, "Ob: ",
				               lx, (cy -= 19), buttonWidth, 19, &amd->curve_ob,
				               "Curve object to fit array length to");
				break;
			}
			uiBlockEndAlign(block);

			/* offset parameters */
			cy -= 10;
			cytop= cy;
			uiBlockBeginAlign(block);
			uiDefButBitI(block, TOG, MOD_ARR_OFF_CONST, B_MODIFIER_RECALC,
			             "Constant Offset", lx, (cy-=19), halfwidth, 19,
			             &amd->offset_type, 0, 0, 0, 0,
			             "Constant offset between duplicates "
			             "(local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "X:",
			          lx, (cy-=19), halfwidth, 19,
			          &amd->offset[0],
			          -range, range, 10, 3,
			          "Constant component for duplicate offsets "
			          "(local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Y:",
			          lx, (cy-=19), halfwidth, 19,
			          &amd->offset[1],
			          -range, range, 10, 3,
			          "Constant component for duplicate offsets "
			          "(local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Z:",
			          lx, (cy-=19), halfwidth, 19,
			          &amd->offset[2],
			          -range, range, 10, 3,
			          "Constant component for duplicate offsets "
			          "(local coordinates)");
			uiBlockEndAlign(block);

			cy= cytop;
			uiBlockBeginAlign(block);
			uiDefButBitI(block, TOG, MOD_ARR_OFF_RELATIVE, B_MODIFIER_RECALC,
			             "Relative Offset", halflx, (cy-=19), halfwidth, 19,
			             &amd->offset_type, 0, 0, 0, 0,
			             "Offset between duplicates relative to object width "
			             "(local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "X:",
			          halflx, (cy-=19), halfwidth, 19,
			          &amd->scale[0],
			          -range, range, 10, 3,
			          "Component for duplicate offsets relative to object "
			          "width (local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Y:",
			          halflx, (cy-=19), halfwidth, 19,
			          &amd->scale[1],
			          -range, range, 10, 3,
			          "Component for duplicate offsets relative to object "
			          "width (local coordinates)");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Z:",
			          halflx, (cy-=19), halfwidth, 19,
			          &amd->scale[2],
			          -range, range, 10, 3,
			          "Component for duplicate offsets relative to object "
			          "width (local coordinates)");
			uiBlockEndAlign(block);

			/* vertex merging parameters */
			cy -= 10;
			cytop= cy;

			uiBlockBeginAlign(block);
			uiDefButBitI(block, TOG, MOD_ARR_MERGE, B_MODIFIER_RECALC,
			             "Merge",
			             lx, (cy-=19), halfwidth/2, 19, &amd->flags,
			             0, 0, 0, 0,
			             "Merge vertices in adjacent duplicates");
			uiDefButBitI(block, TOG, MOD_ARR_MERGEFINAL, B_MODIFIER_RECALC,
			             "First Last",
			             lx + halfwidth/2, cy, (halfwidth+1)/2, 19,
			             &amd->flags,
			             0, 0, 0, 0,
			             "Merge vertices in first duplicate with vertices"
			             " in last duplicate");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Limit:",
					  lx, (cy-=19), halfwidth, 19, &amd->merge_dist,
					  0, 1.0f, 1, 4,
					  "Limit below which to merge vertices");

			/* offset ob */
			cy = cytop;
			uiBlockBeginAlign(block);
			uiDefButBitI(block, TOG, MOD_ARR_OFF_OBJ, B_MODIFIER_RECALC,
			             "Object Offset", halflx, (cy -= 19), halfwidth, 19,
			             &amd->offset_type, 0, 0, 0, 0,
			             "Add an object transformation to the total offset");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP,
			               "Ob: ", halflx, (cy -= 19), halfwidth, 19,
			               &amd->offset_ob,
			               "Object from which to take offset transformation");
			uiBlockEndAlign(block);

			cy -= 10;
			but = uiDefIDPoinBut(block, test_meshobpoin_but, ID_OB,
			                     B_CHANGEDEP, "Start cap: ",
			                     lx, (cy -= 19), halfwidth, 19,
			                     &amd->start_cap,
			                     "Mesh object to use as start cap");
			uiButSetCompleteFunc(but, autocomplete_meshob, (void *)ob);
			but = uiDefIDPoinBut(block, test_meshobpoin_but, ID_OB,
			                     B_CHANGEDEP, "End cap: ",
			                     halflx, cy, halfwidth, 19,
			                     &amd->end_cap,
			                     "Mesh object to use as end cap");
			uiButSetCompleteFunc(but, autocomplete_meshob, (void *)ob);
		} else if (md->type==eModifierType_MeshDeform) {
			MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

			uiBlockBeginAlign(block);
			uiDefIDPoinBut(block, test_meshobpoin_but, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy-=19), buttonWidth,19, &mmd->object, "Mesh object to be use as cage"); 
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",				  lx, (cy-19), buttonWidth-40,19, &mmd->defgrp_name, 0.0, 31.0, 0, 0, "Vertex Group name to control overall meshdeform influence");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);
			uiDefButBitS(block, TOG, MOD_MDEF_INVERT_VGROUP, B_MODIFIER_RECALC, "Inv", lx+buttonWidth-40, (cy-=19), 40,19, &mmd->flag, 0.0, 31.0, 0, 0, "Invert vertex group influence");

			uiBlockBeginAlign(block);
			if(mmd->bindcos) {
				but= uiDefBut(block, BUT, B_MODIFIER_RECALC, "Unbind", lx,(cy-=24), buttonWidth,19, 0, 0, 0, 0, 0, "Unbind mesh from cage");
				uiButSetFunc(but,modifiers_bindMeshDeform,ob,md);
			}
			else {
				but= uiDefBut(block, BUT, B_MODIFIER_RECALC, "Bind", lx,(cy-=24), buttonWidth,19, 0, 0, 0, 0, 0, "Bind mesh to cage");
				uiButSetFunc(but,modifiers_bindMeshDeform,ob,md);
				uiDefButS(block, NUM, B_NOP, "Precision:", lx,(cy-19), buttonWidth/2 + 20,19, &mmd->gridsize, 2, 10, 0.5, 0, "The grid size for binding");
				uiDefButBitS(block, TOG, MOD_MDEF_DYNAMIC_BIND, B_MODIFIER_RECALC, "Dynamic", lx+(buttonWidth+1)/2 + 20, (cy-=19), buttonWidth/2 - 20,19, &mmd->flag, 0.0, 31.0, 0, 0, "Recompute binding dynamically on top of other deformers like Shape Keys (slower and more memory consuming!)");
			}
			uiBlockEndAlign(block);
		} else if (md->type==eModifierType_ParticleSystem) {
			uiDefBut(block, LABEL, 1, "See Particle buttons.",	lx, (cy-=19), buttonWidth,19, NULL, 0.0, 0.0, 0, 0, "");
		} else if (md->type==eModifierType_ParticleInstance) {
			ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData*) md;
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP, "Ob: ", lx, (cy -= 19), buttonWidth, 19, &pimd->ob, "Object that has the particlesystem");
			uiDefButS(block, NUM, B_MODIFIER_RECALC, "PSYS:", lx, (cy -= 19), buttonWidth, 19, &pimd->psys, 1, 10, 10, 3, "Particlesystem number in the object");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Parents, B_MODIFIER_RECALC, "Normal",	lx, (cy -= 19), buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Create instances from normal particles");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Children, B_MODIFIER_RECALC, "Children",	lx+buttonWidth/3, cy, buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Create instances from child particles");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Path, B_MODIFIER_RECALC, "Path",	lx+buttonWidth*2/3, cy, buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Create instances along particle paths");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Unborn, B_MODIFIER_RECALC, "Unborn",	lx, (cy -= 19), buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Show instances when particles are unborn");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Alive, B_MODIFIER_RECALC, "Alive",	lx+buttonWidth/3, cy, buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Show instances when particles are alive");
			uiDefButBitS(block, TOG, eParticleInstanceFlag_Dead, B_MODIFIER_RECALC, "Dead",	lx+buttonWidth*2/3, cy, buttonWidth/3,19, &pimd->flag, 0, 0, 0, 0, "Show instances when particles are dead");
		} else if (md->type==eModifierType_Explode) {
			ExplodeModifierData *emd = (ExplodeModifierData*) md;
			uiBut *but;
			char *menustr= NULL; /* XXX get_vertexgroup_menustr(ob); */
			int defCount=BLI_countlist(&ob->defbase);
			if(defCount==0) emd->vgroup=0;
			uiBlockBeginAlign(block);
			but=uiDefButS(block, MENU, B_MODIFIER_RECALC, menustr,	lx, (cy-=19), buttonWidth-20,19, &emd->vgroup, 0, defCount, 0, 0, "Protect this vertex group");
			uiButSetFunc(but,modifiers_explodeFacepa,emd,0);
			MEM_freeN(menustr);
			
			but=uiDefIconBut(block, BUT, B_MODIFIER_RECALC, ICON_X, (lx+buttonWidth)-20, cy, 20,19, 0, 0, 0, 0, 0, "Disable use of vertex group");
			uiButSetFunc(but, modifiers_explodeDelVg, (void *)emd, (void *)NULL);
			

			but=uiDefButF(block, NUMSLI, B_MODIFIER_RECALC, "",	lx, (cy-=19), buttonWidth,19, &emd->protect, 0.0f, 1.0f, 0, 0, "Clean vertex group edges");
			uiButSetFunc(but,modifiers_explodeFacepa,emd,0);

			but=uiDefBut(block, BUT, B_MODIFIER_RECALC, "Refresh",	lx, (cy-=19), buttonWidth/2,19, 0, 0, 0, 0, 0, "Recalculate faces assigned to particles");
			uiButSetFunc(but,modifiers_explodeFacepa,emd,0);

			uiDefButBitS(block, TOG, eExplodeFlag_EdgeSplit, B_MODIFIER_RECALC, "Split Edges",	lx+buttonWidth/2, cy, buttonWidth/2,19, &emd->flag, 0, 0, 0, 0, "Split face edges for nicer shrapnel");
			uiDefButBitS(block, TOG, eExplodeFlag_Unborn, B_MODIFIER_RECALC, "Unborn",	lx, (cy-=19), buttonWidth/3,19, &emd->flag, 0, 0, 0, 0, "Show mesh when particles are unborn");
			uiDefButBitS(block, TOG, eExplodeFlag_Alive, B_MODIFIER_RECALC, "Alive",	lx+buttonWidth/3, cy, buttonWidth/3,19, &emd->flag, 0, 0, 0, 0, "Show mesh when particles are alive");
			uiDefButBitS(block, TOG, eExplodeFlag_Dead, B_MODIFIER_RECALC, "Dead",	lx+buttonWidth*2/3, cy, buttonWidth/3,19, &emd->flag, 0, 0, 0, 0, "Show mesh when particles are dead");
			uiBlockEndAlign(block);
		} else if (md->type==eModifierType_Shrinkwrap) {
			ShrinkwrapModifierData *smd = (ShrinkwrapModifierData*) md;

			char shrinktypemenu[]="Shrinkwrap type%t|nearest surface point %x0|projection %x1|nearest vertex %x2";

			uiDefIDPoinBut(block, modifier_testMeshObj, ID_OB, B_CHANGEDEP, "Ob: ",	lx, (cy-=19), buttonWidth,19, &smd->target, "Target to shrink to");

			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",		lx, (cy-=19), buttonWidth,19, &smd->vgroup_name, 0, 31, 0, 0, "Vertex Group name");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);

			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Offset:",	lx,(cy-=19),buttonWidth,19, &smd->keepDist, 0.0f, 100.0f, 1.0f, 0, "Specify distance to keep from the target");

			cy -= 3;
			uiDefButS(block, MENU, B_MODIFIER_RECALC, shrinktypemenu, lx,(cy-=19),buttonWidth,19, &smd->shrinkType, 0, 0, 0, 0, "Selects type of shrinkwrap algorithm for target position.");

			uiDefButC(block, NUM, B_MODIFIER_RECALC, "SS Levels:",		lx, (cy-=19), buttonWidth,19, &smd->subsurfLevels, 0, 6, 0, 0, "This indicates the number of CCSubdivisions that must be performed before extracting vertexs positions and normals");

			if (smd->shrinkType == MOD_SHRINKWRAP_PROJECT){


				/* UI for projection axis */
				uiBlockBeginAlign(block);
				uiDefButC(block, ROW, B_MODIFIER_RECALC, "Normal"    , lx,(cy-=19),buttonWidth,19, &smd->projAxis, 18.0, MOD_SHRINKWRAP_PROJECT_OVER_NORMAL, 0, 0, "Projection over X axis");

				uiDefButBitC(block, TOG, MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS, B_MODIFIER_RECALC, "X",	lx+buttonWidth/3*0,(cy-=19),buttonWidth/3,19, &smd->projAxis, 0, 0, 0, 0, "Projection over X axis");
				uiDefButBitC(block, TOG, MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS, B_MODIFIER_RECALC, "Y",	lx+buttonWidth/3*1,cy,buttonWidth/3,19, &smd->projAxis, 0, 0, 0, 0, "Projection over Y axis");
				uiDefButBitC(block, TOG, MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS, B_MODIFIER_RECALC, "Z",	lx+buttonWidth/3*2,cy,buttonWidth/3,19, &smd->projAxis, 0, 0, 0, 0, "Projection over Z axis");


				/* allowed directions of projection axis */
				uiDefButBitS(block, TOG, MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR, B_MODIFIER_RECALC, "Negative",	lx,(cy-=19),buttonWidth/2,19, &smd->shrinkOpts, 0, 0, 0, 0, "Allows to move the vertex in the negative direction of axis");
				uiDefButBitS(block, TOG, MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR, B_MODIFIER_RECALC, "Positive",	lx + buttonWidth/2,cy,buttonWidth/2,19, &smd->shrinkOpts, 0, 0, 0, 0, "Allows to move the vertex in the positive direction of axis");

				uiDefButBitS(block, TOG, MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE, B_MODIFIER_RECALC, "Cull frontfaces",lx,(cy-=19),buttonWidth/2,19, &smd->shrinkOpts, 0, 0, 0, 0, "Controls whether a vertex can be projected to a front face on target");
				uiDefButBitS(block, TOG, MOD_SHRINKWRAP_CULL_TARGET_BACKFACE,  B_MODIFIER_RECALC, "Cull backfaces",	lx+buttonWidth/2,cy,buttonWidth/2,19, &smd->shrinkOpts, 0, 0, 0, 0, "Controls whether a vertex can be projected to a back face on target");
				uiDefIDPoinBut(block, modifier_testMeshObj, ID_OB, B_CHANGEDEP, "Ob2: ",	lx, (cy-=19), buttonWidth,19, &smd->auxTarget, "Aditional mesh to project over");
			}
			else if (smd->shrinkType == MOD_SHRINKWRAP_NEAREST_SURFACE){
				uiDefButBitS(block, TOG, MOD_SHRINKWRAP_KEEP_ABOVE_SURFACE, B_MODIFIER_RECALC, "Above surface",	lx,(cy-=19),buttonWidth,19, &smd->shrinkOpts, 0, 0, 0, 0, "Vertices are kept on the front side of faces");
			}

			uiBlockEndAlign(block);

		} else if (md->type==eModifierType_SimpleDeform) {
			SimpleDeformModifierData *smd = (SimpleDeformModifierData*) md;
			char simpledeform_modemenu[] = "Deform type%t|Twist %x1|Bend %x2|Taper %x3|Strech %x4";

			uiDefButC(block, MENU, B_MODIFIER_RECALC, simpledeform_modemenu, lx,(cy-=19),buttonWidth,19, &smd->mode, 0, 0, 0, 0, "Selects type of deform to apply to object.");
			
			but=uiDefBut(block, TEX, B_MODIFIER_RECALC, "VGroup: ",		lx, (cy-=19), buttonWidth,19, &smd->vgroup_name, 0, 31, 0, 0, "Vertex Group name");
			uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ob);

			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CHANGEDEP, "Ob: ",	lx, (cy-=19), buttonWidth,19, &smd->origin, "Origin of modifier space coordinates");
			if(smd->origin != NULL)
				uiDefButBitC(block, TOG, MOD_SIMPLEDEFORM_ORIGIN_LOCAL, B_MODIFIER_RECALC, "Relative",lx,(cy-=19),buttonWidth,19, &smd->originOpts, 0, 0, 0, 0, "Sets the origin of deform space to be relative to the object");

			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Factor:",	lx,(cy-=19),buttonWidth,19, &smd->factor, -10.0f, 10.0f, 0.5f, 0, "Deform Factor");

			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Upper Limit:",	lx,(cy-=19),buttonWidth,19, &smd->limit[1], 0.0f, 1.0f, 5.0f, 0, "Upper Limit for deform");
			uiDefButF(block, NUM, B_MODIFIER_RECALC, "Lower Limit:",	lx,(cy-=19),buttonWidth,19, &smd->limit[0], 0.0f, 1.0f, 5.0f, 0, "Lower Limit for deform");

			if(smd->mode == MOD_SIMPLEDEFORM_MODE_STRETCH
			|| smd->mode == MOD_SIMPLEDEFORM_MODE_TAPER  )
			{
				uiDefButBitC(block, TOG, MOD_SIMPLEDEFORM_LOCK_AXIS_X, B_MODIFIER_RECALC, "Loc X", lx,             (cy-=19),buttonWidth/2,19, &smd->axis, 0, 0, 0, 0, "Disallow changes on the X coordinate");
				uiDefButBitC(block, TOG, MOD_SIMPLEDEFORM_LOCK_AXIS_Y, B_MODIFIER_RECALC, "Loc Y", lx+(buttonWidth/2), (cy),buttonWidth/2,19, &smd->axis, 0, 0, 0, 0, "Disallow changes on the Y coordinate");
			}
		}

		uiBlockEndAlign(block);
	}

	if (md->error) {

		row = uiLayoutRow(uiLayoutBox(column), 0);

		/* XXX uiBlockSetCol(block, color); */
		uiItemL(row, md->error, ICON_ERROR);
		/* XXX uiBlockSetCol(block, TH_AUTO); */
	}

	return result;
}

uiLayout *uiTemplateModifier(uiLayout *layout, bContext *C, PointerRNA *ptr)
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
	
	uiBlockSetButLock(uiLayoutBlock(layout), (ob && ob->id.lib), ERROR_LIBDATA_MESSAGE);
	
	/* find modifier and draw it */
	cageIndex = modifiers_getCageIndex(ob, &lastCageIndex);

	// XXX virtual modifiers are not accesible for python
	vmd = modifiers_getVirtualModifierList(ob);

	for(i=0; vmd; i++, vmd=vmd->next) {
		if(md == vmd)
			return draw_modifier(C, layout, ob, md, i, cageIndex, lastCageIndex);
		else if(vmd->mode&eModifierMode_Virtual)
			i--;
	}

	return NULL;
}

