/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#ifndef GPU_SHADER
#  pragma once
#endif

#define SUBDIV_GROUP_SIZE 64

/* Uniform buffer bindings */
#define SHADER_DATA_BUF_SLOT 0

/* Storage buffer bindings */
#define NORMALS_FINALIZE_VERTEX_NORMALS_BUF_SLOT 0
#define NORMALS_FINALIZE_VERTEX_LOOP_MAP_BUF_SLOT 1
#define NORMALS_FINALIZE_POS_NOR_BUF_SLOT 2
#define NORMALS_FINALIZE_CUSTOM_NORMALS_BUF_SLOT 0
