/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup prf
 *
 * A tiny wrapper around the TracyClient library profiling API which takes care of including the
 * Tracy header and exposing it via PRF_* macros. When building without Tracy enabled
 * the macros are evaluated to no-op.
 *
 * Important considerations:
 * - Any `name` arguments should be `ustr`s to ensure their lifetime is managed appropriately
 *
 * \see Tracy.hpp for a full list of supported macros
 * \see https://github.com/wolfpld/tracy/releases/latest/download/tracy.pdf
 */

#include <cstdint>

#ifdef WITH_TRACY
#  include <tracy/Tracy.hpp>
#endif

namespace blender {
/**
 * Set of category colors, chosen with color-blindness in mind.
 */
enum class ProfileCategory : uint32_t {
  /**
   * \note Do not use pure black (0x000000) as Tracy uses it to indicate "no user provided color".
   */
  Default = 0x666666,
  Core = 0x0088FE,
  Draw = 0x00C49F,
  Editor = 0xFFBB28,
  Unused_1 = 0xFF8042,
  Unused_2 = 0x8884D8,
};

#ifdef WITH_TRACY

/** Frame markers. */
#  define PRF_frame_mark FrameMark
#  define PRF_frame_mark_start(name) FrameMarkStart(name.c_str())
#  define PRF_frame_mark_end(name) FrameMarkEnd(name.c_str())

/** Profile the current scope, creating a Tracy zone. */
#  define PRF_scope(category) ZoneScopedC(uint32_t(category))
#  define PRF_scope_with_name(ui_name, category) ZoneScopedNC(ui_name, uint32_t(category))

/** Set the profiled zone's name on a per-call basis. */
#  define PRF_scope_set_dynamic_name(fmt, ...) ZoneNameF(fmt, ##__VA_ARGS__)

/** Attach a text string to the current zone (e.g. filename, object name). */
#  define PRF_scope_add_text(fmt, ...) ZoneTextF(fmt, ##__VA_ARGS__)

/** Attach a numeric value to the current zone. */
#  define PRF_scope_add_value(value) ZoneValue(value)

/**
 * Profile the current scope, creating a Tracy zone.
 *
 * The zone is attached to the lifetime of `var` (e.g. for nested scopes).
 */
#  define PRF_scope_var(var, category) ZoneNamedC(var, uint32_t(category), true)
#  define PRF_scope_var_with_name(var, ui_name, category) \
    ZoneNamedNC(var, ui_name, uint32_t(category), true)

/** Set the specified zone's name on a per-call basis. */
#  define PRF_scope_var_set_dynamic_name(var, fmt, ...) ZoneNameVF(var, fmt, ##__VA_ARGS__)

/** Attach a text string to the specified zone (e.g. filename, object name). */
#  define PRF_scope_var_add_text(var, fmt, ...) ZoneTextVF(var, fmt, ##__VA_ARGS__)

/** Attach a numeric value to the specified zone. */
#  define PRF_scope_var_add_value(var, value) ZoneValueV(var, value)

#  define PRF_memory_alloc(ptr, size) TracyAlloc(ptr, size)
#  define PRF_memory_free(ptr) TracyFree(ptr)

#else

#  define PRF_frame_mark
#  define PRF_frame_mark_start(name)
#  define PRF_frame_mark_end(name)

#  define PRF_scope(category)
#  define PRF_scope_with_name(ui_name, category)

#  define PRF_scope_set_dynamic_name(fmt, ...)
#  define PRF_scope_add_text(fmt, ...)
#  define PRF_scope_add_value(value)

#  define PRF_scope_var(var, category)
#  define PRF_scope_var_with_name(var, ui_name, category)

#  define PRF_scope_var_set_dynamic_name(var, fmt, ...)
#  define PRF_scope_var_add_text(var, fmt, ...)
#  define PRF_scope_var_add_value(var, value)

#  define PRF_memory_alloc(ptr, size)
#  define PRF_memory_free(ptr)

#endif

}  // namespace blender
