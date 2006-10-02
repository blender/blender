/*
**
*/

#include <stdlib.h>
#include <stdio.h>

#include "verse_header.h"
#include "v_cmd_buf.h"

#include "v_cmd_gen.h"

#if defined V_GENERATE_FUNC_MODE

#define MAX_PARAMS_PER_CMD	32

static struct {
	FILE		*nodes[V_NT_NUM_TYPES_NETPACK];
	FILE		*init;
	FILE		*unpack;
	FILE		*verse_h;
	FILE		*internal_verse_h;
	const char	*func_name;
	VNodeType	type;
	VCGCommandType command;
	unsigned int	param_count;
	VCGParam	param_type[MAX_PARAMS_PER_CMD];
	const char	*param_name[MAX_PARAMS_PER_CMD];
	unsigned int	cmd_id;
	const char	*alias_name;
	const char	*alias_qualifier;
	unsigned int	alias_param;
	unsigned int	*alias_param_array;
	char		alias_bool_switch;
} VCGData;

extern void v_gen_system_cmd_def(void);
extern void v_gen_object_cmd_def(void);
extern void v_gen_geometry_cmd_def(void);
extern void v_gen_material_cmd_def(void);
extern void v_gen_bitmap_cmd_def(void);
extern void v_gen_text_cmd_def(void);
extern void v_gen_curve_cmd_def(void);
extern void v_gen_audio_cmd_def(void);

static void v_cg_init(void)
{
	int	i;
	FILE	*f;

	VCGData.nodes[V_NT_OBJECT] = fopen("v_gen_pack_o_node.c", "w");
	VCGData.nodes[V_NT_GEOMETRY] = fopen("v_gen_pack_g_node.c", "w");  
	VCGData.nodes[V_NT_MATERIAL] = fopen("v_gen_pack_m_node.c", "w");  
	VCGData.nodes[V_NT_BITMAP] = fopen("v_gen_pack_b_node.c", "w");  
	VCGData.nodes[V_NT_TEXT] = fopen("v_gen_pack_t_node.c", "w");  
	VCGData.nodes[V_NT_CURVE] = fopen("v_gen_pack_c_node.c", "w");  
	VCGData.nodes[V_NT_AUDIO] = fopen("v_gen_pack_a_node.c", "w");  
	VCGData.nodes[V_NT_SYSTEM] = fopen("v_gen_pack_s_node.c", "w"); 
	VCGData.init = fopen("v_gen_pack_init.c", "w");
	VCGData.unpack = fopen("v_gen_unpack_func.h", "w");
	VCGData.verse_h = fopen("verse.h", "w");
	VCGData.internal_verse_h = fopen("v_internal_verse.h", "w");
	for(i = 0; i < V_NT_NUM_TYPES_NETPACK + 1; i++)
	{
		if(i == V_NT_NUM_TYPES_NETPACK)
			f = VCGData.init;
		else
			f = VCGData.nodes[i];
		fprintf(f,
			"/*\n"
			"** This is automatically generated source code -- do not edit.\n"
			"** Changes are affected either by editing the corresponding protocol\n"
			"** definition file (v_cmd_def_X.c where X=node type), or by editing\n"
			"** the code generator itself, in v_cmd_gen.c.\n"
			"*/\n\n");
		fprintf(f, "#include <stdlib.h>\n");
		fprintf(f, "#include <stdio.h>\n\n");
		fprintf(f, "#include \"v_cmd_gen.h\"\n");
		fprintf(f, "#if !defined(V_GENERATE_FUNC_MODE)\n");
		fprintf(f, "#include \"verse.h\"\n");
		fprintf(f, "#include \"v_cmd_buf.h\"\n");
		fprintf(f, "#include \"v_network_out_que.h\"\n");
		fprintf(f, "#include \"v_network.h\"\n");
		fprintf(f, "#include \"v_connection.h\"\n");
		fprintf(f, "#include \"v_util.h\"\n\n");
	}
	VCGData.cmd_id = 0;
	fprintf(f, "#include \"v_gen_unpack_func.h\"\n\n");
	fprintf(f,
		"#include \"verse.h\"\n\n\n"
		"extern void verse_send_packet_ack(uint32 packet_id);\n"
		"extern void verse_send_packet_nak(uint32 packet_id);\n\n");

	fprintf(VCGData.init, "void init_pack_and_unpack(void)\n{\n");
	fprintf(VCGData.verse_h,
		"/*\n"
		"** Verse API Header file (for use with libverse.a).\n"
		"** This is automatically generated code; do not edit.\n"
		"*/\n\n"
		"\n"
		"#if !defined VERSE_H\n"
		"\n"
		"#if defined __cplusplus\t\t/* Declare as C symbols for C++ users. */\n"
		"extern \"C\" {\n"
		"#endif\n\n"
		"#define\tVERSE_H\n\n");
	/* Copy contents of "verse_header.h" into output "verse.h". */
	f = fopen("verse_header.h", "r");
	while((i = fgetc(f)) != EOF)
		fputc(i, VCGData.verse_h);
	fclose(f);
	fprintf(VCGData.verse_h, "\n/* Command sending functions begin. ----------------------------------------- */\n\n");
}

