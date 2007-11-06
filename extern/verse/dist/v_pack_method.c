/*
**
*/

#include <stdlib.h>

#include "v_cmd_gen.h"

#if !defined(V_GENERATE_FUNC_MODE)

#include "verse.h"
#include "v_pack.h"

VNOPackedParams * verse_method_call_pack(unsigned int param_count, const VNOParamType *param_type, const VNOParam *params)
{
	unsigned int i, j, buffer_pos;
	uint8 *buf;

	buf = malloc(1500 + 8 * 16);
	buffer_pos = vnp_raw_pack_uint16(buf, 0);
	for(i = 0; i < param_count; i++)
	{
		switch(param_type[i])
		{
			case VN_O_METHOD_PTYPE_INT8 :
				buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], params[i].vint8);
			break;
			case VN_O_METHOD_PTYPE_INT16 :
				buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], params[i].vint16);
			break;
			case VN_O_METHOD_PTYPE_INT32 :
				buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], params[i].vint32);
			break;
			case VN_O_METHOD_PTYPE_UINT8 :
				buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], params[i].vuint8);
			break;
			case VN_O_METHOD_PTYPE_UINT16 :
				buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], params[i].vuint16);
			break;
			case VN_O_METHOD_PTYPE_UINT32 :
				buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], params[i].vuint32);
			break;
			case VN_O_METHOD_PTYPE_REAL32 :
				buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], params[i].vreal32);
			break;
			case VN_O_METHOD_PTYPE_REAL64 :
				buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], params[i].vreal64);
			break;
			case VN_O_METHOD_PTYPE_STRING :
				buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], params[i].vstring, (1500 + 8 * 16) - buffer_pos);
			break;
			case VN_O_METHOD_PTYPE_NODE :
				buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], params[i].vnode);
			break;
			case VN_O_METHOD_PTYPE_LAYER :
				buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], params[i].vlayer);
			break;
			case VN_O_METHOD_PTYPE_REAL32_VEC2 :
				for(j = 0; j < 2; j++)
					buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], params[i].vreal32_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL32_VEC3 :
				for(j = 0; j < 3; j++)
					buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], params[i].vreal32_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL32_VEC4 :
				for(j = 0; j < 4; j++)
					buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], params[i].vreal32_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_VEC2 :
				for(j = 0; j < 2; j++)
					buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], params[i].vreal64_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_VEC3 :
				for(j = 0; j < 3; j++)
					buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], params[i].vreal64_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_VEC4 :
				for(j = 0; j < 4; j++)
					buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], params[i].vreal64_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL32_MAT4 :
				for(j = 0; j < 4; j++)
					buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], params[i].vreal32_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL32_MAT9 :
				for(j = 0; j < 9; j++)
					buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], params[i].vreal32_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL32_MAT16 :
				for(j = 0; j < 16; j++)
					buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], params[i].vreal32_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_MAT4 :
				for(j = 0; j < 4; j++)
					buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], params[i].vreal64_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_MAT9 :
				for(j = 0; j < 9; j++)
					buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], params[i].vreal64_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_MAT16 :
				for(j = 0; j < 16; j++)
					buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], params[i].vreal64_mat[j]);
			break;
		}
		if(buffer_pos > 1500)
		{
			free(buf);
			return NULL;
		}
	}
	vnp_raw_pack_uint16(buf, buffer_pos);
	return buf;
}

boolean verse_method_call_unpack(const VNOPackedParams *data, unsigned int param_count, const VNOParamType *param_type, VNOParam *params)
{
	unsigned int i, j, buffer_pos = 0, len;
	uint16 size;
	const uint8 *buf;
	static char string[2048];
	char *stringput = string;

	buf = data;
	buffer_pos += vnp_raw_unpack_uint16(buf, &size);
	for(i = 0; i < param_count; i++)
	{
		switch(param_type[i])
		{
			case VN_O_METHOD_PTYPE_INT8 :
				buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &params[i].vint8);
			break;
			case VN_O_METHOD_PTYPE_INT16 :
				buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &params[i].vint16);
			break;
			case VN_O_METHOD_PTYPE_INT32 :
				buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &params[i].vint32);
			break;
			case VN_O_METHOD_PTYPE_UINT8 :
				buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &params[i].vuint8);
			break;
			case VN_O_METHOD_PTYPE_UINT16 :
				buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &params[i].vuint16);
			break;
			case VN_O_METHOD_PTYPE_UINT32 :
				buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &params[i].vuint32);
			break;
			case VN_O_METHOD_PTYPE_REAL32 :
				buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &params[i].vreal32);
			break;
			case VN_O_METHOD_PTYPE_REAL64 :
				buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &params[i].vreal64);
			break;
			case VN_O_METHOD_PTYPE_STRING :
				params[i].vstring = stringput;
				len = vnp_raw_unpack_string(&buf[buffer_pos], stringput, (1500 + 8 * 16) - buffer_pos, -1);
				stringput += len;
				buffer_pos += len;
			break;
			case VN_O_METHOD_PTYPE_NODE :
				buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &params[i].vnode);
			break;
			case VN_O_METHOD_PTYPE_LAYER :
				buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &params[i].vlayer);
			break;
			case VN_O_METHOD_PTYPE_REAL32_VEC2 :
				for(j = 0; j < 2; j++)
					buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &params[i].vreal32_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL32_VEC3 :
				for(j = 0; j < 3; j++)
					buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &params[i].vreal32_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL32_VEC4 :
				for(j = 0; j < 4; j++)
					buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &params[i].vreal32_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_VEC2 :
				for(j = 0; j < 2; j++)
					buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &params[i].vreal64_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_VEC3 :
				for(j = 0; j < 3; j++)
					buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &params[i].vreal64_vec[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_VEC4 :
				for(j = 0; j < 4; j++)
					buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &params[i].vreal64_vec[j]);
			break;			
			case VN_O_METHOD_PTYPE_REAL32_MAT4 :
				for(j = 0; j < 4; j++)
					buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &params[i].vreal32_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL32_MAT9 :
				for(j = 0; j < 9; j++)
					buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &params[i].vreal32_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL32_MAT16 :
				for(j = 0; j < 16; j++)
					buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &params[i].vreal32_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_MAT4 :
				for(j = 0; j < 4; j++)
					buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &params[i].vreal64_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_MAT9 :
				for(j = 0; j < 9; j++)
					buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &params[i].vreal64_mat[j]);
			break;
			case VN_O_METHOD_PTYPE_REAL64_MAT16 :
				for(j = 0; j < 16; j++)
					buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &params[i].vreal64_mat[j]);
			break;
		}
	}
	return TRUE;
}
#endif
