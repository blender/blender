// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef DRACO_CORE_ANS_H_
#define DRACO_CORE_ANS_H_
// An implementation of Asymmetric Numeral Systems (rANS).
// See http://arxiv.org/abs/1311.2540v2 for more information on rANS.
// This file is based off libvpx's ans.h.

#include <vector>

#define ANS_DIVIDE_BY_MULTIPLY 1
#if ANS_DIVIDE_BY_MULTIPLY
#include "draco/core/divide.h"
#endif
#include "draco/core/macros.h"

namespace draco {

#if ANS_DIVIDE_BY_MULTIPLY

#define ANS_DIVREM(quotient, remainder, dividend, divisor) \
  do {                                                     \
    quotient = fastdiv(dividend, divisor);                 \
    remainder = dividend - quotient * divisor;             \
  } while (0)
#define ANS_DIV(dividend, divisor) fastdiv(dividend, divisor)
#else
#define ANS_DIVREM(quotient, remainder, dividend, divisor) \
  do {                                                     \
    quotient = dividend / divisor;                         \
    remainder = dividend % divisor;                        \
  } while (0)
#define ANS_DIV(dividend, divisor) ((dividend) / (divisor))
#endif

struct AnsCoder {
  AnsCoder() : buf(nullptr), buf_offset(0), state(0) {}
  uint8_t *buf;
  int buf_offset;
  uint32_t state;
};

struct AnsDecoder {
  AnsDecoder() : buf(nullptr), buf_offset(0), state(0) {}
  const uint8_t *buf;
  int buf_offset;
  uint32_t state;
};

typedef uint8_t AnsP8;
#define ans_p8_precision 256u
#define ans_p8_shift 8
#define ans_p10_precision 1024u

#define l_base (ans_p10_precision * 4)  // l_base % precision must be 0
#define io_base 256
// Range I = { l_base, l_base + 1, ..., l_base * io_base - 1 }

static uint32_t mem_get_le16(const void *vmem) {
  uint32_t val;
  const uint8_t *mem = (const uint8_t *)vmem;

  val = mem[1] << 8;
  val |= mem[0];
  return val;
}

static uint32_t mem_get_le24(const void *vmem) {
  uint32_t val;
  const uint8_t *mem = (const uint8_t *)vmem;

  val = mem[2] << 16;
  val |= mem[1] << 8;
  val |= mem[0];
  return val;
}

static inline uint32_t mem_get_le32(const void *vmem) {
  uint32_t val;
  const uint8_t *mem = (const uint8_t *)vmem;

  val = mem[3] << 24;
  val |= mem[2] << 16;
  val |= mem[1] << 8;
  val |= mem[0];
  return val;
}

static inline void mem_put_le16(void *vmem, uint32_t val) {
  uint8_t *mem = reinterpret_cast<uint8_t *>(vmem);

  mem[0] = (val >> 0) & 0xff;
  mem[1] = (val >> 8) & 0xff;
}

static inline void mem_put_le24(void *vmem, uint32_t val) {
  uint8_t *mem = reinterpret_cast<uint8_t *>(vmem);

  mem[0] = (val >> 0) & 0xff;
  mem[1] = (val >> 8) & 0xff;
  mem[2] = (val >> 16) & 0xff;
}

static inline void mem_put_le32(void *vmem, uint32_t val) {
  uint8_t *mem = reinterpret_cast<uint8_t *>(vmem);

  mem[0] = (val >> 0) & 0xff;
  mem[1] = (val >> 8) & 0xff;
  mem[2] = (val >> 16) & 0xff;
  mem[3] = (val >> 24) & 0xff;
}

static inline void ans_write_init(struct AnsCoder *const ans,
                                  uint8_t *const buf) {
  ans->buf = buf;
  ans->buf_offset = 0;
  ans->state = l_base;
}

static inline int ans_write_end(struct AnsCoder *const ans) {
  uint32_t state;
  DRACO_DCHECK_GE(ans->state, l_base);
  DRACO_DCHECK_LT(ans->state, l_base * io_base);
  state = ans->state - l_base;
  if (state < (1 << 6)) {
    ans->buf[ans->buf_offset] = (0x00 << 6) + state;
    return ans->buf_offset + 1;
  } else if (state < (1 << 14)) {
    mem_put_le16(ans->buf + ans->buf_offset, (0x01 << 14) + state);
    return ans->buf_offset + 2;
  } else if (state < (1 << 22)) {
    mem_put_le24(ans->buf + ans->buf_offset, (0x02 << 22) + state);
    return ans->buf_offset + 3;
  } else {
    DRACO_DCHECK(0 && "State is too large to be serialized");
    return ans->buf_offset;
  }
}

// rABS with descending spread
// p or p0 takes the place of l_s from the paper
// ans_p8_precision is m
static inline void rabs_desc_write(struct AnsCoder *ans, int val, AnsP8 p0) {
  const AnsP8 p = ans_p8_precision - p0;
  const unsigned l_s = val ? p : p0;
  unsigned quot, rem;
  if (ans->state >= l_base / ans_p8_precision * io_base * l_s) {
    ans->buf[ans->buf_offset++] = ans->state % io_base;
    ans->state /= io_base;
  }
  ANS_DIVREM(quot, rem, ans->state, l_s);
  ans->state = quot * ans_p8_precision + rem + (val ? 0 : p);
}

#define ANS_IMPL1 0
#define UNPREDICTABLE(x) x
static inline int rabs_desc_read(struct AnsDecoder *ans, AnsP8 p0) {
  int val;
#if ANS_IMPL1
  unsigned l_s;
#else
  unsigned quot, rem, x, xn;
#endif
  const AnsP8 p = ans_p8_precision - p0;
  if (ans->state < l_base && ans->buf_offset > 0) {
    ans->state = ans->state * io_base + ans->buf[--ans->buf_offset];
  }
#if ANS_IMPL1
  val = ans->state % ans_p8_precision < p;
  l_s = val ? p : p0;
  ans->state = (ans->state / ans_p8_precision) * l_s +
               ans->state % ans_p8_precision - (!val * p);
#else
  x = ans->state;
  quot = x / ans_p8_precision;
  rem = x % ans_p8_precision;
  xn = quot * p;
  val = rem < p;
  if (UNPREDICTABLE(val)) {
    ans->state = xn + rem;
  } else {
    // ans->state = quot * p0 + rem - p;
    ans->state = x - xn - p;
  }
#endif
  return val;
}

// rABS with ascending spread
// p or p0 takes the place of l_s from the paper
// ans_p8_precision is m
static inline void rabs_asc_write(struct AnsCoder *ans, int val, AnsP8 p0) {
  const AnsP8 p = ans_p8_precision - p0;
  const unsigned l_s = val ? p : p0;
  unsigned quot, rem;
  if (ans->state >= l_base / ans_p8_precision * io_base * l_s) {
    ans->buf[ans->buf_offset++] = ans->state % io_base;
    ans->state /= io_base;
  }
  ANS_DIVREM(quot, rem, ans->state, l_s);
  ans->state = quot * ans_p8_precision + rem + (val ? p0 : 0);
}

static inline int rabs_asc_read(struct AnsDecoder *ans, AnsP8 p0) {
  int val;
#if ANS_IMPL1
  unsigned l_s;
#else
  unsigned quot, rem, x, xn;
#endif
  const AnsP8 p = ans_p8_precision - p0;
  if (ans->state < l_base) {
    ans->state = ans->state * io_base + ans->buf[--ans->buf_offset];
  }
#if ANS_IMPL1
  val = ans->state % ans_p8_precision < p;
  l_s = val ? p : p0;
  ans->state = (ans->state / ans_p8_precision) * l_s +
               ans->state % ans_p8_precision - (!val * p);
#else
  x = ans->state;
  quot = x / ans_p8_precision;
  rem = x % ans_p8_precision;
  xn = quot * p;
  val = rem >= p0;
  if (UNPREDICTABLE(val)) {
    ans->state = xn + rem - p0;
  } else {
    // ans->state = quot * p0 + rem - p0;
    ans->state = x - xn;
  }
#endif
  return val;
}

#define rabs_read rabs_desc_read
#define rabs_write rabs_desc_write

// uABS with normalization
static inline void uabs_write(struct AnsCoder *ans, int val, AnsP8 p0) {
  AnsP8 p = ans_p8_precision - p0;
  const unsigned l_s = val ? p : p0;
  while (ans->state >= l_base / ans_p8_precision * io_base * l_s) {
    ans->buf[ans->buf_offset++] = ans->state % io_base;
    ans->state /= io_base;
  }
  if (!val)
    ans->state = ANS_DIV(ans->state * ans_p8_precision, p0);
  else
    ans->state = ANS_DIV((ans->state + 1) * ans_p8_precision + p - 1, p) - 1;
}

static inline int uabs_read(struct AnsDecoder *ans, AnsP8 p0) {
  AnsP8 p = ans_p8_precision - p0;
  int s;
  // unsigned int xp1;
  unsigned xp, sp;
  unsigned state = ans->state;
  while (state < l_base && ans->buf_offset > 0) {
    state = state * io_base + ans->buf[--ans->buf_offset];
  }
  sp = state * p;
  // xp1 = (sp + p) / ans_p8_precision;
  xp = sp / ans_p8_precision;
  // s = xp1 - xp;
  s = (sp & 0xFF) >= p0;
  if (UNPREDICTABLE(s))
    ans->state = xp;
  else
    ans->state = state - xp;
  return s;
}

static inline int uabs_read_bit(struct AnsDecoder *ans) {
  int s;
  unsigned state = ans->state;
  while (state < l_base && ans->buf_offset > 0) {
    state = state * io_base + ans->buf[--ans->buf_offset];
  }
  s = static_cast<int>(state & 1);
  ans->state = state >> 1;
  return s;
}

static inline int ans_read_init(struct AnsDecoder *const ans,
                                const uint8_t *const buf, int offset) {
  unsigned x;
  if (offset < 1)
    return 1;
  ans->buf = buf;
  x = buf[offset - 1] >> 6;
  if (x == 0) {
    ans->buf_offset = offset - 1;
    ans->state = buf[offset - 1] & 0x3F;
  } else if (x == 1) {
    if (offset < 2)
      return 1;
    ans->buf_offset = offset - 2;
    ans->state = mem_get_le16(buf + offset - 2) & 0x3FFF;
  } else if (x == 2) {
    if (offset < 3)
      return 1;
    ans->buf_offset = offset - 3;
    ans->state = mem_get_le24(buf + offset - 3) & 0x3FFFFF;
  } else {
    return 1;
  }
  ans->state += l_base;
  if (ans->state >= l_base * io_base)
    return 1;
  return 0;
}

static inline int ans_read_end(struct AnsDecoder *const ans) {
  return ans->state == l_base;
}

static inline int ans_reader_has_error(const struct AnsDecoder *const ans) {
  return ans->state < l_base && ans->buf_offset == 0;
}

struct rans_sym {
  uint32_t prob;
  uint32_t cum_prob;  // not-inclusive
};

// Class for performing rANS encoding using a desired number of precision bits.
// The max number of precision bits is currently 19. The actual number of
// symbols in the input alphabet should be (much) smaller than that, otherwise
// the compression rate may suffer.
template <int rans_precision_bits_t>
class RAnsEncoder {
 public:
  RAnsEncoder() {}

