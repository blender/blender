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
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

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
		return ICON_TPAINT_HLT;
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

#define RNA_BUT_WIDTH 150
#define RNA_BUT_HEIGHT 20

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
	uiDefBut(block, LABEL, 0, (char*)RNA_struct_ui_name(ptr), x, y, RNA_BUT_WIDTH, RNA_BUT_HEIGHT-1, NULL, 0, 0, 0, 0, "");
	y -= RNA_BUT_HEIGHT;
	uiSetCurFont(block, UI_HELV);

	iterprop= RNA_struct_iterator_property(ptr);
	RNA_property_collection_begin(ptr, iterprop, &iter);

	for(; iter.valid; RNA_property_collection_next(&iter)) {
		prop= iter.ptr.data;

		if(strcmp(RNA_property_identifier(ptr, prop), "rna_type") == 0)
			continue;

		if((length= RNA_property_array_length(ptr, prop))) {
			name= (char*)RNA_property_ui_name(ptr, prop);
			uiDefBut(block, LABEL, 0, name, x, y, RNA_BUT_WIDTH, RNA_BUT_HEIGHT-1, NULL, 0, 0, 0, 0, "");
		}
		else
			length= 1;

		subtype= RNA_property_subtype(ptr, prop);

		name= (char*)RNA_property_ui_name(ptr, prop);
		uiDefBut(block, LABEL, 0, name, x, y, RNA_BUT_WIDTH, RNA_BUT_HEIGHT-1, NULL, 0, 0, 0, 0, "");

		uiBlockBeginAlign(block);

		if(length <= 16 && subtype == PROP_MATRIX) {
			/* matrix layout */
			int size, row, col, butwidth;

			size= ceil(sqrt(length));
			butwidth= RNA_BUT_WIDTH*2/size;
			y -= RNA_BUT_HEIGHT;

			for(a=0; a<length; a++) {
				col= a%size;
				row= a/size;

				uiDefAutoButR(block, ptr, prop, a, "", x+butwidth*col, y-row*RNA_BUT_HEIGHT, butwidth, RNA_BUT_HEIGHT-1);
			}

			y -= RNA_BUT_HEIGHT*(length/size);
		}
		else if(length <= 4 && ELEM3(subtype, PROP_ROTATION, PROP_VECTOR, PROP_COLOR)) {
			static char *vectoritem[4]= {"X:", "Y:", "Z:", "W:"};
			static char *quatitem[4]= {"W:", "X:", "Y:", "Z:"};
			static char *coloritem[4]= {"R:", "G:", "B:", "A:"};
			int butwidth;

			butwidth= RNA_BUT_WIDTH*2/length;
			y -= RNA_BUT_HEIGHT;

			for(a=0; a<length; a++) {
				if(length == 4 && subtype == PROP_ROTATION)
					name= quatitem[a];
				else if(subtype == PROP_VECTOR || subtype == PROP_ROTATION)
					name= vectoritem[a];
				else
					name= coloritem[a];

				uiDefAutoButR(block, ptr, prop, a, name, x+butwidth*a, y, butwidth, RNA_BUT_HEIGHT-1);
			}
			y -= RNA_BUT_HEIGHT;
		}
		else {
			if(RNA_property_array_length(ptr, prop)) {
				sprintf(namebuf, "%d:", a+1);
				name= namebuf;
			}
			else
				name= "";

			uiDefAutoButR(block, ptr, prop, a, name, x+RNA_BUT_WIDTH, y, RNA_BUT_WIDTH, RNA_BUT_HEIGHT-1);
			y -= RNA_BUT_HEIGHT;
		}

		uiBlockEndAlign(block);
	}

	RNA_property_collection_end(&iter);

	return -y;
}

#if 0
#endif

