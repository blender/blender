/*  DNA_action_types.h   May 2001
 *  
 *  support for the "action" datatype
 *
 *	Reevan McKay
 *
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
 */


#ifndef DNA_ACTION_TYPES_H
#define DNA_ACTION_TYPES_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_view2d_types.h"

struct SpaceLink;

typedef struct bPoseChannel{
	struct bPoseChannel	*next, *prev;
	ListBase			constraints;
	int					flag;
	float loc[3];
	float size[3];
	float quat[4];
	float obmat[4][4];
	char				name[32];	/* Channels need longer names than normal blender objects */
	int					reserved1;
} bPoseChannel;


typedef struct bPose{
	ListBase			chanbase;
} bPose;

typedef struct bActionChannel {
	struct bActionChannel	*next, *prev;
	struct Ipo				*ipo;
	ListBase				constraintChannels;
	int		flag;
	char	name[32];		/* Channel name */
	int		reserved1;

} bActionChannel;

typedef struct bAction {
	ID				id;
	ListBase		chanbase;	/* Channels in this action */
	bActionChannel	*achan;		/* Current action channel */
	bPoseChannel	*pchan;		/* Current pose channel */
} bAction;

typedef struct SpaceAction {
	struct SpaceLink *next, *prev;
	int spacetype, pad;
	struct ScrArea *area;

	View2D v2d;	
	bAction		*action;
	int	flag;
	short pin, reserved1;
	short	actnr;
	short	lock;
	int pad2;
} SpaceAction;

/* Action Channel flags */
#define	ACHAN_SELECTED	0x00000001
#define ACHAN_HILIGHTED	0x00000002

#endif

