/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 */

#include "intermediate.hh"
#include "scope.hh"
#include "time_it.hh"
#include "token.hh"
#include "token_stream.hh"

#include "lexit/lexit.hh"
#include "lexit/tables.hh"

#if defined(_MSC_VER)
#  include <malloc.h>
#endif

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <stack>

#if defined(_MSC_VER)
#  define always_inline __forceinline
#else
#  define always_inline inline __attribute__((always_inline))
#endif

namespace blender::gpu::shader::parser {

size_t line_number(const std::string_view &str, size_t pos)
{
  std::string_view directive = "#line ";
  /* String to count the number of line. */
  std::string_view sub_str = str.substr(0, pos);
  size_t nearest_line_directive = sub_str.rfind(directive);
  size_t line_count = 1;
  if (nearest_line_directive != std::string::npos) {
    sub_str = sub_str.substr(nearest_line_directive + directive.size());
    line_count = std::stoll(std::string(sub_str)) - 1;
  }
  return line_count + std::count(sub_str.begin(), sub_str.end(), '\n');
}

size_t char_number(const std::string_view &str, size_t pos)
{
  std::string_view sub_str = str.substr(0, pos);
  size_t nearest_line_directive = sub_str.rfind('\n');
  return (nearest_line_directive == std::string::npos) ?
             (sub_str.size()) :
             (sub_str.size() - nearest_line_directive - 1);
}

std::string line_str(const std::string_view &str, size_t pos)
{
  size_t start = str.rfind('\n', pos);
  size_t end = str.find('\n', pos);
  start = (start != std::string::npos) ? start + 1 : 0;
  return std::string(str.substr(start, end - start));
}

Scope Token::scope() const
{
  if (this->is_invalid()) {
    return Scope::invalid();
  }
  return Scope::from_position(*data, data->token_scope[index]);
}

Scope Token::attribute_before() const
{
  if (is_invalid()) {
    return Scope::invalid();
  }
  Token prev = this->prev();
  if (prev == ']' && prev.prev().scope().type() == ScopeType::Attributes) {
    return prev.prev().scope();
  }
  return Scope::invalid();
}

Scope Token::attribute_after() const
{
  if (is_invalid()) {
    return Scope::invalid();
  }
  Token next = this->next();
  if (next == '[' && next.next().scope().type() == ScopeType::Attributes) {
    return next.next().scope();
  }
  return Scope::invalid();
}

alignas(128) const std::array<CharClass, 128> LexerBase::default_char_class_table = [] {
  std::array<CharClass, 128> table;
  memcpy(table.data(), lexit::char_class_table, sizeof(lexit::char_class_table));
  return table;
}();

/* Same thing as default table but consider numbers as words to avoid second merging pass. */
alignas(128) const std::array<CharClass, 128> LexerBase::bsl_char_class_table = [] {
  std::array<CharClass, 128> table;
  memcpy(table.data(), lexit::char_class_table, sizeof(lexit::char_class_table));

  table['\n'] = CharClass::WhiteSpace;
  /* Make < and > separators in order to support template.
   * That means >= and <= need to be manually handled. */
  table['<'] = CharClass::Separator;
  table['>'] = CharClass::Separator;
  return table;
}();

static always_inline TokenType multi_tok_lookup(TokenType input, std::string_view s)
{
  switch (s.size()) {
    case 2:
      switch (s[0]) {
        case '=':
          return (s[1] == '=') ? Equal : input;
        case '!':
          return (s[1] == '=') ? NotEqual : input;
        case '|':
          return (s[1] == '|') ? LogicalOr : input;
        case '&':
          return (s[1] == '&') ? LogicalAnd : input;
        case '<':
          return (s[1] == '=') ? LEqual : input;
        case '>':
          return (s[1] == '=') ? GEqual : input;
        case '+':
          return (s[1] == '+') ? Increment : input;
        case '-':
          return (s[1] == '-') ? Decrement : input;
        case '#':
          return (s[1] == '#') ? DoubleHash : input;
        default:
          return input;
      }
    default:
      return input;
  }
}

void LexerBase::merge_tokens()
{
  merge_complex_literals();
  // merge_whitespaces();

  update_string_view();
}

constexpr always_inline uint8_t perfect_hash(std::string_view s)
{
  return s.size() * (s[0] - s.back() * 2);
}

static always_inline TokenType type_lookup(std::string_view s)
{
  switch (perfect_hash(s)) {
    case perfect_hash("do"):
      return (s == "do") ? Do : Word;
    case perfect_hash("if"):
      return (s == "if") ? If : Word;
    case perfect_hash("for"):
      return (s == "for") ? For : Word;
    case perfect_hash("case"):
      return (s == "case") ? Case : Word;
    case perfect_hash("else"):
      return (s == "else") ? Else : Word;
    case perfect_hash("enum"):
      return (s == "enum") ? Enum : Word;
    case perfect_hash("this"):
      return (s == "this") ? This : Word;
    case perfect_hash("break"):
      return (s == "break") ? Break : Word;
    case perfect_hash("class"):
      return (s == "class") ? Class : Word;
    case perfect_hash("const"):
      return (s == "const") ? Const : Word;
    case perfect_hash("union"):
      return (s == "union") ? Union : Word;
    case perfect_hash("using"):
      return (s == "using") ? Using : Word;
    case perfect_hash("while"):
      return (s == "while") ? While : Word;
    case perfect_hash("inline"):
      return (s == "inline") ? Inline : Word;
    case perfect_hash("public"):
      return (s == "public") ? Public : Word;
    case perfect_hash("return"):
      return (s == "return") ? Return : Word;
    case perfect_hash("static"):
      return (s == "static") ? Static : Word;
    case perfect_hash("struct"):
      return (s == "struct") ? Struct : Word;
    case perfect_hash("switch"):
      return (s == "switch") ? Switch : Word;
    case perfect_hash("private"):
      return (s == "private") ? Private : Word;
    case perfect_hash("continue"):
      return (s == "continue") ? Continue : Word;
    case perfect_hash("template"):
      return (s == "template") ? Template : Word;
    case perfect_hash("constexpr"):
      return (s == "constexpr") ? Constexpr : Word;
    case perfect_hash("namespace"):
      return (s == "namespace") ? Namespace : Word;
    default:
      return Word;
  }
}

void LexerBase::identify_keywords()
{
  for (auto tok : *this) {
    switch (tok.type()) {
      case Word:
        tok.type() = type_lookup(tok.str());
        break;
      case Number:
        break;
      default:
        tok.type() = multi_tok_lookup(tok.type(), tok.str());
        break;
    }
  }
}

struct ScopeStack {
  struct Item {
    ScopeType type;
    size_t start;
    int index;
  };

