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

#include <string>

#include "COLLADASWColor.h"
#include "COLLADASWLight.h"

#include "BLI_math.h"

#include "LightExporter.h"
#include "collada_internal.h"

template<class Functor>
void forEachLightObjectInExportSet(Scene *sce, Functor &f, LinkNode *export_set)
{
  LinkNode *node;
  for (node = export_set; node; node = node->next) {
    Object *ob = (Object *)node->link;

    if (ob->type == OB_LAMP && ob->data) {
      f(ob);
    }
  }
}

LightsExporter::LightsExporter(COLLADASW::StreamWriter *sw, BCExportSettings &export_settings)
    : COLLADASW::LibraryLights(sw), export_settings(export_settings)
{
}

void LightsExporter::exportLights(Scene *sce)
{
  openLibrary();

  forEachLightObjectInExportSet(sce, *this, this->export_settings.get_export_set());

  closeLibrary();
}

void LightsExporter::operator()(Object *ob)
{
  Light *la = (Light *)ob->data;
  std::string la_id(get_light_id(ob));
  std::string la_name(id_name(la));
  COLLADASW::Color col(la->r * la->energy, la->g * la->energy, la->b * la->energy);
  float d, constatt, linatt, quadatt;

  d = la->dist;

  constatt = 1.0f;

  if (la->falloff_type == LA_FALLOFF_INVLINEAR) {
    linatt = 1.0f / d;
    quadatt = 0.0f;
  }
  else {
    linatt = 0.0f;
    quadatt = 1.0f / (d * d);
  }

  // sun
  if (la->type == LA_SUN) {
    COLLADASW::DirectionalLight cla(mSW, la_id, la_name);
    cla.setColor(col, false, "color");
    cla.setConstantAttenuation(constatt);
    exportBlenderProfile(cla, la);
    addLight(cla);
  }

  // spot
  else if (la->type == LA_SPOT) {
    COLLADASW::SpotLight cla(mSW, la_id, la_name);
    cla.setColor(col, false, "color");
    cla.setFallOffAngle(RAD2DEGF(la->spotsize), false, "fall_off_angle");
    cla.setFallOffExponent(la->spotblend, false, "fall_off_exponent");
    cla.setConstantAttenuation(constatt);
    cla.setLinearAttenuation(linatt);
    cla.setQuadraticAttenuation(quadatt);
    exportBlenderProfile(cla, la);
    addLight(cla);
  }
  // lamp
  else if (la->type == LA_LOCAL) {
    COLLADASW::PointLight cla(mSW, la_id, la_name);
    cla.setColor(col, false, "color");
    cla.setConstantAttenuation(constatt);
    cla.setLinearAttenuation(linatt);
    cla.setQuadraticAttenuation(quadatt);
    exportBlenderProfile(cla, la);
    addLight(cla);
  }
  // area light is not supported
  // it will be exported as a local lamp
  else {
    COLLADASW::PointLight cla(mSW, la_id, la_name);
    cla.setColor(col, false, "color");
    cla.setConstantAttenuation(constatt);
    cla.setLinearAttenuation(linatt);
    cla.setQuadraticAttenuation(quadatt);
    exportBlenderProfile(cla, la);
    addLight(cla);
  }
}

bool LightsExporter::exportBlenderProfile(COLLADASW::Light &cla, Light *la)
{
  cla.addExtraTechniqueParameter("blender", "type", la->type);
  cla.addExtraTechniqueParameter("blender", "flag", la->flag);
  cla.addExtraTechniqueParameter("blender", "mode", la->mode);
  cla.addExtraTechniqueParameter("blender", "gamma", la->k, "blender_gamma");
  cla.addExtraTechniqueParameter("blender", "red", la->r);
  cla.addExtraTechniqueParameter("blender", "green", la->g);
  cla.addExtraTechniqueParameter("blender", "blue", la->b);
  cla.addExtraTechniqueParameter("blender", "shadow_r", la->shdwr, "blender_shadow_r");
  cla.addExtraTechniqueParameter("blender", "shadow_g", la->shdwg, "blender_shadow_g");
  cla.addExtraTechniqueParameter("blender", "shadow_b", la->shdwb, "blender_shadow_b");
  cla.addExtraTechniqueParameter("blender", "energy", la->energy, "blender_energy");
  cla.addExtraTechniqueParameter("blender", "dist", la->dist, "blender_dist");
  cla.addExtraTechniqueParameter("blender", "spotsize", RAD2DEGF(la->spotsize));
  cla.addExtraTechniqueParameter("blender", "spotblend", la->spotblend);
  cla.addExtraTechniqueParameter("blender", "att1", la->att1);
  cla.addExtraTechniqueParameter("blender", "att2", la->att2);
  // \todo figure out how we can have falloff curve supported here
  cla.addExtraTechniqueParameter("blender", "falloff_type", la->falloff_type);
  cla.addExtraTechniqueParameter("blender", "clipsta", la->clipsta);
  cla.addExtraTechniqueParameter("blender", "clipend", la->clipend);
  cla.addExtraTechniqueParameter("blender", "bias", la->bias);
  cla.addExtraTechniqueParameter("blender", "soft", la->soft);
  cla.addExtraTechniqueParameter("blender", "bufsize", la->bufsize);
  cla.addExtraTechniqueParameter("blender", "samp", la->samp);
  cla.addExtraTechniqueParameter("blender", "buffers", la->buffers);
  cla.addExtraTechniqueParameter("blender", "area_shape", la->area_shape);
  cla.addExtraTechniqueParameter("blender", "area_size", la->area_size);
  cla.addExtraTechniqueParameter("blender", "area_sizey", la->area_sizey);
  cla.addExtraTechniqueParameter("blender", "area_sizez", la->area_sizez);

  return true;
}
