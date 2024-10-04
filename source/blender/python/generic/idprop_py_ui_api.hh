/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 */

#pragma once

struct IDProperty;

extern PyTypeObject BPy_IDPropertyUIManager_Type;

struct BPy_IDPropertyUIManager {
  PyObject_VAR_HEAD
  IDProperty *property;
};

void IDPropertyUIData_Init_Types();
