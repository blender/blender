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
 */

#ifndef BDR_EDITCURVE_H
#define BDR_EDITCURVE_H

struct Object;
struct Curve;
struct Nurb;
struct BezTriple;
struct BPoint;
struct BezTripleNurb;

void set_actNurb(struct Nurb *nu);
struct Nurb * get_actNurb( void );

short isNurbsel(struct Nurb *nu);
int isNurbsel_count(struct Nurb *nu);
void printknots(void);
void load_editNurb(void);
void make_editNurb(void);
void remake_editNurb(void);
void separate_nurb(void);
short isNurbselUV(struct Nurb *nu, int *u, int *v, int flag);
void setflagsNurb(short flag);
void rotateflagNurb(short flag, float *cent, float rotmat[][3]);
void translateflagNurb(short flag, float *vec);
void weightflagNurb(short flag, float w, int mode);
void deleteflagNurb(short flag);
short extrudeflagNurb(int flag);
void adduplicateflagNurb(short flag);
void switchdirectionNurb2(void);
void switchdirection_knots(float *base, int tot);
void deselectall_nurb(void);
void hideNurb(int swap);
void revealNurb(void);
void selectswapNurb(void);
void subdivideNurb(void);

int convertspline(short type, struct Nurb *nu);
void setsplinetype(short type);
void rotate_direction_nurb(struct Nurb *nu);
int is_u_selected(struct Nurb *nu, int u);
void make_selection_list_nurb(void);
void merge_2_nurb(struct Nurb *nu1, struct Nurb *nu2);
void merge_nurb(void);
void addsegment_nurb(void);
void mouse_nurb(void);
void spinNurb(float *dvec, short mode);
void addvert_Nurb(int mode);
void extrude_nurb(void);
void makecyclicNurb(void);
void selectconnected_nurb(void);
void selectrow_nurb(void);
void selectend_nurb(short selfirst, short doswap, short selstatus);
void select_more_nurb(void);
void select_less_nurb(void);
void select_next_nurb(void);
void select_prev_nurb(void);
void select_random_nurb(void);
void select_every_nth_nurb(void);
void adduplicate_nurb(void);
void delNurb(void);
void nurb_set_smooth(short event);
int join_curve(int type);
struct Nurb *addNurbprim(int type, int stype, int newname);
void default_curve_ipo(struct Curve *cu);
void add_primitiveCurve(int stype);
void add_primitiveNurb(int type);
void clear_tilt(void);
void clever_numbuts_curve(void);         
int bezt_compare (const void *e1, const void *e2);
void setweightNurb( void );
void setradiusNurb( void );
void smoothradiusNurb( void );

extern void undo_push_curve(char *name);

#endif  /*  BDR_EDITCURVE_H */
