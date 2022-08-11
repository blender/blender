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
 * The Original Code is Copyright (C) 2022 NVIDIA Corporation.
 * All rights reserved.
 */

#include "usd_skel_convert.h"

#include "usd.h"

#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/blendShape.h>
#include <pxr/usd/usdSkel/bindingAPI.h>

#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BLI_math_vector.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"

#include "BKE_main.h"
#include "BKE_scene.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.h"
#include "ED_keyframing.h"

#include <string>
#include <vector>

namespace usdtokens {
// Attribute names.
//static const pxr::TfToken color("color", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace {

FCurve *create_fcurve(int array_index, const char *rna_path)
{
  FCurve *fcu = BKE_fcurve_create();
  fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
  fcu->rna_path = BLI_strdupn(rna_path, strlen(rna_path));
  fcu->array_index = array_index;
  return fcu;
}

void add_bezt(FCurve *fcu,
              float frame,
              float value,
              eBezTriple_Interpolation ipo = BEZT_IPO_LIN)
{
  BezTriple bez;
  memset(&bez, 0, sizeof(BezTriple));
  bez.vec[1][0] = frame;
  bez.vec[1][1] = value;
  bez.ipo = ipo; /* use default interpolation mode here... */
  bez.f1 = bez.f2 = bez.f3 = SELECT;
  bez.h1 = bez.h2 = HD_AUTO;
  insert_bezt_fcurve(fcu, &bez, INSERTKEY_NOFLAGS);
  BKE_fcurve_handles_recalc(fcu);
}

}  // End anonymous namespace.

