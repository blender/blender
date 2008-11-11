
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_types.h"

typedef struct RNAGenDeps {
	void *udata;
	PropDependencyCallback cb;
} RNAGenDeps;

static void rna_generate_deps(RNAGenDeps *gen, PointerRNA *ptr, PointerRNA *idptr)
{
	PropertyRNA *prop;
	PointerRNA pptr;
	CollectionPropertyIterator iter;

	/* traverse recursively into ID struct properties, other
	 * pointers we potentially add as dependencies */

	for(prop=ptr->type->properties.first; prop; prop=prop->next) {
		if(prop->type == PROP_POINTER) {
			RNA_property_pointer_get(prop, ptr, &pptr);

			if(pptr.data && pptr.type) {
				if(idptr && (pptr.type->flag & STRUCT_ID)) {
					if(prop->flag & PROP_EVALUATE_DEPENDENCY)
						gen->cb(gen->udata, ptr, &pptr);
					else if(prop->flag & PROP_INVERSE_EVALUATE_DEPENDENCY)
						gen->cb(gen->udata, ptr, &pptr);
				}
				else
					rna_generate_deps(gen, &pptr, (idptr)? idptr: &pptr);
			}
		}
		else if(prop->type == PROP_COLLECTION) {
			RNA_property_collection_begin(prop, &iter, ptr);

			while(iter.valid) {
				RNA_property_collection_get(prop, &iter, &pptr);

				if(pptr.data && pptr.type) {
					if(idptr && (pptr.type->flag & STRUCT_ID)) {
						if(prop->flag & PROP_EVALUATE_DEPENDENCY)
							gen->cb(gen->udata, ptr, &pptr);
						else if(prop->flag & PROP_INVERSE_EVALUATE_DEPENDENCY)
							gen->cb(gen->udata, ptr, &pptr);
					}
					else
						rna_generate_deps(gen, &pptr, (idptr)? idptr: &pptr);
				}
				
				RNA_property_collection_next(prop, &iter);
			}

			RNA_property_collection_end(prop, &iter);
		}
	}
}

void RNA_generate_dependencies(PointerRNA *ptr, void *udata, PropDependencyCallback cb)
{
	RNAGenDeps gen;

	gen.udata= udata;
	gen.cb= cb;

	rna_generate_deps(&gen, ptr, NULL);
}

void RNA_test_dependencies_cb(void *udata, PointerRNA *from, PointerRNA *to)
{
	PropertyRNA *prop;
	char name[256], nameto[256];
	
	prop= from->type? from->type->nameproperty: NULL;
	if(prop) RNA_property_string_get(prop, from, name);
	else strcpy(name, "unknown");

	prop= from->type? from->type->nameproperty: NULL;
	if(prop) RNA_property_string_get(prop, to, nameto);
	else strcpy(nameto, "unknown");
	
	printf("%s (%s) -> %s (%s)\n", name, from->type->cname, nameto, to->type->cname);
}

