/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/key.c
 *  \ingroup bke
 */


#include <math.h>
#include <string.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_animsys.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"
#include "BKE_scene.h"


#include "RNA_access.h"

#define KEY_MODE_DUMMY      0 /* use where mode isn't checked for */
#define KEY_MODE_BPOINT     1
#define KEY_MODE_BEZTRIPLE  2

/* old defines from DNA_ipo_types.h for data-type, stored in DNA - don't modify! */
#define IPO_FLOAT       4
#define IPO_BEZTRIPLE   100
#define IPO_BPOINT      101


/** Free (or release) any data used by this shapekey (does not free the key itself). */
void BKE_key_free(Key *key)
{
	KeyBlock *kb;

	BKE_animdata_free((ID *)key, false);

	while ((kb = BLI_pophead(&key->block))) {
		if (kb->data)
			MEM_freeN(kb->data);
		MEM_freeN(kb);
	}
}

void BKE_key_free_nolib(Key *key)
{
	KeyBlock *kb;

	while ((kb = BLI_pophead(&key->block))) {
		if (kb->data)
			MEM_freeN(kb->data);
		MEM_freeN(kb);
	}
}

Key *BKE_key_add(ID *id)    /* common function */
{
	Key *key;
	char *el;
	
	key = BKE_libblock_alloc(G.main, ID_KE, "Key");
	
	key->type = KEY_NORMAL;
	key->from = id;

	key->uidgen = 1;
	
	/* XXX the code here uses some defines which will soon be deprecated... */
	switch (GS(id->name)) {
		case ID_ME:
			el = key->elemstr;

			el[0] = 3;
			el[1] = IPO_FLOAT;
			el[2] = 0;

			key->elemsize = 12;

			break;
		case ID_LT:
			el = key->elemstr;

			el[0] = 3;
			el[1] = IPO_FLOAT;
			el[2] = 0;

			key->elemsize = 12;

			break;
		case ID_CU:
			el = key->elemstr;

			el[0] = 4;
			el[1] = IPO_BPOINT;
			el[2] = 0;

			key->elemsize = 16;

			break;
	}
	
	return key;
}

Key *BKE_key_copy(Main *bmain, const Key *key)
{
	Key *keyn;
	KeyBlock *kbn, *kb;
	
	keyn = BKE_libblock_copy(bmain, &key->id);
	
	BLI_duplicatelist(&keyn->block, &key->block);
	
	kb = key->block.first;
	kbn = keyn->block.first;
	while (kbn) {
		
		if (kbn->data) kbn->data = MEM_dupallocN(kbn->data);
		if (kb == key->refkey) keyn->refkey = kbn;
		
		kbn = kbn->next;
		kb = kb->next;
	}

	BKE_id_copy_ensure_local(bmain, &key->id, &keyn->id);

	return keyn;
}

Key *BKE_key_copy_nolib(Key *key)
{
	Key *keyn;
	KeyBlock *kbn, *kb;
	
	keyn = MEM_dupallocN(key);

	keyn->adt = NULL;

	BLI_duplicatelist(&keyn->block, &key->block);
	
	kb = key->block.first;
	kbn = keyn->block.first;
	while (kbn) {
		
		if (kbn->data) kbn->data = MEM_dupallocN(kbn->data);
		if (kb == key->refkey) keyn->refkey = kbn;
		
		kbn = kbn->next;
		kb = kb->next;
	}
	
	return keyn;
}

/* Sort shape keys and Ipo curves after a change.  This assumes that at most
 * one key was moved, which is a valid assumption for the places it's
 * currently being called.
 */

void BKE_key_sort(Key *key)
{
	KeyBlock *kb;
	KeyBlock *kb2;

	/* locate the key which is out of position */ 
	for (kb = key->block.first; kb; kb = kb->next)
		if ((kb->next) && (kb->pos > kb->next->pos))
			break;

	/* if we find a key, move it */
	if (kb) {
		kb = kb->next; /* next key is the out-of-order one */
		BLI_remlink(&key->block, kb);
		
		/* find the right location and insert before */
		for (kb2 = key->block.first; kb2; kb2 = kb2->next) {
			if (kb2->pos > kb->pos) {
				BLI_insertlinkafter(&key->block, kb2->prev, kb);
				break;
			}
		}
	}

	/* new rule; first key is refkey, this to match drawing channels... */
	key->refkey = key->block.first;
}

/**************** do the key ****************/

void key_curve_position_weights(float t, float data[4], int type)
{
	float t2, t3, fc;
	
	if (type == KEY_LINEAR) {
		data[0] =          0.0f;
		data[1] = -t     + 1.0f;
		data[2] =  t;
		data[3] =          0.0f;
	}
	else if (type == KEY_CARDINAL) {
		t2 = t * t;
		t3 = t2 * t;
		fc = 0.71f;

		data[0] = -fc          * t3  + 2.0f * fc          * t2 - fc * t;
		data[1] =  (2.0f - fc) * t3  + (fc - 3.0f)        * t2 + 1.0f;
		data[2] =  (fc - 2.0f) * t3  + (3.0f - 2.0f * fc) * t2 + fc * t;
		data[3] =  fc          * t3  - fc * t2;
	}
	else if (type == KEY_BSPLINE) {
		t2 = t * t;
		t3 = t2 * t;

		data[0] = -0.16666666f * t3  + 0.5f * t2   - 0.5f * t    + 0.16666666f;
		data[1] =  0.5f        * t3  - t2                        + 0.66666666f;
		data[2] = -0.5f        * t3  + 0.5f * t2   + 0.5f * t    + 0.16666666f;
		data[3] =  0.16666666f * t3;
	}
	else if (type == KEY_CATMULL_ROM) {
		t2 = t * t;
		t3 = t2 * t;
		fc = 0.5f;

		data[0] = -fc          * t3  + 2.0f * fc          * t2 - fc * t;
		data[1] =  (2.0f - fc) * t3  + (fc - 3.0f)        * t2 + 1.0f;
		data[2] =  (fc - 2.0f) * t3  + (3.0f - 2.0f * fc) * t2 + fc * t;
		data[3] =  fc          * t3  - fc * t2;
	}
}

/* first derivative */
void key_curve_tangent_weights(float t, float data[4], int type)
{
	float t2, fc;
	
	if (type == KEY_LINEAR) {
		data[0] = 0.0f;
		data[1] = -1.0f;
		data[2] = 1.0f;
		data[3] = 0.0f;
	}
	else if (type == KEY_CARDINAL) {
		t2 = t * t;
		fc = 0.71f;

		data[0] = -3.0f * fc          * t2  + 4.0f * fc * t                  - fc;
		data[1] =  3.0f * (2.0f - fc) * t2  + 2.0f * (fc - 3.0f) * t;
		data[2] =  3.0f * (fc - 2.0f) * t2  + 2.0f * (3.0f - 2.0f * fc) * t  + fc;
		data[3] =  3.0f * fc          * t2  - 2.0f * fc * t;
	}
	else if (type == KEY_BSPLINE) {
		t2 = t * t;

		data[0] = -0.5f * t2  + t         - 0.5f;
		data[1] =  1.5f * t2  - t * 2.0f;
		data[2] = -1.5f * t2  + t         + 0.5f;
		data[3] =  0.5f * t2;
	}
	else if (type == KEY_CATMULL_ROM) {
		t2 = t * t;
		fc = 0.5f;

		data[0] = -3.0f * fc          * t2  + 4.0f * fc * t                  - fc;
		data[1] =  3.0f * (2.0f - fc) * t2  + 2.0f * (fc - 3.0f) * t;
		data[2] =  3.0f * (fc - 2.0f) * t2  + 2.0f * (3.0f - 2.0f * fc) * t  + fc;
		data[3] =  3.0f * fc          * t2  - 2.0f * fc * t;
	}
}

