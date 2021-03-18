/*
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
 * DNA default value access.
 */

/** \file
 * \ingroup DNA
 *
 * DNA Defaults
 * ============
 *
 * This API provides direct access to DNA default structs
 * to avoid duplicating values for initialization, versioning and RNA.
 * This allows DNA default definitions to be defined in a single header along side the types.
 * So each `DNA_{name}_types.h` can have an optional `DNA_{name}_defaults.h` file along side it.
 *
 * Defining the defaults is optional since it doesn't make sense for some structs to have defaults.
 *
 * Adding Defaults
 * ---------------
 *
 * Adding/removing defaults for existing structs can be done by hand.
 * When adding new defaults for larger structs you may want to write-out the in-memory data.
 *
 * To create these defaults there is a GDB script which can be handy to get started:
 * `./source/tools/utils/gdb_struct_repr_c99.py`
 *
 * Magic numbers should be replaced with flags before committing.
 *
 * Public API
 * ----------
 *
 * The main functions to access these are:
 * - #DNA_struct_default_get
 * - #DNA_struct_default_alloc
 *
 * These access the struct table #DNA_default_table using the struct number.
 *
 * \note Struct members only define their members (pointers are left as NULL set).
 *
 * Typical Usage
 * -------------
 *
 * While there is no restriction for using these defaults,
 * it's worth noting where these functions are typically used:
 *
 * - When creating/allocating new data.
 * - RNA property defaults, used for "Set Default Value" in the buttons right-click context menu.
 *
 * These defaults are not used:
 *
 * - When loading old files that don't contain newly added struct members (these will be zeroed)
 *   to set their values use `versioning_{BLENDER_VERSION}.c` source files.
 * - For startup file data, to update these defaults use
 *   #BLO_update_defaults_startup_blend & #blo_do_versions_userdef.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_endian_switch.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf.h"

#include "DNA_defaults.h"

#include "DNA_armature_types.h"
#include "DNA_asset_types.h"
#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_fluid_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_hair_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"
#include "DNA_space_types.h"
#include "DNA_speaker_types.h"
#include "DNA_texture_types.h"
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "DNA_armature_defaults.h"
#include "DNA_asset_defaults.h"
#include "DNA_brush_defaults.h"
#include "DNA_cachefile_defaults.h"
#include "DNA_camera_defaults.h"
#include "DNA_collection_defaults.h"
#include "DNA_curve_defaults.h"
#include "DNA_fluid_defaults.h"
#include "DNA_gpencil_modifier_defaults.h"
#include "DNA_hair_defaults.h"
#include "DNA_image_defaults.h"
#include "DNA_lattice_defaults.h"
#include "DNA_light_defaults.h"
#include "DNA_lightprobe_defaults.h"
#include "DNA_linestyle_defaults.h"
#include "DNA_material_defaults.h"
#include "DNA_mesh_defaults.h"
#include "DNA_meta_defaults.h"
#include "DNA_modifier_defaults.h"
#include "DNA_movieclip_defaults.h"
#include "DNA_object_defaults.h"
#include "DNA_particle_defaults.h"
#include "DNA_pointcloud_defaults.h"
#include "DNA_scene_defaults.h"
#include "DNA_simulation_defaults.h"
#include "DNA_speaker_defaults.h"
#include "DNA_texture_defaults.h"
#include "DNA_volume_defaults.h"
#include "DNA_world_defaults.h"

#define SDNA_DEFAULT_DECL_STRUCT(struct_name) \
  static const struct_name DNA_DEFAULT_##struct_name = _DNA_DEFAULT_##struct_name

/* DNA_asset_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(AssetMetaData);

/* DNA_armature_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(bArmature);

/* DNA_brush_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Brush);

/* DNA_cachefile_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(CacheFile);

/* DNA_camera_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Camera);

/* DNA_collection_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Collection);

/* DNA_curve_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Curve);

/* DNA_fluid_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(FluidDomainSettings);
SDNA_DEFAULT_DECL_STRUCT(FluidFlowSettings);
SDNA_DEFAULT_DECL_STRUCT(FluidEffectorSettings);

/* DNA_image_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Image);

/* DNA_hair_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Hair);

/* DNA_lattice_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Lattice);

/* DNA_light_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Light);

/* DNA_lightprobe_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(LightProbe);

/* DNA_linestyle_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(FreestyleLineStyle);

/* DNA_material_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Material);

/* DNA_mesh_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Mesh);

/* DNA_meta_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(MetaBall);

/* DNA_movieclip_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(MovieClip);

/* DNA_object_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Object);

/* DNA_particle_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(ParticleSettings);

/* DNA_pointcloud_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(PointCloud);

/* DNA_scene_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Scene);
SDNA_DEFAULT_DECL_STRUCT(ToolSettings);

/* DNA_simulation_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Simulation);

/* DNA_speaker_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Speaker);

/* DNA_texture_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Tex);

/* DNA_view3d_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(View3D);

/* DNA_volume_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Volume);

/* DNA_world_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(World);

/* DNA_modifier_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(ArmatureModifierData);
SDNA_DEFAULT_DECL_STRUCT(ArrayModifierData);
SDNA_DEFAULT_DECL_STRUCT(BevelModifierData);
SDNA_DEFAULT_DECL_STRUCT(BooleanModifierData);
SDNA_DEFAULT_DECL_STRUCT(BuildModifierData);
SDNA_DEFAULT_DECL_STRUCT(CastModifierData);
SDNA_DEFAULT_DECL_STRUCT(ClothSimSettings);
SDNA_DEFAULT_DECL_STRUCT(ClothCollSettings);
SDNA_DEFAULT_DECL_STRUCT(ClothModifierData);
SDNA_DEFAULT_DECL_STRUCT(CollisionModifierData);
SDNA_DEFAULT_DECL_STRUCT(CorrectiveSmoothModifierData);
SDNA_DEFAULT_DECL_STRUCT(CurveModifierData);
// SDNA_DEFAULT_DECL_STRUCT(DataTransferModifierData);
SDNA_DEFAULT_DECL_STRUCT(DecimateModifierData);
SDNA_DEFAULT_DECL_STRUCT(DisplaceModifierData);
SDNA_DEFAULT_DECL_STRUCT(DynamicPaintModifierData);
SDNA_DEFAULT_DECL_STRUCT(EdgeSplitModifierData);
SDNA_DEFAULT_DECL_STRUCT(ExplodeModifierData);
/* Fluid modifier skipped for now. */
SDNA_DEFAULT_DECL_STRUCT(HookModifierData);
SDNA_DEFAULT_DECL_STRUCT(LaplacianDeformModifierData);
SDNA_DEFAULT_DECL_STRUCT(LaplacianSmoothModifierData);
SDNA_DEFAULT_DECL_STRUCT(LatticeModifierData);
SDNA_DEFAULT_DECL_STRUCT(MaskModifierData);
SDNA_DEFAULT_DECL_STRUCT(MeshCacheModifierData);
SDNA_DEFAULT_DECL_STRUCT(MeshDeformModifierData);
SDNA_DEFAULT_DECL_STRUCT(MeshSeqCacheModifierData);
SDNA_DEFAULT_DECL_STRUCT(MirrorModifierData);
SDNA_DEFAULT_DECL_STRUCT(MultiresModifierData);
SDNA_DEFAULT_DECL_STRUCT(NormalEditModifierData);
SDNA_DEFAULT_DECL_STRUCT(OceanModifierData);
SDNA_DEFAULT_DECL_STRUCT(ParticleInstanceModifierData);
SDNA_DEFAULT_DECL_STRUCT(ParticleSystemModifierData);
SDNA_DEFAULT_DECL_STRUCT(RemeshModifierData);
SDNA_DEFAULT_DECL_STRUCT(ScrewModifierData);
/* Shape key modifier has no items. */
SDNA_DEFAULT_DECL_STRUCT(ShrinkwrapModifierData);
SDNA_DEFAULT_DECL_STRUCT(SimpleDeformModifierData);
SDNA_DEFAULT_DECL_STRUCT(NodesModifierData);
SDNA_DEFAULT_DECL_STRUCT(SkinModifierData);
SDNA_DEFAULT_DECL_STRUCT(SmoothModifierData);
/* Softbody modifier skipped for now. */
SDNA_DEFAULT_DECL_STRUCT(SolidifyModifierData);
SDNA_DEFAULT_DECL_STRUCT(SubsurfModifierData);
SDNA_DEFAULT_DECL_STRUCT(SurfaceModifierData);
SDNA_DEFAULT_DECL_STRUCT(SurfaceDeformModifierData);
SDNA_DEFAULT_DECL_STRUCT(TriangulateModifierData);
SDNA_DEFAULT_DECL_STRUCT(UVProjectModifierData);
SDNA_DEFAULT_DECL_STRUCT(UVWarpModifierData);
SDNA_DEFAULT_DECL_STRUCT(WarpModifierData);
SDNA_DEFAULT_DECL_STRUCT(WaveModifierData);
SDNA_DEFAULT_DECL_STRUCT(WeightedNormalModifierData);
SDNA_DEFAULT_DECL_STRUCT(WeightVGEditModifierData);
SDNA_DEFAULT_DECL_STRUCT(WeightVGMixModifierData);
SDNA_DEFAULT_DECL_STRUCT(WeightVGProximityModifierData);
SDNA_DEFAULT_DECL_STRUCT(WeldModifierData);
SDNA_DEFAULT_DECL_STRUCT(WireframeModifierData);

