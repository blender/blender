/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <charconv>
#include <fmt/format.h>
#include <regex>

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#include "NOD_fn_format_string.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"

#include "BKE_path_templates.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_format_string_cc {

NODE_STORAGE_FUNCS(NodeFunctionFormatString)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::String>("Format").optional_label().description(
      "Format string using a Python and path template compatible syntax. For example, \"Count: "
      "{}\" would replace the {} with the first input value.");
  b.add_output<decl::String>("String").align_with_previous();

  const bNodeTree *ntree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (!ntree || !node) {
    return;
  }

  const NodeFunctionFormatString &storage = node_storage(*node);
  for (const int i : IndexRange(storage.items_num)) {
    const NodeFunctionFormatStringItem &item = storage.items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    const StringRef name = item.name;
    const std::string identifier = FormatStringItemsAccessor::socket_identifier_for_item(item);
    b.add_input(socket_type, name, identifier)
        .socket_name_ptr(&ntree->id, FormatStringItemsAccessor::item_srna, &item, "name");
  }

  b.add_input<decl::Extend>("", "__extend__");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeFunctionFormatString *data = MEM_callocN<NodeFunctionFormatString>(__func__);
  node->storage = data;
}

static void node_copy_storage(bNodeTree * /*tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeFunctionFormatString &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeFunctionFormatString>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<FormatStringItemsAccessor>(*src_node, *dst_node);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<FormatStringItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<FormatStringItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<FormatStringItemsAccessor>();
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *ptr->data_as<bNode>();
  if (uiLayout *panel = layout->panel(C, "format_string_items", false, IFACE_("Format Items"))) {
    socket_items::ui::draw_items_list_with_operators<FormatStringItemsAccessor>(
        C, panel, tree, node);
    socket_items::ui::draw_active_item_props<FormatStringItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        });
  }
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<FormatStringItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<FormatStringItemsAccessor>(&reader, node);
}

static std::optional<StringRef> find_format_specifier(const StringRef format)
{
  BLI_assert(format[0] == '{');
  int64_t braces_depth = 1;
  for (const char &c : format.substr(1)) {
    if (c == '{') {
      braces_depth++;
    }
    else if (c == '}') {
      braces_depth--;
    }
    if (braces_depth == 0) {
      const int length = &c - format.data() + 1;
      return format.substr(0, length);
    }
  }
  return std::nullopt;
}

static int64_t find_next_format_start_or_end(const StringRef format,
                                             const int64_t start,
                                             std::string &r_out)
{
  int64_t i = start;
  while (i < format.size()) {
    const char c = format[i];
    switch (c) {
      case '{':
      case '}': {
        if (i + 1 < format.size()) {
          const char next_c = format[i + 1];
          if (next_c == c) {
            i += 2;
            r_out += c;
            continue;
          }
        }
        return i;
      }
      default: {
        r_out += c;
        i++;
        break;
      }
    }
  }
  return format.size();
}

struct FormatPatternInfo {
  std::string pattern_str;
  std::regex pattern;
  int width_group;
  std::optional<int> precision_group;
};

/** Also see https://fmt.dev/latest/syntax/. */
static FormatPatternInfo get_pattern_by_type_impl(const CPPType &type)
{
  std::string pattern;
  int groups_num = 0;

  /* Beginning of string. */
  pattern += '^';
  /* Fill and Align. */
  pattern += "([^{}]?[<>^])?";
  groups_num += 1;
  if (type.is<float>() || type.is<int>()) {
    /* Sign. */
    pattern += "[+\\- ]?";
    /* '#' for alternate form is omitted for better potential future compatibility with
     * path templates (#BKE_path_apply_template). */
    /* Sign-aware zero padding. */
    pattern += "0?";
  }
  const std::string integer_or_identifier = "(\\d+|(\\{.*\\}))";
  /* Width. */
  pattern += integer_or_identifier;
  pattern += "?";
  groups_num += 2;
  const int width_group = groups_num;

  std::optional<int> precision_group;
  if (type.is<float>() || type.is<std::string>()) {
    /* Precision. */
    pattern += "(\\.";
    pattern += integer_or_identifier;
    pattern += ")?";
    groups_num += 3;
    precision_group = groups_num;
  }
  /* "L" is omitted, because we take the current locale into account in Geometry Nodes. */
  /* Allowed type specifiers vary by data type. */
  if (type.is<std::string>()) {
    pattern += "[s\\?]?";
  }
  else if (type.is<int>()) {
    pattern += "[bBcdoxX]?";
  }
  else if (type.is<float>()) {
    pattern += "[aAeEfFgG]?";
  }
  /* End of string. */
  pattern += '$';
  return {pattern, std::regex{pattern}, width_group, precision_group};
}

static const FormatPatternInfo *get_pattern_by_type(const CPPType &type)
{
  if (type.is<float>()) {
    static FormatPatternInfo info = get_pattern_by_type_impl(CPPType::get<float>());
    return &info;
  }
  if (type.is<int>()) {
    static FormatPatternInfo info = get_pattern_by_type_impl(CPPType::get<int>());
    return &info;
  }
  if (type.is<std::string>()) {
    static FormatPatternInfo info = get_pattern_by_type_impl(CPPType::get<std::string>());
    return &info;
  }
  return nullptr;
}

class FormatInputsLookup {
 private:
  const Span<GVArray> inputs_;
  const VectorSet<std::string> &input_names_;
  int64_t next_auto_index_ = 0;
  /**
   * Once the first non-auto-index is used, it's not allowed to use the auto-index afterwards
   * anymore.
   */
  bool non_auto_index_used_ = false;

 public:
  FormatInputsLookup(const Span<GVArray> inputs, const VectorSet<std::string> &input_names)
      : inputs_(inputs), input_names_(input_names)
  {
  }

  const GVArray *find_next_input(const StringRef identifier, std::optional<std::string> &r_error)
  {
    const std::optional<int64_t> input_index = this->find_next_input_index(identifier, r_error);
    if (!input_index.has_value()) {
      return nullptr;
    }
    return &inputs_[*input_index];
  }

  std::optional<int64_t> find_next_input_index(const StringRef identifier,
                                               std::optional<std::string> &r_error)
  {
    if (identifier.is_empty()) {
      if (non_auto_index_used_) {
        /* Once the first explicit identifier is used, it's not allowed to use the auto-index
         * anymore. Only other explicit identifiers are allowed. */
        if (!r_error) {
          r_error = TIP_(
              "Empty identifier cannot be used when explicit identifier was used before. For "
              "example, \"{} {x}\" is ok but \"{x} {}\" is not.");
        }
        return std::nullopt;
      }
      if (next_auto_index_ == inputs_.size()) {
        /* Not enough inputs provided. */
        if (!r_error) {
          r_error = TIP_("Format uses more inputs than provided.");
        }
        return std::nullopt;
      }
      return next_auto_index_++;
    }
    non_auto_index_used_ = true;
    if (std::isdigit(identifier[0])) {
      int64_t index;
      std::from_chars_result res = std::from_chars(identifier.begin(), identifier.end(), index);
      if (res.ec != std::errc()) {
        if (!r_error) {
          r_error = fmt::format(fmt::runtime(TIP_("Invalid identifier: \"{}\"")), identifier);
        }
        return std::nullopt;
      }
      if (res.ptr < identifier.end()) {
        /* There are other characters after the number. */
        if (!r_error) {
          r_error = fmt::format(
              fmt::runtime(TIP_("An input name cannot start with a digit: \"{}\"")), identifier);
        }
        return std::nullopt;
      }
      if (index >= inputs_.size()) {
        if (!r_error) {
          if (inputs_.is_empty()) {
            r_error = fmt::format(fmt::runtime(TIP_("There are no inputs.")), identifier);
          }
          else {
            r_error = fmt::format(
                fmt::runtime(TIP_("Input with index {} does not exist. Currently, the maximum "
                                  "possible index is {}. Did you mean to use {{:{}}}?")),
                identifier,
                inputs_.size() - 1,
                identifier);
          }
        }
        return std::nullopt;
      }
      return index;
    }
    const int index = input_names_.index_of_try_as(identifier);
    if (index == -1) {
      if (!r_error) {
        r_error = fmt::format(fmt::runtime(TIP_("Input does not exist: \"{}\"")), identifier);
      }
      return std::nullopt;
    }
    return index;
  }
};

struct ProcessedPythonCompatibleFormat {
  const GVArray *widths = nullptr;
  const GVArray *precisions = nullptr;
  /**
   * This is compatible with the C++ fmt library.
   * It formats exactly one value and may use a dynamic width or precision.
   */
  std::string fmt_format_str;
};

static std::string create_invalid_python_compatible_format_error(const StringRef format,
                                                                 const StringRef format_outer,
                                                                 const FormatPatternInfo &pattern)
{
  for (const char c : format) {
    if (pattern.pattern_str.find(c) == std::string::npos && std::isprint(c) && !std::isdigit(c)) {
      return fmt::format(
          fmt::runtime(TIP_("Format contains unsupported \"{}\" character: \"{}\"")),
          c,
          format_outer);
    }
  }
  return fmt::format(fmt::runtime(TIP_("Invalid format: \"{}\"")), format_outer);
}

static std::optional<ProcessedPythonCompatibleFormat> preprocess_python_compatible_syntax(
    const StringRef format,
    const StringRef format_outer,
    const CPPType &type,
    FormatInputsLookup &inputs_lookup,
    std::optional<std::string> &r_error)
{
  const FormatPatternInfo *allowed_pattern = get_pattern_by_type(type);
  if (!allowed_pattern) {
    /* The type can't be formatted. The user shouldn't be able to trigger this error but nice to
     * handle it anyway. */
    if (!r_error) {
      r_error = fmt::format(fmt::runtime(TIP_("Type \"{}\" cannot be formatted")), type.name());
    }
    return std::nullopt;
  }

  /* Check the syntax of the format string with what is allowed. */
  std::cmatch m;
  if (!std::regex_search(format.begin(), format.end(), m, allowed_pattern->pattern)) {
    if (!r_error) {
      r_error = create_invalid_python_compatible_format_error(
          format, format_outer, *allowed_pattern);
    }
    return std::nullopt;
  }

  ProcessedPythonCompatibleFormat result;

  /* Identifiers that are used to specify the width or precision will be replaced with {}. */
  Vector<std::string> formats_to_replace;

  /* Check if a dynamic width is specified. */
  const std::string width_outer = m.str(allowed_pattern->width_group);
  if (!width_outer.empty()) {
    const StringRef width_inner = StringRef(width_outer).drop_prefix(1).drop_suffix(1);
    result.widths = inputs_lookup.find_next_input(width_inner, r_error);
    if (!result.widths) {
      return std::nullopt;
    }
    if (!result.widths->type().is<int>()) {
      if (!r_error) {
        r_error = fmt::format(
            fmt::runtime(TIP_("Only integer inputs can be used as dynamic width: \"{}\"")),
            format_outer);
      }
      return std::nullopt;
    }
    formats_to_replace.append(width_outer);
  }

  /* Check if a dynamic precision is specified. */
  if (allowed_pattern->precision_group.has_value()) {
    const std::string precision_outer = m.str(*allowed_pattern->precision_group);
    if (!precision_outer.empty()) {
      const StringRef precision_inner = StringRef(precision_outer).drop_prefix(1).drop_suffix(1);
      result.precisions = inputs_lookup.find_next_input(precision_inner, r_error);
      if (!result.precisions) {
        return std::nullopt;
      }
      if (!result.precisions->type().is<int>()) {
        if (!r_error) {
          r_error = fmt::format(
              fmt::runtime(TIP_("Only integer inputs can be used as dynamic precision: \"{}\"")),
              format_outer);
        }
        return std::nullopt;
      }
      formats_to_replace.append(precision_outer);
    }
  }

  result.fmt_format_str = "{:";
  result.fmt_format_str.append(format.begin(), format.end());
  result.fmt_format_str += '}';

  /* Replace identifiers with {}, because the source identifiers are not passed to fmt. */
  for (const std::string &old : formats_to_replace) {
    const int64_t old_start = result.fmt_format_str.find(old);
    if (old_start != std::string::npos) {
      result.fmt_format_str.replace(old_start, old.size(), "{}");
    }
  }

  return result;
}

static void format_with_fmt(const fmt::runtime_format_string<> format,
                            const GVArray &input,
                            const GVArray *widths,
                            const GVArray *precisions,
                            const IndexMask &mask,
                            MutableSpan<std::string> r_formatted_strings)
{
  const auto append_single_formatted_string = [&](const auto &varray) {
    mask.foreach_index([&](const int64_t i) {
      std::string &output = r_formatted_strings[i];
      auto output_inserter = std::back_inserter(output);
      try {
        if (precisions) {
          const int precision = std::max(0, precisions->get<int>(i));
          if (widths) {
            const int width = std::max(0, widths->get<int>(i));
            fmt::format_to(output_inserter, format, varray[i], width, precision);
          }
          else {
            fmt::format_to(output_inserter, format, varray[i], precision);
          }
        }
        else {
          if (widths) {
            const int width = std::max(0, widths->get<int>(i));
            fmt::format_to(output_inserter, format, varray[i], width);
          }
          else {
            fmt::format_to(output_inserter, format, varray[i]);
          }
        }
      }
      catch (const fmt::format_error & /*error*/) {
        /* Invalid patterns should have been caught before already. */
        BLI_assert_unreachable();
      }
    });
  };

  const CPPType &type = input.type();
  if (type.is<float>()) {
    append_single_formatted_string(input.typed<float>());
  }
  else if (type.is<int>()) {
    append_single_formatted_string(input.typed<int>());
  }
  else if (type.is<std::string>()) {
    append_single_formatted_string(input.typed<std::string>());
  }
  else {
    /* The input type should have been checked earlier already. */
    BLI_assert_unreachable();
  }
}

static void format_with_python_compatible_syntax(const StringRef format_pattern,
                                                 const StringRef format_outer,
                                                 const GVArray &input,
                                                 const IndexMask &mask,
                                                 FormatInputsLookup &inputs_lookup,
                                                 MutableSpan<std::string> r_formatted_strings,
                                                 std::optional<std::string> &r_error)
{
  const CPPType &type = input.type();
  /* Extract information like width and precision inputs. */
  std::optional<ProcessedPythonCompatibleFormat> processed_format =
      preprocess_python_compatible_syntax(
          format_pattern, format_outer, type, inputs_lookup, r_error);
  if (!processed_format.has_value()) {
    BLI_assert(r_error);
    return;
  }
  format_with_fmt(fmt::runtime(processed_format->fmt_format_str),
                  input,
                  processed_format->widths,
                  processed_format->precisions,
                  mask,
                  r_formatted_strings);
}

static void format_with_hash_syntax(const StringRef format_pattern,
                                    const GVArray &input,
                                    const IndexMask &mask,
                                    MutableSpan<std::string> r_formatted_strings,
                                    std::optional<std::string> &r_error)
{
  const CPPType &type = input.type();
  if (type.is<float>()) {
    mask.foreach_index([&](const int64_t i) {
      std::string &output = r_formatted_strings[i];
      const float value = input.get<float>(i);
      if (const std::optional<std::string> value_str = BKE_path_template_format_float(
              format_pattern, value))
      {
        output.append(*value_str);
      }
      else if (!r_error) {
        r_error = fmt::format(fmt::runtime(TIP_("Invalid format specifier: \"{}\"")),
                              format_pattern);
      }
    });
  }
  else if (type.is<int>()) {
    mask.foreach_index([&](const int64_t i) {
      std::string &output = r_formatted_strings[i];
      const int64_t value = input.get<int>(i);
      if (const std::optional<std::string> value_str = BKE_path_template_format_int(format_pattern,
                                                                                    value))
      {
        output.append(*value_str);
      }
      else if (!r_error) {
        r_error = fmt::format(fmt::runtime(TIP_("Invalid format specifier: \"{}\"")),
                              format_pattern);
      }
    });
  }
  else if (type.is<std::string>()) {
    if (!r_error) {
      r_error = fmt::format(fmt::runtime(TIP_("Invalid format specifier for string: \"{}\"")),
                            format_pattern);
    }
  }
  else if (!r_error) {
    r_error = fmt::format(fmt::runtime(TIP_("Type \"{}\" cannot be formatted")), type.name());
  }
}

static void format_without_format_specifier(const GVArray &input,
                                            const IndexMask &mask,
                                            MutableSpan<std::string> r_formatted_strings,
                                            std::optional<std::string> &r_error)
{
  const CPPType &type = input.type();
  if (type.is<float>()) {
    mask.foreach_index([&](const int64_t i) {
      const float value = input.get<float>(i);
      std::string &output = r_formatted_strings[i];
      std::string value_str = fmt::format("{}", value);
      /* Add ".0" if there are no decimals yet to match Python. */
      if (StringRef(value_str).find_first_not_of("-0123456789") == StringRef::not_found) {
        value_str.append(".0");
      }
      output += value_str;
    });
  }
  else if (type.is<int>()) {
    mask.foreach_index([&](const int64_t i) {
      const int64_t value = input.get<int>(i);
      std::string &output = r_formatted_strings[i];
      output += fmt::format("{}", value);
    });
  }
  else if (type.is<std::string>()) {
    mask.foreach_index([&](const int64_t i) {
      const std::string value = input.get<std::string>(i);
      std::string &output = r_formatted_strings[i];
      output += value;
    });
  }
  else if (!r_error) {
    r_error = fmt::format(fmt::runtime(TIP_("Type \"{}\" cannot be formatted")), type.name());
  }
}

static bool format_strings(const StringRef format,
                           const Span<GVArray> inputs,
                           const VectorSet<std::string> &input_names,
                           const IndexMask &mask,
                           MutableSpan<std::string> r_formatted_strings,
                           std::optional<std::string> &r_error)
{
  CPPType::get<std::string>().value_initialize_indices(r_formatted_strings.data(), mask);

  FormatInputsLookup inputs_lookup{inputs, input_names};

  int64_t current_index = 0;
  while (current_index < format.size()) {
    /* Find the string until the next format starts or the string ends. */
    std::string copy_str;
    const int64_t next_format_start_or_end = find_next_format_start_or_end(
        format, current_index, copy_str);

    /* Append the non-formatted string to the outputs. */
    if (!copy_str.empty()) {
      mask.foreach_index([&](const int64_t i) {
        std::string &output = r_formatted_strings[i];
        output.append(copy_str);
      });
    }

    /* The string has ended, so return successfully. */
    if (next_format_start_or_end == format.size()) {
      break;
    }
    current_index = next_format_start_or_end;

    /* Find the format specifier starting at the current index. */
    const std::optional<StringRef> format_outer = find_format_specifier(
        format.substr(current_index));
    if (!format_outer.has_value()) {
      if (!r_error) {
        r_error = fmt::format(fmt::runtime(TIP_("Format specifier is not closed: \"{}\"")),
                              format.substr(current_index));
      }
      return false;
    }
    const StringRef format_inner = format_outer->substr(1, format_outer->size() - 2);

    /* Extract the identifier and the pattern which are split by a colon. */
    StringRef identifier;
    StringRef format_pattern;
    const int64_t colon_index = format_inner.find(':');
    if (colon_index == StringRef::not_found) {
      identifier = format_inner;
    }
    else {
      identifier = format_inner.substr(0, colon_index);
      format_pattern = format_inner.substr(colon_index + 1);
    }

    /* Find the typed input values and get the corresponding allowed pattern. */
    const GVArray *input = inputs_lookup.find_next_input(identifier, r_error);
    if (!input) {
      return false;
    }

    if (format_pattern.is_empty()) {
      format_without_format_specifier(*input, mask, r_formatted_strings, r_error);
    }
    else if (format_pattern.find('#') == StringRef::not_found) {
      format_with_python_compatible_syntax(format_pattern,
                                           *format_outer,
                                           *input,
                                           mask,
                                           inputs_lookup,
                                           r_formatted_strings,
                                           r_error);
    }
    else {
      format_with_hash_syntax(format_pattern, *input, mask, r_formatted_strings, r_error);
    }
    if (r_error) {
      return false;
    }

    current_index += format_outer->size();
  }
  return true;
}

class FormatStringMultiFunction : public mf::MultiFunction {
 private:
  const bNode &node_;
  VectorSet<std::string> input_names_;
  mf::Signature signature_;

 public:
  FormatStringMultiFunction(const bNode &node) : node_(node)
  {
    const NodeFunctionFormatString &storage = node_storage(node);

    mf::SignatureBuilder builder{"Format String", signature_};
    builder.single_input<std::string>("Format");
    for (const int i : IndexRange(storage.items_num)) {
      const NodeFunctionFormatStringItem &item = storage.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const CPPType &type = *bke::socket_type_to_geo_nodes_base_cpp_type(socket_type);
      builder.single_input(item.name, type);
      input_names_.add_new(StringRef(item.name));
    }

    builder.single_output<std::string>("String");

    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context context) const override
  {
    const NodeFunctionFormatString &storage = node_storage(node_);

    const VArray<std::string> formats = params.readonly_single_input<std::string>(0, "Format");
    MutableSpan<std::string> outputs = params.uninitialized_single_output<std::string>(
        storage.items_num + 1, "String");

    Array<GVArray> inputs(storage.items_num);
    for (const int i : IndexRange(storage.items_num)) {
      inputs[i] = params.readonly_single_input(i + 1);
    }

    std::optional<std::string> error_message;

    if (const std::optional<std::string> single_format = formats.get_if_single()) {
      if (!format_strings(*single_format, inputs, input_names_, mask, outputs, error_message)) {
        mask.foreach_index([&](const int64_t i) { outputs[i].clear(); });
      }
    }
    else {
      mask.foreach_index(GrainSize(256), [&](const int64_t i) {
        const StringRef format = formats[i];
        if (!format_strings(
                format, inputs, input_names_, IndexRange::from_single(i), outputs, error_message))
        {
          outputs[i].clear();
        }
      });
    }

    if (error_message.has_value()) {
      report_from_multi_function(context, NodeWarningType::Error, std::move(*error_message));
    }
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  builder.construct_and_set_matching_fn<FormatStringMultiFunction>(builder.node());
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeFormatString");
  ntype.ui_name = "Format String";
  ntype.ui_description =
      "Insert values into a string using a Python and path template compatible formatting syntax";
  ntype.nclass = NODE_CLASS_CONVERTER;
  blender::bke::node_type_storage(
      ntype, "NodeFunctionFormatString", node_free_storage, node_copy_storage);
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  ntype.initfunc = node_init;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.insert_link = node_insert_link;
  ntype.register_operators = node_operators;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_format_string_cc

namespace blender::nodes {

StructRNA *FormatStringItemsAccessor::item_srna = &RNA_NodeFunctionFormatStringItem;

void FormatStringItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void FormatStringItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

std::string FormatStringItemsAccessor::custom_initial_name(const bNode &node, StringRef src_name)
{
  /* The goal is to find a single-letter name that is not used already. Ideally, it starts with the
   * same letter as the given name. */

  const auto &storage = *static_cast<NodeFunctionFormatString *>(node.storage);
  char initial = 'a';
  if (!src_name.is_empty()) {
    const char first_c = src_name[0];
    if (first_c >= 'a' && first_c <= 'z') {
      initial = first_c;
    }
    else if (first_c >= 'A' && first_c <= 'Z') {
      initial = first_c - 'A' + 'a';
    }
  }
  for (const int i : IndexRange('z' - 'a' + 1)) {
    char c = initial + i;
    if (c > 'z') {
      /* Start at 'a' again. */
      c = c - 'z' + 'a' - 1;
    }
    const std::string potential_name = std::string(1, c);
    const bool name_exists = std::any_of(
        storage.items,
        storage.items + storage.items_num,
        [&](const NodeFunctionFormatStringItem &item) { return item.name == potential_name; });
    if (!name_exists) {
      return potential_name;
    }
  }
  return src_name;
}

std::string FormatStringItemsAccessor::validate_name(const StringRef name)
{
  /* The name has to start with a letter or underscore. The remaining letters may additionally be
   * digits. */
  std::string result;
  if (name.is_empty()) {
    return result;
  }
  const char first_char = name[0];
  if (!std::isalpha(first_char) && first_char != '_') {
    result += '_';
  }
  for (const char c : name) {
    if (std::isalnum(c) || c == '_') {
      result += c;
    }
    if (ELEM(c, '-', '.', ' ', '\t')) {
      result += '_';
    }
  }

  return result;
}

}  // namespace blender::nodes
