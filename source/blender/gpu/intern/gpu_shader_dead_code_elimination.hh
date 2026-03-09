/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_bit_vector.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "shader_tool/lexit/lexit.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Streaming Dead Code Eliminator.
 * \{ */

/* Token stream that parses (some) symbols definitions, build a graph of usage and then prune
 * unused definitions for a given set of entry point functions. */
struct PruningStream {
  using Token = lexit::Token;
  using TokenType = lexit::TokenType;
  using TokenAtom = lexit::TokenAtom;

 private:
  /* Data to output. Think of it as a lazy string builder. */
  Vector<StringRef> stream;
  /* Total output length. */
  size_t total_length = 0;
  /* Ranges of character in the stream to remove when constructing the result. */
  Vector<std::pair<int32_t, int32_t>> removals;

  /* Simple circular buffer to access last few token data. */
  class TokenHistoryBuffer {
   private:
    std::array<Token, 2> buffer;
    int head = 0;

   public:
    TokenHistoryBuffer(Token invalid) : buffer({invalid, invalid}) {}

    void push(Token tok)
    {
      buffer[head] = tok;
      head = (head + 1) % buffer.size();
    }

    /* Access elements from newest (0) to oldest (count-1). */
    const Token &operator[](int index) const
    {
      BLI_assert(index < buffer.size());
      int actual_index = (head + buffer.size() - index - 1) % buffer.size();
      return buffer[actual_index];
    }
  } token_history;

  struct FunctionDeclaration {
    Token type, name, end;
  };

  struct FunctionGraph {
    Vector<FunctionDeclaration> declarations;
    /* Function call (from, to). */
    Vector<std::pair<TokenAtom, TokenAtom>> edges;
  } graph;

  TokenAtom current_fn = 0;

  /* State of the parser. Some section have DCE turned off because of unsupported syntax. */
  bool enabled_ = true;

  int stack_depth = 0;

  const TokenAtom return_atom;
  const TokenAtom thread_atom;
  const TokenAtom device_atom;

  /* Bit vector allowing to filter atoms that designate builtin functions or constructors that we
   * should avoid creating graph for. */
  bits::BitVector<> builtin_atoms;

 public:
  PruningStream(Token invalid_tok,
                TokenAtom return_atom,
                TokenAtom thread_atom,
                TokenAtom device_atom,
                Span<TokenAtom> builtin_atoms)
      : token_history(invalid_tok),
        return_atom(return_atom),
        thread_atom(thread_atom),
        device_atom(device_atom),
        /* Reserve enough for all atoms, even generated ones during macro expansions. */
        builtin_atoms(0x10000)
  {
    for (auto atom : builtin_atoms) {
      /* Atom values can only be separated by at least 2 values. */
      this->builtin_atoms[atom / 2].set(true);
    }
  }

  void set_enabled_parsing(bool value)
  {
    enabled_ = value;
  }

  /**
   * Checks if str is a contiguous continuation of the last inserted string ref.
   * If yes, merge it; else append str to the stream.
   */
  PruningStream &operator<<(StringRef str)
  {
    if (str.is_empty()) {
      return *this;
    }

    if (!stream.is_empty()) {
      StringRef &last = stream.last();
      /* Check if the memory addresses are contiguous */
      if (last.data() + last.size() == str.data()) {
        last = StringRef(last.data(), last.size() + str.size());
      }
      else {
        stream.append(str);
      }
    }
    else {
      stream.append(str);
    }

    total_length += str.size();
    return *this;
  }

  BLI_NOINLINE void parse_token(const Token tok)
  {
    if (!enabled_) {
      return;
    }
    parse_token_impl(set_start_char(tok));
  }

  BLI_NOINLINE void parse_token_range(const Token start, const Token end)
  {
    if (!enabled_) {
      return;
    }
    const char *start_char = start.str_with_whitespace().data();

    Token tok = start;

    /* Process first 2 token to avoid branching in the loop. */
    parse_token_impl(set_start_char(tok));
    if (tok == end) {
      return;
    }
    tok = tok.next();
    parse_token_impl(set_start_char(tok, start_char));
    if (tok == end) {
      return;
    }
    tok = tok.next();

    blender::IndexRange range(int(end) - int(tok) + 1);
    Span<TokenType> types(&tok.type(), range.size());

    for (int i : range) {
      switch (types[i]) {
        case TokenType::ParOpen:
          /* Fill the needed history JIT. */
          token_history.push(set_start_char(tok.next(i).prev(2), start_char));
          token_history.push(set_start_char(tok.next(i).prev(1), start_char));
          process_function();
          break;
        case TokenType::BracketOpen:
          stack_depth += (current_fn != 0);
          break;
        case TokenType::BracketClose:
          stack_depth -= (current_fn != 0);
          ATTR_FALLTHROUGH;
        case TokenType::SemiColon:
          /* Finding a semicolon in global scope after a function signature means that this is
           * a forward declaration. Step out of function in this case. */
          register_function_end(set_start_char(tok.next(i), start_char));
          break;
        default:
          break;
      }
    }

    /* Register last two tokens. */
    token_history.push(set_start_char(end.prev(), start_char));
    token_history.push(set_start_char(end, start_char));
  }

  std::string str() const
  {
    if (total_length == 0) {
      return {};
    }

    std::string result;
    result.reserve(total_length);

    size_t global_offset = 0;
    const auto *rem_it = removals.begin();

    for (const auto &segment : stream) {
      size_t seg_idx = 0;
      /* Process the current segment until we reach its end */
      while (seg_idx < segment.size()) {
        size_t current_global_pos = global_offset + seg_idx;
        /* If no active removals remain, copy the rest of the segment. */
        if (rem_it == removals.end()) {
          result.append(segment, seg_idx, std::string::npos);
          break;
        }

        auto [r_start, r_end] = *rem_it;
        /* Safety check: ensure we haven't somehow passed the removal (if unsorted) */
        BLI_assert(current_global_pos < r_end);

        if (current_global_pos < r_start) {
          /* We are before the removal starts.
           * Copy until the segment ends OR the removal starts */
          size_t len = std::min(size_t(segment.size()) - seg_idx, r_start - current_global_pos);
          result.append(segment, seg_idx, len);
          seg_idx += len;
          continue;
        }

        /* We are inside the removal range.
         * Process until the segment ends OR the removal ends */
        size_t len = std::min(size_t(segment.size()) - seg_idx, r_end - current_global_pos);
        /* Replace the removed chunk by newlines. */
        std::string_view chunk(segment.data() + seg_idx, len);
        size_t newlines = std::count(chunk.begin(), chunk.end(), '\n');
        result.append(newlines, '\n');
        seg_idx += len;
        /* If we reached the end of this removal range, move to the next one */
        if (global_offset + seg_idx >= r_end) {
          ++rem_it;
        }
      }
      /* Update global offset before moving to next segment */
      global_offset += segment.size();
    }

    return result;
  }

  void optimize(const Span<TokenAtom> entry_points)
  {
    prune_unused_functions(entry_points);
  }

 private:
  Token set_start_char(Token tok, const char *start_offset = nullptr)
  {
    int offset = start_offset ? tok.str_with_whitespace().data() - start_offset : 0;
    tok.flag = int32_t(this->total_length + offset);
    return tok;
  }

  BLI_INLINE_METHOD void parse_token_impl(const Token tok)
  {
    switch (tok.type()) {
      case TokenType::ParOpen:
        process_function();
        break;
      case TokenType::BracketOpen:
        stack_depth += (current_fn != 0);
        break;
      case TokenType::BracketClose:
        stack_depth -= (current_fn != 0);
        ATTR_FALLTHROUGH;
      case TokenType::SemiColon:
        /* Finding a semicolon in global scope after a function signature means that this is
         * a forward declaration. Step out of function in this case. */
        register_function_end(tok);
        break;
      default:
        break;
    }
    token_history.push(tok);
  }

  void process_function()
  {
    Token name_tok = token_history[0];
    if (name_tok.type() != TokenType::Word) {
      return;
    }

    Token type_tok = token_history[1];

    /* Filter MSL & GLSL specific identifiers that could have confused the parser. */
    if (type_tok.atom() == thread_atom || type_tok.atom() == device_atom) {
      return;
    }
    if (builtin_atoms[name_tok.atom() / 2]) {
      return;
    }

    if (type_tok.type() == TokenType::Word && type_tok.atom() != return_atom) {
      register_function_declaration(type_tok, name_tok);
    }
    else if (current_fn != 0) {
      register_function_call(name_tok);
    }
  }

  BLI_INLINE_METHOD void register_function_end(Token tok)
  {
    if (stack_depth == 0 && current_fn != 0) {
      graph.declarations.last().end = tok;
      current_fn = 0;
    }
  }

  /* Register function declaration at this token position.
   * Associate ID with the token string if first encountering the symbol.
   * Does not differentiate overloads. */
  void register_function_declaration(Token type_tok, Token name_tok)
  {
    BLI_assert_msg(graph.declarations.is_empty() ||
                       graph.declarations.last().name.flag != graph.declarations.last().end.flag,
                   "Missing call to register_function_end");

    graph.declarations.append(FunctionDeclaration{type_tok, name_tok, name_tok});
    current_fn = name_tok.atom();
  }

  /* Register a function call made inside the body of a function by creating an edge inside the
   * graph. */
  void register_function_call(Token name_tok)
  {
    graph.edges.append_as(current_fn, name_tok.atom());
  }

  Map<TokenAtom, Vector<TokenAtom>> build_adjacency()
  {
    Map<TokenAtom, Vector<TokenAtom>> adj;
    adj.reserve(graph.declarations.size());
    for (const auto &[from, to] : graph.edges) {
      adj.lookup_or_add_default(from).append(to);
    }
    return adj;
  }

  Set<TokenAtom> compute_used_functions(Span<TokenAtom> &roots)
  {
    Set<TokenAtom> used;
    used.reserve(graph.declarations.size());

    auto adj = build_adjacency();

    std::vector<TokenAtom> stack;
    stack.reserve(64);

    for (TokenAtom root : roots) {
      if (used.add(root)) {
        stack.push_back(root);
      }

      while (!stack.empty()) {
        TokenAtom f = stack.back();
        stack.pop_back();

        const auto *calls = adj.lookup_ptr(f);
        if (calls == nullptr) {
          continue;
        }

        for (TokenAtom callee : *calls) {
          if (used.add(callee)) {
            stack.push_back(callee);
          }
        }
      }
    }

    return used;
  }

  void prune_unused_functions(Span<TokenAtom> entry_points)
  {
    Set<TokenAtom> used = compute_used_functions(entry_points);

    for (auto [type_tok, name_tok, end_tok] : graph.declarations) {
      if (used.contains(name_tok.atom())) {
        continue;
      }

      if (name_tok.flag == end_tok.flag) {
        removals.clear();
        BLI_assert("Bug in parser, function name not detected");
        break;
      }

      remove_range(type_tok.flag, end_tok.flag + 1);
    }
  }

  /* Remove a range of character from the final string but keeping spaces. */
  void remove_range(int start_char, int end_char)
  {
    removals.append_as(start_char, end_char);
  }
};

/** \} */

}  // namespace blender::gpu
