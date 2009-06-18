#include <stdlib.h>

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"

#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"


/*
void RNA_api_mesh_copy(Mesh *me, Mesh *from)
{
	copy_mesh_data(me, from);
}

void RNA_api_mesh_copy_applied(Mesh *me, Scene *sce, Object *ob)
{
	DerivedMesh *dm= mesh_create_derived_view(sce, ob, CD_MASK_MESH);
	DM_to_mesh(dm, me);
	dm->release(dm);
}
*/

/* copied from init_render_mesh (render code) */
void RNA_api_mesh_make_rendermesh(Mesh *me, Scene *sce, Object *ob)
{
	CustomDataMask mask = CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL;
	DerivedMesh *dm= mesh_create_derived_render(sce, ob, mask);

	/* XXX report reason */
	if(dm==NULL) return;
	
	DM_to_mesh(dm, me);
	dm->release(dm);
}

void RNA_api_mesh_transform(Mesh *me, float **mat)
{
}
