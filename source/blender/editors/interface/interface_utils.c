/**
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_material_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#define DEF_BUT_WIDTH 		150
#define DEF_ICON_BUT_WIDTH	20
#define DEF_BUT_HEIGHT		20

/*************************** RNA Utilities ******************************/

int UI_GetIconRNA(PointerRNA *ptr)
{
	StructRNA *rnatype= ptr->type;

	if(rnatype == &RNA_Scene)
		return ICON_SCENE_DEHLT;
	else if(rnatype == &RNA_World)
		return ICON_WORLD;
	else if(rnatype == &RNA_Object)
		return ICON_OBJECT;
	else if(rnatype == &RNA_Mesh)
		return ICON_MESH;
	else if(rnatype == &RNA_MeshVertex)
		return ICON_VERTEXSEL;
	else if(rnatype == &RNA_MeshEdge)
		return ICON_EDGESEL;
	else if(rnatype == &RNA_MeshFace)
		return ICON_FACESEL;
	else if(rnatype == &RNA_MeshTextureFace)
		return ICON_FACESEL_HLT;
	else if(rnatype == &RNA_VertexGroup)
		return ICON_VGROUP;
	else if(rnatype == &RNA_VertexGroupElement)
		return ICON_VGROUP;
	else if(rnatype == &RNA_Curve)
		return ICON_CURVE;
	else if(rnatype == &RNA_MetaBall)
		return ICON_MBALL;
	else if(rnatype == &RNA_MetaElement)
		return ICON_OUTLINER_DATA_META;
	else if(rnatype == &RNA_Lattice)
		return ICON_LATTICE;
	else if(rnatype == &RNA_Armature)
		return ICON_ARMATURE;
	else if(rnatype == &RNA_Bone)
		return ICON_BONE_DEHLT;
	else if(rnatype == &RNA_Camera)
		return ICON_CAMERA;
	else if(rnatype == &RNA_LocalLamp)
		return ICON_LAMP;
	else if(rnatype == &RNA_AreaLamp)
		return ICON_LAMP;
	else if(rnatype == &RNA_SpotLamp)
		return ICON_LAMP;
	else if(rnatype == &RNA_SunLamp)
		return ICON_LAMP;
	else if(rnatype == &RNA_HemiLamp)
		return ICON_LAMP;
	else if(rnatype == &RNA_Lamp)
		return ICON_LAMP;
	else if(rnatype == &RNA_Group)
		return ICON_GROUP;
	else if(rnatype == &RNA_ParticleSystem)
		return ICON_PARTICLES;
	else if(rnatype == &RNA_ParticleSettings)
		return ICON_PARTICLES;
	else if(rnatype == &RNA_Material)
		return ICON_MATERIAL;
	else if(rnatype == &RNA_Texture)
		return ICON_TEXTURE;
	else if(rnatype == &RNA_TextureSlot)
		return ICON_TEXTURE;
	else if(rnatype == &RNA_WorldTextureSlot)
		return ICON_TEXTURE;
	else if(rnatype == &RNA_MaterialTextureSlot)
		return ICON_TEXTURE;
	else if(rnatype == &RNA_Image)
		return ICON_TEXTURE;
	else if(rnatype == &RNA_Screen)
		return ICON_SPLITSCREEN;
	else if(rnatype == &RNA_NodeTree)
		return ICON_NODE;
	else if(rnatype == &RNA_Text)
		return ICON_TEXT;
	else if(rnatype == &RNA_Sound)
		return ICON_SOUND;
	else if(rnatype == &RNA_Brush)
		return ICON_BRUSH;
	else if(rnatype == &RNA_VectorFont)
		return ICON_FONT;
	else if(rnatype == &RNA_Library)
		return ICON_LIBRARY_DEHLT;
	else if(rnatype == &RNA_Action)
		return ICON_ACTION;
	else if(rnatype == &RNA_FCurve)
		return ICON_IPO_DEHLT;
	//else if(rnatype == &RNA_Ipo)
	//	return ICON_IPO_DEHLT;
	else if(rnatype == &RNA_Key)
		return ICON_SHAPEKEY;
	else if(rnatype == &RNA_Main)
		return ICON_BLENDER;
	else if(rnatype == &RNA_Struct)
		return ICON_RNA;
	else if(rnatype == &RNA_Property)
		return ICON_RNA;
	else if(rnatype == &RNA_BooleanProperty)
		return ICON_RNA;
	else if(rnatype == &RNA_IntProperty)
		return ICON_RNA;
	else if(rnatype == &RNA_FloatProperty)
		return ICON_RNA;
	else if(rnatype == &RNA_StringProperty)
		return ICON_RNA;
	else if(rnatype == &RNA_EnumProperty)
		return ICON_RNA;
	else if(rnatype == &RNA_EnumPropertyItem)
		return ICON_RNA;
	else if(rnatype == &RNA_PointerProperty)
		return ICON_RNA;
	else if(rnatype == &RNA_CollectionProperty)
		return ICON_RNA;
	else if(rnatype == &RNA_GameObjectSettings)
		return ICON_GAME;
	else if(rnatype == &RNA_ScriptLink)
		return ICON_PYTHON;
	
	/* modifiers */
	else if(rnatype == &RNA_SubsurfModifier)
		return ICON_MOD_SUBSURF;
	else if(rnatype == &RNA_ArmatureModifier)
		return ICON_ARMATURE;
	else if(rnatype == &RNA_LatticeModifier)
		return ICON_LATTICE;
	else if(rnatype == &RNA_CurveModifier)
		return ICON_CURVE;
	else if(rnatype == &RNA_BuildModifier)
		return ICON_MOD_BUILD;
	else if(rnatype == &RNA_MirrorModifier)
		return ICON_MOD_MIRROR;
	else if(rnatype == &RNA_DecimateModifier)
		return ICON_MOD_DECIM;
	else if(rnatype == &RNA_WaveModifier)
		return ICON_MOD_WAVE;
	else if(rnatype == &RNA_HookModifier)
		return ICON_HOOK;
	else if(rnatype == &RNA_SoftbodyModifier)
		return ICON_MOD_SOFT;
	else if(rnatype == &RNA_BooleanModifier)
		return ICON_MOD_BOOLEAN;
	else if(rnatype == &RNA_ParticleInstanceModifier)
		return ICON_MOD_PARTICLEINSTANCE;
	else if(rnatype == &RNA_ParticleSystemModifier)
		return ICON_MOD_PARTICLES;
	else if(rnatype == &RNA_EdgeSplitModifier)
		return ICON_MOD_EDGESPLIT;
	else if(rnatype == &RNA_ArrayModifier)
		return ICON_MOD_ARRAY;
	else if(rnatype == &RNA_UVProjectModifier)
		return ICON_MOD_UVPROJECT;
	else if(rnatype == &RNA_DisplaceModifier)
		return ICON_MOD_DISPLACE;
	else
		return ICON_DOT;
}