  // Provides the input buffer where the data is going to be stored.
  inline void write_init(uint8_t *const buf) {
    ans_.buf = buf;
    ans_.buf_offset = 0;
    ans_.state = l_rans_base;
  }

  // Needs to be called after all symbols are encoded.
  inline int write_end() {
    uint32_t state;
    DRACO_DCHECK_GE(ans_.state, l_rans_base);
    DRACO_DCHECK_LT(ans_.state, l_rans_base * io_base);
    state = ans_.state - l_rans_base;
    if (state < (1 << 6)) {
      ans_.buf[ans_.buf_offset] = (0x00 << 6) + state;
      return ans_.buf_offset + 1;
    } else if (state < (1 << 14)) {
      mem_put_le16(ans_.buf + ans_.buf_offset, (0x01 << 14) + state);
      return ans_.buf_offset + 2;
    } else if (state < (1 << 22)) {
      mem_put_le24(ans_.buf + ans_.buf_offset, (0x02 << 22) + state);
      return ans_.buf_offset + 3;
    } else if (state < (1 << 30)) {
      mem_put_le32(ans_.buf + ans_.buf_offset, (0x03u << 30u) + state);
      return ans_.buf_offset + 4;
    } else {
      DRACO_DCHECK(0 && "State is too large to be serialized");
      return ans_.buf_offset;
    }
  }

