/* SPDX-FileCopyrightText: 2026 Clement Foucault
 *
 * SPDX-License-Identifier: MIT */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "types.hh"
#include "vector.hh"

#if defined(_MSC_VER)
#  define INLINE_METHOD __forceinline
#else
#  define INLINE_METHOD inline __attribute__((always_inline))
#endif

namespace lexit {

static constexpr uint64_t padded_string_masks[8] = {
    uint64_t(0xFFFFFFFFFFFFFFFF),
    uint64_t(0x00000000000000FF),
    uint64_t(0x000000000000FFFF),
    uint64_t(0x0000000000FFFFFF),
    uint64_t(0x00000000FFFFFFFF),
    uint64_t(0x000000FFFFFFFFFF),
    uint64_t(0x0000FFFFFFFFFFFF),
    uint64_t(0x00FFFFFFFFFFFFFF),
};

/**
 * Copy of a small string onto aligned bytes.
 * This avoids the cost of calling memcmp during comparison.
 * For performance reason, this needs to be constructed on string whose size falls into
 * the ((Size-1) * 8, Size * 8] range.
 * IMPORTANT: The string view given to the constructor *MUST* point to at least Size * 8
 * addressable byte region regardless of the string size.
 */
template<int Size> struct PaddedString {
  uint64_t data[Size];
  uint32_t size;

  PaddedString() = default;

  /* Caller need to ensure enough bytes are accessible after the end of string. */
  explicit PaddedString(std::string_view str)
  {
    size = str.size();
    assert(str.size() > 8 * (Size - 1));
    assert(str.size() <= 8 * Size);

    std::memcpy(data, (const char *)str.data(), sizeof(data));
    /* Fast way of masking the excess chars. */
    data[Size - 1] &= padded_string_masks[str.size() & (sizeof(data[0]) - 1)];
  }

  operator std::string_view() const
  {
    return {(const char *)data, size};
  }
};

struct Keyword {
  std::string_view str;
  TokenType type;
  TokenAtom atom;

  Keyword() = default;

  Keyword(std::string_view str, TokenType type, TokenAtom atom) : str(str), type(type), atom(atom)
  {
  }
};

struct IdentifierMap {
  struct alignas(8) Identifier {
    uint16_t next;
    uint16_t size;
    uint32_t hash;

    /** WORKAROUND: MSVC does not support trailing null size array. */
    uint64_t *data() const
    {
      return (uint64_t *)(&hash + 1);
    }

    /* Caller must ensure size matches. */
    template<int Size> bool operator==(const PaddedString<Size> &str) const
    {
      if (size != str.size) {
        return false;
      }
      return std::equal(data(), data() + Size, str.data);
    }

    bool operator==(std::string_view str) const
    {
      return std::string_view{(const char *)data(), size} == str;
    }
  };

  lexit::Vector<Identifier> identifier_buffer;

  /* Note: Must be power of two size. */
  static constexpr uint32_t hash_table_size = 16384;
  static constexpr uint32_t hash_table_index_mask = (hash_table_size - 1);
  std::array<uint16_t, hash_table_size> hash_table;

  IdentifierMap()
  {
    /* Set invalid values for all the table. */
    std::memset(hash_table.data(), 0xFFu, sizeof(uint16_t) * hash_table.size());
  }

  void reserve(int token_count)
  {
    identifier_buffer.reserve(token_count);
  }

  /* Return the maximum value for the currently allocated atoms. */
  TokenAtom max_atom_value() const
  {
    return identifier_buffer.size();
  }

  static constexpr uint32_t str_hash(std::string_view s)
  {
    uint32_t hash = 5381;
    hash = ((hash << 5) + hash) + s.size();
    hash = ((hash << 5) + hash) + static_cast<uint8_t>(s[0]);
    hash = ((hash << 5) + hash) + static_cast<uint8_t>(s[s.size() / 2]);
    hash = ((hash << 5) + hash) + static_cast<uint8_t>(s.back());
    return static_cast<uint16_t>(hash);
  }

  template<typename StringT> INLINE_METHOD TokenAtom lookup_or_add(StringT str)
  {
    uint32_t hash = str_hash(str);
    uint16_t index = hash_table[hash & hash_table_index_mask];

    Identifier *id = nullptr;
    for (; index != 0xFFFFu; index = id->next) {
      id = &identifier_buffer[index];
      if (id->hash == hash && *id == str) [[likely]] {
        return index;
      }
    }
    return add_after(hash, str, id);
  }

  TokenAtom add_after(uint32_t hash, std::string_view str, Identifier *id = nullptr)
  {
    /* Cache miss. Add new. */
    uint16_t new_index = identifier_buffer.size();
    if (id) {
      /* Update previous element in the list. */
      id->next = new_index;
    }
    else {
      /* Update entry in table. */
      hash_table[hash & hash_table_index_mask] = new_index;
    }

    {
      /* Fast malloc replacement. */
      int str_as_id_size = ((str.size() + (sizeof(Identifier) - 1)) / sizeof(Identifier));
      identifier_buffer.reserve(new_index + 1 + str_as_id_size);
      Identifier &id = *identifier_buffer.end();
      identifier_buffer.increase_size_by_unchecked(1 + str_as_id_size);
      /* Construct new identifier. */
      id.next = 0xFFFFu;
      id.size = str.size();
      id.hash = hash;
      /* Zero the end of the memcpy for the fast comparison. */
      id.data()[((str.size() - 1) / sizeof(Identifier))] = 0;
      std::memcpy(id.data(), str.data(), str.size());
    }
    return new_index;
  }

  Keyword make_keyword(std::string_view str, TokenType type)
  {
    return Keyword(str, type, lookup_or_add(str));
  }
};

/* Convert Atom to Keyword types. */
struct KeywordTable {
  /* Indexed by string TokenAtom. */
  alignas(64) std::array<TokenType, 64> map;

  KeywordTable(const std::vector<Keyword> &vector)
  {
    /* We only lookup words, so a word mismatch should be a Noop. */
    map.fill(Word);
    for (auto keyword : vector) {
      /* Check for overflow. */
      assert(keyword.atom < 128);
      /* Check default case not being overwritten. */
      assert(keyword.atom != 0);
      map[keyword.atom / 2] = keyword.type;
    }
  }

  TokenType operator[](TokenAtom atom) const
  {
    /* Identifier are always allocated in 2 or more consecutive slots. Which means TokenAtom values
     * always increment by at least 2. Avoid wasting slots in the map by dividing the atom by 2.
     * If atom is bigger than the table, revert to 0 atom (invalid) which becomes a Noop by
     * returning Word. */
    return map[(atom / 2) * (atom < 128)];
  }
};

}  // namespace lexit
