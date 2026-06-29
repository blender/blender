/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Functions for generating and handling "Session UIDs".
 *
 * Note that these are not true universally-unique identifiers, but only unique during the current
 * Blender session.
 *
 * For true UUIDs, see `BLI_uuid.hh`.
 */

#include "DNA_session_uid_types.h"

namespace blender {

/** Generate new UID which is unique throughout the Blender session. */
SessionUID BLI_session_uid_generate();

}  // namespace blender
