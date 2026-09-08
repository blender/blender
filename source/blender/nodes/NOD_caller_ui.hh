/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include <optional>

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"

namespace blender {

struct bContext;
struct PointerRNA;
struct bNodeTreeInterfacePanel;
struct bNodeTreeInterfaceSocket;

namespace ui {
struct Layout;
}  // namespace ui

namespace nodes {

void draw_interface_panel_as_panel(
    const bContext &C,
    ui::Layout &layout,
    PointerRNA *properties_ptr,
    const bNodeTreeInterfacePanel &interface_panel,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_visible,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_active,
    FunctionRef<void(ui::Layout &,
                     const bNodeTreeInterfaceSocket &,
                     PointerRNA *,
                     const std::optional<StringRef>)> fn_draw_property_for_socket);

void draw_interface_panel_content(
    const bContext &C,
    ui::Layout &layout,
    PointerRNA *properties_ptr,
    const bNodeTreeInterfacePanel &interface_panel,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_visible,
    FunctionRef<bool(const bNodeTreeInterfaceSocket &)> fn_input_is_active,
    FunctionRef<void(ui::Layout &,
                     const bNodeTreeInterfaceSocket &,
                     PointerRNA *,
                     const std::optional<StringRef>)> fn_draw_property_for_socket,
    bool skip_first = false,
    std::optional<StringRef> parent_name = std::nullopt);
}  // namespace nodes
}  // namespace blender
