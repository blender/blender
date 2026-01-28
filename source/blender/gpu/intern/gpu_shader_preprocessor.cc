/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"
#include "BLI_struct_equality_utils.hh"

#include "shader_tool/expression.hh"
#include "shader_tool/intermediate.hh"

#include "gpu_shader_dead_code_elimination.hh"
#include "gpu_shader_private.hh"

#include "shader_tool/lexit/lexit.hh"
#include "shader_tool/lexit/tables.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Utilities.
 * \{ */

namespace shader::parser {

struct TokenRange {
  Token start, end;
};

static StringRef str(const Token t)
{
  /* Note: Whitespaces where not merged (because of TokenizePreprocessor), so using
   * str_view_with_whitespace will be faster.  */
  return t.str_view_with_whitespace();
}

static StringRef str(const TokenRange &range)
{
  int start = range.start.str_index_start();
  int end = range.end.str_index_last();
  return StringRef(range.start.data->lex.str.data() + start, end - start + 1);
}

static Token skip_space(Token tok)
{
  while (tok == Space) {
    tok = tok.next();
  }
  return tok;
}

static Token skip_space_backward(Token tok)
{
  while (tok == Space) {
    tok = tok.prev();
  }
  return tok;
}

}  // namespace shader::parser

/** \} */

using namespace shader::parser;

/* -------------------------------------------------------------------- */
/** \name Parser / Lexer classes.
 * \{ */

/**
 * Lexer variant for very fast tokenization for the preprocessor.
 * Consider numbers as words (to avoid splitting and then merging later on).
 * Does not merge newlines and spaces together.
 * Convert all identifier strings (words) into unique identifiers (Atom) for fast comparison.
 */
struct AtomicLexer : LexerBase {
  /* Unique identifier to a word token. */
  using Atom = uint16_t;
  /* String hash used to lookup atoms. uint64_t is slower. */
  using Hash = uint32_t;

#ifndef NDEBUG
  /** We do not support hash collision yet (for speed). */
  Map<StringRef, Hash> check_map;
#endif
  /** Atom per token. NOTE: Values are undefined for non word token. */
  Vector<Atom> token_atoms;

  /* Line index to token range. */
  blender::OffsetIndices<int> line_offsets;
  /* Preprocessor directive to line index. */
  Vector<int> directive_lines;

  BLI_NOINLINE void tokenize()
  {
    lexit::TokenBuffer tok_buf(str.data(), str.size(), token_types.data(), token_offsets.data());
    tok_buf.tokenize(lexit::char_class_table);

    /* Resize to the actual usage. */
    token_types.shrink(tok_buf.size());
    token_ends.shrink(tok_buf.size());
    token_offsets.offsets.shrink(tok_buf.size() + 1);

    update_string_view();
  }

  BLI_NOINLINE void merge_tokens()
  {
    lexit::TokenBuffer tok_buf(
        str.data(), str.size(), token_types.data(), token_offsets.data(), token_types.size());

    tok_buf.merge_complex_literals();

    /* Resize to the actual usage. */
    token_types.shrink(tok_buf.size());
    token_ends.shrink(tok_buf.size());
    token_offsets.offsets.shrink(tok_buf.size() + 1);

    update_string_view();
  }

  void lexical_analysis(std::string_view input)
  {
    str = input;
    ensure_memory();

    tokenize();
    atomize_words();
    build_line_structure();
  }

  BLI_INLINE_METHOD Atom hash(StringRef tok_str)
  {
    union {
      uint64_t u64;
      uint32_t u32[2];
    };
    u64 = 0;

    switch (tok_str.size()) {
      case 1:
        /* Reserve [0-127] range for single char token. */
        return tok_str[0];
      case 2:
        /* Reserve [128-16511] range for double char token. tok_str[1] cannot be 0. */
        return tok_str[0] + tok_str[1] * uint16_t(128);
      case 3:
      case 4:
        std::memcpy(&u64, tok_str.data(), tok_str.size());
        return atom_u32_map_.lookup_or_add_cb(u32[0], [this]() { return this->next_hash(); });
      case 5:
      case 6:
      case 7:
      case 8:
        std::memcpy(&u64, tok_str.data(), tok_str.size());
        return atom_u64_map_.lookup_or_add_cb(u64, [this]() { return this->next_hash(); });
      default:
        /* Long identifier slow path. Do full hash */
        return atomization_map_.lookup_or_add_cb(tok_str, [this]() { return this->next_hash(); });
    }
  }

 protected:
  /** Map string hashes to atom value. */
  Map<StringRef, Atom> atomization_map_;
  Map<uint64_t, Atom> atom_u64_map_;
  Map<uint32_t, Atom> atom_u32_map_;
  /* Reserve [16512-65536] range for longer token. */
  uint16_t atom_hash_counter_ = 16512;

  uint16_t next_hash()
  {
    /* Check for overflow. */
    BLI_assert(atom_hash_counter_ >= 16512);
    return atom_hash_counter_++;
  }

  BLI_NOINLINE void atomize_words()
  {
    const int tok_count = token_types.size();

    token_atoms.resize(tok_count);
    /* From checking our statistics. This heuristic should be enough for 99% of our cases. */
    atom_u32_map_.reserve(tok_count / 170);
    atom_u64_map_.reserve(tok_count / 80);
    atomization_map_.reserve(tok_count / 25);

    for (int tok_id = 0; tok_id < tok_count; tok_id++) {
      if (token_types[tok_id] != Word) {
        continue;
      }
      IndexRange range = token_offsets[tok_id];
      token_atoms[tok_id] = hash(StringRef(str.data() + range.start, range.size));
    }
  }

  /* Backing buffer for line_offsets. */
  Vector<int> line_offsets_buf_;

  BLI_NOINLINE void build_line_structure()
  {
    /* From checking our statistics. This heuristic should be enough for 100% of our cases. */
    line_offsets_buf_.reserve(token_types.size() / 7);
    directive_lines.reserve(line_offsets_buf_.size() / 2);

    line_offsets_buf_.append(0);
    int tok_id = 0;
    for (TokenType type : blender::Span<TokenType>(token_types.data(), token_types.size())) {
      if (type == NewLine) {
        line_offsets_buf_.append(tok_id + 1);
      }
      else if (type == '#') {
        int line_start = line_offsets_buf_.last();
        /* Directive can only start with a hash token (+ optional space).
         * If there is more token before the hash token it cannot be a preprocessor directive. */
        if (tok_id - line_start <= 1) {
          int line_index = line_offsets_buf_.size() - 1;
          if (directive_lines.is_empty() || directive_lines.last() != line_index) {
            directive_lines.append(line_index);
          }
        }
      }
      tok_id++;
    }
    /* Finish last line. But only do so if it contains at least one character. */
    if (line_offsets_buf_.last() != tok_id) {
      line_offsets_buf_.append(tok_id);
    }

    line_offsets = line_offsets_buf_.as_span();
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Type-safe identifier management.
 * \{ */

/* Simple integer identifier with a debug string view.
 * Allow type safety and function overload. */
template<typename Trait, typename T = int> class ID {
#ifndef NDEBUG
 public:
  std::string_view str;
#endif
 private:
  T id_;

 public:
  ID() = delete;

  explicit ID(T i) : id_(i) {}

  static ID invalid()
  {
    return ID(-1);
  }

  explicit operator T() const
  {
    return id_;
  }

  BLI_STRUCT_EQUALITY_OPERATORS_1(ID, id_)

  uint64_t hash() const
  {
    return id_;
  }
};

/* TODO(fclem): Meh find a better way. Exceptions? */
static void report_fn(int /*error_line*/,
                      int /*error_char*/,
                      std::string /*error_line_string*/,
                      const char * /*error_str*/)
{
  BLI_assert_unreachable();
}

report_callback report_fn_ptr = report_fn;

/**
 * Boiler plate class exposing lexer structure using typed IDs.
 */
struct IntermediateFormWithIDs : IntermediateForm<AtomicLexer, NullParser> {

  IntermediateFormWithIDs(StringRef str)
      : IntermediateForm<AtomicLexer, NullParser>(str, report_fn_ptr)
  {
  }

  struct TokenTrait {};
  struct LineTrait {};
  struct DirectiveTrait {};
  struct AtomTrait {};

  using TokenID = ID<TokenTrait>;
  using LineID = ID<LineTrait>;
  using DirectiveID = ID<DirectiveTrait>;
  /* Type-safe Atom. */
  using AtomID = ID<AtomTrait, AtomicLexer::Atom>;

  enum DirectiveType : char {
    Define = 0,
    Undef,
    Line,
    If,
    Ifdef,
    Ifndef,
    Elif,
    Else,
    Endif,
    /* Any other unhandled directives (warnings / errors / pragma etc...). */
    Other,
  };

  /* This relies on lexical_analysis being called inside the constructor. */
  std::array<AtomID, Other> directive_type_table = {
      /* Not using array designators since its not standard C++ :sad:. */
      /*[Define] = */ get_atom("define"),
      /*[Undef] = */ get_atom("undef"),
      /*[Line] = */ get_atom("line"),
      /*[If] = */ get_atom("if"),
      /*[Ifdef] = */ get_atom("ifdef"),
      /*[Ifndef] = */ get_atom("ifndef"),
      /*[Elif] = */ get_atom("elif"),
      /*[Else] = */ get_atom("else"),
      /*[Endif] = */ get_atom("endif"),
  };

  /* Cached 'defined' keyword identifier. */
  AtomID defined_atom = get_atom("defined");

  /**
   * Validity check.
   */
  bool is_valid(TokenID tok)
  {
    return int(tok) >= 0 && int(tok) < lex_.token_types.size();
  }
  bool is_valid(LineID line)
  {
    return int(line) >= 0 && int(line) < lex_.line_offsets.size();
  }
  bool is_valid(DirectiveID dir)
  {
    return int(dir) >= 0 && int(dir) < lex_.directive_lines.size();
  }

  /**
   * Check if item is the last of its kind.
   */
  bool is_last(DirectiveID dir)
  {
    return (lex_.directive_lines.size() - 1) == int(dir);
  }
  bool is_last(LineID line)
  {
    return (lex_.line_offsets.size() - 1) == int(line);
  }
  bool is_last(TokenID tok)
  {
    return (lex_.token_types.size() - 1) == int(tok);
  }

  /**
   * Creation. Creating an invalid token is undefined behavior.
   */
  TokenID make_token(int index)
  {
    TokenID tok(index);
#ifndef NDEBUG
    tok.str = str(tok);
#endif
    BLI_assert(is_valid(tok));
    return tok;
  }
  LineID make_line(int index)
  {
    LineID line(index);
#ifndef NDEBUG
    line.str = str(line);
#endif
    BLI_assert(is_valid(line));
    return line;
  }
  DirectiveID make_directive(int index)
  {
    DirectiveID dir(index);
#ifndef NDEBUG
    dir.str = str(dir);
#endif
    BLI_assert(is_valid(dir));
    return dir;
  }

  /**
   * Convert ID to string.
   */
  StringRef str(DirectiveID dir)
  {
    LineID start = get_start(dir);
    LineID end = get_end(dir);
    Token tok_start = parser_[int(get_start(start))];
    Token tok_end = parser_[int(get_end(end))];
    return substr_range_inclusive_view(tok_start, tok_end);
  }
  StringRef str(LineID line)
  {
    TokenID start = get_start(line);
    TokenID end = get_end(line);
    Token tok_start = parser_[int(start)];
    Token tok_end = parser_[int(end)];
    return substr_range_inclusive_view(tok_start, tok_end);
  }
  StringRef str(TokenID tok)
  {
    return parser_[int(tok)].str_view_with_whitespace();
  }
  StringRef str(TokenID start, TokenID end_inclusive)
  {
    return substr_range_inclusive_view(parser_[int(start)], parser_[int(end_inclusive)]);
  }

  /* Return valid value if tok is valid and a word token. */
  AtomID get_atom(TokenID tok)
  {
    BLI_assert(get_type(tok) == Word);
    return AtomID(lex_.token_atoms[int(tok)]);
  }
  /* Return valid value if dir is valid. */
  AtomID get_atom(DirectiveID dir)
  {
    return AtomID(lex_.token_atoms[int(get_identifier(dir))]);
  }
  /* Return valid value if hash is a known string. Is full hash lookup + hashing. */
  AtomID get_atom(StringRef str)
  {
    return AtomID(lex_.hash(str));
  }

  /**
   * Return next token. Result in undefined behavior if id is last.
   */
  LineID next(LineID line)
  {
    return make_line(int(line) + 1);
  }
  TokenID next(TokenID token)
  {
    return make_token(int(token) + 1);
  }
  DirectiveID next(DirectiveID directive)
  {
    return make_directive(int(directive) + 1);
  }

  /**
   * Return previous token. Result in undefined behavior if id is first.
   */
  LineID prev(LineID line)
  {
    return make_line(int(line) - 1);
  }
  TokenID prev(TokenID token)
  {
    return make_token(int(token) - 1);
  }
  DirectiveID prev(DirectiveID directive)
  {
    return make_directive(int(directive) - 1);
  }

  /**
   * Jump to next token. Undefined behavior if tok is the last token.
   */
  TokenID skip_space(TokenID tok)
  {
    return (get_type(tok) == Space) ? next(tok) : tok;
  }

  /**
   * Return the start element.
   */
  TokenID get_start(LineID line)
  {
    return make_token(lex_.line_offsets[int(line)].start());
  }
  LineID get_start(DirectiveID dir)
  {
    return make_line(lex_.directive_lines[int(dir)]);
  }

  /**
   * Return the end element.
   * NOTE: Return the token before \n or \n if line is empty.
   */
  TokenID get_end(LineID line)
  {
    blender::IndexRange range = lex_.line_offsets[int(line)];
    return make_token(range.size() > 1 ? range.last(1) : range.last());
  }
  LineID get_end(DirectiveID dir)
  {
    /* Could be precomputed if becoming a bottleneck. */
    LineID line = get_start(dir);
    while (get_type(get_end(line)) == Backslash) {
      line = next(line);
    }
    return line;
  }

  /* NOTE: Return the end of line character '\n'. */
  TokenID get_true_end(LineID line)
  {
    return make_token(lex_.line_offsets[int(line)].last());
  }

  /* Return the type of the next token or Invalid if this is the last token. */
  TokenType look_ahead(TokenID tok)
  {
    return is_last(tok) ? Invalid : get_type(next(tok));
  }
  /* Return the type of the previous token or Invalid if this is the first token. */
  TokenType look_behind(TokenID tok)
  {
    return int(tok) == 0 ? Invalid : get_type(prev(tok));
  }

  /**
   * Get the corresponding type enum.
   */
  TokenType get_type(TokenID tok)
  {
    return lex_.token_types[int(tok)];
  }
  DirectiveType get_type(DirectiveID dir)
  {
    AtomID id_hash = get_atom(dir);
    /* Linear search in small array. */
    for (int i : blender::IndexRange(directive_type_table.size())) {
      if (directive_type_table[i] == id_hash) {
        return DirectiveType(i);
      }
    }
    return Other;
  }

  /* Return token defining the directive type (e.g. define, undef, if ...). */
  TokenID get_identifier(DirectiveID dir)
  {
    LineID line = get_start(dir);
    TokenID hash_tok = skip_space(get_start(line));
    BLI_assert(get_type(hash_tok) == Hash);
    TokenID dir_tok = skip_space(next(hash_tok));
    BLI_assert(get_type(dir_tok) == Word);
    return dir_tok;
  }

  /* Returns the type of conditional. */
  DirectiveType increment_to_next_conditional(DirectiveID &dir)
  {
    dir = next(dir);
    while (is_valid(dir)) {
      DirectiveType type = get_type(dir);
      if (ELEM(type, If, Ifdef, Ifndef, Else, Elif, Endif)) {
        return type;
      }
      dir = next(dir);
    }
    /* Missing matching #endif. */
    // TODO exception?
    BLI_assert_unreachable();
    return Other;
  }

  /* Returns the hash token. */
  DirectiveID find_next_matching_conditional(DirectiveID dir)
  {
    int stack = 1;
    while (is_valid(dir)) {
      DirectiveType type = increment_to_next_conditional(dir);
      if (ELEM(type, If, Ifdef, Ifndef)) {
        stack++;
      }
      else if (ELEM(type, Endif)) {
        stack--;
      }

      if (stack == 0) {
        return dir; /* Endif. */
      }
      if (stack == 1 && ELEM(type, Else, Elif)) {
        return dir;
      }
    }
    BLI_assert_unreachable();
    return DirectiveID::invalid();
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preprocessor.
 * \{ */

/* Fast C (incomplete) preprocessor implementation.  */
struct Preprocessor : IntermediateFormWithIDs {
 private:
  using ExpansionParser = IntermediateForm<SimpleLexer, DummyParser>;
  using TokenRange = shader::parser::TokenRange;

  /* Cache the expression lexer to avoid memory allocations. */
  ExpressionLexer expression_lexer;
  ExpressionParser expression_parser = ExpressionParser(expression_lexer);

  struct ParserStack {
    int allocated = 0;
    int used = 0;
    std::deque<ExpansionParser> parser_pool;

    ExpansionParser &alloc()
    {
      if (used == allocated) {
        parser_pool.emplace_back("", report_fn_ptr);
        allocated++;
        used++;
        return parser_pool.back();
      }
      return parser_pool[used++];
    }

    void release(ExpansionParser & /*parser*/)
    {
      used--;
    }
  };

  /* When evaluating a condition directive inside this stack, disregard the directive and jump to
   * the matching #endif. */
  Vector<DirectiveID, 8> jump_stack;
  /* Own stack to avoid memory allocation during recursive expansion parsing. */
  ParserStack recursive_parser_stack;
  /* Set of visited macros during recursion (blue painting stack). Using a vector for speed. */
  Vector<DirectiveID> visited_macros;
  /* Maps containing currently active macros. Map their keyword to their definition. */
  Map<AtomID, DirectiveID> defines;

  /**
   * State Tracking.
   */

  /* Next preprocessor directive to evaluate. Might be overwritten by conditional evaluation. */
  DirectiveID next_directive = DirectiveID::invalid();
  /* End of the last evaluated directive. Might be overwritten by conditional evaluation.
   * Used to resume token expansion after this line. */
  LineID last_directive_end = LineID::invalid();

 public:
  Preprocessor(const std::string_view str) : IntermediateFormWithIDs(str)
  {
    /* From our stats. Should be enough for 100% of our cases. */
    defines.reserve(1000);
  }

  void preprocess()
  {
    if (lex_.directive_lines.is_empty()) {
      return;
    }

    last_directive_end = make_line(0);
    next_directive = make_directive(0);
    /* Expand until the first directive. */
    if (make_line(0) != get_start(next_directive)) {
      expand_macros_in_range(make_line(0), prev(get_start(next_directive)));
    }

    while (!is_last(next_directive)) {
      DirectiveID id = next_directive;
      /* The next directive might be overwritten by evaluate_directive. Increment before call. */
      next_directive = next(id);
      evaluate_directive(id);

      expand_macros_in_range(next(last_directive_end), prev(get_start(next_directive)));
    }
    /* Evaluate last directive without calling next and creating an invalid ID. */
    evaluate_directive(next_directive);

    if (!is_last(last_directive_end)) {
      LineID last_line = make_line(lex_.line_offsets.size() - 1);
      expand_macros_in_range(next(last_directive_end), last_line);
    }
  }

 private:
  void evaluate_directive(DirectiveID dir)
  {
    DirectiveType dir_type = get_type(dir);

    /* Note: gets overwritten by conditional processing. */
    last_directive_end = get_end(dir);

    bool erase_directive = true;
    switch (dir_type) {
      case Define:
        define_macro(dir);
        break;
      case Undef:
        undefine_macro(dir);
        break;
      case If:
      case Ifdef:
      case Ifndef:
      case Elif:
      case Else:
        process_conditional(dir, dir_type);
        erase_directive = false; /* Erases itself. */
        break;
      case Line:
        break;
      case Endif:
        break;
      case Other:
        erase_directive = false;
        break;
    }

    if (erase_directive == true) {
      erase_lines(get_start(dir), get_end(dir));
    }
  }

  /**
   * Macro Management.
   */

  void define_macro(DirectiveID dir)
  {
    TokenID macro_name = skip_space(next(get_identifier(dir)));
    BLI_assert(get_type(macro_name) == Word);
    /* Store the name token of the declaration.
     * The actual parsing of the definition happens during expansion. */
    defines.add_overwrite(get_atom(macro_name), dir);
  }

  void undefine_macro(DirectiveID dir)
  {
    TokenID macro_name = skip_space(next(get_identifier(dir)));
    BLI_assert(get_type(macro_name) == Word);
    defines.remove(get_atom(macro_name));
  }

  /**
   * Condition directives.
   */

  void process_conditional(const DirectiveID dir, const DirectiveType dir_type)
  {
    /* If this is part of an already evaluated statement. */
    if (!jump_stack.is_empty() && jump_stack.last() == dir) {
      jump_stack.pop_last();
      /* Find matching endif. */
      DirectiveID endif = find_next_matching_conditional(dir);
      while (get_type(endif) != Endif) {
        endif = find_next_matching_conditional(endif);
      }
      if (is_last(endif)) {
        /* Erase everything this and the last directive. */
        LineID last_before_endif = prev(get_start(endif));
        erase_lines(get_start(dir), last_before_endif);
        next_directive = endif;
        /* Don't expand inside this section. */
        last_directive_end = last_before_endif;
      }
      else {
        /* Erase everything between this directive and the #endif (inclusive). */
        LineID endif_end = get_end(endif);
        erase_lines(get_start(dir), endif_end);
        /* Evaluate after the endif. */
        next_directive = next(endif);
        /* Don't expand inside this section. */
        last_directive_end = endif_end;
      }
      return;
    }

    const LineID dir_line_start = get_start(dir);
    const LineID dir_line_end = get_end(dir);
    const TokenID dir_tok = get_identifier(dir);
    /* Evaluate condition. */
    const TokenID cond_start = skip_space(next(dir_tok));
    const TokenID cond_end = get_end(dir_line_end);
    const bool condition_result = evaluate_condition(dir_type, cond_start, cond_end);

    /* Find matching endif or else. */
    const DirectiveID next_condition = find_next_matching_conditional(dir);

    if (condition_result) {
      /* If is followed by else statement. */
      DirectiveType next_dir_type = get_type(next_condition);
      if (ELEM(next_dir_type, Elif, Else)) {
        /* Record a jump statement at the next #else statement to jump & erase to the #endif. */
        jump_stack.append(next_condition);
      }
      /* Erase condition and continue parsing content.
       * The #endif will just be erased later. */
      erase_lines(dir_line_start, dir_line_end);
    }
    else {
      LineID last_before_next_cond = prev(get_start(next_condition));
      /* Erase everything until next condition (this directive included). */
      erase_lines(dir_line_start, last_before_next_cond);
      /* Jump to next condition. */
      next_directive = next_condition;
      /* Don't expand inside this section. */
      last_directive_end = last_before_next_cond;
    }
  }

  bool evaluate_condition(const DirectiveType dir_type, TokenID start, TokenID end)
  {
    switch (dir_type) {
      case Else:
        return true;
      case Ifdef:
        return defines.contains(get_atom(start));
      case Ifndef:
        return !defines.contains(get_atom(start));
      case If:
      case Elif:
        return evaluate_expression(start, end);
      default:
        BLI_assert_unreachable();
        return true;
    }
  }

  bool evaluate_expression(const TokenID start, const TokenID end)
  {
#ifndef NDEBUG
    /* For debugging. */
    std::string_view original_expr = substr_range_inclusive_view(parser_[int(start)],
                                                                 parser_[int(end)]);
    UNUSED_VARS(original_expr);
#endif

    /* Expand expression into integer ops string. */
    std::string expand = expand_expression(start, end);

    /* Early out simple cases. */
    if (expand == "0") {
      return false;
    }
    if (expand == "1") {
      return true;
    }

    try {
      expression_lexer.lexical_analysis(expand);
      return expression_parser.eval() != 0;
    }
    catch (const std::exception &e) {
      std::cout << "\"" << str(start, end) << "\" > \"" << expand << "\" ";
      std::cerr << "Error: " << e.what() << "\n";
      return 0;
    }
  }

  /**
   * Macro Expansion.
   */

  void expand_macros_in_range(const LineID start_line, const LineID end_line)
  {
    int start = int(get_start(start_line));
    int end = int(get_true_end(end_line));
    if (start > end) {
      return;
    }

    TokenID end_tok = make_token(end);
    for (TokenID tok = make_token(start); tok != end_tok; tok = next(tok)) {
      if (get_type(tok) == Word) {
        DirectiveID macro_id = defines.lookup_default(get_atom(tok), DirectiveID::invalid());
        if (is_valid(macro_id)) {
          Token token = parser_[int(tok)];
          auto [replacement, end] = expand_macro(token, macro_id);
          replace(token, end, replacement);
          tok = make_token(end.index);
          if (tok == end_tok) {
            break;
          }
        }
      }
    }
  }

  /* Try to match the token pointed at by cursor with a defined macro.
   * If that happen advance the cursor to the end of the macro (in case of functional macro). */
  void try_expand(MutableString &mut_str, const ParserBase &data, int &cursor)
  {
    Token tok = Token::from_position(&data, cursor);
    StringRef tok_str = shader::parser::str(tok);
    /* Early out number literals.
     * Anything below '0' is not an alphabetical character and thus cannot start a word.
     * Saves one comparison. */
    if (tok_str[0] <= '9') {
      return;
    }

    DirectiveID macro_id = defines.lookup_default(get_atom(tok_str), DirectiveID::invalid());
    if (is_valid(macro_id)) {
      auto [replacement, end] = expand_macro(tok, macro_id);
      mut_str.replace(tok, end, replacement);
      cursor = end.index;
    }
  }

  /* Parse and expand with the current set of macro identifier. */
  std::string parse_and_expand(StringRef input)
  {
    if (input.is_empty()) {
      return "";
    }

    ExpansionParser &recursive_parser = recursive_parser_stack.alloc();

    recursive_parser.str_ = input;
    recursive_parser.parse(report_fn_ptr);

    const ParserBase &data = recursive_parser.data_get();

    for (int cursor = 0; cursor < data.lex.token_types.size(); cursor++) {
      TokenType tok_type = TokenType(data.lex.token_types[cursor]);
      if (tok_type == Word) {
        try_expand(recursive_parser, data, cursor);
      }
    }

    std::string result = recursive_parser.result_get(true);

    recursive_parser_stack.release(recursive_parser);

    return result;
  }

  struct ExpandedResult {
    /* Replacement content. */
    std::string str;
    /* End of range to replace. */
    Token end_of_expansion;
  };

  /**
   * IMPORTANT: Because of recursion, expanded_tok can be from another parser.
   * The macro directive however, will always be from the main parser.
   */
  ExpandedResult expand_macro(const Token expanded_tok, const DirectiveID macro)
  {
    const TokenID define_tok = get_identifier(macro);
    BLI_assert(str(define_tok) == "define");
    const TokenID macro_name = skip_space(next(define_tok));
    BLI_assert(get_type(macro_name) == Word);
    const TokenID macro_parenthesis = next(macro_name);

    const bool is_function = (get_type(macro_parenthesis) == '(');

    Token end_of_expansion = expanded_tok;

    TokenID tok = skip_space(macro_parenthesis);

    /* Empty definition. */
    if (get_type(tok) == '\n') {
      return {"", end_of_expansion};
    }

    if (visited_macros.contains(macro)) {
      /* Recursion. Do not expand. Still replace by the original token. */
      return {str(macro_name), end_of_expansion};
    }

    /* TODO: Avoid StringRef in map. */
    Map<StringRef, TokenRange> macro_parameters;
    if (is_function) {
      /* This is a functional macro. */

      Token param = shader::parser::skip_space(expanded_tok.next());
      if (param != '(') {
        /* Macro doesn't have parameters. It should not expand. */
        return {str(macro_name), end_of_expansion};
      }

      /* Parse parameters & arguments. */
      macro_parameters.clear_and_keep_capacity();
      while (get_type(tok) != ')') {
        /* Continue to the next name. */
        tok = skip_space(next(tok));
        if (get_type(tok) == ')') {
          /* Function with no arguments. */
          param = get_end_of_parameter(param);
          if (param == Invalid) {
            /* Error: missing closing parenthesis. */
            /* Cancel expansion. */
            return {str(macro_name), expanded_tok};
          }
          if (param != ')') {
            /* Error: too many arguments provided to function-like macro invocation. */
            /* Cancel expansion. */
            return {str(macro_name), expanded_tok};
          }
          break;
        }

        Token param_start = param;
        Token param_end = get_end_of_parameter(param_start);

        StringRef argument_name = str(tok);
        if (argument_name == "...") {
          param_end = get_end_of_parameter(param_start, true);
          argument_name = "__VA_ARGS__";
        }

        /* If there is only token for parameters (it could be empty string). */
        if (param_start.next() == param_end.prev()) {
          macro_parameters.add(argument_name, {param_start.next(), param_start.next()});
        }
        else {
          macro_parameters.add(argument_name,
                               {shader::parser::skip_space(param_start.next()),
                                shader::parser::skip_space_backward(param_end.prev())});
        }

        /* Continue to the next separator. */
        tok = skip_space(next(tok));
        param = param_end;

        if (get_type(tok) == Invalid) {
          break;
        }
      }
      /* Skip closing parenthesis. */
      tok = skip_space(next(tok));
      /* Make sure to replace the whole call. */
      end_of_expansion = param;
    }

    std::string expanded;
    expanded.reserve(256);

    while (get_type(tok) != NewLine) {
      TokenType curr_type = get_type(tok);
      TokenType next_type = look_ahead(tok);
      /* Skip the token pasting operator. */
      if (curr_type == '#' && next_type == '#') {
        /* Token concatenate. */
        tok = next(next(tok));
        continue;
      }
      if (curr_type == '\\' && next_type == '\n') {
        /* Preprocessor new line. Skip and continue. */
        tok = next(next(tok));
        /* Still insert a space to avoid merging tokens. */
        expanded += ' ';
        continue;
      }

      /* Can't theoretically happen.
       * That would mean a macro is defined and expanded on the last line. */
      BLI_assert(curr_type != Invalid);
      BLI_assert_msg(curr_type != '#', "Stringify operator '#' is not supported");

      TokenType next_type2 = (next_type != Invalid) ? look_ahead(next(tok)) : Invalid;
      TokenType next_type3 = (next_type2 != Invalid) ? look_ahead(next(next(tok))) : Invalid;
      TokenType prev_type = look_behind(tok);
      TokenType prev_type2 = (prev_type != Invalid) ? look_behind(prev(tok)) : Invalid;
      TokenType prev_type3 = (prev_type2 != Invalid) ? look_behind(prev(prev(tok))) : Invalid;

      /* Support spaces around token pasting operator */
      bool next_is_token_pasting = (next_type == ' ') ? (next_type2 == '#' && next_type3 == '#') :
                                                        (next_type == '#' && next_type2 == '#');
      bool prev_is_token_pasting = (prev_type == ' ') ? (prev_type2 == '#' && prev_type3 == '#') :
                                                        (prev_type == '#' && prev_type2 == '#');

      if (curr_type == ' ' && (next_is_token_pasting || prev_is_token_pasting)) {
        /* Do not paste spaces around token pasting operator. */
      }
      else if (curr_type == ' ') {
        /* Replace multiple spaces by only one. Shrinks final codebase. */
        expanded += ' ';
      }
      else if (curr_type == Word) {
        bool replaced = false;

        if (is_function) {
          /* Lookup macro arguments. */
          TokenRange *macro_value_ptr = macro_parameters.lookup_ptr(str(tok));
          if (macro_value_ptr) {
            TokenRange &macro_value = *macro_value_ptr;

            if (!next_is_token_pasting && !prev_is_token_pasting) {
              /* Expand argument. Can expand to the same macro (finite recursion). */
              expanded += parse_and_expand(shader::parser::str(macro_value));
            }
            else {
              expanded += shader::parser::str(macro_value);
            }
            replaced = true;
          }
        }

        if (!replaced) {
          /* Fallback to no expansion. */
          expanded += str(tok);
        }
      }
      else {
        expanded += str(tok);
      }

      tok = next(tok);
    }

    /* Add to the set to avoid infinite recursion. */
    visited_macros.append(macro);

    expanded = parse_and_expand(expanded);

    visited_macros.pop_last();

    return {expanded, end_of_expansion};
  }

  /* Expand token range for condition evaluation (e.g. '#if'). */
  std::string expand_expression(const TokenID start, const TokenID end)
  {
    std::string expand;
    expand.reserve(128);

    TokenID tok = start;
    while (true) {
      BLI_assert(is_valid(tok));
      AtomID tok_atom = get_type(tok) == Word ? get_atom(tok) : AtomID::invalid();

      DirectiveID macro = defines.lookup_default(tok_atom, DirectiveID::invalid());

      if (tok_atom == AtomID::invalid()) {
        /* Non word. */
        expand += str(tok);
      }
      else if (is_valid(macro)) {
        auto [replacement, macro_end] = expand_macro(parser_[int(tok)], macro);
        expand += replacement;
        tok = make_token(macro_end.index);
      }
      else if (tok_atom == defined_atom) {
        /* Parenthesis or space */
        tok = skip_space(next(tok));
        const bool is_function = (get_type(tok) == '(');
        /* Token to search. */
        if (is_function) {
          tok = skip_space(next(tok));
        }
        else {
          BLI_assert(get_type(tok) == Word);
        }
        expand += (defines.contains(get_atom(tok)) ? "1" : "0");
        if (is_function) {
          /* End parenthesis. */
          tok = skip_space(next(tok));
        }
      }
      else {
        /* Substitution failure. */
        expand += str(tok);
      }
      if (tok == end) {
        break;
      }
      tok = skip_directive_newlines(next(tok));
    }

    return expand;
  }

  /**
   * Utilities.
   */

  void erase_lines(LineID start, LineID end)
  {
    Token tok_end = parser_[int(get_end(end))];
    Token tok_start = parser_[int(get_start(start))];
    replace(tok_start, tok_end, new_lines(start, end));
  }

  /* Return a string with the amount of newline character between line_start and line_end. */
  std::string new_lines(LineID line_start, LineID line_end)
  {
    return std::string(int(line_end) - int(line_start), '\n');
  }

  TokenID skip_directive_newlines(TokenID tok)
  {
    /* TODO make it safe */
    while (get_type(tok) == '\\' && get_type(next(tok)) == '\n') {
      tok = next(next(tok));
    }
    return tok;
  }

  /**
   * Return next `,` or `)` skipping occurrences contained in parenthesis.
   * Return invalid token on failure.
   */
  static Token get_end_of_parameter(Token tok, bool skip_to_end = false)
  {
    /* Avoid matching comma inside parameter function calls. */
    int stack = 1;
    tok = tok.next();
    while (tok.is_valid()) {
      if (tok == '(') {
        stack++;
      }
      else if (tok == ')') {
        stack--;
      }
      if (stack == 0) {
        return tok;
      }
      if (stack == 1 && tok == ',' && !skip_to_end) {
        return tok;
      }
      tok = tok.next();
    }
    return tok;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interface.
 * \{ */

std::string Shader::run_preprocessor(StringRef source)
{
  BLI_assert_msg(source.find("//") == std::string::npos && source.find("/*") == std::string::npos,
                 "Input source to the preprocessor should have no comments.");

  if (G.debug & G_DEBUG_GPU_SHADER_NO_PREPROCESSOR) {
    return source;
  }

  Preprocessor processor(source);
  processor.preprocess();

  if (G.debug & G_DEBUG_GPU_SHADER_NO_DCE) {
    return processor.result_get(true);
  }

  DeadCodeEliminator dce(processor.result_get(true));
  dce.optimize();
  return dce.result_get(true);
}

/** \} */

}  // namespace blender::gpu
