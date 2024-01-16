/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_vector.h"

#include "BKE_DerivedMesh.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "GPU_batch.h"
#include "GPU_immediate.h"
#include "GPU_shader.h"
#include "GPU_state.h"

#include "ED_mesh.hh"

#include "UI_resources.hh"

#include "DRW_engine.hh"

#include "view3d_intern.h" /* bad level include */

#ifdef VIEW3D_CAMERA_BORDER_HACK
uchar view3d_camera_border_hack_col[3];
bool view3d_camera_border_hack_test = false;
#endif
