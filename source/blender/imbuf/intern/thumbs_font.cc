/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_hash_md5.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "IMB_thumbs.hh" /* own include. */

/* XXX, bad level call */
#include "../../blenfont/BLF_api.hh"

/* Only change if we need to update the previews in the on-disk cache. */
#define FONT_THUMB_VERSION "1.0.1"

ImBuf *IMB_thumb_load_font(const char *filepath, uint x, uint y)
{
  ImBuf *ibuf = IMB_allocImBuf(x, y, 32, IB_byte_data | IB_metadata);

  /* fill with white and zero alpha */
  const float col[4] = {1.0f, 1.0f, 1.0f, 0.0f};
  IMB_rectfill(ibuf, col);

  if (!BLF_thumb_preview(filepath, ibuf->byte_buffer.data, ibuf->x, ibuf->y, ibuf->channels)) {
    IMB_freeImBuf(ibuf);
    ibuf = nullptr;
  }

  return ibuf;
}

bool IMB_thumb_load_font_get_hash(char *r_hash)
{
  uchar digest[16];
  BLI_hash_md5_buffer(FONT_THUMB_VERSION, sizeof(FONT_THUMB_VERSION), digest);
  r_hash[0] = '\0';
  BLI_hash_md5_to_hexdigest(digest, r_hash);

  return true;
}

ImBuf *IMB_font_preview(const char *filepath,
                        uint width,
                        const float color[4],
                        const char *sample_text)
{
  int font_id = (filepath[0] != '<') ? BLF_load(filepath) : 0;
  if (font_id == -1) {
    return nullptr;
  }
  const char default_sample[] = "ABCDabefg&0123";
  const char *sample = sample_text ? sample_text : default_sample;

  BLF_buffer_col(font_id, color);

  BLF_size(font_id, 50.0f);
  float name_w;
  float name_h;
  BLF_width_and_height(font_id, sample, strlen(sample), &name_w, &name_h);
  const float scale = float(width) / name_w * 0.98f;
  BLF_size(font_id, scale * 50.0f);
  name_w *= scale;
  name_h *= scale;

  const int height = int(name_h * 1.8f);
  ImBuf *ibuf = IMB_allocImBuf(width, height, 32, IB_byte_data);
  /* fill with white and zero alpha */
  const float col[4] = {1.0f, 1.0f, 1.0f, 0.0f};
  IMB_rectfill(ibuf, col);

  BLF_buffer(font_id, ibuf->float_buffer.data, ibuf->byte_buffer.data, width, height, nullptr);

  BLF_position(font_id, 0.0f, height * 0.3f, 0.0f);
  BLF_draw_buffer(font_id, sample, 1024);

  BLF_buffer(font_id, nullptr, nullptr, 0, 0, nullptr);

  if (font_id != 0) {
    BLF_unload_id(font_id);
  }

  return ibuf;
}
