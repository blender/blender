/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cfloat>
#include <md4c.h>

#include "BKE_context.hh"

#include "DNA_theme_types.h"
#include "DNA_userdef_types.h"

#include "BLF_api.hh"

#include "BLI_color.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_rect.hh"
#include "BLI_string_utf8.hh"

#include "BLT_translation.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "RNA_access.hh"

#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "interface_intern.hh"
#include "interface_label_markdown.hh"

#include <fmt/format.h>

namespace blender::ui {

struct MdBlockSpacing {
  float margin_top = 0.0f;
  float margin_bottom = 0.0f;
};

struct MdHeaderStyle {
  float size_factor = 1.0f;
  int weight = 400;
  bool italic = false;
  float margin_top = 0.0f;
  float margin_bottom = 0.0f;
};

struct MdListSpacing {
  float margin_top = 0.0f;
  float margin_bottom = 0.0f;
  float item_spacing_loose = 0.0f;
  float item_spacing_tight = 0.0f;
  float nested_margin_top = 0.0f;
};

static int markdown_layout_generation = 0;

namespace md_style {

static std::array<MdHeaderStyle, 6> headers = {
    MdHeaderStyle{.size_factor = 1.50f,
                  .weight = 700,
                  .italic = false,
                  .margin_top = 1.00f,
                  .margin_bottom = 0.50f},
    MdHeaderStyle{.size_factor = 1.40f,
                  .weight = 600,
                  .italic = false,
                  .margin_top = 0.50f,
                  .margin_bottom = 0.50f},
    MdHeaderStyle{.size_factor = 1.20f,
                  .weight = 600,
                  .italic = false,
                  .margin_top = 0.50f,
                  .margin_bottom = 0.25f},
    MdHeaderStyle{.size_factor = 1.10f,
                  .weight = 600,
                  .italic = false,
                  .margin_top = 0.50f,
                  .margin_bottom = 0.25f},
    MdHeaderStyle{.size_factor = 1.00f,
                  .weight = 600,
                  .italic = false,
                  .margin_top = 0.50f,
                  .margin_bottom = 0.25f},
    MdHeaderStyle{.size_factor = 1.00f,
                  .weight = 500,
                  .italic = false,
                  .margin_top = 0.50f,
                  .margin_bottom = 0.25f},
};

static MdBlockSpacing paragraph = {.margin_top = 0.25f, .margin_bottom = 0.50f};
static MdBlockSpacing quote = {.margin_top = 0.50f, .margin_bottom = 0.75f};
static float quote_bar_padding = 0.10f;
static MdBlockSpacing code_block = {.margin_top = 0.50f, .margin_bottom = 0.75f};
static float code_block_padding_x = 10.00f;
static float code_block_padding_y = 8.00f;
static float code_span_padding_x = 4.00f;
static float code_span_padding_y = 2.00f;
static MdBlockSpacing horizontal_rule = {.margin_top = 0.50f, .margin_bottom = 0.50f};

static MdListSpacing list = {.margin_top = 0.50f,
                             .margin_bottom = 1.00f,
                             .item_spacing_loose = 0.50f,
                             .item_spacing_tight = 0.25f,
                             .nested_margin_top = 0.25f};

static float list_indent_px = 18.00f;
static float list_marker_padding_px = 4.00f;
static float horizontal_rule_padding = 0.25f;

};  // namespace md_style

bool button_label_is_markdown(const Button *button)
{
  return button->type == ButtonType::Label &&
         static_cast<const ButtonLabel *>(button)->label_type == ButtonLabelType::Markdown;
}

static void markdown_apply_font(const int fontid,
                                const float size_px,
                                const int weight,
                                const bool italic)
{
  BLF_disable(fontid, BLF_ASPECT);
  BLF_size(fontid, size_px);
  BLF_character_weight(fontid, weight);
  if (italic) {
    BLF_enable(fontid, BLF_ITALIC);
  }
  else {
    BLF_disable(fontid, BLF_ITALIC);
  }
}

static void markdown_apply_item_font(const MarkdownItemText &item)
{
  markdown_apply_font(item.fontid, item.size_px, item.weight, item.italic);
}

static void markdown_disable_item_font(const MarkdownItemText &item)
{
  BLF_disable(item.fontid, BLF_ITALIC);
}

/**
 * Utility class used to take raw markdown as input and turn it into positions for text and other
 * elements. A class is used to simplify state management during the parsing process.
 */
class MarkdownLayouter {
 private:
  MarkdownLayoutCache &cache_;

  struct FontConfig {
    int fontid;
    float default_size;
    int default_weight;
    int bold_weight;
    float default_line_height_px;
  };

  FontConfig main_font_;
  FontConfig mono_font_;

  /** Horizontal position where the next text run starts. */
  float x_;
  /** Baseline of the current line box. */
  float y_;

  /** Adjacent and nested block margins collapse until the next line box is opened. */
  float pending_spacing_ = 0.0f;
  /** Bottom of laid out content, or empty when no vertical content has been added yet. */
  std::optional<float> flow_bottom_;

  struct LineBox {
    /** Top of the vertical area reserved for this rendered line. */
    float top;
    /** Greatest font line height encountered in this rendered line. */
    float height;
  };
  /** The line box that currently accepts text runs, if any. */
  std::optional<LineBox> current_line_box_;

  struct DocBlock {};

  struct HeaderBlock {
    /** Only values from 1 to 6 are valid. */
    int level = 1;
  };

  struct UnorderedListBlock {
    bool is_tight = false;
    /** Whether a direct child item has already started, used to add sibling item spacing. */
    bool has_item = false;
  };

  struct OrderedListBlock {
    bool is_tight = false;
    /** Whether a direct child item has already started, used to add sibling item spacing. */
    bool has_item = false;
    int next_number = 1;
    char mark_delimiter = '.';
  };

  struct ListItemBlock {
    /** Set for ordered list items; unset for unordered. */
    std::optional<int> number;
    /**
     * Exact marker position can only be known after the line next to it is known. The marker
     * position depends on the height of the line.
     */
    bool marker_pending = true;
  };

  struct ParagraphBlock {};

  struct CodeBlock {
    rctf bounds;
  };

  struct QuoteBlock {
    float start_y = 0.0f;
    float content_top = -FLT_MAX;
    float content_bottom = FLT_MAX;
  };

  struct HorizontalRuleBlock {};

  struct UnknownBlock {};

  struct EmSpan {};
  struct StrongSpan {};
  struct LinkSpan {
    StringRef url;
  };
  struct CodeSpan {
    Vector<rctf, 2> line_bounds;
    rctf current_bounds;
    bool padding_left_applied = false;
  };
  struct UnknownSpan {};

  using MdBlock = std::variant<DocBlock,
                               HeaderBlock,
                               UnorderedListBlock,
                               OrderedListBlock,
                               ListItemBlock,
                               ParagraphBlock,
                               CodeBlock,
                               QuoteBlock,
                               HorizontalRuleBlock,
                               UnknownBlock>;
  using MdSpan = std::variant<EmSpan, StrongSpan, LinkSpan, CodeSpan, UnknownSpan>;

  Vector<MdBlock, 16> block_stack_;
  Vector<MdSpan, 16> span_stack_;

 public:
  MarkdownLayouter(MarkdownLayoutCache &cache_) : cache_(cache_)
  {
    const uiFontStyle &widget_style = style_get()->widget;
    main_font_ = this->make_font_config(
        widget_style.uifont_id, widget_style.points, widget_style.character_weight, 700);
    mono_font_ = this->make_font_config(
        blf_mono_font, widget_style.points, widget_style.character_weight, 700);
    x_ = 0.0f;
    y_ = 0.0f;
  }

  FontConfig make_font_config(const int fontid,
                              const float default_size,
                              const int default_weight,
                              const int bold_weight)
  {
    FontConfig config;
    config.fontid = fontid;
    config.default_size = default_size;
    config.default_weight = default_weight;
    config.bold_weight = bold_weight;
    markdown_apply_font(fontid, default_size * cache_.key.pixels_per_point, default_weight, false);
    config.default_line_height_px = BLF_height_max(fontid);
    return config;
  }

