/* ipo.c
 * 
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): 2008,2009 Joshua Leung (IPO System cleanup, Animation System Recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* NOTE:
 *
 * This file is no longer used to provide tools for the depreceated IPO system. Instead, it
 * is only used to house the conversion code to the new system.
 *
 * -- Joshua Leung, Jan 2009
 */
 
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_dynstr.h"

#include "BKE_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_constraint.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_nla.h"
#include "BKE_object.h"



/* *************************************************** */
/* Old-Data Freeing Tools */

/* Free data from old IPO-Blocks (those which haven't been converted), but not IPO block itself */
// XXX this shouldn't be necessary anymore, but may occur while not all data is converted yet
void free_ipo (Ipo *ipo)
{
	IpoCurve *icu, *icn;
	int n= 0;
	
	for (icu= ipo->curve.first; icu; icu= icn) {
		icn= icu->next;
		n++;
		
		if (icu->bezt) MEM_freeN(icu->bezt);
		if (icu->bp) MEM_freeN(icu->bp);
		if (icu->driver) MEM_freeN(icu->driver);
		
		BLI_freelinkN(&ipo->curve, icu);
	}
	
	if (G.f & G_DEBUG)
		printf("Freed %d (Unconverted) Ipo-Curves from IPO '%s' \n", n, ipo->id.name+2);
}

/* *************************************************** */
/* ADRCODE to RNA-Path Conversion Code  - Special (Bitflags) */

/* Mapping Table for bitflag <-> RNA path */
typedef struct AdrBit2Path {
	int bit;
	char *path;
	int array_index;
} AdrBit2Path;

/* ----------------- */
/* Mapping Tables to use bits <-> RNA paths */

/* Object layers */
static AdrBit2Path ob_layer_bits[]= {
	{(1<<0), "layer", 0},
	{(1<<1), "layer", 1},
	{(1<<2), "layer", 2},
	{(1<<3), "layer", 3},
	{(1<<4), "layer", 4},
	{(1<<5), "layer", 5},
	{(1<<6), "layer", 6},
	{(1<<7), "layer", 7},
	{(1<<8), "layer", 8},
	{(1<<9), "layer", 9},
	{(1<<10), "layer", 10},
	{(1<<11), "layer", 11},
	{(1<<12), "layer", 12},
	{(1<<13), "layer", 13},
	{(1<<14), "layer", 14},
	{(1<<15), "layer", 15},
	{(1<<16), "layer", 16},
	{(1<<17), "layer", 17},
	{(1<<18), "layer", 18},
	{(1<<19), "layer", 19},
	{(1<<20), "layer", 20}
};

/* Material mode */
static AdrBit2Path ma_mode_bits[]= {
//	{MA_TRACEBLE, "traceable", 0},
// 	{MA_SHADOW, "shadow", 0},
//	{MA_SHLESS, "shadeless", 0},
// 	...
	{MA_RAYTRANSP, "transparency", 0},
	{MA_RAYMIRROR, "raytrace_mirror.enabled", 0},
//	{MA_HALO, "type", MA_TYPE_HALO}
};

/* ----------------- */

/* quick macro for returning the appropriate array for adrcode_bitmaps_to_paths() */
#define RET_ABP(items) \
	{ \
		*tot= sizeof(items)/sizeof(AdrBit2Path); \
		return items; \
	}

/* This function checks if a Blocktype+Adrcode combo, returning a mapping table */
static AdrBit2Path *adrcode_bitmaps_to_paths (int blocktype, int adrcode, int *tot)
{
	/* Object layers */
	if ((blocktype == ID_OB) && (adrcode == OB_LAY)) 
		RET_ABP(ob_layer_bits)
	else if ((blocktype == ID_MA) && (adrcode == MA_MODE))
		RET_ABP(ma_mode_bits)
	// XXX TODO: add other types...
	
	/* Normal curve */
	return NULL;
}

/* *************************************************** */
/* ADRCODE to RNA-Path Conversion Code  - Standard */

/* Object types */
static char *ob_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case OB_LOC_X:
			*array_index= 0; return "location";
		case OB_LOC_Y:
			*array_index= 1; return "location";
		case OB_LOC_Z:
			*array_index= 2; return "location";
		case OB_DLOC_X:
			*array_index= 0; return "delta_location";
		case OB_DLOC_Y:
			*array_index= 1; return "delta_location";
		case OB_DLOC_Z:
			*array_index= 2; return "delta_location";
		
		case OB_ROT_X:
			*array_index= 0; return "rotation_euler";
		case OB_ROT_Y:
			*array_index= 1; return "rotation_euler";
		case OB_ROT_Z:
			*array_index= 2; return "rotation_euler";
		case OB_DROT_X:
			*array_index= 0; return "delta_rotation_euler";
		case OB_DROT_Y:
			*array_index= 1; return "delta_rotation_euler";
		case OB_DROT_Z:
			*array_index= 2; return "delta_rotation_euler";
			
		case OB_SIZE_X:
			*array_index= 0; return "scale";
		case OB_SIZE_Y:
			*array_index= 1; return "scale";
		case OB_SIZE_Z:
			*array_index= 2; return "scale";
		case OB_DSIZE_X:
			*array_index= 0; return "delta_scale";
		case OB_DSIZE_Y:
			*array_index= 1; return "delta_scale";
		case OB_DSIZE_Z:
			*array_index= 2; return "delta_scale";
		case OB_COL_R:
			*array_index= 0; return "color";
		case OB_COL_G:
			*array_index= 1; return "color";
		case OB_COL_B:
			*array_index= 2; return "color";
		case OB_COL_A:
			*array_index= 3; return "color";
#if 0
		case OB_PD_FSTR:
			if (ob->pd) poin= &(ob->pd->f_strength);
			break;
		case OB_PD_FFALL:
			if (ob->pd) poin= &(ob->pd->f_power);
			break;
		case OB_PD_SDAMP:
			if (ob->pd) poin= &(ob->pd->pdef_damp);
			break;
		case OB_PD_RDAMP:
			if (ob->pd) poin= &(ob->pd->pdef_rdamp);
			break;
		case OB_PD_PERM:
			if (ob->pd) poin= &(ob->pd->pdef_perm);
			break;
		case OB_PD_FMAXD:
			if (ob->pd) poin= &(ob->pd->maxdist);
			break;
#endif
	}
	
	return NULL;
}

/* PoseChannel types 
 * NOTE: pchan name comes from 'actname' added earlier... 
 */
static char *pchan_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case AC_QUAT_W:
			*array_index= 0; return "rotation_quaternion";
		case AC_QUAT_X:
			*array_index= 1; return "rotation_quaternion";
		case AC_QUAT_Y:
			*array_index= 2; return "rotation_quaternion";
		case AC_QUAT_Z:
			*array_index= 3; return "rotation_quaternion";
			
		case AC_EUL_X:
			*array_index= 0; return "rotation_euler";
		case AC_EUL_Y:
			*array_index= 1; return "rotation_euler";
		case AC_EUL_Z:
			*array_index= 2; return "rotation_euler";
		
		case AC_LOC_X:
			*array_index= 0; return "location";
		case AC_LOC_Y:
			*array_index= 1; return "location";
		case AC_LOC_Z:
			*array_index= 2; return "location";
		
		case AC_SIZE_X:
			*array_index= 0; return "scale";
		case AC_SIZE_Y:
			*array_index= 1; return "scale";
		case AC_SIZE_Z:
			*array_index= 2; return "scale";
	}
	
	/* for debugging only */
	printf("ERROR: unmatched PoseChannel setting (code %d) \n", adrcode);
	return NULL;
}

/* Constraint types */
static char *constraint_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case CO_ENFORCE:
			return "influence";
		case CO_HEADTAIL:	// XXX this needs to be wrapped in RNA.. probably then this path will be invalid
			return "data.head_tail";
	}
	
	return NULL;
}

