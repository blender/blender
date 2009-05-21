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

