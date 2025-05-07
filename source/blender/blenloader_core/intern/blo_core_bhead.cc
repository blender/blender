/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_filereader.h"

#include "BLO_core_bhead.hh"

static void switch_endian_bh4(BHead4 *bhead)
{
  /* the ID_.. codes */
  if ((bhead->code & 0xFFFF) == 0) {
    bhead->code >>= 16;
  }

  if (bhead->code != BLO_CODE_ENDB) {
    BLI_endian_switch_int32(&bhead->len);
    BLI_endian_switch_int32(&bhead->SDNAnr);
    BLI_endian_switch_int32(&bhead->nr);
  }
}

static void switch_endian_small_bh8(SmallBHead8 *bhead)
{
  /* The ID_* codes. */
  if ((bhead->code & 0xFFFF) == 0) {
    bhead->code >>= 16;
  }

  if (bhead->code != BLO_CODE_ENDB) {
    BLI_endian_switch_int32(&bhead->len);
    BLI_endian_switch_int32(&bhead->SDNAnr);
    BLI_endian_switch_int32(&bhead->nr);
  }
}

static void switch_endian_large_bh8(LargeBHead8 *bhead)
{
  /* The ID_* codes. */
  if ((bhead->code & 0xFFFF) == 0) {
    bhead->code >>= 16;
  }

  if (bhead->code != BLO_CODE_ENDB) {
    BLI_endian_switch_int64(&bhead->len);
    BLI_endian_switch_int32(&bhead->SDNAnr);
    BLI_endian_switch_int64(&bhead->nr);
  }
}

static BHead bhead_from_bhead4(const BHead4 &bhead4)
{
  BHead bhead;
  bhead.code = bhead4.code;
  bhead.len = bhead4.len;
  bhead.old = reinterpret_cast<const void *>(uintptr_t(bhead4.old));
  bhead.SDNAnr = bhead4.SDNAnr;
  bhead.nr = bhead4.nr;
  return bhead;
}

static const void *old_ptr_from_uint64_ptr(const uint64_t ptr, const bool use_endian_swap)
{
  if constexpr (sizeof(void *) == 8) {
    return reinterpret_cast<const void *>(ptr);
  }
  else {
    return reinterpret_cast<const void *>(uintptr_t(uint32_from_uint64_ptr(ptr, use_endian_swap)));
  }
}

static BHead bhead_from_small_bhead8(const SmallBHead8 &small_bhead8, const bool use_endian_swap)
{
  BHead bhead;
  bhead.code = small_bhead8.code;
  bhead.len = small_bhead8.len;
  bhead.old = old_ptr_from_uint64_ptr(small_bhead8.old, use_endian_swap);
  bhead.SDNAnr = small_bhead8.SDNAnr;
  bhead.nr = small_bhead8.nr;
  return bhead;
}

static BHead bhead_from_large_bhead8(const LargeBHead8 &large_bhead8, const bool use_endian_swap)
{
  BHead bhead;
  bhead.code = large_bhead8.code;
  bhead.len = large_bhead8.len;
  bhead.old = old_ptr_from_uint64_ptr(large_bhead8.old, use_endian_swap);
  bhead.SDNAnr = large_bhead8.SDNAnr;
  bhead.nr = large_bhead8.nr;
  return bhead;
}

std::optional<BHead> BLO_readfile_read_bhead(FileReader *file,
                                             const BHeadType type,
                                             const bool do_endian_swap)
{
  switch (type) {
    case BHeadType::BHead4: {
      BHead4 bhead4{};
      bhead4.code = BLO_CODE_DATA;
      const int64_t readsize = file->read(file, &bhead4, sizeof(bhead4));
      if (readsize == sizeof(bhead4) || bhead4.code == BLO_CODE_ENDB) {
        if (do_endian_swap) {
          switch_endian_bh4(&bhead4);
        }
        return bhead_from_bhead4(bhead4);
      }
      break;
    }
    case BHeadType::SmallBHead8: {
      SmallBHead8 small_bhead8{};
      small_bhead8.code = BLO_CODE_DATA;
      const int64_t readsize = file->read(file, &small_bhead8, sizeof(small_bhead8));
      if (readsize == sizeof(small_bhead8) || small_bhead8.code == BLO_CODE_ENDB) {
        if (do_endian_swap) {
          switch_endian_small_bh8(&small_bhead8);
        }
        return bhead_from_small_bhead8(small_bhead8, do_endian_swap);
      }
      break;
    }
    case BHeadType::LargeBHead8: {
      LargeBHead8 large_bhead8{};
      large_bhead8.code = BLO_CODE_DATA;
      const int64_t readsize = file->read(file, &large_bhead8, sizeof(large_bhead8));
      if (readsize == sizeof(large_bhead8) || large_bhead8.code == BLO_CODE_ENDB) {
        if (do_endian_swap) {
          switch_endian_large_bh8(&large_bhead8);
        }
        return bhead_from_large_bhead8(large_bhead8, do_endian_swap);
      }
      break;
    }
  }
  return std::nullopt;
}
