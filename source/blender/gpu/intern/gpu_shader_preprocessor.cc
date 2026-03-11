/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"
#include "BLI_bit_vector.hh"

#include "shader_tool/expression.hh"

#include "gpu_shader_dead_code_elimination.hh"
#include "gpu_shader_private.hh"

#include "shader_tool/lexit/identifier.hh"
#include "shader_tool/lexit/lexit.hh"
#include "shader_tool/lexit/tables.hh"

namespace blender::gpu {

using LexerBase = shader::parser::LexerBase;
using NullParser = shader::parser::NullParser;
using namespace lexit;

template<typename IToken> struct TokenRange {
  IToken begin, end;
};

/* -------------------------------------------------------------------- */
/** \name Parser / Lexer classes.
 * \{ */

class Line;
class Directive;

struct TokenPastingBuffer : lexit::TokenBuffer {
 private:
  /** WORKAROUND: We need the string to be immutable since the atomization_map_ contains StringRef.
   * But we also need the pasted token to be in a continuous string buffer for the TokenBuffer.
   * So we keep the old string around when we need to grow the buffer. */
  Vector<std::unique_ptr<std::string>> pasted_tokens_str;

 public:
  TokenPastingBuffer()
  {
    pasted_tokens_str.append(std::make_unique<std::string>());
    pasted_tokens_str.last()->resize(1024 * 2);
    this->reserve(512);
    this->offsets_[0] = 0;
    this->offsets_end_[0] = 0;
  }

  /**
   * tok_str is the final token string with the optional added space at the end.
   */
  BLI_INLINE_METHOD TokenMut paste_token(const std::string &tok_str,
                                         const TokenType type,
                                         const bool followed_by_space)
  {
    std::string *str_ptr = pasted_tokens_str.last().get();
    int occupied = str_.length();

    int token_size = tok_str.size();

    while (occupied + token_size > str_ptr->size()) {
      /* Create new larger buffer and copy current content.
       * Do not free old string to not invalidate the StringRefs inside the atomization map.
       * Cost is relatively small. */
      pasted_tokens_str.append(std::make_unique<std::string>());
      std::string *new_str_ptr = pasted_tokens_str.last().get();
      new_str_ptr->resize(str_ptr->size() * 2);
      std::memcpy(new_str_ptr->data(), str_ptr->data(), str_ptr->size());
      /* Continue logic with new string buffer. */
      str_ptr = new_str_ptr;
    }

    /* Copy token string. */
    std::memcpy(str_ptr->data() + occupied, tok_str.data(), token_size);
    /* Update buffer string_ref. */
    str_ = {str_ptr->data(), size_t(occupied + token_size)};
    /* Add token to buffer. */
    append(type, 0, token_size - followed_by_space, token_size);

    return TokenMut(this, size_ - 1);
  }
};

/**
 * Lexer variant for very fast tokenization for the preprocessor.
 * Does not merge newlines and spaces together.
 * Convert all identifier strings (words) into unique identifiers (TokenAtom) for fast comparison.
 */
struct AtomicLexer : lexit::TokenBuffer {
  /* Line index to token range. */
  blender::OffsetIndices<int> line_offsets;

  /* Preprocessor directive to line index. */
  Vector<int> directive_lines;

 public:
  AtomicLexer(StringRef str)
  {
    lexical_analysis(str);
  }

  Line line(int index) const;
  Directive directive(int index) const;

  BLI_INLINE_METHOD TokenAtom hash(StringRef tok_str)
  {
    return identifier_map.lookup_or_add(tok_str);
  }

  TokenPastingBuffer pasting_buf;

  BLI_INLINE_METHOD Token paste_token(const std::string &tok_str,
                                      const TokenType type,
                                      const bool followed_by_space)
  {
    TokenMut tok = pasting_buf.paste_token(tok_str, type, followed_by_space);
    /** IMPORTANT: The hash function need to store a StringRef of the string. We have to make sure
     * to feed it the final stored string to avoid referencing freed memory. */
    tok.atom() = hash(tok.str());
    return tok;
  }

  TokenAtom max_atom_value() const
  {
    return identifier_map.max_atom_value();
  }

 protected:
  void lexical_analysis(std::string_view input)
  {
    process(input, lexit::char_class_table);
    lex_pass();
  }

  /* Backing buffer for line_offsets. */
  Vector<int> line_offsets_buf_;

  lexit::IdentifierMap identifier_map;