  int scope_index = 0;
  std::vector<ScopeStack::Item> scopes;
  /* Output. */
  std::vector<IndexRange> ranges;
  std::vector<ScopeType> types;

  ScopeStack(size_t predicted_scope_count)
  {
    /* Predicted max nesting depth. */
    scopes.reserve(128);

    ranges.reserve(predicted_scope_count);
    types.reserve(predicted_scope_count);
  }

  void always_inline enter_scope(ScopeType type, size_t start_tok_id)
  {
    scopes.emplace_back(Item{type, start_tok_id, scope_index++});
    ranges.emplace_back(start_tok_id, 1);
    types.emplace_back(type);
  };

  void always_inline exit_scope(int end_tok_id)
  {
    if (scopes.empty()) {
      return;
    }
    Item scope = scopes.back();
    ranges[scope.index].size = end_tok_id - scope.start + 1;
    scopes.pop_back();
  };

  Item always_inline back() const
  {
    return scopes.back();
  }

  bool always_inline empty() const
  {
    return scopes.empty();
  }
};

void ParserBase::build_scope_tree(report_callback &report_error)
{
  Token error_token = Token::invalid();
  const char *error_msg = nullptr;

  size_t predicted_scope_count = lex.token_types.size() / 2;

  ScopeStack stack(predicted_scope_count);

  stack.enter_scope(ScopeType::Global, 0);

  int in_template = 0;

  int tok_id = 0;

  for (; tok_id < lex.token_types.size(); tok_id++) {
    const TokenType type = lex.token_types[tok_id];

    const ScopeType current_scope = stack.back().type;

    switch (type) {
      case Hash:
        stack.enter_scope(ScopeType::Preprocessor, tok_id);
        /* Seek until the end of the directive. */
        while (true) {
          const TokenType type = lex.token_types[tok_id];
          if (type == EndOfFile) {
            tok_id--;
            break;
          }

          IndexRange range = lex.token_offsets[tok_id];
          std::string_view tok_str = {lex.str.substr(range.start, range.size)};
          size_t new_line_offset = -1;
          while ((new_line_offset = tok_str.find("\n", new_line_offset + 1)) != std::string::npos)
          {
            if (new_line_offset == 0 || tok_str[new_line_offset - 1] != '\\') {
              break;
            }
          }
          if (new_line_offset != std::string::npos) {
            break;
          }
          tok_id++;
        }
        stack.exit_scope(tok_id);
        break;
      case Assign:
        if (current_scope == ScopeType::Assignment) {
          /* Chained assignments. */
          stack.exit_scope(tok_id - 1);
        }
        stack.enter_scope(ScopeType::Assignment, tok_id);
        break;
      case BracketOpen: {
        /* Scan back identifier that could contain namespaces. */
        TokenType keyword;
        int pos = 2;
        do {
          keyword = (tok_id >= pos) ? TokenType(lex.token_types[tok_id - pos]) :
                                      TokenType::Invalid;
          pos += 3;
        } while (keyword != Invalid && keyword == Colon);

        /* Skip host_shared attribute for structures if any. */
        if (keyword == ']') {
          keyword = (tok_id >= pos) ? TokenType(lex.token_types[tok_id - pos]) :
                                      TokenType::Invalid;
          if (keyword == '[') {
            pos += 2;
            keyword = (tok_id >= pos) ? TokenType(lex.token_types[tok_id - pos]) :
                                        TokenType::Invalid;
          }
        }

        if (keyword == Struct || keyword == Class) {
          stack.enter_scope(ScopeType::Struct, tok_id);
        }
        else if (keyword == Enum) {
          stack.enter_scope(ScopeType::Local, tok_id);
        }
        else if (keyword == Namespace) {
          stack.enter_scope(ScopeType::Namespace, tok_id);
        }
        else if (current_scope == ScopeType::Global) {
          stack.enter_scope(ScopeType::Function, tok_id);
        }
        else if (current_scope == ScopeType::Struct) {
          stack.enter_scope(ScopeType::Function, tok_id);
        }
        else if (current_scope == ScopeType::Namespace) {
          stack.enter_scope(ScopeType::Function, tok_id);
        }
        else {
          stack.enter_scope(ScopeType::Local, tok_id);
        }
        break;
      }
      case ParOpen:
        if ((tok_id >= 1 && lex.token_types[tok_id - 1] == For) ||
            (tok_id >= 1 && lex.token_types[tok_id - 1] == While))
        {
          stack.enter_scope(ScopeType::LoopArgs, tok_id);
        }
        else if (tok_id >= 1 && lex.token_types[tok_id - 1] == Switch) {
          stack.enter_scope(ScopeType::SwitchArg, tok_id);
        }
        else if (current_scope == ScopeType::Global) {
          stack.enter_scope(ScopeType::FunctionArgs, tok_id);
        }
        else if (current_scope == ScopeType::Struct) {
          stack.enter_scope(ScopeType::FunctionArgs, tok_id);
        }
        else if ((current_scope == ScopeType::Function || current_scope == ScopeType::Local ||
                  current_scope == ScopeType::Assignment ||
                  current_scope == ScopeType::FunctionParam ||
                  current_scope == ScopeType::Subscript ||
                  current_scope == ScopeType::Attribute) &&
                 (tok_id >= 1 && lex.token_types[tok_id - 1] == Word))
        {
          stack.enter_scope(ScopeType::FunctionCall, tok_id);
        }
        else {
          stack.enter_scope(ScopeType::Local, tok_id);
        }
        break;
      case SquareOpen:
        if (tok_id >= 1 && lex.token_types[tok_id - 1] == SquareOpen) {
          stack.enter_scope(ScopeType::Attributes, tok_id);
        }
        else {
          stack.enter_scope(ScopeType::Subscript, tok_id);
        }
        break;
      case AngleOpen:
        if (tok_id >= 1) {
          char prev_char = lex.str[lex.token_offsets[tok_id - 1].last()];
          /* Rely on the fact that template are formatted without spaces but comparison isn't. */
          if ((prev_char != ' ' && prev_char != '\n' && prev_char != '<') ||
              lex.token_types[tok_id - 1] == Template)
          {
            stack.enter_scope(ScopeType::Template, tok_id);
            in_template++;
          }
        }
        break;
      case AngleClose:
        if (stack.back().type == ScopeType::Assignment && in_template > 0) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::TemplateArg) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::Template) {
          stack.exit_scope(tok_id);
          in_template--;
        }
        break;
      case BracketClose:
        if (stack.back().type == ScopeType::Assignment) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::Struct || stack.back().type == ScopeType::Local ||
            stack.back().type == ScopeType::Namespace ||
            stack.back().type == ScopeType::LoopBody ||
            stack.back().type == ScopeType::SwitchBody ||
            stack.back().type == ScopeType::Function || stack.back().type == ScopeType::Function)
        {
          stack.exit_scope(tok_id);
        }
        else {
          error_token = (*this)[tok_id];
          error_msg = "Unexpected '}' token";
          goto error;
        }
        break;
      case ParClose:
        if (stack.back().type == ScopeType::Assignment) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::FunctionArg) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::FunctionParam) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::LoopArg) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::LoopArgs ||
            stack.back().type == ScopeType::SwitchArg ||
            stack.back().type == ScopeType::FunctionArgs ||
            stack.back().type == ScopeType::FunctionCall || stack.back().type == ScopeType::Local)
        {
          stack.exit_scope(tok_id);
        }
        else {
          error_token = (*this)[tok_id];
          error_msg = "Unexpected ')' token";
          goto error;
        }
        break;
      case SquareClose:
        if (stack.back().type == ScopeType::Attribute) {
          stack.exit_scope(tok_id - 1);
        }
        stack.exit_scope(tok_id);
        break;
      case SemiColon:
        if (stack.back().type == ScopeType::Assignment) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::FunctionArg) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::TemplateArg) {
          stack.exit_scope(tok_id - 1);
        }
        if (stack.back().type == ScopeType::LoopArg) {
          stack.exit_scope(tok_id - 1);
        }
        break;
      case Comma:
        if (stack.back().type == ScopeType::Assignment) {
          stack.exit_scope(tok_id - 1);
        }
        switch (stack.back().type) {
          case ScopeType::FunctionArg:
          case ScopeType::FunctionParam:
          case ScopeType::TemplateArg:
          case ScopeType::Attribute:
            stack.exit_scope(tok_id - 1);
            break;
          default:
            break;
        }
        break;
      default:
        switch (current_scope) {
          case ScopeType::Attributes:
            stack.enter_scope(ScopeType::Attribute, tok_id);
            break;
          case ScopeType::FunctionArgs:
            stack.enter_scope(ScopeType::FunctionArg, tok_id);
            break;
          case ScopeType::FunctionCall:
            stack.enter_scope(ScopeType::FunctionParam, tok_id);
            break;
          case ScopeType::LoopArgs:
            stack.enter_scope(ScopeType::LoopArg, tok_id);
            break;
          case ScopeType::Template:
            stack.enter_scope(ScopeType::TemplateArg, tok_id);
            break;
          default:
            break;
        }
        break;
    }
  }

  tok_id = lex.token_types.size() - 1;

  if (stack.empty()) {
    error_token = (*this)[tok_id];
    error_msg = "Extraneous end of scope somewhere in that file";
    goto error;
  }

  if (stack.back().type == ScopeType::Preprocessor) {
    stack.exit_scope(tok_id - 1);
  }

  if (stack.back().type != ScopeType::Global) {
    ScopeStack::Item scope_item = stack.back();
    error_token = (*this)[stack.ranges[scope_item.index].start];
    error_msg = "Unterminated scope";
    goto error;
  }

  stack.exit_scope(tok_id);

  scope_types = std::move(stack.types);
  scope_ranges = std::move(stack.ranges);
  update_string_view();
  return;

