/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_modifier_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_MODIFIER_TYPES_H__
#define __DNA_MODIFIER_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

typedef enum ModifierType {
	eModifierType_None              = 0,
	eModifierType_Subsurf           = 1,
	eModifierType_Lattice           = 2,
	eModifierType_Curve             = 3,
	eModifierType_Build             = 4,
	eModifierType_Mirror            = 5,
	eModifierType_Decimate          = 6,
	eModifierType_Wave              = 7,
	eModifierType_Armature          = 8,
	eModifierType_Hook              = 9,
	eModifierType_Softbody          = 10,
	eModifierType_Boolean           = 11,
	eModifierType_Array             = 12,
	eModifierType_EdgeSplit         = 13,
	eModifierType_Displace          = 14,
	eModifierType_UVProject         = 15,
	eModifierType_Smooth            = 16,
	eModifierType_Cast              = 17,
	eModifierType_MeshDeform        = 18,
	eModifierType_ParticleSystem    = 19,
	eModifierType_ParticleInstance  = 20,
	eModifierType_Explode           = 21,
	eModifierType_Cloth             = 22,
	eModifierType_Collision         = 23,
	eModifierType_Bevel             = 24,
	eModifierType_Shrinkwrap        = 25,
	eModifierType_Fluidsim          = 26,
	eModifierType_Mask              = 27,
	eModifierType_SimpleDeform      = 28,
	eModifierType_Multires          = 29,
	eModifierType_Surface           = 30,
	eModifierType_Smoke             = 31,
	eModifierType_ShapeKey          = 32,
	eModifierType_Solidify          = 33,
	eModifierType_Screw             = 34,
	eModifierType_Warp              = 35,
	eModifierType_WeightVGEdit      = 36,
	eModifierType_WeightVGMix       = 37,
	eModifierType_WeightVGProximity = 38,
	eModifierType_Ocean             = 39,
	eModifierType_DynamicPaint      = 40,
	eModifierType_Remesh            = 41,
	eModifierType_Skin              = 42,
	eModifierType_LaplacianSmooth   = 43,
	eModifierType_Triangulate       = 44,
	eModifierType_UVWarp            = 45,
	eModifierType_MeshCache         = 46,
	eModifierType_LaplacianDeform   = 47,
	eModifierType_Wireframe         = 48,
	NUM_MODIFIER_TYPES
} ModifierType;

typedef enum ModifierMode {
	eModifierMode_Realtime          = (1 << 0),
	eModifierMode_Render            = (1 << 1),
	eModifierMode_Editmode          = (1 << 2),
	eModifierMode_OnCage            = (1 << 3),
	eModifierMode_Expanded          = (1 << 4),
	eModifierMode_Virtual           = (1 << 5),
	eModifierMode_ApplyOnSpline     = (1 << 6),
	eModifierMode_DisableTemporary  = (1 << 31)
} ModifierMode;

typedef struct ModifierData {
	struct ModifierData *next, *prev;

	int type, mode;
	int stackindex, pad;
	char name[64];  /* MAX_NAME */

	/* XXX for timing info set by caller... solve later? (ton) */
	struct Scene *scene;

	char *error;
} ModifierData;

typedef enum {
	eSubsurfModifierFlag_Incremental  = (1 << 0),
	eSubsurfModifierFlag_DebugIncr    = (1 << 1),
	eSubsurfModifierFlag_ControlEdges = (1 << 2),
	eSubsurfModifierFlag_SubsurfUv    = (1 << 3),
} SubsurfModifierFlag;

/* not a real modifier */
typedef struct MappingInfoModifierData {
	ModifierData modifier;

	struct Tex *texture;
	struct Object *map_object;
	char uvlayer_name[64];  /* MAX_CUSTOMDATA_LAYER_NAME */
	int uvlayer_tmp;
	int texmapping;
} MappingInfoModifierData;

typedef struct SubsurfModifierData {
	ModifierData modifier;

	short subdivType, levels, renderLevels, flags;

	void *emCache, *mCache;
} SubsurfModifierData;

typedef struct LatticeModifierData {
	ModifierData modifier;

	struct Object *object;
	char name[64];          /* optional vertexgroup name, MAX_VGROUP_NAME */
	float strength;
	char pad[4];
} LatticeModifierData;

typedef struct CurveModifierData {
	ModifierData modifier;

	struct Object *object;
	char name[64];          /* optional vertexgroup name, MAX_VGROUP_NAME */
	short defaxis;          /* axis along which curve deforms */
	char pad[6];
} CurveModifierData;

/* CurveModifierData->defaxis */
enum {
	MOD_CURVE_POSX = 1,
	MOD_CURVE_POSY = 2,
	MOD_CURVE_POSZ = 3,
	MOD_CURVE_NEGX = 4,
	MOD_CURVE_NEGY = 5,
	MOD_CURVE_NEGZ = 6,
};

typedef struct BuildModifierData {
	ModifierData modifier;

	float start, length;
	short flag;
	
	short randomize;      /* (bool) whether order of vertices is randomized - legacy files (for readfile conversion) */
	int seed;             /* (int) random seed */
} BuildModifierData;

/* Build Modifier -> flag */
enum {
	MOD_BUILD_FLAG_RANDOMIZE = (1 << 0),  /* order of vertices is randomized */
	MOD_BUILD_FLAG_REVERSE   = (1 << 1),  /* frame range is reversed, resulting in a deconstruction effect */
};

/* Mask Modifier */
typedef struct MaskModifierData {
	ModifierData modifier;

	struct Object *ob_arm;  /* armature to use to in place of hardcoded vgroup */
	char vgroup[64];        /* name of vertex group to use to mask, MAX_VGROUP_NAME */

	int mode;               /* using armature or hardcoded vgroup */
	int flag;               /* flags for various things */
} MaskModifierData;

/* Mask Modifier -> mode */
enum {
	MOD_MASK_MODE_VGROUP = 0,
	MOD_MASK_MODE_ARM    = 1,
};

/* Mask Modifier -> flag */
enum {
	MOD_MASK_INV         = (1 << 0),
};

