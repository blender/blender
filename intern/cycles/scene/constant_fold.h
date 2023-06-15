/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __CONSTANT_FOLD_H__
#define __CONSTANT_FOLD_H__

#include "kernel/svm/types.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

class Scene;
class ShaderGraph;
class ShaderInput;
class ShaderNode;
class ShaderOutput;

class ConstantFolder {
 public:
  ShaderGraph *const graph;
  ShaderNode *const node;
  ShaderOutput *const output;

  Scene *scene;

  ConstantFolder(ShaderGraph *graph, ShaderNode *node, ShaderOutput *output, Scene *scene);

  bool all_inputs_constant() const;

  /* Constant folding helpers */
  void make_constant(float value) const;
  void make_constant(float3 value) const;
  void make_constant_clamp(float value, bool clamp) const;
  void make_constant_clamp(float3 value, bool clamp) const;
  void make_zero() const;
  void make_one() const;

  /* Bypass node, relinking to another output socket. */
  void bypass(ShaderOutput *output) const;

  /* For closure nodes, discard node entirely or bypass to one of its inputs. */
  void discard() const;
  void bypass_or_discard(ShaderInput *input) const;

  /* Bypass or make constant, unless we can't due to clamp being true. */
  bool try_bypass_or_make_constant(ShaderInput *input, bool clamp = false) const;

  /* Test if shader inputs of the current nodes have fixed values. */
  bool is_zero(ShaderInput *input) const;
  bool is_one(ShaderInput *input) const;

  /* Specific nodes. */
  void fold_mix(NodeMix type, bool clamp) const;
  void fold_mix_color(NodeMix type, bool clamp_factor, bool clamp) const;
  void fold_mix_float(bool clamp_factor, bool clamp) const;
  void fold_math(NodeMathType type) const;
  void fold_vector_math(NodeVectorMathType type) const;
  void fold_mapping(NodeMappingType type) const;
};

CCL_NAMESPACE_END

#endif /* __CONSTANT_FOLD_H__ */
