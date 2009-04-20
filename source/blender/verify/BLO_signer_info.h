/**
 * $Id$
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
 * 
 */

#include "BLO_sign_verify_Header.h"

/* external struct for signer info */

struct BLO_SignerInfo {
	char name[MAXSIGNERLEN];
	char email[MAXSIGNERLEN];
	char homeUrl[MAXSIGNERLEN];
	/* more to come... */
};

struct BLO_SignerInfo *BLO_getSignerInfo(void);
int BLO_isValidSignerInfo(struct BLO_SignerInfo *info);
void BLO_clrSignerInfo(struct BLO_SignerInfo *info);

