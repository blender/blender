/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup csv
 */

#pragma once

struct CSVImportParams;
struct PointCloud;

namespace blender::io::csv {

PointCloud *read_csv_file(const CSVImportParams &import_params);

}