  void compute_layout()
  {
    MD_PARSER md_parser{};
    md_parser.enter_block = [](MD_BLOCKTYPE block_type, void *detail, void *userdata) {
      static_cast<MarkdownLayouter *>(userdata)->md_enter_block(block_type, detail);
      return 0;
    };
    md_parser.leave_block = [](MD_BLOCKTYPE block_type, void *detail, void *userdata) {
      static_cast<MarkdownLayouter *>(userdata)->md_leave_block(block_type, detail);
      return 0;
    };
    md_parser.enter_span = [](MD_SPANTYPE span_type, void *detail, void *userdata) {
      static_cast<MarkdownLayouter *>(userdata)->md_enter_span(span_type, detail);
      return 0;
    };
    md_parser.leave_span = [](MD_SPANTYPE span_type, void *detail, void *userdata) {
      static_cast<MarkdownLayouter *>(userdata)->md_leave_span(span_type, detail);
      return 0;
    };
    md_parser.text = [](MD_TEXTTYPE text_type, const char *text, uint32_t length, void *userdata) {
      static_cast<MarkdownLayouter *>(userdata)->md_text(text_type, StringRef(text, length));
      return 0;
    };

    const StringRef text = cache_.key.text;
    md_parse(text.data(), text.size(), &md_parser, this);
    this->finalize_vertical_positions();
  }

  void add_pending_spacing(const float spacing)
  {
    pending_spacing_ = std::max(pending_spacing_, this->spacing_px(spacing));
  }

  void line_box_end()
  {
    if (!current_line_box_) {
      return;
    }
    flow_bottom_ = current_line_box_->top - current_line_box_->height;
    current_line_box_.reset();
  }

  void add_line_break()
  {
    if (current_line_box_) {
      this->line_box_end();
      return;
    }
    const float top = flow_bottom_.value_or(0.0f) - pending_spacing_;
    flow_bottom_ = top - main_font_.default_line_height_px;
    pending_spacing_ = 0.0f;
  }

  void md_enter_block(const MD_BLOCKTYPE block_type, void *detail)
  {
    switch (block_type) {
      case MD_BLOCK_QUOTE: {
        block_stack_.append(QuoteBlock{});
        break;
      }
      case MD_BLOCK_UL: {
        const auto *ul_detail = static_cast<const MD_BLOCK_UL_DETAIL *>(detail);
        block_stack_.append(UnorderedListBlock{bool(ul_detail->is_tight)});
        break;
      }
      case MD_BLOCK_OL: {
        const auto *ol_detail = static_cast<const MD_BLOCK_OL_DETAIL *>(detail);
        block_stack_.append(OrderedListBlock{bool(ol_detail->is_tight),
                                             false,
                                             int(ol_detail->start),
                                             char(ol_detail->mark_delimiter)});
        break;
      }
      case MD_BLOCK_LI: {
        ListItemBlock list_item;
        if (!block_stack_.is_empty()) {
          if (auto *ol = std::get_if<OrderedListBlock>(&block_stack_.last())) {
            list_item.number = ol->next_number++;
            if (ol->has_item) {
              this->add_pending_spacing(ol->is_tight ? md_style::list.item_spacing_tight :
                                                       md_style::list.item_spacing_loose);
            }
            ol->has_item = true;
          }
          else if (auto *ul = std::get_if<UnorderedListBlock>(&block_stack_.last())) {
            if (ul->has_item) {
              this->add_pending_spacing(ul->is_tight ? md_style::list.item_spacing_tight :
                                                       md_style::list.item_spacing_loose);
            }
            ul->has_item = true;
          }
        }
        block_stack_.append(list_item);
        break;
      }
      case MD_BLOCK_H: {
        const auto *h_detail = static_cast<const MD_BLOCK_H_DETAIL *>(detail);
        block_stack_.append(HeaderBlock{std::clamp<int>(h_detail->level, 1, 6)});
        break;
      }
      case MD_BLOCK_CODE: {
        CodeBlock code_block;
        BLI_rctf_init_minmax(&code_block.bounds);
        block_stack_.append(code_block);
        break;
      }
      case MD_BLOCK_P: {
        block_stack_.append(ParagraphBlock{});
        break;
      }
      case MD_BLOCK_DOC: {
        block_stack_.append(DocBlock{});
        break;
      }
      case MD_BLOCK_HR: {
        block_stack_.append(HorizontalRuleBlock{});
        break;
      }
      case MD_BLOCK_HTML:
      case MD_BLOCK_TABLE:
      case MD_BLOCK_THEAD:
      case MD_BLOCK_TBODY:
      case MD_BLOCK_TR:
      case MD_BLOCK_TH:
      case MD_BLOCK_TD: {
        /* Not implemented. */
        block_stack_.append(UnknownBlock{});
        break;
      }
    }

    if (!std::holds_alternative<DocBlock>(block_stack_.last())) {
      this->line_box_end();
    }

    if (flow_bottom_) {
      this->add_pending_spacing(this->block_margin_top(block_stack_.last()));
    }
    if (std::holds_alternative<CodeBlock>(block_stack_.last())) {
      pending_spacing_ += this->code_block_padding_y_px();
    }
    if (is_list_container_block(block_stack_.last()) && this->is_direct_child_of_list_item()) {
      this->add_pending_spacing(md_style::list.nested_margin_top);
    }
    x_ = this->context_block_start_x();

    switch (block_type) {
      case MD_BLOCK_HR: {
        this->append_horizontal_rule();
        break;
      }
      case MD_BLOCK_QUOTE: {
        std::get<QuoteBlock>(block_stack_.last()).start_y = flow_bottom_.value_or(0.0f) -
                                                            pending_spacing_;
        break;
      }
      default: {
        break;
      }
    }
  }

  float list_indent_px() const
  {
    return md_style::list_indent_px * cache_.key.pixels_per_point;
  }

  float code_block_padding_x_px() const
  {
    return md_style::code_block_padding_x * cache_.key.pixels_per_point;
  }

  float code_block_padding_y_px() const
  {
    return md_style::code_block_padding_y * cache_.key.pixels_per_point;
  }

  float code_span_padding_x_px() const
  {
    return md_style::code_span_padding_x * cache_.key.pixels_per_point;
  }

  float code_span_padding_y_px() const
  {
    return md_style::code_span_padding_y * cache_.key.pixels_per_point;
  }

  CodeSpan *mutable_current_code_span()
  {
    for (const int i : span_stack_.index_range()) {
      MdSpan &md_span = span_stack_[span_stack_.size() - 1 - i];
      if (auto *code_span = std::get_if<CodeSpan>(&md_span)) {
        return code_span;
      }
    }
    return nullptr;
  }

  float context_block_start_x() const
  {
    float indentation = 0.0f;
    const float indent = this->list_indent_px();
    for (const auto &md_block : block_stack_) {
      if (std::holds_alternative<QuoteBlock>(md_block)) {
        indentation += indent;
      }
      if (std::holds_alternative<ListItemBlock>(md_block)) {
        indentation += indent;
      }
    }
    return indentation;
  }

  float context_content_start_x() const
  {
    float x = this->context_block_start_x();
    if (this->is_in_fenced_code_block()) {
      x += this->code_block_padding_x_px();
    }
    return x;
  }

  float context_content_end_x() const
  {
    float x = float(cache_.key.wrap_width_px);
    if (this->is_in_fenced_code_block()) {
      x -= this->code_block_padding_x_px();
    }
    return x;
  }

  static StringRef unordered_bullet_for_level(const int level)
  {
    switch (level % 3) {
      case 1:
        return "◦";
      case 2:
        return "▪";
      default:
        return "•";
    }
  }

  void append_pending_list_marker()
  {
    for (const int64_t list_item_index : block_stack_.index_range()) {
      auto *list_item = std::get_if<ListItemBlock>(&block_stack_[list_item_index]);
      if (!list_item || !list_item->marker_pending) {
        continue;
      }
      list_item->marker_pending = false;

      MarkdownItemText item;
      item.size_px = main_font_.default_size * cache_.key.pixels_per_point;
      item.fontid = main_font_.fontid;
      item.weight = main_font_.default_weight;
      item.italic = false;
      item.is_in_quote = this->is_in_quote();
      item.y = y_;

      int unordered_list_level = -1;
      char ordered_delimiter = '.';
      float content_x = 0.0f;
      for (const int64_t ancestor_index : IndexRange(list_item_index + 1)) {
        const MdBlock &ancestor = block_stack_[ancestor_index];
        if (std::holds_alternative<QuoteBlock>(ancestor) ||
            std::holds_alternative<ListItemBlock>(ancestor))
        {
          content_x += this->list_indent_px();
        }
        if (std::holds_alternative<UnorderedListBlock>(ancestor)) {
          unordered_list_level++;
        }
        if (const auto *ordered_list = std::get_if<OrderedListBlock>(&ancestor)) {
          ordered_delimiter = ordered_list->mark_delimiter;
        }
      }

      if (list_item->number) {
        item.text = cache_.scope.allocator().copy_string(
            fmt::format("{}{}", *list_item->number, ordered_delimiter));
      }
      else {
        item.text = unordered_bullet_for_level(std::max(unordered_list_level, 0));
      }

      markdown_apply_item_font(item);
      const float marker_width = BLF_width(item.fontid, item.text.data(), item.text.size());
      markdown_disable_item_font(item);

      if (list_item->number) {
        /* Right-align the number towards the content. */
        item.x = content_x - marker_width -
                 md_style::list_marker_padding_px * cache_.key.pixels_per_point;
      }
      else {
        /* Center the bullet in the list-item indent gutter. */
        item.x = content_x - this->list_indent_px() * 0.5f - marker_width * 0.5f;
      }
      cache_.md_layout.items.append(item);
      this->track_quote_text_bounds(item);
    }
  }

