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
 */

/** \file
 * \ingroup bli
 */

#include "BLI_uuid.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <random>
#include <string>

/* Ensure the UUID struct doesn't have any padding, to be compatible with memcmp(). */
static_assert(sizeof(bUUID) == 16, "expect UUIDs to be 128 bit exactly");

bUUID BLI_uuid_generate_random()
{
  static std::mt19937_64 rng = []() {
    std::mt19937_64 rng;

    /* Ensure the RNG really can output 64-bit values. */
    static_assert(std::mt19937_64::min() == 0LL);
    static_assert(std::mt19937_64::max() == 0xffffffffffffffffLL);

    struct timespec ts;
#ifdef __APPLE__
    /* `timespec_get()` is only available on macOS 10.15+, so until that's the minimum version
     * supported by Blender, use another function to get the timespec.
     *
     * `clock_gettime()` is only available on POSIX, so not on Windows; Linux uses the newer C++11
     * function `timespec_get()` as well. */
    clock_gettime(CLOCK_REALTIME, &ts);
#else
    timespec_get(&ts, TIME_UTC);
#endif
    /* XOR the nanosecond and second fields, just in case the clock only has seconds resolution. */
    uint64_t seed = ts.tv_nsec;
    seed ^= ts.tv_sec;
    rng.seed(seed);

    return rng;
  }();

  bUUID uuid;

  /* RFC4122 suggests setting certain bits to a fixed value, and then randomizing the remaining
   * bits. The opposite is easier to implement, though, so that's what's done here. */

  /* Read two 64-bit numbers to randomize all 128 bits of the UUID. */
  uint64_t *uuid_as_int64 = reinterpret_cast<uint64_t *>(&uuid);
  uuid_as_int64[0] = rng();
  uuid_as_int64[1] = rng();

  /* Set the most significant four bits to 0b0100 to indicate version 4 (random UUID). */
  uuid.time_hi_and_version &= ~0xF000;
  uuid.time_hi_and_version |= 0x4000;

  /* Set the most significant two bits to 0b10 to indicate compatibility with RFC4122. */
  uuid.clock_seq_hi_and_reserved &= ~0x40;
  uuid.clock_seq_hi_and_reserved |= 0x80;

  return uuid;
}

bUUID BLI_uuid_nil(void)
{
  const bUUID nil = {0, 0, 0, 0, 0, 0};
  return nil;
}

bool BLI_uuid_is_nil(bUUID uuid)
{
  return BLI_uuid_equal(BLI_uuid_nil(), uuid);
}

bool BLI_uuid_equal(const bUUID uuid1, const bUUID uuid2)
{
  return std::memcmp(&uuid1, &uuid2, sizeof(uuid1)) == 0;
}

void BLI_uuid_format(char *buffer, const bUUID uuid)
{
  std::sprintf(buffer,
               "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               uuid.time_low,
               uuid.time_mid,
               uuid.time_hi_and_version,
               uuid.clock_seq_hi_and_reserved,
               uuid.clock_seq_low,
               uuid.node[0],
               uuid.node[1],
               uuid.node[2],
               uuid.node[3],
               uuid.node[4],
               uuid.node[5]);
}

bool BLI_uuid_parse_string(bUUID *uuid, const char *buffer)
{
  const int num_fields_parsed = std::sscanf(
      buffer,
      "%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
      &uuid->time_low,
      &uuid->time_mid,
      &uuid->time_hi_and_version,
      &uuid->clock_seq_hi_and_reserved,
      &uuid->clock_seq_low,
      &uuid->node[0],
      &uuid->node[1],
      &uuid->node[2],
      &uuid->node[3],
      &uuid->node[4],
      &uuid->node[5]);
  return num_fields_parsed == 11;
}

std::ostream &operator<<(std::ostream &stream, bUUID uuid)
{
  std::string buffer(36, '\0');
  BLI_uuid_format(buffer.data(), uuid);
  stream << buffer;
  return stream;
}