/* DNA_gpencil_modifier_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(ArmatureGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(ArrayGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(BuildGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(ColorGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(HookGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(LatticeGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(MirrorGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(MultiplyGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(NoiseGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(OffsetGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(OpacityGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(SimplifyGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(SmoothGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(SubdivGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(TextureGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(ThickGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(TimeGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(TintGpencilModifierData);
SDNA_DEFAULT_DECL_STRUCT(LineartGpencilModifierData);

#undef SDNA_DEFAULT_DECL_STRUCT

/* Reuse existing definitions. */
extern const struct UserDef U_default;
#define DNA_DEFAULT_UserDef U_default

extern const bTheme U_theme_default;
#define DNA_DEFAULT_bTheme U_theme_default

/**
 * Prevent assigning the wrong struct types since all elements in #DNA_default_table are `void *`.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define SDNA_TYPE_CHECKED(v, t) (&(v) + (_Generic((v), t : 0)))
#else
#  define SDNA_TYPE_CHECKED(v, t) (&(v))
#endif

#define SDNA_DEFAULT_DECL(struct_name) \
  [SDNA_TYPE_FROM_STRUCT(struct_name)] = SDNA_TYPE_CHECKED(DNA_DEFAULT_##struct_name, struct_name)

#define SDNA_DEFAULT_DECL_EX(struct_name, struct_path) \
  [SDNA_TYPE_FROM_STRUCT(struct_name)] = SDNA_TYPE_CHECKED(DNA_DEFAULT_##struct_path, struct_name)

/** Keep headers sorted. */
const void *DNA_default_table[SDNA_TYPE_MAX] = {

    /* DNA_asset_defaults.h */
    SDNA_DEFAULT_DECL(AssetMetaData),

    /* DNA_armature_defaults.h */
    SDNA_DEFAULT_DECL(bArmature),

    /* DNA_brush_defaults.h */
    SDNA_DEFAULT_DECL(Brush),

    /* DNA_cachefile_defaults.h */
    SDNA_DEFAULT_DECL(CacheFile),

    /* DNA_camera_defaults.h */
    SDNA_DEFAULT_DECL(Camera),
    SDNA_DEFAULT_DECL_EX(CameraDOFSettings, Camera.dof),
    SDNA_DEFAULT_DECL_EX(CameraStereoSettings, Camera.stereo),

    /* DNA_collection_defaults.h */
    SDNA_DEFAULT_DECL(Collection),

    /* DNA_curve_defaults.h */
    SDNA_DEFAULT_DECL(Curve),

    /* DNA_fluid_defaults.h */
    SDNA_DEFAULT_DECL(FluidDomainSettings),
    SDNA_DEFAULT_DECL(FluidFlowSettings),
    SDNA_DEFAULT_DECL(FluidEffectorSettings),

    /* DNA_image_defaults.h */
    SDNA_DEFAULT_DECL(Image),

    /* DNA_hair_defaults.h */
    SDNA_DEFAULT_DECL(Hair),

    /* DNA_lattice_defaults.h */
    SDNA_DEFAULT_DECL(Lattice),

    /* DNA_light_defaults.h */
    SDNA_DEFAULT_DECL(Light),

    /* DNA_lightprobe_defaults.h */
    SDNA_DEFAULT_DECL(LightProbe),

    /* DNA_linestyle_defaults.h */
    SDNA_DEFAULT_DECL(FreestyleLineStyle),

    /* DNA_material_defaults.h */
    SDNA_DEFAULT_DECL(Material),

    /* DNA_mesh_defaults.h */
    SDNA_DEFAULT_DECL(Mesh),

    /* DNA_meta_defaults.h */
    SDNA_DEFAULT_DECL(MetaBall),

    /* DNA_movieclip_defaults.h */
    SDNA_DEFAULT_DECL(MovieClip),

    /* DNA_object_defaults.h */
    SDNA_DEFAULT_DECL(Object),

    /* DNA_particle_defaults.h */
    SDNA_DEFAULT_DECL(ParticleSettings),

    /* DNA_pointcloud_defaults.h */
    SDNA_DEFAULT_DECL(PointCloud),

    /* DNA_scene_defaults.h */
    SDNA_DEFAULT_DECL(Scene),
    SDNA_DEFAULT_DECL_EX(RenderData, Scene.r),
    SDNA_DEFAULT_DECL_EX(ImageFormatData, Scene.r.im_format),
    SDNA_DEFAULT_DECL_EX(BakeData, Scene.r.bake),
    SDNA_DEFAULT_DECL_EX(FFMpegCodecData, Scene.r.ffcodecdata),
    SDNA_DEFAULT_DECL_EX(DisplaySafeAreas, Scene.safe_areas),
    SDNA_DEFAULT_DECL_EX(AudioData, Scene.audio),
    SDNA_DEFAULT_DECL_EX(PhysicsSettings, Scene.physics_settings),
    SDNA_DEFAULT_DECL_EX(SceneDisplay, Scene.display),
    SDNA_DEFAULT_DECL_EX(SceneEEVEE, Scene.eevee),

    SDNA_DEFAULT_DECL(ToolSettings),
    SDNA_DEFAULT_DECL_EX(CurvePaintSettings, ToolSettings.curve_paint_settings),
    SDNA_DEFAULT_DECL_EX(ImagePaintSettings, ToolSettings.imapaint),
    SDNA_DEFAULT_DECL_EX(UnifiedPaintSettings, ToolSettings.unified_paint_settings),
    SDNA_DEFAULT_DECL_EX(ParticleEditSettings, ToolSettings.particle),
    SDNA_DEFAULT_DECL_EX(ParticleBrushData, ToolSettings.particle.brush[0]),
    SDNA_DEFAULT_DECL_EX(MeshStatVis, ToolSettings.statvis),
    SDNA_DEFAULT_DECL_EX(GP_Sculpt_Settings, ToolSettings.gp_sculpt),
    SDNA_DEFAULT_DECL_EX(GP_Sculpt_Guide, ToolSettings.gp_sculpt.guide),

    /* DNA_simulation_defaults.h */
    SDNA_DEFAULT_DECL(Simulation),

    /* DNA_speaker_defaults.h */
    SDNA_DEFAULT_DECL(Speaker),

    /* DNA_texture_defaults.h */
    SDNA_DEFAULT_DECL(Tex),
    SDNA_DEFAULT_DECL_EX(MTex, Brush.mtex),

    /* DNA_userdef_types.h */
    SDNA_DEFAULT_DECL(UserDef),
    SDNA_DEFAULT_DECL(bTheme),
    SDNA_DEFAULT_DECL_EX(UserDef_SpaceData, UserDef.space_data),
    SDNA_DEFAULT_DECL_EX(UserDef_FileSpaceData, UserDef.file_space_data),
    SDNA_DEFAULT_DECL_EX(WalkNavigation, UserDef.walk_navigation),

    /* DNA_view3d_defaults.h */
    SDNA_DEFAULT_DECL(View3D),
    SDNA_DEFAULT_DECL_EX(View3DOverlay, View3D.overlay),
    SDNA_DEFAULT_DECL_EX(View3DShading, View3D.shading),
    SDNA_DEFAULT_DECL_EX(View3DCursor, Scene.cursor),

    /* DNA_volume_defaults.h */
    SDNA_DEFAULT_DECL(Volume),

    /* DNA_world_defaults.h */
    SDNA_DEFAULT_DECL(World),

    /* DNA_modifier_defaults.h */
    SDNA_DEFAULT_DECL(ArmatureModifierData),
    SDNA_DEFAULT_DECL(ArrayModifierData),
    SDNA_DEFAULT_DECL(BevelModifierData),
    SDNA_DEFAULT_DECL(BooleanModifierData),
    SDNA_DEFAULT_DECL(BuildModifierData),
    SDNA_DEFAULT_DECL(CastModifierData),
    SDNA_DEFAULT_DECL(ClothSimSettings),
    SDNA_DEFAULT_DECL(ClothCollSettings),
    SDNA_DEFAULT_DECL(ClothModifierData),
    SDNA_DEFAULT_DECL(CollisionModifierData),
    SDNA_DEFAULT_DECL(CorrectiveSmoothModifierData),
    SDNA_DEFAULT_DECL(CurveModifierData),
    // SDNA_DEFAULT_DECL(DataTransferModifierData),
    SDNA_DEFAULT_DECL(DecimateModifierData),
    SDNA_DEFAULT_DECL(DisplaceModifierData),
    SDNA_DEFAULT_DECL(DynamicPaintModifierData),
    SDNA_DEFAULT_DECL(EdgeSplitModifierData),
    SDNA_DEFAULT_DECL(ExplodeModifierData),
    /* Fluid modifier skipped for now. */
    SDNA_DEFAULT_DECL(HookModifierData),
    SDNA_DEFAULT_DECL(LaplacianDeformModifierData),
    SDNA_DEFAULT_DECL(LaplacianSmoothModifierData),
    SDNA_DEFAULT_DECL(LatticeModifierData),
    SDNA_DEFAULT_DECL(MaskModifierData),
    SDNA_DEFAULT_DECL(MeshCacheModifierData),
    SDNA_DEFAULT_DECL(MeshDeformModifierData),
    SDNA_DEFAULT_DECL(MeshSeqCacheModifierData),
    SDNA_DEFAULT_DECL(MirrorModifierData),
    SDNA_DEFAULT_DECL(MultiresModifierData),
    SDNA_DEFAULT_DECL(NormalEditModifierData),
    SDNA_DEFAULT_DECL(OceanModifierData),
    SDNA_DEFAULT_DECL(ParticleInstanceModifierData),
    SDNA_DEFAULT_DECL(ParticleSystemModifierData),
    SDNA_DEFAULT_DECL(RemeshModifierData),
    SDNA_DEFAULT_DECL(ScrewModifierData),
    /* Shape key modifier has no items. */
    SDNA_DEFAULT_DECL(ShrinkwrapModifierData),
    SDNA_DEFAULT_DECL(SimpleDeformModifierData),
    SDNA_DEFAULT_DECL(NodesModifierData),
    SDNA_DEFAULT_DECL(SkinModifierData),
    SDNA_DEFAULT_DECL(SmoothModifierData),
    /* Softbody modifier skipped for now. */
    SDNA_DEFAULT_DECL(SolidifyModifierData),
    SDNA_DEFAULT_DECL(SubsurfModifierData),
    SDNA_DEFAULT_DECL(SurfaceModifierData),
    SDNA_DEFAULT_DECL(SurfaceDeformModifierData),
    SDNA_DEFAULT_DECL(TriangulateModifierData),
    SDNA_DEFAULT_DECL(UVProjectModifierData),
    SDNA_DEFAULT_DECL(UVWarpModifierData),
    SDNA_DEFAULT_DECL(WarpModifierData),
    SDNA_DEFAULT_DECL(WaveModifierData),
    SDNA_DEFAULT_DECL(WeightedNormalModifierData),
    SDNA_DEFAULT_DECL(WeightVGEditModifierData),
    SDNA_DEFAULT_DECL(WeightVGMixModifierData),
    SDNA_DEFAULT_DECL(WeightVGProximityModifierData),
    SDNA_DEFAULT_DECL(WeldModifierData),
    SDNA_DEFAULT_DECL(WireframeModifierData),

    /* DNA_gpencil_modifier_defaults.h */
    SDNA_DEFAULT_DECL(ArmatureGpencilModifierData),
    SDNA_DEFAULT_DECL(ArrayGpencilModifierData),
    SDNA_DEFAULT_DECL(BuildGpencilModifierData),
    SDNA_DEFAULT_DECL(ColorGpencilModifierData),
    SDNA_DEFAULT_DECL(HookGpencilModifierData),
    SDNA_DEFAULT_DECL(LatticeGpencilModifierData),
    SDNA_DEFAULT_DECL(MirrorGpencilModifierData),
    SDNA_DEFAULT_DECL(MultiplyGpencilModifierData),
    SDNA_DEFAULT_DECL(NoiseGpencilModifierData),
    SDNA_DEFAULT_DECL(OffsetGpencilModifierData),
    SDNA_DEFAULT_DECL(OpacityGpencilModifierData),
    SDNA_DEFAULT_DECL(SimplifyGpencilModifierData),
    SDNA_DEFAULT_DECL(SmoothGpencilModifierData),
    SDNA_DEFAULT_DECL(SubdivGpencilModifierData),
    SDNA_DEFAULT_DECL(TextureGpencilModifierData),
    SDNA_DEFAULT_DECL(ThickGpencilModifierData),
    SDNA_DEFAULT_DECL(TimeGpencilModifierData),
    SDNA_DEFAULT_DECL(TintGpencilModifierData),
    SDNA_DEFAULT_DECL(LineartGpencilModifierData),
};
#undef SDNA_DEFAULT_DECL
#undef SDNA_DEFAULT_DECL_EX

char *_DNA_struct_default_alloc_impl(const char *data_src, size_t size, const char *alloc_str)
{
  char *data_dst = MEM_mallocN(size, alloc_str);
  memcpy(data_dst, data_src, size);
  return data_dst;
}
