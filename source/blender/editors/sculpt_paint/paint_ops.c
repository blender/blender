
#include "ED_sculpt.h"

#include "WM_api.h"
#include "WM_types.h"

#include "paint_intern.h"

/**************************** registration **********************************/

void ED_operatortypes_paint(void)
{
	/* image */
	WM_operatortype_append(PAINT_OT_texture_paint_toggle);
	WM_operatortype_append(PAINT_OT_texture_paint_radial_control);
	WM_operatortype_append(PAINT_OT_image_paint);
	WM_operatortype_append(PAINT_OT_image_paint_radial_control);
	WM_operatortype_append(PAINT_OT_sample_color);
	WM_operatortype_append(PAINT_OT_grab_clone);
	WM_operatortype_append(PAINT_OT_set_clone_cursor);

	/* weight */
	WM_operatortype_append(PAINT_OT_weight_paint_toggle);
	WM_operatortype_append(PAINT_OT_weight_paint_radial_control);
	WM_operatortype_append(PAINT_OT_weight_paint);

	/* vertex */
	WM_operatortype_append(PAINT_OT_vertex_paint_radial_control);
	WM_operatortype_append(PAINT_OT_vertex_paint_toggle);
	WM_operatortype_append(PAINT_OT_vertex_paint);
}

