/* SPDX-FileCopyrightText: 2010-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  /* sun */
  if (la->type == LA_SUN) {
    COLLADASW::DirectionalLight cla(mSW, la_id, la_name);
    cla.setColor(col, false, "color");
    exportBlenderProfile(cla, la);
    addLight(cla);
  }

  /* spot */
  else if (la->type == LA_SPOT) {
    COLLADASW::SpotLight cla(mSW, la_id, la_name);
    cla.setColor(col, false, "color");
    cla.setFallOffAngle(RAD2DEGF(la->spotsize), false, "fall_off_angle");
    cla.setFallOffExponent(la->spotblend, false, "fall_off_exponent");
    exportBlenderProfile(cla, la);
    addLight(cla);
  }
  /* lamp */
  else if (la->type == LA_LOCAL) {
    COLLADASW::PointLight cla(mSW, la_id, la_name);
    cla.setColor(col, false, "color");
    exportBlenderProfile(cla, la);
    addLight(cla);
  }
  /* area light is not supported
   * it will be exported as a local lamp */
  else {
    COLLADASW::PointLight cla(mSW, la_id, la_name);
    cla.setColor(col, false, "color");
    exportBlenderProfile(cla, la);
    addLight(cla);
  }
}

bool LightsExporter::exportBlenderProfile(COLLADASW::Light &cla, Light *la)
{
  cla.addExtraTechniqueParameter("blender", "type", la->type);
  cla.addExtraTechniqueParameter("blender", "flag", la->flag);
  cla.addExtraTechniqueParameter("blender", "mode", la->mode);
  cla.addExtraTechniqueParameter("blender", "red", la->r);
  cla.addExtraTechniqueParameter("blender", "green", la->g);
  cla.addExtraTechniqueParameter("blender", "blue", la->b);
  cla.addExtraTechniqueParameter("blender", "shadow_r", la->shdwr, "blender_shadow_r");
  cla.addExtraTechniqueParameter("blender", "shadow_g", la->shdwg, "blender_shadow_g");
  cla.addExtraTechniqueParameter("blender", "shadow_b", la->shdwb, "blender_shadow_b");
  cla.addExtraTechniqueParameter("blender", "energy", la->energy, "blender_energy");
  cla.addExtraTechniqueParameter("blender", "spotsize", RAD2DEGF(la->spotsize));
  cla.addExtraTechniqueParameter("blender", "spotblend", la->spotblend);
  cla.addExtraTechniqueParameter("blender", "clipsta", la->clipsta);
  cla.addExtraTechniqueParameter("blender", "clipend", la->clipend);
  cla.addExtraTechniqueParameter("blender", "bias", la->bias);
  cla.addExtraTechniqueParameter("blender", "radius", la->radius);
  cla.addExtraTechniqueParameter("blender", "area_shape", la->area_shape);
  cla.addExtraTechniqueParameter("blender", "area_size", la->area_size);
  cla.addExtraTechniqueParameter("blender", "area_sizey", la->area_sizey);
  cla.addExtraTechniqueParameter("blender", "area_sizez", la->area_sizez);

  return true;
}