error:
  report_error(
      error_token.line_number(), error_token.char_number(), error_token.line_str(), error_msg);
  /* Avoid out of bound access for the rest of the processing. Empty everything. */
  scope_types = {ScopeType::Global};
  scope_ranges = {IndexRange(0, 0)};
}

void ParserBase::build_token_to_scope_map()
{
  token_scope.clear();
  token_scope.resize(scope_ranges[0].size);

  std::stack<uint32_t> stack;

  int scope_id = 0;
  for (const IndexRange &range : scope_ranges) {
    std::fill(token_scope.begin() + range.start,
              token_scope.begin() + range.start + range.size,
              scope_id);
    scope_id++;
  }

  update_string_view();
}

Token ParserBase::operator[](int i) const
{
  return Token::from_position(this, i);
}

void LexerBase::update_string_view()
{
  this->token_types_str = std::string_view(reinterpret_cast<char *>(this->token_types.data()),
                                           this->token_types.size());
}

void ParserBase::update_string_view()
{
  this->scope_types_str = std::string_view(reinterpret_cast<char *>(this->scope_types.data()),
                                           this->scope_types.size());
}

bool MutableString::apply_mutations(LexerBase &lexer, const bool all_mutation_ordered)
{
  if (mutations_.empty()) {
    return false;
  }

  if (!all_mutation_ordered) {
    /* Order mutations so that they can be applied in one pass. */
    std::stable_sort(mutations_.begin(), mutations_.end());
  }
#ifndef NDEBUG
  else {
    assert(std::is_sorted(mutations_.begin(), mutations_.end()));
  }
#endif

  /* Make sure to pad the input string in case of insertion after the last char. */
  bool added_trailing_new_line = false;
  if (str_.back() != '\n') {
    str_ += '\n';
    added_trailing_new_line = true;
  }

  std::string result;
  result.reserve(str_.size());

  int64_t offset = 0;
  for (const Mutation &mut : mutations_) {
    size_t start = mut.src_range.start;
    size_t end = start + mut.src_range.size;
    /* Copy unchanged text. */
    result.append(str_.data() + offset, start - offset);
    /* Append replacement. */
    result.append(mut.replacement);
    offset = end;
  }
  result.append(str_.data() + offset, str_.size() - offset);

  str_ = std::move(result);

  mutations_.clear();

  if (added_trailing_new_line) {
    str_.pop_back();
  }
  /* String have changed. Update string view. */
  lexer.str = str_;
  return true;
}

}  // namespace blender::gpu::shader::parser
