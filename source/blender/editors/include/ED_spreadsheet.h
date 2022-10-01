/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct ID;
struct Main;
struct Object;
struct SpaceNode;
struct SpaceSpreadsheet;
struct bContext;
struct bNode;

#ifdef __cplusplus
extern "C" {
#endif

struct ID *ED_spreadsheet_get_current_id(const struct SpaceSpreadsheet *sspreadsheet);

#ifdef __cplusplus
}
#endif