/* ShapeKey types 
 * NOTE: as we don't have access to the keyblock where the data comes from (for now), 
 *	 	we'll just use numerical indicies for now... 
 */
static char *shapekey_adrcodes_to_paths (int adrcode, int *array_index)
{
	static char buf[128];
	
	/* block will be attached to ID_KE block, and setting that we alter is the 'value' (which sets keyblock.curval) */
	// XXX adrcode 0 was dummy 'speed' curve 
	if (adrcode == 0) 
		sprintf(buf, "speed");
	else
		sprintf(buf, "keys[%d].value", adrcode);
	return buf;
}

/* MTex (Texture Slot) types */
static char *mtex_adrcodes_to_paths (int adrcode, int *array_index)
{
	char *base=NULL, *prop=NULL;
	static char buf[128];
	
	/* base part of path */
	if (adrcode & MA_MAP1) base= "textures[0]";
	else if (adrcode & MA_MAP2) base= "textures[1]";
	else if (adrcode & MA_MAP3) base= "textures[2]";
	else if (adrcode & MA_MAP4) base= "textures[3]";
	else if (adrcode & MA_MAP5) base= "textures[4]";
	else if (adrcode & MA_MAP6) base= "textures[5]";
	else if (adrcode & MA_MAP7) base= "textures[6]";
	else if (adrcode & MA_MAP8) base= "textures[7]";
	else if (adrcode & MA_MAP9) base= "textures[8]";
	else if (adrcode & MA_MAP10) base= "textures[9]";
	else if (adrcode & MA_MAP11) base= "textures[10]";
	else if (adrcode & MA_MAP12) base= "textures[11]";
	else if (adrcode & MA_MAP13) base= "textures[12]";
	else if (adrcode & MA_MAP14) base= "textures[13]";
	else if (adrcode & MA_MAP15) base= "textures[14]";
	else if (adrcode & MA_MAP16) base= "textures[15]";
	else if (adrcode & MA_MAP17) base= "textures[16]";
	else if (adrcode & MA_MAP18) base= "textures[17]";
		
	/* property identifier for path */
	adrcode= (adrcode & (MA_MAP1-1));
	switch (adrcode) {
#if 0 // XXX these are not wrapped in RNA yet!
		case MAP_OFS_X:
			poin= &(mtex->ofs[0]); break;
		case MAP_OFS_Y:
			poin= &(mtex->ofs[1]); break;
		case MAP_OFS_Z:
			poin= &(mtex->ofs[2]); break;
		case MAP_SIZE_X:
			poin= &(mtex->size[0]); break;
		case MAP_SIZE_Y:
			poin= &(mtex->size[1]); break;
		case MAP_SIZE_Z:
			poin= &(mtex->size[2]); break;
		case MAP_R:
			poin= &(mtex->r); break;
		case MAP_G:
			poin= &(mtex->g); break;
		case MAP_B:
			poin= &(mtex->b); break;
		case MAP_DVAR:
			poin= &(mtex->def_var); break;
		case MAP_COLF:
			poin= &(mtex->colfac); break;
		case MAP_NORF:
			poin= &(mtex->norfac); break;
		case MAP_VARF:
			poin= &(mtex->varfac); break;
#endif
		case MAP_DISP:
			prop= "warp_factor"; break;
	}
	
	/* only build and return path if there's a property */
	if (prop) {
		BLI_snprintf(buf, 128, "%s.%s", base, prop);
		return buf;
	}
	else
		return NULL;
}

/* Texture types */
static char *texture_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case TE_NSIZE:
			return "noise_size";
		case TE_TURB:
			return "turbulence";
			
		case TE_NDEPTH:	// XXX texture RNA undefined
			//poin= &(tex->noisedepth); *type= IPO_SHORT; break;
			break;
		case TE_NTYPE: // XXX texture RNA undefined
			//poin= &(tex->noisetype); *type= IPO_SHORT; break;
			break;
			
		case TE_N_BAS1:
			return "noise_basis";
		case TE_N_BAS2:
			return "noise_basis"; // XXX this is not yet defined in RNA...
		
			/* voronoi */
		case TE_VNW1:
			*array_index= 0; return "feature_weights";
		case TE_VNW2:
			*array_index= 1; return "feature_weights";
		case TE_VNW3:
			*array_index= 2; return "feature_weights";
		case TE_VNW4:
			*array_index= 3; return "feature_weights";
		case TE_VNMEXP:
			return "minkovsky_exponent";
		case TE_VN_DISTM:
			return "distance_metric";
		case TE_VN_COLT:
			return "color_type";
		
			/* distorted noise / voronoi */
		case TE_ISCA:
			return "noise_intensity";
			
			/* distorted noise */
		case TE_DISTA:
			return "distortion_amount";
		
			/* musgrave */
		case TE_MG_TYP: // XXX texture RNA undefined
		//	poin= &(tex->stype); *type= IPO_SHORT; break;
			break;
		case TE_MGH:
			return "highest_dimension";
		case TE_MG_LAC:
			return "lacunarity";
		case TE_MG_OCT:
			return "octaves";
		case TE_MG_OFF:
			return "offset";
		case TE_MG_GAIN:
			return "gain";
			
		case TE_COL_R:
			*array_index= 0; return "rgb_factor";
		case TE_COL_G:
			*array_index= 1; return "rgb_factor";
		case TE_COL_B:
			*array_index= 2; return "rgb_factor";
			
		case TE_BRIGHT:
			return "brightness";
		case TE_CONTRA:
			return "constrast";
	}
	
	return NULL;
}

/* Material Types */
static char *material_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case MA_COL_R:
			*array_index= 0; return "diffuse_color";
		case MA_COL_G:
			*array_index= 1; return "diffuse_color";
		case MA_COL_B:
			*array_index= 2; return "diffuse_color";
			
		case MA_SPEC_R:
			*array_index= 0; return "specular_color";
		case MA_SPEC_G:
			*array_index= 1; return "specular_color";
		case MA_SPEC_B:
			*array_index= 2; return "specular_color";
			
		case MA_MIR_R:
			*array_index= 0; return "mirror_color";
		case MA_MIR_G:
			*array_index= 1; return "mirror_color";
		case MA_MIR_B:
			*array_index= 2; return "mirror_color";
			
		case MA_ALPHA:
			return "alpha";
			
		case MA_REF:
			return "diffuse_reflection";
		
		case MA_EMIT:
			return "emit";
		
		case MA_AMB:
			return "ambient";
		
		case MA_SPEC:
			return "specular_reflection";
		
		case MA_HARD:
			return "specular_hardness";
			
		case MA_SPTR:
			return "specular_opacity";
			
		case MA_IOR:
			return "ior";
			
		case MA_HASIZE:
			return "halo.size";
			
		case MA_TRANSLU:
			return "translucency";
			
		case MA_RAYM:
			return "raytrace_mirror.reflect";
			
		case MA_FRESMIR:
			return "raytrace_mirror.fresnel";
			
		case MA_FRESMIRI:
			return "raytrace_mirror.fresnel_fac";
			
		case MA_FRESTRA:
			return "raytrace_transparency.fresnel";
			
		case MA_FRESTRAI:
			return "raytrace_transparency.fresnel_fac";
			
		case MA_ADD:
			return "halo.add";
		
		default: /* for now, we assume that the others were MTex channels */
			return mtex_adrcodes_to_paths(adrcode, array_index);
	}
	
	return NULL;	
}

/* Camera Types */
static char *camera_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case CAM_LENS:
#if 0 // XXX this cannot be resolved easily... perhaps we assume camera is perspective (works for most cases...
			if (ca->type == CAM_ORTHO)
				return "ortho_scale";
			else
				return "lens"; 
#else // XXX lazy hack for now...
			return "lens";
#endif // XXX this cannot be resolved easily
			
		case CAM_STA:
			return "clip_start";
		case CAM_END:
			return "clip_end";
			