static void v_cg_close(void)
{
	unsigned int i;
	for(i = 0; i < V_NT_NUM_TYPES_NETPACK; i++)
	{
		fprintf(VCGData.nodes[i], "#endif\n\n");
	}
	fprintf(VCGData.init, "}\n#endif\n\n");
	fprintf(VCGData.verse_h,
		"\n#if defined __cplusplus\n"
		"}\n"
		"#endif\n");
	fprintf(VCGData.verse_h, "\n#endif\t\t/* VERSE_H */\n");
}

void v_cg_new_cmd(VCGCommandType type, const char *name, unsigned int cmd_id, VCGCommandType command)
{
	VCGData.param_count = 0;
	VCGData.func_name = name;
	VCGData.type = type;
	VCGData.cmd_id = cmd_id;
	VCGData.command = command;
/*	printf("def: %u: %s\n", cmd_id, name);*/
}

void v_cg_new_manual_cmd(unsigned int cmd_id, const char *name, const char *def, const char *alias_name, const char *alias_def)
{
	fprintf(VCGData.verse_h, "extern %s;\n", def);
	if(alias_def != NULL)
		fprintf(VCGData.verse_h, "extern %s;\n", alias_def);
	fprintf(VCGData.init, "\tv_fs_add_func(%i, v_unpack_%s, verse_send_%s, ", cmd_id, name, name);
	if(alias_name != NULL)
		fprintf(VCGData.init, "verse_send_%s);\n", alias_name);
	else
		fprintf(VCGData.init, "NULL);\n");
	fprintf(VCGData.unpack, "extern unsigned int v_unpack_%s(const char *data, size_t length);\n", name);
/*	printf("def: %u: %s\n", cmd_id, name);*/
}

void v_cg_alias(char bool_switch, const char *name, const char *qualifier, unsigned int param, unsigned int *param_array)
{
	VCGData.alias_name = name;
	VCGData.alias_qualifier = qualifier;
	VCGData.alias_param = param;
	VCGData.alias_param_array = param_array;
	VCGData.alias_bool_switch = bool_switch;
}

void v_cg_add_param(VCGParam type, const char *name)
{
	if(VCGData.param_count == MAX_PARAMS_PER_CMD)
		exit(1);
	VCGData.param_type[VCGData.param_count] = type;
	VCGData.param_name[VCGData.param_count] = name;
	VCGData.param_count++;
}

