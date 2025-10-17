/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"

/**
 * #UserDef.dupflag
 *
 * The flag tells #BKE_object_duplicate() whether to copy data linked to the object,
 * or to reference the existing data.
 * #U.dupflag should be used for default operations or you can construct a flag as Python does.
 * If #eDupli_ID_Flags is 0 then no data will be copied (linked duplicate).
 */
typedef enum eDupli_ID_Flags {
  USER_DUP_MESH = (1 << 0),
  USER_DUP_CURVE = (1 << 1),
  USER_DUP_SURF = (1 << 2),
  USER_DUP_FONT = (1 << 3),
  USER_DUP_MBALL = (1 << 4),
  USER_DUP_LAMP = (1 << 5),
  /* USER_DUP_FCURVE = (1 << 6), */ /* UNUSED, keep because we may implement. */
  USER_DUP_MAT = (1 << 7),
  /* USER_DUP_TEX = (1 << 8), */ /* UNUSED, keep because we may implement. */
  USER_DUP_ARM = (1 << 9),
  USER_DUP_ACT = (1 << 10),
  USER_DUP_PSYS = (1 << 11),
  USER_DUP_LIGHTPROBE = (1 << 12),
  USER_DUP_GPENCIL = (1 << 13),
  USER_DUP_CURVES = (1 << 14),
  USER_DUP_POINTCLOUD = (1 << 15),
  USER_DUP_VOLUME = (1 << 16),
  USER_DUP_LATTICE = (1 << 17),
  USER_DUP_CAMERA = (1 << 18),
  USER_DUP_SPEAKER = (1 << 19),
  USER_DUP_NTREE = (1 << 20),

  USER_DUP_OBDATA = (~0) & ((1 << 24) - 1),

  /* Those are not exposed as user preferences, only used internally. */
  USER_DUP_OBJECT = (1 << 24),
  /* USER_DUP_COLLECTION = (1 << 25), */ /* UNUSED, keep because we may implement. */

  /* Duplicate (and hence make local) linked data. */
  USER_DUP_LINKED_ID = (1 << 30),
} eDupli_ID_Flags;
ENUM_OPERATORS(eDupli_ID_Flags)
