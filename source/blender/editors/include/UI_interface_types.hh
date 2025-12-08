/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_string_ref.hh"

struct bContext;

namespace blender::ui {
struct Block;
struct Button;
struct Layout;
struct TooltipData;

/* names */
#define UI_MAX_DRAW_STR 550
#define UI_MAX_NAME_STR 256
#define UI_MAX_SHORTCUT_STR 64

/* Menu Callbacks */

using MenuCreateFunc = void (*)(bContext *C, Layout *layout, void *arg1);
using MenuHandleFunc = void (*)(bContext *C, void *arg, int event);

/**
 * Used for cycling menu values without opening the menu (Ctrl-Wheel).
 * \param direction: forward or backwards [1 / -1].
 * \param arg1: `Button.poin` (as with #MenuCreateFunc).
 * \return true when the button was changed.
 */
using MenuStepFunc = bool (*)(bContext *C, int direction, void *arg1);

using CopyArgFunc = void *(*)(const void *arg);
using FreeArgFunc = void (*)(void *arg);

/** Must return an allocated string. */
using ButtonToolTipFunc = std::string (*)(bContext *C, void *argN, StringRef tip);

/**
 * \param data: The tooltip data to be filled.
 * \param but: The exact button the tooltip is shown for. This is needed when the tooltip function
 *   is shared across multiple buttons but there still needs to be some customization per button.
 *   Mostly useful when using #uiLayoutSetTooltipCustomFunc.
 */
using ButtonToolTipCustomFunc = void (*)(bContext &C, TooltipData &data, Button *but, void *argN);

}  // namespace blender::ui

namespace blender::ocio {
class Display;
}  // namespace blender::ocio
using ColorManagedDisplay = blender::ocio::Display;
