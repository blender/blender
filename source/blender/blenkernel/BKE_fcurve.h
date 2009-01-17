/* Testing code for new animation system in 2.5 
 * Copyright 2009, Joshua Leung
 */

#ifndef BKE_FCURVE_H
#define BKE_FCURVE_H

//struct ListBase;
struct FCurve;
struct FModifier;
struct ChannelDriver;
struct BezTriple;

/* ************** Keyframe Tools ***************** */

// XXX this stuff is defined in BKE_ipo.h too, so maybe skip for now?
typedef struct CfraElem {
	struct CfraElem *next, *prev;
	float cfra;
	int sel;
} CfraElem;

void bezt_add_to_cfra_elem(ListBase *lb, struct BezTriple *bezt);

/* ************** F-Curve Drivers ***************** */

void fcurve_free_driver(struct FCurve *fcu);

/* ************** F-Curve Modifiers *************** */

/* F-Curve Modifier Type-Info (fmi):
 *  This struct provides function pointers for runtime, so that functions can be
 *  written more generally (with fewer/no special exceptions for various modifiers).
 *
 *  Callers of these functions must check that they actually point to something useful,
 *  as some constraints don't define some of these.
 *
 *  Warning: it is not too advisable to reorder order of members of this struct,
 *			as you'll have to edit quite a few ($FMODIFIER_NUM_TYPES) of these
 *			structs.
 */
typedef struct FModifierTypeInfo {
	/* admin/ident */
	short type;				/* FMODIFIER_TYPE_### */
	short size;				/* size in bytes of the struct */
	char name[32]; 			/* name of modifier in interface */
	char structName[32];	/* name of struct for SDNA */
	
	/* data management function pointers - special handling */
		/* free any data that is allocated separately (optional) */
	void (*free_data)(struct FModifier *fcm);
		/* copy any special data that is allocated separately (optional) */
	void (*copy_data)(struct FModifier *fcm, struct FModifier *src);
		/* set settings for data that will be used for FCuModifier.data (memory already allocated using MEM_callocN) */
	void (*new_data)(void *mdata);
	
	/* evaluation */
		/* evaluate the modifier for the given time and 'accumulated' value */
	void (*evaluate_modifier)(struct FCurve *fcu, struct FModifier *fcm, float *cvalue, float evaltime);
} FModifierTypeInfo;

/* Function Prototypes for FModifierTypeInfo's */
FModifierTypeInfo *fmodifier_get_typeinfo(struct FModifier *fcm);
FModifierTypeInfo *get_fmodifier_typeinfo(int type);

/* ---------------------- */

// TODO... general API here..
struct FModifier *fcurve_add_modifier(struct FCurve *fcu, int type);
void fcurve_copy_modifiers(ListBase *dst, ListBase *src);
void fcurve_remove_modifier(struct FCurve *fcu, struct FModifier *fcm);
void fcurve_free_modifiers(struct FCurve *fcu);
void fcurve_bake_modifiers(struct FCurve *fcu, int start, int end);

/* ************** F-Curves API ******************** */

/* -------- Data Managemnt  --------  */

void free_fcurve(struct FCurve *fcu);
struct FCurve *copy_fcurve(struct FCurve *fcu);

/* find matching F-Curve in the given list of F-Curves */
struct FCurve *list_find_fcurve(ListBase *list, const char rna_path[], const int array_index);

/* -------- Curve Sanity --------  */

void calchandles_fcurve(struct FCurve *fcu);
void testhandles_fcurve(struct FCurve *fcu);
void sort_time_fcurve(struct FCurve *fcu);
short test_time_fcurve(struct FCurve *fcu);

/* -------- Evaluation --------  */

/* evaluate fcurve */
float evaluate_fcurve(struct FCurve *fcu, float evaltime);
/* evaluate fcurve and store value */
void calculate_fcurve(struct FCurve *fcu, float ctime);


#endif /* BKE_FCURVE_H*/
