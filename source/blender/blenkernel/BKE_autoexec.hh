/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

/**
 * \param path: The path to check against.
 * \return Success
 */
bool BKE_autoexec_match(const char *path);

/**
 * \return True when scripts in the blend-file at `filepath` are trusted to run automatically
 * by default, used for "Trusted Source" when opening the file.
 */
bool BKE_autoexec_default_trust_source(const char *filepath);
