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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_endian_switch.h"
#include "BLI_memarena.h"
#include "BLI_math.h"

#include "DNA_defaults.h"
#include "DNA_scene_types.h"
#include "DNA_curve_types.h"

#include "DNA_scene_defaults.h"

const Scene DNA_DEFAULT_Scene = _DNA_DEFAULT_Scene;
const ToolSettings DNA_DEFAULT_ToolSettings = _DNA_DEFAULT_ToolSettings;

/**
 * Prevent assigning the wrong struct types since all elements in #DNA_default_table are `void *`.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define SDNA_TYPE_CHECKED(v, t) (&(v) + (_Generic((v), t : 0)))
#else
#  define SDNA_TYPE_CHECKED(v, t) (&(v))
#endif

/*  */
#define SDNA_DEFAULT_DECL(struct_name) \
  [SDNA_TYPE_FROM_STRUCT(struct_name)] = SDNA_TYPE_CHECKED(DNA_DEFAULT_##struct_name, struct_name)

#define SDNA_DEFAULT_DECL_EX(struct_name, struct_path) \
  [SDNA_TYPE_FROM_STRUCT(struct_name)] = SDNA_TYPE_CHECKED(DNA_DEFAULT_##struct_path, struct_name)

/** Keep headers sorted. */
const void *DNA_default_table[SDNA_TYPE_MAX] = {
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

    SDNA_DEFAULT_DECL(ToolSettings),
    SDNA_DEFAULT_DECL_EX(CurvePaintSettings, ToolSettings.curve_paint_settings),
    SDNA_DEFAULT_DECL_EX(ImagePaintSettings, ToolSettings.imapaint),
    SDNA_DEFAULT_DECL_EX(ParticleEditSettings, ToolSettings.particle),
    SDNA_DEFAULT_DECL_EX(ParticleBrushData, ToolSettings.particle.brush[0]),
    SDNA_DEFAULT_DECL_EX(MeshStatVis, ToolSettings.statvis),
    SDNA_DEFAULT_DECL_EX(GP_Sculpt_Settings, ToolSettings.gp_sculpt),
    SDNA_DEFAULT_DECL_EX(GP_Sculpt_Guide, ToolSettings.gp_sculpt.guide),

    /* DNA_view3d_defaults.h */
    SDNA_DEFAULT_DECL_EX(View3DShading, Scene.display.shading),
    SDNA_DEFAULT_DECL_EX(View3DCursor, Scene.cursor),
};
#undef SDNA_DEFAULT_DECL
#undef SDNA_DEFAULT_DECL_EX

char *_DNA_struct_default_alloc_impl(const char *data_src, size_t size, const char *alloc_str)
{
  char *data_dst = MEM_mallocN(size, alloc_str);
  memcpy(data_dst, data_src, size);
  return data_dst;
}