/* second derivative */
void key_curve_normal_weights(float t, float data[4], int type)
{
	float fc;
	
	if (type == KEY_LINEAR) {
		data[0] = 0.0f;
		data[1] = 0.0f;
		data[2] = 0.0f;
		data[3] = 0.0f;
	}
	else if (type == KEY_CARDINAL) {
		fc = 0.71f;

		data[0] = -6.0f * fc          * t  + 4.0f * fc;
		data[1] =  6.0f * (2.0f - fc) * t  + 2.0f * (fc - 3.0f);
		data[2] =  6.0f * (fc - 2.0f) * t  + 2.0f * (3.0f - 2.0f * fc);
		data[3] =  6.0f * fc          * t  - 2.0f * fc;
	}
	else if (type == KEY_BSPLINE) {
		data[0] = -1.0f * t  + 1.0f;
		data[1] =  3.0f * t  - 2.0f;
		data[2] = -3.0f * t  + 1.0f;
		data[3] =  1.0f * t;
	}
	else if (type == KEY_CATMULL_ROM) {
		fc = 0.5f;

		data[0] = -6.0f * fc          * t  + 4.0f * fc;
		data[1] =  6.0f * (2.0f - fc) * t  + 2.0f * (fc - 3.0f);
		data[2] =  6.0f * (fc - 2.0f) * t  + 2.0f * (3.0f - 2.0f * fc);
		data[3] =  6.0f * fc          * t  - 2.0f * fc;
	}
}

static int setkeys(float fac, ListBase *lb, KeyBlock *k[], float t[4], int cycl)
{
	/* return 1 means k[2] is the position, return 0 means interpolate */
	KeyBlock *k1, *firstkey;
	float d, dpos, ofs = 0, lastpos;
	short bsplinetype;

	firstkey = lb->first;
	k1 = lb->last;
	lastpos = k1->pos;
	dpos = lastpos - firstkey->pos;

	if (fac < firstkey->pos) fac = firstkey->pos;
	else if (fac > k1->pos) fac = k1->pos;

	k1 = k[0] = k[1] = k[2] = k[3] = firstkey;
	t[0] = t[1] = t[2] = t[3] = k1->pos;

	/* if (fac < 0.0 || fac > 1.0) return 1; */

	if (k1->next == NULL) return 1;

	if (cycl) { /* pre-sort */
		k[2] = k1->next;
		k[3] = k[2]->next;
		if (k[3] == NULL) k[3] = k1;
		while (k1) {
			if (k1->next == NULL) k[0] = k1;
			k1 = k1->next;
		}
		/* k1 = k[1]; */ /* UNUSED */
		t[0] = k[0]->pos;
		t[1] += dpos;
		t[2] = k[2]->pos + dpos;
		t[3] = k[3]->pos + dpos;
		fac += dpos;
		ofs = dpos;
		if (k[3] == k[1]) {
			t[3] += dpos;
			ofs = 2.0f * dpos;
		}
		if (fac < t[1]) fac += dpos;
		k1 = k[3];
	}
	else {  /* pre-sort */
		k[2] = k1->next;
		t[2] = k[2]->pos;
		k[3] = k[2]->next;
		if (k[3] == NULL) k[3] = k[2];
		t[3] = k[3]->pos;
		k1 = k[3];
	}
	
	while (t[2] < fac) {    /* find correct location */
		if (k1->next == NULL) {
			if (cycl) {
				k1 = firstkey;
				ofs += dpos;
			}
			else if (t[2] == t[3]) {
				break;
			}
		}
		else {
			k1 = k1->next;
		}

		t[0] = t[1];
		k[0] = k[1];
		t[1] = t[2];
		k[1] = k[2];
		t[2] = t[3];
		k[2] = k[3];
		t[3] = k1->pos + ofs;
		k[3] = k1;

		if (ofs > 2.1f + lastpos) break;
	}
	
	bsplinetype = 0;
	if (k[1]->type == KEY_BSPLINE || k[2]->type == KEY_BSPLINE) bsplinetype = 1;


	if (cycl == 0) {
		if (bsplinetype == 0) {   /* B spline doesn't go through the control points */
			if (fac <= t[1]) {  /* fac for 1st key */
				t[2] = t[1];
				k[2] = k[1];
				return 1;
			}
			if (fac >= t[2]) {  /* fac after 2nd key */
				return 1;
			}
		}
		else if (fac > t[2]) {  /* last key */
			fac = t[2];
			k[3] = k[2];
			t[3] = t[2];
		}
	}

	d = t[2] - t[1];
	if (d == 0.0f) {
		if (bsplinetype == 0) {
			return 1;  /* both keys equal */
		}
	}
	else {
		d = (fac - t[1]) / d;
	}

	/* interpolation */
	key_curve_position_weights(d, t, k[1]->type);

	if (k[1]->type != k[2]->type) {
		float t_other[4];
		key_curve_position_weights(d, t_other, k[2]->type);
		interp_v4_v4v4(t, t, t_other, d);
	}

	return 0;

}

static void flerp(int tot, float *in, float *f0, float *f1, float *f2, float *f3, float *t)
{
	int a;

	for (a = 0; a < tot; a++) {
		in[a] = t[0] * f0[a] + t[1] * f1[a] + t[2] * f2[a] + t[3] * f3[a];
	}
}

static void rel_flerp(int tot, float *in, float *ref, float *out, float fac)
{
	int a;
	
	for (a = 0; a < tot; a++) {
		in[a] -= fac * (ref[a] - out[a]);
	}
}

static char *key_block_get_data(Key *key, KeyBlock *actkb, KeyBlock *kb, char **freedata)
{
	if (kb == actkb) {
		/* this hack makes it possible to edit shape keys in
		 * edit mode with shape keys blending applied */
		if (GS(key->from->name) == ID_ME) {
			Mesh *me;
			BMVert *eve;
			BMIter iter;
			float (*co)[3];
			int a;

			me = (Mesh *)key->from;

			if (me->edit_btmesh && me->edit_btmesh->bm->totvert == kb->totelem) {
				a = 0;
				co = MEM_mallocN(sizeof(float) * 3 * me->edit_btmesh->bm->totvert, "key_block_get_data");

				BM_ITER_MESH (eve, &iter, me->edit_btmesh->bm, BM_VERTS_OF_MESH) {
					copy_v3_v3(co[a], eve->co);
					a++;
				}

				*freedata = (char *)co;
				return (char *)co;
			}
		}
	}

	*freedata = NULL;
	return kb->data;
}


/* currently only the first value of 'ofs' may be set. */
static bool key_pointer_size(const Key *key, const int mode, int *poinsize, int *ofs)
{
	if (key->from == NULL) {
		return false;
	}

	switch (GS(key->from->name)) {
		case ID_ME:
			*ofs = sizeof(float) * 3;
			*poinsize = *ofs;
			break;
		case ID_LT:
			*ofs = sizeof(float) * 3;
			*poinsize = *ofs;
			break;
		case ID_CU:
			if (mode == KEY_MODE_BPOINT) {
				*ofs = sizeof(float) * 4;
				*poinsize = *ofs;
			}
			else {
				ofs[0] = sizeof(float) * 12;
				*poinsize = (*ofs) / 3;
			}

			break;
		default:
			BLI_assert(!"invalid 'key->from' ID type");
			return false;
	}

	return true;
}

