/*
**
*/

/* Define this to at least build the code that regenerates the variable parts of the code. */
/*#define V_GENERATE_FUNC_MODE*/

typedef enum {
	VCGP_UINT8,
	VCGP_UINT16,
	VCGP_UINT32,
	VCGP_REAL32,
	VCGP_REAL64,
	VCGP_POINTER_TYPE,
	VCGP_POINTER,
	VCGP_NAME,
	VCGP_LONG_NAME,
	VCGP_NODE_ID,
	VCGP_LAYER_ID,
	VCGP_BUFFER_ID,
	VCGP_FRAGMENT_ID,
	VCGP_ENUM_NAME,
	VCGP_ENUM,
	VCGP_PACK_INLINE,
	VCGP_UNPACK_INLINE,
	VCGP_END_ADDRESS
} VCGParam;

typedef enum {
	VCGCT_NORMAL,
	VCGCT_UNIQUE,
	VCGCT_ONCE,
	VCGCT_INVISIBLE_SYSTEM,	/* In the dark we are all invisible. */
	VCGCT_ORDERED
} VCGCommandType;

extern void v_cg_new_cmd(VCGCommandType type, const char *name, unsigned int cmd_id, VCGCommandType command);
extern void v_cg_add_param(VCGParam type, const char *name);
extern void v_cg_alias(char bool_switch, const char *name, const char *qualifier,
		       unsigned int param, unsigned int *param_array);
extern void v_cg_end_cmd(void);
extern void v_cg_new_manual_cmd(unsigned int cmd_id, const char *name, const char *params, const char *alias_name, const char *alias_params);