static void v_cg_gen_func_params(FILE *f, boolean types, boolean alias)
{
	unsigned int i;
	unsigned int length, active;
	length = VCGData.param_count;
	if(alias)
		length = VCGData.alias_param;
	for(i = 0; i < length; i++)
	{
		if(alias && VCGData.alias_param_array != NULL)
			active = VCGData.alias_param_array[i];
		else
		{
			for(;(VCGData.param_type[i] == VCGP_PACK_INLINE || VCGData.param_type[i] == VCGP_UNPACK_INLINE || VCGData.param_type[i] == VCGP_POINTER_TYPE  || VCGData.param_type[i] == VCGP_ENUM_NAME) && i < VCGData.param_count; i++);
			if(i == VCGData.param_count)
				break;
			active = i;
		}

		if(active < VCGData.param_count && VCGData.param_type[active] != VCGP_END_ADDRESS)
		{
			switch(VCGData.param_type[active])
			{
				case VCGP_UINT8 :
					fprintf(f, "uint8 %s", VCGData.param_name[active]);
				break;
				case VCGP_UINT16 :
					fprintf(f, "uint16 %s", VCGData.param_name[active]);
				break;
				case VCGP_UINT32 :
					fprintf(f, "uint32 %s", VCGData.param_name[active]);
				break;
				case VCGP_REAL32 :
					fprintf(f, "real32 %s", VCGData.param_name[active]);
				break;
				case VCGP_REAL64 :
					fprintf(f, "real64 %s", VCGData.param_name[active]);
				break;
				case VCGP_POINTER :
					if(active != 0 && VCGData.param_type[active - 1] == VCGP_POINTER_TYPE)
						fprintf(f, "const %s *%s", VCGData.param_name[active - 1], VCGData.param_name[active]);
					else
						fprintf(f, "const void *%s", VCGData.param_name[active]);
				break;
				case VCGP_NAME :
					if(types)
						fprintf(f, "char %s[16]", VCGData.param_name[active]);
					else
						fprintf(f, "const char *%s", VCGData.param_name[active]);
				break;
				case VCGP_LONG_NAME :
					if(types)
						fprintf(f, "char %s[512]", VCGData.param_name[active]);
					else
						fprintf(f, "const char *%s", VCGData.param_name[active]);
				break;
				case VCGP_NODE_ID :
					fprintf(f, "VNodeID %s", VCGData.param_name[active]);
				break;
				case VCGP_LAYER_ID :
					fprintf(f, "VLayerID %s", VCGData.param_name[active]);
				break;
				case VCGP_BUFFER_ID :
					fprintf(f, "VBufferID %s", VCGData.param_name[active]);
				break;
				case VCGP_FRAGMENT_ID :
					fprintf(f, "VNMFragmentID %s", VCGData.param_name[active]);
				break;
				case VCGP_ENUM :
/*					if(types)
						fprintf(f, "uint8 %s", VCGData.param_name[active]);
					else
*/						fprintf(f, "%s %s", VCGData.param_name[active - 1], VCGData.param_name[active]);
				break;
			}
			if(types)
				fprintf(f, ";\n\t");
			else
			{
				for(;(VCGData.param_type[active + 1] == VCGP_END_ADDRESS || VCGData.param_type[active + 1] == VCGP_PACK_INLINE || VCGData.param_type[active + 1] == VCGP_UNPACK_INLINE || VCGData.param_type[active + 1] == VCGP_POINTER_TYPE) && active < VCGData.param_count; active++);
				if(active + 1 < length)
					fprintf(f, ", ");
			}
		}
	}
}

static void v_cg_create_print(FILE *f, boolean send, boolean alias)
{
	unsigned int i, length, active;
	const char *name;
	if(VCGData.command == VCGCT_INVISIBLE_SYSTEM)
		return;
	name = VCGData.func_name;
	if(alias)
		name = VCGData.alias_name;
	if(send)
		fprintf(f, "\tprintf(\"send: verse_send_%s(", name);
	else
		fprintf(f, "\tprintf(\"receive: verse_send_%s(", name);

	length = VCGData.param_count;
	if(alias)
		length = VCGData.alias_param;
	for(i = 0; i < length; i++)
	{
		if(alias && VCGData.alias_param_array != NULL)
			active = VCGData.alias_param_array[i];
		else
			active = i;

		switch(VCGData.param_type[active])
		{
			case VCGP_NODE_ID :
				fprintf(f, "%s = %%u ", VCGData.param_name[active]);
			break;
			case VCGP_UINT8 :
			case VCGP_UINT16 :
			case VCGP_UINT32 :
			case VCGP_LAYER_ID :
			case VCGP_BUFFER_ID :
			case VCGP_ENUM :
			case VCGP_FRAGMENT_ID :
				fprintf(f, "%s = %%u ", VCGData.param_name[active]);
			break;
			case VCGP_REAL32 :
			case VCGP_REAL64 :
				fprintf(f, "%s = %%f ", VCGData.param_name[active]);
			break;
			case VCGP_POINTER :
				if(send)
					fprintf(f, "%s = %%p ", VCGData.param_name[active]);
			break;
			case VCGP_NAME :
			case VCGP_LONG_NAME :
				fprintf(f, "%s = %%s ", VCGData.param_name[active]);
			break;
		}
	}
	if(send)
		fprintf(f, ");\\n\"");
	else
		fprintf(f, "); callback = %%p\\n\"");

	for(i = 0; i < length; i++)
	{
		if(alias && VCGData.alias_param_array != NULL)
			active = VCGData.alias_param_array[i];
		else
			active = i;
		switch(VCGData.param_type[active])
		{
			case VCGP_NODE_ID :
				fprintf(f, ", %s", VCGData.param_name[active]);
			break;
			case VCGP_POINTER :
				if(!send)
					break;
			case VCGP_UINT8 :
			case VCGP_UINT16 :
			case VCGP_UINT32 :
			case VCGP_LAYER_ID :
			case VCGP_BUFFER_ID :
			case VCGP_ENUM :
			case VCGP_FRAGMENT_ID :
			case VCGP_REAL32 :
			case VCGP_REAL64 :
			case VCGP_NAME :
			case VCGP_LONG_NAME :
				fprintf(f, ", %s", VCGData.param_name[active]);
			break;
		}
	}
	if(send)
		fprintf(f, ");\n");
	else if(alias)
		fprintf(f, ", v_fs_get_alias_user_func(%u));\n", VCGData.cmd_id);
	else
		fprintf(f, ", v_fs_get_user_func(%u));\n", VCGData.cmd_id);

}

