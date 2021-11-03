/*
 * Copyright 2011-2018 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_IES_H__
#define __UTIL_IES_H__

#include "util/string.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class IESFile {
 public:
  IESFile()
  {
  }
  ~IESFile();

  int packed_size();
  void pack(float *data);

  bool load(const string &ies);
  void clear();

 protected:
  bool parse(const string &ies);
  bool process();
  bool process_type_b();
  bool process_type_c();

  /* The brightness distribution is stored in spherical coordinates.
   * The horizontal angles correspond to theta in the regular notation
   * and always span the full range from 0° to 360°.
   * The vertical angles correspond to phi and always start at 0°. */
  vector<float> v_angles, h_angles;
  /* The actual values are stored here, with every entry storing the values
   * of one horizontal segment. */
  vector<vector<float>> intensity;

  /* Types of angle representation in IES files. Currently, only B and C are supported. */
  enum IESType { TYPE_A = 3, TYPE_B = 2, TYPE_C = 1 } type;
};

CCL_NAMESPACE_END

#endif /* __UTIL_IES_H__ */
