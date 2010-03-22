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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
 #include <math.h>			/* fabs */
#include <stdio.h>			/* for sprintf		*/

#include "BKE_global.h"		/* for G			*/
#include "BKE_utildefines.h"	/* ABS */

#include "DNA_view3d_types.h" /* for G.vd (view3d) */
#include "DNA_windowmanager_types.h" /* for G.vd (view3d) */

#include "WM_types.h"

#include "transform.h"

#if 0
static int updateNDofMotion(NDofInput *n); // return 0 when motion is null
#endif
static void resetNDofInput(NDofInput *n);

void initNDofInput(NDofInput *n)
{
	int i;

	n->flag = 0;
	n->axis = 0;
	
	resetNDofInput(n);
	
	for(i = 0; i < 3; i++)
	{
		n->factor[i] = 1.0f;
	}
}

static void resetNDofInput(NDofInput *n)
{	
	int i;
	for(i = 0; i < 6; i++)
	{
		n->fval[i] = 0.0f;
	}
}

 
int handleNDofInput(NDofInput *n, wmEvent *event)
{
	int retval = 0;
	// TRANSFORM_FIX_ME
#if 0
	switch(event)
	{
		case NDOFMOTION:
			if (updateNDofMotion(n) == 0)
			{
				retval = NDOF_NOMOVE;
			}
			else
			{
				retval = NDOF_REFRESH;
			}
			break;
		case NDOFBUTTON:
			if (val == 1) 
			{
				retval = NDOF_CONFIRM;
			}
			else if (val == 2) 
			{
				retval = NDOF_CANCEL;
				resetNDofInput(n);
				n->flag &= ~NDOF_INIT;
			}
			break;
	}
#endif 	
	return retval;
}

int hasNDofInput(NDofInput *n)
{
	return (n->flag & NDOF_INIT) == NDOF_INIT;
}

void applyNDofInput(NDofInput *n, float *vec)
{
	if (hasNDofInput(n))
	{
		int i, j;
		
		for (i = 0, j = 0; i < 6; i++)
		{
			if (n->axis & (1 << i))
			{
				vec[j] = n->fval[i] * n->factor[j];
				j++;
			}
		}
	}
}

// TRANSFORM_FIX_ME
#if 0

static int updateNDofMotion(NDofInput *n)
{
	float fval[7];
	int i;
	int retval = 0;

	getndof(fval);

	if (G.vd->ndoffilter)
		filterNDOFvalues(fval);

	for(i = 0; i < 6; i++)
	{
		if (!retval && fval[i] != 0.0f)
		{
			retval = 1;
		}
		
		n->fval[i] += fval[i] / 1024.0f;
	}
	
	n->flag |= NDOF_INIT;
	
	return retval;
}
#endif




