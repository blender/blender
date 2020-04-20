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
 * This API provides direct access to DNA default structs
 * to avoid duplicating values for initialization, versioning and RNA.
 * This allows DNA default definitions to be defined in a single header along side the types.
 * So each `DNA_{name}_types.h` can have an optional `DNA_{name}_defaults.h` file along side it.
 *
 * Defining the defaults is optional since it doesn't make sense for some structs to have defaults.
 *
 * To create these defaults there is a GDB script which can be handy to get started:
 * `./source/tools/utils/gdb_struct_repr_c99.py`
 *
 * Magic numbers should be replaced with flags before committing.
 *
 * The main functions to access these are:
 * - #DNA_struct_default_get
 * - #DNA_struct_default_alloc
 *
 * These access the struct table #DNA_default_table using the struct number.
 *
 * \note Struct members only define their members (pointers are left as NULL set).
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

#include "DNA_defaults.h"

#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
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
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"
#include "DNA_space_types.h"
#include "DNA_speaker_types.h"
#include "DNA_texture_types.h"
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "DNA_brush_defaults.h"
#include "DNA_cachefile_defaults.h"
#include "DNA_camera_defaults.h"
#include "DNA_curve_defaults.h"
#include "DNA_hair_defaults.h"
#include "DNA_image_defaults.h"
#include "DNA_lattice_defaults.h"
#include "DNA_light_defaults.h"
#include "DNA_lightprobe_defaults.h"
#include "DNA_linestyle_defaults.h"
#include "DNA_material_defaults.h"
#include "DNA_mesh_defaults.h"
#include "DNA_meta_defaults.h"
#include "DNA_object_defaults.h"
#include "DNA_pointcloud_defaults.h"
#include "DNA_scene_defaults.h"
#include "DNA_simulation_defaults.h"
#include "DNA_speaker_defaults.h"
#include "DNA_texture_defaults.h"
#include "DNA_volume_defaults.h"
#include "DNA_world_defaults.h"

#define SDNA_DEFAULT_DECL_STRUCT(struct_name) \
  static const struct_name DNA_DEFAULT_##struct_name = _DNA_DEFAULT_##struct_name

/* DNA_brush_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Brush);

/* DNA_cachefile_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(CacheFile);

/* DNA_camera_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Camera);

/* DNA_curve_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Curve);

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

/* DNA_object_defaults.h */
SDNA_DEFAULT_DECL_STRUCT(Object);

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

    /* DNA_brush_defaults.h */
    SDNA_DEFAULT_DECL(Brush),

    /* DNA_cachefile_defaults.h */
    SDNA_DEFAULT_DECL(CacheFile),

    /* DNA_camera_defaults.h */
    SDNA_DEFAULT_DECL(Camera),
    SDNA_DEFAULT_DECL_EX(CameraDOFSettings, Camera.dof),
    SDNA_DEFAULT_DECL_EX(CameraStereoSettings, Camera.stereo),

    /* DNA_curve_defaults.h */
    SDNA_DEFAULT_DECL(Curve),

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

    /* DNA_object_defaults.h */
    SDNA_DEFAULT_DECL(Object),

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
};
#undef SDNA_DEFAULT_DECL
#undef SDNA_DEFAULT_DECL_EX

char *_DNA_struct_default_alloc_impl(const char *data_src, size_t size, const char *alloc_str)
{
  char *data_dst = MEM_mallocN(size, alloc_str);
  memcpy(data_dst, data_src, size);
  return data_dst;
}
