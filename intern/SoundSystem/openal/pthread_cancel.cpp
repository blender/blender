/* $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * FreeBSD 3.4 does not yet have pthread_cancel (3.5 and above do)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __FreeBSD__

#include <osreldate.h>

#if (__FreeBSD_version < 350000)
#include <pthread.h>

#define FD_READ             0x1
#define _FD_LOCK(_fd,_type,_ts)         _thread_fd_lock(_fd, _type, _ts)
#define _FD_UNLOCK(_fd,_type)		_thread_fd_unlock(_fd, _type)

int pthread_cancel(pthread_t pthread) {
    pthread_exit(NULL);
    return 0;
}

long fpathconf(int fd, int name)
{
    long            ret;

    if ((ret = _FD_LOCK(fd, FD_READ, NULL)) == 0) {
	ret = _thread_sys_fpathconf(fd, name);
	_FD_UNLOCK(fd, FD_READ);
    }
    return ret;
}

#endif

int pthread_atfork(void *a, void *b, void *c) {
    return 0;
}

#endif
