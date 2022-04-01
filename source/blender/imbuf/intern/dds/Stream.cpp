/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbdds
 */

#include <Stream.h>

#include <cstdio>  /* printf */
#include <cstring> /* memcpy */

static const char *msg_error_seek = "DDS: trying to seek beyond end of stream (corrupt file?)";
static const char *msg_error_read = "DDS: trying to read beyond end of stream (corrupt file?)";

inline bool is_read_within_bounds(const Stream &mem, unsigned int count)
{
  if (mem.pos >= mem.size) {
    /* No more data remained in the memory buffer. */
    return false;
  }

  if (count > mem.size - mem.pos) {
    /* Reading past the memory bounds. */
    return false;
  }

  return true;
}

unsigned int Stream::seek(unsigned int p)
{
  if (p > size) {
    set_failed(msg_error_seek);
  }
  else {
    pos = p;
  }

  return pos;
}

unsigned int mem_read(Stream &mem, unsigned long long &i)
{
  if (!is_read_within_bounds(mem, 8)) {
    mem.set_failed(msg_error_seek);
    return 0;
  }
  memcpy(&i, mem.mem + mem.pos, 8); /* @@ todo: make sure little endian */
  mem.pos += 8;
  return 8;
}

unsigned int mem_read(Stream &mem, unsigned int &i)
{
  if (!is_read_within_bounds(mem, 4)) {
    mem.set_failed(msg_error_read);
    return 0;
  }
  memcpy(&i, mem.mem + mem.pos, 4); /* @@ todo: make sure little endian */
  mem.pos += 4;
  return 4;
}

unsigned int mem_read(Stream &mem, unsigned short &i)
{
  if (!is_read_within_bounds(mem, 2)) {
    mem.set_failed(msg_error_read);
    return 0;
  }
  memcpy(&i, mem.mem + mem.pos, 2); /* @@ todo: make sure little endian */
  mem.pos += 2;
  return 2;
}

unsigned int mem_read(Stream &mem, unsigned char &i)
{
  if (!is_read_within_bounds(mem, 1)) {
    mem.set_failed(msg_error_read);
    return 0;
  }
  i = (mem.mem + mem.pos)[0];
  mem.pos += 1;
  return 1;
}

unsigned int mem_read(Stream &mem, unsigned char *i, unsigned int count)
{
  if (!is_read_within_bounds(mem, count)) {
    mem.set_failed(msg_error_read);
    return 0;
  }
  memcpy(i, mem.mem + mem.pos, count);
  mem.pos += count;
  return count;
}

void Stream::set_failed(const char *msg)
{
  if (!failed) {
    puts(msg);
    failed = true;
  }
}
