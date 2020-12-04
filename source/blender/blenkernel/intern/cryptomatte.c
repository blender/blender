/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_cryptomatte.h"

#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "BLI_compiler_attrs.h"
#include "BLI_hash_mm3.h"
#include "BLI_string.h"
#include <string.h>

static uint32_t cryptomatte_hash(const ID *id)
{
  const char *name = &id->name[2];
  const int len = BLI_strnlen(name, MAX_NAME - 2);
  uint32_t cryptohash_int = BLI_hash_mm3((const unsigned char *)name, len, 0);
  return cryptohash_int;
}

uint32_t BKE_cryptomatte_object_hash(const Object *object)
{
  return cryptomatte_hash(&object->id);
}

uint32_t BKE_cryptomatte_material_hash(const Material *material)
{
  if (material == NULL) {
    return 0.0f;
  }
  return cryptomatte_hash(&material->id);
}

uint32_t BKE_cryptomatte_asset_hash(const Object *object)
{
  const Object *asset_object = object;
  while (asset_object->parent != NULL) {
    asset_object = asset_object->parent;
  }
  return cryptomatte_hash(&asset_object->id);
}

/* Convert a cryptomatte hash to a float.
 *
 * Cryptomatte hashes are stored in float textures and images. The conversion is taken from the
 * cryptomatte specification. See Floating point conversion section in
 * https://github.com/Psyop/Cryptomatte/blob/master/specification/cryptomatte_specification.pdf.
 *
 * The conversion uses as many 32 bit floating point values as possible to minimize hash
 * collisions. Unfortunately not all 32 bits can be as NaN and Inf can be problematic.
 *
 * Note that this conversion assumes to be running on a L-endian system. */
float BKE_cryptomatte_hash_to_float(uint32_t cryptomatte_hash)
{
  uint32_t mantissa = cryptomatte_hash & ((1 << 23) - 1);
  uint32_t exponent = (cryptomatte_hash >> 23) & ((1 << 8) - 1);
  exponent = MAX2(exponent, (uint32_t)1);
  exponent = MIN2(exponent, (uint32_t)254);
  exponent = exponent << 23;
  uint32_t sign = (cryptomatte_hash >> 31);
  sign = sign << 31;
  uint32_t float_bits = sign | exponent | mantissa;
  float f;
  memcpy(&f, &float_bits, sizeof(uint32_t));
  return f;
}
