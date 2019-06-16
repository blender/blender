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

#include <map>
#include <set>

#include "COLLADASWEffectProfile.h"
#include "COLLADAFWColorOrTexture.h"

#include "EffectExporter.h"
#include "DocumentExporter.h"
#include "MaterialExporter.h"

#include "collada_internal.h"
#include "collada_utils.h"

extern "C" {
#include "DNA_mesh_types.h"
#include "DNA_world_types.h"

#include "BKE_collection.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
}

static std::string getActiveUVLayerName(Object *ob)
{
  Mesh *me = (Mesh *)ob->data;

  int num_layers = CustomData_number_of_layers(&me->ldata, CD_MLOOPUV);
  if (num_layers) {
    return std::string(bc_CustomData_get_active_layer_name(&me->ldata, CD_MLOOPUV));
  }

  return "";
}

EffectsExporter::EffectsExporter(COLLADASW::StreamWriter *sw,
                                 BCExportSettings &export_settings,
                                 KeyImageMap &key_image_map)
    : COLLADASW::LibraryEffects(sw), export_settings(export_settings), key_image_map(key_image_map)
{
}

bool EffectsExporter::hasEffects(Scene *sce)
{
  FOREACH_SCENE_OBJECT_BEGIN (sce, ob) {
    int a;
    for (a = 0; a < ob->totcol; a++) {
      Material *ma = give_current_material(ob, a + 1);

      // no material, but check all of the slots
      if (!ma) {
        continue;
      }

      return true;
    }
  }
  FOREACH_SCENE_OBJECT_END;
  return false;
}

void EffectsExporter::exportEffects(bContext *C, Scene *sce)
{
  if (hasEffects(sce)) {
    this->mContext = C;
    this->scene = sce;
    openLibrary();
    MaterialFunctor mf;
    mf.forEachMaterialInExportSet<EffectsExporter>(
        sce, *this, this->export_settings.get_export_set());

    closeLibrary();
  }
}

void EffectsExporter::set_shader_type(COLLADASW::EffectProfile &ep, Material *ma)
{
  /* XXX check if BLINN and PHONG can be supported as well */
  ep.setShaderType(COLLADASW::EffectProfile::LAMBERT);
}

void EffectsExporter::set_transparency(COLLADASW::EffectProfile &ep, Material *ma)
{
  double alpha = bc_get_alpha(ma);
  ep.setTransparency(alpha, false, "alpha");
}

void EffectsExporter::set_diffuse_color(COLLADASW::EffectProfile &ep, Material *ma)
{
  // get diffuse color
  COLLADASW::ColorOrTexture cot = bc_get_base_color(ma);
  ep.setDiffuse(cot, false, "diffuse");
}

void EffectsExporter::set_ambient(COLLADASW::EffectProfile &ep, Material *ma)
{
  // get diffuse color
  COLLADASW::ColorOrTexture cot = bc_get_ambient(ma);
  ep.setAmbient(cot, false, "ambient");
}
void EffectsExporter::set_specular(COLLADASW::EffectProfile &ep, Material *ma)
{
  // get diffuse color
  COLLADASW::ColorOrTexture cot = bc_get_specular(ma);
  ep.setSpecular(cot, false, "specular");
}
void EffectsExporter::set_reflective(COLLADASW::EffectProfile &ep, Material *ma)
{
  // get diffuse color
  COLLADASW::ColorOrTexture cot = bc_get_reflective(ma);
  ep.setReflective(cot, false, "reflective");
}

void EffectsExporter::set_reflectivity(COLLADASW::EffectProfile &ep, Material *ma)
{
  double reflectivity = bc_get_reflectivity(ma);
  ep.setReflectivity(reflectivity, false, "specular");
}

void EffectsExporter::set_emission(COLLADASW::EffectProfile &ep, Material *ma)
{
  COLLADASW::ColorOrTexture cot = bc_get_emission(ma);
  ep.setEmission(cot, false, "emission");
}

void EffectsExporter::set_ior(COLLADASW::EffectProfile &ep, Material *ma)
{
  double alpha = bc_get_ior(ma);
  ep.setIndexOfRefraction(alpha, false, "ior");
}

void EffectsExporter::set_shininess(COLLADASW::EffectProfile &ep, Material *ma)
{
  double shininess = bc_get_shininess(ma);
  ep.setShininess(shininess, false, "shininess");
}

void EffectsExporter::get_images(Material *ma, KeyImageMap &material_image_map)
{
  if (!ma->use_nodes) {
    return;
  }

  MaterialNode material = MaterialNode(mContext, ma, key_image_map);
  Image *image = material.get_diffuse_image();
  if (image == nullptr) {
    return;
  }

  std::string uid(id_name(image));
  std::string key = translate_id(uid);

  if (material_image_map.find(key) == material_image_map.end()) {
    material_image_map[key] = image;
    key_image_map[key] = image;
  }
}