typedef struct ArrayModifierData {
	ModifierData modifier;

	/* the object with which to cap the start of the array  */
	struct Object *start_cap;
	/* the object with which to cap the end of the array  */
	struct Object *end_cap;
	/* the curve object to use for MOD_ARR_FITCURVE */
	struct Object *curve_ob;
	/* the object to use for object offset */
	struct Object *offset_ob;
	/* a constant duplicate offset;
	 * 1 means the duplicates are 1 unit apart
	 */
	float offset[3];
	/* a scaled factor for duplicate offsets;
	 * 1 means the duplicates are 1 object-width apart
	 */
	float scale[3];
	/* the length over which to distribute the duplicates */
	float length;
	/* the limit below which to merge vertices in adjacent duplicates */
	float merge_dist;
	/* determines how duplicate count is calculated; one of:
	 * - MOD_ARR_FIXEDCOUNT -> fixed
	 * - MOD_ARR_FITLENGTH  -> calculated to fit a set length
	 * - MOD_ARR_FITCURVE   -> calculated to fit the length of a Curve object
	 */
	int fit_type;
	/* flags specifying how total offset is calculated; binary OR of:
	 * - MOD_ARR_OFF_CONST    -> total offset += offset
	 * - MOD_ARR_OFF_RELATIVE -> total offset += relative * object width
	 * - MOD_ARR_OFF_OBJ      -> total offset += offset_ob's matrix
	 * total offset is the sum of the individual enabled offsets
	 */
	int offset_type;
	/* general flags:
	 * MOD_ARR_MERGE -> merge vertices in adjacent duplicates
	 */
	int flags;
	/* the number of duplicates to generate for MOD_ARR_FIXEDCOUNT */
	int count;
} ArrayModifierData;

/* ArrayModifierData->fit_type */
enum {
	MOD_ARR_FIXEDCOUNT = 0,
	MOD_ARR_FITLENGTH  = 1,
	MOD_ARR_FITCURVE   = 2,
};

/* ArrayModifierData->offset_type */
enum {
	MOD_ARR_OFF_CONST    = (1 << 0),
	MOD_ARR_OFF_RELATIVE = (1 << 1),
	MOD_ARR_OFF_OBJ      = (1 << 2),
};

/* ArrayModifierData->flags */
enum {
	MOD_ARR_MERGE      = (1 << 0),
	MOD_ARR_MERGEFINAL = (1 << 1),
};

typedef struct MirrorModifierData {
	ModifierData modifier;

	short axis  DNA_DEPRECATED; /* deprecated, use flag instead */
	short flag;
	float tolerance;
	struct Object *mirror_ob;
} MirrorModifierData;

/* MirrorModifierData->flag */
enum {
	MOD_MIR_CLIPPING  = (1 << 0),
	MOD_MIR_MIRROR_U  = (1 << 1),
	MOD_MIR_MIRROR_V  = (1 << 2),
	MOD_MIR_AXIS_X    = (1 << 3),
	MOD_MIR_AXIS_Y    = (1 << 4),
	MOD_MIR_AXIS_Z    = (1 << 5),
	MOD_MIR_VGROUP    = (1 << 6),
	MOD_MIR_NO_MERGE  = (1 << 7),
};

typedef struct EdgeSplitModifierData {
	ModifierData modifier;

	float split_angle;    /* angle above which edges should be split */
	int flags;
} EdgeSplitModifierData;

/* EdgeSplitModifierData->flags */
enum {
	MOD_EDGESPLIT_FROMANGLE  = (1 << 1),
	MOD_EDGESPLIT_FROMFLAG   = (1 << 2),
};

typedef struct BevelModifierData {
	ModifierData modifier;

	float value;          /* the "raw" bevel value (distance/amount to bevel) */
	int res;              /* the resolution (as originally coded, it is the number of recursive bevels) */
	short flags;          /* general option flags */
	short val_flags;      /* used to interpret the bevel value */
	short lim_flags;      /* flags to tell the tool how to limit the bevel */
	short e_flags;        /* flags to direct how edge weights are applied to verts */
	float profile;        /* controls profile shape (0->1, .5 is round) */
	/* if the MOD_BEVEL_ANGLE is set, this will be how "sharp" an edge must be before it gets beveled */
	float bevel_angle;
	/* if the MOD_BEVEL_VWEIGHT option is set, this will be the name of the vert group, MAX_VGROUP_NAME */
	char defgrp_name[64];
} BevelModifierData;

/* BevelModifierData->flags and BevelModifierData->lim_flags */
enum {
	MOD_BEVEL_VERT          = (1 << 1),
/*	MOD_BEVEL_RADIUS        = (1 << 2), */
	MOD_BEVEL_ANGLE         = (1 << 3),
	MOD_BEVEL_WEIGHT        = (1 << 4),
	MOD_BEVEL_VGROUP        = (1 << 5),
	MOD_BEVEL_EMIN          = (1 << 7),
	MOD_BEVEL_EMAX          = (1 << 8),
/*	MOD_BEVEL_RUNNING       = (1 << 9), */
/*	MOD_BEVEL_RES           = (1 << 10), */
	/* This is a new setting not related to old (trunk bmesh bevel code)
	 * but adding here because they are mixed - campbell
	 */
/*	MOD_BEVEL_EVEN          = (1 << 11), */
/*	MOD_BEVEL_DIST          = (1 << 12), */  /* same as above */
	MOD_BEVEL_OVERLAP_OK    = (1 << 13),
};

/* BevelModifierData->val_flags (not used as flags any more) */
enum {
	MOD_BEVEL_AMT_OFFSET = 0,
	MOD_BEVEL_AMT_WIDTH = 1,
	MOD_BEVEL_AMT_DEPTH = 2,
	MOD_BEVEL_AMT_PERCENT = 3,
};

typedef struct SmokeModifierData {
	ModifierData modifier;

	struct SmokeDomainSettings *domain;
	struct SmokeFlowSettings *flow; /* inflow, outflow, smoke objects */
	struct SmokeCollSettings *coll; /* collision objects */
	float time;
	int type;  /* domain, inflow, outflow, ... */
} SmokeModifierData;

