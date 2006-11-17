#ifndef _BKE_IDPROP_H
#define _BKE_IDPROP_H

#include "DNA_ID.h"

/*
these two are included for their (new :P )function
pointers.
*/
#include "BLO_readfile.h"
#include "BLO_writefile.h"

struct WriteData;
struct FileData;

struct IDProperty;
struct ID;

typedef union {
	int i;
	float f;
	char *str;
	struct ID *id;
	struct {
		short type;
		short len;
	} array;
	struct {
		int matvec_size;
		float *example;
	} matrix_or_vector;
} IDPropertyTemplate;

/* ----------- Array Type ----------- */
/*this function works for strings too!*/
void IDP_ResizeArray(struct IDProperty *prop, int newlen);
void IDP_FreeArray(struct IDProperty *prop);
void IDP_UnlinkArray(struct IDProperty *prop);

/* ---------- String Type ------------ */
void IDP_AssignString(struct IDProperty *prop, char *st);
void IDP_ConcatStringC(struct IDProperty *prop, char *st);
void IDP_ConcatString(struct IDProperty *str1, struct IDProperty *append);
void IDP_FreeString(struct IDProperty *prop);

/*-------- ID Type -------*/
void IDP_LinkID(struct IDProperty *prop, ID *id);
void IDP_UnlinkID(struct IDProperty *prop);

/*-------- Group Functions -------*/
void IDP_AddToGroup(struct IDProperty *group, struct IDProperty *prop);
void IDP_RemFromGroup(struct IDProperty *group, struct IDProperty *prop);
IDProperty *IDP_GetPropertyFromGroup(struct IDProperty *prop, char *name);

/*Get an iterator to iterate over the members of an id property group.
 Note that this will automatically free the iterator once iteration is complete;
 if you stop the iteration before hitting the end, make sure to call
 IDP_FreeIterBeforeEnd().*/
void *IDP_GetGroupIterator(struct IDProperty *prop);

/*Returns the next item in the iteration.  To use, simple for a loop like the following:
 while (IDP_GroupIterNext(iter) != NULL) {
	. . .
 }*/
void *IDP_GroupIterNext(void *vself);

/*Frees the iterator pointed to at vself, only use this if iteration is stopped early; 
  when the iterator hits the end of the list it'll automatially free itself.*/
void IDP_FreeIterBeforeEnd(void *vself);

/*-------- Main Functions --------*/
/*Get the Group property that contains the id properties for ID id.  Set create_if_needed
  to create the Group property and attach it to id if it doesn't exist; otherwise
  the function will return NULL if there's no Group property attached to the ID.*/
struct IDProperty *IDP_GetProperties(struct ID *id, int create_if_needed);

/*
Allocate a new ID.

This function takes three arguments: the ID property type, a union which defines
it's initial value, and a name.

The union is simple to use; see the top of this header file for its definition. 
An example of using this function:

 IDPropertyTemplate val;
 IDProperty *group, *idgroup, *color;
 group = IDP_New(IDP_GROUP, val, "group1"); //groups don't need a template.

 val.array.len = 4
 val.array.type = IDP_FLOAT;
 color = IDP_New(IDP_ARRAY, val, "color1");

 idgroup = IDP_GetProperties(some_id, 1);
 IDP_AddToGroup(idgroup, color);
 IDP_AddToGroup(idgroup, group);

Note that you MUST either attach the id property to an id property group with 
IDP_AddToGroup or MEM_freeN the property, doing anything else might result in
a memory leak.
*/
struct IDProperty *IDP_New(int type, IDPropertyTemplate val, char *name);
\
/*NOTE: this will free all child properties of list arrays and groups!
  Also, note that this does NOT unlink anything!  Plus it doesn't free
  the actual struct IDProperty struct either.*/
void IDP_FreeProperty(struct IDProperty *prop);

/*Unlinks any struct IDProperty<->ID linkage that might be going on.*/
void IDP_UnlinkProperty(struct IDProperty *prop);

#endif /* _BKE_IDPROP_H */