  void append_horizontal_rule()
  {
    const float padding = this->spacing_px(md_style::horizontal_rule_padding);
    const float rule_top = flow_bottom_.value_or(0.0f) - pending_spacing_ - padding;
    constexpr float rule_height = 1.0f;
    cache_.md_layout.items.append(MarkdownItemHorizontalRule{rule_top - rule_height * 0.5f});
    flow_bottom_ = rule_top - rule_height - padding;
    pending_spacing_ = 0.0f;
  }

  void track_quote_text_bounds(const MarkdownItemText &item)
  {
    if (!this->is_in_quote()) {
      return;
    }

    markdown_apply_item_font(item);
    const float top = item.y + BLF_ascender(item.fontid);
    const float bottom = item.y + BLF_descender(item.fontid);
    markdown_disable_item_font(item);

    for (MdBlock &md_block : block_stack_) {
      if (auto *quote = std::get_if<QuoteBlock>(&md_block)) {
        quote->content_top = std::max(quote->content_top, top);
        quote->content_bottom = std::min(quote->content_bottom, bottom);
      }
    }
  }

  void track_code_text_bounds(const MarkdownItemText &item)
  {
    markdown_apply_item_font(item);
    const float top = item.y + BLF_ascender(item.fontid);
    const float bottom = item.y + BLF_descender(item.fontid);
    const float right = item.x + BLF_width(item.fontid, item.text.data(), item.text.size());
    markdown_disable_item_font(item);

    const rctf part{.xmin = item.x, .xmax = right, .ymin = bottom, .ymax = top};

    if (this->is_in_fenced_code_block()) {
      for (MdBlock &md_block : block_stack_) {
        if (auto *code_block = std::get_if<CodeBlock>(&md_block)) {
          BLI_rctf_union(&code_block->bounds, &part);
        }
      }
    }

    if (item.code_context == MarkdownItemText::CodeContext::CodeSpan) {
      if (CodeSpan *code_span = this->mutable_current_code_span()) {
        BLI_rctf_union(&code_span->current_bounds, &part);
      }
    }
  }

  void apply_code_span_left_padding()
  {
    CodeSpan *code_span = this->mutable_current_code_span();
    if (!code_span || code_span->padding_left_applied) {
      return;
    }
    x_ += this->code_span_padding_x_px();
    code_span->padding_left_applied = true;
  }

  void finalize_code_span_line_bounds()
  {
    CodeSpan *code_span = this->mutable_current_code_span();
    if (!code_span || BLI_rctf_is_empty(&code_span->current_bounds)) {
      return;
    }
    code_span->line_bounds.append(code_span->current_bounds);
    BLI_rctf_init_minmax(&code_span->current_bounds);
  }

  void append_code_span_boxes(const CodeSpan &code_span)
  {
    const float padding_x = this->code_span_padding_x_px();
    const float padding_y = this->code_span_padding_y_px();

    for (const rctf &bounds : code_span.line_bounds) {
      if (BLI_rctf_is_empty(&bounds)) {
        continue;
      }

      MarkdownItemCodeBox box;
      box.kind = MarkdownItemCodeBox::Kind::Inline;
      box.rect.xmin = bounds.xmin - padding_x;
      box.rect.xmax = bounds.xmax + padding_x;
      box.rect.ymax = bounds.ymax + padding_y;
      box.rect.ymin = bounds.ymin - padding_y;
      cache_.md_layout.items.append(box);
    }
  }

  void extend_flow_bottom_for_code_block(const CodeBlock &code_block)
  {
    if (BLI_rctf_is_empty(&code_block.bounds)) {
      return;
    }

    const float box_bottom = code_block.bounds.ymin - this->code_block_padding_y_px();
    if (!flow_bottom_) {
      flow_bottom_ = box_bottom;
      return;
    }
    flow_bottom_ = std::min(*flow_bottom_, box_bottom);
  }

  void append_code_block_box()
  {
    const auto *code_block = std::get_if<CodeBlock>(&block_stack_.last());
    if (!code_block || BLI_rctf_is_empty(&code_block->bounds)) {
      return;
    }

    const float padding_y = this->code_block_padding_y_px();

    MarkdownItemCodeBox box;
    box.kind = MarkdownItemCodeBox::Kind::Block;
    box.rect.xmin = this->context_block_start_x();
    box.rect.xmax = float(cache_.key.wrap_width_px);
    box.rect.ymax = code_block->bounds.ymax + padding_y;
    box.rect.ymin = code_block->bounds.ymin - padding_y;
    cache_.md_layout.items.append(box);
  }

  void append_quote_line()
  {
    const auto *quote = std::get_if<QuoteBlock>(&block_stack_.last());
    BLI_assert(quote);
    if (!quote) {
      return;
    }

    float top;
    float bottom;
    if (quote->content_top != -FLT_MAX) {
      const float padding = md_style::quote_bar_padding * main_font_.default_line_height_px;
      top = quote->content_top + padding;
      bottom = quote->content_bottom - padding;
    }
    else {
      top = quote->start_y;
      bottom = y_;
    }

    const float height = top - bottom;
    if (height <= 0.0f) {
      return;
    }
    MarkdownItemQuoteLine line;
    line.x = this->context_block_start_x() - this->list_indent_px() * 0.5f;
    line.y = top;
    line.height = height;
    cache_.md_layout.items.append(line);
  }

  bool is_in_quote() const
  {
    for (const auto &md_block : block_stack_) {
      if (std::holds_alternative<QuoteBlock>(md_block)) {
        return true;
      }
    }
    return false;
  }

  bool is_in_fenced_code_block() const
  {
    for (const MdBlock &md_block : block_stack_) {
      if (std::holds_alternative<CodeBlock>(md_block)) {
        return true;
      }
    }
    return false;
  }

  bool has_code_span() const
  {
    for (const MdSpan &md_span : span_stack_) {
      if (std::holds_alternative<CodeSpan>(md_span)) {
        return true;
      }
    }
    return false;
  }

  bool is_code() const
  {
    return this->is_in_fenced_code_block() || this->has_code_span();
  }

  static bool is_list_container_block(const MdBlock &block)
  {
    return std::holds_alternative<UnorderedListBlock>(block) ||
           std::holds_alternative<OrderedListBlock>(block);
  }

  bool is_direct_child_of_list_item() const
  {
    if (block_stack_.size() < 2) {
      return false;
    }
    return std::holds_alternative<ListItemBlock>(block_stack_[block_stack_.size() - 2]);
  }

  float spacing_px(const float multiplier) const
  {
    return main_font_.default_line_height_px * multiplier;
  }

  float block_margin_top(const MdBlock &block) const
  {
    if (const auto *header = std::get_if<HeaderBlock>(&block)) {
      return md_style::headers[header->level - 1].margin_top;
    }
    if (std::holds_alternative<ParagraphBlock>(block)) {
      return md_style::paragraph.margin_top;
    }
    if (std::holds_alternative<QuoteBlock>(block)) {
      return md_style::quote.margin_top;
    }
    if (std::holds_alternative<CodeBlock>(block)) {
      return md_style::code_block.margin_top;
    }
    if (std::holds_alternative<HorizontalRuleBlock>(block)) {
      return md_style::horizontal_rule.margin_top;
    }
    if (std::holds_alternative<UnorderedListBlock>(block) ||
        std::holds_alternative<OrderedListBlock>(block))
    {
      return md_style::list.margin_top;
    }
    return 0.0f;
  }