/* Smoke modifier flags */
enum {
	MOD_SMOKE_TYPE_DOMAIN = (1 << 0),
	MOD_SMOKE_TYPE_FLOW   = (1 << 1),
	MOD_SMOKE_TYPE_COLL   = (1 << 2),
};

typedef struct DisplaceModifierData {
	ModifierData modifier;

	/* keep in sync with MappingInfoModifierData */
	struct Tex *texture;
	struct Object *map_object;
	char uvlayer_name[64];  /* MAX_CUSTOMDATA_LAYER_NAME */
	int uvlayer_tmp;
	int texmapping;
	/* end MappingInfoModifierData */

	float strength;
	int direction;
	char defgrp_name[64];   /* MAX_VGROUP_NAME */
	float midlevel;
	int pad;
} DisplaceModifierData;

/* DisplaceModifierData->direction */
enum {
	MOD_DISP_DIR_X       = 0,
	MOD_DISP_DIR_Y       = 1,
	MOD_DISP_DIR_Z       = 2,
	MOD_DISP_DIR_NOR     = 3,
	MOD_DISP_DIR_RGB_XYZ = 4,
};

/* DisplaceModifierData->texmapping */
enum {
	MOD_DISP_MAP_LOCAL  = 0,
	MOD_DISP_MAP_GLOBAL = 1,
	MOD_DISP_MAP_OBJECT = 2,
	MOD_DISP_MAP_UV     = 3,
};

typedef struct UVProjectModifierData {
	ModifierData modifier;

	/* the objects which do the projecting */
	struct Object *projectors[10]; /* MOD_UVPROJECT_MAXPROJECTORS */
	struct Image *image;           /* the image to project */
	int flags;
	int num_projectors;
	float aspectx, aspecty;
	float scalex, scaley;
	char uvlayer_name[64];         /* MAX_CUSTOMDATA_LAYER_NAME */
	int uvlayer_tmp, pad;
} UVProjectModifierData;

#define MOD_UVPROJECT_MAXPROJECTORS 10

/* UVProjectModifierData->flags */
enum {
	MOD_UVPROJECT_OVERRIDEIMAGE = (1 << 0),
};

typedef struct DecimateModifierData {
	ModifierData modifier;

	float percent;  /* (mode == MOD_DECIM_MODE_COLLAPSE) */
	short iter;     /* (mode == MOD_DECIM_MODE_UNSUBDIV) */
	char delimit;   /* (mode == MOD_DECIM_MODE_DISSOLVE) */
	char pad;
	float angle;    /* (mode == MOD_DECIM_MODE_DISSOLVE) */

	char defgrp_name[64];  /* MAX_VGROUP_NAME */
	short flag, mode;

	/* runtime only */
	int face_count, pad2;
} DecimateModifierData;

enum {
	MOD_DECIM_FLAG_INVERT_VGROUP       = (1 << 0),
	MOD_DECIM_FLAG_TRIANGULATE         = (1 << 1),  /* for collapse only. dont convert tri pairs back to quads */
	MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS  = (1 << 2),  /* for dissolve only. collapse all verts between 2 faces */
};

enum {
	MOD_DECIM_MODE_COLLAPSE,
	MOD_DECIM_MODE_UNSUBDIV,
	MOD_DECIM_MODE_DISSOLVE,  /* called planar in the UI */
};

typedef struct SmoothModifierData {
	ModifierData modifier;
	float fac;
	char defgrp_name[64];	/* MAX_VGROUP_NAME */
	short flag, repeat;

} SmoothModifierData;

/* Smooth modifier flags */
enum {
	MOD_SMOOTH_X = (1 << 1),
	MOD_SMOOTH_Y = (1 << 2),
	MOD_SMOOTH_Z = (1 << 3),
};

typedef struct CastModifierData {
	ModifierData modifier;

	struct Object *object;
	float fac;
	float radius;
	float size;
	char defgrp_name[64];  /* MAX_VGROUP_NAME */
	short flag, type;
} CastModifierData;

/* Cast modifier flags */
enum {
	/* And what bout (1 << 0) flag? ;) */
	MOD_CAST_X                = (1 << 1),
	MOD_CAST_Y                = (1 << 2),
	MOD_CAST_Z                = (1 << 3),
	MOD_CAST_USE_OB_TRANSFORM = (1 << 4),
	MOD_CAST_SIZE_FROM_RADIUS = (1 << 5),
};

/* Cast modifier projection types */
enum {
	MOD_CAST_TYPE_SPHERE   = 0,
	MOD_CAST_TYPE_CYLINDER = 1,
	MOD_CAST_TYPE_CUBOID   = 2,
};

typedef struct WaveModifierData {
	ModifierData modifier;

	/* keep in sync with MappingInfoModifierData */
	struct Tex *texture;
	struct Object *map_object;
	char uvlayer_name[64];  /* MAX_CUSTOMDATA_LAYER_NAME */
	int uvlayer_tmp;
	int texmapping;
	/* end MappingInfoModifierData */

	struct Object *objectcenter;
	char defgrp_name[64];   /* MAX_VGROUP_NAME */

	short flag, pad;

	float startx, starty, height, width;
	float narrow, speed, damp, falloff;

	float timeoffs, lifetime;
	float pad1;
} WaveModifierData;

/* WaveModifierData.flag */
enum {
	/* And what bout (1 << 0) flag? ;) */
	MOD_WAVE_X      = (1 << 1),
	MOD_WAVE_Y      = (1 << 2),
	MOD_WAVE_CYCL   = (1 << 3),
	MOD_WAVE_NORM   = (1 << 4),
	MOD_WAVE_NORM_X = (1 << 5),
	MOD_WAVE_NORM_Y = (1 << 6),
	MOD_WAVE_NORM_Z = (1 << 7),
};

typedef struct ArmatureModifierData {
	ModifierData modifier;

	short deformflag, multi;  /* deformflag replaces armature->deformflag */
	int pad2;
	struct Object *object;
	float *prevCos;           /* stored input of previous modifier, for vertexgroup blending */
	char defgrp_name[64];     /* MAX_VGROUP_NAME */
} ArmatureModifierData;

