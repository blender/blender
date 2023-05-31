/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct Panel;
struct bContext;

namespace blender::ed::spreadsheet {

void spreadsheet_data_set_panel_draw(const bContext *C, Panel *panel);

}  // namespace blender::ed::spreadsheet