static void cp_key(const int start, int end, const int tot, char *poin, Key *key, KeyBlock *actkb, KeyBlock *kb, float *weights, const int mode)
{
	float ktot = 0.0, kd = 0.0;
	int elemsize, poinsize = 0, a, *ofsp, ofs[32], flagflo = 0;
	char *k1, *kref, *freek1, *freekref;
	char *cp, elemstr[8];

	/* currently always 0, in future key_pointer_size may assign */
	ofs[1] = 0;

	if (!key_pointer_size(key, mode, &poinsize, &ofs[0]))
		return;

	if (end > tot) end = tot;
	
	if (tot != kb->totelem) {
		ktot = 0.0;
		flagflo = 1;
		if (kb->totelem) {
			kd = kb->totelem / (float)tot;
		}
		else {
			return;
		}
	}

	k1 = key_block_get_data(key, actkb, kb, &freek1);
	kref = key_block_get_data(key, actkb, key->refkey, &freekref);

	/* this exception is needed curves with multiple splines */
	if (start != 0) {
		
		poin += poinsize * start;
		
		if (flagflo) {
			ktot += start * kd;
			a = (int)floor(ktot);
			if (a) {
				ktot -= a;
				k1 += a * key->elemsize;
			}
		}
		else {
			k1 += start * key->elemsize;
		}
	}
	
	if (mode == KEY_MODE_BEZTRIPLE) {
		elemstr[0] = 1;
		elemstr[1] = IPO_BEZTRIPLE;
		elemstr[2] = 0;
	}
	
	/* just do it here, not above! */
	elemsize = key->elemsize;
	if (mode == KEY_MODE_BEZTRIPLE) elemsize *= 3;

	for (a = start; a < end; a++) {
		cp = key->elemstr;
		if (mode == KEY_MODE_BEZTRIPLE) cp = elemstr;

		ofsp = ofs;

		while (cp[0]) {

			switch (cp[1]) {
				case IPO_FLOAT:
					if (weights) {
						memcpy(poin, kref, sizeof(float) * 3);
						if (*weights != 0.0f)
							rel_flerp(cp[0], (float *)poin, (float *)kref, (float *)k1, *weights);
						weights++;
					}
					else {
						memcpy(poin, k1, sizeof(float) * 3);
					}
					break;
				case IPO_BPOINT:
					memcpy(poin, k1, sizeof(float) * 4);
					break;
				case IPO_BEZTRIPLE:
					memcpy(poin, k1, sizeof(float) * 12);
					break;
				default:
					/* should never happen */
					if (freek1) MEM_freeN(freek1);
					if (freekref) MEM_freeN(freekref);
					BLI_assert(!"invalid 'cp[1]'");
					return;
			}

			poin += *ofsp;
			cp += 2; ofsp++;
		}
		
		/* are we going to be nasty? */
		if (flagflo) {
			ktot += kd;
			while (ktot >= 1.0f) {
				ktot -= 1.0f;
				k1 += elemsize;
				kref += elemsize;
			}
		}
		else {
			k1 += elemsize;
			kref += elemsize;
		}
		
		if (mode == KEY_MODE_BEZTRIPLE) {
			a += 2;
		}
	}

	if (freek1) MEM_freeN(freek1);
	if (freekref) MEM_freeN(freekref);
}

static void cp_cu_key(Curve *cu, Key *key, KeyBlock *actkb, KeyBlock *kb, const int start, int end, char *out, const int tot)
{
	Nurb *nu;
	int a, step, a1, a2;

	for (a = 0, nu = cu->nurb.first; nu; nu = nu->next, a += step) {
		if (nu->bp) {
			step = nu->pntsu * nu->pntsv;

			a1 = max_ii(a, start);
			a2 = min_ii(a + step, end);

			if (a1 < a2) cp_key(a1, a2, tot, out, key, actkb, kb, NULL, KEY_MODE_BPOINT);
		}
		else if (nu->bezt) {
			step = 3 * nu->pntsu;

			/* exception because keys prefer to work with complete blocks */
			a1 = max_ii(a, start);
			a2 = min_ii(a + step, end);

			if (a1 < a2) cp_key(a1, a2, tot, out, key, actkb, kb, NULL, KEY_MODE_BEZTRIPLE);
		}
		else {
			step = 0;
		}
	}
}

void BKE_key_evaluate_relative(const int start, int end, const int tot, char *basispoin, Key *key, KeyBlock *actkb,
                               float **per_keyblock_weights, const int mode)
{
	KeyBlock *kb;
	int *ofsp, ofs[3], elemsize, b;
	char *cp, *poin, *reffrom, *from, elemstr[8];
	int poinsize, keyblock_index;

	/* currently always 0, in future key_pointer_size may assign */
	ofs[1] = 0;

	if (!key_pointer_size(key, mode, &poinsize, &ofs[0]))
		return;

	if (end > tot) end = tot;

	/* in case of beztriple */
	elemstr[0] = 1;              /* nr of ipofloats */
	elemstr[1] = IPO_BEZTRIPLE;
	elemstr[2] = 0;

	/* just here, not above! */
	elemsize = key->elemsize;
	if (mode == KEY_MODE_BEZTRIPLE) elemsize *= 3;

	/* step 1 init */
	cp_key(start, end, tot, basispoin, key, actkb, key->refkey, NULL, mode);
	
	/* step 2: do it */
	
	for (kb = key->block.first, keyblock_index = 0; kb; kb = kb->next, keyblock_index++) {
		if (kb != key->refkey) {
			float icuval = kb->curval;
			
			/* only with value, and no difference allowed */
			if (!(kb->flag & KEYBLOCK_MUTE) && icuval != 0.0f && kb->totelem == tot) {
				KeyBlock *refb;
				float weight, *weights = per_keyblock_weights ? per_keyblock_weights[keyblock_index] : NULL;
				char *freefrom = NULL, *freereffrom = NULL;

				/* reference now can be any block */
				refb = BLI_findlink(&key->block, kb->relative);
				if (refb == NULL) continue;
				
				poin = basispoin;
				from = key_block_get_data(key, actkb, kb, &freefrom);
				reffrom = key_block_get_data(key, actkb, refb, &freereffrom);

				poin += start * poinsize;
				reffrom += key->elemsize * start;  // key elemsize yes!
				from += key->elemsize * start;
				
				for (b = start; b < end; b++) {
				
					weight = weights ? (*weights * icuval) : icuval;
					
					cp = key->elemstr;
					if (mode == KEY_MODE_BEZTRIPLE) cp = elemstr;
					
					ofsp = ofs;
					
					while (cp[0]) {  /* (cp[0] == amount) */
						
						switch (cp[1]) {
							case IPO_FLOAT:
								rel_flerp(3, (float *)poin, (float *)reffrom, (float *)from, weight);
								break;
							case IPO_BPOINT:
								rel_flerp(4, (float *)poin, (float *)reffrom, (float *)from, weight);
								break;
							case IPO_BEZTRIPLE:
								rel_flerp(12, (float *)poin, (float *)reffrom, (float *)from, weight);
								break;
							default:
								/* should never happen */
								if (freefrom) MEM_freeN(freefrom);
								if (freereffrom) MEM_freeN(freereffrom);
								BLI_assert(!"invalid 'cp[1]'");
								return;
						}

						poin += *ofsp;
						
						cp += 2;
						ofsp++;
					}
					
					reffrom += elemsize;
					from += elemsize;
					
					if (mode == KEY_MODE_BEZTRIPLE) b += 2;
					if (weights) weights++;
				}

				if (freefrom) MEM_freeN(freefrom);
				if (freereffrom) MEM_freeN(freereffrom);
			}
		}
	}
}


