/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "subd/osd.h"

#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Mesh;
class Attribute;

/* Attribute Interpolation. */

class SubdAttributeInterpolation {
 protected:
  Mesh &mesh;
  int num_patches;
  vector<int> ptex_face_to_base_face;

#ifdef WITH_OPENSUBDIV
  OsdMesh &osd_mesh;
  OsdData &osd_data;
#endif

 public:
  SubdAttributeInterpolation(Mesh &mesh,
#ifdef WITH_OPENSUBDIV
                             OsdMesh &osd_mesh,
                             OsdData &osd_data,
#endif
                             const int num_patches)
      : mesh(mesh),
        num_patches(num_patches)
#ifdef WITH_OPENSUBDIV
        ,
        osd_mesh(osd_mesh),
        osd_data(osd_data)
#endif
  {
  }

  bool support_interp_attribute(const Attribute &attr) const;
  void interp_attribute(const Attribute &subd_attr, Attribute &mesh_attr);

 protected:
  /* PTex face id to base mesh face mapping. */
  const int *get_ptex_face_mapping();

  template<typename T>
  void interp_attribute_vertex_linear(const Attribute &subd_attr,
                                      Attribute &mesh_attr,
                                      const int motion_step = 0);
  template<typename T>
  void interp_attribute_corner_linear(const Attribute &subd_attr, Attribute &mesh_attr);

#ifdef WITH_OPENSUBDIV
  template<typename T>
  void interp_attribute_vertex_smooth(const Attribute &subd_attr,
                                      Attribute &mesh_attr,
                                      const int motion_step = 0);
  template<typename T>
  void interp_attribute_corner_smooth(Attribute &mesh_attr,
                                      const int channel,
                                      const vector<char> &merged_values);
#endif

  template<typename T>
  void interp_attribute_face(const Attribute &subd_attr, Attribute &mesh_attr);

  template<typename T>
  void interp_attribute_type(const Attribute &subd_attr, Attribute &mesh_attr);
};

CCL_NAMESPACE_END
