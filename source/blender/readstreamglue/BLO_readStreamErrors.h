/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * all Blender Read Stream errors
 */

#ifndef BLO_READSTREAMERRORS_H
#define BLO_READSTREAMERRORS_H

#ifdef __cplusplus
extern "C" {
#endif

#define BRS_SETFUNCTION(x)	(  (int)(x) << 1)
#define BRS_GETFUNCTION(x)	(( (int)(x) >> 1) & 7)
#define BRS_SETGENERR(x)	(  (int)(x) << 4)
#define BRS_GETGENERR(x)	(( (int)(x) >> 4) & 7)
#define BRS_SETSPECERR(x)	(  (int)(x) << 7)
#define BRS_GETSPECERR(x)	(( (int)(x) >> 7) & 7)

// FUNCTION
#define BRS_READSTREAMGLUE	1
#define BRS_READSTREAMLOOP	2
#define BRS_KEYSTORE		3
#define BRS_READSTREAMFILE	4
#define BRS_INFLATE			5
#define BRS_DECRYPT			6
#define BRS_VERIFY			7

// GENeric errors
#define BRS_MALLOC			1
#define BRS_NULL			2
#define BRS_MAGIC			3
#define BRS_CRCHEADER		4
#define BRS_CRCDATA			5
#define BRS_DATALEN			6
#define BRS_STUB			7

// READSTREAMGLUE specific
#define BRS_UNKNOWN			1

// READSTREAMFILE specific
#define BRS_NOTABLEND		1
#define BRS_READERROR		2

// INFLATE specific
#define BRS_INFLATEERROR	1

// DECRYPT specific
#define BRS_RSANEWERROR		1
#define BRS_DECRYPTERROR	2
#define BRS_NOTOURPUBKEY	3

// VERIFY specific
#define BRS_RSANEWERROR		1
#define BRS_SIGFAILED		2

#ifdef __cplusplus
}
#endif

#endif /* BLO_READSTREAMERRORS_H */

