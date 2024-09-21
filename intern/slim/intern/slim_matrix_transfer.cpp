/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_slim
 */

#include <Eigen/Dense>

#include "geometry_data_retrieval.h"
#include "slim.h"
#include "slim_matrix_transfer.h"

namespace slim {

MatrixTransferChart::MatrixTransferChart() = default;
MatrixTransferChart::MatrixTransferChart(MatrixTransferChart &&) = default;
MatrixTransferChart::~MatrixTransferChart() = default;
MatrixTransfer::MatrixTransfer() = default;
MatrixTransfer::~MatrixTransfer() = default;

void MatrixTransferChart::free_slim_data()
{
  data.reset(nullptr);
}

void MatrixTransfer::setup_slim_data(MatrixTransferChart &chart) const
{
  SLIMDataPtr slim_data = std::make_unique<SLIMDataPtr::element_type>();

  try {
    if (!chart.succeeded) {
      throw SlimFailedException();
    }

    GeometryData geometry_data(*this, chart);
    geometry_data.construct_slim_data(*slim_data, skip_initialization, reflection_mode);

    chart.pinned_vertices_num = geometry_data.number_of_pinned_vertices;
  }
  catch (SlimFailedException &) {
    slim_data->valid = false;
    chart.succeeded = false;
  }

  chart.data = std::move(slim_data);
}

}  // namespace slim
