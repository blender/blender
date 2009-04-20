/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

@@@ this file should not be compiled any more @@@

#include <string.h>
#include <errno.h>
#include "stack.h"
#include "ode/error.h"
#include "ode/config.h"

//****************************************************************************
// unix version that uses mmap(). some systems have anonymous mmaps and some
// need to mmap /dev/zero.

#ifndef WIN32

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


void dStack::init (int max_size)
{
  if (sizeof(long int) != sizeof(char*)) dDebug (0,"internal");
  if (max_size <= 0) dDebug (0,"Stack::init() given size <= 0");

#ifndef MMAP_ANONYMOUS
  static int dev_zero_fd = -1;	// cached file descriptor for /dev/zero
  if (dev_zero_fd < 0) dev_zero_fd = open ("/dev/zero", O_RDWR);
  if (dev_zero_fd < 0) dError (0,"can't open /dev/zero (%s)",strerror(errno));
  base = (char*) mmap (0,max_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
		       dev_zero_fd,0);
#else
  base = (char*) mmap (0,max_size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANON,0,0);
#endif

  if (int(base) == -1) dError (0,"Stack::init(), mmap() failed, "
    "max_size=%d (%s)",max_size,strerror(errno));
  size = max_size;
  pointer = base;
  frame = 0;
}


void dStack::destroy()
{
  munmap (base,size);
  base = 0;
  size = 0;
  pointer = 0;
  frame = 0;
}

#endif

//****************************************************************************

#ifdef WIN32

#include "windows.h"


void dStack::init (int max_size)
{
  if (sizeof(LPVOID) != sizeof(char*)) dDebug (0,"internal");
  if (max_size <= 0) dDebug (0,"Stack::init() given size <= 0");
  base = (char*) VirtualAlloc (NULL,max_size,MEM_RESERVE,PAGE_READWRITE);
  if (base == 0) dError (0,"Stack::init(), VirtualAlloc() failed, "
    "max_size=%d",max_size);
  size = max_size;
  pointer = base;
  frame = 0;
  committed = 0;

  // get page size
  SYSTEM_INFO info;
  GetSystemInfo (&info);
  pagesize = info.dwPageSize;
}


void dStack::destroy()
{
  VirtualFree (base,0,MEM_RELEASE);
  base = 0;
  size = 0;
  pointer = 0;
  frame = 0;
}

#endif