static void do_key(const int start, int end, const int tot, char *poin, Key *key, KeyBlock *actkb, KeyBlock **k, float *t, const int mode)
{
	float k1tot = 0.0, k2tot = 0.0, k3tot = 0.0, k4tot = 0.0;
	float k1d = 0.0, k2d = 0.0, k3d = 0.0, k4d = 0.0;
	int a, ofs[32], *ofsp;
	int flagdo = 15, flagflo = 0, elemsize, poinsize = 0;
	char *k1, *k2, *k3, *k4, *freek1, *freek2, *freek3, *freek4;
	char *cp, elemstr[8];

	/* currently always 0, in future key_pointer_size may assign */
	ofs[1] = 0;

	if (!key_pointer_size(key, mode, &poinsize, &ofs[0]))
		return;
	
	if (end > tot) end = tot;

	k1 = key_block_get_data(key, actkb, k[0], &freek1);
	k2 = key_block_get_data(key, actkb, k[1], &freek2);
	k3 = key_block_get_data(key, actkb, k[2], &freek3);
	k4 = key_block_get_data(key, actkb, k[3], &freek4);

	/*  test for more or less points (per key!) */
	if (tot != k[0]->totelem) {
		k1tot = 0.0;
		flagflo |= 1;
		if (k[0]->totelem) {
			k1d = k[0]->totelem / (float)tot;
		}
		else {
			flagdo -= 1;
		}
	}
	if (tot != k[1]->totelem) {
		k2tot = 0.0;
		flagflo |= 2;
		if (k[0]->totelem) {
			k2d = k[1]->totelem / (float)tot;
		}
		else {
			flagdo -= 2;
		}
	}
	if (tot != k[2]->totelem) {
		k3tot = 0.0;
		flagflo |= 4;
		if (k[0]->totelem) {
			k3d = k[2]->totelem / (float)tot;
		}
		else {
			flagdo -= 4;
		}
	}
	if (tot != k[3]->totelem) {
		k4tot = 0.0;
		flagflo |= 8;
		if (k[0]->totelem) {
			k4d = k[3]->totelem / (float)tot;
		}
		else {
			flagdo -= 8;
		}
	}

	/* this exception is needed for curves with multiple splines */
	if (start != 0) {

		poin += poinsize * start;
		
		if (flagdo & 1) {
			if (flagflo & 1) {
				k1tot += start * k1d;
				a = (int)floor(k1tot);
				if (a) {
					k1tot -= a;
					k1 += a * key->elemsize;
				}
			}
			else {
				k1 += start * key->elemsize;
			}
		}
		if (flagdo & 2) {
			if (flagflo & 2) {
				k2tot += start * k2d;
				a = (int)floor(k2tot);
				if (a) {
					k2tot -= a;
					k2 += a * key->elemsize;
				}
			}
			else {
				k2 += start * key->elemsize;
			}
		}
		if (flagdo & 4) {
			if (flagflo & 4) {
				k3tot += start * k3d;
				a = (int)floor(k3tot);
				if (a) {
					k3tot -= a;
					k3 += a * key->elemsize;
				}
			}
			else {
				k3 += start * key->elemsize;
			}
		}
		if (flagdo & 8) {
			if (flagflo & 8) {
				k4tot += start * k4d;
				a = (int)floor(k4tot);
				if (a) {
					k4tot -= a;
					k4 += a * key->elemsize;
				}
			}
			else {
				k4 += start * key->elemsize;
			}
		}

	}

	/* in case of beztriple */
	elemstr[0] = 1;              /* nr of ipofloats */
	elemstr[1] = IPO_BEZTRIPLE;
	elemstr[2] = 0;

	/* only here, not above! */
	elemsize = key->elemsize;
	if (mode == KEY_MODE_BEZTRIPLE) elemsize *= 3;

	for (a = start; a < end; a++) {
	
		cp = key->elemstr;
		if (mode == KEY_MODE_BEZTRIPLE) cp = elemstr;
		
		ofsp = ofs;

		while (cp[0]) {  /* (cp[0] == amount) */

			switch (cp[1]) {
				case IPO_FLOAT:
					flerp(3, (float *)poin, (float *)k1, (float *)k2, (float *)k3, (float *)k4, t);
					break;
				case IPO_BPOINT:
					flerp(4, (float *)poin, (float *)k1, (float *)k2, (float *)k3, (float *)k4, t);
					break;
				case IPO_BEZTRIPLE:
					flerp(12, (void *)poin, (void *)k1, (void *)k2, (void *)k3, (void *)k4, t);
					break;
				default:
					/* should never happen */
					if (freek1) MEM_freeN(freek1);
					if (freek2) MEM_freeN(freek2);
					if (freek3) MEM_freeN(freek3);
					if (freek4) MEM_freeN(freek4);
					BLI_assert(!"invalid 'cp[1]'");
					return;
			}
			
			poin += *ofsp;
			cp += 2;
			ofsp++;
		}
		/* lets do it the difficult way: when keys have a different size */
		if (flagdo & 1) {
			if (flagflo & 1) {
				k1tot += k1d;
				while (k1tot >= 1.0f) {
					k1tot -= 1.0f;
					k1 += elemsize;
				}
			}
			else {
				k1 += elemsize;
			}
		}
		if (flagdo & 2) {
			if (flagflo & 2) {
				k2tot += k2d;
				while (k2tot >= 1.0f) {
					k2tot -= 1.0f;
					k2 += elemsize;
				}
			}
			else {
				k2 += elemsize;
			}
		}
		if (flagdo & 4) {
			if (flagflo & 4) {
				k3tot += k3d;
				while (k3tot >= 1.0f) {
					k3tot -= 1.0f;
					k3 += elemsize;
				}
			}
			else {
				k3 += elemsize;
			}
		}
		if (flagdo & 8) {
			if (flagflo & 8) {
				k4tot += k4d;
				while (k4tot >= 1.0f) {
					k4tot -= 1.0f;
					k4 += elemsize;
				}
			}
			else {
				k4 += elemsize;
			}
		}
		
		if (mode == KEY_MODE_BEZTRIPLE) a += 2;
	}

	if (freek1) MEM_freeN(freek1);
	if (freek2) MEM_freeN(freek2);
	if (freek3) MEM_freeN(freek3);
	if (freek4) MEM_freeN(freek4);
}

static float *get_weights_array(Object *ob, char *vgroup, WeightsArrayCache *cache)
{
	MDeformVert *dvert = NULL;
	BMEditMesh *em = NULL;
	BMIter iter;
	BMVert *eve;
	int totvert = 0, defgrp_index = 0;
	
	/* no vgroup string set? */
	if (vgroup[0] == 0) return NULL;
	
	/* gather dvert and totvert */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		dvert = me->dvert;
		totvert = me->totvert;

		if (me->edit_btmesh && me->edit_btmesh->bm->totvert == totvert)
			em = me->edit_btmesh;
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = ob->data;
		dvert = lt->dvert;
		totvert = lt->pntsu * lt->pntsv * lt->pntsw;
	}
	
	if (dvert == NULL) return NULL;
	
	/* find the group (weak loop-in-loop) */
	defgrp_index = defgroup_name_index(ob, vgroup);
	if (defgrp_index != -1) {
		float *weights;
		int i;

		if (cache) {
			if (cache->defgroup_weights == NULL) {
				int num_defgroup = BLI_listbase_count(&ob->defbase);
				cache->defgroup_weights =
				    MEM_callocN(sizeof(*cache->defgroup_weights) * num_defgroup,
				                "cached defgroup weights");
				cache->num_defgroup_weights = num_defgroup;
			}

			if (cache->defgroup_weights[defgrp_index]) {
				return cache->defgroup_weights[defgrp_index];
			}
		}

		weights = MEM_mallocN(totvert * sizeof(float), "weights");

		if (em) {
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
			BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				dvert = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
				weights[i] = defvert_find_weight(dvert, defgrp_index);
			}
		}
		else {
			for (i = 0; i < totvert; i++, dvert++) {
				weights[i] = defvert_find_weight(dvert, defgrp_index);
			}
		}

		if (cache) {
			cache->defgroup_weights[defgrp_index] = weights;
		}

		return weights;
	}
	return NULL;
}

