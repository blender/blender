/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Share between `interface/templates/` files.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_types.hh"

#include "UI_interface_layout.hh"

struct bContext;

#define CURVE_ZOOM_MAX (1.0f / 25.0f)
#define ERROR_LIBDATA_MESSAGE N_("Can't edit external library data")

/* Defines for templateID/TemplateSearch. */
#define TEMPLATE_SEARCH_TEXTBUT_MIN_WIDTH (UI_UNIT_X * 4)
#define TEMPLATE_SEARCH_TEXTBUT_HEIGHT UI_UNIT_Y

struct RNAUpdateCb {
  PointerRNA ptr = {};
  PropertyRNA *prop;
};

static inline void rna_update_cb(bContext &C, const RNAUpdateCb &cb)
{
  /* we call update here on the pointer property, this way the
   * owner of the curve mapping can still define its own update
   * and notifier, even if the CurveMapping struct is shared. */
  RNA_property_update(&C, &const_cast<PointerRNA &>(cb.ptr), cb.prop);
}

static inline void rna_update_cb(bContext *C, void *arg_cb, void * /*arg*/)
{
  RNAUpdateCb *cb = (RNAUpdateCb *)arg_cb;
  rna_update_cb(*C, *cb);
}

/* `interface_template.cc` */
int template_search_textbut_width(PointerRNA *ptr, PropertyRNA *name_prop);
int template_search_textbut_height();
/**
 * Add a block button for the search menu for templateID and templateSearch.
 */
void template_add_button_search_menu(const bContext *C,
                                     uiLayout *layout,
                                     uiBlock *block,
                                     PointerRNA *ptr,
                                     PropertyRNA *prop,
                                     uiBlockCreateFunc block_func,
                                     void *block_argN,
                                     std::optional<blender::StringRef> tip,
                                     const bool use_previews,
                                     const bool editable,
                                     const bool live_icon,
                                     uiButArgNFree func_argN_free_fn = MEM_freeN,
                                     uiButArgNCopy func_argN_copy_fn = MEM_dupallocN);

uiBlock *template_common_search_menu(const bContext *C,
                                     ARegion *region,
                                     uiButSearchUpdateFn search_update_fn,
                                     void *search_arg,
                                     uiButHandleFunc search_exec_fn,
                                     void *active_item,
                                     uiButSearchTooltipFn item_tooltip_fn,
                                     const int preview_rows,
                                     const int preview_cols,
                                     float scale);
