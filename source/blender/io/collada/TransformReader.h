/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include "COLLADAFWMatrix.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWRotate.h"
#include "COLLADAFWScale.h"
#include "COLLADAFWTransformation.h"
#include "COLLADAFWTranslate.h"
#include "COLLADAFWUniqueId.h"
#include "Math/COLLADABUMathVector3.h"

#include "BLI_math.h"
#include "DNA_object_types.h"

#include "collada_internal.h"

// struct Object;

class TransformReader {
 protected:
  UnitConverter *unit_converter;

 public:
  struct Animation {
    Object *ob;
    COLLADAFW::Node *node;
    COLLADAFW::Transformation *tm; /* which transform is animated by an AnimationList->id */
  };

  TransformReader(UnitConverter *conv);

  void get_node_mat(float mat[4][4],
                    COLLADAFW::Node *node,
                    std::map<COLLADAFW::UniqueId, Animation> *animation_map,
                    Object *ob);
  void get_node_mat(float mat[4][4],
                    COLLADAFW::Node *node,
                    std::map<COLLADAFW::UniqueId, Animation> *animation_map,
                    Object *ob,
                    float parent_mat[4][4]);

  void dae_rotate_to_mat4(COLLADAFW::Transformation *tm, float m[4][4]);
  void dae_translate_to_mat4(COLLADAFW::Transformation *tm, float m[4][4]);
  void dae_scale_to_mat4(COLLADAFW::Transformation *tm, float m[4][4]);
  void dae_matrix_to_mat4(COLLADAFW::Transformation *tm, float m[4][4]);
  void dae_translate_to_v3(COLLADAFW::Transformation *tm, float v[3]);
  void dae_scale_to_v3(COLLADAFW::Transformation *tm, float v[3]);
  void dae_vector3_to_v3(const COLLADABU::Math::Vector3 &v3, float v[3]);
};
