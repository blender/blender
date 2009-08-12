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
void	bUnit_AsString(char *str, double value, int prec, int system, int type, int split, int pad);

/* replace units with values, used before python button evaluation */
int		bUnit_ReplaceString(char *str, char *str_orig, char *str_prev, double scale_pref, int system, int type);

/* the size of the unit used for this value (used for calculating the ckickstep) */
double bUnit_Size(double value, int system, int type);

#ifdef __cplusplus
}
#endif

#endif /* BKE_UNIT_H */
