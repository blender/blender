/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 */

#include "parser.hh"
#include "intermediate.hh"
#include "scope.hh"
#include "token.hh"

#include <algorithm>
#include <stack>

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
  if (prev == ']' && prev.prev().scope().type() != ScopeType::Attributes) {
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
  if (next == ']' && next.next().scope().type() != ScopeType::Attributes) {
    return next.next().scope();
  }
  return Scope::invalid();
}

/** If `keep_whitespace` is false, white-spaces are merged with the previous token. */
void Parser::tokenize(const bool keep_whitespace)
{
  if (str.empty()) {
    *this = {};
    return;
  }

  {
    /* Tokenization. */
    token_types.clear();
    token_offsets.clear();

    token_types += char(to_type(str[0]));
    token_offsets.offsets.emplace_back(0);

    /* When doing white-space merging, keep knowledge about whether previous char was white-space.
     * This allows to still split words on spaces. */
    bool prev_was_whitespace = (token_types[0] == NewLine || token_types[0] == Space);
    bool inside_preprocessor_directive = token_types[0] == Hash;
    bool next_character_is_escape = false;
    bool inside_string = false;

    char curr_c = str[0];
    char prev_c = str[0];
    int offset = 0;
    for (const char c : str.substr(1)) {
      offset++;
      TokenType type = to_type(c);
      TokenType prev = TokenType(token_types.back());

      std::swap(curr_c, prev_c);
      curr_c = c;

      /* Merge string literal. */
      if (inside_string) {
        if (!next_character_is_escape && c == '\"') {
          inside_string = false;
        }
        next_character_is_escape = c == '\\';
        continue;
      }
      if (c == '\"') {
        inside_string = true;
      }
      /* Detect preprocessor directive newlines `\\\n`. */
      if (prev == Backslash && type == NewLine) {
        token_types.back() = PreprocessorNewline;
        continue;
      }
      /* Make sure to keep the ending newline for a preprocessor directive. */
      if (inside_preprocessor_directive && type == NewLine) {
        inside_preprocessor_directive = false;
        token_types += char(type);
        token_offsets.offsets.emplace_back(offset);
        continue;
      }
      if (type == Hash) {
        inside_preprocessor_directive = true;
      }
      /* Merge newlines and spaces with previous token. */
      if (!keep_whitespace && (type == NewLine || type == Space)) {
        prev_was_whitespace = true;
        continue;
      }
      /* Merge '=='. */
      if (prev == Assign && type == Assign) {
        token_types.back() = Equal;
        continue;
      }
      /* Merge '!='. */
      if (prev == '!' && type == Assign) {
        token_types.back() = NotEqual;
        continue;
      }
      /* Merge '>='. */
      if (prev == '>' && type == Assign) {
        token_types.back() = GEqual;
        continue;
      }
      /* Merge '<='. */
      if (prev == '<' && type == Assign) {
        token_types.back() = LEqual;
        continue;
      }
      /* Merge '->'. */
      if (prev == '-' && type == '>') {
        token_types.back() = Deref;
        continue;
      }
      /* If digit is part of word. */
      if (type == Number && prev == Word && !prev_was_whitespace) {
        continue;
      }
      /* If 'x' is part of hex literal. */
      if (c == 'x' && prev == Number) {
        continue;
      }
      /* If 'A-F' is part of hex literal. */
      if (c >= 'A' && c <= 'F' && prev == Number) {
        continue;
      }
      /* If 'a-f' is part of hex literal. */
      if (c >= 'a' && c <= 'f' && prev == Number) {
        continue;
      }
      /* If 'u' is part of unsigned int literal. */
      if (c == 'u' && prev == Number) {
        continue;
      }
      /* If dot is part of float literal. */
      if (type == Dot && prev == Number) {
        continue;
      }
      /* If 'f' suffix is part of float literal. */
      if (c == 'f' && prev == Number) {
        continue;
      }
      /* If 'e' is part of float literal. */
      if (c == 'e' && prev == Number) {
        continue;
      }
      /* If sign is part of float literal after exponent. */
      if ((c == '+' || c == '-') && prev_c == 'e') {
        continue;
      }
      /* Detect increment. */
      if (type == '+' && prev == '+') {
        token_types.back() = Increment;
        continue;
      }
      /* Detect decrement. */
      if (type == '+' && prev == '+') {
        token_types.back() = Decrement;
        continue;
      }
      /* Only merge these token. Otherwise, always emit a token. */
      if (type != Word && type != NewLine && type != Space && type != Number) {
        prev = Word;
      }
      /* Split words on white-spaces even when merging. */
      if (!keep_whitespace && type == Word && prev_was_whitespace) {
        prev = Space;
        prev_was_whitespace = false;
      }
      /* Emit a token if we don't merge. */
      if (type != prev) {
        token_types += char(type);
        token_offsets.offsets.emplace_back(offset);
      }
    }
    offset++;
    token_offsets.offsets.emplace_back(offset);
  }
  {
    /* Keywords detection. */
    int tok_id = -1;
    for (char &c : token_types) {
      tok_id++;
      if (TokenType(c) == Word) {
        IndexRange range = token_offsets[tok_id];
        std::string word = str.substr(range.start, range.size);
        if (!keep_whitespace) {
          size_t last_non_whitespace = word.find_last_not_of(" \n");
          if (last_non_whitespace != std::string::npos) {
            word = word.substr(0, last_non_whitespace + 1);
          }
        }

        if (word == "namespace") {
          c = Namespace;
        }
        else if (word == "struct") {
          c = Struct;
        }
        else if (word == "class") {
          c = Class;
        }
        else if (word == "const") {
          c = Const;
        }
        else if (word == "constexpr") {
          c = Constexpr;
        }
        else if (word == "return") {
          c = Return;
        }
        else if (word == "break") {
          c = Break;
        }
        else if (word == "continue") {
          c = Continue;
        }
        else if (word == "case") {
          c = Case;
        }
        else if (word == "switch") {
          c = Switch;
        }
        else if (word == "if") {
          c = If;
        }
        else if (word == "else") {
          c = Else;
        }
        else if (word == "while") {
          c = While;
        }
        else if (word == "do") {
          c = Do;
        }
        else if (word == "for") {
          c = For;
        }
        else if (word == "template") {
          c = Template;
        }
        else if (word == "this") {
          c = This;
        }
        else if (word == "static") {
          c = Static;
        }
        else if (word == "private") {
          c = Private;
        }
        else if (word == "public") {
          c = Public;
        }
        else if (word == "enum") {
          c = Enum;
        }
        else if (word == "using") {
          c = Using;
        }
        else if (word == "inline") {
          c = Inline;
        }
        else if (word == "union") {
          c = Union;
        }
      }
    }
  }
}