float **BKE_keyblock_get_per_block_weights(Object *ob, Key *key, WeightsArrayCache *cache)
{
	KeyBlock *keyblock;
	float **per_keyblock_weights;
	int keyblock_index;

	per_keyblock_weights =
		MEM_mallocN(sizeof(*per_keyblock_weights) * key->totkey,
		            "per keyblock weights");

	for (keyblock = key->block.first, keyblock_index = 0;
	     keyblock;
	     keyblock = keyblock->next, keyblock_index++)
	{
		per_keyblock_weights[keyblock_index] = get_weights_array(ob, keyblock->vgroup, cache);
	}

	return per_keyblock_weights;
}

void BKE_keyblock_free_per_block_weights(Key *key, float **per_keyblock_weights, WeightsArrayCache *cache)
{
	int a;

	if (cache) {
		if (cache->num_defgroup_weights) {
			for (a = 0; a < cache->num_defgroup_weights; a++) {
				if (cache->defgroup_weights[a]) {
					MEM_freeN(cache->defgroup_weights[a]);
				}
			}
			MEM_freeN(cache->defgroup_weights);
		}
		cache->defgroup_weights = NULL;
	}
	else {
		for (a = 0; a < key->totkey; a++) {
			if (per_keyblock_weights[a]) {
				MEM_freeN(per_keyblock_weights[a]);
			}
		}
	}

	MEM_freeN(per_keyblock_weights);
}

static void do_mesh_key(Object *ob, Key *key, char *out, const int tot)
{
	KeyBlock *k[4], *actkb = BKE_keyblock_from_object(ob);
	float t[4];
	int flag = 0;

	if (key->type == KEY_RELATIVE) {
		WeightsArrayCache cache = {0, NULL};
		float **per_keyblock_weights;
		per_keyblock_weights = BKE_keyblock_get_per_block_weights(ob, key, &cache);
		BKE_key_evaluate_relative(0, tot, tot, (char *)out, key, actkb, per_keyblock_weights, KEY_MODE_DUMMY);
		BKE_keyblock_free_per_block_weights(key, per_keyblock_weights, &cache);
	}
	else {
		const float ctime_scaled = key->ctime / 100.0f;

		flag = setkeys(ctime_scaled, &key->block, k, t, 0);

		if (flag == 0) {
			do_key(0, tot, tot, (char *)out, key, actkb, k, t, KEY_MODE_DUMMY);
		}
		else {
			cp_key(0, tot, tot, (char *)out, key, actkb, k[2], NULL, KEY_MODE_DUMMY);
		}
	}
}

static void do_cu_key(Curve *cu, Key *key, KeyBlock *actkb, KeyBlock **k, float *t, char *out, const int tot)
{
	Nurb *nu;
	int a, step;
	
	for (a = 0, nu = cu->nurb.first; nu; nu = nu->next, a += step) {
		if (nu->bp) {
			step = nu->pntsu * nu->pntsv;
			do_key(a, a + step, tot, out, key, actkb, k, t, KEY_MODE_BPOINT);
		}
		else if (nu->bezt) {
			step = 3 * nu->pntsu;
			do_key(a, a + step, tot, out, key, actkb, k, t, KEY_MODE_BEZTRIPLE);
		}
		else {
			step = 0;
		}
	}
}

static void do_rel_cu_key(Curve *cu, Key *key, KeyBlock *actkb, char *out, const int tot)
{
	Nurb *nu;
	int a, step;
	
	for (a = 0, nu = cu->nurb.first; nu; nu = nu->next, a += step) {
		if (nu->bp) {
			step = nu->pntsu * nu->pntsv;
			BKE_key_evaluate_relative(a, a + step, tot, out, key, actkb, NULL, KEY_MODE_BPOINT);
		}
		else if (nu->bezt) {
			step = 3 * nu->pntsu;
			BKE_key_evaluate_relative(a, a + step, tot, out, key, actkb, NULL, KEY_MODE_BEZTRIPLE);
		}
		else {
			step = 0;
		}
	}
}

static void do_curve_key(Object *ob, Key *key, char *out, const int tot)
{
	Curve *cu = ob->data;
	KeyBlock *k[4], *actkb = BKE_keyblock_from_object(ob);
	float t[4];
	int flag = 0;

	if (key->type == KEY_RELATIVE) {
		do_rel_cu_key(cu, cu->key, actkb, out, tot);
	}
	else {
		const float ctime_scaled = key->ctime / 100.0f;

		flag = setkeys(ctime_scaled, &key->block, k, t, 0);

		if (flag == 0) {
			do_cu_key(cu, key, actkb, k, t, out, tot);
		}
		else {
			cp_cu_key(cu, key, actkb, k[2], 0, tot, out, tot);
		}
	}
}

static void do_latt_key(Object *ob, Key *key, char *out, const int tot)
{
	Lattice *lt = ob->data;
	KeyBlock *k[4], *actkb = BKE_keyblock_from_object(ob);
	float t[4];
	int flag;
	
	if (key->type == KEY_RELATIVE) {
		float **per_keyblock_weights;
		per_keyblock_weights = BKE_keyblock_get_per_block_weights(ob, key, NULL);
		BKE_key_evaluate_relative(0, tot, tot, (char *)out, key, actkb, per_keyblock_weights, KEY_MODE_DUMMY);
		BKE_keyblock_free_per_block_weights(key, per_keyblock_weights, NULL);
	}
	else {
		const float ctime_scaled = key->ctime / 100.0f;
		
		flag = setkeys(ctime_scaled, &key->block, k, t, 0);

		if (flag == 0) {
			do_key(0, tot, tot, (char *)out, key, actkb, k, t, KEY_MODE_DUMMY);
		}
		else {
			cp_key(0, tot, tot, (char *)out, key, actkb, k[2], NULL, KEY_MODE_DUMMY);
		}
	}

	if (lt->flag & LT_OUTSIDE) outside_lattice(lt);
}

/* returns key coordinates (+ tilt) when key applied, NULL otherwise */
float *BKE_key_evaluate_object_ex(
        Object *ob, int *r_totelem,
        float *arr, size_t arr_size)
{
	Key *key = BKE_key_from_object(ob);
	KeyBlock *actkb = BKE_keyblock_from_object(ob);
	char *out;
	int tot = 0, size = 0;

	if (key == NULL || BLI_listbase_is_empty(&key->block))
		return NULL;

	/* compute size of output array */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		tot = me->totvert;
		size = tot * 3 * sizeof(float);
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = ob->data;

		tot = lt->pntsu * lt->pntsv * lt->pntsw;
		size = tot * 3 * sizeof(float);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		Nurb *nu;

		for (nu = cu->nurb.first; nu; nu = nu->next) {
			if (nu->bezt) {
				tot += 3 * nu->pntsu;
				size += nu->pntsu * 12 * sizeof(float);
			}
			else if (nu->bp) {
				tot += nu->pntsu * nu->pntsv;
				size += nu->pntsu * nu->pntsv * 12 * sizeof(float);
			}
		}
	}

	/* if nothing to interpolate, cancel */
	if (tot == 0 || size == 0)
		return NULL;
	
	/* allocate array */
	if (arr == NULL) {
		out = MEM_callocN(size, "BKE_key_evaluate_object out");
	}
	else {
		if (arr_size != size) {
			return NULL;
		}

		out = (char *)arr;
	}

	/* prevent python from screwing this up? anyhoo, the from pointer could be dropped */
	key->from = (ID *)ob->data;
		
	if (ob->shapeflag & OB_SHAPE_LOCK) {
		/* shape locked, copy the locked shape instead of blending */
		KeyBlock *kb = BLI_findlink(&key->block, ob->shapenr - 1);
		
		if (kb && (kb->flag & KEYBLOCK_MUTE))
			kb = key->refkey;

		if (kb == NULL) {
			kb = key->block.first;
			ob->shapenr = 1;
		}
		
		if (OB_TYPE_SUPPORT_VGROUP(ob->type)) {
			float *weights = get_weights_array(ob, kb->vgroup, NULL);

			cp_key(0, tot, tot, out, key, actkb, kb, weights, 0);

			if (weights) MEM_freeN(weights);
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF))
			cp_cu_key(ob->data, key, actkb, kb, 0, tot, out, tot);
	}
	else {
		
		if (ob->type == OB_MESH) do_mesh_key(ob, key, out, tot);
		else if (ob->type == OB_LATTICE) do_latt_key(ob, key, out, tot);
		else if (ob->type == OB_CURVE) do_curve_key(ob, key, out, tot);
		else if (ob->type == OB_SURF) do_curve_key(ob, key, out, tot);
	}
	
	if (r_totelem) {
		*r_totelem = tot;
	}
	return (float *)out;
}

