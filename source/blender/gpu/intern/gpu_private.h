/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* gpu_backend.cc */

void gpu_backend_delete_resources(void);

/* gpu_pbvh.c */

void gpu_pbvh_init(void);
void gpu_pbvh_exit(void);

#ifdef __cplusplus
}
#endif
