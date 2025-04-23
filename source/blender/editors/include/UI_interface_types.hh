/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_string_ref.hh"

struct bContext;
struct uiLayout;

/* names */
#define UI_MAX_DRAW_STR 400
#define UI_MAX_NAME_STR 128
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
