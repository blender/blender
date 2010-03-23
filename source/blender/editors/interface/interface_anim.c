
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


#include "ED_keyframing.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

static FCurve *ui_but_get_fcurve(uiBut *but, bAction **action, int *driven)
{
	return rna_get_fcurve(&but->rnapoin, but->rnaprop, but->rnaindex, action, driven);
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

void ui_but_anim_autokey(bContext *C, uiBut *but, Scene *scene, float cfra)
{
	ID *id;
	bAction *action;
	FCurve *fcu;
	int driven;

	fcu= ui_but_get_fcurve(but, &action, &driven);

	if(fcu && !driven) {
		id= but->rnapoin.id.data;
		
		// TODO: this should probably respect the keyingset only option for anim
		if(autokeyframe_cfra_can_key(scene, id)) {
			short flag = ANIM_get_keyframing_flags(scene, 1);
			
			fcu->flag &= ~FCURVE_SELECTED;
			insert_keyframe(id, action, ((fcu->grp)?(fcu->grp->name):(NULL)), fcu->rna_path, fcu->array_index, cfra, flag);
			WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
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
				if((but->active || but->flag & UI_BUT_LAST_ACTIVE) && but->rnapoin.id.data) {
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
	WM_operator_name_call(C, "ANIM_OT_keyframe_insert_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_delete_keyframe(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_keyframe_delete_button", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_add_driver(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_driver_button_add", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_remove_driver(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_driver_button_remove", WM_OP_INVOKE_DEFAULT, NULL);
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
	WM_operator_name_call(C, "ANIM_OT_keyingset_button_add", WM_OP_INVOKE_DEFAULT, NULL);
}

void ui_but_anim_remove_keyingset(bContext *C)
{
	/* this operator calls uiAnimContextProperty above */
	WM_operator_name_call(C, "ANIM_OT_keyingset_button_remove", WM_OP_INVOKE_DEFAULT, NULL);
}