typedef struct HookModifierData {
	ModifierData modifier;

	struct Object *object;
	char subtarget[64];     /* optional name of bone target, MAX_ID_NAME-2 */

	float parentinv[4][4];  /* matrix making current transform unmodified */
	float cent[3];          /* visualization of hook */
	float falloff;          /* if not zero, falloff is distance where influence zero */

	int *indexar;           /* if NULL, it's using vertexgroup */
	int totindex;
	float force;
	char name[64];          /* optional vertexgroup name, MAX_VGROUP_NAME */
} HookModifierData;

typedef struct SoftbodyModifierData {
	ModifierData modifier;
} SoftbodyModifierData;

typedef struct ClothModifierData {
	ModifierData modifier;

	struct Scene *scene;                  /* the context, time etc is here */
	struct Cloth *clothObject;            /* The internal data structure for cloth. */
	struct ClothSimSettings *sim_parms;   /* definition is in DNA_cloth_types.h */
	struct ClothCollSettings *coll_parms; /* definition is in DNA_cloth_types.h */
	struct PointCache *point_cache;       /* definition is in DNA_object_force.h */
	struct ListBase ptcaches;
} ClothModifierData;

typedef struct CollisionModifierData {
	ModifierData modifier;

	struct MVert *x;            /* position at the beginning of the frame */
	struct MVert *xnew;         /* position at the end of the frame */
	struct MVert *xold;         /* unused atm, but was discussed during sprint */
	struct MVert *current_xnew; /* new position at the actual inter-frame step */
	struct MVert *current_x;    /* position at the actual inter-frame step */
	struct MVert *current_v;    /* (xnew - x) at the actual inter-frame step */

	struct MFace *mfaces;       /* object face data */

	unsigned int numverts;
	unsigned int numfaces;
	float time_x, time_xnew;    /* cfra time of modifier */
	struct BVHTree *bvhtree;    /* bounding volume hierarchy for this cloth object */
} CollisionModifierData;

typedef struct SurfaceModifierData {
	ModifierData modifier;

	struct MVert *x; /* old position */
	struct MVert *v; /* velocity */

	struct DerivedMesh *dm;

	struct BVHTreeFromMesh *bvhtree; /* bounding volume hierarchy of the mesh faces */

	int cfra, numverts;
} SurfaceModifierData;

typedef struct BooleanModifierData {
	ModifierData modifier;

	struct Object *object;
	int operation, pad;
} BooleanModifierData;

typedef enum {
	eBooleanModifierOp_Intersect  = 0,
	eBooleanModifierOp_Union      = 1,
	eBooleanModifierOp_Difference = 2,
} BooleanModifierOp;

typedef struct MDefInfluence {
	int vertex;
	float weight;
} MDefInfluence;

typedef struct MDefCell {
	int offset;
	int totinfluence;
} MDefCell;

typedef struct MeshDeformModifierData {
	ModifierData modifier;

	struct Object *object;          /* mesh object */
	char defgrp_name[64];           /* optional vertexgroup name, MAX_VGROUP_NAME */

	short gridsize, flag, pad[2];

	/* result of static binding */
	MDefInfluence *bindinfluences;  /* influences */
	int *bindoffsets;               /* offsets into influences array */
	float *bindcagecos;             /* coordinates that cage was bound with */
	int totvert, totcagevert;       /* total vertices in mesh and cage */

	/* result of dynamic binding */
	MDefCell *dyngrid;              /* grid with dynamic binding cell points */
	MDefInfluence *dyninfluences;   /* dynamic binding vertex influences */
	int *dynverts;                  /* is this vertex bound or not? */
	int dyngridsize;                /* size of the dynamic bind grid */
	int totinfluence;               /* total number of vertex influences */
	float dyncellmin[3];            /* offset of the dynamic bind grid */
	float dyncellwidth;             /* width of dynamic bind cell */
	float bindmat[4][4];            /* matrix of cage at binding time */

	/* deprecated storage */
	float *bindweights;             /* deprecated inefficient storage */
	float *bindcos;                 /* deprecated storage of cage coords */

	/* runtime */
	void (*bindfunc)(struct Scene *scene, struct MeshDeformModifierData *mmd,
	                 float *vertexcos, int totvert, float cagemat[4][4]);
} MeshDeformModifierData;

enum {
	MOD_MDEF_INVERT_VGROUP = (1 << 0),
	MOD_MDEF_DYNAMIC_BIND  = (1 << 1),
};

enum {
	MOD_MDEF_VOLUME   = 0,
	MOD_MDEF_SURFACE  = 1,
};

typedef struct ParticleSystemModifierData {
	ModifierData modifier;

	struct ParticleSystem *psys;
	struct DerivedMesh *dm;
	int totdmvert, totdmedge, totdmface;
	short flag, pad;
} ParticleSystemModifierData;

typedef enum {
	eParticleSystemFlag_Pars         = (1 << 0),
	eParticleSystemFlag_psys_updated = (1 << 1),
	eParticleSystemFlag_file_loaded  = (1 << 2),
} ParticleSystemModifierFlag;

typedef enum {
	eParticleInstanceFlag_Parents   = (1 << 0),
	eParticleInstanceFlag_Children  = (1 << 1),
	eParticleInstanceFlag_Path      = (1 << 2),
	eParticleInstanceFlag_Unborn    = (1 << 3),
	eParticleInstanceFlag_Alive     = (1 << 4),
	eParticleInstanceFlag_Dead      = (1 << 5),
	eParticleInstanceFlag_KeepShape = (1 << 6),
	eParticleInstanceFlag_UseSize   = (1 << 7),
} ParticleInstanceModifierFlag;

typedef struct ParticleInstanceModifierData {
	ModifierData modifier;

	struct Object *ob;
	short psys, flag, axis, pad;
	float position, random_position;
} ParticleInstanceModifierData;

typedef enum {
	eExplodeFlag_CalcFaces = (1 << 0),
	eExplodeFlag_PaSize    = (1 << 1),
	eExplodeFlag_EdgeCut   = (1 << 2),
	eExplodeFlag_Unborn    = (1 << 3),
	eExplodeFlag_Alive     = (1 << 4),
	eExplodeFlag_Dead      = (1 << 5),
} ExplodeModifierFlag;