void EffectsExporter::create_image_samplers(COLLADASW::EffectProfile &ep,
                                            KeyImageMap &material_image_map,
                                            std::string &active_uv)
{
  KeyImageMap::iterator iter;

  for (iter = material_image_map.begin(); iter != material_image_map.end(); iter++) {

    Image *image = iter->second;
    std::string uid(id_name(image));
    std::string key = translate_id(uid);

    COLLADASW::Sampler *sampler = new COLLADASW::Sampler(
        COLLADASW::Sampler::SAMPLER_TYPE_2D,
        key + COLLADASW::Sampler::SAMPLER_SID_SUFFIX,
        key + COLLADASW::Sampler::SURFACE_SID_SUFFIX);

    sampler->setImageId(key);

    ep.setDiffuse(createTexture(image, active_uv, sampler), false, "diffuse");
  }
}

void EffectsExporter::operator()(Material *ma, Object *ob)
{
  KeyImageMap material_image_map;

  openEffect(get_effect_id(ma));

  COLLADASW::EffectProfile ep(mSW);
  ep.setProfileType(COLLADASW::EffectProfile::COMMON);
  ep.openProfile();
  set_shader_type(ep, ma);

  COLLADASW::ColorOrTexture cot;

  set_diffuse_color(ep, ma);
  set_emission(ep, ma);
  set_ior(ep, ma);
  set_shininess(ep, ma);
  set_reflectivity(ep, ma);
  set_transparency(ep, ma);

  /* TODO: from where to get ambient, specular and reflective? */
  // set_ambient(ep, ma);
  // set_specular(ep, ma);
  // set_reflective(ep, ma);

  get_images(ma, material_image_map);
  std::string active_uv(getActiveUVLayerName(ob));
  create_image_samplers(ep, material_image_map, active_uv);

#if 0
  unsigned int a, b;
  for (a = 0, b = 0; a < tex_indices.size(); a++) {
    MTex *t = ma->mtex[tex_indices[a]];
    Image *ima = t->tex->ima;

    // Image not set for texture
    if (!ima) {
      continue;
    }

    std::string key(id_name(ima));
    key = translate_id(key);

    // create only one <sampler>/<surface> pair for each unique image
    if (im_samp_map.find(key) == im_samp_map.end()) {
      //<newparam> <sampler> <source>
      COLLADASW::Sampler sampler(COLLADASW::Sampler::SAMPLER_TYPE_2D,
                                 key + COLLADASW::Sampler::SAMPLER_SID_SUFFIX,
                                 key + COLLADASW::Sampler::SURFACE_SID_SUFFIX);
      sampler.setImageId(key);
      // copy values to arrays since they will live longer
      samplers[a] = sampler;

      // store pointers so they can be used later when we create <texture>s
      samp_surf[b] = &samplers[a];
      //samp_surf[b][1] = &surfaces[a];

      im_samp_map[key] = b;
      b++;
    }
  }

  for (a = 0; a < tex_indices.size(); a++) {
    MTex *t = ma->mtex[tex_indices[a]];
    Image *ima = t->tex->ima;

    if (!ima) {
      continue;
    }

    std::string key(id_name(ima));
    key = translate_id(key);
    int i = im_samp_map[key];
    std::string uvname = strlen(t->uvname) ? t->uvname : active_uv;
    COLLADASW::Sampler *sampler = (COLLADASW::Sampler *)
        samp_surf[i];  // possibly uninitialized memory ...
    writeTextures(ep, key, sampler, t, ima, uvname);
  }
#endif

  // performs the actual writing
  ep.addProfileElements();
  ep.addExtraTechniques(mSW);

  ep.closeProfile();
  closeEffect();
}

COLLADASW::ColorOrTexture EffectsExporter::createTexture(Image *ima,
                                                         std::string &uv_layer_name,
                                                         COLLADASW::Sampler *sampler
                                                         /*COLLADASW::Surface *surface*/)
{

  COLLADASW::Texture texture(translate_id(id_name(ima)));
  texture.setTexcoord(uv_layer_name);
  // texture.setSurface(*surface);
  texture.setSampler(*sampler);

  COLLADASW::ColorOrTexture cot(texture);
  return cot;
}

COLLADASW::ColorOrTexture EffectsExporter::getcol(float r, float g, float b, float a)
{
  COLLADASW::Color color(r, g, b, a);
  COLLADASW::ColorOrTexture cot(color);
  return cot;
}