float *BKE_key_evaluate_object(Object *ob, int *r_totelem)
{
	return BKE_key_evaluate_object_ex(ob, r_totelem, NULL, 0);
}

Key **BKE_key_from_id_p(ID *id)
{
	switch (GS(id->name)) {
		case ID_ME:
		{
			Mesh *me = (Mesh *)id;
			return &me->key;
		}
		case ID_CU:
		{
			Curve *cu = (Curve *)id;
			if (cu->vfont == NULL) {
				return &cu->key;
			}
			break;
		}
		case ID_LT:
		{
			Lattice *lt = (Lattice *)id;
			return &lt->key;
		}
	}

	return NULL;
}

Key *BKE_key_from_id(ID *id)
{
	Key **key_p;
	key_p = BKE_key_from_id_p(id);
	if (key_p) {
		return *key_p;
	}

	return NULL;
}

Key **BKE_key_from_object_p(Object *ob)
{
	if (ob == NULL || ob->data == NULL)
		return NULL;

	return BKE_key_from_id_p(ob->data);
}

Key *BKE_key_from_object(Object *ob)
{
	Key **key_p;
	key_p = BKE_key_from_object_p(ob);
	if (key_p) {
		return *key_p;
	}

	return NULL;
}

KeyBlock *BKE_keyblock_add(Key *key, const char *name)
{
	KeyBlock *kb;
	float curpos = -0.1;
	int tot;
	
	kb = key->block.last;
	if (kb) curpos = kb->pos;
	
	kb = MEM_callocN(sizeof(KeyBlock), "Keyblock");
	BLI_addtail(&key->block, kb);
	kb->type = KEY_CARDINAL;
	
	tot = BLI_listbase_count(&key->block);
	if (name) {
		BLI_strncpy(kb->name, name, sizeof(kb->name));
	}
	else {
		if (tot == 1)
			BLI_strncpy(kb->name, DATA_("Basis"), sizeof(kb->name));
		else
			BLI_snprintf(kb->name, sizeof(kb->name), DATA_("Key %d"), tot - 1);
	}

	BLI_uniquename(&key->block, kb, DATA_("Key"), '.', offsetof(KeyBlock, name), sizeof(kb->name));

	kb->uid = key->uidgen++;

	key->totkey++;
	if (key->totkey == 1) key->refkey = kb;
	
	kb->slidermin = 0.0f;
	kb->slidermax = 1.0f;

	/**
	 * \note caller may want to set this to current time, but don't do it here since we need to sort
	 * which could cause problems in some cases, see #BKE_keyblock_add_ctime */
	kb->pos = curpos + 0.1f; /* only used for absolute shape keys */

	return kb;
}

/**
 * \note sorting is a problematic side effect in some cases,
 * better only do this explicitly by having its own function,
 *
 * \param key The key datablock to add to.
 * \param name Optional name for the new keyblock.
 * \param do_force always use ctime even for relative keys.
 */
KeyBlock *BKE_keyblock_add_ctime(Key *key, const char *name, const bool do_force)
{
	KeyBlock *kb = BKE_keyblock_add(key, name);
	const float cpos = key->ctime / 100.0f;

	/* In case of absolute keys, there is no point in adding more than one key with the same pos.
	 * Hence only set new keybloc pos to current time if none previous one already use it.
	 * Now at least people just adding absolute keys without touching to ctime
	 * won't have to systematically use retiming func (and have ordering issues, too). See T39897.
	 */
	if (!do_force && (key->type != KEY_RELATIVE)) {
		KeyBlock *it_kb;
		for (it_kb = key->block.first; it_kb; it_kb = it_kb->next) {
			if (it_kb->pos == cpos) {
				return kb;
			}
		}
	}
	if (do_force || (key->type != KEY_RELATIVE)) {
		kb->pos = cpos;
		BKE_key_sort(key);
	}

	return kb;
}

/* only the active keyblock */
KeyBlock *BKE_keyblock_from_object(Object *ob) 
{
	Key *key = BKE_key_from_object(ob);
	
	if (key) {
		KeyBlock *kb = BLI_findlink(&key->block, ob->shapenr - 1);
		return kb;
	}

	return NULL;
}

KeyBlock *BKE_keyblock_from_object_reference(Object *ob)
{
	Key *key = BKE_key_from_object(ob);
	
	if (key)
		return key->refkey;

	return NULL;
}

/* get the appropriate KeyBlock given an index */
KeyBlock *BKE_keyblock_from_key(Key *key, int index)
{
	KeyBlock *kb;
	int i;
	
	if (key) {
		kb = key->block.first;
		
		for (i = 1; i < key->totkey; i++) {
			kb = kb->next;
			
			if (index == i)
				return kb;
		}
	}
	
	return NULL;
}

/* get the appropriate KeyBlock given a name to search for */
KeyBlock *BKE_keyblock_find_name(Key *key, const char name[])
{
	return BLI_findstring(&key->block, name, offsetof(KeyBlock, name));
}

/**
 * \brief copy shape-key attributes, but not key data.or name/uid
 */
void BKE_keyblock_copy_settings(KeyBlock *kb_dst, const KeyBlock *kb_src)
{
	kb_dst->pos        = kb_src->pos;
	kb_dst->curval     = kb_src->curval;
	kb_dst->type       = kb_src->type;
	kb_dst->relative   = kb_src->relative;
	BLI_strncpy(kb_dst->vgroup, kb_src->vgroup, sizeof(kb_dst->vgroup));
	kb_dst->slidermin  = kb_src->slidermin;
	kb_dst->slidermax  = kb_src->slidermax;
}

/* Get RNA-Path for 'value' setting of the given ShapeKey 
 * NOTE: the user needs to free the returned string once they're finish with it
 */
char *BKE_keyblock_curval_rnapath_get(Key *key, KeyBlock *kb)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	
	/* sanity checks */
	if (ELEM(NULL, key, kb))
		return NULL;
	
	/* create the RNA pointer */
	RNA_pointer_create(&key->id, &RNA_ShapeKey, kb, &ptr);
	/* get pointer to the property too */
	prop = RNA_struct_find_property(&ptr, "value");
	
	/* return the path */
	return RNA_path_from_ID_to_property(&ptr, prop);
}


/* conversion functions */

/************************* Lattice ************************/
void BKE_keyblock_update_from_lattice(Lattice *lt, KeyBlock *kb)
{
	BPoint *bp;
	float (*fp)[3];
	int a, tot;

	BLI_assert(kb->totelem == lt->pntsu * lt->pntsv * lt->pntsw);

	tot = kb->totelem;
	if (tot == 0) return;

	bp = lt->def;
	fp = kb->data;
	for (a = 0; a < kb->totelem; a++, fp++, bp++) {
		copy_v3_v3(*fp, bp->vec);
	}
}

