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

#include "MaterialExporter.h"
#include "COLLADABUUtils.h"
#include "collada_internal.h"

MaterialsExporter::MaterialsExporter(COLLADASW::StreamWriter *sw,
                                     BCExportSettings &export_settings)
    : COLLADASW::LibraryMaterials(sw), export_settings(export_settings)
{
  /* pass */
}

void MaterialsExporter::exportMaterials(Scene *sce)
{
  if (hasMaterials(sce)) {
    openLibrary();

    MaterialFunctor mf;
    mf.forEachMaterialInExportSet<MaterialsExporter>(
        sce, *this, this->export_settings.get_export_set());

    closeLibrary();
  }
}

bool MaterialsExporter::hasMaterials(Scene *sce)
{
  LinkNode *node;
  for (node = this->export_settings.get_export_set(); node; node = node->next) {
    Object *ob = (Object *)node->link;
    int a;
    for (a = 0; a < ob->totcol; a++) {
      Material *ma = give_current_material(ob, a + 1);

      // no material, but check all of the slots
      if (!ma)
        continue;

      return true;
    }
  }
  return false;
}

void MaterialsExporter::operator()(Material *ma, Object *ob)
{
  std::string mat_name = encode_xml(id_name(ma));
  std::string mat_id = get_material_id(ma);
  std::string eff_id = get_effect_id(ma);

  openMaterial(mat_id, mat_name);
  addInstanceEffect(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, eff_id));

  closeMaterial();
}
