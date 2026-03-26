/* SPDX-FileCopyrightText: 2026 Clement Foucault
 *
 * SPDX-License-Identifier: MIT */

/**
 * LexIt is a lexer tool library focus on simplicity and efficiency.
 *
 * It is aimed at building source code processors without requiring huge dependencies like LLVM.
 * It only supports unextended-ASCII inputs that are under 4GB (because of 32bit offsets).
 */

#pragma once

#include <cassert>
#include <climits>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string_view>

#include "identifier.hh"
#include "types.hh"
#include "vector.hh"

// #define LEXIT_DEBUG

#ifdef LEXIT_DEBUG
/* Make it a warning to avoid shipping with it. */
#  warning "Lexit debug mode enabled"

#  include <vector>
#endif

#if defined(_MSC_VER)
#  define INLINE_METHOD __forceinline
#else
#  define INLINE_METHOD inline __attribute__((always_inline))
#endif

namespace lexit {

struct TokenBuffer;

struct Token {
#ifdef LEXIT_DEBUG
  std::string_view debug_str_;
  const TokenType *debug_type_;
  const TokenAtom *debug_atom_;
#endif
  const TokenBuffer *buf_;
  int32_t index_;
  /* General purpose flag. */
  int32_t flag = 0;

  Token(const TokenBuffer *buf, int32_t index);

  /* Invalid token that can still be compared with other tokens. */
  static Token invalid(const TokenBuffer *buf);

  explicit operator int32_t() const
  {
    return index_;
  }

  bool is_valid() const
  {
    return type() != EndOfFile;
  }
  bool is_invalid() const
  {
    return type() == EndOfFile;
  }

  const TokenType &type() const;
  const TokenAtom &atom() const;

  std::string_view str() const;
  std::string_view str_with_whitespace() const;

  bool followed_by_whitespace() const;

  Token next(int i = 1) const;
  Token prev(int i = 1) const;

  friend bool operator==(const Token &a, const Token &b)
  {
    assert(a.buf_ == b.buf_);
    return a.index_ == b.index_;
  }
  friend bool operator!=(const Token &a, const Token &b)
  {
    assert(a.buf_ == b.buf_);
    return a.index_ != b.index_;
  }

  friend bool operator==(const Token &a, TokenType b)
  {
    return a.type() == b;
  }
  friend bool operator!=(const Token &a, TokenType b)
  {
    return a.type() != b;
  }
};

/* Same as Token but allow type assignment. */
struct TokenMut : public Token {
  TokenMut(TokenBuffer *buf, int32_t index) : Token(buf, index) {}

  TokenType &type();
  TokenAtom &atom();
};

template<typename T> struct AlignedDeleter {
  void operator()(T *p) const
  {
    ::operator delete[](p, std::align_val_t{64});
  }
};

struct TokenBuffer {
  /* Input string. */
  std::string_view str_;
  /* Type of each token. */
  AlignedArrayPtr<TokenType> types_;
  /* Starting character index of each token. */
  AlignedArrayPtr<uint32_t> offsets_;
  /* Original character index of each token before whitespace merging. */
  AlignedArrayPtr<uint32_t> offsets_end_;
  /* Length in characters of each token. A value of 127 means the real size is over 126. */
  AlignedArrayPtr<uint8_t> lengths_;
  /* Unique id for identifiers (Words). Externally set (optional). */
  AlignedArrayPtr<TokenAtom> atoms_;
  /* Number of tokens inside the buffer excluding the terminating EndOfFile token. */
  uint32_t size_ = 0;
  /* Number of tokens that can be contained. */
  uint32_t allocated_size_ = 0;

#ifdef LEXIT_DEBUG
  std::vector<std::string_view> token_str_debug_;
  std::vector<std::string_view> token_str_with_whitespace_debug_;
#endif

  TokenBuffer() = default;

  TokenBuffer(const std::string_view str, const CharClass char_class_table[128])
  {
    process(str, char_class_table);
  }

  /*
   * The given string lifetime should outlive the #TokenBuffer. No copy is done.
   */
  void process(const std::string_view str, const CharClass char_class_table[128])
  {
    assert(str.size() < UINT_MAX);
    str_ = str;
    clear();
    tokenize(char_class_table);
    compute_lengths();
  }

  /**
   * @brief Discard all currently held data. Does not reallocate.
   */
  void clear();

  /**
   * @brief Allocate backing memory for the given number of tokens and move currently held data.
   *
   * Does nothing if allocation is already large enough.
   */
  void reserve(const uint32_t count);

  /**
   * @brief Tokenizes the input string by grouping contiguous characters of the same class.
   *
   * This function iterates through the input string and identifies "runs" of characters
   * that map to the same CharClass. For each new group, it records the type and the
   * starting byte offset into the result arrays.
   *
   * Only characters with the #CanMerge flag are merged together.
   * Characters with a class greater than #ClassToTypeThreshold will just be assigned their class
   * as #TokenType. Otherwise, the first character of the token will be used as #TokenType.
   *
   * If the input string contains characters that are not inside the ASCII range, the result of
   * the operation is undefined and might cause segmentation fault.
   *
   * @param char_class_table  A lookup table mapping ASCII values (0-127) to an 8-bit CharClass.
   */
  void tokenize(const CharClass char_class_table[128]);

  /**
   * @brief Merge complex literals such as floats, strings and comments.
   */
  void merge_complex_literals();

