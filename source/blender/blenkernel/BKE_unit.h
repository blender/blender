/**
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_UNIT_H
#define BKE_UNIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* in all cases the value is assumed to be scaled by the user preference */

/* humanly readable representation of a value in units (used for button drawing) */
void	bUnit_AsString(char *str, int len_max, double value, int prec, int system, int type, int split, int pad);

/* replace units with values, used before python button evaluation */
int		bUnit_ReplaceString(char *str, int len_max, char *str_prev, double scale_pref, int system, int type);

/* the size of the unit used for this value (used for calculating the ckickstep) */
double bUnit_ClosestScalar(double value, int system, int type);

/* base scale for these units */
double bUnit_BaseScalar(int system, int type);

/* loop over scales, coudl add names later */
//double bUnit_Iter(void **unit, char **name, int system, int type);

void	bUnit_GetSystem(void **usys_pt, int *len, int system, int type);
char*	bUnit_GetName(void *usys_pt, int index);
char*	bUnit_GetNameDisplay(void *usys_pt, int index);
double	bUnit_GetScaler(void *usys_pt, int index);

/* aligned with PropertyUnit */
#define		B_UNIT_NONE 0
#define 	B_UNIT_LENGTH 1
#define 	B_UNIT_AREA 2
#define 	B_UNIT_VOLUME 3
#define 	B_UNIT_MASS 4
#define 	B_UNIT_ROTATION 5
#define 	B_UNIT_TIME 6
#define 	B_UNIT_VELOCITY 7
#define 	B_UNIT_ACCELERATION 8

#ifdef __cplusplus
}
#endif

#endif /* BKE_UNIT_H */
