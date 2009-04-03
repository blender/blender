
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

void ui_but_anim_flag(uiBut *but, float cfra)
{
	but->flag &= ~(UI_BUT_ANIMATED|UI_BUT_ANIMATED_KEY);

	if(but->rnaprop && but->rnapoin.id.data) {
		AnimData *adt= BKE_animdata_from_id(but->rnapoin.id.data);
		FCurve *fcu;
		char *path;

		if(adt && adt->action && adt->action->curves.first) {
			/* XXX this function call can become a performance bottleneck */
			path= RNA_path_from_ID_to_property(&but->rnapoin, but->rnaprop);

			if(path) {
				fcu= list_find_fcurve(&adt->action->curves, path, but->rnaindex);

				if(fcu) {
					but->flag |= UI_BUT_ANIMATED;

					if(on_keyframe_fcurve(fcu, cfra))
						but->flag |= UI_BUT_ANIMATED_KEY;
				}

				MEM_freeN(path);
			}
		}
	}
}

void uiAnimContextProperty(const bContext *C, struct PointerRNA *ptr, struct PropertyRNA **prop, int *index)
{
	ARegion *ar= CTX_wm_region(C);
	uiBlock *block;
	uiBut *but;

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

void ui_but_anim_menu(bContext *C, uiBut *but)
{
	uiMenuItem *head;
	int length;

	if(but->rnapoin.data && but->rnaprop) {
		head= uiPupMenuBegin(RNA_property_ui_name(&but->rnapoin, but->rnaprop), 0);

		length= RNA_property_array_length(&but->rnapoin, but->rnaprop);

		if(but->flag & UI_BUT_ANIMATED_KEY) {
			if(length) {
				uiMenuItemBooleanO(head, "Delete Keyframes", 0, "ANIM_OT_delete_keyframe_button", "all", 1);
				uiMenuItemBooleanO(head, "Delete Single Keyframe", 0, "ANIM_OT_delete_keyframe_button", "all", 0);
			}
			else {
				uiMenuItemBooleanO(head, "Delete Keyframe", 0, "ANIM_OT_delete_keyframe_button", "all", 0);
			}
		}
		else if(RNA_property_animateable(&but->rnapoin, but->rnaprop)) {
			if(length) {
				uiMenuItemBooleanO(head, "Insert Keyframes", 0, "ANIM_OT_insert_keyframe_button", "all", 1);
				uiMenuItemBooleanO(head, "Insert Single Keyframe", 0, "ANIM_OT_insert_keyframe_button", "all", 0);
			}
			else {
				uiMenuItemBooleanO(head, "Insert Keyframe", 0, "ANIM_OT_insert_keyframe_button", "all", 0);
			}
		}

		uiPupMenuEnd(C, head);
	}
}

