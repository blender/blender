/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(_MSC_VER)
#  define API(returnType) extern "C" __declspec(dllexport) returnType __cdecl
#else
#  define API(returnType) extern "C" returnType
#endif
