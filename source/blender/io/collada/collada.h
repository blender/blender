/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <stdlib.h>

#include "ExportSettings.h"
#include "ImportSettings.h"

#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;

/*
 * both return 1 on success, 0 on error
 */
int collada_import(struct bContext *C, ImportSettings *import_settings);

int collada_export(struct bContext *C, ExportSettings *export_settings);

#ifdef __cplusplus
}
#endif
