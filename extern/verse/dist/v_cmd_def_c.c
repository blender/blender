
#include <stdlib.h>
#include <stdio.h>
#include "verse_header.h"
#include "v_cmd_gen.h"
#include "v_cmd_buf.h"

#if defined(V_GENERATE_FUNC_MODE)

void v_gen_curve_cmd_def(void)
{
	v_cg_new_cmd(V_NT_CURVE,		"c_curve_create", 128, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_LAYER_ID,	"curve_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_NAME,		"name");
	v_cg_add_param(VCGP_UINT8,		"dimensions");
	v_cg_alias(FALSE, "c_curve_destroy", "if(name[0] == 0)", 2, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_CURVE,		"c_curve_subscribe", 129, VCGCT_NORMAL); 
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_LAYER_ID,	"curve_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_alias(TRUE, "c_curve_unsubscribe", "if(!alias_bool)", 2, NULL);
	v_cg_end_cmd();

	v_cg_new_manual_cmd(130, "c_key_set", "void verse_send_c_key_set(VNodeID node_id, VLayerID curve_id, "
			    "uint32 key_id, uint8 dimensions, const real64 *pre_value, const uint32 *pre_pos, "
			    "const real64 *value, real64 pos, const real64 *post_value, const uint32 *post_pos)",
			    "c_key_destroy", "void verse_send_c_key_destroy(VNodeID node_id, VLayerID curve_id, "
			    "uint32 key_id)");
}

#endif
