/* SPDX-FileCopyrightText: 2010-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#include "BLI_math_matrix.h"
#include "BLI_sys_types.h"

#include "BKE_object.h"

#include "TransformWriter.h"

void TransformWriter::add_joint_transform(COLLADASW::Node &node,
                                          float mat[4][4],
                                          float parent_mat[4][4],
                                          BCExportSettings &export_settings,
                                          bool has_restmat)
{
  float local[4][4];

  if (parent_mat) {
    float invpar[4][4];
    invert_m4_m4(invpar, parent_mat);
    mul_m4_m4m4(local, invpar, mat);
  }
  else {
    copy_m4_m4(local, mat);
  }

  if (!has_restmat && export_settings.get_apply_global_orientation()) {
    bc_apply_global_transform(local, export_settings.get_global_transform());
  }

  double dmat[4][4];
  UnitConverter::mat4_to_dae_double(dmat, local);

  if (export_settings.get_object_transformation_type() == BC_TRANSFORMATION_TYPE_MATRIX) {
    node.addMatrix("transform", dmat);
  }
  else {
    float loc[3], rot[3], scale[3];
    bc_decompose(local, loc, rot, nullptr, scale);
    add_transform(node, loc, rot, scale);
  }
}

void TransformWriter::add_node_transform_ob(COLLADASW::Node &node,
                                            Object *ob,
                                            BCExportSettings &export_settings)
{
  bool limit_precision = export_settings.get_limit_precision();

  /* Export the local Matrix (relative to the object parent,
   * be it an object, bone or vertices (one or more)). */
  Matrix f_obmat;
  BKE_object_matrix_local_get(ob, f_obmat);

  if (export_settings.get_apply_global_orientation()) {
    bc_apply_global_transform(f_obmat, export_settings.get_global_transform());
  }
  else {
    bc_add_global_transform(f_obmat, export_settings.get_global_transform());
  }

  switch (export_settings.get_object_transformation_type()) {
    case BC_TRANSFORMATION_TYPE_MATRIX: {
      double d_obmat[4][4];
      UnitConverter::mat4_to_dae_double(d_obmat, f_obmat);

      if (limit_precision) {
        BCMatrix::sanitize(d_obmat, LIMITTED_PRECISION);
      }
      node.addMatrix("transform", d_obmat);
      break;
    }
    case BC_TRANSFORMATION_TYPE_DECOMPOSED: {
      float loc[3], rot[3], scale[3];
      bc_decompose(f_obmat, loc, rot, nullptr, scale);
      if (limit_precision) {
        bc_sanitize_v3(loc, LIMITTED_PRECISION);
        bc_sanitize_v3(rot, LIMITTED_PRECISION);
        bc_sanitize_v3(scale, LIMITTED_PRECISION);
      }
      add_transform(node, loc, rot, scale);
      break;
    }
  }
}

void TransformWriter::add_node_transform_identity(COLLADASW::Node &node,
                                                  BCExportSettings &export_settings)
{
  BC_export_transformation_type transformation_type =
      export_settings.get_object_transformation_type();
  switch (transformation_type) {
    case BC_TRANSFORMATION_TYPE_MATRIX: {
      BCMatrix mat;
      DMatrix d_obmat;
      mat.get_matrix(d_obmat);
      node.addMatrix("transform", d_obmat);
      break;
    }
    default: {
      float loc[3] = {0.0f, 0.0f, 0.0f};
      float scale[3] = {1.0f, 1.0f, 1.0f};
      float rot[3] = {0.0f, 0.0f, 0.0f};
      add_transform(node, loc, rot, scale);
      break;
    }
  }
}

void TransformWriter::add_transform(COLLADASW::Node &node,
                                    const float loc[3],
                                    const float rot[3],
                                    const float scale[3])
{
  node.addScale("scale", scale[0], scale[1], scale[2]);
  node.addRotateZ("rotationZ", RAD2DEGF(rot[2]));
  node.addRotateY("rotationY", RAD2DEGF(rot[1]));
  node.addRotateX("rotationX", RAD2DEGF(rot[0]));
  node.addTranslate("location", loc[0], loc[1], loc[2]);
}
