/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_span.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_geometry_set.hh"

#include "ED_curves.hh"

#include "MEM_guardedalloc.h"

#include "transform.hh"
#include "transform_convert.hh"

/* -------------------------------------------------------------------- */
/** \name Curve/Surfaces Transform Creation
 * \{ */

namespace blender::ed::transform::pointcloud {

struct PointCloudTransformData {
  IndexMaskMemory memory;
  IndexMask selection;

  Array<float3> positions;
  Array<float> radii;
};

static PointCloudTransformData *create_transform_custom_data(TransCustomData &custom_data)
{
  PointCloudTransformData *transform_data = MEM_new<PointCloudTransformData>(__func__);
  custom_data.data = transform_data;
  custom_data.free_cb = [](TransInfo *, TransDataContainer *, TransCustomData *custom_data) {
    PointCloudTransformData *data = static_cast<PointCloudTransformData *>(custom_data->data);
    MEM_delete(data);
    custom_data->data = nullptr;
  };
  return transform_data;
}

static void createTransPointCloudVerts(bContext * /*C*/, TransInfo *t)
{
  MutableSpan<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  const bool use_proportional_edit = (t->flag & T_PROP_EDIT_ALL) != 0;

  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    PointCloud &pointcloud = *static_cast<PointCloud *>(tc.obedit->data);
    bke::MutableAttributeAccessor attributes = pointcloud.attributes_for_write();
    PointCloudTransformData &transform_data = *create_transform_custom_data(tc.custom.type);
    const VArray selection_attr = *attributes.lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);
    if (use_proportional_edit) {
      transform_data.selection = IndexMask(pointcloud.totpoint);
      tc.data_len = transform_data.selection.size();
    }
    else {
      transform_data.selection = IndexMask::from_bools(selection_attr, transform_data.memory);
      tc.data_len = transform_data.selection.size();
    }
    if (tc.data_len == 0) {
      tc.custom.type.free_cb(t, &tc, &tc.custom.type);
      continue;
    }

    tc.data = MEM_calloc_arrayN<TransData>(tc.data_len, __func__);
    MutableSpan<TransData> tc_data = MutableSpan(tc.data, tc.data_len);

    transform_data.positions.reinitialize(tc.data_len);
    array_utils::gather(pointcloud.positions(),
                        transform_data.selection,
                        transform_data.positions.as_mutable_span());

    if (t->mode == TFM_CURVE_SHRINKFATTEN) {
      transform_data.radii.reinitialize(transform_data.selection.size());
      array_utils::gather(
          pointcloud.radius(), transform_data.selection, transform_data.radii.as_mutable_span());
    }

    const float4x4 &transform = tc.obedit->object_to_world();
    const float3x3 mtx_base = transform.view<3, 3>();
    const float3x3 smtx_base = math::pseudo_invert(mtx_base);

    threading::parallel_for(tc_data.index_range(), 1024, [&](const IndexRange range) {
      for (const int64_t i : range) {
        TransData &td = tc_data[i];
        float3 *elem = &transform_data.positions[i];
        copy_v3_v3(td.iloc, *elem);
        copy_v3_v3(td.center, *elem);
        td.loc = *elem;

        td.flag = 0;
        if (use_proportional_edit) {
          if (selection_attr[i]) {
            td.flag = TD_SELECTED;
          }
        }
        else {
          td.flag = TD_SELECTED;
        }

        if (t->mode == TFM_CURVE_SHRINKFATTEN) {
          float *value = &transform_data.radii[i];
          td.val = value;
          td.ival = *value;
        }

        copy_m3_m3(td.smtx, smtx_base.ptr());
        copy_m3_m3(td.mtx, mtx_base.ptr());
      }
    });
  }
}

static void recalcData_pointcloud(TransInfo *t)
{
  const Span<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  for (const TransDataContainer &tc : trans_data_contrainers) {
    const PointCloudTransformData &transform_data = *static_cast<PointCloudTransformData *>(
        tc.custom.type.data);
    PointCloud &pointcloud = *static_cast<PointCloud *>(tc.obedit->data);
    if (t->mode == TFM_CURVE_SHRINKFATTEN) {
      array_utils::scatter(
          transform_data.radii.as_span(), transform_data.selection, pointcloud.radius_for_write());
      pointcloud.tag_radii_changed();
    }
    else {
      array_utils::scatter(transform_data.positions.as_span(),
                           transform_data.selection,
                           pointcloud.positions_for_write());
      pointcloud.tag_positions_changed();
    }
    DEG_id_tag_update(&pointcloud.id, ID_RECALC_GEOMETRY);
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_PointCloud = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*create_trans_data*/ pointcloud::createTransPointCloudVerts,
    /*recalc_data*/ pointcloud::recalcData_pointcloud,
    /*special_aftertrans_update*/ nullptr,
};

}  // namespace blender::ed::transform::pointcloud
