/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLF_api.hh"
#include "BLI_vector.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_prototypes.hh"

namespace blender::ui {

void context_path_add_generic(Vector<ContextPathItem> &path,
                              StructRNA &rna_type,
                              void *ptr,
                              const BIFIconID icon_override,
                              std::function<void(bContext &)> handle_func,
                              MenuCreateFunc menu_func,
                              void *menu_arg)
{
  /* Add the null check here to make calling functions less verbose. */
  if (!ptr) {
    return;
  }

  PointerRNA rna_ptr = RNA_pointer_create_discrete(nullptr, &rna_type, ptr);
  char name_buf[128], *name;
  name = RNA_struct_name_get_alloc(&rna_ptr, name_buf, sizeof(name_buf), nullptr);

  /* Use a blank icon by default to check whether to retrieve it automatically from the type. */
  const BIFIconID icon = icon_override == ICON_NONE ? RNA_struct_ui_icon(rna_ptr.type) :
                                                      icon_override;

  if (&rna_type == RNA_NodeTree) {
    ID *id = static_cast<ID *>(ptr);
    path.append({name, icon, ID_REAL_USERS(id), handle_func, menu_func, menu_arg});
  }
  else {
    path.append({name, icon, 1, handle_func, menu_func, menu_arg});
  }
  if (name != name_buf) {
    MEM_delete(name);
  }
}

/* -------------------------------------------------------------------- */
/** \name Breadcrumb Template
 * \{ */

void template_breadcrumbs(Layout &layout, Span<ContextPathItem> context_path)
{
  Layout &row = layout.row(true);
  layout.alignment_set(LayoutAlign::Left);

  for (const int i : context_path.index_range()) {
    Layout &sub_row = row.row(true);
    sub_row.alignment_set(LayoutAlign::Left);

    Button *but;
    int icon = context_path[i].icon;
    std::string name = context_path[i].name;
    if (context_path[i].handle_func) {
      /* Pulldown adds more left padding to icons, making them misalign with label icons. */
      sub_row.emboss_set(EmbossType::Pulldown);
      but = sub_row.button(name.c_str(), icon, context_path[i].handle_func);
    }
    else {
      but = uiItemL_ex(&sub_row, name.c_str(), icon, false, false);
    }
    button_icon_indicator_number_set(but, context_path[i].icon_indicator_number);

    if (context_path[i].menu_func) {
      sub_row.emboss_set(EmbossType::Pulldown);
      sub_row.menu_fn(
          "", ICON_RIGHTARROW_THIN, context_path[i].menu_func, context_path[i].menu_arg);
    }
    else if (i < context_path.size() - 1) {
      sub_row.label("", ICON_RIGHTARROW_THIN);
    }
  }
}

/** \} */

}  // namespace blender::ui
