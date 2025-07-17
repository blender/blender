/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_string_ref.hh"

struct bContext;
struct uiLayout;
struct uiBut;
struct uiTooltipData;

/* names */
#define UI_MAX_DRAW_STR 550
#define UI_MAX_NAME_STR 256
#define UI_MAX_SHORTCUT_STR 64

/* Menu Callbacks */

using uiMenuCreateFunc = void (*)(bContext *C, uiLayout *layout, void *arg1);
using uiMenuHandleFunc = void (*)(bContext *C, void *arg, int event);

/**
 * Used for cycling menu values without opening the menu (Ctrl-Wheel).
 * \param direction: forward or backwards [1 / -1].
 * \param arg1: `uiBut.poin` (as with #uiMenuCreateFunc).
 * \return true when the button was changed.
 */
using uiMenuStepFunc = bool (*)(bContext *C, int direction, void *arg1);

using uiCopyArgFunc = void *(*)(const void *arg);
using uiFreeArgFunc = void (*)(void *arg);

/** Must return an allocated string. */
using uiButToolTipFunc = std::string (*)(bContext *C, void *argN, blender::StringRef tip);

/**
 * \param data: The tooltip data to be filled.
 * \param but: The exact button the tooltip is shown for. This is needed when the tooltip function
 *   is shared across multiple buttons but there still needs to be some customization per button.
 *   Mostly useful when using #uiLayoutSetTooltipCustomFunc.
 */
using uiButToolTipCustomFunc = void (*)(bContext &C, uiTooltipData &data, uiBut *but, void *argN);

namespace blender::ocio {
class Display;
}  // namespace blender::ocio
using ColorManagedDisplay = blender::ocio::Display;
