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
