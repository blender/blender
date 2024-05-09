/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Display characters using an ascii table.
 */

bool char_intersect(uvec2 bitmap_position)
{
  /* Using 8x8 = 64bits = uvec2. */
  uvec2 ascii_bitmap[96] = uvec2[96](uvec2(0x00000000u, 0x00000000u),
                                     uvec2(0x18001800u, 0x183c3c18u),
                                     uvec2(0x00000000u, 0x36360000u),
                                     uvec2(0x7f363600u, 0x36367f36u),
                                     uvec2(0x301f0c00u, 0x0c3e031eu),
                                     uvec2(0x0c666300u, 0x00633318u),
                                     uvec2(0x3b336e00u, 0x1c361c6eu),
                                     uvec2(0x00000000u, 0x06060300u),
                                     uvec2(0x060c1800u, 0x180c0606u),
                                     uvec2(0x180c0600u, 0x060c1818u),
                                     uvec2(0x3c660000u, 0x00663cffu),
                                     uvec2(0x0c0c0000u, 0x000c0c3fu),
                                     uvec2(0x000c0c06u, 0x00000000u),
                                     uvec2(0x00000000u, 0x0000003fu),
                                     uvec2(0x000c0c00u, 0x00000000u),
                                     uvec2(0x06030100u, 0x6030180cu),
                                     uvec2(0x6f673e00u, 0x3e63737bu),
                                     uvec2(0x0c0c3f00u, 0x0c0e0c0cu),
                                     uvec2(0x06333f00u, 0x1e33301cu),
                                     uvec2(0x30331e00u, 0x1e33301cu),
                                     uvec2(0x7f307800u, 0x383c3633u),
                                     uvec2(0x30331e00u, 0x3f031f30u),
                                     uvec2(0x33331e00u, 0x1c06031fu),
                                     uvec2(0x0c0c0c00u, 0x3f333018u),
                                     uvec2(0x33331e00u, 0x1e33331eu),
                                     uvec2(0x30180e00u, 0x1e33333eu),
                                     uvec2(0x000c0c00u, 0x000c0c00u),
                                     uvec2(0x000c0c06u, 0x000c0c00u),
                                     uvec2(0x060c1800u, 0x180c0603u),
                                     uvec2(0x003f0000u, 0x00003f00u),
                                     uvec2(0x180c0600u, 0x060c1830u),
                                     uvec2(0x0c000c00u, 0x1e333018u),
                                     uvec2(0x7b031e00u, 0x3e637b7bu),
                                     uvec2(0x3f333300u, 0x0c1e3333u),
                                     uvec2(0x66663f00u, 0x3f66663eu),
                                     uvec2(0x03663c00u, 0x3c660303u),
                                     uvec2(0x66361f00u, 0x1f366666u),
                                     uvec2(0x16467f00u, 0x7f46161eu),
                                     uvec2(0x16060f00u, 0x7f46161eu),
                                     uvec2(0x73667c00u, 0x3c660303u),
                                     uvec2(0x33333300u, 0x3333333fu),
                                     uvec2(0x0c0c1e00u, 0x1e0c0c0cu),
                                     uvec2(0x33331e00u, 0x78303030u),
                                     uvec2(0x36666700u, 0x6766361eu),
                                     uvec2(0x46667f00u, 0x0f060606u),
                                     uvec2(0x6b636300u, 0x63777f7fu),
                                     uvec2(0x73636300u, 0x63676f7bu),
                                     uvec2(0x63361c00u, 0x1c366363u),
                                     uvec2(0x06060f00u, 0x3f66663eu),
                                     uvec2(0x3b1e3800u, 0x1e333333u),
                                     uvec2(0x36666700u, 0x3f66663eu),
                                     uvec2(0x38331e00u, 0x1e33070eu),
                                     uvec2(0x0c0c1e00u, 0x3f2d0c0cu),
                                     uvec2(0x33333f00u, 0x33333333u),
                                     uvec2(0x331e0c00u, 0x33333333u),
                                     uvec2(0x7f776300u, 0x6363636bu),
                                     uvec2(0x1c366300u, 0x6363361cu),
                                     uvec2(0x0c0c1e00u, 0x3333331eu),
                                     uvec2(0x4c667f00u, 0x7f633118u),
                                     uvec2(0x06061e00u, 0x1e060606u),
                                     uvec2(0x30604000u, 0x03060c18u),
                                     uvec2(0x18181e00u, 0x1e181818u),
                                     uvec2(0x00000000u, 0x081c3663u),
                                     uvec2(0x000000ffu, 0x00000000u),
                                     uvec2(0x00000000u, 0x0c0c1800u),
                                     uvec2(0x3e336e00u, 0x00001e30u),
                                     uvec2(0x66663b00u, 0x0706063eu),
                                     uvec2(0x03331e00u, 0x00001e33u),
                                     uvec2(0x33336e00u, 0x3830303eu),
                                     uvec2(0x3f031e00u, 0x00001e33u),
                                     uvec2(0x06060f00u, 0x1c36060fu),
                                     uvec2(0x333e301fu, 0x00006e33u),
                                     uvec2(0x66666700u, 0x0706366eu),
                                     uvec2(0x0c0c1e00u, 0x0c000e0cu),
                                     uvec2(0x3033331eu, 0x30003030u),
                                     uvec2(0x1e366700u, 0x07066636u),
                                     uvec2(0x0c0c1e00u, 0x0e0c0c0cu),
                                     uvec2(0x7f6b6300u, 0x0000337fu),
                                     uvec2(0x33333300u, 0x00001f33u),
                                     uvec2(0x33331e00u, 0x00001e33u),
                                     uvec2(0x663e060fu, 0x00003b66u),
                                     uvec2(0x333e3078u, 0x00006e33u),
                                     uvec2(0x66060f00u, 0x00003b6eu),
                                     uvec2(0x1e301f00u, 0x00003e03u),
                                     uvec2(0x0c2c1800u, 0x080c3e0cu),
                                     uvec2(0x33336e00u, 0x00003333u),
                                     uvec2(0x331e0c00u, 0x00003333u),
                                     uvec2(0x7f7f3600u, 0x0000636bu),
                                     uvec2(0x1c366300u, 0x00006336u),
                                     uvec2(0x333e301fu, 0x00003333u),
                                     uvec2(0x0c263f00u, 0x00003f19u),
                                     uvec2(0x0c0c3800u, 0x380c0c07u),
                                     uvec2(0x18181800u, 0x18181800u),
                                     uvec2(0x0c0c0700u, 0x070c0c38u),
                                     uvec2(0x00000000u, 0x6e3b0000u),
                                     uvec2(0x00000000u, 0x00000000u));

  if (any(lessThan(bitmap_position, uvec2(0))) || any(greaterThanEqual(bitmap_position, uvec2(8))))
  {
    return false;
  }
  uint char_bits = ascii_bitmap[char_index % 96u][bitmap_position.y >> 2u & 1u];
  char_bits = (char_bits >> ((bitmap_position.y & 3u) * 8u + bitmap_position.x));
  return (char_bits & 1u) != 0u;
}

void main()
{
  uvec2 bitmap_position = uvec2(gl_PointCoord.xy * 8.0);
#ifndef GPU_METAL /* Metal has different gl_PointCoord.y. */
  /* Point coord start from top left corner. But layout is from bottom to top. */
  bitmap_position.y = 7 - bitmap_position.y;
#endif

  if (char_intersect(bitmap_position)) {
    out_color = vec4(1);
  }
  else if (char_intersect(bitmap_position + uvec2(0, 1))) {
    /* Shadow */
    out_color = vec4(0, 0, 0, 1);
  }
  else {
    /* Transparent Background for ease of read. */
    out_color = vec4(0, 0, 0, 0.2);
  }
}
