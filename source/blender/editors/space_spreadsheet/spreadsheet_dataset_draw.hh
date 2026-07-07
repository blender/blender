/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {

struct Panel;
struct bContext;

namespace ed::spreadsheet {

void spreadsheet_data_set_panel_draw(const bContext *C, Panel *panel);

}  // namespace ed::spreadsheet
}  // namespace blender
