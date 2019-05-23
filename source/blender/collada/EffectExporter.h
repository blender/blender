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
 */

/** \file
 * \ingroup collada
 */

#ifndef __EFFECTEXPORTER_H__
#define __EFFECTEXPORTER_H__

#include <string>
#include <vector>

#include "COLLADASWColorOrTexture.h"
#include "COLLADASWStreamWriter.h"
#include "COLLADASWSampler.h"
#include "COLLADASWLibraryEffects.h"

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ExportSettings.h"
#include "collada_utils.h"

class EffectsExporter : COLLADASW::LibraryEffects {
 public:
  EffectsExporter(COLLADASW::StreamWriter *sw,
                  BCExportSettings &export_settings,
                  KeyImageMap &key_image_map);
  void exportEffects(bContext *C, Scene *sce);

  void operator()(Material *ma, Object *ob);

  COLLADASW::ColorOrTexture createTexture(Image *ima,
                                          std::string &uv_layer_name,
                                          COLLADASW::Sampler *sampler
                                          /*COLLADASW::Surface *surface*/);

  COLLADASW::ColorOrTexture getcol(float r, float g, float b, float a);

 private:
  void set_shader_type(COLLADASW::EffectProfile &ep, Material *ma);
  void set_transparency(COLLADASW::EffectProfile &ep, Material *ma);
  void set_diffuse_color(COLLADASW::EffectProfile &ep, Material *ma);
  void set_reflectivity(COLLADASW::EffectProfile &ep, Material *ma);
  void set_emission(COLLADASW::EffectProfile &ep, Material *ma);
  void get_images(Material *ma, KeyImageMap &uid_image_map);
  void create_image_samplers(COLLADASW::EffectProfile &ep,
                             KeyImageMap &uid_image_map,
                             std::string &active_uv);

  void writeTextures(COLLADASW::EffectProfile &ep,
                     std::string &key,
                     COLLADASW::Sampler *sampler,
                     MTex *t,
                     Image *ima,
                     std::string &uvname);

  bool hasEffects(Scene *sce);

  BCExportSettings &export_settings;
  KeyImageMap &key_image_map;
  Scene *scene;
  bContext *mContext;
};

#endif