  float block_margin_bottom(const MdBlock &block) const
  {
    if (const auto *header = std::get_if<HeaderBlock>(&block)) {
      return md_style::headers[header->level - 1].margin_bottom;
    }
    if (std::holds_alternative<ParagraphBlock>(block)) {
      return md_style::paragraph.margin_bottom;
    }
    if (std::holds_alternative<QuoteBlock>(block)) {
      return md_style::quote.margin_bottom;
    }
    if (std::holds_alternative<CodeBlock>(block)) {
      return md_style::code_block.margin_bottom;
    }
    if (std::holds_alternative<HorizontalRuleBlock>(block)) {
      return md_style::horizontal_rule.margin_bottom;
    }
    if (std::holds_alternative<UnorderedListBlock>(block) ||
        std::holds_alternative<OrderedListBlock>(block))
    {
      return md_style::list.margin_bottom;
    }
    return 0.0f;
  }

  void md_leave_block(const MD_BLOCKTYPE block_type, void * /*detail*/)
  {
    const MdBlock &md_block = block_stack_.last();
    if (const auto *list_item = std::get_if<ListItemBlock>(&md_block);
        list_item && list_item->marker_pending)
    {
      markdown_apply_font(main_font_.fontid,
                          main_font_.default_size * cache_.key.pixels_per_point,
                          main_font_.default_weight,
                          false);
      this->ensure_line_box(main_font_.fontid);
    }
    if (std::holds_alternative<HeaderBlock>(md_block) ||
        std::holds_alternative<ListItemBlock>(md_block) ||
        std::holds_alternative<ParagraphBlock>(md_block) ||
        std::holds_alternative<CodeBlock>(md_block))
    {
      this->line_box_end();
    }
    switch (block_type) {
      case MD_BLOCK_QUOTE: {
        BLI_assert(std::holds_alternative<QuoteBlock>(md_block));
        this->append_quote_line();
        break;
      }
      case MD_BLOCK_UL: {
        BLI_assert(std::holds_alternative<UnorderedListBlock>(md_block));
        break;
      }
      case MD_BLOCK_OL: {
        BLI_assert(std::holds_alternative<OrderedListBlock>(md_block));
        break;
      }
      case MD_BLOCK_LI: {
        BLI_assert(std::holds_alternative<ListItemBlock>(md_block));
        break;
      }
      case MD_BLOCK_H: {
        BLI_assert(std::holds_alternative<HeaderBlock>(md_block));
        break;
      }
      case MD_BLOCK_CODE: {
        BLI_assert(std::holds_alternative<CodeBlock>(md_block));
        this->extend_flow_bottom_for_code_block(std::get<CodeBlock>(md_block));
        this->append_code_block_box();
        break;
      }
      case MD_BLOCK_P: {
        BLI_assert(std::holds_alternative<ParagraphBlock>(md_block));
        break;
      }
      case MD_BLOCK_DOC: {
        BLI_assert(std::holds_alternative<DocBlock>(md_block));
        break;
      }
      case MD_BLOCK_HR: {
        BLI_assert(std::holds_alternative<HorizontalRuleBlock>(md_block));
        break;
      }
      case MD_BLOCK_HTML:
      case MD_BLOCK_TABLE:
      case MD_BLOCK_THEAD:
      case MD_BLOCK_TBODY:
      case MD_BLOCK_TR:
      case MD_BLOCK_TH:
      case MD_BLOCK_TD: {
        BLI_assert(std::holds_alternative<UnknownBlock>(md_block));
        break;
      }
    }
    this->add_pending_spacing(this->block_margin_bottom(md_block));
    block_stack_.pop_last();
  }

  void md_enter_span(const MD_SPANTYPE span_type, void *detail)
  {
    switch (span_type) {
      case MD_SPAN_EM: {
        span_stack_.append(EmSpan{});
        break;
      }
      case MD_SPAN_STRONG: {
        span_stack_.append(StrongSpan{});
        break;
      }
      case MD_SPAN_A: {
        const auto *a_detail = static_cast<const MD_SPAN_A_DETAIL *>(detail);
        span_stack_.append(LinkSpan{StringRef(a_detail->href.text, a_detail->href.size)});
        break;
      }
      case MD_SPAN_CODE: {
        CodeSpan code_span;
        BLI_rctf_init_minmax(&code_span.current_bounds);
        span_stack_.append(code_span);
        break;
      }
      case MD_SPAN_IMG:
      case MD_SPAN_DEL:
      case MD_SPAN_LATEXMATH:
      case MD_SPAN_LATEXMATH_DISPLAY:
      case MD_SPAN_WIKILINK:
      case MD_SPAN_U: {
        /* Not implemented. */
        span_stack_.append(UnknownSpan{});
        break;
      }
    }
  }

  void md_leave_span(const MD_SPANTYPE span_type, void * /*detail*/)
  {
    const MdSpan &md_span = span_stack_.last();
    switch (span_type) {
      case MD_SPAN_EM: {
        BLI_assert(std::holds_alternative<EmSpan>(md_span));
        break;
      }
      case MD_SPAN_STRONG: {
        BLI_assert(std::holds_alternative<StrongSpan>(md_span));
        break;
      }
      case MD_SPAN_A: {
        BLI_assert(std::holds_alternative<LinkSpan>(md_span));
        break;
      }
      case MD_SPAN_CODE: {
        BLI_assert(std::holds_alternative<CodeSpan>(md_span));
        this->finalize_code_span_line_bounds();
        this->append_code_span_boxes(std::get<CodeSpan>(md_span));
        x_ += this->code_span_padding_x_px();
        break;
      }
      case MD_SPAN_IMG:
      case MD_SPAN_DEL:
      case MD_SPAN_LATEXMATH:
      case MD_SPAN_LATEXMATH_DISPLAY:
      case MD_SPAN_WIKILINK:
      case MD_SPAN_U: {
        BLI_assert(std::holds_alternative<UnknownSpan>(md_span));
        break;
      }
    }
    span_stack_.pop_last();
  }

  void md_text(const MD_TEXTTYPE text_type, const StringRef text)
  {
    switch (text_type) {
      case MD_TEXT_NORMAL: {
        this->md_text_normal(text);
        break;
      }
      case MD_TEXT_CODE: {
        this->md_text_code(text);
        break;
      }
      case MD_TEXT_BR:
      case MD_TEXT_SOFTBR: {
        this->add_line_break();
        x_ = this->context_content_start_x();
        break;
      }
      case MD_TEXT_NULLCHAR:
      case MD_TEXT_ENTITY:
      case MD_TEXT_HTML:
      case MD_TEXT_LATEXMATH: {
        /* Not implemented. */
        break;
      }
    }
  }

  void md_text_normal(const StringRef text)
  {
    const ContextFontSettings settings = this->get_context_font_settings();
    const float start_x = this->context_content_start_x();
    this->output_wrapped_text(text,
                              settings.size,
                              settings.fontid,
                              settings.weight,
                              settings.italic,
                              settings.url,
                              start_x);
  }

  void md_text_code(StringRef text)
  {
    const ContextFontSettings settings = this->get_context_font_settings();
    const float start_x = this->context_content_start_x();
    markdown_apply_font(settings.fontid,
                        settings.size * cache_.key.pixels_per_point,
                        settings.weight,
                        settings.italic);
    while (!text.is_empty()) {
      /* Literal newlines should be taken into account in code. */
      if (text[0] == '\n') {
        text = text.drop_prefix(1);
        this->add_line_break();
        x_ = start_x;
        continue;
      }
      const int64_t newline = text.find_first_of('\n');
      if (newline == StringRef::not_found) {
        this->output_wrapped_text(text,
                                  settings.size,
                                  settings.fontid,
                                  settings.weight,
                                  settings.italic,
                                  settings.url,
                                  start_x);
        break;
      }
      this->output_wrapped_text(text.substr(0, newline),
                                settings.size,
                                settings.fontid,
                                settings.weight,
                                settings.italic,
                                settings.url,
                                start_x);
      text = text.drop_prefix(newline);
    }
  }

  struct ContextFontSettings {
    int fontid;
    int weight;
    bool italic;
    float size;
    std::optional<StringRef> url;
  };

  void ensure_line_box(const int fontid)
  {
    const float padding_y = this->has_code_span() ? this->code_span_padding_y_px() : 0.0f;
    const float line_height = BLF_height_max(fontid) + 2.0f * padding_y;
    if (current_line_box_) {
      current_line_box_->height = std::max(current_line_box_->height, line_height);
      return;
    }

    current_line_box_ = LineBox{flow_bottom_.value_or(0.0f) - pending_spacing_, line_height};
    y_ = current_line_box_->top - BLF_ascender(fontid) - padding_y;
    pending_spacing_ = 0.0f;
    x_ = this->context_content_start_x();
    this->append_pending_list_marker();
  }

