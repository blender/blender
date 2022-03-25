/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbdds
 */

/* simple memory stream functions with buffer overflow check */

#pragma once

struct Stream {
  unsigned char *mem; /* location in memory */
  unsigned int size;  /* size */
  unsigned int pos;   /* current position */
  bool failed;        /* error occurred when seeking */
  Stream(unsigned char *m, unsigned int s) : mem(m), size(s), pos(0), failed(false)
  {
  }
  unsigned int seek(unsigned int p);
  void set_failed(const char *msg);
};

unsigned int mem_read(Stream &mem, unsigned long long &i);
unsigned int mem_read(Stream &mem, unsigned int &i);
unsigned int mem_read(Stream &mem, unsigned short &i);
unsigned int mem_read(Stream &mem, unsigned char &i);
unsigned int mem_read(Stream &mem, unsigned char *i, unsigned int count);
