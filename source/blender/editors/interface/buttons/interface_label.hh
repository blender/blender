/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup buttons
 */

#pragma once

namespace blender::ui {
struct Button;
struct ButtonLabel;

bool button_label_is_multiline(const Button *button);

/**
 * Wraps button text, this will try to reuse text wrap cache form last redraws.
 * \param icon_pad: Space used for drawing the button icon.
 */
void label_multiline_wrap_lines(ButtonLabel *button, int icon_pad);

}  // namespace blender::ui
