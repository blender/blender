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

#ifndef __TRANSFORMREADER_H__
#define __TRANSFORMREADER_H__

#include "COLLADAFWNode.h"
#include "COLLADAFWTransformation.h"
#include "COLLADAFWTranslate.h"
#include "COLLADAFWRotate.h"
#include "COLLADAFWScale.h"
#include "COLLADAFWMatrix.h"
#include "COLLADAFWUniqueId.h"
#include "Math/COLLADABUMathVector3.h"

#include "DNA_object_types.h"
#include "BLI_math.h"

#include "collada_internal.h"

// struct Object;

class TransformReader {
 protected:
  UnitConverter *unit_converter;

 public:
  struct Animation {
    Object *ob;
    COLLADAFW::Node *node;
    COLLADAFW::Transformation *tm;  // which transform is animated by an AnimationList->id
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

#endif