void BKE_keyblock_convert_from_lattice(Lattice *lt, KeyBlock *kb)
{
	int tot;

	tot = lt->pntsu * lt->pntsv * lt->pntsw;
	if (tot == 0) return;

	MEM_SAFE_FREE(kb->data);

	kb->data = MEM_mallocN(lt->key->elemsize * tot, __func__);
	kb->totelem = tot;

	BKE_keyblock_update_from_lattice(lt, kb);
}

void BKE_keyblock_convert_to_lattice(KeyBlock *kb, Lattice *lt)
{
	BPoint *bp;
	const float (*fp)[3];
	int a, tot;

	bp = lt->def;
	fp = kb->data;

	tot = lt->pntsu * lt->pntsv * lt->pntsw;
	tot = min_ii(kb->totelem, tot);

	for (a = 0; a < tot; a++, fp++, bp++) {
		copy_v3_v3(bp->vec, *fp);
	}
}

/************************* Curve ************************/
void BKE_keyblock_update_from_curve(Curve *UNUSED(cu), KeyBlock *kb, ListBase *nurb)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float *fp;
	int a, tot;

	/* count */
	BLI_assert(BKE_nurbList_verts_count(nurb) == kb->totelem);

	tot = kb->totelem;
	if (tot == 0) return;

	fp = kb->data;
	for (nu = nurb->first; nu; nu = nu->next) {
		if (nu->bezt) {
			for (a = nu->pntsu, bezt = nu->bezt; a; a--, bezt++) {
				int i;

				for (i = 0; i < 3; i++, fp += 3) {
					copy_v3_v3(fp, bezt->vec[i]);
				}
				fp[0] = bezt->alfa;
				fp += 3; /* alphas */
			}
		}
		else {
			
			;
			for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a; a--, fp += 4, bp++) {
				copy_v3_v3(fp, bp->vec);
				fp[3] = bp->alfa;
			}
		}
	}
}

void BKE_keyblock_convert_from_curve(Curve *cu, KeyBlock *kb, ListBase *nurb)
{
	int tot;

	/* count */
	tot = BKE_nurbList_verts_count(nurb);
	if (tot == 0) return;

	MEM_SAFE_FREE(kb->data);

	kb->data = MEM_mallocN(cu->key->elemsize * tot, __func__);
	kb->totelem = tot;

	BKE_keyblock_update_from_curve(cu, kb, nurb);
}

void BKE_keyblock_convert_to_curve(KeyBlock *kb, Curve *UNUSED(cu), ListBase *nurb)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	const float *fp;
	int a, tot;

	tot = BKE_nurbList_verts_count(nurb);
	tot = min_ii(kb->totelem, tot);

	fp = kb->data;
	for (nu = nurb->first; nu && tot > 0; nu = nu->next) {
		if (nu->bezt) {
			for (a = nu->pntsu, bezt = nu->bezt; a && tot > 0; a--, tot -= 3, bezt++) {
				int i;

				for (i = 0; i < 3; i++, fp += 3) {
					copy_v3_v3(bezt->vec[i], fp);
				}
				bezt->alfa = fp[0];
				fp += 3; /* alphas */
			}
		}
		else {
			for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a && tot; a--, tot--, fp += 4, bp++) {
				copy_v3_v3(bp->vec, fp);
				bp->alfa = fp[3];
			}
		}
	}
}

/************************* Mesh ************************/
void BKE_keyblock_update_from_mesh(Mesh *me, KeyBlock *kb)
{
	MVert *mvert;
	float (*fp)[3];
	int a, tot;

	BLI_assert(me->totvert == kb->totelem);

	tot = me->totvert;
	if (tot == 0) return;

	mvert = me->mvert;
	fp = kb->data;
	for (a = 0; a < tot; a++, fp++, mvert++) {
		copy_v3_v3(*fp, mvert->co);
	}
}

void BKE_keyblock_convert_from_mesh(Mesh *me, KeyBlock *kb)
{
	int tot = me->totvert;

	if (me->totvert == 0) return;

	MEM_SAFE_FREE(kb->data);

	kb->data = MEM_mallocN(me->key->elemsize * tot, __func__);
	kb->totelem = tot;

	BKE_keyblock_update_from_mesh(me, kb);
}

void BKE_keyblock_convert_to_mesh(KeyBlock *kb, Mesh *me)
{
	MVert *mvert;
	const float (*fp)[3];
	int a, tot;

	mvert = me->mvert;
	fp = kb->data;

	tot = min_ii(kb->totelem, me->totvert);

	for (a = 0; a < tot; a++, fp++, mvert++) {
		copy_v3_v3(mvert->co, *fp);
	}
}

/**
 * Computes normals (vertices, polygons and/or loops ones) of given mesh for given shape key.
 *
 * \param kb the KeyBlock to use to compute normals.
 * \param mesh the Mesh to apply keyblock to.
 * \param r_vertnors if non-NULL, an array of vectors, same length as number of vertices.
 * \param r_polynors if non-NULL, an array of vectors, same length as number of polygons.
 * \param r_loopnors if non-NULL, an array of vectors, same length as number of loops.
 */
void BKE_keyblock_mesh_calc_normals(
        struct KeyBlock *kb, struct Mesh *mesh,
        float (*r_vertnors)[3], float (*r_polynors)[3], float (*r_loopnors)[3])
{
	/* We use a temp, shallow copy of mesh to work. */
	Mesh me;
	bool free_polynors = false;

	if (r_vertnors == NULL && r_polynors == NULL && r_loopnors == NULL) {
		return;
	}

	me = *mesh;
	me.mvert = MEM_dupallocN(mesh->mvert);
	CustomData_reset(&me.vdata);
	CustomData_reset(&me.edata);
	CustomData_reset(&me.pdata);
	CustomData_reset(&me.ldata);
	CustomData_reset(&me.fdata);

	BKE_keyblock_convert_to_mesh(kb, &me);

	if (r_polynors == NULL && r_loopnors != NULL) {
		r_polynors = MEM_mallocN(sizeof(float[3]) * me.totpoly, __func__);
		free_polynors = true;
	}
	BKE_mesh_calc_normals_poly(
	            me.mvert, r_vertnors, me.totvert, me.mloop, me.mpoly, me.totloop, me.totpoly, r_polynors, false);

	if (r_loopnors) {
		short (*clnors)[2] = CustomData_get_layer(&mesh->ldata, CD_CUSTOMLOOPNORMAL);  /* May be NULL. */

		BKE_mesh_normals_loop_split(
		        me.mvert, me.totvert, me.medge, me.totedge,
		        me.mloop, r_loopnors, me.totloop, me.mpoly, r_polynors, me.totpoly,
		        (me.flag & ME_AUTOSMOOTH) != 0, me.smoothresh, NULL, clnors, NULL);
	}

	CustomData_free(&me.vdata, me.totvert);
	CustomData_free(&me.edata, me.totedge);
	CustomData_free(&me.pdata, me.totpoly);
	CustomData_free(&me.ldata, me.totloop);
	CustomData_free(&me.fdata, me.totface);
	MEM_freeN(me.mvert);

	if (free_polynors) {
		MEM_freeN(r_polynors);
	}
}


/************************* raw coords ************************/
void BKE_keyblock_update_from_vertcos(Object *ob, KeyBlock *kb, float (*vertCos)[3])
{
	float (*co)[3] = vertCos;
	float *fp = kb->data;
	int tot, a;

#ifndef NDEBUG
	if (ob->type == OB_LATTICE) {
		Lattice *lt = ob->data;
		BLI_assert((lt->pntsu * lt->pntsv * lt->pntsw) == kb->totelem);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu = ob->data;
		BLI_assert(BKE_nurbList_verts_count(&cu->nurb) == kb->totelem);
	}
	else if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		BLI_assert(me->totvert == kb->totelem);
	}
	else {
		BLI_assert(0 == kb->totelem);
	}
