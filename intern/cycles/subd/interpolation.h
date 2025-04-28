/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "subd/osd.h"

#include "util/types.h"
#include "util/vector.h"

#include <cstddef>
#include <functional>

CCL_NAMESPACE_BEGIN

class Mesh;
class Attribute;

/* Attribute Interpolation. */

struct SubdAttribute {
  std::function<void(const int, const int, const int, const int *, const float2 *, const int)>
      interp;
  vector<char> refined_data;
};

class SubdAttributeInterpolation {
 protected:
  Mesh &mesh;

#ifdef WITH_OPENSUBDIV
  OsdMesh &osd_mesh;
  OsdData &osd_data;
#endif

 public:
  vector<SubdAttribute> vertex_attributes;
  vector<SubdAttribute> triangle_attributes;

#ifdef WITH_OPENSUBDIV
  SubdAttributeInterpolation(Mesh &mesh, OsdMesh &osd_mesh, OsdData &osd_data);
#else
  SubdAttributeInterpolation(Mesh &mesh);
#endif

  void setup();

 protected:
  bool support_interp_attribute(const Attribute &attr) const;
  void setup_attribute(const Attribute &subd_attr, Attribute &mesh_attr);

  template<typename T>
  void setup_attribute_vertex_linear(const Attribute &subd_attr,
                                     Attribute &mesh_attr,
                                     const int motion_step = 0);

  template<typename T>
  void setup_attribute_corner_linear(const Attribute &subd_attr, Attribute &mesh_attr);

#ifdef WITH_OPENSUBDIV
  template<typename T>
  void setup_attribute_vertex_smooth(const Attribute &subd_attr,
                                     Attribute &mesh_attr,
                                     const int motion_step = 0);

  template<typename T>
  void setup_attribute_corner_smooth(Attribute &mesh_attr,
                                     const int channel,
                                     const vector<char> &merged_values);
#endif

  template<typename T> void setup_attribute_face(const Attribute &subd_attr, Attribute &mesh_attr);

  template<typename T> void setup_attribute_type(const Attribute &subd_attr, Attribute &mesh_attr);
};

CCL_NAMESPACE_END
