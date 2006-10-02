
#include <stdlib.h>
#include <stdio.h>
#include "verse_header.h"
#include "v_cmd_gen.h"
#include "v_cmd_buf.h"

#if defined(V_GENERATE_FUNC_MODE)

void v_gen_system_cmd_def(void)
{
	v_cg_new_manual_cmd(0, "connect", "VSession verse_send_connect(const char *name, const char *pass, const char *address, const uint8 *expected_host_id)", NULL, NULL);

	v_cg_new_manual_cmd(1, "connect_accept", "VSession verse_send_connect_accept(VNodeID avatar, const char *address, uint8 *host_id)", NULL, NULL);

	v_cg_new_manual_cmd(2, "connect_terminate", "void verse_send_connect_terminate(const char *address, const char *bye)", NULL, NULL);
	
	v_cg_new_manual_cmd(5, "ping", "void verse_send_ping(const char *address, const char *message)", NULL, NULL);

	v_cg_new_cmd(V_NT_SYSTEM,		"error_message", 6, VCGCT_UNIQUE);
	v_cg_add_param(VCGP_LONG_NAME, "message");

	v_cg_new_cmd(V_NT_SYSTEM,		"packet_ack", 7, VCGCT_INVISIBLE_SYSTEM);
	v_cg_add_param(VCGP_UINT32,		"packet_id");
	v_cg_add_param(VCGP_PACK_INLINE, "\tv_cmd_buf_set_unique_address_size(head, buffer_pos);\n"
	"\tv_cmd_buf_set_size(head, buffer_pos);\n"
	"\tv_noq_send_ack_nak_buf(v_con_get_network_queue(), head);\n"
	"\treturn;\n");
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_SYSTEM,		"packet_nak", 8, VCGCT_INVISIBLE_SYSTEM);
	v_cg_add_param(VCGP_UINT32,		"packet_id");
	v_cg_add_param(VCGP_PACK_INLINE, "\tv_cmd_buf_set_unique_address_size(head, buffer_pos);\n"
	"\tv_cmd_buf_set_size(head, buffer_pos);\n"
	"\tv_noq_send_ack_nak_buf(v_con_get_network_queue(), head);\n"
	"\treturn;\n");
	v_cg_end_cmd();


	v_cg_new_cmd(V_NT_SYSTEM,		"node_index_subscribe", 9, VCGCT_NORMAL);
	v_cg_add_param(VCGP_UINT32,		"mask");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_SYSTEM,		"node_create", 10, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_ENUM_NAME,	"VNodeType");
	v_cg_add_param(VCGP_ENUM,		"type");
	v_cg_add_param(VCGP_ENUM_NAME,	"VNodeOwner");
	v_cg_add_param(VCGP_ENUM,		"owner");
	v_cg_alias(FALSE, "node_destroy", "if(owner == (uint8) ~0u || type >= V_NT_NUM_TYPES)", 1, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_SYSTEM,		"node_subscribe", 11, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_alias(TRUE, "node_unsubscribe", NULL, 1, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_SYSTEM,		"tag_group_create", 16, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_UINT16,		"group_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_NAME,		"name");
	v_cg_alias(FALSE, "tag_group_destroy", "if(name[0] == 0)", 2, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_SYSTEM,		"tag_group_subscribe", 17, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_UINT16,		"group_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_alias(TRUE, "tag_group_unsubscribe", NULL, 2, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_SYSTEM,		"tag_create", 18, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_UINT16,		"group_id");
	v_cg_add_param(VCGP_UINT16,		"tag_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_NAME,		"name");
	v_cg_add_param(VCGP_ENUM_NAME,	"VNTagType");
	v_cg_add_param(VCGP_ENUM,		"type");
	v_cg_add_param(VCGP_POINTER_TYPE, "VNTag");
	v_cg_add_param(VCGP_POINTER,	"tag"); 
	v_cg_add_param(VCGP_PACK_INLINE, "\tif(type > VN_TAG_BLOB)\n"
	"\t{\n"
	"\t\tv_cmd_buf_free(head);\n"
	"\t\treturn;\n"
	"\t}\n"
	"\tswitch(type)\n"
	"\t{\n"
	"\t\tcase VN_TAG_BOOLEAN :\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], ((VNTag *)tag)->vboolean);\n"
	"\t\tbreak;\n"
	"\t\tcase VN_TAG_UINT32 :\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], ((VNTag *)tag)->vuint32);\n"
	"\t\tbreak;\n"
	"\t\tcase VN_TAG_REAL64 :\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], ((VNTag *)tag)->vreal64);\n"
	"\t\tbreak;\n"
	"\t\tcase VN_TAG_STRING :\n"
	"\t\t{\n"
	"\t\t\tunsigned int i;\n"
	"\t\t\tfor(i = 0; ((VNTag *)tag)->vstring[i] != 0 && i < VN_TAG_MAX_BLOB_SIZE; i++)\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], ((VNTag *)tag)->vstring[i]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 0);\n"
	"\t\t}\n"
	"\t\tbreak;\n"
	"\t\tcase VN_TAG_REAL64_VEC3 :\n"
	"\t\t{\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], ((VNTag *)tag)->vreal64_vec3[0]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], ((VNTag *)tag)->vreal64_vec3[1]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], ((VNTag *)tag)->vreal64_vec3[2]);\n"
	"\t\t}\n"
	"\t\tbreak;\n"
	"\t\tcase VN_TAG_LINK :\n"
	"\t\t{\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], ((VNTag *)tag)->vlink);\n"
	"\t\t}\n"
	"\t\tbreak;\n"
	"\t\tcase VN_TAG_ANIMATION :\n"
	"\t\t{\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], ((VNTag *)tag)->vanimation.curve);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], ((VNTag *)tag)->vanimation.start);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], ((VNTag *)tag)->vanimation.end);\n"
	"\t\t}\n"
	"\t\tbreak;\n"
	"\t\tcase VN_TAG_BLOB :\n"
	"\t\t{\n"
	"\t\t\tunsigned int i;\n"
	"\t\t\tif(((VNTag *)tag)->vblob.size > VN_TAG_MAX_BLOB_SIZE)\n"
	"\t\t\t\t((VNTag *)tag)->vblob.size = VN_TAG_MAX_BLOB_SIZE;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], ((VNTag *)tag)->vblob.size);\n"
	"\t\t\tfor(i = 0; i < ((VNTag *)tag)->vblob.size; i++)\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], ((uint8 *)((VNTag *)tag)->vblob.blob)[i]);\n"
	"\t\t}\n"
	"\t\tbreak;\n"
	"\t\tdefault :\n"
	"\t\t\t;\n"
	"\t}\n");
	v_cg_add_param(VCGP_UNPACK_INLINE, "\tif(type < VN_TAG_TYPE_COUNT)\n"
	"\t{\n"
	"\t\tVNTag tag;\n"
	"\t\tunsigned int i;\n"
	"\t\tchar string[VN_TAG_MAX_BLOB_SIZE];\n"
	"\t\tswitch(type)\n"
	"\t\t{\n"
	"\t\t\tcase VN_TAG_BOOLEAN :\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &tag.vboolean);\n"
	"\t\t\tbreak;\n"
	"\t\t\tcase VN_TAG_UINT32 :\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &tag.vuint32);\n"
	"\t\t\tbreak;\n"
	"\t\t\tcase VN_TAG_REAL64 :\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &tag.vreal64);\n"
	"\t\t\tbreak;\n"
	"\t\t\tcase VN_TAG_STRING :\n"
	"\t\t\t{\n"
	"\t\t\t\ttag.vstring = string;\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], string, VN_TAG_MAX_BLOB_SIZE, buffer_length - buffer_pos);\n"
	"\t\t\t}\n"
	"\t\t\tbreak;\n"
	"\t\t\tcase VN_TAG_REAL64_VEC3 :\n"
	"\t\t\t{\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &tag.vreal64_vec3[0]);\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &tag.vreal64_vec3[1]);\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &tag.vreal64_vec3[2]);\n"
	"\t\t\t}\n"
	"\t\t\tbreak;\n"
	"\t\t\tcase VN_TAG_LINK :\n"
	"\t\t\t{\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &tag.vlink);\n"
	"\t\t\t}\n"
	"\t\t\tbreak;\n"
	"\t\t\tcase VN_TAG_ANIMATION :\n"
	"\t\t\t{\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &tag.vanimation.curve);\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &tag.vanimation.start);\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &tag.vanimation.end);\n"
	"\t\t\t}\n"
	"\t\t\tbreak;\n"
	"\t\t\tcase VN_TAG_BLOB :\n"
	"\t\t\t{\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &tag.vblob.size);\n"
	"\t\t\t\tif(tag.vblob.size > VN_TAG_MAX_BLOB_SIZE)\n"
	"\t\t\t\t\ttag.vblob.size = VN_TAG_MAX_BLOB_SIZE;\n"
	"\t\t\t\ttag.vblob.blob = string;\n"
	"\t\t\t\tfor(i = 0; i < tag.vblob.size; i++)\n"
	"\t\t\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &string[i]);\n"
	"\t\t\t}\n"
	"\t\t\tbreak;\n"
	"\t\tdefault :\n"
	"\t\t\t;\n"
	"\t\t}\n"
	"\t\tif(func_tag_create != NULL)\n"
	"\t\tfunc_tag_create(v_fs_get_user_data(18), node_id, group_id, tag_id, name, type, &tag);\n"
	"\t\treturn buffer_pos;\n"
	"\t}\n");
	v_cg_alias(FALSE, "tag_destroy", "if(type >= VN_TAG_TYPE_COUNT)", 3, NULL);
	v_cg_end_cmd();

	v_cg_new_cmd(V_NT_SYSTEM,		"node_name_set", 19, VCGCT_NORMAL); 
	v_cg_add_param(VCGP_NODE_ID,	"node_id");
	v_cg_add_param(VCGP_END_ADDRESS, NULL);
	v_cg_add_param(VCGP_LONG_NAME,	"name");

	v_cg_end_cmd();
}

#endif