  // rANS with normalization
  // sym->prob takes the place of l_s from the paper
  // rans_precision is m
  inline void rans_write(const struct rans_sym *const sym) {
    const uint32_t p = sym->prob;
    while (ans_.state >= l_rans_base / rans_precision * io_base * p) {
      ans_.buf[ans_.buf_offset++] = ans_.state % io_base;
      ans_.state /= io_base;
    }
    // TODO(ostava): The division and multiplication should be optimized.
    ans_.state =
        (ans_.state / p) * rans_precision + ans_.state % p + sym->cum_prob;
  }

 private:
  static constexpr int rans_precision = 1 << rans_precision_bits_t;
  static constexpr int l_rans_base = rans_precision * 4;
  AnsCoder ans_;
};

struct rans_dec_sym {
  uint32_t val;
  uint32_t prob;
  uint32_t cum_prob;  // not-inclusive
};

// Class for performing rANS decoding using a desired number of precision bits.
// The number of precision bits needs to be the same as with the RAnsEncoder
// that was used to encode the input data.
template <int rans_precision_bits_t>
class RAnsDecoder {
 public:
  RAnsDecoder() {}

  // Initializes the decoder from the input buffer. The |offset| specifies the
  // number of bytes encoded by the encoder. A non zero return value is an
  // error.
  inline int read_init(const uint8_t *const buf, int offset) {
    unsigned x;
    if (offset < 1)
      return 1;
    ans_.buf = buf;
    x = buf[offset - 1] >> 6;
    if (x == 0) {
      ans_.buf_offset = offset - 1;
      ans_.state = buf[offset - 1] & 0x3F;
    } else if (x == 1) {
      if (offset < 2)
        return 1;
      ans_.buf_offset = offset - 2;
      ans_.state = mem_get_le16(buf + offset - 2) & 0x3FFF;
    } else if (x == 2) {
      if (offset < 3)
        return 1;
      ans_.buf_offset = offset - 3;
      ans_.state = mem_get_le24(buf + offset - 3) & 0x3FFFFF;
    } else if (x == 3) {
      ans_.buf_offset = offset - 4;
      ans_.state = mem_get_le32(buf + offset - 4) & 0x3FFFFFFF;
    } else {
      return 1;
    }
    ans_.state += l_rans_base;
    if (ans_.state >= l_rans_base * io_base)
      return 1;
    return 0;
  }