  /**
   * @brief Assign keyword types and atoms for a small set of identifier.
   */
  void atomize_words(IdentifierMap &identifiers, const KeywordTable &keywords);

  /**
   * @brief Compute small token length for speeding up certain tasks.
   */
  void compute_lengths();

  /**
   * @brief Return the amount of token inside the buffer.
   */
  uint32_t size() const
  {
    return size_;
  }

  /**
   * @brief Return the substring between the start and end tokens (included).
   *
   * @param with_trailing_whitespaces If true, include the trailing whitespaces.
   */
  std::string_view substr(const Token &start,
                          const Token &end,
                          const bool with_trailing_whitespaces = false) const
  {
    int start_char = offsets_[int(start)];
    int end_char = (!with_trailing_whitespaces) ? offsets_end_[int(end)] : offsets_[int(end) + 1];
    return str_.substr(start_char, end_char - start_char);
  }

  /**
   * @brief Append a Token at the end of the buffer.
   */
  void append(TokenType type, TokenAtom atom, int32_t str_size, int32_t str_size_with_witespaces)
  {
    reserve(size_ + 1);
    types_[size_] = type;
    atoms_[size_] = atom;
    offsets_[size_ + 1] = offsets_[size_] + str_size_with_witespaces;
    offsets_end_[size_] = offsets_[size_] + str_size;
    size_++;
  }

  Token operator[](int index) const
  {
    return Token(this, index);
  }
  TokenMut operator[](int index)
  {
    return TokenMut(this, index);
  }

  /**
   * @brief Token iterator.
   */
  struct TokenIt {
    using iterator_category = std::forward_iterator_tag;

   private:
    TokenBuffer *buf_;
    int32_t index_;

   public:
    TokenIt(TokenBuffer *buf, const int index) : buf_(buf), index_(index) {}

    TokenMut operator*() const
    {
      return TokenMut(buf_, index_);
    }

    TokenIt &operator++()
    {
      index_++;
      return *this;
    }

    int32_t index() const
    {
      return index_;
    }

    bool operator==(const TokenIt &other) const
    {
      return index_ == other.index_;
    }
    bool operator!=(const TokenIt &other) const
    {
      return index_ != other.index_;
    }
    bool operator<(const TokenIt &other) const
    {
      return index_ < other.index_;
    }
  };

  TokenIt begin()
  {
    return TokenIt(this, 0);
  }
  TokenIt end()
  {
    return TokenIt(this, size_);
  }

  Token front()
  {
    return (*this)[0];
  }
  Token back()
  {
    return (*this)[size_ - 1];
  }

 private:
  inline void tokenize_scalar(uint32_t &__restrict offset,
                              uint32_t &__restrict cursor_begin,
                              uint32_t &__restrict cursor_end,
                              CharClass &__restrict prev_char_class,
                              bool &__restrict prev_whitespace,
                              uint32_t end,
                              const CharClass char_class_table[128]);

  template<int Size = 0>
  INLINE_METHOD void atomize_short_tokens_in_mask(uint64_t mask,
                                                  uint32_t tok_id_base,
                                                  IdentifierMap &id_map,
                                                  const KeywordTable &kw_table);

  INLINE_METHOD void atomize_tokens_in_mask(uint64_t mask,
                                            uint32_t tok_id_base,
                                            IdentifierMap &id_map,
                                            const KeywordTable &kw_table);
};

inline Token::Token(const TokenBuffer *buf, int32_t index) : buf_(buf)
{
  assert(buf_ != nullptr);
  /* Set to Invalid / EndOfFile token if out of range. */
  index_ = (index < 0 || index > buf_->size_) ? buf_->size_ : index;
#ifdef LEXIT_DEBUG
  debug_str_ = str_with_whitespace();
  debug_type_ = &type();
  debug_atom_ = &atom();
#endif
}

inline Token Token::invalid(const TokenBuffer *buf)
{
  return Token(buf, buf->size_);
}

inline const TokenType &Token::type() const
{
  return buf_->types_[index_];
}
inline TokenType &TokenMut::type()
{
  return const_cast<TokenBuffer *>(buf_)->types_[index_];
}

inline const TokenAtom &Token::atom() const
{
  return buf_->atoms_[index_];
}
inline TokenAtom &TokenMut::atom()
{
  return const_cast<TokenBuffer *>(buf_)->atoms_[index_];
}

inline Token Token::next(int i) const
{
  return Token(buf_, index_ + i);
}
inline Token Token::prev(int i) const
{
  return is_valid() ? Token(buf_, index_ - i) : Token(buf_, -1);
}

inline std::string_view Token::str() const
{
  int start = buf_->offsets_[index_];
  int end = buf_->offsets_end_[index_];
  return {buf_->str_.data() + start, size_t(end - start)};
}

inline std::string_view Token::str_with_whitespace() const
{
  int start = buf_->offsets_[index_];
  int end = buf_->offsets_[index_ + 1];
  return {buf_->str_.data() + start, size_t(end - start)};
}

inline bool Token::followed_by_whitespace() const
{
  assert(is_valid());
  return buf_->offsets_end_[index_] != buf_->offsets_[index_ + 1];
}

inline std::ostream &operator<<(std::ostream &os, const Token &tok)
{
  os << "Token(";
  os << "type='" << tok.type() << "', ";
  os << "atom=" << tok.atom() << ", ";
  os << "str=\"" << tok.str() << "\", ";
  os << "str_with_whitespace=\"" << tok.str_with_whitespace() << "\"";
  os << ")";
  return os;
}

}  // namespace lexit
