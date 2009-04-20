/*
** Verse API Header file (for use with libverse.a).
** This is automatically generated code; do not edit.
*/


#if !defined VERSE_H

#if defined __cplusplus		/* Declare as C symbols for C++ users. */
extern "C" {
#endif

#define	VERSE_H

#if !defined VERSE_TYPES
#define	VERSE_TYPES

#include <stdlib.h>

/* Release information. */
#define	V_RELEASE_NUMBER	6
#define	V_RELEASE_PATCH		1
#define	V_RELEASE_LABEL		""

typedef unsigned char	boolean;
typedef signed char	int8;
typedef unsigned char	uint8;
typedef short		int16;
typedef unsigned short	uint16;
typedef int		int32;
typedef unsigned int	uint32;
typedef float		real32;
typedef double		real64;

#define V_REAL64_MAX         1.7976931348623158e+308
#define V_REAL32_MAX         3.402823466e+38f

#if !defined TRUE
#define TRUE  1
#define FALSE 0
#endif

#define V_HOST_ID_SIZE	(3 * (512 / 8))		/* The size of host IDs (keys), in 8-bit bytes. */

typedef enum {
	V_NT_OBJECT = 0, 
	V_NT_GEOMETRY, 
	V_NT_MATERIAL, 
	V_NT_BITMAP, 
	V_NT_TEXT, 
	V_NT_CURVE, 
	V_NT_AUDIO, 
	V_NT_NUM_TYPES, 
	V_NT_SYSTEM = V_NT_NUM_TYPES, 
	V_NT_NUM_TYPES_NETPACK
} VNodeType;

typedef uint32		VNodeID;
typedef uint16		VLayerID;		/* Commonly used to identify layers, nodes that have them. */
typedef uint16		VBufferID;		/* Commonly used to identify buffers, nodes that have them. */
typedef uint16		VNMFragmentID;

typedef void *		VSession;

#define V_MAX_NAME_LENGTH_SHORT 16
#define V_MAX_NAME_LENGTH_LONG 48
#define V_MAX_NAME_PASS_LENGTH 128

typedef enum {
	VN_OWNER_OTHER = 0,
	VN_OWNER_MINE
} VNodeOwner;

typedef enum {
	VN_O_METHOD_PTYPE_INT8 = 0,
	VN_O_METHOD_PTYPE_INT16,
	VN_O_METHOD_PTYPE_INT32,

	VN_O_METHOD_PTYPE_UINT8,
	VN_O_METHOD_PTYPE_UINT16,
	VN_O_METHOD_PTYPE_UINT32,

	VN_O_METHOD_PTYPE_REAL32,
	VN_O_METHOD_PTYPE_REAL64,

	VN_O_METHOD_PTYPE_REAL32_VEC2,
	VN_O_METHOD_PTYPE_REAL32_VEC3,
	VN_O_METHOD_PTYPE_REAL32_VEC4,

	VN_O_METHOD_PTYPE_REAL64_VEC2,
	VN_O_METHOD_PTYPE_REAL64_VEC3,
	VN_O_METHOD_PTYPE_REAL64_VEC4,

	VN_O_METHOD_PTYPE_REAL32_MAT4,
	VN_O_METHOD_PTYPE_REAL32_MAT9,
	VN_O_METHOD_PTYPE_REAL32_MAT16,

	VN_O_METHOD_PTYPE_REAL64_MAT4,
	VN_O_METHOD_PTYPE_REAL64_MAT9,
	VN_O_METHOD_PTYPE_REAL64_MAT16,

	VN_O_METHOD_PTYPE_STRING,

	VN_O_METHOD_PTYPE_NODE,
	VN_O_METHOD_PTYPE_LAYER
} VNOParamType;

typedef	union {
	int8		vint8;
	int16		vint16;
	int32		vint32;
	uint8		vuint8;
	uint16		vuint16;
	uint32		vuint32;
	real32		vreal32;
	real64		vreal64;
	real32		vreal32_vec[4];
	real32		vreal32_mat[16];
	real64		vreal64_vec[4];
	real64		vreal64_mat[16];
	char		*vstring;
	VNodeID		vnode;
	VLayerID	vlayer;
} VNOParam;

#define VN_TAG_MAX_BLOB_SIZE 500

typedef enum {
	VN_TAG_BOOLEAN = 0,
	VN_TAG_UINT32,
	VN_TAG_REAL64,
	VN_TAG_STRING,
	VN_TAG_REAL64_VEC3,
	VN_TAG_LINK,
	VN_TAG_ANIMATION,
	VN_TAG_BLOB,
	VN_TAG_TYPE_COUNT
} VNTagType;

typedef enum {
	VN_TAG_GROUP_SIZE = 16,
	VN_TAG_NAME_SIZE = 16,
	VN_TAG_FULL_NAME_SIZE = 64,
	VN_TAG_STRING_SIZE = 128
} VNTagConstants;

typedef union {
	boolean vboolean;
	uint32	vuint32;
	real64	vreal64;
	char	*vstring;
	real64	vreal64_vec3[3];
	VNodeID	vlink;
	struct {
		VNodeID curve;
		uint32 start;
		uint32 end;
	} vanimation;
	struct {
		uint16	size;
		void	*blob;
	} vblob;
} VNTag;

typedef enum {
	VN_S_CONNECT_NAME_SIZE = 32,
	VN_S_CONNECT_KEY_SIZE = 4,
	VN_S_CONNECT_DATA_SIZE = 32,
	VS_S_CONNECT_HOSTID_PRIVATE_SIZE = 3 * 2048 / 8,
	VS_S_CONNECT_HOSTID_PUBLIC_SIZE  = 2 * 2048 / 8
} VNSConnectConstants;

typedef enum {
	VN_FORMAT_REAL32,
	VN_FORMAT_REAL64
} VNRealFormat;

typedef struct {
	real32	x, y, z, w;
} VNQuat32;

typedef struct {
	real64	x, y, z, w;
} VNQuat64;

typedef enum {
	VN_O_METHOD_GROUP_NAME_SIZE = 16,
	VN_O_METHOD_NAME_SIZE = 16,
	VN_O_METHOD_SIG_SIZE = 256
} VNOMethodConstants;

typedef void VNOPackedParams;	/* Opaque type. */

typedef enum {
	VN_G_LAYER_VERTEX_XYZ = 0,
	VN_G_LAYER_VERTEX_UINT32,
	VN_G_LAYER_VERTEX_REAL,
	VN_G_LAYER_POLYGON_CORNER_UINT32 = 128,
	VN_G_LAYER_POLYGON_CORNER_REAL,
	VN_G_LAYER_POLYGON_FACE_UINT8,
	VN_G_LAYER_POLYGON_FACE_UINT32,
	VN_G_LAYER_POLYGON_FACE_REAL
} VNGLayerType;

typedef enum {
	VN_M_LIGHT_DIRECT = 0,
	VN_M_LIGHT_AMBIENT,
	VN_M_LIGHT_DIRECT_AND_AMBIENT,
	VN_M_LIGHT_BACK_DIRECT,
	VN_M_LIGHT_BACK_AMBIENT,
	VN_M_LIGHT_BACK_DIRECT_AND_AMBIENT
} VNMLightType;

typedef enum {
	VN_M_NOISE_PERLIN_ZERO_TO_ONE = 0,
	VN_M_NOISE_PERLIN_MINUS_ONE_TO_ONE,
	VN_M_NOISE_POINT_ZERO_TO_ONE,
	VN_M_NOISE_POINT_MINUS_ONE_TO_ONE
} VNMNoiseType;

typedef enum {
	VN_M_RAMP_SQUARE = 0,
	VN_M_RAMP_LINEAR,
	VN_M_RAMP_SMOOTH
} VNMRampType;

typedef enum {
	VN_M_RAMP_RED = 0,
	VN_M_RAMP_GREEN,
	VN_M_RAMP_BLUE
} VNMRampChannel;

typedef struct {
	real64	pos;
	real64	red;
	real64	green;
	real64	blue;
} VNMRampPoint;

typedef enum {
	VN_M_BLEND_FADE = 0,
	VN_M_BLEND_ADD,
	VN_M_BLEND_SUBTRACT,
	VN_M_BLEND_MULTIPLY,
	VN_M_BLEND_DIVIDE,
} VNMBlendType;

typedef enum {
	VN_M_FT_COLOR = 0,
	VN_M_FT_LIGHT,
	VN_M_FT_REFLECTION,
	VN_M_FT_TRANSPARENCY,
	VN_M_FT_VOLUME,
	VN_M_FT_VIEW,
	VN_M_FT_GEOMETRY,
	VN_M_FT_TEXTURE,
	VN_M_FT_NOISE,
	VN_M_FT_BLENDER,
	VN_M_FT_CLAMP,
	VN_M_FT_MATRIX,
	VN_M_FT_RAMP,
	VN_M_FT_ANIMATION,
	VN_M_FT_ALTERNATIVE,
	VN_M_FT_OUTPUT
} VNMFragmentType;

typedef union {
	struct {
		real64 red;
		real64 green;
		real64 blue;
	} color;
	struct {
		uint8 type;
		real64 normal_falloff; 
		VNodeID brdf;
		char brdf_r[16];
		char brdf_g[16];
		char brdf_b[16];
	} light;
	struct {
		real64 normal_falloff;
	} reflection;
	struct {
		real64 normal_falloff;
		real64 refraction_index;
	} transparency;
	struct {
		real64 diffusion;
		real64 col_r;
		real64 col_g;
		real64 col_b;
	} volume;
	struct {
		char layer_r[16];
		char layer_g[16];
		char layer_b[16];
	} geometry;
	struct{
		VNodeID bitmap;
		char layer_r[16];
		char layer_g[16];
		char layer_b[16];
		boolean filtered;
		VNMFragmentID mapping;
	} texture;
	struct {
		uint8 type;
		VNMFragmentID mapping;
	} noise;
	struct {
		uint8 type;
		VNMFragmentID data_a;
		VNMFragmentID data_b; 
		VNMFragmentID control;
	} blender;
	struct {
		boolean min;
		real64 red;
		real64 green;
		real64 blue;
		VNMFragmentID data;
	} clamp;
	struct {
		real64 matrix[16];
		VNMFragmentID data;
	} matrix;
	struct {
		uint8 type;
		uint8 channel;
		VNMFragmentID mapping; 
		uint8 point_count;
		VNMRampPoint ramp[48];
	} ramp;
	struct {
		char label[16];
	} animation;
	struct {
		VNMFragmentID alt_a;
		VNMFragmentID alt_b;
	} alternative;
	struct {
		char label[16];
		VNMFragmentID front;
		VNMFragmentID back;
	} output;
} VMatFrag;

typedef enum {
	VN_B_LAYER_UINT1 = 0,
	VN_B_LAYER_UINT8,
	VN_B_LAYER_UINT16,
	VN_B_LAYER_REAL32,
	VN_B_LAYER_REAL64
} VNBLayerType;

#define VN_B_TILE_SIZE 8

typedef union{
	uint8 vuint1[8];
	uint8 vuint8[64];
	uint16 vuint16[64];
	real32 vreal32[64];
	real64 vreal64[64];
} VNBTile;

typedef enum {
	VN_T_CONTENT_LANGUAGE_SIZE = 32,
	VN_T_CONTENT_INFO_SIZE = 256,
	VN_T_BUFFER_NAME_SIZE = 16,
	VN_T_MAX_TEXT_CMD_SIZE = 1450
} VNTConstants;

/* This is how many *samples* are included in a block of the given type. Not bytes. */
typedef enum {
	VN_A_BLOCK_SIZE_INT8 = 1024,
	VN_A_BLOCK_SIZE_INT16 = 512,
	VN_A_BLOCK_SIZE_INT24 = 384,
	VN_A_BLOCK_SIZE_INT32 = 256,
	VN_A_BLOCK_SIZE_REAL32 = 256,
	VN_A_BLOCK_SIZE_REAL64 = 128
} VNAConstants;

typedef enum {
	VN_A_BLOCK_INT8,
	VN_A_BLOCK_INT16,
	VN_A_BLOCK_INT24,
	VN_A_BLOCK_INT32,
	VN_A_BLOCK_REAL32,
	VN_A_BLOCK_REAL64
} VNABlockType;

/* Audio commands take pointers to blocks of these. They are not packed as unions. */
typedef union {
	int8	vint8[VN_A_BLOCK_SIZE_INT8];
	int16	vint16[VN_A_BLOCK_SIZE_INT16];
	int32	vint24[VN_A_BLOCK_SIZE_INT24];
	int32	vint32[VN_A_BLOCK_SIZE_INT32];
	real32	vreal32[VN_A_BLOCK_SIZE_REAL32];
	real64	vreal64[VN_A_BLOCK_SIZE_REAL64];
} VNABlock;

extern void		verse_set_port(uint16 port);
extern void		verse_host_id_create(uint8 *id);
extern void		verse_host_id_set(uint8 *id);
extern void		verse_callback_set(void *send_func, void *callback, void *user_data);
extern void		verse_callback_update(uint32 microseconds);
extern void		verse_session_set(VSession session);
extern VSession		verse_session_get(void);
extern void		verse_session_destroy(VSession session);
extern size_t	verse_session_get_size(void);
extern VNodeID	verse_session_get_avatar(void);
extern void		verse_session_get_time(uint32 *seconds, uint32 *fractions);

extern VNOPackedParams * verse_method_call_pack(uint32 param_count, const VNOParamType *param_type, const VNOParam *params);
extern boolean	verse_method_call_unpack(const VNOPackedParams *data, uint32 param_count, const VNOParamType *param_type, VNOParam *params);

/*
#define V_PRINT_SEND_COMMANDS
#define V_PRINT_RECEIVE_COMMANDS
*/

#endif		/* VERSE_TYPES */

/* Command sending functions begin. ----------------------------------------- */

extern VSession verse_send_connect(const char *name, const char *pass, const char *address, const uint8 *expected_host_id);
extern VSession verse_send_connect_accept(VNodeID avatar, const char *address, uint8 *host_id);
extern void verse_send_connect_terminate(const char *address, const char *bye);
extern void verse_send_ping(const char *address, const char *message);
extern void verse_send_node_index_subscribe(uint32 mask);
extern void verse_send_node_create(VNodeID node_id, VNodeType type, VNodeOwner owner);
extern void verse_send_node_destroy(VNodeID node_id);
extern void verse_send_node_subscribe(VNodeID node_id);
extern void verse_send_node_unsubscribe(VNodeID node_id);
extern void verse_send_tag_group_create(VNodeID node_id, uint16 group_id, const char *name);
extern void verse_send_tag_group_destroy(VNodeID node_id, uint16 group_id);
extern void verse_send_tag_group_subscribe(VNodeID node_id, uint16 group_id);
extern void verse_send_tag_group_unsubscribe(VNodeID node_id, uint16 group_id);
extern void verse_send_tag_create(VNodeID node_id, uint16 group_id, uint16 tag_id, const char *name, VNTagType type, const VNTag *tag);
extern void verse_send_tag_destroy(VNodeID node_id, uint16 group_id, uint16 tag_id);
extern void verse_send_node_name_set(VNodeID node_id, const char *name);

extern void verse_send_o_transform_pos_real32(VNodeID node_id, uint32 time_s, uint32 time_f, const real32 *pos, const real32 *speed, const real32 *accelerate, const real32 *drag_normal, real32 drag);
extern void verse_send_o_transform_rot_real32(VNodeID node_id, uint32 time_s, uint32 time_f, const VNQuat32 *rot, const VNQuat32 *speed, const VNQuat32 *accelerate, const VNQuat32 *drag_normal, real32 drag);
extern void verse_send_o_transform_scale_real32(VNodeID node_id, real32 scale_x, real32 scale_y, real32 scale_z);
extern void verse_send_o_transform_pos_real64(VNodeID node_id, uint32 time_s, uint32 time_f, const real64 *pos, const real64 *speed, const real64 *accelerate, const real64 *drag_normal, real64 drag);
extern void verse_send_o_transform_rot_real64(VNodeID node_id, uint32 time_s, uint32 time_f, const VNQuat64 *rot, const VNQuat64 *speed, const VNQuat64 *accelerate, const VNQuat64 *drag_normal, real64 drag);
extern void verse_send_o_transform_scale_real64(VNodeID node_id, real64 scale_x, real64 scale_y, real64 scale_z);
extern void verse_send_o_transform_subscribe(VNodeID node_id, VNRealFormat type);
extern void verse_send_o_transform_unsubscribe(VNodeID node_id, VNRealFormat type);
extern void verse_send_o_light_set(VNodeID node_id, real64 light_r, real64 light_g, real64 light_b);
extern void verse_send_o_link_set(VNodeID node_id, uint16 link_id, VNodeID link, const char *label, uint32 target_id);
extern void verse_send_o_link_destroy(VNodeID node_id, uint16 link_id);
extern void verse_send_o_method_group_create(VNodeID node_id, uint16 group_id, const char *name);
extern void verse_send_o_method_group_destroy(VNodeID node_id, uint16 group_id);
extern void verse_send_o_method_group_subscribe(VNodeID node_id, uint16 group_id);
extern void verse_send_o_method_group_unsubscribe(VNodeID node_id, uint16 group_id);
extern void verse_send_o_method_create(VNodeID node_id, uint16 group_id, uint16 method_id, const char *name, uint8 param_count, const VNOParamType *param_types, const char * *param_names);
extern void verse_send_o_method_destroy(VNodeID node_id, uint16 group_id, uint16 method_id);
extern void verse_send_o_method_call(VNodeID node_id, uint16 group_id, uint16 method_id, VNodeID sender, const VNOPackedParams *params);
extern void verse_send_o_anim_run(VNodeID node_id, uint16 link_id, uint32 time_s, uint32 time_f, uint8 dimensions, const real64 *pos, const real64 *speed, const real64 *accel, const real64 *scale, const real64 *scale_speed);
extern void verse_send_o_hide(VNodeID node_id, uint8 hidden);

extern void verse_send_g_layer_create(VNodeID node_id, VLayerID layer_id, const char *name, VNGLayerType type, uint32 def_uint, real64 def_real);
extern void verse_send_g_layer_destroy(VNodeID node_id, VLayerID layer_id);
extern void verse_send_g_layer_subscribe(VNodeID node_id, VLayerID layer_id, VNRealFormat type);
extern void verse_send_g_layer_unsubscribe(VNodeID node_id, VLayerID layer_id);
extern void verse_send_g_vertex_set_xyz_real32(VNodeID node_id, VLayerID layer_id, uint32 vertex_id, real32 x, real32 y, real32 z);
extern void verse_send_g_vertex_delete_real32(VNodeID node_id, uint32 vertex_id);
extern void verse_send_g_vertex_set_xyz_real64(VNodeID node_id, VLayerID layer_id, uint32 vertex_id, real64 x, real64 y, real64 z);
extern void verse_send_g_vertex_delete_real64(VNodeID node_id, uint32 vertex_id);
extern void verse_send_g_vertex_set_uint32(VNodeID node_id, VLayerID layer_id, uint32 vertex_id, uint32 value);
extern void verse_send_g_vertex_set_real64(VNodeID node_id, VLayerID layer_id, uint32 vertex_id, real64 value);
extern void verse_send_g_vertex_set_real32(VNodeID node_id, VLayerID layer_id, uint32 vertex_id, real32 value);
extern void verse_send_g_polygon_set_corner_uint32(VNodeID node_id, VLayerID layer_id, uint32 polygon_id, uint32 v0, uint32 v1, uint32 v2, uint32 v3);
extern void verse_send_g_polygon_delete(VNodeID node_id, uint32 polygon_id);
extern void verse_send_g_polygon_set_corner_real64(VNodeID node_id, VLayerID layer_id, uint32 polygon_id, real64 v0, real64 v1, real64 v2, real64 v3);
extern void verse_send_g_polygon_set_corner_real32(VNodeID node_id, VLayerID layer_id, uint32 polygon_id, real32 v0, real32 v1, real32 v2, real32 v3);
extern void verse_send_g_polygon_set_face_uint8(VNodeID node_id, VLayerID layer_id, uint32 polygon_id, uint8 value);
extern void verse_send_g_polygon_set_face_uint32(VNodeID node_id, VLayerID layer_id, uint32 polygon_id, uint32 value);
extern void verse_send_g_polygon_set_face_real64(VNodeID node_id, VLayerID layer_id, uint32 polygon_id, real64 value);
extern void verse_send_g_polygon_set_face_real32(VNodeID node_id, VLayerID layer_id, uint32 polygon_id, real32 value);
extern void verse_send_g_crease_set_vertex(VNodeID node_id, const char *layer, uint32 def_crease);
extern void verse_send_g_crease_set_edge(VNodeID node_id, const char *layer, uint32 def_crease);
extern void verse_send_g_bone_create(VNodeID node_id, uint16 bone_id, const char *weight, const char *reference, uint16 parent, real64 pos_x, real64 pos_y, real64 pos_z, const char *position_label, const char *rotation_label, const char *scale_label);
extern void verse_send_g_bone_destroy(VNodeID node_id, uint16 bone_id);

extern void verse_send_m_fragment_create(VNodeID node_id, VNMFragmentID frag_id, VNMFragmentType type, const VMatFrag *fragment);
extern void verse_send_m_fragment_destroy(VNodeID node_id, VNMFragmentID frag_id);

extern void verse_send_b_dimensions_set(VNodeID node_id, uint16 width, uint16 height, uint16 depth);
extern void verse_send_b_layer_create(VNodeID node_id, VLayerID layer_id, const char *name, VNBLayerType type);
extern void verse_send_b_layer_destroy(VNodeID node_id, VLayerID layer_id);
extern void verse_send_b_layer_subscribe(VNodeID node_id, VLayerID layer_id, uint8 level);
extern void verse_send_b_layer_unsubscribe(VNodeID node_id, VLayerID layer_id);
extern void verse_send_b_tile_set(VNodeID node_id, VLayerID layer_id, uint16 tile_x, uint16 tile_y, uint16 z, VNBLayerType type, const VNBTile *tile);

extern void verse_send_t_language_set(VNodeID node_id, const char *language);
extern void verse_send_t_buffer_create(VNodeID node_id, VBufferID buffer_id, const char *name);
extern void verse_send_t_buffer_destroy(VNodeID node_id, VBufferID buffer_id);
extern void verse_send_t_buffer_subscribe(VNodeID node_id, VBufferID buffer_id);
extern void verse_send_t_buffer_unsubscribe(VNodeID node_id, VBufferID buffer_id);
extern void verse_send_t_text_set(VNodeID node_id, VBufferID buffer_id, uint32 pos, uint32 length, const char *text);

extern void verse_send_c_curve_create(VNodeID node_id, VLayerID curve_id, const char *name, uint8 dimensions);
extern void verse_send_c_curve_destroy(VNodeID node_id, VLayerID curve_id);
extern void verse_send_c_curve_subscribe(VNodeID node_id, VLayerID curve_id);
extern void verse_send_c_curve_unsubscribe(VNodeID node_id, VLayerID curve_id);
extern void verse_send_c_key_set(VNodeID node_id, VLayerID curve_id, uint32 key_id, uint8 dimensions, const real64 *pre_value, const uint32 *pre_pos, const real64 *value, real64 pos, const real64 *post_value, const uint32 *post_pos);
extern void verse_send_c_key_destroy(VNodeID node_id, VLayerID curve_id, uint32 key_id);

extern void verse_send_a_buffer_create(VNodeID node_id, VBufferID buffer_id, const char *name, VNABlockType type, real64 frequency);
extern void verse_send_a_buffer_destroy(VNodeID node_id, VBufferID buffer_id);
extern void verse_send_a_buffer_subscribe(VNodeID node_id, VBufferID layer_id);
extern void verse_send_a_buffer_unsubscribe(VNodeID node_id, VBufferID layer_id);
extern void verse_send_a_block_set(VNodeID node_id, VLayerID buffer_id, uint32 block_index, VNABlockType type, const VNABlock *samples);
extern void verse_send_a_block_clear(VNodeID node_id, VLayerID buffer_id, uint32 block_index);
extern void verse_send_a_stream_create(VNodeID node_id, VLayerID stream_id, const char *name);
extern void verse_send_a_stream_destroy(VNodeID node_id, VLayerID stream_id);
extern void verse_send_a_stream_subscribe(VNodeID node_id, VLayerID stream_id);
extern void verse_send_a_stream_unsubscribe(VNodeID node_id, VLayerID stream_id);
extern void verse_send_a_stream(VNodeID node_id, VLayerID stream_id, uint32 time_s, uint32 time_f, VNABlockType type, real64 frequency, const VNABlock *samples);


#if defined __cplusplus
}
#endif

#endif		/* VERSE_H */
