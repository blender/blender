/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gl_context.hh"

#ifndef NDEBUG
#  define GL_CHECK_RESOURCES(info) debug::check_gl_resources(info)
#else
#  define GL_CHECK_RESOURCES(info)
#endif

namespace blender::gpu::debug {

void raise_gl_error(const char *info);
void check_gl_error(const char *info);
void check_gl_resources(const char *info);
/**
 * This function needs to be called once per context.
 */
void init_gl_callbacks();

void object_label(GLenum type, GLuint object, const char *name);

}  // namespace blender::gpu::debug