#endif

	tot = kb->totelem;
	if (tot == 0) return;

	/* Copy coords to keyblock */
	if (ELEM(ob->type, OB_MESH, OB_LATTICE)) {
		for (a = 0; a < tot; a++, fp += 3, co++) {
			copy_v3_v3(fp, *co);
		}
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu = (Curve *)ob->data;
		Nurb *nu;
		BezTriple *bezt;
		BPoint *bp;

		for (nu = cu->nurb.first; nu; nu = nu->next) {
			if (nu->bezt) {
				for (a = nu->pntsu, bezt = nu->bezt; a; a--, bezt++) {
					int i;

					for (i = 0; i < 3; i++, fp += 3, co++) {
						copy_v3_v3(fp, *co);
					}
					fp += 3; /* skip alphas */
				}
			}
			else {
				for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a; a--, bp++, fp += 4, co++) {
					copy_v3_v3(fp, *co);
				}
			}
		}
	}
}

void BKE_keyblock_convert_from_vertcos(Object *ob, KeyBlock *kb, float (*vertCos)[3])
{
	int tot = 0, elemsize;

	MEM_SAFE_FREE(kb->data);

	/* Count of vertex coords in array */
	if (ob->type == OB_MESH) {
		Mesh *me = (Mesh *)ob->data;
		tot = me->totvert;
		elemsize = me->key->elemsize;
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = (Lattice *)ob->data;
		tot = lt->pntsu * lt->pntsv * lt->pntsw;
		elemsize = lt->key->elemsize;
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu = (Curve *)ob->data;
		elemsize = cu->key->elemsize;
		tot = BKE_nurbList_verts_count(&cu->nurb);
	}

	if (tot == 0) return;

	kb->data = MEM_mallocN(tot * elemsize, __func__);

	/* Copy coords to keyblock */
	BKE_keyblock_update_from_vertcos(ob, kb, vertCos);
}

float (*BKE_keyblock_convert_to_vertcos(Object *ob, KeyBlock *kb))[3]
{
	float (*vertCos)[3], (*co)[3];
	const float *fp = kb->data;
	int tot = 0, a;

	/* Count of vertex coords in array */
	if (ob->type == OB_MESH) {
		Mesh *me = (Mesh *)ob->data;
		tot = me->totvert;
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = (Lattice *)ob->data;
		tot = lt->pntsu * lt->pntsv * lt->pntsw;
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu = (Curve *)ob->data;
		tot = BKE_nurbList_verts_count(&cu->nurb);
	}

	if (tot == 0) return NULL;

	co = vertCos = MEM_mallocN(tot * sizeof(*vertCos), __func__);

	/* Copy coords to array */
	if (ELEM(ob->type, OB_MESH, OB_LATTICE)) {
		for (a = 0; a < tot; a++, fp += 3, co++) {
			copy_v3_v3(*co, fp);
		}
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu = (Curve *)ob->data;
		Nurb *nu;
		BezTriple *bezt;
		BPoint *bp;

		for (nu = cu->nurb.first; nu; nu = nu->next) {
			if (nu->bezt) {
				for (a = nu->pntsu, bezt = nu->bezt; a; a--, bezt++) {
					int i;

					for (i = 0; i < 3; i++, fp += 3, co++) {
						copy_v3_v3(*co, fp);
					}
					fp += 3; /* skip alphas */
				}
			}
			else {
				for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a; a--, bp++, fp += 4, co++) {
					copy_v3_v3(*co, fp);
				}
			}
		}
	}

	return vertCos;
}

/************************* raw coord offsets ************************/
void BKE_keyblock_update_from_offset(Object *ob, KeyBlock *kb, float (*ofs)[3])
{
	int a;
	float *fp = kb->data;

	if (ELEM(ob->type, OB_MESH, OB_LATTICE)) {
		for (a = 0; a < kb->totelem; a++, fp += 3, ofs++) {
			add_v3_v3(fp, *ofs);
		}
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu = (Curve *)ob->data;
		Nurb *nu;
		BezTriple *bezt;
		BPoint *bp;

		for (nu = cu->nurb.first; nu; nu = nu->next) {
			if (nu->bezt) {
				for (a = nu->pntsu, bezt = nu->bezt; a; a--, bezt++) {
					int i;

					for (i = 0; i < 3; i++, fp += 3, ofs++) {
						add_v3_v3(fp, *ofs);
					}
					fp += 3; /* skip alphas */
				}
			}
			else {
				for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a; a--, bp++, fp += 4, ofs++) {
					add_v3_v3(fp, *ofs);
				}
			}
		}
	}
}

/* ==========================================================*/

/** Move shape key from org_index to new_index. Safe, clamps index to valid range, updates reference keys,
 * the object's active shape index, the 'frame' value in case of absolute keys, etc.
 * Note indices are expected in real values (not 'fake' shapenr +1 ones).
 *
 * \param org_index if < 0, current object's active shape will be used as skey to move.
 * \return true if something was done, else false.
 */
bool BKE_keyblock_move(Object *ob, int org_index, int new_index)
{
	Key *key = BKE_key_from_object(ob);
	KeyBlock *kb;
	const int act_index = ob->shapenr - 1;
	const int totkey = key->totkey;
	int i;
	bool rev, in_range = false;

	if (org_index < 0) {
		org_index = act_index;
	}

	CLAMP(new_index, 0, key->totkey - 1);
	CLAMP(org_index, 0, key->totkey - 1);

	if (new_index == org_index) {
		return false;
	}

	rev = ((new_index - org_index) < 0) ? true : false;

	/* We swap 'org' element with its previous/next neighbor (depending on direction of the move) repeatedly,
	 * until we reach final position.
	 * This allows us to only loop on the list once! */
	for (kb = (rev ? key->block.last : key->block.first), i = (rev ? totkey - 1 : 0);
	     kb;
	     kb = (rev ? kb->prev : kb->next), rev ? i-- : i++)
	{
		if (i == org_index) {
			in_range = true;  /* Start list items swapping... */
		}
		else if (i == new_index) {
			in_range = false;  /* End list items swapping. */
		}

		if (in_range) {
			KeyBlock *other_kb = rev ? kb->prev : kb->next;

			/* Swap with previous/next list item. */
			BLI_listbase_swaplinks(&key->block, kb, other_kb);

			/* Swap absolute positions. */
			SWAP(float, kb->pos, other_kb->pos);

			kb = other_kb;
		}

		/* Adjust relative indices, this has to be done on the whole list! */
		if (kb->relative == org_index) {
			kb->relative = new_index;
		}
		else if (kb->relative < org_index && kb->relative >= new_index) {
			/* remove after, insert before this index */
			kb->relative++;
		}
		else if (kb->relative > org_index && kb->relative <= new_index) {
			/* remove before, insert after this index */
			kb->relative--;
		}
	}

	/* Need to update active shape number if it's affected, same principle as for relative indices above. */
	if (org_index == act_index) {
		ob->shapenr = new_index + 1;
	}
	else if (act_index < org_index && act_index >= new_index) {
		ob->shapenr++;
	}
	else if (act_index > org_index && act_index <= new_index) {
		ob->shapenr--;
	}

	/* First key is always refkey, matches interface and BKE_key_sort */
	key->refkey = key->block.first;

	return true;
}

/**
 * Check if given keyblock (as index) is used as basis by others in given key.
 */
bool BKE_keyblock_is_basis(Key *key, const int index)
{
	KeyBlock *kb;
	int i;

	if (key->type == KEY_RELATIVE) {
		for (i = 0, kb = key->block.first; kb; i++, kb = kb->next) {
			if ((i != index) && (kb->relative == index)) {
				return true;
			}
		}
	}

	return false;
}
