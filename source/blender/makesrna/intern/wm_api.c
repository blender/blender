#include "WM_api.h"

#include "BKE_context.h"

void RNA_api_wm_add_fileselect(wmWindowManager *self, bContext *C, wmOperator *op)
{
	WM_event_add_fileselect(C, op);
}