  inline int read_end() { return ans_.state == l_rans_base; }

  inline int reader_has_error() {
    return ans_.state < l_rans_base && ans_.buf_offset == 0;
  }

  inline int rans_read() {
    unsigned rem;
    unsigned quo;
    struct rans_dec_sym sym;
    while (ans_.state < l_rans_base && ans_.buf_offset > 0) {
      ans_.state = ans_.state * io_base + ans_.buf[--ans_.buf_offset];
    }
    // |rans_precision| is a power of two compile time constant, and the below
    // division and modulo are going to be optimized by the compiler.
    quo = ans_.state / rans_precision;
    rem = ans_.state % rans_precision;
    fetch_sym(&sym, rem);
    ans_.state = quo * sym.prob + rem - sym.cum_prob;
    return sym.val;
  }

  // Construct a lookup table with |rans_precision| number of entries.
  // Returns false if the table couldn't be built (because of wrong input data).
  inline bool rans_build_look_up_table(const uint32_t token_probs[],
                                       uint32_t num_symbols) {
    lut_table_.resize(rans_precision);
    probability_table_.resize(num_symbols);
    uint32_t cum_prob = 0;
    uint32_t act_prob = 0;
    for (uint32_t i = 0; i < num_symbols; ++i) {
      probability_table_[i].prob = token_probs[i];
      probability_table_[i].cum_prob = cum_prob;
      cum_prob += token_probs[i];
      if (cum_prob > rans_precision) {
        return false;
      }
      for (uint32_t j = act_prob; j < cum_prob; ++j) {
        lut_table_[j] = i;
      }
      act_prob = cum_prob;
    }
    if (cum_prob != rans_precision) {
      return false;
    }
    return true;
  }

 private:
  inline void fetch_sym(struct rans_dec_sym *out, uint32_t rem) {
    uint32_t symbol = lut_table_[rem];
    out->val = symbol;
    out->prob = probability_table_[symbol].prob;
    out->cum_prob = probability_table_[symbol].cum_prob;
  }

  static constexpr int rans_precision = 1 << rans_precision_bits_t;
  static constexpr int l_rans_base = rans_precision * 4;
  std::vector<uint32_t> lut_table_;
  std::vector<rans_sym> probability_table_;
  AnsDecoder ans_;
};

#undef ANS_DIVREM

}  // namespace draco

#endif  // DRACO_CORE_ANS_H_