typedef struct ExplodeModifierData {
	ModifierData modifier;

	int *facepa;
	short flag, vgroup;
	float protect;
	char uvname[64]; /* MAX_CUSTOMDATA_LAYER_NAME */
} ExplodeModifierData;

typedef struct MultiresModifierData {
	ModifierData modifier;

	char lvl, sculptlvl, renderlvl, totlvl;
	char simple, flags, pad[2];
} MultiresModifierData;

typedef enum {
	eMultiresModifierFlag_ControlEdges = (1 << 0),
	eMultiresModifierFlag_PlainUv      = (1 << 1),
} MultiresModifierFlag;

typedef struct FluidsimModifierData {
	ModifierData modifier;

	struct FluidsimSettings *fss;   /* definition is in DNA_object_fluidsim.h */
	struct PointCache *point_cache; /* definition is in DNA_object_force.h */
} FluidsimModifierData;

typedef struct ShrinkwrapModifierData {
	ModifierData modifier;

	struct Object *target;    /* shrink target */
	struct Object *auxTarget; /* additional shrink target */
	char vgroup_name[64];     /* optional vertexgroup name, MAX_VGROUP_NAME */
	float keepDist;           /* distance offset to keep from mesh/projection point */
	short shrinkType;         /* shrink type projection */
	char  shrinkOpts;         /* shrink options */
	char  pad1;
	float projLimit;          /* limit the projection ray cast */
	char  projAxis;           /* axis to project over */

	/* If using projection over vertex normal this controls the level of subsurface that must be done
	 * before getting the vertex coordinates and normal
	 */
	char subsurfLevels;

	char pad[2];
} ShrinkwrapModifierData;

/* Shrinkwrap->shrinkType */
enum {
	MOD_SHRINKWRAP_NEAREST_SURFACE = 0,
	MOD_SHRINKWRAP_PROJECT         = 1,
	MOD_SHRINKWRAP_NEAREST_VERTEX  = 2,
};

/* Shrinkwrap->shrinkOpts */
enum {
	/* allow shrinkwrap to move the vertex in the positive direction of axis */
	MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR = (1 << 0),
	/* allow shrinkwrap to move the vertex in the negative direction of axis */
	MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR = (1 << 1),

	/* ignore vertex moves if a vertex ends projected on a front face of the target */
	MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE = (1 << 3),
	/* ignore vertex moves if a vertex ends projected on a back face of the target */
	MOD_SHRINKWRAP_CULL_TARGET_BACKFACE  = (1 << 4),

	MOD_SHRINKWRAP_KEEP_ABOVE_SURFACE    = (1 << 5),  /* distance is measure to the front face of the target */
};

/* Shrinkwrap->projAxis */
enum {
	MOD_SHRINKWRAP_PROJECT_OVER_NORMAL   = 0,        /* projection over normal is used if no axis is selected */
	MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS   = (1 << 0),
	MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS   = (1 << 1),
	MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS   = (1 << 2),
};


typedef struct SimpleDeformModifierData {
	ModifierData modifier;

	struct Object *origin;  /* object to control the origin of modifier space coordinates */
	char vgroup_name[64];   /* optional vertexgroup name, MAX_VGROUP_NAME */
	float factor;           /* factors to control simple deforms */
	float limit[2];         /* lower and upper limit */

	char mode;              /* deform function */
	char axis;              /* lock axis (for taper and strech) */
	char pad[2];

} SimpleDeformModifierData;

enum {
	MOD_SIMPLEDEFORM_MODE_TWIST   = 1,
	MOD_SIMPLEDEFORM_MODE_BEND    = 2,
	MOD_SIMPLEDEFORM_MODE_TAPER   = 3,
	MOD_SIMPLEDEFORM_MODE_STRETCH = 4,
};

enum {
	MOD_SIMPLEDEFORM_LOCK_AXIS_X = (1 << 0),
	MOD_SIMPLEDEFORM_LOCK_AXIS_Y = (1 << 1),
};

typedef struct ShapeKeyModifierData {
	ModifierData modifier;
} ShapeKeyModifierData;

typedef struct SolidifyModifierData {
	ModifierData modifier;

	char defgrp_name[64];   /* name of vertex group to use, MAX_VGROUP_NAME */
	float offset;           /* new surface offset level*/
	float offset_fac;       /* midpoint of the offset  */
	/* factor for the minimum weight to use when vgroups are used, avoids 0.0 weights giving duplicate geometry */
	float offset_fac_vg;
	float offset_clamp;     /* clamp offset based on surrounding geometry */
	float pad;
	float crease_inner;
	float crease_outer;
	float crease_rim;
	int flag;
	short mat_ofs;
	short mat_ofs_rim;
} SolidifyModifierData;

enum {
	MOD_SOLIDIFY_RIM            = (1 << 0),
	MOD_SOLIDIFY_EVEN           = (1 << 1),
	MOD_SOLIDIFY_NORMAL_CALC    = (1 << 2),
	MOD_SOLIDIFY_VGROUP_INV     = (1 << 3),
	MOD_SOLIDIFY_RIM_MATERIAL   = (1 << 4),  /* deprecated, used in do_versions */
	MOD_SOLIDIFY_FLIP           = (1 << 5),
};

#if (DNA_DEPRECATED_GCC_POISON == 1)
#pragma GCC poison MOD_SOLIDIFY_RIM_MATERIAL
#endif

typedef struct ScrewModifierData {
	ModifierData modifier;

	struct Object *ob_axis;
	unsigned int steps;
	unsigned int render_steps;
	unsigned int iter;
	float screw_ofs;
	float angle;
	char axis;
	char pad;
	short flag;
} ScrewModifierData;

enum {
	MOD_SCREW_NORMAL_FLIP    = (1 << 0),
	MOD_SCREW_NORMAL_CALC    = (1 << 1),
	MOD_SCREW_OBJECT_OFFSET  = (1 << 2),
/*	MOD_SCREW_OBJECT_ANGLE   = (1 << 4), */
	MOD_SCREW_SMOOTH_SHADING = (1 << 5),
	MOD_SCREW_UV_STRETCH_U   = (1 << 6),
	MOD_SCREW_UV_STRETCH_V   = (1 << 7),
};

