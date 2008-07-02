/**
 * blenlib/BKE_ipo.h (mar-2001 nzc)
 *	
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
#ifndef BKE_IPO_H
#define BKE_IPO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CfraElem {
	struct CfraElem *next, *prev;
	float cfra;
	int sel;
} CfraElem;

struct Ipo;
struct IpoCurve;
struct MTex;
struct Material;
struct Object;
struct Sequence;
struct ListBase;
struct BezTriple;
struct ID;
struct bPoseChannel;
struct bActionChannel;
struct rctf;

float frame_to_float(int cfra);

void free_ipo_curve(struct IpoCurve *icu);
void free_ipo(struct Ipo *ipo);
void ipo_default_v2d_cur(int blocktype, struct rctf *cur);
struct Ipo *add_ipo(char *name, int idcode);
struct Ipo *copy_ipo(struct Ipo *ipo);
void ipo_idnew(struct Ipo *ipo);
void make_local_obipo(struct Ipo *ipo);
void make_local_matipo(struct Ipo *ipo);
void make_local_keyipo(struct Ipo *ipo);
void make_local_ipo(struct Ipo *ipo);
struct IpoCurve *find_ipocurve(struct Ipo *ipo, int adrcode);

void calchandles_ipocurve(struct IpoCurve *icu);
void testhandles_ipocurve(struct IpoCurve *icu);
void sort_time_ipocurve(struct IpoCurve *icu);
int test_time_ipocurve(struct IpoCurve *icu);
void correct_bezpart(float *v1, float *v2, float *v3, float *v4);
int findzero(float x, float q0, float q1, float q2, float q3, float *o);
void berekeny(float f1, float f2, float f3, float f4, float *o, int b);
void berekenx(float *f, float *o, int b);
float eval_icu(struct IpoCurve *icu, float ipotime);
void calc_icu(struct IpoCurve *icu, float ctime);
float calc_ipo_time(struct Ipo *ipo, float ctime);
void calc_ipo(struct Ipo *ipo, float ctime);
void write_ipo_poin(void *poin, int type, float val);
float read_ipo_poin(void *poin, int type);
void *give_mtex_poin(struct MTex *mtex, int adrcode );

void *get_ipo_poin(struct ID *id, struct IpoCurve *icu, int *type);
void *get_pchan_ipo_poin(struct bPoseChannel *pchan, int adrcode);

void set_icu_vars(struct IpoCurve *icu);

void execute_ipo(struct ID *id, struct Ipo *ipo);
void execute_action_ipo(struct bActionChannel *achan, struct bPoseChannel *pchan);

void do_ipo_nocalc(struct Ipo *ipo);
void do_ipo(struct Ipo *ipo);
void do_mat_ipo(struct Material *ma);
void do_ob_ipo(struct Object *ob);
void do_seq_ipo(struct Sequence *seq, int cfra);
void do_ob_ipodrivers(struct Object *ob, struct Ipo *ipo, float ctime);

int has_ipo_code(struct Ipo *ipo, int code);
void do_all_data_ipos(void);
int calc_ipo_spec(struct Ipo *ipo, int adrcode, float *ctime);
void clear_delta_obipo(struct Ipo *ipo);
void add_to_cfra_elem(struct ListBase *lb, struct BezTriple *bezt);
void make_cfra_list(struct Ipo *ipo, struct ListBase *elems);

/* the sort is an IPO_Channel... */
int IPO_GetChannels(struct Ipo *ipo, short *channels);

float IPO_GetFloatValue(struct Ipo *ipo,
/*  						struct IPO_Channel channel, */
						/* channels are shorts... bit ugly for now*/
						short c,
						float ctime);

#ifdef __cplusplus
};
#endif

#endif

