#include <stdlib.h>

#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_library.h"

#include "BLI_listbase.h"

#include "DNA_mesh_types.h"

Mesh *RNA_api_main_add_mesh(Main *main, char *name)
{
	return add_mesh(name);
}

void RNA_api_main_remove_mesh(Main *main, Mesh *me)
{
	if (BLI_findindex(&main->mesh, me) == -1) {
		/* XXX report error */
		return;
	}

	/* XXX correct ? */
	if (me->id.us == 1)
		free_libblock(&main->mesh, me);
}
