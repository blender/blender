/* SPDX-FileCopyrightText: 2026 Clement Foucault
 *
 * SPDX-License-Identifier: MIT */

/**
 * LexIt is a lexer tool library focus on simplicity and efficiency.
 *
 * It is aimed at building source code processors without requiring huge dependencies like LLVM.
 * It only supports unextended-ASCII input which are under 4GB (because of 32bit offsets).
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <iterator>
#include <string_view>

#include "types.hh"

namespace lexit {

/**
 * Non-owning container for token datas stored in structure of array layout.
 */
class TokenBuffer {
 protected:
  /** Input string. */
  const uint8_t *str_;
  /** Length of the input string without including null terminator. */
  const uint32_t str_len_;
  /** Type of each token. */
  TokenType *types_;
  /** Starting character index of each token. */
  uint32_t *offsets_;
  /** Original character index of each next token before white-space merging (optional). */
  uint32_t *original_offsets_;
  /** Amount of tokens inside the buffer excluding the terminating EndOfFile token. */
  uint32_t size_;

 public:
  /**
   * @param c_str      An null-terminated C string.
   * @param str_len    Length of c_str excluding the null terminator.
   * @param types      An aligned array which can contain str_len+1 TokenType.
   * @param offsets    An aligned array which can contain str_len+1 uint32_t.
   * @param token_len  (optional) The amount of token already parsed.
   */
  TokenBuffer(const char *c_str,
              uint32_t str_len,
              TokenType *types,
              uint32_t *offsets,
              uint32_t token_len = 0)
      : str_((const uint8_t *)c_str),
        str_len_(str_len),
        types_(types),
        offsets_(offsets),
        original_offsets_(offsets),
        size_(token_len)
  {
  }

  /**
   * @param c_str             An null-terminated C string.
   * @param str_len           Length of c_str excluding the null terminator.
   * @param types             An aligned array which can contain str_len+1 TokenType.
   * @param offsets           An aligned array which can contain str_len+1 uint32_t.
   * @param original_offsets  An aligned array which can contain str_len+1 uint32_t.
   * @param token_len         The amount of token already parsed.
   */
  TokenBuffer(const char *c_str,
              uint32_t str_len,
              TokenType *types,
              uint32_t *offsets,
              uint32_t *original_offsets,
              uint32_t token_len)
      : str_((const uint8_t *)c_str),
        str_len_(str_len),
        types_(types),
        offsets_(offsets),
        original_offsets_(original_offsets),
        size_(token_len)
  {
  }

  /**
   * @brief Tokenizes the input string by grouping contiguous characters of the same class.
   *
   * This function iterates through the input string and identifies "runs" of characters
   * that map to the same CharClass. For each new group, it records the type and the
   * starting byte offset into the result arrays.
   *
   * Only characters with the #CanMerge flag are merged together.
   * Characters with class greater than #ClassToTypeThreshold will just be assigned their class as
   * #TokenType. Otherwise, the first character of the token will be used as #TokenType.
   *
   * @param char_class_table  A lookup table mapping ASCII values (0-127) to a 8-bit CharClass.
   */
  void tokenize(const CharClass char_class_table[128]);

  /**
   * @brief Return the amount of token inside the buffer.
   */
  uint32_t size() const
  {
    return size_;
  }

  /**
   * @brief Merge complex literals such as floats and strings.
   */
  void merge_complex_literals();

  /**
   * @brief Merge whitespaces with their preceding token.
   */
  void merge_whitespaces();

  struct Token {
    const std::string_view str;
    TokenType &type;
  };

  /**
   * @brief Token iterator.
   */
  struct TokenIt {
    using iterator_category = std::forward_iterator_tag;

   private:
    TokenBuffer *buf_;
    int32_t index_;

   public:
    explicit TokenIt(TokenBuffer *buf, int index) : buf_(buf), index_(index) {}

    Token operator*() const
    {
      int start = buf_->offsets_[index_];
      int end = buf_->original_offsets_[index_ + 1];
      assert(start < end);
      return Token{std::string_view((const char *)buf_->str_ + start, end - start),
                   buf_->types_[index_]};
    }

    TokenIt &operator++()
    {
      index_++;
      return *this;
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
};

}  // namespace lexit
