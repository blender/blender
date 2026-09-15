/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup buttons
 */

#pragma once

#include <optional>
#include <variant>

#include "BLI_rect.hh"
#include "BLI_resource_scope.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::ui {

struct Block;
struct Button;
struct ButtonLabel;
struct Layout;

/**
 * After resolving a markdown label this represents a text span on a single line with a specific
 * format.
 */
struct MarkdownItemText {
  enum class CodeContext {
    None,
    CodeSpan,
    CodeBlock,
  };

  /**
   * Baseline position of the text in pixels.
   */
  float x = 0.0f;
  float y = 0.0f;
  float size_px = 0.0f;
  int fontid = 0;
  int weight = 400;
  bool italic = false;
  bool is_in_quote = false;
  CodeContext code_context = CodeContext::None;
  StringRef text;
  std::optional<StringRef> url;
};

/**
 * Represents a horizontal separator line. The position is determined during the markdown
 * layouting phase.
 */
struct MarkdownItemHorizontalRule {
  /** Vertical center of the line in pixels. */
  float y = 0.0f;
};

/**
 * Represents the vertical line drawn next to a quote block. The position is determined during the
 * markdown layouting phase.
 */
struct MarkdownItemQuoteLine {
  /** Top position of the bar in pixels. */
  float x = 0.0f;
  float y = 0.0f;
  /** Length of the bar extending downward. */
  float height = 0.0f;
};

/**
 * Represents the background box drawn behind a code block or inline code span. The position is
 * determined during the markdown layouting phase.
 */
struct MarkdownItemCodeBox {
  enum class Kind {
    Block,
    Inline,
  };

  rctf rect = {};
  Kind kind = Kind::Block;
};

/**
 * Markdown text is parsed into a list of #MarkdownItem values, each representing something that
 * needs to be rendered in the UI.
 */
using MarkdownItem = std::variant<MarkdownItemText,
                                  MarkdownItemHorizontalRule,
                                  MarkdownItemQuoteLine,
                                  MarkdownItemCodeBox>;

/**
 * The information that impacts the markdown layouting process. If this changes, the layout
 * generally changes too.
 */
struct MarkdownLayoutCacheKey {
  StringRef text;
  int wrap_width_px = 0;
  float pixels_per_point = 0.0f;
  int markdown_layout_generation = 0;

  bool operator==(const MarkdownLayoutCacheKey &other) const = default;
};

/** The actual generated layout based on the parsed text. */
struct MarkdownLayout {
  Vector<MarkdownItem> items;
  /** Height of laid-out content in pixels. */
  float content_height = 0.0f;
};

/**
 * Drawing markdown is split into two phases:
 * 1. The text is parsed into a #MarkdownLayout which contains rendering instructions. This happens
 *    as part of resolving the ui::Layout.
 * 2. The computed layout is drawn using the computed instructions.
 *
 * Importantly, the drawing instructions are reused across different redraws (unless e.g. the width
 * or font size changes).
 */
struct MarkdownLayoutCache {
  ResourceScope scope;
  MarkdownLayoutCacheKey key;
  MarkdownLayout md_layout;
};

bool button_label_is_markdown(const Button *button);

/**
 * Parses and lays out markdown for a label button, adjusting its height.
 * Tries to reuse layout cache from previous redraws.
 */
void label_markdown_resolve(ButtonLabel *button);

/**
 * Creates invisible link buttons for markdown labels in \a block.
 * Call after layout resolve so button positions are final.
 */
void label_markdown_create_link_buttons(Block *block);

/** Draws markdown label contents into \a rect (pixel space). */
void label_markdown_draw(const ButtonLabel *button, const uchar color[4], const rcti *rect);

void label_markdown_dev_config(Layout &layout);

}  // namespace blender::ui