void Parser::parse_scopes(report_callback &report_error)
{
  {
    /* Scope detection. */
    scope_ranges.clear();
    scope_types.clear();

    struct ScopeItem {
      ScopeType type;
      size_t start;
      int index;
    };

    int scope_index = 0;
    std::stack<ScopeItem> scopes;

    auto enter_scope = [&](ScopeType type, size_t start_tok_id) {
      scopes.emplace(ScopeItem{type, start_tok_id, scope_index++});
      scope_ranges.emplace_back(start_tok_id, 1);
      scope_types += char(type);
    };

    auto exit_scope = [&](int end_tok_id) {
      if (scopes.empty()) {
        return;
      }
      ScopeItem scope = scopes.top();
      scope_ranges[scope.index].size = end_tok_id - scope.start + 1;
      scopes.pop();
    };

    enter_scope(ScopeType::Global, 0);

    int in_template = 0;

    int tok_id = -1;
    for (char &c : token_types) {
      tok_id++;

      if (scopes.top().type == ScopeType::Preprocessor) {
        if (TokenType(c) == NewLine) {
          exit_scope(tok_id);
        }
        else {
          /* Do nothing. Enclose all preprocessor lines together. */
          continue;
        }
      }

      switch (TokenType(c)) {
        case Hash:
          enter_scope(ScopeType::Preprocessor, tok_id);
          break;
        case Assign:
          if (scopes.top().type == ScopeType::Assignment) {
            /* Chained assignments. */
            exit_scope(tok_id - 1);
          }
          enter_scope(ScopeType::Assignment, tok_id);
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
              keyword = (tok_id >= pos) ? TokenType(token_types[tok_id - pos]) :
                                          TokenType::Invalid;
            }
          }

          if (keyword == Struct || keyword == Class) {
            enter_scope(ScopeType::Struct, tok_id);
          }
          else if (keyword == Enum) {
            enter_scope(ScopeType::Local, tok_id);
          }
          else if (keyword == Namespace) {
            enter_scope(ScopeType::Namespace, tok_id);
          }
          else if (scopes.top().type == ScopeType::Global) {
            enter_scope(ScopeType::Function, tok_id);
          }
          else if (scopes.top().type == ScopeType::Struct) {
            enter_scope(ScopeType::Function, tok_id);
          }
          else if (scopes.top().type == ScopeType::Namespace) {
            enter_scope(ScopeType::Function, tok_id);
          }
          else {
            enter_scope(ScopeType::Local, tok_id);
          }
          break;
        }
        case ParOpen:
          if ((tok_id >= 1 && token_types[tok_id - 1] == For) ||
              (tok_id >= 1 && token_types[tok_id - 1] == While))
          {
            enter_scope(ScopeType::LoopArgs, tok_id);
          }
          else if (tok_id >= 1 && token_types[tok_id - 1] == Switch) {
            enter_scope(ScopeType::SwitchArg, tok_id);
          }
          else if (scopes.top().type == ScopeType::Global) {
            enter_scope(ScopeType::FunctionArgs, tok_id);
          }
          else if (scopes.top().type == ScopeType::Struct) {
            enter_scope(ScopeType::FunctionArgs, tok_id);
          }
          else if ((scopes.top().type == ScopeType::Function ||
                    scopes.top().type == ScopeType::Local ||
                    scopes.top().type == ScopeType::Attribute) &&
                   (tok_id >= 1 && token_types[tok_id - 1] == Word))
          {
            enter_scope(ScopeType::FunctionCall, tok_id);
          }
          else {
            enter_scope(ScopeType::Local, tok_id);
          }
          break;
        case SquareOpen:
          if (tok_id >= 1 && token_types[tok_id - 1] == SquareOpen) {
            enter_scope(ScopeType::Attributes, tok_id);
          }
          else {
            enter_scope(ScopeType::Subscript, tok_id);
          }
          break;
        case AngleOpen:
          if (tok_id >= 1) {
            char prev_char = str[token_offsets[tok_id - 1].last()];
            /* Rely on the fact that template are formatted without spaces but comparison isn't. */
            if ((prev_char != ' ' && prev_char != '\n' && prev_char != '<') ||
                token_types[tok_id - 1] == Template)
            {
              enter_scope(ScopeType::Template, tok_id);
              in_template++;
            }
          }
          break;
        case AngleClose:
          if (in_template > 0 && scopes.top().type == ScopeType::Assignment) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::TemplateArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::Template) {
            exit_scope(tok_id);
            in_template--;
          }
          break;
        case BracketClose:
          if (scopes.top().type == ScopeType::Assignment) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::Struct || scopes.top().type == ScopeType::Local ||
              scopes.top().type == ScopeType::Namespace ||
              scopes.top().type == ScopeType::LoopBody ||
              scopes.top().type == ScopeType::SwitchBody ||
              scopes.top().type == ScopeType::Function || scopes.top().type == ScopeType::Function)
          {
            exit_scope(tok_id);
          }
          else {
            Token token = Token::from_position(this, tok_id);
            report_error(token.line_number(),
                         token.char_number(),
                         token.line_str(),
                         "Unexpected '}' token");
            /* Avoid out of bound access for the rest of the processing. Empty everything. */
            *this = {};
            return;
          }
          break;
        case ParClose:
          if (scopes.top().type == ScopeType::Assignment) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::FunctionArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::FunctionParam) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::LoopArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::LoopArgs ||
              scopes.top().type == ScopeType::SwitchArg ||
              scopes.top().type == ScopeType::FunctionArgs ||
              scopes.top().type == ScopeType::FunctionCall ||
              scopes.top().type == ScopeType::Local)
          {
            exit_scope(tok_id);
          }
          else {
            Token token = Token::from_position(this, tok_id);
            report_error(token.line_number(),
                         token.char_number(),
                         token.line_str(),
                         "Unexpected ')' token");
            /* Avoid out of bound access for the rest of the processing. Empty everything. */
            *this = {};
            return;
          }
          break;
        case SquareClose:
          if (scopes.top().type == ScopeType::Attribute) {
            exit_scope(tok_id - 1);
          }
          exit_scope(tok_id);
          break;
        case SemiColon:
          if (scopes.top().type == ScopeType::Assignment) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::FunctionArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::TemplateArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::LoopArg) {
            exit_scope(tok_id - 1);
          }
          break;
        case Comma:
          if (scopes.top().type == ScopeType::Assignment) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::FunctionArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::FunctionParam) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::TemplateArg) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::Attributes) {
            exit_scope(tok_id - 1);
          }
          if (scopes.top().type == ScopeType::Attribute) {
            exit_scope(tok_id - 1);
          }
          break;
        default:
          if (scopes.top().type == ScopeType::Attributes) {
            enter_scope(ScopeType::Attribute, tok_id);
          }
          if (scopes.top().type == ScopeType::FunctionArgs) {
            enter_scope(ScopeType::FunctionArg, tok_id);
          }
          if (scopes.top().type == ScopeType::FunctionCall) {
            enter_scope(ScopeType::FunctionParam, tok_id);
          }
          if (scopes.top().type == ScopeType::LoopArgs) {
            enter_scope(ScopeType::LoopArg, tok_id);
          }
          if (scopes.top().type == ScopeType::Template) {
            enter_scope(ScopeType::TemplateArg, tok_id);
          }
          break;
      }
    }

    if (scopes.empty()) {
      Token token = Token::from_position(this, tok_id);
      report_error(token.line_number(),
                   token.char_number(),
                   token.line_str(),
                   "Extraneous end of scope somewhere in that file");

      /* Avoid out of bound access for the rest of the processing. Empty everything. */
      *this = {};
      return;
    }

    if (scopes.top().type == ScopeType::Preprocessor) {
      exit_scope(tok_id - 1);
    }

    if (scopes.top().type != ScopeType::Global) {
      ScopeItem scope_item = scopes.top();
      Token token = Token::from_position(this, scope_ranges[scope_item.index].start);
      report_error(
          token.line_number(), token.char_number(), token.line_str(), "Unterminated scope");

      /* Avoid out of bound access for the rest of the processing. Empty everything. */
      *this = {};
      return;
    }

    exit_scope(tok_id);
  }
  {
    token_scope.clear();
    token_scope.resize(scope_ranges[0].size);

    int scope_id = -1;
    for (const IndexRange &range : scope_ranges) {
      scope_id++;
      for (int i = 0; i < range.size; i++) {
        int j = range.start + i;
        token_scope[j] = scope_id;
      }
    }
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

  int64_t offset = 0;
  for (const Mutation &mut : mutations_) {
    data_.str.replace(mut.src_range.start + offset, mut.src_range.size, mut.replacement);
    offset += mut.replacement.size() - mut.src_range.size;
  }
  mutations_.clear();

  if (added_trailing_new_line) {
    data_.str.pop_back();
  }
  return true;
}

}  // namespace blender::gpu::shader::parser
