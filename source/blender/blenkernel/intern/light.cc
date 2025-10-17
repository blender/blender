/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <optional>

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_defaults.h"
#include "DNA_light_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_base.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_utildefines.h"

#include "BKE_icons.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_light.h"
#include "BKE_node.hh"
#include "BKE_preview_image.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"

#include "IMB_colormanagement.hh"

#include "BLO_read_write.hh"

static void light_init_data(ID *id)
{
  Light *la = (Light *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(la, id));

  MEMCPY_STRUCT_AFTER(la, DNA_struct_default_get(Light), id);
}

/**
 * Only copy internal data of Light ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.hh's LIB_ID_COPY_... flags for more).
 */
static void light_copy_data(Main *bmain,
                            std::optional<Library *> owner_library,
                            ID *id_dst,
                            const ID *id_src,
                            const int flag)
{
  Light *la_dst = (Light *)id_dst;
  const Light *la_src = (const Light *)id_src;

  const bool is_localized = (flag & LIB_ID_CREATE_LOCAL) != 0;
  /* We always need allocation of our private ID data.
   * User reference-counting is also handled by calling code,
   * so the duplication calls for embedded data should _never_ handle it from here. */
  const int flag_embedded_id_data = (flag & ~LIB_ID_CREATE_NO_ALLOCATE) |
                                    LIB_ID_CREATE_NO_USER_REFCOUNT;

  if (la_src->nodetree) {
    if (is_localized) {
      la_dst->nodetree = blender::bke::node_tree_localize(la_src->nodetree, &la_dst->id);
    }
    else {
      BKE_id_copy_in_lib(bmain,
                         owner_library,
                         &la_src->nodetree->id,
                         &la_dst->id,
                         reinterpret_cast<ID **>(&la_dst->nodetree),
                         flag_embedded_id_data);
    }
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&la_dst->id, &la_src->id);
  }
  else {
    la_dst->preview = nullptr;
  }
}

static void light_free_data(ID *id)
{
  Light *la = (Light *)id;

  /* is no lib link block, but light extension */
  if (la->nodetree) {
    blender::bke::node_tree_free_embedded_tree(la->nodetree);
    MEM_freeN(la->nodetree);
    la->nodetree = nullptr;
  }

  BKE_previewimg_free(&la->preview);
  BKE_icon_id_delete(&la->id);
  la->id.icon_id = 0;
}

static void light_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Light *lamp = reinterpret_cast<Light *>(id);

  if (lamp->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, BKE_library_foreach_ID_embedded(data, (ID **)&lamp->nodetree));
  }
}

static void light_foreach_working_space_color(ID *id, const IDTypeForeachColorFunctionCallback &fn)
{
  Light *la = (Light *)id;

  fn.single(&la->r);
}

static void light_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Light *la = (Light *)id;

  /* Forward compatibility for energy. */
  la->energy_deprecated = la->energy * exp2f(la->exposure);
  if (la->type == LA_AREA) {
    la->energy_deprecated /= M_PI_4;
  }

  /* write LibData */
  BLO_write_id_struct(writer, Light, id_address, &la->id);
  BKE_id_blend_write(writer, &la->id);

  /* Node-tree is integral part of lights, no libdata. */
  if (la->nodetree) {
    BLO_Write_IDBuffer temp_embedded_id_buffer{la->nodetree->id, writer};
    BLO_write_struct_at_address(writer, bNodeTree, la->nodetree, temp_embedded_id_buffer.get());
    blender::bke::node_tree_blend_write(
        writer, reinterpret_cast<bNodeTree *>(temp_embedded_id_buffer.get()));
  }

  BKE_previewimg_blend_write(writer, la->preview);
}

static void light_blend_read_data(BlendDataReader *reader, ID *id)
{
  Light *la = (Light *)id;

  BLO_read_struct(reader, PreviewImage, &la->preview);
  BKE_previewimg_blend_read(reader, la->preview);
}

IDTypeInfo IDType_ID_LA = {
    /*id_code*/ Light::id_type,
    /*id_filter*/ FILTER_ID_LA,
    /*dependencies_id_types*/ FILTER_ID_TE,
    /*main_listbase_index*/ INDEX_ID_LA,
    /*struct_size*/ sizeof(Light),
    /*name*/ "Light",
    /*name_plural*/ N_("lights"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_LIGHT,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ light_init_data,
    /*copy_data*/ light_copy_data,
    /*free_data*/ light_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ light_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ light_foreach_working_space_color,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ light_blend_write,
    /*blend_read_data*/ light_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

Light *BKE_light_add(Main *bmain, const char *name)
{
  Light *la;

  la = BKE_id_new<Light>(bmain, name);

  return la;
}

void BKE_light_eval(Depsgraph *depsgraph, Light *la)
{
  DEG_debug_print_eval(depsgraph, __func__, la->id.name, la);
}

float BKE_light_power(const Light &light)
{
  return light.energy * exp2f(light.exposure);
}

blender::float3 BKE_light_color(const Light &light)
{
  blender::float3 color(&light.r);

  if (light.mode & LA_USE_TEMPERATURE) {
    float temperature_color[4];
    IMB_colormanagement_blackbody_temperature_to_rgb(temperature_color, light.temperature);
    color *= blender::float3(temperature_color);
  }

  return color;
}

float BKE_light_area(const Light &light, const blender::float4x4 &object_to_world)
{
  /* Make illumination power constant. */
  switch (light.type) {
    case LA_AREA: {
      /* Rectangle area. */
      const blender::float3x3 scalemat = object_to_world.view<3, 3>();
      const blender::float3 scale = blender::math::to_scale(scalemat);

      const float size_x = light.area_size * scale.x;
      const float size_y = (ELEM(light.area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE) ?
                                light.area_sizey :
                                light.area_size) *
                           scale.y;

      float area = size_x * size_y;
      /* Scale for smaller area of the ellipse compared to the surrounding rectangle. */
      if (ELEM(light.area_shape, LA_AREA_DISK, LA_AREA_ELLIPSE)) {
        area *= float(M_PI / 4.0f);
      }
      return area;
    }
    case LA_LOCAL:
    case LA_SPOT: {
      /* Sphere area. For legacy reasons object scale is not taken into account
       * here, even though logically it should be. */
      const float radius = light.radius;
      return (radius > 0.0f) ? float(4.0f * M_PI) * blender::math::square(radius) : 4.0f;
    }
    case LA_SUN: {
      /* Sun disk area. */
      const float angle = light.sun_angle / 2.0f;
      return (angle > 0.0f) ? float(M_PI) * blender::math::square(sinf(angle)) : 1.0f;
    }
  }

  BLI_assert_unreachable();
  return 1.0f;
}
