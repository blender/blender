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

#ifndef __MATERIALEXPORTER_H__
#define __MATERIALEXPORTER_H__

#include <string>
#include <vector>

#include "COLLADASWLibraryMaterials.h"
#include "COLLADASWStreamWriter.h"

extern "C" {
#include "BKE_material.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
}

#include "GeometryExporter.h"
#include "collada_internal.h"
#include "ExportSettings.h"
#include "Materials.h"

class MaterialsExporter : COLLADASW::LibraryMaterials {
 public:
  MaterialsExporter(COLLADASW::StreamWriter *sw, BCExportSettings &export_settings);
  void exportMaterials(Scene *sce);
  void operator()(Material *ma, Object *ob);

 private:
  bool hasMaterials(Scene *sce);
  BCExportSettings &export_settings;
};

// used in forEachMaterialInScene
template<class Functor> class ForEachMaterialFunctor {
  std::vector<std::string>
      mMat;  // contains list of material names, to avoid duplicate calling of f
  Functor *f;

 public:
  ForEachMaterialFunctor(Functor *f) : f(f)
  {
  }

  void operator()(Object *ob)
  {
    int a;
    for (a = 0; a < ob->totcol; a++) {

      Material *ma = give_current_material(ob, a + 1);

      if (!ma)
        continue;

      std::string translated_id = translate_id(id_name(ma));
      if (find(mMat.begin(), mMat.end(), translated_id) == mMat.end()) {
        (*this->f)(ma, ob);

        mMat.push_back(translated_id);
      }
    }
  }
};

struct MaterialFunctor {
  // calls f for each unique material linked to each object in sce
  // f should have
  // void operator()(Material *ma)
  template<class Functor>
  void forEachMaterialInExportSet(Scene *sce, Functor &f, LinkNode *export_set)
  {
    ForEachMaterialFunctor<Functor> matfunc(&f);
    GeometryFunctor gf;
    gf.forEachMeshObjectInExportSet<ForEachMaterialFunctor<Functor>>(sce, matfunc, export_set);
  }
};

#endif