  ContextFontSettings get_context_font_settings() const
  {
    ContextFontSettings settings;
    settings.fontid = main_font_.fontid;
    settings.weight = main_font_.default_weight;
    settings.size = main_font_.default_size;
    settings.italic = false;

    if (this->is_in_fenced_code_block()) {
      settings.fontid = mono_font_.fontid;
      settings.size = mono_font_.default_size;
      settings.weight = mono_font_.default_weight;
      settings.italic = false;
      return settings;
    }

    for (const auto &md_block : block_stack_) {
      if (const auto *header_block = std::get_if<HeaderBlock>(&md_block)) {
        const MdHeaderStyle &style = md_style::headers[header_block->level - 1];
        settings.fontid = main_font_.fontid;
        settings.size = main_font_.default_size * style.size_factor;
        settings.weight = style.weight;
        settings.italic = style.italic;
      }
    }
    for (const auto &md_span : span_stack_) {
      if (std::holds_alternative<CodeSpan>(md_span)) {
        settings.fontid = mono_font_.fontid;
        settings.weight = mono_font_.default_weight;
        settings.italic = false;
      }
      if (std::holds_alternative<StrongSpan>(md_span)) {
        settings.weight = main_font_.bold_weight;
      }
      if (std::holds_alternative<EmSpan>(md_span)) {
        settings.italic = true;
      }
      if (const auto *link_span = std::get_if<LinkSpan>(&md_span)) {
        settings.url = link_span->url;
      }
    }
    return settings;
  }

  void output_wrapped_text(StringRef text,
                           const float size,
                           const int fontid,
                           const int weight,
                           const bool italic,
                           const std::optional<StringRef> &url,
                           const float start_x)
  {
    MarkdownItemText item_template;
    item_template.size_px = size * cache_.key.pixels_per_point;
    item_template.fontid = fontid;
    item_template.weight = weight;
    item_template.italic = italic;
    item_template.is_in_quote = this->is_in_quote();
    item_template.code_context = this->is_in_fenced_code_block() ?
                                     MarkdownItemText::CodeContext::CodeBlock :
                                     (this->has_code_span() ?
                                          MarkdownItemText::CodeContext::CodeSpan :
                                          MarkdownItemText::CodeContext::None);
    item_template.url = url;

    markdown_apply_item_font(item_template);
    BLI_SCOPED_DEFER([&]() { markdown_disable_item_font(item_template); });

    while (!text.is_empty()) {
      this->ensure_line_box(fontid);
      if (item_template.code_context == MarkdownItemText::CodeContext::CodeSpan) {
        this->apply_code_span_left_padding();
      }
      /* A list marker uses the body font and may have changed the active font settings. */
      markdown_apply_item_font(item_template);
      const float remaining_width = this->context_content_end_x() - x_;
      float needed_width = 0.0f;
      const int64_t last_fit_char_i = int64_t(BLF_width_to_strlen(
          fontid, text.data(), size_t(text.size()), remaining_width, &needed_width));

      /* Number of bytes that should be drawn in the current line. */
      int64_t draw_bytes = last_fit_char_i;
      /* Number of bytes that should be skipped (this skips e.g. whitespace at the line end).*/
      int64_t drop_bytes = last_fit_char_i;
      const bool wrap_to_next_line = last_fit_char_i < text.size();

      if (wrap_to_next_line) {
        const int64_t last_space = (last_fit_char_i > 0) ?
                                       text.find_last_of(' ', int64_t(last_fit_char_i) - 1) :
                                       StringRef::not_found;
        if (last_space != StringRef::not_found) {
          draw_bytes = last_space;
          /* Skip the space. */
          drop_bytes = last_space + 1;
          needed_width = BLF_width(fontid, text.data(), draw_bytes);
        }
        else if (x_ > start_x) {
          /* Line has already started and the current word does not fit on it; retry on the next
           * line. */
          this->line_box_end();
          x_ = start_x;
          continue;
        }
        else if (last_fit_char_i == 0) {
          /* Nothing fits on an empty line; advance one character to avoid an infinit loop. */
          draw_bytes = BLI_str_utf8_size_safe(text.data());
          drop_bytes = draw_bytes;
          needed_width = BLF_width(fontid, text.data(), draw_bytes);
        }
      }

      const StringRef fitting_text = text.substr(0, draw_bytes);
      text = text.drop_prefix(drop_bytes);

      if (draw_bytes > 0) {
        item_template.text = fitting_text;
        item_template.x = x_;
        item_template.y = y_;
        cache_.md_layout.items.append(item_template);
        this->track_quote_text_bounds(item_template);
        this->track_code_text_bounds(item_template);
      }
      if (wrap_to_next_line) {
        if (item_template.code_context == MarkdownItemText::CodeContext::CodeSpan) {
          this->finalize_code_span_line_bounds();
        }
        this->line_box_end();
        x_ = start_x;
      }
      else {
        x_ += needed_width;
      }
    }
  }

