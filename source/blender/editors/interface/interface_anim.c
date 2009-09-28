
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "ED_keyframing.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

static FCurve *ui_but_get_fcurve(uiBut *but, bAction **action, int *driven)
{
	FCurve *fcu= NULL;

	*driven= 0;

	/* there must be some RNA-pointer + property combo for this button */
	if(but->rnaprop && but->rnapoin.id.data && 
		RNA_property_animateable(&but->rnapoin, but->rnaprop)) 
	{
		AnimData *adt= BKE_animdata_from_id(but->rnapoin.id.data);
		char *path;
		
		if(adt) {
			if((adt->action && adt->action->curves.first) || (adt->drivers.first)) {
				/* XXX this function call can become a performance bottleneck */
				path= RNA_path_from_ID_to_property(&but->rnapoin, but->rnaprop);

				if(path) {
					/* animation takes priority over drivers */
					if(adt->action && adt->action->curves.first)
						fcu= list_find_fcurve(&adt->action->curves, path, but->rnaindex);
					
					/* if not animated, check if driven */
					if(!fcu && (adt->drivers.first)) {
						fcu= list_find_fcurve(&adt->drivers, path, but->rnaindex);
						
						if(fcu)
							*driven= 1;
					}

					if(fcu && action)
						*action= adt->action;

					MEM_freeN(path);
				}
			}
		}
	}

	return fcu;
}

void ui_but_anim_flag(uiBut *but, float cfra)
{
	FCurve *fcu;
	int driven;

	but->flag &= ~(UI_BUT_ANIMATED|UI_BUT_ANIMATED_KEY|UI_BUT_DRIVEN);

	fcu= ui_but_get_fcurve(but, NULL, &driven);

	if(fcu) {
		if(!driven) {
			but->flag |= UI_BUT_ANIMATED;
			
			if(fcurve_frame_has_keyframe(fcu, cfra, 0))
				but->flag |= UI_BUT_ANIMATED_KEY;
		}
		else {
			but->flag |= UI_BUT_DRIVEN;
		}
	}
}

int ui_but_anim_expression_get(uiBut *but, char *str, int maxlen)
{
	FCurve *fcu;
	ChannelDriver *driver;
	int driven;

	fcu= ui_but_get_fcurve(but, NULL, &driven);

	if(fcu && driven) {
		driver= fcu->driver;

		if(driver && driver->type == DRIVER_TYPE_PYTHON) {
			BLI_strncpy(str, driver->expression, maxlen);
			return 1;
		}
	}

	return 0;
}

int ui_but_anim_expression_set(uiBut *but, const char *str)
{
	FCurve *fcu;
	ChannelDriver *driver;
	int driven;

	fcu= ui_but_get_fcurve(but, NULL, &driven);

	if(fcu && driven) {
		driver= fcu->driver;

		if(driver && driver->type == DRIVER_TYPE_PYTHON) {
			BLI_strncpy(driver->expression, str, sizeof(driver->expression));
			return 1;
		}
	}

	return 0;
}

void ui_but_anim_autokey(uiBut *but, Scene *scene, float cfra)
{
	ID *id;
	bAction *action;
	FCurve *fcu;
	int driven;

	fcu= ui_but_get_fcurve(but, &action, &driven);

	if(fcu && !driven) {
		id= but->rnapoin.id.data;
		
		if(autokeyframe_cfra_can_key(scene, id)) {
			short flag = 0;
			
			if (IS_AUTOKEY_FLAG(INSERTNEEDED))
				flag |= INSERTKEY_NEEDED;
			if (IS_AUTOKEY_FLAG(AUTOMATKEY))
				flag |= INSERTKEY_MATRIX;
			if (IS_AUTOKEY_MODE(scene, EDITKEYS))
				flag |= INSERTKEY_REPLACE;
			
			fcu->flag &= ~FCURVE_SELECTED;
			insert_keyframe(id, action, ((fcu->grp)?(fcu->grp->name):(NULL)), fcu->rna_path, fcu->array_index, cfra, flag);
		}
	}
}

void uiAnimContextProperty(const bContext *C, struct PointerRNA *ptr, struct PropertyRNA **prop, int *index)
{
	ARegion *ar= CTX_wm_region(C);
	uiBlock *block;
	uiBut *but;

	memset(ptr, 0, sizeof(*ptr));
	*prop= NULL;
	*index= 0;

	if(ar) {
		for(block=ar->uiblocks.first; block; block=block->next) {
			for(but=block->buttons.first; but; but= but->next) {
				if(but->active && but->rnapoin.id.data) {
					*ptr= but->rnapoin;
					*prop= but->rnaprop;
					*index= but->rnaindex;
					return;
				}
			}
		}
	}
}