#if 0 // XXX these are not defined in RNA
		case CAM_YF_APERT:
			poin= &(ca->YF_aperture); break;
		case CAM_YF_FDIST:
			poin= &(ca->YF_dofdist); break;
#endif // XXX these are not defined in RNA
			
		case CAM_SHIFT_X:
			return "shift_x";
		case CAM_SHIFT_Y:
			return "shift_y";
	}
	
	/* unrecognised adrcode, or not-yet-handled ones! */
	return NULL;
}

/* Lamp Types */
static char *lamp_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case LA_ENERGY:
			return "energy";
			
		case LA_COL_R:
			*array_index= 0;  return "color";
		case LA_COL_G:
			*array_index= 1;  return "color";
		case LA_COL_B:
			*array_index= 2;  return "color";
			
		case LA_DIST:
			return "distance";
		
		case LA_SPOTSI:
			return "spot_size";
		case LA_SPOTBL:
			return "spot_blend";
			
		case LA_QUAD1:
			return "linear_attenuation";
		case LA_QUAD2:
			return "quadratic_attenuation";
			
		case LA_HALOINT:
			return "halo_intensity";
			
		default: /* for now, we assume that the others were MTex channels */
			return mtex_adrcodes_to_paths(adrcode, array_index);
	}
	
	/* unrecognised adrcode, or not-yet-handled ones! */
	return NULL;
}

/* Sound Types */
static char *sound_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case SND_VOLUME:
			return "volume";
		case SND_PITCH:
			return "pitch";
	/* XXX Joshua -- I had wrapped panning in rna, but someone commented out, calling it "unused" */
	/*	case SND_PANNING:
			return "panning"; */
		case SND_ATTEN:
			return "attenuation";
	}
	
	/* unrecognised adrcode, or not-yet-handled ones! */
	return NULL;
}

/* World Types */
static char *world_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case WO_HOR_R:
			*array_index= 0; return "horizon_color";
		case WO_HOR_G:
			*array_index= 1; return "horizon_color";
		case WO_HOR_B:
			*array_index= 2; return "horizon_color";
		case WO_ZEN_R:
			*array_index= 0; return "zenith_color";
		case WO_ZEN_G:
			*array_index= 1; return "zenith_color";
		case WO_ZEN_B:
			*array_index= 2; return "zenith_color";
		
		case WO_EXPOS:
			return "exposure";
		
		case WO_MISI:
			return "mist.intensity";
		case WO_MISTDI:
			return "mist.depth";
		case WO_MISTSTA:
			return "mist.start";
		case WO_MISTHI:
			return "mist.height";
		
	/*	Star Color is unused -- recommend removal */
	/*	case WO_STAR_R:
			*array_index= 0; return "stars.color";
		case WO_STAR_G:
			*array_index= 1; return "stars.color";
		case WO_STAR_B:
			*array_index= 2; return "stars.color"; */
		
		case WO_STARDIST:
			return "stars.min_distance";
		case WO_STARSIZE:
			return "stars.size";
		
		default: /* for now, we assume that the others were MTex channels */
			return mtex_adrcodes_to_paths(adrcode, array_index);
		}
		
	return NULL;	
}

/* Particle Types */
static char *particle_adrcodes_to_paths (int adrcode, int *array_index)
{
	/* set array index like this in-case nothing sets it correctly  */
	*array_index= 0;
	
	/* result depends on adrcode */
	switch (adrcode) {
		case PART_CLUMP:
			return "settings.clump_factor";
		case PART_AVE:
			return "settings.angular_velocity_factor";
		case PART_SIZE:
			return "settings.particle_size";
		case PART_DRAG:
			return "settings.drag_factor";
		case PART_BROWN:
			return "settings.brownian_factor";
		case PART_DAMP:
			return "settings.damp_factor";
		case PART_LENGTH:
			return "settings.length";
		case PART_GRAV_X:
			*array_index= 0; return "settings.acceleration";
		case PART_GRAV_Y:
			*array_index= 1; return "settings.acceleration";
		case PART_GRAV_Z:
			*array_index= 2; return "settings.acceleration";
		case PART_KINK_AMP:
			return "settings.kink_amplitude";
		case PART_KINK_FREQ:
			return "settings.kink_frequency";
		case PART_KINK_SHAPE:
			return "settings.kink_shape";
		case PART_BB_TILT:
			return "settings.billboard_tilt";
		
		/* PartDeflect needs to be sorted out properly in rna_object_force;
		   If anyone else works on this, but is unfamiliar, these particular
			settings reference the particles of the system themselves
			being used as forces -- it will use the same rna structure
			as the similar object forces				*/
		/*case PART_PD_FSTR:
			if (part->pd) poin= &(part->pd->f_strength);
			break;
		case PART_PD_FFALL:
			if (part->pd) poin= &(part->pd->f_power);
			break;
		case PART_PD_FMAXD:
			if (part->pd) poin= &(part->pd->maxdist);
			break;
		case PART_PD2_FSTR:
			if (part->pd2) poin= &(part->pd2->f_strength);
			break;
		case PART_PD2_FFALL:
			if (part->pd2) poin= &(part->pd2->f_power);
			break;
		case PART_PD2_FMAXD:
			if (part->pd2) poin= &(part->pd2->maxdist);
			break;*/

		}
		
	return NULL;	
}

/* ------- */

/* Allocate memory for RNA-path for some property given a blocktype, adrcode, and 'root' parts of path
 *	Input:
 *		- blocktype, adrcode	- determines setting to get
 *		- actname, constname	- used to build path
 *	Output:
 *		- array_index			- index in property's array (if applicable) to use
 *		- return				- the allocated path...
 */