uiBut *uiDefAutoButR(uiBlock *block, PointerRNA *ptr, PropertyRNA *prop, int index, char *name, int x1, int y1, int x2, int y2)
{
	uiBut *but=NULL;
	const char *propname= RNA_property_identifier(ptr, prop);
	int arraylen= RNA_property_array_length(ptr, prop);

	switch(RNA_property_type(ptr, prop)) {
		case PROP_BOOLEAN: {
			int value, length;

			if(arraylen && index == -1)
				return NULL;

			length= RNA_property_array_length(ptr, prop);

			if(length)
				value= RNA_property_boolean_get_index(ptr, prop, index);
			else
				value= RNA_property_boolean_get(ptr, prop);

			if(name && strcmp(name, "") == 0)
				name= (value)? "Enabled": "Disabled";

			but= uiDefButR(block, TOG, 0, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		}
		case PROP_INT:
		case PROP_FLOAT:
			if(arraylen && index == -1) {
				if(RNA_property_subtype(ptr, prop) == PROP_COLOR)
					but= uiDefButR(block, COL, 0, name, x1, y1, x2, y2, ptr, propname, 0, 0, 0, -1, -1, NULL);
			}
			else
				but= uiDefButR(block, NUM, 0, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_ENUM:
			but= uiDefButR(block, MENU, 0, NULL, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_STRING:
			but= uiDefButR(block, TEX, 0, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_POINTER: {
			PointerRNA pptr;
			PropertyRNA *nameprop;
			char *text, *descr, textbuf[256];
			int icon;

			pptr= RNA_property_pointer_get(ptr, prop);

			if(!pptr.data)
				return NULL;

			icon= UI_GetIconRNA(&pptr);
			nameprop= RNA_struct_name_property(&pptr);

			if(nameprop) {
				text= RNA_property_string_get_alloc(&pptr, nameprop, textbuf, sizeof(textbuf));
				descr= (char*)RNA_property_ui_description(&pptr, prop);
				but= uiDefIconTextBut(block, LABEL, 0, icon, text, x1, y1, x2, y2, NULL, 0, 0, 0, 0, descr);
				if(text != textbuf)
					MEM_freeN(text);
			}
			else {
				text= (char*)RNA_struct_ui_name(&pptr);
				descr= (char*)RNA_property_ui_description(&pptr, prop);
				but= uiDefIconTextBut(block, LABEL, 0, icon, text, x1, y1, x2, y2, NULL, 0, 0, 0, 0, descr);
			}
			break;
		}
		case PROP_COLLECTION: {
			char text[256];
			sprintf(text, "%d items", RNA_property_collection_length(ptr, prop));
			but= uiDefBut(block, LABEL, 0, text, x1, y1, x2, y2, NULL, 0, 0, 0, 0, NULL);
			uiButSetFlag(but, UI_BUT_DISABLED);
			break;
		}
		default:
			but= NULL;
			break;
	}

	return but;
}

int uiDefAutoButsRNA(uiBlock *block, PointerRNA *ptr)
{
	CollectionPropertyIterator iter;
	PropertyRNA *iterprop, *prop;
	PropertySubType subtype;
	char *name, namebuf[128];
	int a, length, x= 0, y= 0;

	x= 0;
	y= 0;

	/* create buttons */
	uiSetCurFont(block, UI_HELVB);
	uiDefBut(block, LABEL, 0, (char*)RNA_struct_ui_name(ptr), x, y, DEF_BUT_WIDTH, DEF_BUT_HEIGHT-1, NULL, 0, 0, 0, 0, "");
	y -= DEF_BUT_HEIGHT;
	uiSetCurFont(block, UI_HELV);

	iterprop= RNA_struct_iterator_property(ptr);
	RNA_property_collection_begin(ptr, iterprop, &iter);

	for(; iter.valid; RNA_property_collection_next(&iter)) {
		prop= iter.ptr.data;

		if(strcmp(RNA_property_identifier(ptr, prop), "rna_type") == 0)
			continue;

		if((length= RNA_property_array_length(ptr, prop))) {
			name= (char*)RNA_property_ui_name(ptr, prop);
			uiDefBut(block, LABEL, 0, name, x, y, DEF_BUT_WIDTH, DEF_BUT_HEIGHT-1, NULL, 0, 0, 0, 0, "");
		}
		else
			length= 1;

		subtype= RNA_property_subtype(ptr, prop);

		name= (char*)RNA_property_ui_name(ptr, prop);
		uiDefBut(block, LABEL, 0, name, x, y, DEF_BUT_WIDTH, DEF_BUT_HEIGHT-1, NULL, 0, 0, 0, 0, "");

		uiBlockBeginAlign(block);

		if(length <= 16 && subtype == PROP_MATRIX) {
			/* matrix layout */
			int size, row, col, butwidth;

			size= ceil(sqrt(length));
			butwidth= DEF_BUT_WIDTH*2/size;
			y -= DEF_BUT_HEIGHT;

			for(a=0; a<length; a++) {
				col= a%size;
				row= a/size;

				uiDefAutoButR(block, ptr, prop, a, "", x+butwidth*col, y-row*DEF_BUT_HEIGHT, butwidth, DEF_BUT_HEIGHT-1);
			}

			y -= DEF_BUT_HEIGHT*(length/size);
		}
		else if(length <= 4 && ELEM3(subtype, PROP_ROTATION, PROP_VECTOR, PROP_COLOR)) {
			static char *vectoritem[4]= {"X:", "Y:", "Z:", "W:"};
			static char *quatitem[4]= {"W:", "X:", "Y:", "Z:"};
			static char *coloritem[4]= {"R:", "G:", "B:", "A:"};
			int butwidth;

			butwidth= DEF_BUT_WIDTH*2/length;
			y -= DEF_BUT_HEIGHT;

			for(a=0; a<length; a++) {
				if(length == 4 && subtype == PROP_ROTATION)
					name= quatitem[a];
				else if(subtype == PROP_VECTOR || subtype == PROP_ROTATION)
					name= vectoritem[a];
				else
					name= coloritem[a];

				uiDefAutoButR(block, ptr, prop, a, name, x+butwidth*a, y, butwidth, DEF_BUT_HEIGHT-1);
			}
			y -= DEF_BUT_HEIGHT;
		}
		else {
			if(RNA_property_array_length(ptr, prop)) {
				sprintf(namebuf, "%d:", a+1);
				name= namebuf;
			}
			else
				name= "";

			uiDefAutoButR(block, ptr, prop, a, name, x+DEF_BUT_WIDTH, y, DEF_BUT_WIDTH, DEF_BUT_HEIGHT-1);
			y -= DEF_BUT_HEIGHT;
		}

		uiBlockEndAlign(block);
	}

	RNA_property_collection_end(&iter);

	return -y;
}

/***************************** ID Utilities *******************************/

typedef struct uiIDPoinParams {
	uiIDPoinFunc func;
	ID **id_p;
	short id_code;
	short browsenr;
} uiIDPoinParams;

static void idpoin_cb(bContext *C, void *arg_params, void *arg_event)
{
	Main *bmain;
	ListBase *lb;
	uiIDPoinParams *params= (uiIDPoinParams*)arg_params;
	uiIDPoinFunc func= params->func;
	ID **id_p= params->id_p;
	ID *id= *id_p, *idtest;
	int nr, event= GET_INT_FROM_POINTER(arg_event);

	bmain= CTX_data_main(C);
	lb= wich_libbase(bmain, params->id_code);

	switch(event) {
		case UI_ID_RENAME:
			if(id) test_idbutton(id->name+2);
			else return;
			break;
		case UI_ID_BROWSE: {
			if(id==0) id= lb->first;
			if(id==0) return;

			if(params->browsenr== -2) {
				/* XXX implement or find a replacement
				 * activate_databrowse((ID *)G.buts->lockpoin, GS(id->name), 0, B_MESHBROWSE, &params->browsenr, do_global_buttons); */
				return;
			}
			if(params->browsenr < 0)
				return;

			for(idtest=lb->first, nr=1; idtest; idtest=idtest->next, nr++) {
				if(nr==params->browsenr) {
					if(id == idtest)
						return;

					*id_p= idtest;
					break;
				}
			}
			break;
		}
		case UI_ID_DELETE:
			*id_p= NULL;
			break;
		case UI_ID_FAKE_USER:
			if(id) {
				if(id->flag & LIB_FAKEUSER) id->us++;
				else id->us--;
			}
			else return;
			break;
		case UI_ID_PIN:
			break;
		case UI_ID_ADD_NEW:
			break;
		case UI_ID_OPEN:
			break;
		case UI_ID_ALONE:
			if(!id || id->us < 1)
				return;
			break;
		case UI_ID_LOCAL:
			if(!id || id->us < 1)
				return;
			break;
		case UI_ID_AUTO_NAME:
			break;
	}

	if(func)
		func(C, *id_p, event);
}

int uiDefIDPoinButs(uiBlock *block, Main *bmain, ID *parid, ID **id_p, int id_code, short *pin_p, int x, int y, uiIDPoinFunc func, int events)
{
	ListBase *lb;
	uiBut *but;
	ID *id= *id_p;
	uiIDPoinParams *params, *dup_params;
	char *str=NULL, str1[10];
	int len, oldcol, add_addbutton=0;

	/* setup struct that we will pass on with the buttons */
	params= MEM_callocN(sizeof(uiIDPoinParams), "uiIDPoinParams");
	params->id_p= id_p;
	params->id_code= id_code;
	params->func= func;

	lb= wich_libbase(bmain, id_code);

	/* create buttons */
	uiBlockBeginAlign(block);
	oldcol= uiBlockGetCol(block);

	if(id && id->us>1)
		uiBlockSetCol(block, TH_BUT_SETTING1);

	if((events & UI_ID_PIN) && *pin_p)
		uiBlockSetCol(block, TH_BUT_SETTING2);

	/* pin button */
	if(id && (events & UI_ID_PIN)) {
		but= uiDefIconButS(block, ICONTOG, (events & UI_ID_PIN), ICON_KEY_DEHLT, x, y ,DEF_ICON_BUT_WIDTH,DEF_BUT_HEIGHT, pin_p, 0, 0, 0, 0, "Keeps this view displaying the current data regardless of what object is selected");
		uiButSetNFunc(but, idpoin_cb, MEM_dupallocN(params), SET_INT_IN_POINTER(UI_ID_PIN));
		x+= DEF_ICON_BUT_WIDTH;
	}

	/* browse menu */
	if(events & UI_ID_BROWSE) {
		char *extrastr= NULL;
		
		if(ELEM4(id_code, ID_MA, ID_TE, ID_BR, ID_PA))
			add_addbutton= 1;
		
		if(ELEM8(id_code, ID_SCE, ID_SCR, ID_MA, ID_TE, ID_WO, ID_IP, ID_AC, ID_BR) || id_code == ID_PA)
			extrastr= "ADD NEW %x 32767";
		else if(id_code==ID_TXT)
			extrastr= "OPEN NEW %x 32766 |ADD NEW %x 32767";
		else if(id_code==ID_SO)
			extrastr= "OPEN NEW %x 32766";

		/* XXX should be moved out of this function
		uiBlockSetButLock(block, G.scene->id.lib!=0, "Can't edit external libdata");
		if( id_code==ID_SCE || id_code==ID_SCR ) uiBlockClearButLock(block); */
		
		/* XXX should be moved out of this function
		if(curarea->spacetype==SPACE_BUTS)
			uiBlockSetButLock(block, id_code!=ID_SCR && G.obedit!=0 && G.buts->mainb==CONTEXT_EDITING, "Cannot perform in EditMode"); */
		
		if(parid)
			uiBlockSetButLock(block, parid->lib!=0, "Can't edit external libdata");

		if(lb) {
			if(id_code!=ID_IM || (events & UI_ID_BROWSE_RENDER))
				IDnames_to_pupstring(&str, NULL, extrastr, lb, id, &params->browsenr);
			else
				IMAnames_to_pupstring(&str, NULL, extrastr, lb, id, &params->browsenr);
		}

		dup_params= MEM_dupallocN(params);
		but= uiDefButS(block, MENU, 0, str, x, y, DEF_ICON_BUT_WIDTH, DEF_BUT_HEIGHT, &dup_params->browsenr, 0, 0, 0, 0, "Browse existing choices, or add new");
		uiButSetNFunc(but, idpoin_cb, dup_params, SET_INT_IN_POINTER(UI_ID_BROWSE));
		x+= DEF_ICON_BUT_WIDTH;
		
		uiBlockClearButLock(block);
	
		MEM_freeN(str);
	}

	uiBlockSetCol(block, oldcol);

	/* text button with name */
	if(id) {
		/* name */
		if(id->us > 1)
			uiBlockSetCol(block, TH_BUT_SETTING1);

		/* pinned data? */
		if((events & UI_ID_PIN) && *pin_p)
			uiBlockSetCol(block, TH_BUT_SETTING2);

		/* redalert overrides pin color */
		if(id->us<=0)
			uiBlockSetCol(block, TH_REDALERT);

		uiBlockSetButLock(block, id->lib!=0, "Can't edit external libdata");
		
		/* name button */
		if(GS(id->name)==ID_SCE)
			strcpy(str1, "SCE:");
		else if(GS(id->name)==ID_SCE)
			strcpy(str1, "SCR:");
		else if(GS(id->name)==ID_MA && ((Material*)id)->use_nodes)
			strcpy(str1, "NT:");
		else {
			str1[0]= id->name[0];
			str1[1]= id->name[1];
			str1[2]= ':';
			str1[3]= 0;
		}
		
		if(GS(id->name)==ID_IP) len= 110;
		else if((y) && (GS(id->name)==ID_AC)) len= 100; // comes from button panel (poselib)
		else if(y) len= 140;	// comes from button panel
		else len= 120;
		
		but= uiDefBut(block, TEX, 0, str1,x, y, (short)len, DEF_BUT_HEIGHT, id->name+2, 0.0, 21.0, 0, 0, "Displays current Datablock name. Click to change.");
		uiButSetNFunc(but, idpoin_cb, MEM_dupallocN(params), SET_INT_IN_POINTER(UI_ID_RENAME));

		x+= len;

		uiBlockClearButLock(block);
		
		/* lib make local button */
		if(id->lib) {
			if(id->flag & LIB_INDIRECT) uiDefIconBut(block, BUT, 0, 0 /* XXX ICON_DATALIB */,x,y,DEF_ICON_BUT_WIDTH,DEF_BUT_HEIGHT, 0, 0, 0, 0, 0, "Indirect Library Datablock. Cannot change.");
			else {
				but= uiDefIconBut(block, BUT, 0, 0 /* XXX ICON_PARLIB */, x,y,DEF_ICON_BUT_WIDTH,DEF_BUT_HEIGHT, 0, 0, 0, 0, 0, 
							  (events & UI_ID_LOCAL)? "Direct linked Library Datablock. Click to make local.": "Direct linked Library Datablock, cannot make local.");
				uiButSetNFunc(but, idpoin_cb, MEM_dupallocN(params), SET_INT_IN_POINTER(UI_ID_ALONE));
			}
			
			x+= DEF_ICON_BUT_WIDTH;
		}
		
		/* number of users / make local button */
		if((events & UI_ID_ALONE) && id->us>1) {
			int butwidth;

			uiBlockSetButLock(block, (events & UI_ID_PIN) && *pin_p, "Can't make pinned data single-user");
			
			sprintf(str1, "%d", id->us);
			butwidth= (id->us<10)? DEF_ICON_BUT_WIDTH: DEF_ICON_BUT_WIDTH+10;

			but= uiDefBut(block, BUT, 0, str1, x, y, butwidth, DEF_BUT_HEIGHT, 0, 0, 0, 0, 0, "Displays number of users of this data. Click to make a single-user copy.");
			uiButSetNFunc(but, idpoin_cb, MEM_dupallocN(params), SET_INT_IN_POINTER(UI_ID_ALONE));
			x+= butwidth;
			
			uiBlockClearButLock(block);
		}
		
		/* delete button */
		if(events & UI_ID_DELETE) {
			uiBlockSetButLock(block, (events & UI_ID_PIN) && *pin_p, "Can't unlink pinned data");
			if(parid && parid->lib);
			else {
				but= uiDefIconBut(block, BUT, 0, ICON_X, x,y,DEF_ICON_BUT_WIDTH,DEF_BUT_HEIGHT, 0, 0, 0, 0, 0, "Deletes link to this Datablock");
				uiButSetNFunc(but, idpoin_cb, MEM_dupallocN(params), SET_INT_IN_POINTER(UI_ID_DELETE));
				x+= DEF_ICON_BUT_WIDTH;
			}
			
			uiBlockClearButLock(block);
		}

		/* auto name button */
		if(events & UI_ID_AUTO_NAME) {
			if(parid && parid->lib);
			else {
				but= uiDefIconBut(block, BUT, 0, ICON_AUTO,x,y,DEF_ICON_BUT_WIDTH,DEF_BUT_HEIGHT, 0, 0, 0, 0, 0, "Generates an automatic name");
				uiButSetNFunc(but, idpoin_cb, MEM_dupallocN(params), SET_INT_IN_POINTER(UI_ID_AUTO_NAME));
				x+= DEF_ICON_BUT_WIDTH;
			}
		}

		/* fake user button */
		if(events & UI_ID_FAKE_USER) {
			but= uiDefButBitS(block, TOG, LIB_FAKEUSER, 0, "F", x,y,DEF_ICON_BUT_WIDTH,DEF_BUT_HEIGHT, &id->flag, 0, 0, 0, 0, "Saves this datablock even if it has no users");
			uiButSetNFunc(but, idpoin_cb, MEM_dupallocN(params), SET_INT_IN_POINTER(UI_ID_FAKE_USER));
			x+= DEF_ICON_BUT_WIDTH;
		}
	}
	/* add new button */
	else if(add_addbutton) {
		uiBlockSetCol(block, oldcol);
		if(parid) uiBlockSetButLock(block, parid->lib!=0, "Can't edit external libdata");
		dup_params= MEM_dupallocN(params);
		but= uiDefButS(block, TOG, 0, "Add New", x, y, 110, DEF_BUT_HEIGHT, &dup_params->browsenr, params->browsenr, 32767.0, 0, 0, "Add new data block");
		uiButSetNFunc(but, idpoin_cb, dup_params, SET_INT_IN_POINTER(UI_ID_ADD_NEW));
		x+= 110;
	}
	
	uiBlockSetCol(block, oldcol);
	uiBlockEndAlign(block);

	MEM_freeN(params);

	return x;
}