typedef struct OceanModifierData {
	ModifierData modifier;

	struct Ocean *ocean;
	struct OceanCache *oceancache;
	
	int resolution;
	int spatial_size;

	float wind_velocity;

	float damp;
	float smallest_wave;
	float depth;

	float wave_alignment;
	float wave_direction;
	float wave_scale;

	float chop_amount;
	float foam_coverage;
	float time;

	int bakestart;
	int bakeend;

	char cachepath[1024];    /* FILE_MAX */
	char foamlayername[64];  /* MAX_CUSTOMDATA_LAYER_NAME */
	char cached;
	char geometry_mode;

	char flag;
	char refresh;

	short repeat_x;
	short repeat_y;

	int seed;

	float size;

	float foam_fade;

	int pad;
} OceanModifierData;

enum {
	MOD_OCEAN_GEOM_GENERATE = 0,
	MOD_OCEAN_GEOM_DISPLACE = 1,
	MOD_OCEAN_GEOM_SIM_ONLY = 2,
};

enum {
	MOD_OCEAN_REFRESH_RESET        = (1 << 0),
	MOD_OCEAN_REFRESH_SIM          = (1 << 1),
	MOD_OCEAN_REFRESH_ADD          = (1 << 2),
	MOD_OCEAN_REFRESH_CLEAR_CACHE  = (1 << 3),
	MOD_OCEAN_REFRESH_TOPOLOGY     = (1 << 4),
};

enum {
	MOD_OCEAN_GENERATE_FOAM     = (1 << 0),
	MOD_OCEAN_GENERATE_NORMALS  = (1 << 1),
};

typedef struct WarpModifierData {
	ModifierData modifier;
	/* keep in sync with MappingInfoModifierData */
	struct Tex *texture;
	struct Object *map_object;
	char uvlayer_name[64];  /* MAX_CUSTOMDATA_LAYER_NAME */
	int uvlayer_tmp;
	int texmapping;
	/* end MappingInfoModifierData */

	struct Object *object_from;
	struct Object *object_to;
	struct CurveMapping *curfalloff;
	char defgrp_name[64];  /* optional vertexgroup name, MAX_VGROUP_NAME */
	float strength;
	float falloff_radius;
	char flag; /* not used yet */
	char falloff_type;
	char pad[6];
} WarpModifierData;

#define MOD_WARP_VOLUME_PRESERVE 1

typedef enum {
	eWarp_Falloff_None   = 0,
	eWarp_Falloff_Curve  = 1,
	eWarp_Falloff_Sharp  = 2, /* PROP_SHARP */
	eWarp_Falloff_Smooth = 3, /* PROP_SMOOTH */
	eWarp_Falloff_Root   = 4, /* PROP_ROOT */
	eWarp_Falloff_Linear = 5, /* PROP_LIN */
	eWarp_Falloff_Const  = 6, /* PROP_CONST */
	eWarp_Falloff_Sphere = 7, /* PROP_SPHERE */
	/* PROP_RANDOM not used */
} WarpModifierFalloff;

typedef struct WeightVGEditModifierData {
	ModifierData modifier;

	char defgrp_name[64]; /* Name of vertex group to edit. MAX_VGROUP_NAME. */

	short edit_flags;     /* Using MOD_WVG_EDIT_* flags. */
	short falloff_type;   /* Using MOD_WVG_MAPPING_* defines. */
	float default_weight; /* Weight for vertices not in vgroup. */

	/* Mapping stuff. */
	struct CurveMapping *cmap_curve;  /* The custom mapping curve! */

	/* The add/remove vertices weight thresholds. */
	float add_threshold, rem_threshold;

	/* Masking options. */
	float mask_constant;        /* The global "influence", if no vgroup nor tex is used as mask. */
	char mask_defgrp_name[64];  /* Name of mask vertex group from which to get weight factors. MAX_VGROUP_NAME */

	/* Texture masking. */
	int mask_tex_use_channel;          /* Which channel to use as weightf. */
	struct Tex *mask_texture;          /* The texture. */
	struct Object *mask_tex_map_obj;   /* Name of the map object. */
	int mask_tex_mapping;              /* How to map the texture (using MOD_DISP_MAP_* enums). */
	char mask_tex_uvlayer_name[64];    /* Name of the UV map. MAX_CUSTOMDATA_LAYER_NAME */

	/* Padding... */
	int pad_i1;
} WeightVGEditModifierData;

/* WeightVGEdit flags. */
enum {
	/* (1 << 0), (1 << 1) and (1 << 2) are free for future use! */
	MOD_WVG_EDIT_ADD2VG  = (1 << 3),  /* Add vertices with higher weight than threshold to vgroup. */
	MOD_WVG_EDIT_REMFVG  = (1 << 4),  /* Remove vertices with lower weight than threshold from vgroup. */
};

typedef struct WeightVGMixModifierData {
	ModifierData modifier;

	char defgrp_name_a[64];    /* Name of vertex group to modify/weight. MAX_VGROUP_NAME. */
	char defgrp_name_b[64];    /* Name of other vertex group to mix in. MAX_VGROUP_NAME. */
	float default_weight_a;    /* Default weight value for first vgroup. */
	float default_weight_b;    /* Default weight value to mix in. */
	char mix_mode;             /* How second vgroups weights affect first ones */
	char mix_set;              /* What vertices to affect. */

	char pad_c1[6];

	/* Masking options. */
	float mask_constant;        /* The global "influence", if no vgroup nor tex is used as mask. */
	char mask_defgrp_name[64];  /* Name of mask vertex group from which to get weight factors. MAX_VGROUP_NAME */

	/* Texture masking. */
	int mask_tex_use_channel;         /* Which channel to use as weightf. */
	struct Tex *mask_texture;         /* The texture. */
	struct Object *mask_tex_map_obj;  /* Name of the map object. */
	int mask_tex_mapping;             /* How to map the texture! */
	char mask_tex_uvlayer_name[64];   /* Name of the UV map. MAX_CUSTOMDATA_LAYER_NAME. */

	/* Padding... */
	int pad_i1;
} WeightVGMixModifierData;