static char *get_rna_access (int blocktype, int adrcode, char actname[], char constname[], int *array_index)
{
	DynStr *path= BLI_dynstr_new();
	char *propname=NULL, *rpath=NULL;
	char buf[512];
	int dummy_index= 0;
	
	/* hack: if constname is set, we can only be dealing with an Constraint curve */
	if (constname)
		blocktype= ID_CO;
	
	/* get property name based on blocktype */
	switch (blocktype) {
		case ID_OB: /* object */
			propname= ob_adrcodes_to_paths(adrcode, &dummy_index);
			break;
		
		case ID_PO: /* pose channel */
			propname= pchan_adrcodes_to_paths(adrcode, &dummy_index);
			break;
			
		case ID_KE: /* shapekeys */
			propname= shapekey_adrcodes_to_paths(adrcode, &dummy_index);
			break;
			
		case ID_CO: /* constraint */
			propname= constraint_adrcodes_to_paths(adrcode, &dummy_index);	
			break;
			
		case ID_TE: /* texture */
			propname= texture_adrcodes_to_paths(adrcode, &dummy_index);
			break;
			
		case ID_MA: /* material */
			propname= material_adrcodes_to_paths(adrcode, &dummy_index);
			break;
			
		case ID_CA: /* camera */
			propname= camera_adrcodes_to_paths(adrcode, &dummy_index);
			break;
			
		case ID_LA: /* lamp */
			propname= lamp_adrcodes_to_paths(adrcode, &dummy_index);
			break;
		
		case ID_SO: /* sound */
			propname= sound_adrcodes_to_paths(adrcode, &dummy_index);
			break;
		
		case ID_WO: /* world */
			propname= world_adrcodes_to_paths(adrcode, &dummy_index);
			break;

		case ID_PA: /* particle */
			propname= particle_adrcodes_to_paths(adrcode, &dummy_index);
			break;
			
		/* XXX problematic blocktypes */
		case ID_CU: /* curve */
			/* this used to be a 'dummy' curve which got evaluated on the fly... 
			 * now we've got real var for this!
			 */
			propname= "eval_time";
			break;
			
		case ID_SEQ: /* sequencer strip */
			//SEQ_FAC1:
			switch (adrcode) {
			case SEQ_FAC1:
				propname= "effect_fader";
				break;
			case SEQ_FAC_SPEED:
				propname= "speed_fader";
				break;
			case SEQ_FAC_OPACITY:
				propname= "blend_opacity";
				break;
			}
			//	poin= &(seq->facf0); // XXX this doesn't seem to be included anywhere in sequencer RNA...
			break;
			
		/* special hacks */
		case -1:
			/* special case for rotdiff drivers... we don't need a property for this... */
			break;
			
		// TODO... add other blocktypes...
		default:
			printf("IPO2ANIMATO WARNING: No path for blocktype %d, adrcode %d yet \n", blocktype, adrcode);
			break;
	}
	
	/* check if any property found 
	 *	- blocktype < 0 is special case for a specific type of driver, where we don't need a property name...
	 */
	if ((propname == NULL) && (blocktype > 0)) {
		/* nothing was found, so exit */
		if (array_index) 
			*array_index= 0;
			
		BLI_dynstr_free(path);
		
		return NULL;
	}
	else {
		if (array_index)
			*array_index= dummy_index;
	}
	
	/* append preceeding bits to path */
	if ((actname && actname[0]) && (constname && constname[0])) {
		/* Constraint in Pose-Channel */
		sprintf(buf, "pose.bones[\"%s\"].constraints[\"%s\"]", actname, constname);
	}
	else if (actname && actname[0]) {
		/* Pose-Channel */
		sprintf(buf, "pose.bones[\"%s\"]", actname);
	}
	else if (constname && constname[0]) {
		/* Constraint in Object */
		sprintf(buf, "constraints[\"%s\"]", constname);
	}
	else
		strcpy(buf, ""); /* empty string */
	BLI_dynstr_append(path, buf);
	
	/* need to add dot before property if there was anything precceding this */
	if (buf[0])
		BLI_dynstr_append(path, ".");
	
	/* now write name of property */
	BLI_dynstr_append(path, propname);
	
	/* if there was no array index pointer provided, add it to the path */
	if (array_index == NULL) {
		sprintf(buf, "[\"%d\"]", dummy_index);
		BLI_dynstr_append(path, buf);
	}
	
	/* convert to normal MEM_malloc'd string */
	rpath= BLI_dynstr_get_cstring(path);
	BLI_dynstr_free(path);
	
	/* return path... */
	return rpath;
}

/* *************************************************** */
/* Conversion Utilities */

/* Convert adrcodes to driver target transform channel types */
static short adrcode_to_dtar_transchan (short adrcode)
{
	switch (adrcode) {
		case OB_LOC_X:	
			return DTAR_TRANSCHAN_LOCX;
		case OB_LOC_Y:
			return DTAR_TRANSCHAN_LOCY;
		case OB_LOC_Z:
			return DTAR_TRANSCHAN_LOCZ;
		
		case OB_ROT_X:	
			return DTAR_TRANSCHAN_ROTX;
		case OB_ROT_Y:
			return DTAR_TRANSCHAN_ROTY;
		case OB_ROT_Z:
			return DTAR_TRANSCHAN_ROTZ;
		
		case OB_SIZE_X:	
			return DTAR_TRANSCHAN_SCALEX;
		case OB_SIZE_Y:
			return DTAR_TRANSCHAN_SCALEX;
		case OB_SIZE_Z:
			return DTAR_TRANSCHAN_SCALEX;
			
		default:
			return 0;
	}
}

/* Convert IpoDriver to ChannelDriver - will free the old data (i.e. the old driver) */
static ChannelDriver *idriver_to_cdriver (IpoDriver *idriver)
{
	ChannelDriver *cdriver;
	
	/* allocate memory for new driver */
	cdriver= MEM_callocN(sizeof(ChannelDriver), "ChannelDriver");
	
	/* if 'pydriver', just copy data across */
	if (idriver->type == IPO_DRIVER_TYPE_PYTHON) {
		/* PyDriver only requires the expression to be copied */
		// FIXME: expression will be useless due to API changes, but at least not totally lost
		cdriver->type = DRIVER_TYPE_PYTHON;
		if (idriver->name[0])
			BLI_strncpy(cdriver->expression, idriver->name, sizeof(cdriver->expression));
	}
	else {
		DriverVar *dvar = NULL;
		DriverTarget *dtar = NULL;
		
		/* this should be ok for all types here... */
		cdriver->type= DRIVER_TYPE_AVERAGE;
		
		/* what to store depends on the 'blocktype' - object or posechannel */
		if (idriver->blocktype == ID_AR) { /* PoseChannel */
			if (idriver->adrcode == OB_ROT_DIFF) {
				/* Rotational Difference requires a special type of variable */
				dvar= driver_add_new_variable(cdriver);
				driver_change_variable_type(dvar, DVAR_TYPE_ROT_DIFF);
				
					/* first bone target */
				dtar= &dvar->targets[0];
				dtar->id= (ID *)idriver->ob;
				if (idriver->name[0])
					BLI_strncpy(dtar->pchan_name, idriver->name, 32);
				
					/* second bone target (name was stored in same var as the first one) */
				dtar= &dvar->targets[1];
				dtar->id= (ID *)idriver->ob;
				if (idriver->name[0]) // xxx... for safety
					BLI_strncpy(dtar->pchan_name, idriver->name+DRIVER_NAME_OFFS, 32);
			}
			else {
				/* only a single variable, of type 'transform channel' */
				dvar= driver_add_new_variable(cdriver);
				driver_change_variable_type(dvar, DVAR_TYPE_TRANSFORM_CHAN);
				
				/* only requires a single target */
				dtar= &dvar->targets[0];
				dtar->id= (ID *)idriver->ob;
				if (idriver->name[0])
					BLI_strncpy(dtar->pchan_name, idriver->name, 32);
				dtar->transChan= adrcode_to_dtar_transchan(idriver->adrcode);
				dtar->flag |= DTAR_FLAG_LOCALSPACE; /* old drivers took local space */
			}
		}
		else { /* Object */
			/* only a single variable, of type 'transform channel' */
			dvar= driver_add_new_variable(cdriver);
			driver_change_variable_type(dvar, DVAR_TYPE_TRANSFORM_CHAN);
			
				/* only requires single target */
			dtar= &dvar->targets[0];
			dtar->id= (ID *)idriver->ob;
			dtar->transChan= adrcode_to_dtar_transchan(idriver->adrcode);
		}
	}
	
	/* return the new one */
	return cdriver;
}

/* Add F-Curve to the correct list 
 *	- grpname is needed to be used as group name where relevant, and is usually derived from actname
 */
static void fcurve_add_to_list (ListBase *groups, ListBase *list, FCurve *fcu, char *grpname, int muteipo)
{
	/* If we're adding to an action, we will have groups to write to... */
	if (groups && grpname) {
		/* wrap the pointers given into a dummy action that we pass to the API func
		 * and extract the resultant lists...
		 */
		bAction tmp_act;
		bActionGroup *agrp= NULL;
		
		/* init the temp action */
		//memset(&tmp_act, 0, sizeof(bAction)); // XXX only enable this line if we get errors
		tmp_act.groups.first= groups->first;
		tmp_act.groups.last= groups->last;
		tmp_act.curves.first= list->first;
		tmp_act.curves.last= list->last;
		/* ... xxx, the other vars don't need to be filled in */
		
		/* get the group to use */
		agrp= action_groups_find_named(&tmp_act, grpname);
		if (agrp == NULL) {
			/* no matching group, so add one */
			if (agrp == NULL) {
				/* Add a new group, and make it active */
				agrp= MEM_callocN(sizeof(bActionGroup), "bActionGroup");
				
				agrp->flag = AGRP_SELECTED;
				if(muteipo) agrp->flag |= AGRP_MUTED;

				strncpy(agrp->name, grpname, sizeof(agrp->name));
				
				BLI_addtail(&tmp_act.groups, agrp);
				BLI_uniquename(&tmp_act.groups, agrp, "Group", '.', offsetof(bActionGroup, name), sizeof(agrp->name));
			}
		}
		
		/* add F-Curve to group */
		/* WARNING: this func should only need to look at the stuff we initialised, if not, things may crash */
		action_groups_add_channel(&tmp_act, agrp, fcu);
		
		if(agrp->flag & AGRP_MUTED) /* flush down */
			fcu->flag |= FCURVE_MUTED;

		/* set the output lists based on the ones in the temp action */
		groups->first= tmp_act.groups.first;
		groups->last= tmp_act.groups.last;
		list->first= tmp_act.curves.first;
		list->last= tmp_act.curves.last;
	}
	else {
		/* simply add the F-Curve to the end of the given list */
		BLI_addtail(list, fcu);
	}
}