static unsigned int v_cg_compute_command_size(unsigned int start, boolean end)
{
	unsigned int size = 0;
	for(; start < VCGData.param_count; start++)
	{
		switch(VCGData.param_type[start])
		{
			case  VCGP_UINT8 :
			case  VCGP_ENUM :
				size++;
				break;
			case  VCGP_UINT16 :
			case  VCGP_LAYER_ID :
			case  VCGP_BUFFER_ID :
			case  VCGP_FRAGMENT_ID :
				size += 2;
				break;
			case  VCGP_NODE_ID : 
			case  VCGP_UINT32 :
			case  VCGP_REAL32 :
				size += 4;
				break;
			case  VCGP_REAL64 :
				size += 8;
				break;
			case  VCGP_NAME :
				if(end)
					return size;
				size += 16;
				break;
			case  VCGP_LONG_NAME :
				if(end)
					return size;
				size += 512;
				break;
			case  VCGP_POINTER :
			case  VCGP_PACK_INLINE :
			case  VCGP_UNPACK_INLINE :
				if(end)
					return size;
				size += 1500;
				break;
			case VCGP_END_ADDRESS :
				if(end)
					return size;
		}
	}
	return size;
}

void v_cg_set_command_address(FILE *f, boolean alias)
{
	unsigned int i, j, count = 0, length, size = 1, *param, def[] ={0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

	for(i = 0; i < VCGData.param_count; i++)
		if(VCGData.param_type[i] == VCGP_END_ADDRESS)
			break;
	if(i == VCGData.param_count)
		return;
	if(alias)
		length = VCGData.alias_param;
	else
		length = VCGData.param_count;

	if(alias && VCGData.alias_param_array != 0)
		param = VCGData.alias_param_array;
	else
		param = def;

	if(i == VCGData.param_count)
		return;
	fprintf(f, "\tif(");
	for(i = j = 0; i < VCGData.param_count; i++)
	{
		switch(VCGData.param_type[i])
		{
			case  VCGP_UINT8 :
			case  VCGP_ENUM :
				size++;
				break;
			case  VCGP_UINT16 :
			case  VCGP_LAYER_ID :
			case  VCGP_BUFFER_ID :
			case  VCGP_FRAGMENT_ID :
				size += 2;
				break;
			case  VCGP_NODE_ID : 
			case  VCGP_UINT32 :
			case  VCGP_REAL32 :
				size += 4;
				break;
		}
		if(j < length && param[j] == i)
		{
			switch(VCGData.param_type[param[j]])
			{
				case  VCGP_UINT8 :
				case  VCGP_ENUM :
					break;
				case  VCGP_UINT16 :
				case  VCGP_LAYER_ID :
				case  VCGP_BUFFER_ID :
				case  VCGP_FRAGMENT_ID :
					if(count++ != 0)
						fprintf(f, " || ");
					fprintf(f, "%s == (uint16) ~0u", VCGData.param_name[param[j]]);
					break;
				case  VCGP_NODE_ID : 
				case  VCGP_UINT32 :
				case  VCGP_REAL32 :
					if(count++ != 0)
						fprintf(f, " || ");
					fprintf(f, "%s == (uint32) ~0u", VCGData.param_name[param[j]]);
					break;
			}
			j++;
		}
		if(VCGData.param_type[i] == VCGP_END_ADDRESS)
		{
			fprintf(f, ")\n");
			fprintf(f, "\t\tv_cmd_buf_set_unique_address_size(head, %u);\n", size);
			fprintf(f, "\telse\n");
			fprintf(f, "\t\tv_cmd_buf_set_address_size(head, %u);\n", size);
			return;
		}
	}
	fprintf(f, ")\n");
	fprintf(f, "\t\tv_cmd_buf_set_unique_address_size(head, %u);\n", size);
	fprintf(f, "\telse\n");
	fprintf(f, "\t\tv_cmd_buf_set_address_size(head, %u);\n", size);
	return;
}

static const char * v_cg_compute_buffer_size(void)
{
	unsigned int size; 
	size = v_cg_compute_command_size(0, FALSE) + 1;
	if(size <= 10)
		return "VCMDBS_10";
	else if(size <= 20)
		return "VCMDBS_20";
	else if(size <= 30)
		return "VCMDBS_30";
	else if(size <= 80)
		return "VCMDBS_80";
	else if(size <= 160)
		return "VCMDBS_160";
	else if(size <= 320)
		return "VCMDBS_320";
	return "VCMDBS_1500";
}

static void v_cg_gen_pack(boolean alias)
{
	unsigned int i, j, size = 0, ad_size = 0;
	boolean printed = FALSE;
	boolean address = FALSE;
	boolean no_param;

	FILE *f;
	f = VCGData.nodes[VCGData.type];
	printf("generating function: verse_send_%s\n", VCGData.func_name);
	if(alias)
		fprintf(f, "void verse_send_%s(", VCGData.alias_name);
	else
		fprintf(f, "void verse_send_%s(", VCGData.func_name);
	v_cg_gen_func_params(f, FALSE, alias);
	fprintf(f, ")\n{\n\tuint8 *buf;\n");
	fprintf(f, "\tunsigned int buffer_pos = 0;\n");
	fprintf(f, "\tVCMDBufHead *head;\n");
	fprintf(f, "\thead = v_cmd_buf_allocate(%s);/* Allocating the buffer */\n", v_cg_compute_buffer_size());
	fprintf(f, "\tbuf = ((VCMDBuffer10 *)head)->buf;\n\n");

	fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], %u);\t/* Pack the command. */\n", VCGData.cmd_id);

	fprintf(f, "#if defined V_PRINT_SEND_COMMANDS\n");
	v_cg_create_print(f, TRUE, alias);
	fprintf(f, "#endif\n");

	for(i = 0; i < VCGData.param_count; i++)
	{
		const char *param = VCGData.param_name[i];
		no_param = FALSE;
		if(alias)
		{
			if(i >= VCGData.alias_param && VCGData.alias_param_array == NULL)
				no_param = TRUE;
			if(VCGData.alias_param_array != NULL)
			{
				for(j = 0; j < VCGData.alias_param; j++)
					if(VCGData.alias_param_array[j] == i)
						break;
				if(j == VCGData.alias_param)
					no_param = TRUE;
			}
		}

		if(no_param)
			param = "-1";

		switch(VCGData.param_type[i])
		{
			case VCGP_UINT8 :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], %s);\n", param);
			break;
			case VCGP_UINT16 :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], %s);\n", param);
			break;
			case VCGP_UINT32 :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], %s);\n", param);
			break;
			case VCGP_ENUM :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)%s);\n", param);
			break;
		}
		if(VCGData.param_type[i] == VCGP_REAL32)
		{
			if(no_param)
				param = "V_REAL32_MAX";
			fprintf(f, "\tbuffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], %s);\n", param);
		}
		if(VCGData.param_type[i] == VCGP_REAL64)
		{
			if(no_param)
				param = "V_REAL64_MAX";
			fprintf(f, "\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], %s);\n", param);
		}
		if(no_param)
			param = "NULL";
		switch(VCGData.param_type[i])
		{
			case VCGP_NAME :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], %s, 16);\n", param);
			break;
			case VCGP_LONG_NAME :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], %s, 512);\n", param);
			break;
		}
		if(no_param)
		{
			/* Horrible work-around, that prevents vertex/polygon deletes from misbehaving. */
			if(strncmp(VCGData.alias_name, "g_vertex_delete_real", 20) == 0 && i == 1)
				param = "0";
			else if(strncmp(VCGData.alias_name, "g_polygon_delete", 16) == 0 && i == 1)
				param = "1";
			else
				param = "-1";
		}
		switch(VCGData.param_type[i])
		{	
			case VCGP_NODE_ID :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], %s);\n", param);
			break;
			case VCGP_LAYER_ID :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], %s);\n", param);
			break;
			case VCGP_BUFFER_ID :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], %s);\n", param);
			break;
			case VCGP_FRAGMENT_ID :
				fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], %s);\n", param);
			break;
		}
		if(!alias && VCGData.param_type[i] == VCGP_PACK_INLINE)
				fprintf(f, "%s", VCGData.param_name[i]);
	}
	if(VCGData.alias_name != NULL && VCGData.alias_bool_switch)
	{
		if(alias)
			fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], FALSE);\n");
		else
			fprintf(f, "\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], TRUE);\n");
	}
	v_cg_set_command_address(f, alias);
	fprintf(f, "\tv_cmd_buf_set_size(head, buffer_pos);\n");
	
	fprintf(f, "\tv_noq_send_buf(v_con_get_network_queue(), head);\n");
	fprintf(f, "}\n\n");
}

