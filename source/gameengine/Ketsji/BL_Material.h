
/** \file BL_Material.h
 *  \ingroup ketsji
 */

#ifndef __BL_MATERIAL_H__
#define __BL_MATERIAL_H__

#include "STR_String.h"
#include "MT_Point2.h"
#include "DNA_meshdata_types.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

// --
struct MTex;
struct Material;
struct Image;
struct MTFace;
struct MTex;
struct Material;
struct EnvMap;
// --

/** max units
 * this will default to users available units
 * to build with more available, just increment this value
 * although the more you add the slower the search time will be.
 * we will go for eight, which should be enough
 */
#define MAXTEX			8	//match in RAS_TexVert & RAS_OpenGLRasterizer

// different mapping modes
class BL_Mapping
{
public:
	int mapping;
	float scale[3];
	float offsets[3];
	int projplane[3];
	STR_String objconame;
	STR_String uvCoName;
};

// base material struct
class BL_Material
{
private:
	int num_users;
	bool share;

public:
	// -----------------------------------
	BL_Material();
	void Initialize();

	int IdMode;
	unsigned int ras_mode;
	bool glslmat;

	STR_String texname[MAXTEX];
	unsigned int flag[MAXTEX];
	int tile,tilexrep[MAXTEX],tileyrep[MAXTEX];
	STR_String matname;
	STR_String mtexname[MAXTEX];
	int materialindex;

	float matcolor[4];
	float speccolor[3];
	short alphablend, pad;

	float hard, spec_f;
	float alpha, emit, color_blend[MAXTEX], ref;
	float amb;

	int blend_mode[MAXTEX];

	int num_enabled;
	
	BL_Mapping	mapping[MAXTEX];
	STR_String	imageId[MAXTEX];


	Material*			material;
	MTFace				tface; /* copy of the derived meshes tface */
	Image*				img[MAXTEX];
	EnvMap*				cubemap[MAXTEX];

	unsigned int rgb[4];

	void SetSharedMaterial(bool v);
	bool IsShared();
	void SetUsers(int num);
	
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BL_Material")
#endif
};

// BL_Material::IdMode
enum BL_IdMode {
	DEFAULT_BLENDER=-1,
	TEXFACE,
	ONETEX,
	TWOTEX,
	GREATERTHAN2
};

// BL_Material::blend_mode[index]
enum BL_BlendMode
{
	BLEND_MIX=1,
	BLEND_ADD,
	BLEND_SUB,
	BLEND_MUL,
	BLEND_SCR
};

// -------------------------------------
// BL_Material::flag[index]
enum BL_flag
{
	MIPMAP=1,		// set to use mipmaps
	CALCALPHA=2,	// additive
	USEALPHA=4,		// use actual alpha channel
	TEXALPHA=8,		// use alpha combiner functions
	TEXNEG=16,		// negate blending
	/*HASIPO=32,*/	// unused, commeted for now.
	USENEGALPHA=64
};

// BL_Material::ras_mode
enum BL_ras_mode
{
	// POLY_VIS=1,
	COLLIDER=2,
	ZSORT=4,
	ALPHA=8,
	// TRIANGLE=16,
	USE_LIGHT=32,
	WIRE=64,
	CAST_SHADOW=128,
	TEX=256,
	TWOSIDED=512,
	ONLY_SHADOW=1024,
};

// -------------------------------------
// BL_Material::mapping[index]::mapping
enum BL_MappingFlag
{
	USEENV	=1,
	// --
	USEREFL	=2,
	USEOBJ	=4,
	USENORM	=8,
	USEORCO =16,
	USEUV	=32,
	USETANG	=64,
	DISABLE =128,
	USECUSTOMUV=256
};

// BL_Material::BL_Mapping::projplane
enum BL_MappingProj
{
	PROJN=0,
	PROJX,
	PROJY,
	PROJZ
};

// ------------------------------------
//extern void initBL_Material(BL_Material* mat);
extern MTex* getMTexFromMaterial(Material *mat, int index);
// ------------------------------------

#endif