/* Convert IPO-Curve to F-Curve (including Driver data), and free any of the old data that 
 * is not relevant, BUT do not free the IPO-Curve itself...
 *	actname: name of Action-Channel (if applicable) that IPO-Curve's IPO-block belonged to
 *	constname: name of Constraint-Channel (if applicable) that IPO-Curve's IPO-block belonged to
 */
static void icu_to_fcurves (ListBase *groups, ListBase *list, IpoCurve *icu, char *actname, char *constname, int muteipo)
{
	AdrBit2Path *abp;
	FCurve *fcu;
	int i=0, totbits;
	
	/* allocate memory for a new F-Curve */
	fcu= MEM_callocN(sizeof(FCurve), "FCurve");
	
	/* convert driver */
	if (icu->driver)
		fcu->driver= idriver_to_cdriver(icu->driver);
	
	/* copy flags */
	if (icu->flag & IPO_VISIBLE) fcu->flag |= FCURVE_VISIBLE;
	if (icu->flag & IPO_SELECT) fcu->flag |= FCURVE_SELECTED;
	if (icu->flag & IPO_ACTIVE) fcu->flag |= FCURVE_ACTIVE;
	if (icu->flag & IPO_MUTE) fcu->flag |= FCURVE_MUTED;
	if (icu->flag & IPO_PROTECT) fcu->flag |= FCURVE_PROTECTED;
	if (icu->flag & IPO_AUTO_HORIZ) fcu->flag |= FCURVE_AUTO_HANDLES;
	
	/* set extrapolation */
	switch (icu->extrap) {
		case IPO_HORIZ: /* constant extrapolation */
		case IPO_DIR: /* linear extrapolation */
		{
			/* just copy, as the new defines match the old ones... */
			fcu->extend= icu->extrap;
		}
			break;
			
		case IPO_CYCL: /* cyclic extrapolation */
		case IPO_CYCLX: /* cyclic extrapolation + offset */
		{
			/* Add a new FModifier (Cyclic) instead of setting extend value 
			 * as that's the new equivilant of that option. 
			 */
			FModifier *fcm= add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES);
			FMod_Cycles *data= (FMod_Cycles *)fcm->data;
			
			/* if 'offset' one is in use, set appropriate settings */
			if (icu->extrap == IPO_CYCLX)
				data->before_mode= data->after_mode= FCM_EXTRAPOLATE_CYCLIC_OFFSET;
			else
				data->before_mode= data->after_mode= FCM_EXTRAPOLATE_CYCLIC;
		}
			break;
	}
	
	/* -------- */
	
	/* get adrcode <-> bitflags mapping to handle nasty bitflag curves? */
	abp= adrcode_bitmaps_to_paths(icu->blocktype, icu->adrcode, &totbits);
	if (abp && totbits) {
		FCurve *fcurve;
		int b;
		
		if (G.f & G_DEBUG) printf("\tconvert bitflag ipocurve, totbits = %d \n", totbits);
		
		/* add the 'only int values' flag */
		fcu->flag |= (FCURVE_INT_VALUES|FCURVE_DISCRETE_VALUES);		
		
		/* for each bit we have to remap + check for:
		 * 1) we need to make copy the existing F-Curve data (fcu -> fcurve),
		 * 	  except for the last one which will use the original 
		 * 2) copy the relevant path info across
		 * 3) filter the keyframes for the flag of interest
		 */
		for (b=0; b < totbits; b++, abp++) {
			/* make a copy of existing base-data if not the last curve */
			if (b < (totbits-1))
				fcurve= copy_fcurve(fcu);
			else
				fcurve= fcu;
				
			/* set path */
			fcurve->rna_path= BLI_strdupn(abp->path, strlen(abp->path));
			fcurve->array_index= abp->array_index;
			
			/* convert keyframes 
			 *	- beztriples and bpoints are mutually exclusive, so we won't have both at the same time
			 *	- beztriples are more likely to be encountered as they are keyframes (the other type wasn't used yet)
			 */
			fcurve->totvert= icu->totvert;
			
			if (icu->bezt) {
				BezTriple *dst, *src;
				
				/* allocate new array for keyframes/beztriples */
				fcurve->bezt= MEM_callocN(sizeof(BezTriple)*fcurve->totvert, "BezTriples");
				
				/* loop through copying all BezTriples individually, as we need to modify a few things */
				for (dst=fcurve->bezt, src=icu->bezt; i < fcurve->totvert; i++, dst++, src++) {
					/* firstly, copy BezTriple data */
					*dst= *src;
					
					/* interpolation can only be constant... */
					dst->ipo= BEZT_IPO_CONST;
					
					/* 'hide' flag is now used for keytype - only 'keyframes' existed before */
					dst->hide= BEZT_KEYTYPE_KEYFRAME;
					
					/* correct values, by checking if the flag of interest is set */
					if ( ((int)(dst->vec[1][1])) & (abp->bit) )
						dst->vec[0][1]= dst->vec[1][1]= dst->vec[2][1] = 1.0f;
					else
						dst->vec[0][1]= dst->vec[1][1]= dst->vec[2][1] = 0.0f;
				}
			}
			else if (icu->bp) {
				/* TODO: need to convert from BPoint type to the more compact FPoint type... but not priority, since no data used this */
				//BPoint *bp;
				//FPoint *fpt;
			}
			
			/* add new F-Curve to list */
			fcurve_add_to_list(groups, list, fcurve, actname, muteipo);
		}
	}
	else {
		/* get rna-path
		 *	- we will need to set the 'disabled' flag if no path is able to be made (for now)
		 */
		fcu->rna_path= get_rna_access(icu->blocktype, icu->adrcode, actname, constname, &fcu->array_index);
		if (fcu->rna_path == NULL)
			fcu->flag |= FCURVE_DISABLED;
		
		/* convert keyframes 
		 *	- beztriples and bpoints are mutually exclusive, so we won't have both at the same time
		 *	- beztriples are more likely to be encountered as they are keyframes (the other type wasn't used yet)
		 */
		fcu->totvert= icu->totvert;
		
		if (icu->bezt) {
			BezTriple *dst, *src;
			
			/* allocate new array for keyframes/beztriples */
			fcu->bezt= MEM_callocN(sizeof(BezTriple)*fcu->totvert, "BezTriples");
			
			/* loop through copying all BezTriples individually, as we need to modify a few things */
			for (dst=fcu->bezt, src=icu->bezt; i < fcu->totvert; i++, dst++, src++) {
				/* firstly, copy BezTriple data */
				*dst= *src;
				
				/* now copy interpolation from curve (if not already set) */
				if (icu->ipo != IPO_MIXED)
					dst->ipo= icu->ipo;
					
				/* 'hide' flag is now used for keytype - only 'keyframes' existed before */
				dst->hide= BEZT_KEYTYPE_KEYFRAME;
					
				/* correct values for euler rotation curves 
				 *	- they were degrees/10 
				 *	- we need radians for RNA to do the right thing
				 */
				if ( ((icu->blocktype == ID_OB) && ELEM3(icu->adrcode, OB_ROT_X, OB_ROT_Y, OB_ROT_Z)) ||
					 ((icu->blocktype == ID_PO) && ELEM3(icu->adrcode, AC_EUL_X, AC_EUL_Y, AC_EUL_Z)) )
				{
					const float fac= (float)M_PI / 18.0f; //10.0f * M_PI/180.0f;
					
					dst->vec[0][1] *= fac;
					dst->vec[1][1] *= fac;
					dst->vec[2][1] *= fac;
				}
				
				/* correct times for rotation drivers 
				 *	- need to go from degrees to radians...
				 * 	- there's only really 1 target to worry about 
				 */
				if (fcu->driver && fcu->driver->variables.first) {
					DriverVar *dvar= fcu->driver->variables.first;
					DriverTarget *dtar= &dvar->targets[0];
					
					if (ELEM3(dtar->transChan, DTAR_TRANSCHAN_ROTX, DTAR_TRANSCHAN_ROTY, DTAR_TRANSCHAN_ROTZ)) {
						const float fac= (float)M_PI / 180.0f;
						
						dst->vec[0][0] *= fac;
						dst->vec[1][0] *= fac;
						dst->vec[2][0] *= fac;
					}
				}
			}
		}
		else if (icu->bp) {
			/* TODO: need to convert from BPoint type to the more compact FPoint type... but not priority, since no data used this */
			//BPoint *bp;
			//FPoint *fpt;
		}
		
		/* add new F-Curve to list */
		fcurve_add_to_list(groups, list, fcu, actname, muteipo);
	}
}