static void v_cg_gen_unpack(void)
{
	FILE *f;
	unsigned int i;
	boolean printed = FALSE;

	f = VCGData.nodes[VCGData.type];
	printf("generating function: v_unpack_%s\n", VCGData.func_name);
	fprintf(f, "unsigned int v_unpack_%s(const char *buf, size_t buffer_length)\n", VCGData.func_name);
	fprintf(f, "{\n");
	for(i = 0; i < VCGData.param_count && VCGData.param_type[i] != VCGP_ENUM; i++);
	if(i < VCGData.param_count)
		fprintf(f, "\tuint8 enum_temp;\n");
	fprintf(f, "\tunsigned int buffer_pos = 0;\n");
	fprintf(f, "\tvoid (* func_%s)(void *user_data, ", VCGData.func_name);
	v_cg_gen_func_params(f, FALSE, FALSE);
	fprintf(f, ");\n\t");
	v_cg_gen_func_params(f, TRUE, FALSE);
	if(VCGData.alias_name != NULL && VCGData.alias_bool_switch)
		fprintf(f, "uint8\talias_bool;\n");
	fprintf(f, "\n\tfunc_%s = v_fs_get_user_func(%u);\n", VCGData.func_name, VCGData.cmd_id);
	fprintf(f, "\tif(buffer_length < %u)\n\t\treturn -1;\n", v_cg_compute_command_size(0, TRUE));
	for(i = 0; i < VCGData.param_count; i++)
	{
		switch(VCGData.param_type[i])
		{
			case VCGP_UINT8 :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &%s);\n", VCGData.param_name[i]);
			break;
			case VCGP_UINT16 :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &%s);\n", VCGData.param_name[i]);
			break;
			case VCGP_UINT32 :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &%s);\n", VCGData.param_name[i]);
			break;
			case VCGP_REAL32 :
			fprintf(f, "\tbuffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &%s);\n", VCGData.param_name[i]);
			break;
			case VCGP_REAL64 :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &%s);\n", VCGData.param_name[i]);
			break;
			case VCGP_POINTER_TYPE :
			break;
			case VCGP_POINTER :
			break;
			case VCGP_NAME :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], %s, 16, buffer_length - buffer_pos);\n", VCGData.param_name[i]);
				if(i + 1 < VCGData.param_count)
					fprintf(f, "\tif(buffer_length < %u + buffer_pos)\n\t\treturn -1;\n", v_cg_compute_command_size(i + 1, TRUE));
			break;
			case VCGP_LONG_NAME :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], %s, 512, buffer_length - buffer_pos);\n", VCGData.param_name[i]);
				if(i + 1 < VCGData.param_count)
					fprintf(f, "\tif(buffer_length < %u + buffer_pos)\n\t\treturn -1;\n", v_cg_compute_command_size(i + 1, TRUE));
			break;
			case VCGP_NODE_ID :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &%s);\n", VCGData.param_name[i]);
			break;
			case VCGP_LAYER_ID :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &%s);\n", VCGData.param_name[i]);
			break;
			case VCGP_BUFFER_ID :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &%s);\n", VCGData.param_name[i]);
			break;
			case VCGP_FRAGMENT_ID :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &%s);\n", VCGData.param_name[i]);
			break;
			case VCGP_ENUM :
				fprintf(f, "\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &enum_temp);\n");
				fprintf(f, "\t%s = (%s)enum_temp;\n", VCGData.param_name[i], VCGData.param_name[i - 1]);
			break;
			case VCGP_UNPACK_INLINE :
				if(!printed)
				{
					fprintf(f, "#if defined V_PRINT_RECEIVE_COMMANDS\n");
					if(VCGData.alias_name != NULL)
					{
						fprintf(f, "\t%s\n\t", VCGData.alias_qualifier);
						v_cg_create_print(f, FALSE, TRUE);
						fprintf(f, "\telse\n\t");
					}
					v_cg_create_print(f, FALSE, FALSE);
					fprintf(f, "#endif\n");
					printed = TRUE;
				}
				fprintf(f, "%s\n", VCGData.param_name[i++]);
			break;
		}
	}
	if(VCGData.alias_name != NULL && VCGData.alias_bool_switch)
	{
		fprintf(f, "\tif(buffer_length < buffer_pos + 1)\n");
		fprintf(f, "\t\treturn -1;\n");
		fprintf(f, "\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &alias_bool);\n");
	}
	if(!printed)
	{
		fprintf(f, "#if defined V_PRINT_RECEIVE_COMMANDS\n");
		if(VCGData.alias_name != NULL)
		{
			if(VCGData.alias_qualifier != NULL)
				fprintf(f, "\t%s\n\t", VCGData.alias_qualifier);
			else
				fprintf(f, "\tif(!alias_bool)\n\t");
			v_cg_create_print(f, FALSE, TRUE);
			fprintf(f, "\telse\n\t");
		}
		v_cg_create_print(f, FALSE, FALSE);
		fprintf(f, "#endif\n");
		printed = TRUE;
	}

	if(VCGData.alias_name != NULL)
	{
		unsigned int	active;

		if(VCGData.alias_bool_switch)
			fprintf(f, "\tif(!alias_bool)\n");
		else
			fprintf(f, "\t%s\n", VCGData.alias_qualifier);
		fprintf(f, "\t{\n");
		fprintf(f, "\t\tvoid (* alias_%s)(void *user_data, ", VCGData.alias_name);
		v_cg_gen_func_params(f, FALSE, TRUE);
		fprintf(f, ");\n");
		fprintf(f, "\t\talias_%s = v_fs_get_alias_user_func(%u);\n", VCGData.alias_name, VCGData.cmd_id);
		fprintf(f, "\t\tif(alias_%s != NULL)\n", VCGData.alias_name);
		fprintf(f, "\t\t\talias_%s(v_fs_get_alias_user_data(%u)", VCGData.alias_name, VCGData.cmd_id);
		for(i = 0; i < VCGData.param_count && i < VCGData.alias_param; i++)
		{
			if(VCGData.alias_param_array != NULL)
				active = VCGData.alias_param_array[i];
			else
				active = i;

			if(VCGData.param_type[active] != VCGP_PACK_INLINE &&
			   VCGData.param_type[active] != VCGP_UNPACK_INLINE &&
			   VCGData.param_type[active] != VCGP_END_ADDRESS &&
			   VCGData.param_type[active] != VCGP_POINTER_TYPE)
			{
				if(VCGData.param_type[active] == VCGP_ENUM_NAME)
				{
					fprintf(f, ", (%s)%s", VCGData.param_name[active], VCGData.param_name[active + 1]);
					i++;
				}
				else
					fprintf(f, ", %s", VCGData.param_name[active]);
			}
		}
		fprintf(f, ");\n\t\treturn buffer_pos;\n\t}\n");
	}
	
	fprintf(f, "\tif(func_%s != NULL)\n", VCGData.func_name);
	fprintf(f, "\t\tfunc_%s(v_fs_get_user_data(%u)", VCGData.func_name, VCGData.cmd_id);
	for(i = 0; i < VCGData.param_count; i++)
	{
		if(VCGData.param_type[i] != VCGP_PACK_INLINE && VCGData.param_type[i] != VCGP_UNPACK_INLINE && VCGData.param_type[i] != VCGP_END_ADDRESS && VCGData.param_type[i] != VCGP_POINTER_TYPE)
		{
			if(VCGData.param_type[i] == VCGP_ENUM_NAME)
			{
				fprintf(f, ", (%s) %s", VCGData.param_name[i], VCGData.param_name[i + 1]);
				i++;
			}
			else
				fprintf(f, ", %s", VCGData.param_name[i]);
		}
	}
	fprintf(f, ");\n");
	fprintf(f, "\n\treturn buffer_pos;\n");
	fprintf(f, "}\n\n");
}