namespace blender::io::usd {

void test_create_shapekeys(Main *bmain, Object *obj)
{
  if (!(obj && obj->data && obj->type == OB_MESH)) {
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(obj->data);

  /* insert key to source mesh */
  Key *key = BKE_key_add(bmain, (ID *)mesh);
  key->type = KEY_RELATIVE;

  mesh->key = key;

  /* insert basis key */
  KeyBlock *kb = BKE_keyblock_add(key, "Basis");
  BKE_keyblock_convert_from_mesh(mesh, key, kb);

  kb = BKE_keyblock_add(key, "Key1");
  BKE_keyblock_convert_from_mesh(mesh, key, kb);

  float offsets[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
  BKE_keyblock_update_from_offset(obj, kb, (float(*)[3])&offsets);

  bAction *act = ED_id_action_ensure(bmain, (ID *)&key->id);

  FCurve *fcu = create_fcurve(0, "key_blocks[\"Key1\"].value");
  fcu->totvert = 3;

  add_bezt(fcu, 0.f, 0.f);
  add_bezt(fcu, 30.f, 1.f);
  add_bezt(fcu, 60.f, 0.3f);

  BLI_addtail(&act->curves, fcu);
}

void import_blendshapes(Main *bmain, Object *obj, pxr::UsdPrim prim)
{
  if (!(obj && obj->data && obj->type == OB_MESH && prim)) {
    return;
  }

  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(prim);

  if (!skel_api) {
    return;
  }

  if (!skel_api.GetBlendShapeTargetsRel().HasAuthoredTargets()) {
    return;
  }

  pxr::SdfPathVector targets;
  if (!skel_api.GetBlendShapeTargetsRel().GetTargets(&targets)) {
    std::cout << "Couldn't get blendshape targets for prim " << prim.GetPath() << std::endl;
    return;
  }

  if (targets.empty()) {
    return;
  }

  if (!skel_api.GetBlendShapesAttr().HasAuthoredValue()) {
    return;
  }

  pxr::VtTokenArray blendshapes;
  if (!skel_api.GetBlendShapesAttr().Get(&blendshapes)) {
    return;
  }

  if (blendshapes.empty()) {
    return;
  }

  if (targets.size() != blendshapes.size()) {
    std::cout << "Number of blendshapes doesn't match number of blendshape targets for prim " << prim.GetPath() << std::endl;
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(obj->data);

  /* insert key to source mesh */
  Key *key = BKE_key_add(bmain, (ID *)mesh);
  key->type = KEY_RELATIVE;

  mesh->key = key;

  /* insert basis key */
  KeyBlock *kb = BKE_keyblock_add(key, "Basis");
  BKE_keyblock_convert_from_mesh(mesh, key, kb);

  pxr::UsdStageRefPtr stage = prim.GetStage();

  if (!stage) {
    return;
  }

  /* Keep track of the shapkeys we're adding, for
   * validation when creating curves later. */
  std::set<pxr::TfToken> shapekey_names;

  for (int i = 0; i < targets.size(); ++i) {

    const pxr::SdfPath &path = targets[i];

    pxr::UsdSkelBlendShape blendshape(stage->GetPrimAtPath(path));

    if (!blendshape) {
      continue;
    }

    if (!blendshape.GetOffsetsAttr().HasAuthoredValue()) {
      continue;
    }

    pxr::VtVec3fArray offsets;
    if (!blendshape.GetOffsetsAttr().Get(&offsets)) {
      std::cout << "Couldn't get offsets for blendshape " << path << std::endl;
      continue;
    }

    shapekey_names.insert(blendshapes[i]);

    kb = BKE_keyblock_add(key, blendshapes[i].GetString().c_str());
    BKE_keyblock_convert_from_mesh(mesh, key, kb);

    pxr::VtArray<int> point_indices;
    if (blendshape.GetPointIndicesAttr().HasAuthoredValue()) {
      blendshape.GetPointIndicesAttr().Get(&point_indices);
    }

    float *fp = static_cast<float *>(kb->data);

    if (point_indices.empty()) {
      for (int a = 0; a < kb->totelem; ++a, fp += 3) {
        add_v3_v3(fp, offsets[a].data());
      }
    }
    else {
      int a = 0;
      for (int i : point_indices) {
        if (i < 0 || i > kb->totelem) {
          std::cout << "out of bounds point index " << i << std::endl;
          ++a;
          continue;
        }
        add_v3_v3(&fp[3 * i], offsets[a].data());
        ++a;
      }
    }
  }

  pxr::UsdSkelSkeleton skel_prim = skel_api.GetInheritedSkeleton();

  if (!skel_prim) {
    return;
  }

  skel_api = pxr::UsdSkelBindingAPI::Apply(skel_prim.GetPrim());

  if (!skel_api) {
    return;
  }

  pxr::UsdPrim anim_prim = skel_api.GetInheritedAnimationSource();

  if (!anim_prim) {
    return;
  }

  pxr::UsdSkelAnimation skel_anim(anim_prim);

  if (!skel_anim) {
    return;
  }

  if (!skel_anim.GetBlendShapesAttr().HasAuthoredValue()) {
    return;
  }

  pxr::UsdAttribute weights_attr = skel_anim.GetBlendShapeWeightsAttr();

  if (!(weights_attr && weights_attr.HasAuthoredValue())) {
    return;
  }

  std::vector<double> times;
  if (!weights_attr.GetTimeSamples(&times)) {
    return;
  }

  if (times.empty()) {
    return;
  }

  blendshapes;
  if (!skel_anim.GetBlendShapesAttr().Get(&blendshapes)) {
    return;
  }

  if (blendshapes.empty()) {
    return;
  }

  size_t num_samples = times.size();

  /* Create the animation and curves. */
  bAction *act = ED_id_action_ensure(bmain, (ID *)&key->id);
  std::vector<FCurve *> curves;

  for (auto blendshape_name : blendshapes) {

    if (shapekey_names.find(blendshape_name) == shapekey_names.end()) {
      /* We didn't create a shapekey fo this blendshape, so we don't
       * create a curve and insert a null placeholder in the curve array. */
      curves.push_back(nullptr);
      continue;
    }

    std::string rna_path = "key_blocks[\"" + blendshape_name.GetString() + "\"].value";
    FCurve *fcu = create_fcurve(0, rna_path.c_str());
    fcu->totvert = num_samples;
    curves.push_back(fcu);
    BLI_addtail(&act->curves, fcu);
  }

  for (double frame : times) {
    pxr::VtFloatArray weights;
    if (!weights_attr.Get(&weights, frame)) {
      std::cout << "Couldn't get blendshape weights for time " << frame << std::endl;
      continue;
    }

    if (weights.size() != curves.size()) {
      std::cout << "Programmer error: number of weight samples doesn't match number of shapekey curve entries for time " << time << std::endl;
      continue;
    }

    for (int wi = 0; wi < weights.size(); ++wi) {
      if (curves[wi] != nullptr) {
        add_bezt(curves[wi], frame, weights[wi]);
      }
    }
  }

}

}  // namespace blender::io::usd
