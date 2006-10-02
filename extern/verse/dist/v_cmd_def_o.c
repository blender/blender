
#include <stdlib.h>
#include <stdio.h>
#include "verse_header.h"
#include "v_cmd_gen.h"
#include "v_cmd_buf.h"

#if defined(V_GENERATE_FUNC_MODE)

void v_gen_object_cmd_def(void)
{
	v_cg_new_cmd(V_NT_OBJECT,		"o_transform_pos_real32", 32, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_UINT32,		"time_s");
	v_cg_add_param(VCGP_UINT32,		"time_f");
	v_cg_add_param(VCGP_POINTER_TYPE,"real32");
	v_cg_add_param(VCGP_POINTER,	"pos");
	v_cg_add_param(VCGP_POINTER_TYPE,"real32");
	v_cg_add_param(VCGP_POINTER,	"speed");
	v_cg_add_param(VCGP_POINTER_TYPE,"real32");
	v_cg_add_param(VCGP_POINTER,	"accelerate");
	v_cg_add_param(VCGP_POINTER_TYPE,"real32");
	v_cg_add_param(VCGP_POINTER,	"drag_normal");
	v_cg_add_param(VCGP_PACK_INLINE, "\t{\n"
	"\t\tunsigned char mask = 0;\n"
	"\t\tunsigned int cmd;\n"
	"\t\tcmd = buffer_pos++;\n"
	"\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], pos[0]);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], pos[1]);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], pos[2]);\n"
	"\t\tif(speed != NULL && (speed[0] > 0.0000001 || speed[0] < -0.0000001 || speed[1] > 0.0000001 || speed[1] < -0.0000001 || speed[2] > 0.0000001 || speed[2] < -0.0000001))\n"
	"\t\t{\n"
	"\t\t\tmask |= 1;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], speed[0]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], speed[1]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], speed[2]);\n"
	"\t\t}\n"
	"\t\tif(accelerate != NULL && (accelerate[0] > 0.0000001 || accelerate[0] < -0.0000001 || accelerate[1] > 0.0000001 || accelerate[1] < -0.0000001 || accelerate[2] > 0.0000001 || accelerate[2] < -0.0000001))\n"
	"\t\t{\n"
	"\t\t\tmask |= 2;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], accelerate[0]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], accelerate[1]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], accelerate[2]);\n"
	"\t\t}\n"
	"\t\tif(drag_normal != NULL && (drag > 0.0000001 || drag < -0.0000001) && (drag_normal[0] > 0.0000001 || drag_normal[0] < -0.0000001 || drag_normal[1] > 0.0000001 || drag_normal[1] < -0.0000001 || drag_normal[2] > 0.0000001 || drag_normal[2] < -0.0000001))\n"
	"\t\t{\n"
	"\t\t\tmask |= 4;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag_normal[0]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag_normal[1]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag_normal[2]);\n"
	"\t\t}\n"
	"\t\tif(drag > 0.0000001 || drag < -0.0000001)\n"
	"\t\t{\n"
	"\t\t\tmask |= 8;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag);\n"
	"\t\t}\n"
	"\t\tvnp_raw_pack_uint8(&buf[cmd], mask);\n"
	"\t}if(FALSE)\n");
	v_cg_add_param(VCGP_UNPACK_INLINE, "\t{\n"
	"\t\tfloat output[4][3];\n"
	"\t\tunsigned int i, j;\n"
	"\t\tchar mask, pow = 1;\n"
	"\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);\n"
	"\t\tfor(j = 0; j < 3; j++)\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &output[0][j]);\n"
	"\t\tfor(i = 1; i < 4; i++)\n"
	"\t\t{\n"
	"\t\t\tif((mask & pow) != 0)\n"
	"\t\t\t\tfor(j = 0; j < 3; j++)\n"
	"\t\t\t\t\tbuffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &output[i][j]);\n"
	"\t\t\telse\n"
	"\t\t\t\tfor(j = 0; j < 3; j++)\n"
	"\t\t\t\t\toutput[i][j] = 0;\n"
	"\t\t\tpow *= 2;\n"
	"\t\t}\n"
	"\t\tif((mask & pow) != 0)\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &drag);\n"
	"\t\telse\n"
	"\t\t\tdrag = 0.0f;\n"
	"\t\tif(func_o_transform_pos_real32 != NULL)\n"
	"\t\t\tfunc_o_transform_pos_real32(v_fs_get_user_data(32), node_id, time_s, time_f, &output[0][0], &output[1][0], &output[2][0], &output[3][0], drag);\n"
	"\t\treturn buffer_pos;\n"
	"\t}\n");
	v_cg_add_param(VCGP_REAL32,		"drag");	
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_transform_rot_real32", 33, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_UINT32,		"time_s");
	v_cg_add_param(VCGP_UINT32,		"time_f");
	v_cg_add_param(VCGP_POINTER_TYPE,	"VNQuat32");
	v_cg_add_param(VCGP_POINTER,		"rot");
	v_cg_add_param(VCGP_POINTER_TYPE,	"VNQuat32");
	v_cg_add_param(VCGP_POINTER,		"speed");
	v_cg_add_param(VCGP_POINTER_TYPE,	"VNQuat32");
	v_cg_add_param(VCGP_POINTER,		"accelerate");
	v_cg_add_param(VCGP_POINTER_TYPE,	"VNQuat32");
	v_cg_add_param(VCGP_POINTER,		"drag_normal");
	v_cg_add_param(VCGP_PACK_INLINE, "\t{\n"
	"\t\tuint8 mask = 0;\n"
	"\t\tunsigned int maskpos;\n"
	"\t\tmaskpos = buffer_pos++;\t\t/* Remember location, and reserve a byte for the mask. */\n"
	"\t\tbuffer_pos += vnp_pack_quat32(&buf[buffer_pos], rot);\n"
	"\t\tif(v_quat32_valid(speed))\n"
	"\t\t{\n"
	"\t\t\tmask |= 1;\n"
	"\t\t\tbuffer_pos += vnp_pack_quat32(&buf[buffer_pos], speed);\n"
	"\t\t}\n"
	"\t\tif(v_quat32_valid(accelerate))\n"
	"\t\t{\n"
	"\t\t\tmask |= 2;\n"
	"\t\t\tbuffer_pos += vnp_pack_quat32(&buf[buffer_pos], accelerate);\n"
	"\t\t}\n"
	"\t\tif(v_quat32_valid(drag_normal))\n"
	"\t\t{\n"
	"\t\t\tmask |= 4;\n"
	"\t\t\tbuffer_pos += vnp_pack_quat32(&buf[buffer_pos], drag_normal);\n"
	"\t\t}\n"
	"\t\tif(drag > 0.0000001 || drag < -0.0000001)\n"
	"\t\t{\n"
	"\t\t\tmask |= 8;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag);\n"
	"\t\t}\n"
	"\t\tvnp_raw_pack_uint8(&buf[maskpos], mask);\t/* Write the mask into start of command. */\n"
	"\t}\n"
	"\tif(FALSE)\n");
	v_cg_add_param(VCGP_UNPACK_INLINE, "\t{\n"
	"\t\tVNQuat32 trot, temp[3], *q[3];\n"
	"\t\tunsigned int i;\n"
	"\t\tuint8 mask, test;\n"
	"\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);\n"
	"\t\tbuffer_pos += vnp_unpack_quat32(&buf[buffer_pos], &trot);\n"
	"\t\tfor(i = 0, test = 1; i < sizeof temp / sizeof *temp; i++, test <<= 1)\n"
	"\t\t{\n"
	"\t\t\tif(mask & test)\t\t/* Field present? */\n"
	"\t\t\t{\n"
	"\t\t\t\tbuffer_pos += vnp_unpack_quat32(&buf[buffer_pos], &temp[i]);\n"
	"\t\t\t\tq[i] = &temp[i];\n"
	"\t\t\t}\n"
	"\t\t\telse\n"
	"\t\t\t\tq[i] = NULL;\n"
	"\t\t}\n"
	"\t\tif(mask & test)\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &drag);\n"
	"\t\telse\n"
	"\t\t\tdrag = 0.0;\n"
	"\t\tif(func_o_transform_rot_real32 != NULL)\n"
	"\t\t\tfunc_o_transform_rot_real32(v_fs_get_user_data(33), node_id, time_s, time_f, &trot, q[0], q[1], q[2], drag);\n"
	"\t\treturn buffer_pos;\n"
	"\t}\n");
	v_cg_add_param(VCGP_REAL32,	"drag");
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_transform_scale_real32", 34, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_REAL32,		"scale_x");	
	v_cg_add_param(VCGP_REAL32,		"scale_y");	
	v_cg_add_param(VCGP_REAL32,		"scale_z");	
	v_cg_end_cmd();
	
	v_cg_new_cmd(V_NT_OBJECT,		"o_transform_pos_real64", 35, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_UINT32,		"time_s");
	v_cg_add_param(VCGP_UINT32,		"time_f");
	v_cg_add_param(VCGP_POINTER_TYPE,"real64");
	v_cg_add_param(VCGP_POINTER,	"pos");
	v_cg_add_param(VCGP_POINTER_TYPE,"real64");
	v_cg_add_param(VCGP_POINTER,	"speed");
	v_cg_add_param(VCGP_POINTER_TYPE,"real64");
	v_cg_add_param(VCGP_POINTER,	"accelerate");
	v_cg_add_param(VCGP_POINTER_TYPE,"real64");
	v_cg_add_param(VCGP_POINTER,	"drag_normal");
	v_cg_add_param(VCGP_PACK_INLINE, "\t{\n"
	"\t\tunsigned char mask = 0;\n"
	"\t\tunsigned int cmd;\n"
	"\t\tcmd = buffer_pos++;\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], pos[0]);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], pos[1]);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], pos[2]);\n"
	"\t\tif(speed != NULL && (speed[0] > 0.0000001 || speed[0] < -0.0000001 || speed[1] > 0.0000001 || speed[1] < -0.0000001 || speed[2] > 0.0000001 || speed[2] < -0.0000001))\n"
	"\t\t{\n"
	"\t\t\tmask |= 1;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], speed[0]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], speed[1]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], speed[2]);\n"
	"\t\t}\n"
	"\t\tif(accelerate != NULL && (accelerate[0] > 0.0000001 || accelerate[0] < -0.0000001 || accelerate[1] > 0.0000001 || accelerate[1] < -0.0000001 || accelerate[2] > 0.0000001 || accelerate[2] < -0.0000001))\n"
	"\t\t{\n"
	"\t\t\tmask |= 2;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], accelerate[0]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], accelerate[1]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], accelerate[2]);\n"
	"\t\t}\n"
	"\t\tif(drag_normal != NULL && (drag > 0.0000001 || drag < -0.0000001) && (drag_normal[0] > 0.0000001 || drag_normal[0] < -0.0000001 || drag_normal[1] > 0.0000001 || drag_normal[1] < -0.0000001 || drag_normal[2] > 0.0000001 || drag_normal[2] < -0.0000001))\n"
	"\t\t{\n"
	"\t\t\tmask |= 4;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag_normal[0]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag_normal[1]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag_normal[2]);\n"
	"\t\t}\n"
	"\t\tif(drag > 0.0000001 || drag < -0.0000001)\n"
	"\t\t{\n"
	"\t\t\tmask |= 8;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag);\n"
	"\t\t}\n"
	"\t\tvnp_raw_pack_uint8(&buf[cmd], mask);\n"
	"\t}if(FALSE)\n");
	v_cg_add_param(VCGP_UNPACK_INLINE, "\t{\n"
	"\t\tdouble output[4][3];\n"
	"\t\tunsigned int i, j;\n"
	"\t\tchar mask, pow = 1;\n"
	"\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);\n"
	"\t\tfor(j = 0; j < 3; j++)\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &output[0][j]);\n"
	"\t\tfor(i = 1; i < 4; i++)\n"
	"\t\t{\n"
	"\t\t\tif((mask & pow) != 0)\n"
	"\t\t\t\tfor(j = 0; j < 3; j++)\n"
	"\t\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &output[i][j]);\n"
	"\t\t\telse\n"
	"\t\t\t\tfor(j = 0; j < 3; j++)\n"
	"\t\t\t\t\toutput[i][j] = 0;\n"
	"\t\t\tpow *= 2;\n"
	"\t\t}\n"
	"\t\tif((mask & pow) != 0)\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &drag);\n"
	"\t\telse\n"
	"\t\t\tdrag = 0.0;\n"
	"\t\tif(func_o_transform_pos_real64 != NULL)\n"
	"\t\t\tfunc_o_transform_pos_real64(v_fs_get_user_data(35), node_id, time_s, time_f, &output[0][0], &output[1][0], &output[2][0], &output[3][0], drag);\n"
	"\t\treturn buffer_pos;\n"
	"\t}\n");
	v_cg_add_param(VCGP_REAL64,		"drag");	
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_transform_rot_real64", 36, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_UINT32,		"time_s");
	v_cg_add_param(VCGP_UINT32,		"time_f");
	v_cg_add_param(VCGP_POINTER_TYPE,"VNQuat64");
	v_cg_add_param(VCGP_POINTER,	"rot");
	v_cg_add_param(VCGP_POINTER_TYPE,"VNQuat64");
	v_cg_add_param(VCGP_POINTER,	"speed");
	v_cg_add_param(VCGP_POINTER_TYPE,"VNQuat64");
	v_cg_add_param(VCGP_POINTER,	"accelerate");
	v_cg_add_param(VCGP_POINTER_TYPE,"VNQuat64");
	v_cg_add_param(VCGP_POINTER,	"drag_normal");
	v_cg_add_param(VCGP_PACK_INLINE, "\t{\n"
	"\t\tuint8 mask = 0;\n"
	"\t\tunsigned int maskpos;\n"
	"\t\tmaskpos = buffer_pos++;\t\t/* Remember location, and reserve a byte for the mask. */\n"
	"\t\tbuffer_pos += vnp_pack_quat64(&buf[buffer_pos], rot);\n"
	"\t\tif(v_quat64_valid(speed))\n"
	"\t\t{\n"
	"\t\t\tmask |= 1;\n"
	"\t\t\tbuffer_pos += vnp_pack_quat64(&buf[buffer_pos], speed);\n"
	"\t\t}\n"
	"\t\tif(v_quat64_valid(accelerate))\n"
	"\t\t{\n"
	"\t\t\tmask |= 2;\n"
	"\t\t\tbuffer_pos += vnp_pack_quat64(&buf[buffer_pos], accelerate);\n"
	"\t\t}\n"
	"\t\tif(v_quat64_valid(drag_normal))\n"
	"\t\t{\n"
	"\t\t\tmask |= 4;\n"
	"\t\t\tbuffer_pos += vnp_pack_quat64(&buf[buffer_pos], drag_normal);\n"
	"\t\t}\n"
	"\t\tif(drag > 0.0000001 || drag < -0.0000001)\n"
	"\t\t{\n"
	"\t\t\tmask |= 8;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag);\n"
	"\t\t}\n"
	"\t\tvnp_raw_pack_uint8(&buf[maskpos], mask);\t/* Write the mask into start of command. */\n"
	"\t}\n"
	"\tif(FALSE)\n");
	v_cg_add_param(VCGP_UNPACK_INLINE, "\t{\n"
	"\t\tVNQuat64 trot, temp[3], *q[3];\n"
	"\t\tunsigned int i;\n"
	"\t\tuint8 mask, test;\n"
	"\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);\n"
	"\t\tbuffer_pos += vnp_unpack_quat64(&buf[buffer_pos], &trot);\n"
	"\t\tfor(i = 0, test = 1; i < sizeof temp / sizeof *temp; i++, test <<= 1)\n"
	"\t\t{\n"
	"\t\t\tif(mask & test)\t\t/* Field present? */\n"
	"\t\t\t{\n"
	"\t\t\t\tbuffer_pos += vnp_unpack_quat64(&buf[buffer_pos], &temp[i]);\n"
	"\t\t\t\tq[i] = &temp[i];\n"
	"\t\t\t}\n"
	"\t\t\telse\n"
	"\t\t\t\tq[i] = NULL;\n"
	"\t\t}\n"
	"\t\tif(mask & test)\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &drag);\n"
	"\t\telse\n"
	"\t\t\tdrag = 0.0;\n"
	"\t\tif(func_o_transform_rot_real64 != NULL)\n"
	"\t\t\tfunc_o_transform_rot_real64(v_fs_get_user_data(36), node_id, time_s, time_f, &trot, q[0], q[1], q[2], drag);\n"
	"\t\treturn buffer_pos;\n"
	"\t}\n");
	v_cg_add_param(VCGP_REAL64,		"drag");	
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_transform_scale_real64", 37, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_REAL64,		"scale_x");	
	v_cg_add_param(VCGP_REAL64,		"scale_y");	
	v_cg_add_param(VCGP_REAL64,		"scale_z");	
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_transform_subscribe", 38, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_ENUM_NAME,	"VNRealFormat");
	v_cg_add_param(VCGP_ENUM,		"type");
	v_cg_alias(TRUE, "o_transform_unsubscribe", NULL, 4, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_light_set", 39, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_REAL64,		"light_r");	
	v_cg_add_param(VCGP_REAL64,		"light_g");	
	v_cg_add_param(VCGP_REAL64,		"light_b");	
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_link_set", 40, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_UINT16,		"link_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_NODE_ID,	"link");
	v_cg_add_param(VCGP_NAME,		"label");
	v_cg_add_param(VCGP_UINT32,		"target_id");
	v_cg_alias(TRUE, "o_link_destroy", NULL, 2, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_method_group_create", 41, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_UINT16,		"group_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_NAME,		"name");
	v_cg_alias(FALSE, "o_method_group_destroy", "if(name[0] == 0)", 2, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_method_group_subscribe", 42, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_UINT16,		"group_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_alias(TRUE, "o_method_group_unsubscribe", NULL, 2, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_method_create", 43, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_UINT16,		"group_id");
	v_cg_add_param(VCGP_UINT16,		"method_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_LONG_NAME,	"name");
	v_cg_add_param(VCGP_UINT8,		"param_count");
	v_cg_add_param(VCGP_POINTER_TYPE,"VNOParamType");
	v_cg_add_param(VCGP_POINTER,	"param_types");
	v_cg_add_param(VCGP_POINTER_TYPE,"char *");
	v_cg_add_param(VCGP_POINTER,	"param_names");
	v_cg_add_param(VCGP_PACK_INLINE, "\t{\n"
	"\t\tunsigned int i, j, sum = 1;\n"
	"\t\tfor(i = 0; i < param_count; i++)\n"
	"\t\t{\n"
	"\t\t\tsum += 3;\n"
	"\t\t\tfor(j = 0; param_names[i][j] != 0; j++);\n"
	"\t\t}\n"
	"\t\tif(sum + buffer_pos > 1500)\n"
	"\t\t\treturn;\n"
	"\t\tfor(i = 0; i < param_count; i++)\n"
	"\t\t{\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], param_types[i]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], param_names[i], 1500 - buffer_pos);\n"
	"\t\t}\n"
	"\t}\n");
	v_cg_add_param(VCGP_UNPACK_INLINE, 	"\tif(param_count != 255)\n"
	"\t{\n"
	"\t\tunsigned int i, size, text = 0;\n"
	"\t\tVNOParamType types[256];\n"
	"\t\tuint8 t;\n"
	"\t\tchar name_buf[1500], *names[256];\n"
	"\t\tfor(i = 0; i < param_count; i++)\n"
	"\t\t{\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &t);\n"
	"\t\t\ttypes[i] = t;\n"
	"\t\t\tnames[i] = &name_buf[text];\n"
	"\t\t\tsize = vnp_raw_unpack_string(&buf[buffer_pos], names[i], 1500 - buffer_pos, buffer_length - buffer_pos);\n"
	"\t\t\tbuffer_pos += size;\n"
	"\t\t\ttext += size;\n"
	"\t\t}\n"
	"\t\tif(func_o_method_create != NULL)\n"
	"\t\t\tfunc_o_method_create(v_fs_get_user_data(43), node_id, group_id, method_id, name, param_count, types, (const char **) names);\n"
	"\t\treturn buffer_pos;\n"
	"\t}\n");
	v_cg_alias(FALSE, "o_method_destroy", "if(name[0] == 0)", 3, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_method_call", 44, VCGCT_UNIQUE);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_UINT16,		"group_id");
	v_cg_add_param(VCGP_UINT16,		"method_id");
	v_cg_add_param(VCGP_NODE_ID,	"sender");
	v_cg_add_param(VCGP_POINTER_TYPE, "VNOPackedParams");
	v_cg_add_param(VCGP_POINTER,	"params");
	v_cg_add_param(VCGP_PACK_INLINE, "\t{\n"
	"\t\tunsigned int i;\n"
	"\t\tuint16 size;\n"
	"\t\tvnp_raw_unpack_uint16(params, &size);\n"
	"\t\tfor(i = 0; i < size; i++)\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], ((uint8 *)params)[i]);\n"
	"\t\tfree((void *) params);\t/* Drop the const. */\n"
	"\t}\n");
	v_cg_add_param(VCGP_UNPACK_INLINE, "\t{\n"
	"\t\tunsigned int i;\n"
	"\t\tuint8 par[1500];\n"
	"\t\tuint16 size;\n"
	"\t\tvnp_raw_unpack_uint16(&buf[buffer_pos], &size);\n"
	"\t\tfor(i = 0; i < size; i++)\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &par[i]);\n"
	"\t\tif(func_o_method_call != NULL)\n"
	"\t\t\tfunc_o_method_call(v_fs_get_user_data(44), node_id, group_id, method_id, sender, par);\n"
	"\t\treturn buffer_pos;\n"
	"\t}\n");

	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_anim_run", 45, VCGCT_UNIQUE);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_UINT16,		"link_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_UINT32,		"time_s");
	v_cg_add_param(VCGP_UINT32,		"time_f");
	v_cg_add_param(VCGP_UINT8,		"dimensions");
	v_cg_add_param(VCGP_POINTER_TYPE, "real64");
	v_cg_add_param(VCGP_POINTER,	"pos");
	v_cg_add_param(VCGP_POINTER_TYPE, "real64");
	v_cg_add_param(VCGP_POINTER,	"speed");
	v_cg_add_param(VCGP_POINTER_TYPE, "real64");
	v_cg_add_param(VCGP_POINTER,	"accel");
	v_cg_add_param(VCGP_POINTER_TYPE, "real64");
	v_cg_add_param(VCGP_POINTER,	"scale");
	v_cg_add_param(VCGP_POINTER_TYPE, "real64");
	v_cg_add_param(VCGP_POINTER,	"scale_speed");
	v_cg_add_param(VCGP_PACK_INLINE, "\t{\n"
	"\t\tunsigned char mask = 0;\n"
	"\t\tunsigned int cmd, i;\n"
	"\t\tcmd = buffer_pos++;\n"
	"\t\tif(dimensions > 4)\n"
	"\t\t\tdimensions = 4;\n"	
	"\t\tfor(i = 0; i < dimensions; i++)\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], pos[i]);\n"
	"\t\tif(speed != NULL)\n"
	"\t\t{\n"
	"\t\t\tmask |= 1;\n"
	"\t\t\tfor(i = 0; i < dimensions; i++)\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], speed[i]);\n"
	"\t\t}\n"
	"\t\tif(accel != NULL)\n"
	"\t\t{\n"
	"\t\t\tmask |= 2;\n"
	"\t\t\tfor(i = 0; i < dimensions; i++)\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], accel[i]);\n"
	"\t\t}\n"
	"\t\tif(scale != NULL)\n"
	"\t\t{\n"
	"\t\t\tmask |= 3;\n"
	"\t\t\tfor(i = 0; i < dimensions; i++)\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], scale[i]);\n"
	"\t\t}\n"
	"\t\tif(scale_speed != NULL)\n"
	"\t\t{\n"
	"\t\t\tmask |= 4;\n"
	"\t\t\tfor(i = 0; i < dimensions; i++)\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], scale_speed[i]);\n"
	"\t\t}\n"
	"\t\tvnp_raw_pack_uint8(&buf[cmd], mask);\n"
	"\t}\n");
	v_cg_add_param(VCGP_UNPACK_INLINE, "\t{\n"
	"\t\tdouble output[5][4];\n"
	"\t\tunsigned int i, j;\n"
	"\t\tchar mask, pow = 1;\n"
	"\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);\n"
	"\t\tif(dimensions > 4)\n"
	"\t\t\tdimensions = 4;\n"
	"\t\tfor(j = 0; j < dimensions; j++)\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &output[0][j]);\n"
	"\t\tfor(i = 1; i < 5; i++)\n"
	"\t\t{\n"
	"\t\t\tif((mask & pow) != 0)\n"
	"\t\t\t\tfor(j = 0; j < dimensions; j++)\n"
	"\t\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &output[i][j]);\n"
	"\t\t\telse\n"
	"\t\t\t\tfor(j = 0; j < dimensions; j++)\n"
	"\t\t\t\t\toutput[i][j] = 0;\n"
	"\t\t\tpow *= 2;\n"
	"\t\t}\n"
	"\t\tif(func_o_anim_run != NULL)\n"
	"\t\t\tfunc_o_anim_run(v_fs_get_user_data(45), node_id, link_id, time_s, time_f, dimensions, &output[0][0], &output[1][0], &output[2][0], &output[3][0], &output[4][0]);\n"
	"\t\treturn buffer_pos;\n"
	"\t}\n");
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_OBJECT,		"o_hide", 46, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_UINT8,		"hidden");
	v_cg_end_cmd();
}

#endif