static void v_cg_gen_alias(void)
{
	FILE *f;
	unsigned int i;
	f = VCGData.nodes[VCGData.type];
	fprintf(f, "void verse_send_%s(", VCGData.alias_name);
	v_cg_gen_func_params(f, FALSE, TRUE);
	fprintf(f, ")\n{\n");
	fprintf(f, "\tverse_send_%s(", VCGData.func_name);
	for(i = 0; i < VCGData.param_count; i++)
		if(VCGData.param_type[i] != VCGP_ENUM_NAME && VCGData.param_type[i] != VCGP_PACK_INLINE && VCGData.param_type[i] != VCGP_UNPACK_INLINE && VCGData.param_type[i] != VCGP_END_ADDRESS && VCGData.param_type[i] != VCGP_POINTER_TYPE)
			fprintf(f, ", %s", VCGData.param_name[i]);
	fprintf(f, "}\n\n");
}

static void v_cg_gen_init(void)
{
	FILE *f;
	f = VCGData.init;
	fprintf(f, "\tv_fs_add_func(%i, v_unpack_%s, verse_send_%s, ", VCGData.cmd_id, VCGData.func_name, VCGData.func_name);
	if(VCGData.alias_name != NULL)
		fprintf(f, "verse_send_%s);\n", VCGData.alias_name);
	else
		fprintf(f, "NULL);\n");
}

