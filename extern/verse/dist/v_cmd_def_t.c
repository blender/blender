
#include <stdlib.h>
#include <stdio.h>
#include "verse_header.h"
#include "v_cmd_gen.h"
#include "v_cmd_buf.h"

#if defined(V_GENERATE_FUNC_MODE)

void v_gen_text_cmd_def(void)
{
	v_cg_new_cmd(V_NT_TEXT,			"t_language_set", 96, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_LONG_NAME,	"language");
	v_cg_end_cmd();
	
	v_cg_new_cmd(V_NT_TEXT,			"t_buffer_create", 97, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_BUFFER_ID,	"buffer_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_NAME,		"name");
	v_cg_alias(FALSE, "t_buffer_destroy", "if(name[0] == 0)", 2, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_TEXT,			"t_buffer_subscribe", 98, VCGCT_NORMAL); 
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_BUFFER_ID,	"buffer_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_alias(TRUE, "t_buffer_unsubscribe", NULL, 2, NULL);
	v_cg_end_cmd();

	v_cg_new_manual_cmd(99, "t_text_set", "void verse_send_t_text_set(VNodeID node_id, VBufferID buffer_id, uint32 pos, uint32 length, const char *text)", NULL, NULL);
}

#endif
