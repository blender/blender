#include "bmesh.h"
#include "bmesh_private.h"

#include <stdio.h>

BMOpDefine def_connectverts = {
	"connectvert",
	{BMOP_OPSLOT_PNT_BUF,
	 BMOP_OPSLOT_PNT_BUF},
	connectverts_exec,
	BM_CONVERTS_TOTSLOT,
	0
};

BMOpDefine def_extrudefaceregion = {
	"extrudefaceregion",
	{BMOP_OPSLOT_PNT_BUF,
	 BMOP_OPSLOT_MAPPING,
	 BMOP_OPSLOT_PNT_BUF},
	extrude_edge_context_exec,
	BMOP_EXFACE_TOTSLOT,
	0
};

BMOpDefine def_makefgonsop = {
	"makefgon",
	{0},
	bmesh_make_fgons_exec,
	BMOP_MAKE_FGONS_TOTSLOT,
	0
};

BMOpDefine def_dissolvevertsop = {
	"dissolveverts",
	{BMOP_OPSLOT_PNT_BUF},
	dissolveverts_exec,
	BMOP_DISVERTS_TOTSLOT,
	0
};

BMOpDefine def_dissolvefacesop = {
	"dissolvefaces",
	{BMOP_OPSLOT_PNT_BUF,
	 BMOP_OPSLOT_PNT_BUF},
	dissolvefaces_exec,
	BMOP_DISFACES_TOTSLOT,
	0
};


BMOpDefine def_triangop = {
	"triangulate",
	{BMOP_OPSLOT_PNT_BUF, 
	 BMOP_OPSLOT_PNT_BUF,
	 BMOP_OPSLOT_PNT_BUF},
	triangulate_exec,
	BMOP_TRIANG_TOTSLOT,
	0
};

BMOpDefine def_subdop = {
	"esubd",
	{BMOP_OPSLOT_PNT_BUF,
	 BMOP_OPSLOT_INT,
	 BMOP_OPSLOT_INT,
	 BMOP_OPSLOT_FLT,
	 BMOP_OPSLOT_MAPPING,
	 BMOP_OPSLOT_MAPPING,
	 BMOP_OPSLOT_PNT_BUF,
	 BMOP_OPSLOT_PNT_BUF,
	 },
	esubdivide_exec,
	BMOP_ESUBDIVIDE_TOTSLOT,
	0
};

BMOpDefine def_edit2bmesh = {
	"editmesh_to_bmesh",
	{BMOP_OPSLOT_PNT},
	edit2bmesh_exec,
	BMOP_TO_EDITMESH_TOTSLOT,
	0
};

BMOpDefine def_bmesh2edit = {
	"bmesh_to_editmesh",
	{BMOP_OPSLOT_PNT},
	bmesh2edit_exec,
	BMOP_FROM_EDITMESH_TOTSLOT,
	0
};

BMOpDefine def_delop = {
	"del",
	{BMOP_OPSLOT_PNT_BUF, BMOP_OPSLOT_INT},
	delop_exec,
	BMOP_DEL_TOTSLOT,
	0
};

BMOpDefine def_dupeop = {
	"dupe",
	{BMOP_OPSLOT_PNT_BUF, BMOP_OPSLOT_PNT_BUF,
	 BMOP_OPSLOT_PNT_BUF, BMOP_OPSLOT_MAPPING},
	dupeop_exec,
	BMOP_DUPE_TOTSLOT,
	0
};

BMOpDefine def_splitop = {
	"split",
	{BMOP_OPSLOT_PNT_BUF,
	 BMOP_OPSLOT_PNT_BUF, BMOP_OPSLOT_MAPPING},
	splitop_exec,
	BMOP_SPLIT_TOTSLOT,
	0
};

BMOpDefine *opdefines[] = {
	&def_splitop,
	&def_dupeop,
	&def_delop,
	&def_edit2bmesh,
	&def_bmesh2edit,
	&def_subdop,
	&def_triangop,
	&def_dissolvefacesop,
	&def_dissolvevertsop,
	&def_makefgonsop,
	&def_extrudefaceregion,
	&def_connectverts,
};

int bmesh_total_ops = (sizeof(opdefines) / sizeof(void*));