  void finalize_vertical_positions()
  {
    this->line_box_end();
    if (!flow_bottom_) {
      cache_.md_layout.content_height = main_font_.default_line_height_px;
      return;
    }

    float bottom_y = *flow_bottom_;
    for (const MarkdownItem &item : cache_.md_layout.items) {
      if (const auto *quote_line = std::get_if<MarkdownItemQuoteLine>(&item)) {
        bottom_y = std::min(bottom_y, quote_line->y - quote_line->height);
      }
      if (const auto *code_box = std::get_if<MarkdownItemCodeBox>(&item)) {
        bottom_y = std::min(bottom_y, code_box->rect.ymin);
      }
    }
    cache_.md_layout.content_height = -bottom_y;
  }
};

static void create_link_buttons_for_label(ButtonLabel *button)
{
  if (!button->markdown_cache) {
    return;
  }
  const MarkdownLayoutCache &cache = *button->markdown_cache;
  if (cache.md_layout.items.is_empty()) {
    return;
  }

  Block *block = button->block;
  const float aspect = block->aspect;
  wmOperatorType *ot = WM_operatortype_find("WM_OT_url_open", false);
  if (!ot) {
    return;
  }

  for (const MarkdownItem &item : cache.md_layout.items) {
    const auto *text_item_ptr = std::get_if<MarkdownItemText>(&item);
    if (!text_item_ptr || !text_item_ptr->url) {
      continue;
    }
    const MarkdownItemText &text_item = *text_item_ptr;

    markdown_apply_item_font(text_item);

    const StringRef text = text_item.text;
    const float descender_px = BLF_descender(text_item.fontid);
    const float ascender_px = BLF_ascender(text_item.fontid);
    const float font_height = (ascender_px - descender_px) * aspect;
    const float width = BLF_width(text_item.fontid, text.data(), text.size()) * aspect;

    const float x = button->rect.xmin + text_item.x * aspect;
    const float y = button->rect.ymax + (text_item.y + descender_px) * aspect;

    const EmbossType previous_emboss = block_emboss_get(block);
    block_emboss_set(block, EmbossType::None);
    Button *but = uiDefIconBut(block,
                               ButtonType::But,
                               ICON_NONE,
                               x,
                               y,
                               width,
                               font_height,
                               nullptr,
                               0.0,
                               0.0,
                               std::nullopt);
    block_emboss_set(block, previous_emboss);

    static_cast<ButtonPush *>(but)->draw_as_link = true;

    PointerRNA props = WM_operator_properties_create_ptr(ot);
    RNA_string_set(&props, "url", std::string(*text_item.url).c_str());
    button_operator_set(but, ot, wm::OpCallContext::InvokeDefault, &props);

    /* Invisible hit-target; markdown draw paints the link text. Show URL on hover. */
    button_func_tooltip_custom_set(
        but,
        [](bContext & /*C*/, TooltipData &data, Button *tip_but, void * /*argN*/) {
          tooltip_text_field_add(data,
                                 RNA_string_get(tip_but->opptr, "url"),
                                 {},
                                 TIP_STYLE_NORMAL,
                                 TIP_LC_NORMAL,
                                 false);
        },
        nullptr,
        nullptr);

    markdown_disable_item_font(text_item);
  }
}

void label_markdown_create_link_buttons(Block *block)
{
  Vector<ButtonLabel *> markdown_labels;
  for (Button &but : block->buttons()) {
    if (button_label_is_markdown(&but)) {
      markdown_labels.append(static_cast<ButtonLabel *>(&but));
    }
  }
  for (ButtonLabel *label : markdown_labels) {
    create_link_buttons_for_label(label);
  }
}

static MarkdownLayoutCacheKey markdown_layout_key_for_button(const ButtonLabel &button)
{
  const float aspect = button.block->aspect;
  MarkdownLayoutCacheKey key;
  key.text = button.str;
  key.wrap_width_px = std::max<int>(std::ceil(BLI_rctf_size_x(&button.rect) / aspect), 0);
  key.pixels_per_point = UI_SCALE_FAC / aspect;
  key.markdown_layout_generation = markdown_layout_generation;
  return key;
}

static void markdown_apply_button_height(ButtonLabel &button)
{
  button.rect.ymin = button.rect.ymax -
                     button.markdown_cache->md_layout.content_height * button.block->aspect;
}

void label_markdown_resolve(ButtonLabel *button)
{
  const MarkdownLayoutCacheKey cache_lookup_key = markdown_layout_key_for_button(*button);
  {
    /* Try to reuse an already attached cache. */
    if (button->markdown_cache) {
      const MarkdownLayoutCache &cache = *button->markdown_cache;
      if (cache.key != cache_lookup_key) {
        button->markdown_cache.reset();
      }
    }
    /* Try to take a matching cache from the previous block. */
    if (!button->markdown_cache && button->block->oldblock) {
      int i = 0;
      for (std::shared_ptr<MarkdownLayoutCache> &cache_ptr :
           button->block->oldblock->markdown_layout_cache)
      {
        MarkdownLayoutCache &cache = *cache_ptr;
        if (cache.key == cache_lookup_key) {
          button->markdown_cache = cache_ptr;
          break;
        }
        i++;
      }
      if (button->markdown_cache) {
        button->block->oldblock->markdown_layout_cache.remove(i);
      }
    }
    if (button->markdown_cache) {
      button->block->markdown_layout_cache.append(button->markdown_cache);
      markdown_apply_button_height(*button);
      return;
    }
  }

  button->markdown_cache = std::make_shared<MarkdownLayoutCache>();
  MarkdownLayoutCache &cache = *button->markdown_cache;
  cache.key = cache_lookup_key;
  /* Copy the string to the cache so that references into it can be preserved. */
  cache.key.text = cache.scope.allocator().copy_string(cache.key.text);
  button->block->markdown_layout_cache.append(button->markdown_cache);

  MarkdownLayouter layouter(cache);
  layouter.compute_layout();

  markdown_apply_button_height(*button);
}

struct MarkdownDrawColors {
  ColorTheme4b link;
  ColorTheme4b quote_dimmed;
  ColorTheme4b code_span_text;
  ColorTheme4b code_box_text;
  ColorTheme4b quote_dimmed_link;
  ColorTheme4b code_span_link;
  ColorTheme4f code_box_fill;
  ColorTheme4f code_box_border;
  ColorTheme4f code_span_fill;
  ColorTheme4b horizontal_rule;
  ColorTheme4b quote_line;
};

static MarkdownDrawColors markdown_draw_colors_init(const uchar color[4])
{
  MarkdownDrawColors colors;

  theme::get_color_4ubv(TH_LINK, colors.link);
  colors.link.a = color[3];

  /* Blend toward the panel background so quote text is muted consistently across themes. */
  ColorTheme4b back_color;
  theme::get_color_3ubv(TH_PANEL_BACK, back_color);
  constexpr float quote_dimmed_blend = 0.3f;
  constexpr float code_span_fill_blend = 0.20f;

  const bTheme *btheme = theme::theme_get();
  const uchar *code_box_fill_color = btheme->tui.wcol_box.inner;
  const uchar *code_box_border_color = btheme->tui.wcol_box.outline;
  const uchar *code_box_text_color = btheme->tui.wcol_box.text;

  theme::get_color_blend_shade_3ubv(color, back_color, quote_dimmed_blend, 0, colors.quote_dimmed);
  colors.quote_dimmed.a = color[3];

  colors.code_span_text = ColorTheme4b(code_box_text_color);
  colors.code_box_text = ColorTheme4b(code_box_text_color);

  theme::get_color_blend_shade_3ubv(
      colors.link, back_color, quote_dimmed_blend, 0, colors.quote_dimmed_link);
  colors.quote_dimmed_link.a = colors.link.a;

  theme::get_color_blend_shade_3ubv(
      colors.link, code_box_fill_color, 0.1f, 0, colors.code_span_link);
  colors.code_span_link.a = colors.link.a;

  ColorTheme4b code_span_fill_color;
  theme::get_color_blend_shade_3ubv(
      back_color, code_box_fill_color, code_span_fill_blend, 0, code_span_fill_color);
  code_span_fill_color.a = 255;

  colors.code_box_fill = color::to_float(ColorTheme4b(code_box_fill_color));
  colors.code_box_border = color::to_float(ColorTheme4b(code_box_border_color));
  colors.code_span_fill = color::to_float(code_span_fill_color);

  colors.horizontal_rule = ColorTheme4b(color[0], color[1], color[2], 30);
  colors.quote_line = ColorTheme4b(color[0], color[1], color[2], 80);

  return colors;
}

void label_markdown_draw(const ButtonLabel *button, const uchar color[4], const rcti *rect)
{
  if (!button->markdown_cache || button->markdown_cache->md_layout.items.is_empty()) {
    return;
  }

  const MarkdownLayoutCache &cache = *button->markdown_cache;
  const MarkdownDrawColors colors = markdown_draw_colors_init(color);

  const bTheme *btheme = theme::theme_get();
  const float corner_radius = btheme->tui.wcol_box.roundness * U.widget_unit *
                              cache.key.pixels_per_point;

  for (const MarkdownItem &item : cache.md_layout.items) {
    const auto *code_box = std::get_if<MarkdownItemCodeBox>(&item);
    if (!code_box) {
      continue;
    }

    rctf box_rect = code_box->rect;
    box_rect.xmin += rect->xmin;
    box_rect.xmax += rect->xmin;
    box_rect.ymax += rect->ymax;
    box_rect.ymin += rect->ymax;

    draw_roundbox_corner_set(CNR_ALL);
    if (code_box->kind == MarkdownItemCodeBox::Kind::Block) {
      draw_roundbox_4fv_ex(&box_rect,
                           colors.code_box_fill,
                           nullptr,
                           1.0f,
                           colors.code_box_border,
                           U.pixelsize,
                           corner_radius);
    }
    else {
      draw_roundbox_4fv_ex(
          &box_rect, colors.code_span_fill, nullptr, 1.0f, nullptr, 0.0f, corner_radius);
    }
  }

  for (const MarkdownItem &item : cache.md_layout.items) {
    if (const auto *text_item = std::get_if<MarkdownItemText>(&item)) {
      markdown_apply_item_font(*text_item);
      if (text_item->url) {
        if (text_item->is_in_quote) {
          BLF_color4ubv(text_item->fontid, colors.quote_dimmed_link);
        }
        else if (text_item->code_context == MarkdownItemText::CodeContext::CodeSpan) {
          BLF_color4ubv(text_item->fontid, colors.code_span_link);
        }
        else {
          BLF_color4ubv(text_item->fontid, colors.link);
        }
      }
      else {
        if (text_item->is_in_quote) {
          BLF_color4ubv(text_item->fontid, colors.quote_dimmed);
        }
        else if (text_item->code_context == MarkdownItemText::CodeContext::CodeBlock) {
          BLF_color4ubv(text_item->fontid, colors.code_box_text);
        }
        else if (text_item->code_context == MarkdownItemText::CodeContext::CodeSpan) {
          BLF_color4ubv(text_item->fontid, colors.code_span_text);
        }
        else {
          BLF_color4ubv(text_item->fontid, color);
        }
      }
      const float x = rect->xmin + text_item->x;
      const float y = rect->ymax + text_item->y;
      BLF_position(text_item->fontid, x, y, 0.0f);
      BLF_draw(text_item->fontid, text_item->text.data(), text_item->text.size());
      markdown_disable_item_font(*text_item);
    }
    else if (const auto *rule_item = std::get_if<MarkdownItemHorizontalRule>(&item)) {
      const float y = rect->ymax + rule_item->y;

      const uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      GPU_blend(GPU_BLEND_ALPHA);
      immUniformColor4ubv(colors.horizontal_rule);
      GPU_line_width(1.0f);
      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, rect->xmin, y);
      immVertex2f(pos, rect->xmax, y);
      immEnd();
      GPU_blend(GPU_BLEND_NONE);
      immUnbindProgram();
    }
    else if (const auto *quote_line = std::get_if<MarkdownItemQuoteLine>(&item)) {
      const float x = rect->xmin + quote_line->x;
      const float y_top = rect->ymax + quote_line->y;
      const float y_bottom = y_top - quote_line->height;

      const uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", gpu::VertAttrType::SFLOAT_32_32);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      GPU_blend(GPU_BLEND_ALPHA);
      immUniformColor4ubv(colors.quote_line);
      GPU_line_width(2.0f);
      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, x, y_top);
      immVertex2f(pos, x, y_bottom);
      immEnd();
      GPU_blend(GPU_BLEND_NONE);
      immUnbindProgram();
    }
  }
}