/* ------------------------- */

/* Convert IPO-block (i.e. all its IpoCurves) to the new system.
 * This does not assume that any ID or AnimData uses it, but does assume that
 * it is given two lists, which it will perform driver/animation-data separation.
 */
static void ipo_to_animato (Ipo *ipo, char actname[], char constname[], ListBase *animgroups, ListBase *anim, ListBase *drivers)
{
	IpoCurve *icu;
	
	/* sanity check */
	if (ELEM3(NULL, ipo, anim, drivers))
		return;
		
	if (G.f & G_DEBUG) printf("ipo_to_animato \n");
		
	/* validate actname and constname 
	 *	- clear actname if it was one of the generic <builtin> ones (i.e. 'Object', or 'Shapes')
	 *	- actname can then be used to assign F-Curves in Action to Action Groups 
	 *	  (i.e. thus keeping the benefits that used to be provided by Action Channels for grouping
	 *		F-Curves for bones). This may be added later... for now let's just dump without them...
	 */
	if (actname) {
		if ((ipo->blocktype == ID_OB) && (strcmp(actname, "Object") == 0))
			actname= NULL;
		else if ((ipo->blocktype == ID_OB) && (strcmp(actname, "Shape") == 0))
			actname= NULL;
	}
	
	/* loop over IPO-Curves, freeing as we progress */
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		/* Since an IPO-Curve may end up being made into many F-Curves (i.e. bitflag curves), 
		 * we figure out the best place to put the channel, then tell the curve-converter to just dump there
		 */
		if (icu->driver) {
			/* Blender 2.4x allowed empty drivers, but we don't now, since they cause more trouble than they're worth */
			if ((icu->driver->ob) || (icu->driver->type == IPO_DRIVER_TYPE_PYTHON)) {
				icu_to_fcurves(NULL, drivers, icu, actname, constname, ipo->muteipo);
			}
			else {
				MEM_freeN(icu->driver);
				icu->driver= NULL;
			}
		}
		else
			icu_to_fcurves(animgroups, anim, icu, actname, constname, ipo->muteipo);
	}
	
	/* if this IPO block doesn't have any users after this one, free... */
	ipo->id.us--;
	if ( (ipo->id.us == 0) || ((ipo->id.us == 1) && (ipo->id.flag & LIB_FAKEUSER)) ) 
	{
		IpoCurve *icn;
		
		for (icu= ipo->curve.first; icu; icu= icn) {
			icn= icu->next;
			
			/* free driver */
			if (icu->driver)
				MEM_freeN(icu->driver);
				
			/* free old data of curve now that it's no longer needed for converting any more curves */
			if (icu->bezt) MEM_freeN(icu->bezt);
			if (icu->bp) MEM_freeN(icu->bezt);
			
			/* free this IPO-Curve */
			BLI_freelinkN(&ipo->curve, icu);
		}
	}
}

/* Convert Action-block to new system, separating animation and drivers
 * New curves may not be converted directly into the given Action (i.e. for Actions linked
 * to Objects, where ob->ipo and ob->action need to be combined).
 * NOTE: we need to be careful here, as same data-structs are used for new system too!
 */
static void action_to_animato (bAction *act, ListBase *groups, ListBase *curves, ListBase *drivers)
{
	bActionChannel *achan, *achann;
	bConstraintChannel *conchan, *conchann;
	
	/* only continue if there are Action Channels (indicating unconverted data) */
	if (act->chanbase.first == NULL)
		return;
		
	/* get rid of all Action Groups */
	// XXX this is risky if there's some old + some new data in the Action...
	if (act->groups.first) 
		BLI_freelistN(&act->groups);
	
	/* loop through Action-Channels, converting data, freeing as we go */
	for (achan= act->chanbase.first; achan; achan= achann) {
		/* get pointer to next Action Channel */
		achann= achan->next;
		
		/* convert Action Channel's IPO data */
		if (achan->ipo) {
			ipo_to_animato(achan->ipo, achan->name, NULL, groups, curves, drivers);
			achan->ipo->id.us--;
			achan->ipo= NULL;
		}
		
		/* convert constraint channel IPO-data */
		for (conchan= achan->constraintChannels.first; conchan; conchan= conchann) {
			/* get pointer to next Constraint Channel */
			conchann= conchan->next;
			
			/* convert Constraint Channel's IPO data */
			if (conchan->ipo) {
				ipo_to_animato(conchan->ipo, achan->name, conchan->name, groups, curves, drivers);
				conchan->ipo->id.us--;
				conchan->ipo= NULL;
			}
			
			/* free Constraint Channel */
			BLI_freelinkN(&achan->constraintChannels, conchan);
		}
		
		/* free Action Channel */
		BLI_freelinkN(&act->chanbase, achan);
	}
}


/* ------------------------- */

/* Convert IPO-block (i.e. all its IpoCurves) for some ID to the new system
 * This assumes that AnimData has been added already. Separation of drivers
 * from animation data is accomplished here too...
 */
static void ipo_to_animdata (ID *id, Ipo *ipo, char actname[], char constname[])
{
	AnimData *adt= BKE_animdata_from_id(id);
	ListBase anim = {NULL, NULL};
	ListBase drivers = {NULL, NULL};
	
	/* sanity check */
	if ELEM(NULL, id, ipo)
		return;
	if (adt == NULL) {
		printf("ERROR ipo_to_animdata(): adt invalid \n");
		return;
	}
	
	if (G.f & G_DEBUG) {
		printf("ipo to animdata - ID:%s, IPO:%s, actname:%s constname:%s  curves:%d \n", 
			id->name+2, ipo->id.name+2, (actname)?actname:"<None>", (constname)?constname:"<None>", 
			BLI_countlist(&ipo->curve));
	}
	
	/* Convert curves to animato system (separated into separate lists of F-Curves for animation and drivers),
	 * and the try to put these lists in the right places, but do not free the lists here
	 */
	// XXX there shouldn't be any need for the groups, so don't supply pointer for that now... 
	ipo_to_animato(ipo, actname, constname, NULL, &anim, &drivers);
	
	/* deal with animation first */
	if (anim.first) {
		if (G.f & G_DEBUG) printf("\thas anim \n");
		/* try to get action */
		if (adt->action == NULL) {
			adt->action= add_empty_action("ConvData_Action"); // XXX we need a better name for this
			if (G.f & G_DEBUG) printf("\t\tadded new action \n");
		}
		
		/* add F-Curves to action */
		addlisttolist(&adt->action->curves, &anim);
	}
	
	/* deal with drivers */
	if (drivers.first) {
		if (G.f & G_DEBUG) printf("\thas drivers \n");
		/* add drivers to end of driver stack */
		addlisttolist(&adt->drivers, &drivers);
	}
}