void ui_but_anim_insert_keyframe(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_insert_keyframe_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_delete_keyframe(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_delete_keyframe_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_add_driver(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_add_driver_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_remove_driver(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_remove_driver_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_copy_driver(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_copy_driver_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_paste_driver(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_paste_driver_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_add_keyingset(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_add_keyingset_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_remove_keyingset(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_remove_keyingset_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_menu(bContext *C, uiBut *but)
{
	uiPopupMenu *pup;
	uiLayout *layout;
	int length;

	if(but->rnapoin.data && but->rnaprop) {
		pup= uiPupMenuBegin(C, RNA_property_ui_name(but->rnaprop), 0);
		layout= uiPupMenuLayout(pup);
		
		length= RNA_property_array_length(&but->rnapoin, but->rnaprop);
		
		if(but->flag & UI_BUT_ANIMATED_KEY) {
			if(length) {
				uiItemBooleanO(layout, "Replace Keyframes", 0, "ANIM_OT_insert_keyframe_button", "all", 1);
				uiItemBooleanO(layout, "Replace Single Keyframe", 0, "ANIM_OT_insert_keyframe_button", "all", 0);
				uiItemBooleanO(layout, "Delete Keyframes", 0, "ANIM_OT_delete_keyframe_button", "all", 1);
				uiItemBooleanO(layout, "Delete Single Keyframe", 0, "ANIM_OT_delete_keyframe_button", "all", 0);
			}
			else {
				uiItemBooleanO(layout, "Replace Keyframe", 0, "ANIM_OT_insert_keyframe_button", "all", 0);
				uiItemBooleanO(layout, "Delete Keyframe", 0, "ANIM_OT_delete_keyframe_button", "all", 0);
			}
		}
		else if(but->flag & UI_BUT_DRIVEN);
		else if(RNA_property_animateable(&but->rnapoin, but->rnaprop)) {
			if(length) {
				uiItemBooleanO(layout, "Insert Keyframes", 0, "ANIM_OT_insert_keyframe_button", "all", 1);
				uiItemBooleanO(layout, "Insert Single Keyframe", 0, "ANIM_OT_insert_keyframe_button", "all", 0);
			}
			else 
				uiItemBooleanO(layout, "Insert Keyframe", 0, "ANIM_OT_insert_keyframe_button", "all", 0);
		}
		
		if(but->flag & UI_BUT_DRIVEN) {
			uiItemS(layout);
			
			if(length) {
				uiItemBooleanO(layout, "Delete Drivers", 0, "ANIM_OT_remove_driver_button", "all", 1);
				uiItemBooleanO(layout, "Delete Single Driver", 0, "ANIM_OT_remove_driver_button", "all", 0);
			}
			else
				uiItemBooleanO(layout, "Delete Driver", 0, "ANIM_OT_remove_driver_button", "all", 0);
				
			uiItemO(layout, "Copy Driver", 0, "ANIM_OT_copy_driver_button");
			if (ANIM_driver_can_paste())
				uiItemO(layout, "Paste Driver", 0, "ANIM_OT_paste_driver_button");
		}
		else if(but->flag & UI_BUT_ANIMATED_KEY);
		else if(RNA_property_animateable(&but->rnapoin, but->rnaprop)) {
			uiItemS(layout);
			
			if(length) {
				uiItemBooleanO(layout, "Add Drivers", 0, "ANIM_OT_add_driver_button", "all", 1);
				uiItemBooleanO(layout, "Add Single Driver", 0, "ANIM_OT_add_driver_button", "all", 0);
			}
			else
				uiItemBooleanO(layout, "Add Driver", 0, "ANIM_OT_add_driver_button", "all", 0);
			
			if (ANIM_driver_can_paste())			
				uiItemO(layout, "Paste Driver", 0, "ANIM_OT_paste_driver_button");
		}
		
		if(RNA_property_animateable(&but->rnapoin, but->rnaprop)) {
			uiItemS(layout);
			
			if(length) {
				uiItemBooleanO(layout, "Add All to Keying Set", 0, "ANIM_OT_add_keyingset_button", "all", 1);
				uiItemBooleanO(layout, "Add Single to Keying Set", 0, "ANIM_OT_add_keyingset_button", "all", 0);
				uiItemO(layout, "Remove from Keying Set", 0, "ANIM_OT_remove_keyingset_button");
			}
			else {
				uiItemBooleanO(layout, "Add to Keying Set", 0, "ANIM_OT_add_keyingset_button", "all", 0);
				uiItemO(layout, "Remove from Keying Set", 0, "ANIM_OT_remove_keyingset_button");
			}
		}

		uiPupMenuEnd(C, pup);
	}
}

