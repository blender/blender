/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "IO_wavefront_obj.hh"

namespace blender::io::obj {

void importer_geometry(const OBJImportParams &import_params, Vector<bke::GeometrySet> &geometries);

void importer_main(bContext *C, const OBJImportParams &import_params);

}  // namespace blender::io::obj
