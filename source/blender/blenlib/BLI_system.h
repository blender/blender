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

#ifndef __BLI_SYSTEM_H__
#define __BLI_SYSTEM_H__

#include <stdio.h>

/** \file
 * \ingroup bli
 */

int BLI_cpu_support_sse2(void);
void BLI_system_backtrace(FILE *fp);

/* Get CPU brand, result is to be MEM_freeN()-ed. */
char *BLI_cpu_brand_string(void);

/**
 * Obtain the hostname from the system.
 *
 * This simply determines the host's name, and doesn't do any DNS lookup of any
 * IP address of the machine. As such, it's only usable for identification
 * purposes, and not for reachability over a network.
 *
 * \param buffer: Character buffer to write the hostname into.
 * \param bufsize: Size of the character buffer, including trailing '\0'.
 */
void BLI_hostname_get(char *buffer, size_t bufsize);

/* Get maximum addressable memory in megabytes. */
size_t BLI_system_memory_max_in_megabytes(void);
int BLI_system_memory_max_in_megabytes_int(void);

/* getpid */
#ifdef WIN32
#  define BLI_SYSTEM_PID_H <process.h>
#else
#  define BLI_SYSTEM_PID_H <unistd.h>
#endif

#endif /* __BLI_SYSTEM_H__ */