/* Convert Action-block to new system
 * NOTE: we need to be careful here, as same data-structs are used for new system too!
 */
static void action_to_animdata (ID *id, bAction *act)
{
	AnimData *adt= BKE_animdata_from_id(id);
	
	/* only continue if there are Action Channels (indicating unconverted data) */
	if (ELEM(NULL, adt, act->chanbase.first))
		return;
	
	/* check if we need to set this Action as the AnimData's action */
	if (adt->action == NULL) {
		/* set this Action as AnimData's Action */
		if (G.f & G_DEBUG) printf("act_to_adt - set adt action to act \n");
		adt->action= act;
	}
	
	/* convert Action data */
	action_to_animato(act, &adt->action->groups, &adt->action->curves, &adt->drivers);
}

/* ------------------------- */

// TODO:
//	- NLA group duplicators info
//	- NLA curve/stride modifiers...

/* Convert NLA-Strip to new system */
static void nlastrips_to_animdata (ID *id, ListBase *strips)
{
	AnimData *adt= BKE_animdata_from_id(id);
	NlaTrack *nlt = NULL;
	NlaStrip *strip;
	bActionStrip *as, *asn;
	
	/* for each one of the original strips, convert to a new strip and free the old... */
	for (as= strips->first; as; as= asn) {
		asn= as->next;
		
		/* this old strip is only worth something if it had an action... */
		if (as->act) {
			/* convert Action data (if not yet converted), storing the results in the same Action */
			action_to_animato(as->act, &as->act->groups, &as->act->curves, &adt->drivers);
			
			/* create a new-style NLA-strip which references this Action, then copy over relevant settings */
			{
				/* init a new strip, and assign the action to it 
				 *	- no need to muck around with the user-counts, since this is just 
				 *	  passing over the ref to the new owner, not creating an additional ref
				 */
				strip= MEM_callocN(sizeof(NlaStrip), "NlaStrip");
				strip->act= as->act;
				
					/* endpoints */
				strip->start= as->start;
				strip->end= as->end;
				strip->actstart= as->actstart;
				strip->actend= as->actend;
				
					/* action reuse */
				strip->repeat= as->repeat;
				strip->scale= as->scale;
				if (as->flag & ACTSTRIP_LOCK_ACTION)	strip->flag |= NLASTRIP_FLAG_SYNC_LENGTH;
				
					/* blending */
				strip->blendin= as->blendin;
				strip->blendout= as->blendout;
				strip->blendmode= (as->mode==ACTSTRIPMODE_ADD) ? NLASTRIP_MODE_ADD : NLASTRIP_MODE_REPLACE;
				if (as->flag & ACTSTRIP_AUTO_BLENDS)	strip->flag |= NLASTRIP_FLAG_AUTO_BLENDS;
					
					/* assorted setting flags */
				if (as->flag & ACTSTRIP_SELECT) 		strip->flag |= NLASTRIP_FLAG_SELECT;
				if (as->flag & ACTSTRIP_ACTIVE) 		strip->flag |= NLASTRIP_FLAG_ACTIVE;
				
				if (as->flag & ACTSTRIP_MUTE)			strip->flag |= NLASTRIP_FLAG_MUTED;
				if (as->flag & ACTSTRIP_REVERSE)		strip->flag |= NLASTRIP_FLAG_REVERSE;
				
					/* by default, we now always extrapolate, while in the past this was optional */
				if ((as->flag & ACTSTRIP_HOLDLASTFRAME)==0) 
					strip->extendmode= NLASTRIP_EXTEND_NOTHING;
			}	
			
			/* try to add this strip to the current NLA-Track (i.e. the 'last' one on the stack atm) */
			if (BKE_nlatrack_add_strip(nlt, strip) == 0) {
				/* trying to add to the current failed (no space), 
				 * so add a new track to the stack, and add to that...
				 */
				nlt= add_nlatrack(adt, NULL);
				BKE_nlatrack_add_strip(nlt, strip);
			}
		}
		
		/* modifiers */
		// FIXME: for now, we just free them...
		if (as->modifiers.first)
			BLI_freelistN(&as->modifiers);
		
		/* free the old strip */
		BLI_freelinkN(strips, as);
	}
}

/* *************************************************** */
/* External API - Only Called from do_versions() */

/* Called from do_versions() in readfile.c to convert the old 'IPO/adrcode' system
 * to the new 'Animato/RNA' system.
 *
 * The basic method used here, is to loop over datablocks which have IPO-data, and 
 * add those IPO's to new AnimData blocks as Actions. 
 * Action/NLA data only works well for Objects, so these only need to be checked for there.
 *  
 * Data that has been converted should be freed immediately, which means that it is immediately
 * clear which datablocks have yet to be converted, and also prevent freeing errors when we exit.
 */