static void v_cg_gen_verse_h(void)
{
	FILE *f;
	if(VCGData.command == VCGCT_INVISIBLE_SYSTEM)
		f = VCGData.internal_verse_h;
	else
		f = VCGData.verse_h;
	fprintf(f, "extern void verse_send_%s(", VCGData.func_name);
	v_cg_gen_func_params(f, FALSE, FALSE);
	fprintf(f, ");\n");
	if(VCGData.alias_name != NULL)
	{
		fprintf(f, "extern void verse_send_%s(", VCGData.alias_name);
		v_cg_gen_func_params(f, FALSE, TRUE);
		fprintf(f, ");\n");
	}
}

static void v_cg_gen_unpack_h(void)
{
	fprintf(VCGData.unpack, "extern unsigned int v_unpack_%s(const char *data, size_t length);\n", VCGData.func_name);
}

void v_cg_end_cmd(void)
{
	v_cg_gen_pack(FALSE);
	if(VCGData.alias_name != NULL)
		v_cg_gen_pack(TRUE);
	v_cg_gen_unpack();
	v_cg_gen_init();
	v_cg_gen_verse_h();
	v_cg_gen_unpack_h();
	VCGData.alias_name = NULL;
}

int main(int argc, char *argv[])
{
	printf("start\n");
	v_cg_init();
	v_gen_system_cmd_def();
	fprintf(VCGData.verse_h, "\n");
	v_gen_object_cmd_def();
	fprintf(VCGData.verse_h, "\n");
	v_gen_geometry_cmd_def();
	fprintf(VCGData.verse_h, "\n");
	v_gen_material_cmd_def();
	fprintf(VCGData.verse_h, "\n");
	v_gen_bitmap_cmd_def();
	fprintf(VCGData.verse_h, "\n");
	v_gen_text_cmd_def();
	fprintf(VCGData.verse_h, "\n");
	v_gen_curve_cmd_def();
	fprintf(VCGData.verse_h, "\n");
	v_gen_audio_cmd_def();
	fprintf(VCGData.verse_h, "\n");
	v_cg_close();
	printf("end\n");

	return EXIT_SUCCESS;
}

#endif
