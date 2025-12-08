/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* Keep for 'Camera' versioning. */
/** Properties for DOF effect. */
struct GPUDOFSettings {
  /** Focal distance for depth of field. */
  float focus_distance = 0;
  float fstop = 0;
  float focal_length = 0;
  float sensor = 0;
  float rotation = 0;
  float ratio = 0;
  int num_blades = 0;
  int high_quality = 0;
};