  /**
   * All-in-one lexing pass.
   * - Keywords identification.
   * - Identifiers atomization.
   * - Line structure building.
   */
  BLI_NOINLINE void lex_pass()
  {
    identifier_map.reserve(size());

    std::memset(atoms_.get(), 0, size() * sizeof(TokenAtom));

    /* Reserved 0 atom (invalid). */
    hash(" ");

    KeywordTable keywords({
        identifier_map.make_keyword("line", TokenType::Line),
        identifier_map.make_keyword("define", Define),
        identifier_map.make_keyword("if", If),
        identifier_map.make_keyword("ifdef", Ifdef),
        identifier_map.make_keyword("ifndef", Ifndef),
        identifier_map.make_keyword("else", Else),
        identifier_map.make_keyword("elif", Elif),
        identifier_map.make_keyword("endif", Endif),
        identifier_map.make_keyword("pragma", Pragma),
        identifier_map.make_keyword("undef", Undef),
    });

    /* Not keywords, but warm the identifier map with common identifiers. */
    identifier_map.lookup_or_add("r");
    identifier_map.lookup_or_add("a");
    identifier_map.lookup_or_add("void");
    identifier_map.lookup_or_add("return");
    identifier_map.lookup_or_add("x");
    identifier_map.lookup_or_add("y");
    identifier_map.lookup_or_add("texture");
    identifier_map.lookup_or_add("coord");
    identifier_map.lookup_or_add("struct");
    identifier_map.lookup_or_add("int");
    identifier_map.lookup_or_add("uint");
    identifier_map.lookup_or_add("float");

    atomize_words(identifier_map, keywords);

    /* Create line structure. */
    line_offsets_buf_.resize(size_);
    line_offsets_buf_[0] = 0;
    int line_id = 1;
    for (int i = 0; i < size_; ++i) {
      line_offsets_buf_[line_id] = i + 1;
      line_id += types_[i] == NewLine;
    }
    line_offsets_buf_.resize(line_id);
    /* Finish last line. But only do so if it contains at least one character. */
    if (line_offsets_buf_.last() != size_) {
      line_offsets_buf_.append(size_);
    }

    /* Create directive structure. */
    directive_lines.reserve(line_offsets_buf_.size());
    for (int i = 0; i < line_offsets_buf_.size(); ++i) {
      if (types_[line_offsets_buf_[i]] == '#') {
        directive_lines.append(i);
      }
    }

    line_offsets = line_offsets_buf_.as_span();
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Type-safe identifier management.
 * \{ */

/* Has quite a performance hit in debug. Enable if needed. */
// #define DEBUG_ID_STRING

class Line {
  friend Directive;
  friend AtomicLexer;

 private:
#ifdef DEBUG_ID_STRING
  std::string_view debug_str_;
#endif
  const AtomicLexer *lex_;
  int index_;

  Line() : lex_(nullptr), index_(-1) {};

  Line(const AtomicLexer *lex, int index) : lex_(lex), index_(index)
  {
    BLI_assert(is_valid());
#ifdef DEBUG_ID_STRING
    debug_str_ = str();
#endif
  };

 public:
  static Line invalid()
  {
    return Line();
  }

  explicit operator int() const
  {
    return index_;
  }

  Token first() const
  {
    return (*lex_)[lex_->line_offsets[index_].first()];
  }
  /* Return the end element. Return the token before \n or \n if line is empty. */
  Token last() const
  {
    blender::IndexRange range = lex_->line_offsets[index_];
    return (*lex_)[range.size() > 1 ? range.last(1) : range.last()];
  }
  /* NOTE: Return the end of line character '\n'. */
  Token end() const
  {
    return (*lex_)[lex_->line_offsets[index_].last()];
  }

  StringRef str() const
  {
    IndexRange range = lex_->line_offsets[index_];
    return lex_->substr((*lex_)[range.first()], (*lex_)[range.last()]);
  }

  bool is_last() const
  {
    return (lex_->line_offsets.size() - 1) == index_;
  }

  bool is_valid() const
  {
    return index_ >= 0 && index_ < lex_->line_offsets.size();
  }

  /* Return next token. Result in undefined behavior if id is last. */
  Line next() const
  {
    return Line(lex_, index_ + 1);
  }
  /* Return previous token. Result in undefined behavior if id is first. */
  Line prev() const
  {
    return Line(lex_, index_ - 1);
  }

  friend bool operator==(const Line &a, const Line &b)
  {
    return a.index_ == b.index_;
  }

  uint64_t hash() const
  {
    return index_;
  }
};

enum class DirectiveType : char {
  Define = TokenType::Define,
  Undef = TokenType::Undef,
  Line = TokenType::Line,
  If = TokenType::If,
  Ifdef = TokenType::Ifdef,
  Ifndef = TokenType::Ifndef,
  Elif = TokenType::Elif,
  Else = TokenType::Else,
  Endif = TokenType::Endif,
  Pragma = TokenType::Pragma,
  /* Any other unhandled directives (warnings / errors / pragma etc...). */
  Other = TokenType::Word,
};

/* Simple integer identifier with a debug string view.
 * Allow type safety and function overload. */
class Directive {
  friend AtomicLexer;

 private:
#ifdef DEBUG_ID_STRING
  std::string_view debug_str_;
#endif
  const AtomicLexer *lex_;
  int index_;

  Directive() : lex_(nullptr), index_(-1) {};
  Directive(const AtomicLexer *lex, int index) : lex_(lex), index_(index)
  {
    BLI_assert(is_valid());
#ifdef DEBUG_ID_STRING
    debug_str_ = str();
#endif
  };

 public:
  static Directive invalid()
  {
    return Directive();
  }

  explicit operator int() const
  {
    return index_;
  }

  Line first() const
  {
    return lex_->line(lex_->directive_lines[index_]);
  }

  Line last() const
  {
    /* Could be precomputed if becoming a bottleneck. */
    Line line = first();
    while (line.last() == Backslash) {
      line = line.next();
    }
    return line;
  }

  DirectiveType type() const
  {
    return DirectiveType(identifier().type());
  }

  /* Return token defining the directive type (e.g. define, undef, if ...). */
  Token identifier() const
  {
    Line line = first();
    Token hash_tok = line.first();
    BLI_assert(hash_tok == Hash);
    Token dir_tok = hash_tok.next();
    return dir_tok;
  }

  bool is_valid() const
  {
    return index_ >= 0 && index_ < lex_->directive_lines.size();
  }

  bool is_last() const
  {
    return (lex_->directive_lines.size() - 1) == index_;
  }

  Directive next() const
  {
    return Directive(lex_, index_ + 1);
  }
  Directive prev() const
  {
    return Directive(lex_, index_ - 1);
  }

  StringRef str() const
  {
    Line start = first();
    Line end = last();
    Token tok_start = start.first();
    Token tok_end = end.last();
    return lex_->substr(tok_start, tok_end);
  }

  StringRef str_with_whitespace() const
  {
    Line start = first();
    Line end = last();
    Token tok_start = start.first();
    Token tok_end = end.end();
    return lex_->substr(tok_start, tok_end);
  }

  friend bool operator==(const Directive &a, const Directive &b)
  {
    return a.index_ == b.index_;
  }

  uint64_t hash() const
  {
    return index_;
  }
};

/* Returns the type of conditional. */
static DirectiveType increment_to_next_conditional(Directive &dir)
{
  dir = dir.next();
  while (dir.is_valid()) {
    DirectiveType type = dir.type();
    if (ELEM(type,
             DirectiveType::If,
             DirectiveType::Ifdef,
             DirectiveType::Ifndef,
             DirectiveType::Else,
             DirectiveType::Elif,
             DirectiveType::Endif))
    {
      return type;
    }
    dir = dir.next();
  }
  /* Missing matching #endif. */
  // TODO exception?
  BLI_assert_unreachable();
  return DirectiveType::Other;
}

static Directive find_next_matching_conditional(Directive dir)
{
  int stack = 1;
  while (dir.is_valid()) {
    DirectiveType type = increment_to_next_conditional(dir);
    if (ELEM(type, DirectiveType::If, DirectiveType::Ifdef, DirectiveType::Ifndef)) {
      stack++;
    }
    else if (ELEM(type, DirectiveType::Endif)) {
      stack--;
    }

    if (stack == 0) {
      return dir; /* Endif. */
    }
    if (stack == 1 && ELEM(type, DirectiveType::Else, DirectiveType::Elif)) {
      return dir;
    }
  }
  BLI_assert_unreachable();
  return Directive::invalid();
}

Line AtomicLexer::line(int index) const
{
  return Line(this, index);
}

Directive AtomicLexer::directive(int index) const
{
  return Directive(this, index);
}

struct TokenStream {
  /* Ensure to add a Space after the previous token. */
  struct Space {};
  /* Will paste a "1" token in the stream. */
  struct True {};
  /* Will paste a "0" token in the stream. */
  struct False {};
  /* Concatenate the previous token with the next one. */
  struct Concatenate {};

  /* Data container. The tokens can be from different lexers.
   * We can't iterate using tok.next(). */
  Vector<Token> tokens;

 private:
  /* Needed for token pasting. */
  AtomicLexer &lex_;
  /* State tracking. */
  bool concat_next_ = false;
  /* Cached output buffer to avoid reallocations. */
  std::string result_buf_;

 public:
  explicit TokenStream(AtomicLexer &lex) : lex_(lex) {};

  /* Set size to 0. Doesn't reallocate. */
  void clear()
  {
    tokens.clear();
  }

  TokenStream &operator<<(const TokenStream &stream)
  {
    tokens.extend(stream.tokens);
    return *this;
  }

  TokenStream &operator<<(Token tok)
  {
    bool followed_by_space = tok.followed_by_whitespace();
    if (UNLIKELY(concat_next_)) {
      tok = paste_token(tokens.last().str(), tok.str(), followed_by_space);
      tok.flag = followed_by_space;
      tokens.last() = tok;
      concat_next_ = false;
    }
    else {
      tok.flag = followed_by_space;
      tokens.append(tok);
    }
    return *this;
  }
  /* NOTE: Not compatible with concatenation. */
  TokenStream &operator<<(True /*tok*/)
  {
    tokens.append(paste_token("1", "", false));
    return *this;
  }
  /* NOTE: Not compatible with concatenation. */
  TokenStream &operator<<(False /*tok*/)
  {
    tokens.append(paste_token("0", "", false));
    return *this;
  }

  template<typename IToken> TokenStream &operator<<(const TokenRange<IToken> &stream)
  {
    for (IToken tok = stream.begin; tok != stream.end; tok = tok.next()) {
      *this << tok;
    }
    /* "Cancel" concatenation in case range is empty. */
    concat_next_ = false;
    return *this;
  }

  TokenStream &operator<<(const Concatenate /*concat*/)
  {
    /* Don't concat if there is nothing to concatenate. */
    concat_next_ = !tokens.is_empty();
    return *this;
  }

  TokenStream &operator<<(Space /*space*/)
  {
    if (!tokens.is_empty()) {
      tokens.last().flag = true;
    }
    return *this;
  }

  /* TODO(fclem): Remove this. Only there for expansion parser. */
  std::string str()
  {
    result_buf_.clear();
    result_buf_.reserve(tokens.size() * 7);
    for (const auto stream_tok : tokens) {
      result_buf_ += stream_tok.str();
      if (stream_tok.flag) {
        result_buf_ += ' ';
      }
    }
    return result_buf_;
  }

  /* Wrapper to allow the same interface as a Token on a Token pointer. */
  struct Iterator {
    const TokenStream *stream;
    const Token *tok;

    Iterator(const TokenStream &stream, const Token *tok) : stream(&stream), tok(tok) {}
    Iterator(const Iterator &other) = default;

    bool is_valid() const
    {
      return tok != nullptr;
    }

    TokenType type() const
    {
      return tok ? tok->type() : Invalid;
    }
    TokenAtom atom() const
    {
      return tok ? tok->atom() : 0;
    }

    std::string_view str() const
    {
      return tok ? tok->str() : "";
    }

    bool followed_by_whitespace() const
    {
      return tok ? tok->flag : false;
    }

    Iterator next() const
    {
      if (tok == nullptr || tok + 1 >= stream->tokens.end()) {
        return Iterator(*stream, nullptr);
      }
      return Iterator(*stream, tok + 1);
    }
    Iterator prev() const
    {
      if (tok == nullptr || tok - 1 < stream->tokens.begin()) {
        return Iterator(*stream, nullptr);
      }
      return Iterator(*stream, tok - 1);
    }

    friend bool operator==(const Iterator &a, const Iterator &b)
    {
      return a.tok == b.tok;
    }
    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      return a.tok != b.tok;
    }

    friend bool operator==(const Iterator &a, TokenType b)
    {
      return a.type() == b;
    }
    friend bool operator!=(const Iterator &a, TokenType b)
    {
      return a.type() != b;
    }

    /* For iterator compatibility. */
    Iterator &operator++()
    {
      if (tok == nullptr || tok + 1 >= stream->tokens.end()) {
        tok = nullptr;
      }
      else {
        ++tok;
      }
      return *this;
    }
    const Iterator &operator*()
    {
      return *this;
    }
  };

  TokenStream &operator<<(const Iterator &it)
  {
    *this << *it.tok;
    tokens.last().flag = it.followed_by_whitespace();
    return *this;
  }

  Iterator begin() const
  {
    if (tokens.is_empty()) {
      return end();
    }
    return Iterator(*this, tokens.begin());
  }

  Iterator end() const
  {
    return Iterator(*this, nullptr);
  }

 private:
  Token paste_token(const StringRef &a, const StringRef &b, bool followed_by_space)
  {
    std::string pasted;
    pasted.reserve(a.size() + b.size() + followed_by_space);
    pasted.append(a);
    pasted.append(b);
    if (followed_by_space) {
      pasted += ' ';
    }
    return lex_.paste_token(pasted, Word, followed_by_space);
  }
};

inline PruningStream &operator<<(PruningStream &dst, const TokenStream &src)
{
  for (const auto tok : src.tokens) {
    dst.parse_token(tok);
    dst << tok.str();
    if (tok.flag) {
      dst << " ";
    }
  }
  return dst;
}

inline PruningStream &operator<<(PruningStream &dst, const TokenRange<Token> &range)
{
  dst.parse_token_range(range.begin, range.end);
  dst << range.begin.buf_->substr(range.begin, range.end, true);
  return dst;
}

inline PruningStream &operator<<(PruningStream &dst, const Token &tok)
{
  dst.parse_token(tok);
  dst << tok.str_with_whitespace();
  return dst;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preprocessor.
 * \{ */

/**
 * Fast C preprocessor implementation.
 *
 * This is not a compliant implementation, but it supports most common features.
 *
 * What is not supported:
 * - Trigraphs.
 * - Backslash at end of a line does not continue a token.
 * - Error checking is incomplete.
 *
 * Unsupported preprocessor directive (like #warning and #error) are left untouched.
 */
struct Preprocessor {
 private:
  AtomicLexer lex_;

  PruningStream out_stream_;

  /* Cache the expression lexer to avoid memory allocations. */
  shader::parser::ExpressionParser expression_parser;

  struct TokenStreamPool {
    /* Reference to lexer only for atom value lookups. */
    AtomicLexer &lex_;
    int allocated = 0;
    int used = 0;
    std::deque<TokenStream> pool;
    std::vector<int> free_indices;

    struct Deleter {
      TokenStreamPool *stack;
      int index;

      void operator()(TokenStream * /*stream*/)
      {
        stack->free_indices.push_back(index);
      }
    };

    using Ptr = std::unique_ptr<TokenStream, Deleter>;

    Ptr alloc()
    {
      int target_idx;
      if (free_indices.empty()) {
        target_idx = pool.size();
        pool.emplace_back(lex_);
      }
      else {
        target_idx = free_indices.back();
        free_indices.pop_back();
      }

      TokenStream &s = pool[target_idx];
      s.clear();
      return Ptr(&s, Deleter{this, target_idx});
    }
  };

  using TokenStreamPtr = TokenStreamPool::Ptr;

  struct Macro {
    Directive id;
    TokenStreamPtr definition;
    Vector<TokenAtom> params;
    bool is_function = false;
    bool contains_concat = false;

    Macro(Directive id) : id(id) {}

    bool is_empty() const
    {
      return definition->tokens.is_empty();
    }
  };

  /* When evaluating a condition directive inside this stack, disregard the directive and jump to
   * the matching #endif. */
  Vector<Directive, 8> jump_stack;
  /* Own stack to avoid memory allocation during recursive expansion parsing. */
  TokenStreamPool stream_pool = {lex_};
  /* Set of visited macros during recursion (blue painting stack). Using a vector for speed. */
  Vector<Directive> visited_macros;

  /* Maps containing currently active macros. Map their keyword to their definition. */
  Map<TokenAtom, Directive> defines;
  /* Cached parsed data for each Macro. Allow lazy parsing. Indexed by Directive. */
  Vector<std::unique_ptr<Macro>> parsed_macro;
  /* Allows fast checking of tokens not matching any declared macro. One bit per TokenAtom. */
  bits::BitVector<> enabled_macros;

  /**
   * State Tracking.
   */

  /* Next preprocessor directive to evaluate. Might be overwritten by conditional evaluation. */
  Directive next_directive = Directive::invalid();
  /* End of the last evaluated directive. Might be overwritten by conditional evaluation.
   * Used to resume token expansion after this line. */
  Line last_directive_end = Line::invalid();

  /* Cached keyword identifier. */
  TokenAtom defined_atom;
  TokenAtom va_args_atom;

 public:
  Preprocessor(const std::string_view str)
      : lex_(str),
        out_stream_(Token::invalid(&lex_),
                    lex_.hash("return"),
                    lex_.hash("thread"),
                    lex_.hash("device"),
                    /* In order to avoid too many false positive edges in the DCE graph, pass a
                     * list of common symbols that are builtin and shouldn't be considered as a
                     * function. */
                    Vector<TokenAtom>{
                        lex_.hash("layout"),
                        lex_.hash("float"),
                        lex_.hash("float2"),
                        lex_.hash("float3"),
                        lex_.hash("float4"),
                        lex_.hash("float2x2"),
                        lex_.hash("float3x2"),
                        lex_.hash("float4x2"),
                        lex_.hash("float2x3"),
                        lex_.hash("float3x3"),
                        lex_.hash("float4x3"),
                        lex_.hash("float2x4"),
                        lex_.hash("float3x4"),
                        lex_.hash("float4x4"),
                        lex_.hash("packed_float3"),
                        lex_.hash("packed_int3"),
                        lex_.hash("int"),
                        lex_.hash("int2"),
                        lex_.hash("int3"),
                        lex_.hash("int4"),
                        lex_.hash("uint"),
                        lex_.hash("uint2"),
                        lex_.hash("uint3"),
                        lex_.hash("uint4"),
                        lex_.hash("bool"),
                        lex_.hash("bool2"),
                        lex_.hash("bool3"),
                        lex_.hash("bool4"),
                        lex_.hash("int32_t"),
                        lex_.hash("uint32_t"),
                        lex_.hash("bool32_t"),
                        /* Builtin functions. */
                        lex_.hash("any"),
                        lex_.hash("all"),
                        lex_.hash("texelFetch"),
                        lex_.hash("texture"),
                        lex_.hash("textureLod"),
                        lex_.hash("textureSize"),
                        lex_.hash("imageLoad"),
                        lex_.hash("imageStore"),
                        lex_.hash("imageSize"),
                        lex_.hash("clamp"),
                        lex_.hash("mix"),
                        lex_.hash("dot"),
                        lex_.hash("abs"),
                        lex_.hash("min"),
                        lex_.hash("max"),
                        lex_.hash("for"),
                        lex_.hash("if"),
                        lex_.hash("while"),
                        lex_.hash("cos"),
                        lex_.hash("sin"),
                        lex_.hash("tan"),
                        lex_.hash("atan"),
                        lex_.hash("acos"),
                        lex_.hash("asin"),
                        lex_.hash("exp"),
                        lex_.hash("exp2"),
                        lex_.hash("log"),
                        lex_.hash("pow"),
                        lex_.hash("length"),
                        lex_.hash("floor"),
                        lex_.hash("ceil"),
                        lex_.hash("fract"),
                        lex_.hash("sqrt"),
                        lex_.hash("sign"),
                        lex_.hash("return"),
                        lex_.hash("intBitsToFloat"),
                        lex_.hash("floatBitsToInt"),
                        lex_.hash("uintBitsToFloat"),
                        lex_.hash("floatBitsToUint"),
                    }),
        enabled_macros(lex_.max_atom_value()),
        defined_atom(lex_.hash("defined")),
        va_args_atom(lex_.hash("__VA_ARGS__"))
  {
    /* From our stats. Should be enough for 100% of our cases. */
    defines.reserve(1000);
    /* Ensure one slot for each directive. */
    parsed_macro.resize(lex_.directive_lines.size());
  }

  void preprocess()
  {
    if (lex_.directive_lines.is_empty()) {
      out_stream_ << TokenRange<Token>{.begin = lex_.front(), .end = lex_.back()};
      return;
    }

    last_directive_end = lex_.line(0);
    next_directive = lex_.directive(0);
    /* Expand until the first directive. */
    if (lex_.line(0) != next_directive.first()) {
      expand_macros_in_range(lex_.line(0), next_directive.first().prev());
    }

    while (!next_directive.is_last()) {
      Directive id = next_directive;
      /* The next directive might be overwritten by evaluate_directive. Increment before call. */
      next_directive = id.next();
      evaluate_directive(id);

      expand_macros_in_range(last_directive_end.next(), next_directive.first().prev());
    }
    /* Evaluate last directive without calling next and creating an invalid ID. */
    evaluate_directive(next_directive);

    if (!last_directive_end.is_last()) {
      Line last_line = lex_.line(lex_.line_offsets.size() - 1);
      expand_macros_in_range(last_directive_end.next(), last_line);
    }
  }

  void optimize()
  {
    Vector<TokenAtom> entry_points;
    entry_points.append(lex_.hash("main"));

    out_stream_.optimize(entry_points.as_span());
  }

  std::string result_get()
  {
    return out_stream_.str();
  }

 private:
  /* Return valid value if hash is a known string. Is full hash lookup + hashing. */
  TokenAtom get_atom(StringRef str)
  {
    return lex_.hash(str);
  }

  void evaluate_directive(Directive dir)
  {
    DirectiveType dir_type = dir.type();

    /* Note: gets overwritten by conditional processing. */
    last_directive_end = dir.last();

    switch (dir_type) {
      case DirectiveType::Define:
        define_macro(dir);
        break;
      case DirectiveType::Undef:
        undefine_macro(dir);
        break;
      case DirectiveType::If:
      case DirectiveType::Ifdef:
      case DirectiveType::Ifndef:
      case DirectiveType::Elif:
      case DirectiveType::Else:
        process_conditional(dir, dir_type);
        break;
      case DirectiveType::Line:
        erase_lines(dir.first(), dir.last());
        break;
      case DirectiveType::Endif:
        erase_lines(dir.first(), dir.last());
        break;
      case DirectiveType::Pragma:
        process_pragma(dir);
        ATTR_FALLTHROUGH;
      case DirectiveType::Other:
        out_stream_ << dir.str_with_whitespace();
        break;
    }
  }

  /**
   * Pragmas.
   */

  TokenAtom blender_atom = lex_.hash("blender");
  TokenAtom dce_atom = lex_.hash("dead_code_elimination");
  TokenAtom off_atom = lex_.hash("off");
  TokenAtom on_atom = lex_.hash("on");

  BLI_NOINLINE void process_pragma(Directive dir)
  {
    Token tok = dir.identifier().next();
    if (tok.atom() == blender_atom) {
      tok = tok.next();
      if (tok.atom() == dce_atom) {
        tok = tok.next();
        if (tok.atom() == off_atom) {
          out_stream_.set_enabled_parsing(false);
        }
        else if (tok.atom() == on_atom) {
          out_stream_.set_enabled_parsing(true);
        }
        else {
          BLI_assert_msg(false, "Invalid dead_code_elimination pragma. Expecting on or off.");
        }
      }
    }
  }

  /**
   * Macro Management.
   */

  /**
   * Parse macro parameters and advance the token cursor.
   */
  BLI_NOINLINE Vector<TokenAtom> parse_macro_params(Token &tok)
  {
    Vector<TokenAtom> macro_parameters;
    while (true) {
      /* Continue to the next name. */
      tok = tok.next();
      if (tok == Backslash && tok.next() == NewLine) {
        /* Preprocessor new line. Skip and continue. */
        tok = tok.next(2);
        continue;
      }
      if (tok == ParClose) {
        break;
      }
      macro_parameters.append(tok.str() == "..." ? va_args_atom : tok.atom());
      /* Continue to the next separator. */
      tok = tok.next();
      if (tok == Backslash && tok.next() == NewLine) {
        /* Preprocessor new line. Skip and continue. */
        tok = tok.next(2);
        continue;
      }
      if (tok == ParClose) {
        break;
      }
    }
    /* Skip closing parenthesis. */
    tok = tok.next();

    return macro_parameters;
  }

  BLI_NOINLINE TokenStreamPtr parse_macro_definition(Token definition_start_tok,
                                                     bool &contains_concat)
  {
    TokenStreamPtr definition = stream_pool.alloc();

    Token tok = definition_start_tok;
    while (true) {
      Token tok_next = tok.next();
      if (ELEM(tok, NewLine, lexit::EndOfFile)) {
        break;
      }
      if (tok == Backslash && tok_next == NewLine) {
        /* Preprocessor new line. Skip and continue. */
        tok = tok.next(2);
        /* Still insert a space to avoid merging tokens. */
        *definition << TokenStream::Space{};
        continue;
      }
      if (tok == Hash && tok_next == Hash) {
        /* Token Pasting operator. Emit a single Hash token. */
        *definition << tok;
        contains_concat = true;
        tok = tok.next(2);
        continue;
      }
      BLI_assert_msg(tok != Hash, "Stringify operator is not supported");
      *definition << tok;
      tok = tok_next;
    }
    return definition;
  }

  BLI_NOINLINE void define_macro(Directive dir)
  {
    const Token name = dir.identifier().next();
    BLI_assert(name == Word);
    defines.add_overwrite(name.atom(), dir);
    enabled_macros[name.atom()].set(true);
    erase_lines(dir.first(), dir.last());
  }

  BLI_NOINLINE Macro &get_macro(Directive dir)
  {
    if (parsed_macro[int(dir)] == nullptr) {
      const Token name = dir.identifier().next();
      const Token after_name = name.next();
      Token cursor = after_name;

      auto macro = std::make_unique<Macro>(dir);
      macro->is_function = (after_name == ParOpen) && !name.followed_by_whitespace();
      if (macro->is_function) {
        macro->params = parse_macro_params(cursor);
      }
      macro->definition = parse_macro_definition(cursor, macro->contains_concat);

      parsed_macro[int(dir)] = std::move(macro);
    }
    return *parsed_macro[int(dir)];
  }

  void undefine_macro(Directive dir)
  {
    const Token name = dir.identifier().next();
    BLI_assert(name == Word);
    enabled_macros[name.atom()].set(false);
    defines.remove(name.atom());
    erase_lines(dir.first(), dir.last());
  }

  /**
   * Condition directives.
   */

  BLI_NOINLINE void process_conditional(const Directive dir, const DirectiveType dir_type)
  {
    /* If this is part of an already evaluated statement. */
    if (!jump_stack.is_empty() && jump_stack.last() == dir) {
      jump_stack.pop_last();
      /* Find matching endif. */
      Directive endif = find_next_matching_conditional(dir);
      while (endif.type() != DirectiveType::Endif) {
        endif = find_next_matching_conditional(endif);
      }
      if (endif.is_last()) {
        /* Erase everything this and the last directive. */
        Line last_before_endif = endif.first().prev();
        erase_lines(dir.first(), last_before_endif);
        next_directive = endif;
        /* Don't expand inside this section. */
        last_directive_end = last_before_endif;
      }
      else {
        /* Erase everything between this directive and the #endif (inclusive). */
        Line endif_end = endif.last();
        erase_lines(dir.first(), endif_end);
        /* Evaluate after the endif. */
        next_directive = endif.next();
        /* Don't expand inside this section. */
        last_directive_end = endif_end;
      }
      return;
    }

    const Line dir_line_start = dir.first();
    const Line dir_line_end = dir.last();
    const Token dir_tok = dir.identifier();
    /* Evaluate condition. */
    const Token cond_start = dir_tok.next();
    const Token cond_end = dir_line_end.last();
    const bool condition_result = evaluate_condition(dir_type, cond_start, cond_end);

    /* Find matching endif or else. */
    const Directive next_condition = find_next_matching_conditional(dir);

    if (condition_result) {
      /* If is followed by else statement. */
      DirectiveType next_dir_type = next_condition.type();
      if (ELEM(next_dir_type, DirectiveType::Elif, DirectiveType::Else)) {
        /* Record a jump statement at the next #else statement to jump & erase to the #endif. */
        jump_stack.append(next_condition);
      }
      /* Erase condition and continue parsing content.
       * The #endif will just be erased later. */
      erase_lines(dir_line_start, dir_line_end);
    }
    else {
      Line last_before_next_cond = next_condition.first().prev();
      /* Erase everything until next condition (this directive included). */
      erase_lines(dir_line_start, last_before_next_cond);
      /* Jump to next condition. */
      next_directive = next_condition;
      /* Don't expand inside this section. */
      last_directive_end = last_before_next_cond;
    }
  }

  bool evaluate_condition(const DirectiveType dir_type, Token start, Token end)
  {
    switch (dir_type) {
      case DirectiveType::Else:
        return true;
      case DirectiveType::Ifdef:
        return defines.contains(start.atom());
      case DirectiveType::Ifndef:
        return !defines.contains(start.atom());
      case DirectiveType::If:
      case DirectiveType::Elif:
        return evaluate_expression(start, end);
      default:
        BLI_assert_unreachable();
        return true;
    }
  }

  bool evaluate_expression(const Token start, const Token end)
  {
    /* Expand expression into integer ops string. */
    TokenStreamPtr expand = expand_expression(start, end);

    /* Early out simple cases. */
    if (expand->tokens.size() == 1) {
      const auto &token = expand->tokens.first();
      StringRef str = token.str();
      if (str == "0") {
        return false;
      }
      if (str == "1") {
        return true;
      }
    }

    try {
      /* TODO(fclem): Do not parse again. Simply use the token stream. */
      std::string str = expand->str();
      expression_parser.lexical_analysis(str);
      return expression_parser.eval() != 0;
    }
    catch (const std::exception &e) {
      std::cout << "\"" << lex_.substr(start, end) << "\" > \"" << expand->str() << "\" ";
      std::cerr << "Error: " << e.what() << "\n";
      return false;
    }
  }

  /**
   * Macro Expansion.
   */

  BLI_NOINLINE void expand_macros_in_range(const Line start_line, const Line end_line)
  {
    int start = int(start_line.first());
    int end = int(end_line.end());
    if (start > end) {
      return;
    }
    if (start == end) {
      out_stream_ << lex_[start];
      return;
    }

    const Vector<int> &candidates = gather_candidate_in_range(start, end);

    Token after_last_emitted = lex_[start];

    for (const auto *it = candidates.begin(); it != candidates.end();) {
      const Token tok = lex_[*it];
      const Directive macro_id = defines.lookup(tok.atom());
      /* Emit tokens between the last emitted token and this one. */
      if (int(after_last_emitted) < *it) {
        out_stream_ << TokenRange<Token>{after_last_emitted, tok.prev()};
      }

      if (macro_id == Directive::invalid()) {
        out_stream_ << tok;
        after_last_emitted = tok.next();
        ++it;
        continue;
      }

      const Token end_tok = expand_and_replace(tok, macro_id);
      after_last_emitted = end_tok.next();
      /* Skip candidates already expanded. */
      while (it != candidates.end() && *it <= int(end_tok)) {
        ++it;
      }
    }

    const Token last_tok = lex_[end];
    out_stream_ << TokenRange<Token>{after_last_emitted, last_tok};
  }

  /* Cached vector to avoid reallocation. */
  Vector<int> expand_candidates_;

  /* Perform coarse check using small cache table of defined macros.
   * Returns a vector of token index that can potentially expand.
   * This avoid costly hash table lookups for every token. */
  BLI_NOINLINE const Vector<int> &gather_candidate_in_range(int start, int end)
  {
    expand_candidates_.clear();
    expand_candidates_.resize(end - start + 1);
    int candidates_count = 0;
    /* TODO(fclem): This is the major bottleneck after the actual expansion.
     * Can be sped up using SIMD. */
    for (int i : blender::IndexRange::from_begin_end(start, end)) {
      /* Branchless insertion. Faster than. */
      expand_candidates_[candidates_count] = i;
      candidates_count += enabled_macros[lex_.atoms_[i]];
    }
    expand_candidates_.resize(candidates_count);
    return expand_candidates_;
  }

  BLI_NOINLINE Token expand_and_replace(const Token tok, const Directive macro_id)
  {
    auto [replacement, end] = expand_macro(tok, get_macro(macro_id));
    out_stream_ << *replacement;
    return end;
  }

  /* Parse and expand with the current set of macro identifier. */
  BLI_NOINLINE TokenStreamPtr parse_and_expand(const TokenStream &tok_stream)
  {
    auto result = stream_pool.alloc();

    for (auto tok = tok_stream.begin(), end = tok_stream.end(); tok != end; ++tok) {
      if (tok == Word) {
        /* Try to match the token pointed at by cursor with a defined macro. If that happen advance
         * the cursor to the end of the macro (in case of functional macro). */
        Directive macro_id = defines.lookup_default(tok.atom(), Directive::invalid());
        if (macro_id != Directive::invalid()) {
          auto [replacement, end] = expand_macro(tok, get_macro(macro_id));
          *result << *replacement;
          tok = end;
          continue;
        }
      }
      *result << tok;
    }
    return result;
  }

  template<typename IToken> struct ExpandedResult {
    /* Replacement content. */
    TokenStreamPtr output;
    /* End of range to replace. */
    IToken end_of_expansion;
  };

  /**
   * \arg tok : Opening parenthesis token of the argument list.
   */
  template<typename IToken>
  BLI_NOINLINE Vector<TokenRange<IToken>> parse_macro_args(const Vector<TokenAtom> &params,
                                                           IToken &tok)
  {
    Vector<TokenRange<IToken>> args;
    for (const TokenAtom param_atom : params) {
      IToken param_start = tok;
      IToken param_end = get_end_of_parameter(param_start);

      if (param_atom == va_args_atom) {
        /* Seek end of argument list for variadic arguments. */
        param_end = get_end_of_parameter(param_start, true);
      }

      args.append({param_start.next(), param_end});
      /* Continue to the next separator. */
      tok = param_end;
      if (tok == Invalid) {
        break;
      }
    }
    /* No parameter case. */
    if (params.is_empty()) {
      /* Continue to end parenthesis. */
      tok = tok.next();
    }
    if (tok == Invalid) {
      /* Error: missing closing parenthesis. */
      /* Cancel expansion. */
      return {};
    }
    if (tok != ParClose) {
      /* Error: too many arguments provided to function-like macro invocation. */
      /* Cancel expansion. */
      return {};
    }
    return args;
  }

  template<typename IToken>
  BLI_NOINLINE TokenStreamPtr expand_macro_args(const Macro &macro,
                                                const Vector<TokenRange<IToken>> &macro_args)
  {
    TokenStreamPtr ts = stream_pool.alloc();
    for (const auto &def_tok : *macro.definition) {
      if (def_tok.type() == Hash) {
        *ts << TokenStream::Concatenate{};
        continue;
      }
      if (def_tok.type() == Word && !macro_args.is_empty()) {
        /* Lookup macro arguments. */
        int arg_id = macro.params.first_index_of_try(def_tok.atom());
        if (arg_id != -1) {
          const TokenRange<IToken> &macro_value = macro_args[arg_id];

          if (def_tok.prev() == Hash || def_tok.next() == Hash) {
            /* Token pasting. Do not expand now. */
            *ts << macro_value;
          }
          else {
            /* Expand argument. Can expand to the same macro (finite recursion). */
            TokenStreamPtr stream = stream_pool.alloc();
            *stream << macro_value;
            *ts << *parse_and_expand(*stream);
          }

          if (def_tok.followed_by_whitespace()) {
            /* Don't lose whitespace after macro token. */
            *ts << TokenStream::Space{};
          }
          continue;
        }
      }
      *ts << def_tok;
    }
    return ts;
  }

  template<typename IToken>
  BLI_NOINLINE ExpandedResult<IToken> expand_macro(const IToken expanded_tok, const Macro &macro)
  {
    if (visited_macros.contains(macro.id)) {
      /* Recursion. Do not expand. */
      /* Currently still replace by the original token (noop).
       * Would be better to not bypass replacement alltogether. */
      TokenStreamPtr expanded = stream_pool.alloc();
      *expanded << expanded_tok;
      return {std::move(expanded), expanded_tok};
    }

    IToken end_of_expansion = expanded_tok;

    Vector<TokenRange<IToken>> fn_arguments;
    if (macro.is_function) {
      IToken tok = expanded_tok;
      tok = tok.next();
      if (tok != ParOpen) {
        /* Macro doesn't have parameters. It should not expand. */
        /* Currently still replace by the original token (noop).
         * Would be better to bypass replacement alltogether. */
        TokenStreamPtr expanded = stream_pool.alloc();
        *expanded << expanded_tok;
        return {std::move(expanded), expanded_tok};
      }

      fn_arguments = parse_macro_args(macro.params, tok);
      /* Make sure to replace the whole call. */
      end_of_expansion = tok;
    }

    if (macro.is_empty()) {
      /* Empty definition. */
      TokenStreamPtr expanded = stream_pool.alloc();
      return {std::move(expanded), end_of_expansion};
    }

    TokenStreamPtr result;

    if (!macro.is_function && !macro.contains_concat) {
      /* Fast Path. */
      visited_macros.append(macro.id);
      result = parse_and_expand(*macro.definition);
      visited_macros.pop_last();
    }
    else {
      TokenStreamPtr expanded = expand_macro_args(macro, fn_arguments);
      /* Add to the set to avoid infinite recursion. */
      visited_macros.append(macro.id);
      result = parse_and_expand(*expanded);
      visited_macros.pop_last();
    }

    if (end_of_expansion.followed_by_whitespace()) {
      *result << TokenStream::Space{};
    }

    return {std::move(result), end_of_expansion};
  }

  /* Expand token range for condition evaluation (e.g. '#if'). */
  TokenStreamPtr expand_expression(const Token start, const Token end)
  {
    TokenStreamPtr result = stream_pool.alloc();

    Token tok = start;
    while (true) {
      BLI_assert(tok.is_valid());
      TokenAtom tok_atom = tok == Word ? tok.atom() : TokenAtom(0);

      Directive macro_id = defines.lookup_default(tok.atom(), Directive::invalid());

      if (tok_atom == TokenAtom(0)) {
        /* Non word. */
        *result << lex_[int(tok)];
      }
      else if (macro_id != Directive::invalid()) {
        Macro &macro = get_macro(macro_id);
        auto [replacement, macro_end] = expand_macro(Token(lex_[int(tok)]), macro);
        *result << *replacement;
        tok = lex_[int(macro_end)];
      }
      else if (tok_atom == defined_atom) {
        /* Parenthesis or space */
        tok = tok.next();
        const bool is_function = (tok == ParOpen);
        /* Token to search. */
        if (is_function) {
          tok = tok.next();
        }
        else {
          BLI_assert(tok == Word);
        }
        if (defines.contains(tok.atom())) {
          *result << TokenStream::True{};
        }
        else {
          *result << TokenStream::False{};
        }
        if (is_function) {
          /* End parenthesis. */
          tok = tok.next();
        }
      }
      else {
        /* Substitution failure. */
        *result << lex_[int(tok)];
      }
      if (tok == end) {
        break;
      }
      tok = skip_directive_newlines(tok.next());
    }

    return result;
  }

  /**
   * Utilities.
   */

  void erase_lines(Line start, Line end)
  {
    if (int(end) > int(start)) {
      out_stream_ << new_lines(start, end);
    }
    out_stream_ << end.end().str_with_whitespace();
  }

  /* Buffer of newlines since the StringRef must stay valid until result string is built.
   * Since this is only to replace content, we cannot have more lines than there is.
   * Avoid reallocation and persistent storage logic for a few KB of memory. */
  std::string new_lines_buf = std::string(lex_.line_offsets.size(), '\n');

  /* Return a string with the amount of newline character between line_start and line_end. */
  StringRef new_lines(Line line_start, Line line_end)
  {
    return StringRef(new_lines_buf).substr(0, int(line_end) - int(line_start));
  }

  Token skip_directive_newlines(Token tok)
  {
    while (tok == Backslash && tok.next() == NewLine) {
      tok = tok.next().next();
    }
    return tok;
  }

  /**
   * Return next `,` or `)` skipping occurrences contained in parenthesis.
   * Return invalid token on failure.
   */
  template<typename IToken>
  static IToken get_end_of_parameter(IToken tok, bool skip_to_end = false)
  {
    /* Avoid matching comma inside parameter function calls. */
    int stack = 1;
    tok = tok.next();
    while (tok.is_valid()) {
      if (tok == ParOpen) {
        stack++;
      }
      else if (tok == ParClose) {
        stack--;
      }
      if (stack == 0) {
        return tok;
      }
      if (stack == 1 && tok == Comma && !skip_to_end) {
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

std::string Shader::run_preprocessor(StringRef source, bool no_dead_code_elimination)
{
  BLI_assert_msg(source.find("//") == std::string::npos && source.find("/*") == std::string::npos,
                 "Input source to the preprocessor should have no comments.");

  if (G.debug & G_DEBUG_GPU_SHADER_NO_PREPROCESSOR) {
    return source;
  }

  Preprocessor processor(source);
  processor.preprocess();
  if (!no_dead_code_elimination) {
    processor.optimize();
  }
  return processor.result_get();
}

/** \} */

}  // namespace blender::gpu
