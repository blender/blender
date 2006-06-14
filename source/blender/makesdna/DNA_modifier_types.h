/**
 * $Id$ 
 */

#ifndef DNA_MODIFIER_TYPES_H
#define DNA_MODIFIER_TYPES_H

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE! */

typedef enum ModifierType {
	eModifierType_None = 0,
	eModifierType_Subsurf,
	eModifierType_Lattice,
	eModifierType_Curve,
	eModifierType_Build,
	eModifierType_Mirror,
	eModifierType_Decimate,
	eModifierType_Wave,
	eModifierType_Armature,
	eModifierType_Hook,
	eModifierType_Softbody,
	eModifierType_Boolean,
	eModifierType_Array,

	NUM_MODIFIER_TYPES
} ModifierType;

	/* These numerical values are explicitly chosen so that 
	 * (mode&1) is true for realtime calc and (mode&2) is true
	 * for render calc.
	 */
typedef enum ModifierMode {
	eModifierMode_Realtime = (1<<0),
	eModifierMode_Render = (1<<1),
	eModifierMode_Editmode = (1<<2),
	eModifierMode_OnCage = (1<<3),
	eModifierMode_Expanded = (1<<4),
	eModifierMode_Virtual = (1<<5),
} ModifierMode;

typedef struct ModifierData {
	struct ModifierData *next, *prev;

	int type, mode;
	char name[32];

	char *error;
} ModifierData;

typedef enum {
	eSubsurfModifierFlag_Incremental = (1<<0),
	eSubsurfModifierFlag_DebugIncr = (1<<1),
	eSubsurfModifierFlag_ControlEdges = (1<<2),
	eSubsurfModifierFlag_SubsurfUv = (1<<3)
} SubsurfModifierFlag;

typedef struct SubsurfModifierData {
	ModifierData modifier;

	short subdivType, levels, renderLevels, flags;

	void *emCache, *mCache;
} SubsurfModifierData;

typedef struct LatticeModifierData {
	ModifierData modifier;

	struct Object *object;
	char name[32];			/* optional vertexgroup name */
} LatticeModifierData;

typedef struct CurveModifierData {
	ModifierData modifier;

	struct Object *object;
	char name[32];			/* optional vertexgroup name */
} CurveModifierData;

typedef struct BuildModifierData {
	ModifierData modifier;

	float start, length;
	int randomize, seed;
} BuildModifierData;

typedef struct ArrayModifierData {
	ModifierData modifier;

	/* the curve object to use for MOD_ARR_FITCURVE */
	struct Object *curve_ob;
	/* the object to use for object offset */
	struct Object *offset_ob;
	/* a constant duplicate offset;
	   1 means the duplicates are 1 unit apart
    */
	float offset[3];
	/* a scaled factor for duplicate offsets;
	   1 means the duplicates are 1 object-width apart
    */
	float scale[3];
	/* the length over which to distribute the duplicates */
	float length;
	/* the limit below which to merge vertices in adjacent duplicates */
	float merge_dist;
	/* determines how duplicate count is calculated; one of:
	      MOD_ARR_FIXEDCOUNT -> fixed
	      MOD_ARR_FITLENGTH  -> calculated to fit a set length
	      MOD_ARR_FITCURVE   -> calculated to fit the length of a Curve object
    */
	int fit_type;
	/* flags specifying how total offset is calculated; binary OR of:
	     MOD_ARR_OFF_CONST    -> total offset += offset
	     MOD_ARR_OFF_RELATIVE -> total offset += relative * object width
	     MOD_ARR_OFF_OBJ      -> total offset += offset_ob's matrix
	   total offset is the sum of the individual enabled offsets
	*/
	int offset_type;
	/* general flags:
	      MOD_ARR_MERGE -> merge vertices in adjacent duplicates
	*/
	int flags;
	/* the number of duplicates to generate for MOD_ARR_FIXEDCOUNT */
	int count;
} ArrayModifierData;

/* ArrayModifierData->fit_type */
#define MOD_ARR_FIXEDCOUNT 0
#define MOD_ARR_FITLENGTH  1
#define MOD_ARR_FITCURVE   2

/* ArrayModifierData->offset_type */
#define MOD_ARR_OFF_CONST    1<<0
#define MOD_ARR_OFF_RELATIVE 1<<1
#define MOD_ARR_OFF_OBJ      1<<2

/* ArrayModifierData->flags */
#define MOD_ARR_MERGE      1<<0
#define MOD_ARR_MERGEFINAL 1<<1

typedef struct MirrorModifierData {
	ModifierData modifier;

	short axis, flag;
	float tolerance;
} MirrorModifierData;

/* MirrorModifierData->flag */
#define MOD_MIR_CLIPPING	1

typedef struct DecimateModifierData {
	ModifierData modifier;

	float percent;
	int faceCount;
} DecimateModifierData;

typedef struct WaveModifierData {
	ModifierData modifier;

	short flag, pad;

	float startx, starty, height, width;
	float narrow, speed, damp;
	
	float timeoffs, lifetime;
} WaveModifierData;

typedef struct ArmatureModifierData {
	ModifierData modifier;

	short deformflag, pad1;		/* deformflag replaces armature->deformflag */
	int pad2;
	struct Object *object;
} ArmatureModifierData;

typedef struct HookModifierData {
	ModifierData modifier;

	struct Object *object;
	float parentinv[4][4];	/* matrix making current transform unmodified */
	float cent[3];			/* visualization of hook */
	float falloff;			/* if not zero, falloff is distance where influence zero */
	
	int *indexar;			/* if NULL, it's using vertexgroup */
	int totindex;
	float force;
	char name[32];			/* optional vertexgroup name */
} HookModifierData;

typedef struct SoftbodyModifierData {
	ModifierData modifier;
} SoftbodyModifierData;

typedef enum {
	eBooleanModifierOp_Intersect,
	eBooleanModifierOp_Union,
	eBooleanModifierOp_Difference,
} BooleanModifierOp;
typedef struct BooleanModifierData {
	ModifierData modifier;

	struct Object *object;
	int operation, pad;
} BooleanModifierData;

#endif