/* How second vgroup's weights affect first ones. */
enum {
	MOD_WVG_MIX_SET = 1,  /* Second weights replace weights. */
	MOD_WVG_MIX_ADD = 2,  /* Second weights are added to weights. */
	MOD_WVG_MIX_SUB = 3,  /* Second weights are subtracted from weights. */
	MOD_WVG_MIX_MUL = 4,  /* Second weights are multiplied with weights. */
	MOD_WVG_MIX_DIV = 5,  /* Second weights divide weights. */
	MOD_WVG_MIX_DIF = 6,  /* Difference between second weights and weights. */
	MOD_WVG_MIX_AVG = 7,  /* Average of both weights. */
};

/* What vertices to affect. */
enum {
	MOD_WVG_SET_ALL = 1,  /* Affect all vertices. */
	MOD_WVG_SET_A   = 2,  /* Affect only vertices in first vgroup. */
	MOD_WVG_SET_B   = 3,  /* Affect only vertices in second vgroup. */
	MOD_WVG_SET_OR  = 4,  /* Affect only vertices in one vgroup or the other. */
	MOD_WVG_SET_AND = 5,  /* Affect only vertices in both vgroups. */
};

typedef struct WeightVGProximityModifierData {
	ModifierData modifier;

	char defgrp_name[64];      /* Name of vertex group to modify/weight. MAX_VGROUP_NAME. */

	/* Proximity modes. */
	int proximity_mode;
	int proximity_flags;

	/* Target object from which to calculate vertices distances. */
	struct Object *proximity_ob_target;

	/* Masking options. */
	float mask_constant;        /* The global "influence", if no vgroup nor tex is used as mask. */
	char mask_defgrp_name[64];  /* Name of mask vertex group from which to get weight factors. MAX_VGROUP_NAME */

	/* Texture masking. */
	int mask_tex_use_channel;        /* Which channel to use as weightf. */
	struct Tex *mask_texture;        /* The texture. */
	struct Object *mask_tex_map_obj; /* Name of the map object. */
	int mask_tex_mapping;            /* How to map the texture! */
	char mask_tex_uvlayer_name[64];  /* Name of the UV Map. MAX_CUSTOMDATA_LAYER_NAME. */

	float min_dist, max_dist;        /* Distances mapping to 0.0/1.0 weights. */

	/* Put here to avoid breaking existing struct... */
	short falloff_type;              /* Using MOD_WVG_MAPPING_* enums. */

	/* Padding... */
	short pad_s1;
} WeightVGProximityModifierData;

/* Modes of proximity weighting. */
enum {
	MOD_WVG_PROXIMITY_OBJECT    = 1,  /* source vertex to other location */
	MOD_WVG_PROXIMITY_GEOMETRY  = 2,  /* source vertex to other geometry */
};

/* Flags options for proximity weighting. */
enum {
	/* Use nearest vertices of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
	MOD_WVG_PROXIMITY_GEOM_VERTS  = (1 << 0),
	/* Use nearest edges of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
	MOD_WVG_PROXIMITY_GEOM_EDGES  = (1 << 1),
	/* Use nearest faces of target obj, in MOD_WVG_PROXIMITY_GEOMETRY mode. */
	MOD_WVG_PROXIMITY_GEOM_FACES  = (1 << 2),
};

/* Defines common to all WeightVG modifiers. */
/* Mapping modes. */
enum {
	MOD_WVG_MAPPING_NONE    = 0,
	MOD_WVG_MAPPING_CURVE   = 1,
	MOD_WVG_MAPPING_SHARP   = 2,  /* PROP_SHARP */
	MOD_WVG_MAPPING_SMOOTH  = 3,  /* PROP_SMOOTH */
	MOD_WVG_MAPPING_ROOT    = 4,  /* PROP_ROOT */
	/* PROP_LIN not used (same as NONE, here...). */
	/* PROP_CONST not used. */
	MOD_WVG_MAPPING_SPHERE  = 7,  /* PROP_SPHERE */
	MOD_WVG_MAPPING_RANDOM  = 8,  /* PROP_RANDOM */
	MOD_WVG_MAPPING_STEP    = 9,  /* Median Step. */
};

/* Tex channel to be used as mask. */
enum {
	MOD_WVG_MASK_TEX_USE_INT    = 1,
	MOD_WVG_MASK_TEX_USE_RED    = 2,
	MOD_WVG_MASK_TEX_USE_GREEN  = 3,
	MOD_WVG_MASK_TEX_USE_BLUE   = 4,
	MOD_WVG_MASK_TEX_USE_HUE    = 5,
	MOD_WVG_MASK_TEX_USE_SAT    = 6,
	MOD_WVG_MASK_TEX_USE_VAL    = 7,
	MOD_WVG_MASK_TEX_USE_ALPHA  = 8,
};

typedef struct DynamicPaintModifierData {
	ModifierData modifier;

	struct DynamicPaintCanvasSettings *canvas;
	struct DynamicPaintBrushSettings *brush;
	int type;  /* ui display: canvas / brush */
	int pad;
} DynamicPaintModifierData;

/* Dynamic paint modifier flags */
enum {
	MOD_DYNAMICPAINT_TYPE_CANVAS  = (1 << 0),
	MOD_DYNAMICPAINT_TYPE_BRUSH   = (1 << 1),
};

/* Remesh modifier */
typedef enum RemeshModifierFlags {
	MOD_REMESH_FLOOD_FILL     = 1,
	MOD_REMESH_SMOOTH_SHADING = 2,
} RemeshModifierFlags;

typedef enum RemeshModifierMode {
	/* blocky */
	MOD_REMESH_CENTROID       = 0,
	/* smooth */
	MOD_REMESH_MASS_POINT     = 1,
	/* keeps sharp edges */
	MOD_REMESH_SHARP_FEATURES = 2,
} RemeshModifierMode;

