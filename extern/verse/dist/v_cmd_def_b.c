
#include <stdlib.h>
#include <stdio.h>

#include "verse_header.h"
#include "v_cmd_gen.h"
#include "v_cmd_buf.h"

#if defined(V_GENERATE_FUNC_MODE)

void v_gen_bitmap_cmd_def(void)
{
	v_cg_new_cmd(V_NT_BITMAP,		"b_dimensions_set", 80, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_UINT16,		"width");
	v_cg_add_param(VCGP_UINT16,		"height");
	v_cg_add_param(VCGP_UINT16,		"depth");
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_BITMAP,		"b_layer_create", 81, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_LAYER_ID,	"layer_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_NAME,		"name");
	v_cg_add_param(VCGP_ENUM_NAME,	"VNBLayerType");
	v_cg_add_param(VCGP_ENUM,		"type");
	v_cg_alias(FALSE, "b_layer_destroy", "if(name[0] == 0)", 2, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_BITMAP,		"b_layer_subscribe", 82, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_LAYER_ID,	"layer_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_UINT8,		"level");
	v_cg_alias(FALSE, "b_layer_unsubscribe", "if(level == 255)", 2, NULL);
	v_cg_end_cmd();

	v_cg_new_manual_cmd(83, "b_tile_set", "void verse_send_b_tile_set(VNodeID node_id, VLayerID layer_id, " 
				"uint16 tile_x, uint16 tile_y, uint16 z, VNBLayerType type, const VNBTile *tile)",
			    NULL, NULL);
}

#endif
