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

#include <algorithm>
#include <array>
#include <stack>

#if defined(_MSC_VER)
#  define always_inline __forceinline
#else
#  define always_inline inline __attribute__((always_inline))
#endif

namespace blender::gpu::shader::parser {

size_t line_number(const std::string &str, size_t pos)
{
  std::string directive = "#line ";
  /* String to count the number of line. */
  std::string sub_str = str.substr(0, pos);
  size_t nearest_line_directive = sub_str.rfind(directive);
  size_t line_count = 1;
  if (nearest_line_directive != std::string::npos) {
    sub_str = sub_str.substr(nearest_line_directive + directive.size());
    line_count = std::stoll(sub_str) - 1;
  }
  return line_count + std::count(sub_str.begin(), sub_str.end(), '\n');
}

size_t char_number(const std::string &str, size_t pos)
{
  std::string sub_str = str.substr(0, pos);
  size_t nearest_line_directive = sub_str.rfind('\n');
  return (nearest_line_directive == std::string::npos) ?
             (sub_str.size()) :
             (sub_str.size() - nearest_line_directive - 1);
}

std::string line_str(const std::string &str, size_t pos)
{
  size_t start = str.rfind('\n', pos);
  size_t end = str.find('\n', pos);
  start = (start != std::string::npos) ? start + 1 : 0;
  return str.substr(start, end - start);
}

Scope Token::scope() const
{
  if (this->is_invalid()) {
    return Scope::invalid();
  }
  return Scope::from_position(data, data->token_scope[index]);
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

struct TokenData {
  std::vector<TokenType> types;
  OffsetIndices offsets;
  /* Word size without white-spaces. */
  std::vector<uint32_t> sizes;
};

void TokenStream::lexical_analysis(ParserStage stop_after)
{
  if (str.empty()) {
    *this = {};
    return;
  }

  TokenData data;

  tokenize(data);
  if (stop_after == Tokenize) {
    goto end;
  }

  merge_tokens(data);
  if (stop_after == MergeTokens) {
    goto end;
  }

  identify_keywords(data);

end:
  /* TODO(fclem): Get rid of this.*/
  /* Convert vector of char to string for faster lookups. */
  this->token_types = std::string(reinterpret_cast<char *>(data.types.data()), data.types.size());
  this->token_offsets = std::move(data.offsets);
}

static always_inline TokenType to_type(const char c)
{
  switch (c) {
    case '\n':
      return TokenType::NewLine;
    case ' ':
      return TokenType::Space;
    case '#':
      return TokenType::Hash;
    case '&':
      return TokenType::Ampersand;
    case '^':
      return TokenType::Caret;
    case '|':
      return TokenType::Pipe;
    case '%':
      return TokenType::Percent;
    case '.':
      return TokenType::Dot;
    case '(':
      return TokenType::ParOpen;
    case ')':
      return TokenType::ParClose;
    case '{':
      return TokenType::BracketOpen;
    case '}':
      return TokenType::BracketClose;
    case '[':
      return TokenType::SquareOpen;
    case ']':
      return TokenType::SquareClose;
    case '<':
      return TokenType::AngleOpen;
    case '>':
      return TokenType::AngleClose;
    case '=':
      return TokenType::Assign;
    case '!':
      return TokenType::Not;
    case '*':
      return TokenType::Star;
    case '-':
      return TokenType::Minus;
    case '+':
      return TokenType::Plus;
    case '/':
      return TokenType::Divide;
    case '~':
      return TokenType::Tilde;
    case '\\':
      return TokenType::Backslash;
    case '\"':
      return TokenType::String;
    case '?':
      return TokenType::Question;
    case ':':
      return TokenType::Colon;
    case ',':
      return TokenType::Comma;
    case ';':
      return TokenType::SemiColon;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return TokenType::Number;
    default:
      return TokenType::Word;
  }
}

static always_inline bool always_split_token(const TokenType c)
{
  switch (c) {
    case TokenType::Number:
    case TokenType::Word:
    case TokenType::NewLine:
    case TokenType::Space:
      return false;
    default:
      return true;
  }
}

static const std::array<std::pair<TokenType, bool>, 256> token_table = [] {
  std::array<std::pair<TokenType, bool>, 256> t;
  for (int i = 0; i < 256; ++i) {
    TokenType type = to_type(i);
    t[i] = {type, always_split_token(type)};
  }
  return t;
}();

/* Table lookup variant. Much faster than switch statement.  */
static always_inline std::pair<TokenType, bool> to_type_table(const unsigned char c)
{
  return token_table[c];
}

void TokenStream::tokenize(TokenData &tokens)
{
  /* Reserve space inside the data structures. Allocate 1 token per char as we do not want to
   * resize or check for size inside the hot loop. */
  tokens.types.resize(str.size());
  tokens.offsets.offsets.resize(str.size() + 1);

  TokenType type = TokenType::Invalid;

  TokenType *types_raw = tokens.types.data();
  uint32_t *offsets_raw = tokens.offsets.offsets.data();

  int offset = 0, cursor = 0;
  for (const char c : str) {
    const TokenType prev = type;
    auto [tok_type, always_split] = to_type_table(c);
    type = tok_type;
    /* Its faster to overwrite the previous value with the same value
     * than having a condition. */
    types_raw[cursor] = type;
    offsets_raw[cursor] = offset++;
    /* Split if type mismatch. */
    cursor += (type != prev || always_split);
  }
  /* Set end of last token. */
  offsets_raw[cursor] = offset++;
  /* Resize to the actual usage. */
  tokens.types.resize(cursor);
  tokens.offsets.offsets.resize(cursor + 1);
}

static const std::array<bool, 256> num_literal_table = [] {
  std::array<bool, 256> t;
  for (int c = 0; c < 256; ++c) {
    t[c] = true;
    /* If dot is part of float literal. */
    if (c == '.') {
      continue; /* Merge. */
    }
    /* If 'A-F' is part of hex literal. */
    if (c >= 'A' && c <= 'F') {
      continue; /* Merge. */
    }
    /* If 'a-f' is part of hex literal. */
    /* If 'f' suffix is part of float literal. */
    /* If 'e' is part of float literal. */
    if (c >= 'a' && c <= 'f') {
      continue; /* Merge. */
    }
    /* If 'x' is part of hex literal. */
    if (c == 'x') {
      continue; /* Merge. */
    }
    /* If 'u' is part of unsigned int literal. */
    if (c == 'u') {
      continue; /* Merge. */
    }
    t[c] = false;
  }
  return t;
}();

/* Table lookup variant. Much faster than switch statement.  */
static always_inline bool is_char_part_of_number_literal(const unsigned char c)
{
  return num_literal_table[c];
}

static always_inline bool is_word_part_of_number_literal(const std::string_view str)
{
  for (char c : str) {
    if (!is_char_part_of_number_literal(c)) {
      return false;
    }
  }
  return true;
}

static always_inline bool is_whitespace(TokenType t)
{
  return (t == ' ') || (t == '\n');
}

void TokenStream::merge_tokens(TokenData &tokens)
{
  tokens.sizes.resize(tokens.types.size());

  const char *str_raw = str.data();
  TokenType *types_raw = tokens.types.data();
  uint32_t *offsets_raw = tokens.offsets.offsets.data();
  uint32_t *sizes_raw = tokens.sizes.data();

  /* Never merge the first token. We don't want to loose it. */
  TokenType prev = types_raw[0];
  sizes_raw[0] = tokens.offsets[0].size;

  /* State. */
  bool after_whitespace = is_whitespace(prev);
  bool inside_escaped_char = false;
  bool inside_preprocessor_directive = false;
  bool inside_string = false;
  bool inside_number = false;

  uint32_t cursor = 1;
  for (uint32_t i = 1; i < tokens.types.size(); i++) {
    bool emit = true;
#define merge_if(a) emit &= !(a)

    TokenType tok = types_raw[i];
    uint32_t offset = offsets_raw[i];
    uint32_t tok_size = offsets_raw[i + 1] - offset;

#ifndef NDEBUG
    std::string_view tok_str{str_raw + offset, tok_size};
#endif

    /* Merge string literal. */
    merge_if(inside_string);

    /* Flip flop inside string when finding and unescaped quote. */
    if (tok == String && !inside_escaped_char) {
      inside_string = !inside_string;
    }
    inside_escaped_char = inside_string && (tok == '\\');

    /* Merge number literal. */
    if (inside_number) {
      merge_if((tok == Word || tok == '.') &&
               is_word_part_of_number_literal({str_raw + offset, tok_size}));
      /* If sign is part of float literal after exponent. */
      merge_if((tok == '+' || tok == '-') && str_raw[offset - 1] == 'e');

      /* Disable if we do not emit. */
      inside_number = (tok == Number) || !emit;
    }

    switch (tok) {
      case Hash:
        inside_preprocessor_directive = true;
        break;

      case NewLine:
        after_whitespace = true;
        /* Preprocessor directives. */
        if (inside_preprocessor_directive) {
          /* Detect preprocessor directive newlines `\\\n`. */
          if (prev == Backslash) {
            types_raw[cursor - 1] = PreprocessorNewline;
            continue;
          }
          inside_preprocessor_directive = false;
          /* Make sure to keep the ending newline for a preprocessor directive. */
          break;
        }
        continue;

      case Space:
        after_whitespace = true;
        continue;

      case Word:
        /* Merge words that contain numbers that were split by the tokenizer. */
        if (prev == Word && !after_whitespace) {
          sizes_raw[cursor - 1] += tok_size;
          continue;
        }
        sizes_raw[cursor] = tok_size;
        break;

      case Number:
        /* If digit is part of word. */
        if (prev == Word && !after_whitespace) {
          sizes_raw[cursor - 1] += tok_size;
          continue;
        }
        if (prev == Number) {
          continue;
        }
        inside_number = true;
        break;

      case '=':
        /* Merge '=='. */
        if (prev == '=') {
          types_raw[cursor - 1] = Equal;
          continue;
        }
        /* Merge '!='. */
        if (prev == '!') {
          types_raw[cursor - 1] = NotEqual;
          continue;
        }
        /* Merge '>='. */
        if (prev == '>') {
          types_raw[cursor - 1] = GEqual;
          continue;
        }
        /* Merge '<='. */
        if (prev == '<') {
          types_raw[cursor - 1] = LEqual;
          continue;
        }
        break;

      case '>':
        /* Merge '->'. */
        if (prev == '-') {
          types_raw[cursor - 1] = Deref;
          continue;
        }
        break;

      case '+':
        /* Detect increment. */
        if (prev == '+') {
          types_raw[cursor - 1] = Increment;
          continue;
        }
        break;

      case '-':
        /* Detect decrement. */
        if (prev == '-') {
          types_raw[cursor - 1] = Decrement;
          continue;
        }
        break;

      default:
        break;
    }
    after_whitespace = false;

    if (emit) {
      prev = tok;
      types_raw[cursor] = tok;
      offsets_raw[cursor] = offset;
      cursor += 1;
    }
  }

  tokens.types.resize(cursor);

  tokens.offsets.offsets[cursor] = tokens.offsets.offsets.back();
  tokens.offsets.offsets.resize(cursor + 1);
}

static always_inline TokenType type_lookup(std::string_view s)
{
  switch (s.size()) {
    case 2:
      switch (s[0]) {
        case 'd':
          if (s == "do") {
            return Do;
          }
          break;
        case 'i':
          if (s == "if") {
            return If;
          }
          break;
      }
      break;
    case 3:
      switch (s[0]) {
        case 'f':
          if (s == "for") {
            return For;
          }
          break;
      }
      break;
    case 4:
      switch (s[0]) {
        case 'c':
          if (s == "case") {
            return Case;
          }
          break;
        case 'e':
          if (s == "else") {
            return Else;
          }
          if (s == "enum") {
            return Enum;
          }
          break;
        case 't':
          if (s == "this") {
            return This;
          }
          break;
      }
      break;
    case 5:
      switch (s[0]) {
        case 'b':
          if (s == "break") {
            return Break;
          }
          break;
        case 'c':
          if (s == "class") {
            return Class;
          }
          if (s == "const") {
            return Const;
          }
          break;
        case 'u':
          if (s == "union") {
            return Union;
          }
          if (s == "using") {
            return Using;
          }
          break;
        case 'w':
          if (s == "while") {
            return While;
          }
          break;
      }
      break;
    case 6:
      switch (s[0]) {
        case 'i':
          if (s == "inline") {
            return Inline;
          }
          break;
        case 'p':
          if (s == "public") {
            return Public;
          }
          break;
        case 'r':
          if (s == "return") {
            return Return;
          }
          break;
        case 's':
          if (s == "static") {
            return Static;
          }
          if (s == "struct") {
            return Struct;
          }
          if (s == "switch") {
            return Switch;
          }
          break;
      }
      break;
    case 7:
      switch (s[0]) {
        case 'p':
          if (s == "private") {
            return Private;
          }
          break;
      }
      break;

    case 8:
      switch (s[0]) {
        case 'c':
          if (s == "continue") {
            return Continue;
          }
          break;
        case 't':
          if (s == "template") {
            return Template;
          }
          break;
      }
      break;

    case 9:
      switch (s[0]) {
        case 'c':
          if (s == "constexpr") {
            return Constexpr;
          }
          break;
        case 'n':
          if (s == "namespace") {
            return Namespace;
          }
          break;
      }
      break;
  }
  return Word;
}

void TokenStream::identify_keywords(TokenData &tokens)
{
  int tok_id = -1;
  for (TokenType &type : tokens.types) {
    tok_id++;
    if (type == Word) {
      IndexRange range = tokens.offsets[tok_id];
      type = type_lookup({str.data() + range.start, size_t(tokens.sizes[tok_id])});
    }
  }
}

void TokenStream::semantic_analysis(ParserStage stop_after, report_callback &report_error)
{
  if (stop_after == BuildScopeTree) {
    build_scope_tree(report_error);
  }
  else {
    this->scope_types = "G";
    this->scope_ranges = {IndexRange(0, token_types.size())};
  }
  build_token_to_scope_map();
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

void TokenStream::build_scope_tree(report_callback &report_error)
{
  Token error_token = Token::invalid();
  const char *error_msg = nullptr;

  size_t predicted_scope_count = token_types.size() / 2;

  ScopeStack stack(predicted_scope_count);

  stack.enter_scope(ScopeType::Global, 0);

  int in_template = 0;

  int tok_id = -1;
  for (const char &c : token_types) {
    tok_id++;

    const ScopeType current_scope = stack.back().type;

    if (stack.back().type == ScopeType::Preprocessor) {  // Here
      if (TokenType(c) == NewLine) {
        stack.exit_scope(tok_id);
      }
      else {
        /* Do nothing. Enclose all preprocessor lines together. */
        continue;
      }
    }

    switch (TokenType(c)) {
      case Hash:
        stack.enter_scope(ScopeType::Preprocessor, tok_id);
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
          keyword = (tok_id >= pos) ? TokenType(token_types[tok_id - pos]) : TokenType::Invalid;
          pos += 3;
        } while (keyword != Invalid && keyword == Colon);

        /* Skip host_shared attribute for structures if any. */
        if (keyword == ']') {
          keyword = (tok_id >= pos) ? TokenType(token_types[tok_id - pos]) : TokenType::Invalid;
          if (keyword == '[') {
            pos += 2;
            keyword = (tok_id >= pos) ? TokenType(token_types[tok_id - pos]) : TokenType::Invalid;
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
        if ((tok_id >= 1 && token_types[tok_id - 1] == For) ||
            (tok_id >= 1 && token_types[tok_id - 1] == While))
        {
          stack.enter_scope(ScopeType::LoopArgs, tok_id);
        }
        else if (tok_id >= 1 && token_types[tok_id - 1] == Switch) {
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
                 (tok_id >= 1 && token_types[tok_id - 1] == Word))
        {
          stack.enter_scope(ScopeType::FunctionCall, tok_id);
        }
        else {
          stack.enter_scope(ScopeType::Local, tok_id);
        }
        break;
      case SquareOpen:
        if (tok_id >= 1 && token_types[tok_id - 1] == SquareOpen) {
          stack.enter_scope(ScopeType::Attributes, tok_id);
        }
        else {
          stack.enter_scope(ScopeType::Subscript, tok_id);
        }
        break;
      case AngleOpen:
        if (tok_id >= 1) {
          char prev_char = str[token_offsets[tok_id - 1].last()];
          /* Rely on the fact that template are formatted without spaces but comparison isn't. */
          if ((prev_char != ' ' && prev_char != '\n' && prev_char != '<') ||
              token_types[tok_id - 1] == Template)
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
          error_token = Token::from_position(this, tok_id);
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
          error_token = Token::from_position(this, tok_id);
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

  if (stack.empty()) {
    error_token = Token::from_position(this, tok_id);
    error_msg = "Extraneous end of scope somewhere in that file";
    goto error;
  }

  if (stack.back().type == ScopeType::Preprocessor) {
    stack.exit_scope(tok_id - 1);
  }

  if (stack.back().type != ScopeType::Global) {
    ScopeStack::Item scope_item = stack.back();
    error_token = Token::from_position(this, scope_ranges[scope_item.index].start);
    error_msg = "Unterminated scope";
    goto error;
  }

  stack.exit_scope(tok_id);

  /* Convert vector of char to string for faster lookups. */
  this->scope_types = std::string(reinterpret_cast<char *>(stack.types.data()),
                                  stack.types.size());
  this->scope_ranges = std::move(stack.ranges);
  return;

error:
  report_error(
      error_token.line_number(), error_token.char_number(), error_token.line_str(), error_msg);
  /* Avoid out of bound access for the rest of the processing. Empty everything. */
  *this = {};
}

void TokenStream::build_token_to_scope_map()
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
}

/* Return true if any mutation was applied. */
bool IntermediateForm::only_apply_mutations()
{
  if (mutations_.empty()) {
    return false;
  }

  /* Order mutations so that they can be applied in one pass. */
  std::stable_sort(mutations_.begin(), mutations_.end());

  /* Make sure to pad the input string in case of insertion after the last char. */
  bool added_trailing_new_line = false;
  if (data_.str.back() != '\n') {
    data_.str += '\n';
    added_trailing_new_line = true;
  }

  std::string result;
  result.reserve(data_.str.size());

  int64_t offset = 0;
  for (const Mutation &mut : mutations_) {
    size_t start = mut.src_range.start;
    size_t end = start + mut.src_range.size;
    /* Copy unchanged text. */
    result.append(data_.str.data() + offset, start - offset);
    /* Append replacement. */
    result.append(mut.replacement);
    offset = end;
  }
  result.append(data_.str.data() + offset, data_.str.size() - offset);

  data_.str = std::move(result);

  mutations_.clear();

  if (added_trailing_new_line) {
    data_.str.pop_back();
  }
  return true;
}

void IntermediateForm::parse(ParserStage stop_after, report_callback &report_error)
{
  TimeIt::Duration lex_time, sem_time;
  {
    TimeIt time_it(lex_time);
    data_.lexical_analysis(stop_after);
  }
  {
    TimeIt time_it(sem_time);
    data_.semantic_analysis(stop_after, report_error);
  }
  lexical_time = lex_time.count();
  semantic_time = sem_time.count();
}

}  // namespace blender::gpu::shader::parser