typedef struct RemeshModifierData {
	ModifierData modifier;

	/* floodfill option, controls how small components can be before they are removed */
	float threshold;

	/* ratio between size of model and grid */
	float scale;

	float hermite_num;

	/* octree depth */
	char depth;

	char flag;
	char mode;
	char pad;
} RemeshModifierData;

/* Skin modifier */
typedef struct SkinModifierData {
	ModifierData modifier;

	float branch_smoothing;

	char flag;

	char symmetry_axes;

	char pad[2];
} SkinModifierData;

/* SkinModifierData.symmetry_axes */
enum {
	MOD_SKIN_SYMM_X = (1 << 0),
	MOD_SKIN_SYMM_Y = (1 << 1),
	MOD_SKIN_SYMM_Z = (1 << 2),
};

/* SkinModifierData.flag */
enum {
	MOD_SKIN_SMOOTH_SHADING = 1,
};

/* Triangulate modifier */
typedef struct TriangulateModifierData {
	ModifierData modifier;

	int flag;
	int quad_method;
	int ngon_method;
	int pad;
} TriangulateModifierData;

enum {
	MOD_TRIANGULATE_BEAUTY = (1 << 0), /* deprecated */
};

#if (DNA_DEPRECATED_GCC_POISON == 1)
#pragma GCC poison MOD_TRIANGULATE_BEAUTY
#endif

/* Triangulate methods - NGons */
enum {
	MOD_TRIANGULATE_NGON_BEAUTY = 0,
	MOD_TRIANGULATE_NGON_SCANFILL,
};

/* Triangulate methods - Quads */
enum {
	MOD_TRIANGULATE_QUAD_BEAUTY = 0,
	MOD_TRIANGULATE_QUAD_FIXED,
	MOD_TRIANGULATE_QUAD_ALTERNATE,
	MOD_TRIANGULATE_QUAD_SHORTEDGE
};

typedef struct LaplacianSmoothModifierData {
	ModifierData modifier;

	float lambda, lambda_border, pad1;
	char defgrp_name[64];  /* MAX_VGROUP_NAME */
	short flag, repeat;
} LaplacianSmoothModifierData;

/* Smooth modifier flags */
enum {
	MOD_LAPLACIANSMOOTH_X               = (1 << 1),
	MOD_LAPLACIANSMOOTH_Y               = (1 << 2),
	MOD_LAPLACIANSMOOTH_Z               = (1 << 3),
	MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME = (1 << 4),
	MOD_LAPLACIANSMOOTH_NORMALIZED      = (1 << 5),
};

typedef struct UVWarpModifierData {
	ModifierData modifier;

	char axis_u, axis_v;
	char pad[6];
	float center[2];            /* used for rotate/scale */

	struct Object *object_src;  /* source */
	char bone_src[64];          /* optional name of bone target, MAX_ID_NAME-2 */
	struct Object *object_dst;  /* target */
	char bone_dst[64];          /* optional name of bone target, MAX_ID_NAME-2 */

	char vgroup_name[64];       /* optional vertexgroup name, MAX_VGROUP_NAME */
	char uvlayer_name[64];      /* MAX_CUSTOMDATA_LAYER_NAME */
} UVWarpModifierData;

/* cache modifier */
typedef struct MeshCacheModifierData {
	ModifierData modifier;

	char flag;
	char type;  /* file format */
	char time_mode;
	char play_mode;

	/* axis conversion */
	char forward_axis;
	char up_axis;
	char flip_axis;

	char interp;

	float factor;
	char deform_mode;
	char pad[7];

	/* play_mode == MOD_MESHCACHE_PLAY_CFEA */
	float frame_start;
	float frame_scale;

	/* play_mode == MOD_MESHCACHE_PLAY_EVAL */
	/* we could use one float for all these but their purpose is very different */
	float eval_frame;
	float eval_time;
	float eval_factor;

	char filepath[1024];  /* FILE_MAX */
} MeshCacheModifierData;

enum {
	MOD_MESHCACHE_TYPE_MDD  = 1,
	MOD_MESHCACHE_TYPE_PC2  = 2,
};

enum {
	MOD_MESHCACHE_DEFORM_OVERWRITE  = 0,
	MOD_MESHCACHE_DEFORM_INTEGRATE  = 1,
};

enum {
	MOD_MESHCACHE_INTERP_NONE      = 0,
	MOD_MESHCACHE_INTERP_LINEAR    = 1,
/*	MOD_MESHCACHE_INTERP_CARDINAL  = 2, */
};

enum {
	MOD_MESHCACHE_TIME_FRAME   = 0,
	MOD_MESHCACHE_TIME_SECONDS = 1,
	MOD_MESHCACHE_TIME_FACTOR  = 2,
};

enum {
	MOD_MESHCACHE_PLAY_CFEA = 0,
	MOD_MESHCACHE_PLAY_EVAL = 1,
};


typedef struct LaplacianDeformModifierData {
	ModifierData modifier;
	char anchor_grp_name[64];  /* MAX_VGROUP_NAME */
	int total_verts, repeat;
	float *vertexco;
	void *cache_system;  /* runtime only */
	short flag, pad[3];

} LaplacianDeformModifierData;

/* Smooth modifier flags */
enum {
	MOD_LAPLACIANDEFORM_BIND = 1,
};

/* many of these options match 'solidify' */
typedef struct WireframeModifierData {
	ModifierData modifier;
	char defgrp_name[64];  /* MAX_VGROUP_NAME */
	float offset;
	float offset_fac;
	float offset_fac_vg;
	float crease_weight;
	short flag, mat_ofs;
	short pad[2];
} WireframeModifierData;

enum {
	MOD_WIREFRAME_INVERT_VGROUP = (1 << 0),
	MOD_WIREFRAME_REPLACE       = (1 << 1),
	MOD_WIREFRAME_BOUNDARY      = (1 << 2),
	MOD_WIREFRAME_OFS_EVEN      = (1 << 3),
	MOD_WIREFRAME_OFS_RELATIVE  = (1 << 4),
	MOD_WIREFRAME_CREASE        = (1 << 5),
};



#endif  /* __DNA_MODIFIER_TYPES_H__ */
