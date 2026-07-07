/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "RNA_types.hh"

namespace blender {

namespace io::usd {

/** Data for registering USD IO hooks. */
struct USDHook {

  /* Identifier used for class name. */
  char idname[64];
  /* Identifier used as label. */
  char name[64];
  /* Short help/description. */
  char description[/*RNA_DYN_DESCR_MAX*/ 1024];

  /* rna_ext.data points to the USDHook class PyObject. */
  ExtensionRNA rna_ext;
};

void USD_register_hook(std::unique_ptr<USDHook> hook);
/**
 * Remove the given entry from the list of registered hooks and
 * free the allocated memory for the hook instance.
 */
void USD_unregister_hook(const USDHook *hook);
USDHook *USD_find_hook_name(const char idname[]);

};  // namespace io::usd

}  // namespace blender