// XXX currently done after all file reading... 
void do_versions_ipos_to_animato(Main *main)
{
	ListBase drivers = {NULL, NULL};
	ID *id;
	AnimData *adt;
	Scene *scene;
	
	if (main == NULL) {
		printf("Argh! Main is NULL in do_versions_ipos_to_animato() \n");
		return;
	}
		
	/* only convert if version is right */
	// XXX???
	if (main->versionfile >= 250) {
		printf("WARNING: Animation data too new to convert (Version %d) \n", main->versionfile);
		return;
	}
	else
		printf("INFO: Converting to Animato... \n"); // xxx debug
		
	/* ----------- Animation Attached to Data -------------- */
	
	/* objects */
	for (id= main->object.first; id; id= id->next) {
		Object *ob= (Object *)id;
		bPoseChannel *pchan;
		bConstraint *con;
		bConstraintChannel *conchan, *conchann;
		
		if (G.f & G_DEBUG) printf("\tconverting ob %s \n", id->name+2);
		
		/* check if object has any animation data */
		if (ob->nlastrips.first) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* IPO first to take into any non-NLA'd Object Animation */
			if (ob->ipo) {
				ipo_to_animdata(id, ob->ipo, NULL, NULL);
				
				ob->ipo->id.us--;
				ob->ipo= NULL;
			}
			
			/* Action is skipped since it'll be used by some strip in the NLA anyway, 
			 * causing errors with evaluation in the new evaluation pipeline
			 */
			if (ob->action) {
				ob->action->id.us--;
				ob->action= NULL;
			}
			
			/* finally NLA */
			nlastrips_to_animdata(id, &ob->nlastrips);
		}
		else if ((ob->ipo) || (ob->action)) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* Action first - so that Action name get conserved */
			if (ob->action) {
				action_to_animdata(id, ob->action);
				
				/* only decrease usercount if this Action isn't now being used by AnimData */
				if (ob->action != adt->action) {
					ob->action->id.us--;
					ob->action= NULL;
				}
			}
			
			/* IPO second... */
			if (ob->ipo) {
				ipo_to_animdata(id, ob->ipo, NULL, NULL);
				ob->ipo->id.us--;
				ob->ipo= NULL;
			}
		}
		
		/* check PoseChannels for constraints with local data */
		if (ob->pose) {
			/* Verify if there's AnimData block */
			BKE_id_add_animdata(id);
			
			for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				for (con= pchan->constraints.first; con; con= con->next) {
					/* if constraint has own IPO, convert add these to Object 
					 * (NOTE: they're most likely to be drivers too) 
					 */
					if (con->ipo) {
						/* although this was the constraint's local IPO, we still need to provide pchan + con 
						 * so that drivers can be added properly...
						 */
						ipo_to_animdata(id, con->ipo, pchan->name, con->name);
						con->ipo->id.us--;
						con->ipo= NULL;
					}
				}
			}
		}
		
		/* check constraints for local IPO's */
		for (con= ob->constraints.first; con; con= con->next) {
			/* if constraint has own IPO, convert add these to Object 
			 * (NOTE: they're most likely to be drivers too) 
			 */
			if (con->ipo) {
				/* Verify if there's AnimData block, just in case */
				BKE_id_add_animdata(id);
				
				/* although this was the constraint's local IPO, we still need to provide con 
				 * so that drivers can be added properly...
				 */
				ipo_to_animdata(id, con->ipo, NULL, con->name);
				con->ipo->id.us--;
				con->ipo= NULL;
			}
			 
			/* check for Action Constraint */
			// XXX do we really want to do this here?
		}
		
		/* check constraint channels - we need to remove them anyway... */
		if (ob->constraintChannels.first) {
			/* Verify if there's AnimData block */
			BKE_id_add_animdata(id);
			
			for (conchan= ob->constraintChannels.first; conchan; conchan= conchann) {
				/* get pointer to next Constraint Channel */
				conchann= conchan->next;
				
				/* convert Constraint Channel's IPO data */
				if (conchan->ipo) {
					ipo_to_animdata(id, conchan->ipo, NULL, conchan->name);
					conchan->ipo->id.us--;
					conchan->ipo= NULL;
				}
				
				/* free Constraint Channel */
				BLI_freelinkN(&ob->constraintChannels, conchan);
			}
		}
	}
	
	/* shapekeys */
	for (id= main->key.first; id; id= id->next) {
		Key *key= (Key *)id;
		
		if (G.f & G_DEBUG) printf("\tconverting key %s \n", id->name+2);
		
		/* we're only interested in the IPO 
		 * NOTE: for later, it might be good to port these over to Object instead, as many of these
		 * are likely to be drivers, but it's hard to trace that from here, so move this to Ob loop?
		 */
		if (key->ipo) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* Convert Shapekey data... */
			ipo_to_animdata(id, key->ipo, NULL, NULL);
			key->ipo->id.us--;
			key->ipo= NULL;
		}
	}
	
	/* materials */
	for (id= main->mat.first; id; id= id->next) {
		Material *ma= (Material *)id;
		
		if (G.f & G_DEBUG) printf("\tconverting material %s \n", id->name+2);
		
		/* we're only interested in the IPO */
		if (ma->ipo) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* Convert Material data... */
			ipo_to_animdata(id, ma->ipo, NULL, NULL);
			ma->ipo->id.us--;
			ma->ipo= NULL;
		}
	}
	
	/* worlds */
	for (id= main->world.first; id; id= id->next) {
		World *wo= (World *)id;
		
		if (G.f & G_DEBUG) printf("\tconverting world %s \n", id->name+2);
		
		/* we're only interested in the IPO */
		if (wo->ipo) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* Convert World data... */
			ipo_to_animdata(id, wo->ipo, NULL, NULL);
			wo->ipo->id.us--;
			wo->ipo= NULL;
		}
	}
	
	/* sequence strips */
	for(scene = main->scene.first; scene; scene = scene->id.next) {
		if(scene->ed && scene->ed->seqbasep) {
			Sequence * seq;
			
			for(seq = scene->ed->seqbasep->first; 
			    seq; seq = seq->next) {
				short adrcode = SEQ_FAC1;
				
				if (G.f & G_DEBUG) 
					printf("\tconverting sequence strip %s \n", seq->name+2);
				
				if (!seq->ipo || !seq->ipo->curve.first) {
					seq->flag |= 
						SEQ_USE_EFFECT_DEFAULT_FADE;
					continue;
				}
				
				/* patch adrcode, so that we can map
				   to different DNA variables later 
				   (semi-hack (tm) )
				*/
				switch(seq->type) {
				case SEQ_IMAGE:
				case SEQ_META:
				case SEQ_SCENE:
				case SEQ_MOVIE:
				case SEQ_COLOR:
					adrcode = SEQ_FAC_OPACITY;
					break;
				case SEQ_SPEED:
					adrcode = SEQ_FAC_SPEED;
					break;
				}
				((IpoCurve*) seq->ipo->curve.first)
					->adrcode = adrcode;
				ipo_to_animdata((ID*) seq, seq->ipo,
						NULL, NULL);
				seq->ipo->id.us--;
				seq->ipo = NULL;
			}
		}
	}


	/* textures */
	for (id= main->tex.first; id; id= id->next) {
		Tex *te= (Tex *)id;
		
		if (G.f & G_DEBUG) printf("\tconverting texture %s \n", id->name+2);
		
		/* we're only interested in the IPO */
		if (te->ipo) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* Convert Texture data... */
			ipo_to_animdata(id, te->ipo, NULL, NULL);
			te->ipo->id.us--;
			te->ipo= NULL;
		}
	}
	
	/* cameras */
	for (id= main->camera.first; id; id= id->next) {
		Camera *ca= (Camera *)id;
		
		if (G.f & G_DEBUG) printf("\tconverting camera %s \n", id->name+2);
		
		/* we're only interested in the IPO */
		if (ca->ipo) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* Convert Camera data... */
			ipo_to_animdata(id, ca->ipo, NULL, NULL);
			ca->ipo->id.us--;
			ca->ipo= NULL;
		}
	}
	
	/* lamps */
	for (id= main->lamp.first; id; id= id->next) {
		Lamp *la= (Lamp *)id;
		
		if (G.f & G_DEBUG) printf("\tconverting lamp %s \n", id->name+2);
		
		/* we're only interested in the IPO */
		if (la->ipo) {
			/* Add AnimData block */
			adt= BKE_id_add_animdata(id);
			
			/* Convert Lamp data... */
			ipo_to_animdata(id, la->ipo, NULL, NULL);
			la->ipo->id.us--;
			la->ipo= NULL;
		}
	}
	
	/* --------- Unconverted Animation Data ------------------ */
	/* For Animation data which may not be directly connected (i.e. not linked) to any other 
	 * data, we need to perform a separate pass to make sure that they are converted to standalone
	 * Actions which may then be able to be reused. This does mean that we will be going over data that's
	 * already been converted, but there are no problems with that.
	 *
	 * The most common case for this will be Action Constraints, or IPO's with Fake-Users. 
	 * We collect all drivers that were found into a temporary collection, and free them in one go, as they're 
	 * impossible to resolve.
	 */
	
	/* actions */
	for (id= main->action.first; id; id= id->next) {
		bAction *act= (bAction *)id;
		
		if (G.f & G_DEBUG) printf("\tconverting action %s \n", id->name+2);
		
		/* be careful! some of the actions we encounter will be converted ones... */
		action_to_animato(act, &act->groups, &act->curves, &drivers);
	}
	
	/* ipo's */
	for (id= main->ipo.first; id; id= id->next) {
		Ipo *ipo= (Ipo *)id;
		
		if (G.f & G_DEBUG) printf("\tconverting ipo %s \n", id->name+2);
		
		/* most likely this IPO has already been processed, so check if any curves left to convert */
		if (ipo->curve.first) {
			bAction *new_act;
			
			/* add a new action for this, and convert all data into that action */
			new_act= add_empty_action("ConvIPO_Action"); // XXX need a better name...
			ipo_to_animato(ipo, NULL, NULL, NULL, &new_act->curves, &drivers);
		}
		
		/* clear fake-users, and set user-count to zero to make sure it is cleared on file-save */
		ipo->id.us= 0;
		ipo->id.flag &= ~LIB_FAKEUSER;
	}
	
	/* free unused drivers from actions + ipos */
	free_fcurves(&drivers);
	
	printf("INFO: Animato convert done \n"); // xxx debug
}