static void label_markdown_dev_config_update(bContext & /*C*/)
{
  markdown_layout_generation++;
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

static std::string markdown_md_style_to_source()
{
  fmt::memory_buffer buf;
  fmt::appender dst{buf};

  fmt::format_to(dst, "namespace md_style {{\n\n");
  fmt::format_to(dst, "static std::array<MdHeaderStyle, 6> headers = {{\n");
  for (const int level : IndexRange(md_style::headers.size())) {
    const MdHeaderStyle &style = md_style::headers[level];
    fmt::format_to(dst,
                   "    MdHeaderStyle{{.size_factor = {:.2f}f, .weight = {}, .italic = {}, "
                   ".margin_top = {:.2f}f, .margin_bottom = {:.2f}f}},\n",
                   style.size_factor,
                   style.weight,
                   style.italic ? "true" : "false",
                   style.margin_top,
                   style.margin_bottom);
  }
  fmt::format_to(dst, "}};\n\n");

  auto append_block_spacing = [&](const char *name, const MdBlockSpacing &spacing) {
    fmt::format_to(
        dst,
        "static MdBlockSpacing {} = {{.margin_top = {:.2f}f, .margin_bottom = {:.2f}f}};\n",
        name,
        spacing.margin_top,
        spacing.margin_bottom);
  };
  append_block_spacing("paragraph", md_style::paragraph);
  append_block_spacing("quote", md_style::quote);
  fmt::format_to(dst, "static float quote_bar_padding = {:.2f}f;\n", md_style::quote_bar_padding);
  append_block_spacing("code_block", md_style::code_block);
  fmt::format_to(
      dst, "static float code_block_padding_x = {:.2f}f;\n", md_style::code_block_padding_x);
  fmt::format_to(
      dst, "static float code_block_padding_y = {:.2f}f;\n", md_style::code_block_padding_y);
  fmt::format_to(
      dst, "static float code_span_padding_x = {:.2f}f;\n", md_style::code_span_padding_x);
  fmt::format_to(
      dst, "static float code_span_padding_y = {:.2f}f;\n", md_style::code_span_padding_y);
  append_block_spacing("horizontal_rule", md_style::horizontal_rule);
  fmt::format_to(dst, "\n");

  auto append_list_spacing = [&](const char *name, const MdListSpacing &spacing) {
    fmt::format_to(dst,
                   "static MdListSpacing {} = {{.margin_top = {:.2f}f, .margin_bottom = {:.2f}f, "
                   ".item_spacing_loose = {:.2f}f, .item_spacing_tight = {:.2f}f, "
                   ".nested_margin_top = {:.2f}f}};\n",
                   name,
                   spacing.margin_top,
                   spacing.margin_bottom,
                   spacing.item_spacing_loose,
                   spacing.item_spacing_tight,
                   spacing.nested_margin_top);
  };
  append_list_spacing("list", md_style::list);
  fmt::format_to(dst, "\n");

  fmt::format_to(dst, "static float list_indent_px = {:.2f}f;\n", md_style::list_indent_px);
  fmt::format_to(
      dst, "static float list_marker_padding_px = {:.2f}f;\n", md_style::list_marker_padding_px);
  fmt::format_to(
      dst, "static float horizontal_rule_padding = {:.2f}f;\n", md_style::horizontal_rule_padding);
  fmt::format_to(dst, "\n}};\n");
  return fmt::to_string(buf);
}

static void label_markdown_copy_style_to_clipboard(bContext & /*C*/)
{
  const std::string text = markdown_md_style_to_source();
  WM_clipboard_text_set(text.c_str(), false);
}

static short md_dev_config_h_col_width()
{
  return short(2.0f * UI_UNIT_X);
}

static short md_dev_config_name_col_width()
{
  return short(7.0f * UI_UNIT_X);
}

static short md_dev_config_num_width()
{
  return short(3.5f * UI_UNIT_X);
}

static short md_dev_config_detail_label_width()
{
  return short(5.0f * UI_UNIT_X);
}

static void label_markdown_dev_config_col_label(Block *block,
                                                Layout &col,
                                                const StringRef label,
                                                const short width)
{
  block_layout_set_current(block, &col);
  uiDefBut(
      block, ButtonType::Label, label, 0, 0, width, UI_UNIT_Y, nullptr, 0.0f, 0.0f, std::nullopt);
}

static void label_markdown_dev_config_num(Block *block,
                                          Layout &col,
                                          float *value,
                                          const float min,
                                          const float max,
                                          const float step,
                                          const int precision,
                                          const StringRef tip)
{
  block_layout_set_current(block, &col);
  Button *but = uiDefButV(block,
                          ButtonType::Num,
                          "",
                          0,
                          0,
                          md_dev_config_num_width(),
                          UI_UNIT_Y,
                          value,
                          min,
                          max,
                          tip);
  button_number_step_size_set(but, step);
  button_number_precision_set(but, precision);
  button_func_set(but, label_markdown_dev_config_update);
}

static void label_markdown_dev_config_int(Block *block,
                                          Layout &col,
                                          int *value,
                                          const float min,
                                          const float max,
                                          const float step,
                                          const int precision,
                                          const StringRef tip)
{
  block_layout_set_current(block, &col);
  Button *but = uiDefButV(block,
                          ButtonType::Num,
                          "",
                          0,
                          0,
                          md_dev_config_num_width(),
                          UI_UNIT_Y,
                          value,
                          min,
                          max,
                          tip);
  button_number_step_size_set(but, step);
  button_number_precision_set(but, precision);
  button_func_set(but, label_markdown_dev_config_update);
}

static void label_markdown_dev_config_checkbox(Block *block,
                                               Layout &col,
                                               bool *value,
                                               const StringRef tip)
{
  block_layout_set_current(block, &col);
  Button *but = uiDefButV(
      block, ButtonType::Checkbox, "", 0, 0, UI_UNIT_X, UI_UNIT_Y, value, 0, 0, tip);
  button_func_set(but, label_markdown_dev_config_update);
}

static void label_markdown_dev_config_headers_table(Block *block, Layout &panel_col)
{
  Layout &table = panel_col.row(false);
  table.alignment_set(LayoutAlign::Expand);

  Layout &col_h = table.column(true);
  Layout &col_size = table.column(true);
  Layout &col_weight = table.column(true);
  Layout &col_it = table.column(true);
  Layout &col_top = table.column(true);
  Layout &col_bottom = table.column(true);

  const short num_w = md_dev_config_num_width();
  label_markdown_dev_config_col_label(block, col_h, "", md_dev_config_h_col_width());
  label_markdown_dev_config_col_label(block, col_size, IFACE_("Size"), num_w);
  label_markdown_dev_config_col_label(block, col_weight, IFACE_("Weight"), num_w);
  label_markdown_dev_config_col_label(block, col_it, IFACE_("It"), UI_UNIT_X);
  label_markdown_dev_config_col_label(block, col_top, IFACE_("Top"), num_w);
  label_markdown_dev_config_col_label(block, col_bottom, IFACE_("Bottom"), num_w);

  for (const int level : IndexRange(md_style::headers.size())) {
    MdHeaderStyle &style = md_style::headers[level];
    const std::string h_label = fmt::format("H{}", level + 1);
    label_markdown_dev_config_col_label(block, col_h, h_label, md_dev_config_h_col_width());
    label_markdown_dev_config_num(block,
                                  col_size,
                                  &style.size_factor,
                                  0.25f,
                                  4.0f,
                                  0.05f,
                                  2,
                                  IFACE_("Size factor relative to body text"));
    label_markdown_dev_config_int(
        block, col_weight, &style.weight, 100, 900, 100, -1, IFACE_("Font weight"));
    label_markdown_dev_config_checkbox(block, col_it, &style.italic, IFACE_("Use italic style"));
    label_markdown_dev_config_num(block,
                                  col_top,
                                  &style.margin_top,
                                  0.0f,
                                  4.0f,
                                  0.05f,
                                  2,
                                  IFACE_("Top margin as a multiple of the default line height"));
    label_markdown_dev_config_num(
        block,
        col_bottom,
        &style.margin_bottom,
        0.0f,
        4.0f,
        0.05f,
        2,
        IFACE_("Bottom margin as a multiple of the default line height"));
  }
}

static void label_markdown_dev_config_block_margins_table(Block *block, Layout &panel_col)
{
  Layout &table = panel_col.row(false);
  table.alignment_set(LayoutAlign::Expand);

  Layout &col_name = table.column(true);
  Layout &col_top = table.column(true);
  Layout &col_bottom = table.column(true);

  const short num_w = md_dev_config_num_width();
  const short name_w = md_dev_config_name_col_width();
  label_markdown_dev_config_col_label(block, col_name, "", name_w);
  label_markdown_dev_config_col_label(block, col_top, IFACE_("Top"), num_w);
  label_markdown_dev_config_col_label(block, col_bottom, IFACE_("Bottom"), num_w);

  struct BlockMarginRow {
    const char *label;
    float &margin_top;
    float &margin_bottom;
  };

  const BlockMarginRow rows[] = {
      {IFACE_("Paragraph"), md_style::paragraph.margin_top, md_style::paragraph.margin_bottom},
      {IFACE_("Quote"), md_style::quote.margin_top, md_style::quote.margin_bottom},
      {IFACE_("Code Block"), md_style::code_block.margin_top, md_style::code_block.margin_bottom},
      {IFACE_("Horizontal Rule"),
       md_style::horizontal_rule.margin_top,
       md_style::horizontal_rule.margin_bottom},
      {IFACE_("List"), md_style::list.margin_top, md_style::list.margin_bottom},
  };

  for (const BlockMarginRow &row : rows) {
    label_markdown_dev_config_col_label(block, col_name, row.label, name_w);
    label_markdown_dev_config_num(block,
                                  col_top,
                                  &row.margin_top,
                                  0.0f,
                                  4.0f,
                                  0.05f,
                                  2,
                                  IFACE_("Top margin as a multiple of the default line height"));
    label_markdown_dev_config_num(
        block,
        col_bottom,
        &row.margin_bottom,
        0.0f,
        4.0f,
        0.05f,
        2,
        IFACE_("Bottom margin as a multiple of the default line height"));
  }
}

static void label_markdown_dev_config_list_spacing_table(Block *block, Layout &panel_col)
{
  Layout &table = panel_col.row(false);
  table.alignment_set(LayoutAlign::Expand);

  Layout &col_name = table.column(true);
  Layout &col_loose = table.column(true);
  Layout &col_tight = table.column(true);
  Layout &col_nested = table.column(true);

  const short num_w = md_dev_config_num_width();
  label_markdown_dev_config_col_label(block, col_name, "", md_dev_config_h_col_width());
  label_markdown_dev_config_col_label(block, col_loose, IFACE_("Loose"), num_w);
  label_markdown_dev_config_col_label(block, col_tight, IFACE_("Tight"), num_w);
  label_markdown_dev_config_col_label(block, col_nested, IFACE_("Nested"), num_w);

  label_markdown_dev_config_col_label(
      block, col_name, IFACE_("Items"), md_dev_config_h_col_width());
  label_markdown_dev_config_num(block,
                                col_loose,
                                &md_style::list.item_spacing_loose,
                                0.0f,
                                4.0f,
                                0.05f,
                                2,
                                IFACE_("Spacing between items in loose lists"));
  label_markdown_dev_config_num(block,
                                col_tight,
                                &md_style::list.item_spacing_tight,
                                0.0f,
                                4.0f,
                                0.05f,
                                2,
                                IFACE_("Spacing between items in tight lists"));
  label_markdown_dev_config_num(block,
                                col_nested,
                                &md_style::list.nested_margin_top,
                                0.0f,
                                4.0f,
                                0.05f,
                                2,
                                IFACE_("Margin above the first item in a nested list"));
}

struct MarkdownDevConfigDetailColumn {
  const char *label;
  float *value;
  float min;
  float max;
  float step;
  int precision;
  const char *tip;
};

static void label_markdown_dev_config_details_row(
    Block *block, Layout &panel_col, const Span<MarkdownDevConfigDetailColumn> columns)
{
  Layout &table = panel_col.row(false);
  table.alignment_set(LayoutAlign::Expand);

  for (const MarkdownDevConfigDetailColumn &column : columns) {
    Layout &col = table.column(true);
    label_markdown_dev_config_col_label(
        block, col, column.label, md_dev_config_detail_label_width());
    label_markdown_dev_config_num(block,
                                  col,
                                  column.value,
                                  column.min,
                                  column.max,
                                  column.step,
                                  column.precision,
                                  column.tip);
  }
}

static void label_markdown_dev_config_details_table(Block *block, Layout &panel_col)
{
  const MarkdownDevConfigDetailColumn general_columns[] = {
      {IFACE_("Quote Bar"),
       &md_style::quote_bar_padding,
       0.0f,
       1.0f,
       0.025f,
       3,
       IFACE_("Vertical padding around quote text for the quote bar")},
      {IFACE_("HR Pad"),
       &md_style::horizontal_rule_padding,
       0.0f,
       4.0f,
       0.05f,
       2,
       IFACE_("Additional vertical padding around the horizontal rule line")},
      {IFACE_("Indent"),
       &md_style::list_indent_px,
       4.0f,
       40.0f,
       1.0f,
       1,
       IFACE_("Horizontal indent per list level in pixels at 1x scale")},
      {IFACE_("Marker"),
       &md_style::list_marker_padding_px,
       0.0f,
       20.0f,
       0.5f,
       1,
       IFACE_("Padding between ordered list markers and item text in pixels at 1x scale")},
  };

  const MarkdownDevConfigDetailColumn code_block_columns[] = {
      {IFACE_("Code Block X"),
       &md_style::code_block_padding_x,
       0.0f,
       20.0f,
       0.5f,
       1,
       IFACE_("Horizontal padding around fenced code blocks in pixels at 1x scale")},
      {IFACE_("Code Block Y"),
       &md_style::code_block_padding_y,
       0.0f,
       20.0f,
       0.5f,
       1,
       IFACE_("Vertical padding around fenced code blocks in pixels at 1x scale")},
  };

  const MarkdownDevConfigDetailColumn code_span_columns[] = {
      {IFACE_("Code Span X"),
       &md_style::code_span_padding_x,
       0.0f,
       20.0f,
       0.5f,
       1,
       IFACE_("Horizontal padding around inline code spans in pixels at 1x scale")},
      {IFACE_("Code Span Y"),
       &md_style::code_span_padding_y,
       0.0f,
       20.0f,
       0.5f,
       1,
       IFACE_("Vertical padding around inline code spans in pixels at 1x scale")},
  };

  label_markdown_dev_config_details_row(
      block, panel_col, Span(general_columns, std::size(general_columns)));
  label_markdown_dev_config_details_row(
      block, panel_col, Span(code_block_columns, std::size(code_block_columns)));
  label_markdown_dev_config_details_row(
      block, panel_col, Span(code_span_columns, std::size(code_span_columns)));
}

void label_markdown_dev_config(Layout &layout)
{
  Block *block = layout.block();
  bContext *C = static_cast<bContext *>(block->evil_C);
  if (!C) {
    return;
  }

  layout.use_property_split_set(false);
  Layout &col = layout.column(false);

  if (Layout *panel = layout.panel(C, "md_style_headers", false, IFACE_("Headers"))) {
    label_markdown_dev_config_headers_table(block, panel->column(false));
  }

  if (Layout *panel = layout.panel(C, "md_style_block_margins", false, IFACE_("Block Margins"))) {
    label_markdown_dev_config_block_margins_table(block, panel->column(false));
  }

  if (Layout *panel = layout.panel(C, "md_style_list_spacing", false, IFACE_("List Spacing"))) {
    label_markdown_dev_config_list_spacing_table(block, panel->column(false));
  }

  if (Layout *panel = layout.panel(C, "md_style_details", true, IFACE_("Details"))) {
    label_markdown_dev_config_details_table(block, panel->column(false));
  }

  col.separator();
  Layout &copy_row = col.row(false);
  block_layout_set_current(block, &copy_row);
  Button *copy_but = uiDefBut(block,
                              ButtonType::But,
                              IFACE_("Copy Style to Clipboard"),
                              0,
                              0,
                              short(15 * UI_UNIT_X),
                              UI_UNIT_Y,
                              nullptr,
                              0,
                              0,
                              IFACE_("Copy md_style namespace source to the clipboard"));
  button_func_set(copy_but, label_markdown_copy_style_to_clipboard);
}

}  // namespace blender::ui
