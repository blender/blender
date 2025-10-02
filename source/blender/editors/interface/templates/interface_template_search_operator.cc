/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Search available operators by scanning all and checking their poll function.
 * accessed via the #WM_OT_search_operator operator.
 */

#include <cstring>
#include <fmt/format.h>

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "BKE_global.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Operator Search Template Implementation
 * \{ */

static void operator_search_exec_fn(bContext *C, void * /*arg1*/, void *arg2)
{
  wmOperatorType *ot = static_cast<wmOperatorType *>(arg2);

  if (ot) {
    WM_operator_name_call_ptr(C, ot, blender::wm::OpCallContext::InvokeDefault, nullptr, nullptr);
  }
}

static void operator_search_update_fn(const bContext *C,
                                      void * /*arg*/,
                                      const char *str,
                                      uiSearchItems *items,
                                      const bool /*is_first*/)
{
  /* Prepare BLI_string_all_words_matched. */
  const size_t str_len = strlen(str);
  const int words_max = BLI_string_max_possible_word_count(str_len);
  blender::Array<blender::int2> words(words_max);
  const int words_len = BLI_string_find_split_words(
      str, str_len, ' ', (int (*)[2])words.data(), words_max);

  for (wmOperatorType *ot : WM_operatortypes_registered_get()) {
    const char *ot_ui_name = CTX_IFACE_(ot->translation_context, ot->name);

    if ((ot->flag & OPTYPE_INTERNAL) && (G.debug & G_DEBUG_WM) == 0) {
      continue;
    }

    if (BLI_string_all_words_matched(ot_ui_name, str, (int (*)[2])words.data(), words_len)) {
      if (WM_operator_poll((bContext *)C, ot)) {
        std::string name = ot_ui_name;
        if (const std::optional<std::string> kmi_str = WM_key_event_operator_string(
                C, ot->idname, blender::wm::OpCallContext::ExecDefault, nullptr, true))
        {
          name += UI_SEP_CHAR;
          name += *kmi_str;
        }

        if (!UI_search_item_add(items, name, ot, ICON_NONE, 0, 0)) {
          break;
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Search Template API
 * \{ */

void UI_but_func_operator_search(uiBut *but)
{
  UI_but_func_search_set(but,
                         ui_searchbox_create_operator,
                         operator_search_update_fn,
                         nullptr,
                         false,
                         nullptr,
                         operator_search_exec_fn,
                         nullptr);
}

void uiTemplateOperatorSearch(uiLayout *layout)
{
  uiBlock *block;
  uiBut *but;
  static char search[256] = "";

  block = layout->block();
  blender::ui::block_layout_set_current(block, layout);

  but = uiDefSearchBut(
      block, search, 0, ICON_VIEWZOOM, sizeof(search), 0, 0, UI_UNIT_X * 6, UI_UNIT_Y, "");
  UI_but_func_operator_search(but);
}

/** \} */
