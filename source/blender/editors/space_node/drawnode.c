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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): David Millan Escriva, Juho Vepsäläinen, Bob Holcomb, Thomas Dinges
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/drawnode.c
 *  \ingroup spnode
 *  \brief lower level node drawing for nodes (boarders, headers etc), also node layout.
 */

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_tracking.h"

#include "BLF_api.h"
#include "BLF_translation.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_node.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "node_intern.h"  /* own include */
#include "NOD_composite.h"
#include "NOD_shader.h"
#include "NOD_texture.h"


/* ****************** SOCKET BUTTON DRAW FUNCTIONS ***************** */

static void node_socket_button_label(bContext *UNUSED(C), uiLayout *layout, PointerRNA *UNUSED(ptr), PointerRNA *UNUSED(node_ptr),
                                     const char *text)
{
	uiItemL(layout, text, 0);
}


/* ****************** BASE DRAW FUNCTIONS FOR NEW OPERATOR NODES ***************** */

#if 0 /* UNUSED */
static void node_draw_socket_new(bNodeSocket *sock, float size)
{
	float x = sock->locx, y = sock->locy;
	
	/* 16 values of sin function */
	static float si[16] = {
		0.00000000f, 0.39435585f, 0.72479278f, 0.93775213f,
		0.99871650f, 0.89780453f, 0.65137248f, 0.29936312f,
		-0.10116832f, -0.48530196f, -0.79077573f, -0.96807711f,
		-0.98846832f, -0.84864425f, -0.57126821f, -0.20129852f
	};
	/* 16 values of cos function */
	static float co[16] = {
		1.00000000f, 0.91895781f, 0.68896691f, 0.34730525f,
		-0.05064916f, -0.44039415f, -0.75875812f, -0.95413925f,
		-0.99486932f, -0.87434661f, -0.61210598f, -0.25065253f,
		0.15142777f, 0.52896401f, 0.82076344f, 0.97952994f,
	};
	int a;
	
	glColor3ub(180, 180, 180);
	
	glBegin(GL_POLYGON);
	for (a = 0; a < 16; a++)
		glVertex2f(x + size * si[a], y + size * co[a]);
	glEnd();
	
	glColor4ub(0, 0, 0, 150);
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glBegin(GL_LINE_LOOP);
	for (a = 0; a < 16; a++)
		glVertex2f(x + size * si[a], y + size * co[a]);
	glEnd();
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}
#endif

/* ****************** BUTTON CALLBACKS FOR ALL TREES ***************** */

static void node_buts_value(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	bNode *node = ptr->data;
	/* first output stores value */
	bNodeSocket *output = node->outputs.first;
	PointerRNA sockptr;
	RNA_pointer_create(ptr->id.data, &RNA_NodeSocket, output, &sockptr);
	
	uiItemR(layout, &sockptr, "default_value", 0, "", ICON_NONE);
}

static void node_buts_rgb(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	bNode *node = ptr->data;
	/* first output stores value */
	bNodeSocket *output = node->outputs.first;
	PointerRNA sockptr;
	uiLayout *col;
	RNA_pointer_create(ptr->id.data, &RNA_NodeSocket, output, &sockptr);
	
	col = uiLayoutColumn(layout, false);
	uiTemplateColorPicker(col, &sockptr, "default_value", 1, 0, 0, 0);
	uiItemR(col, &sockptr, "default_value", UI_ITEM_R_SLIDER, "", ICON_NONE);
}

static void node_buts_mix_rgb(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *row, *col;

	bNodeTree *ntree = (bNodeTree *)ptr->id.data;

	col = uiLayoutColumn(layout, false);
	row = uiLayoutRow(col, true);
	uiItemR(row, ptr, "blend_type", 0, "", ICON_NONE);
	if (ELEM(ntree->type, NTREE_COMPOSIT, NTREE_TEXTURE))
		uiItemR(row, ptr, "use_alpha", 0, "", ICON_IMAGE_RGB_ALPHA);

	uiItemR(col, ptr, "use_clamp", 0, NULL, ICON_NONE);
}

static void node_buts_time(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row;
#if 0
	/* XXX no context access here .. */
	bNode *node = ptr->data;
	CurveMapping *cumap = node->storage;
	
	if (cumap) {
		cumap->flag |= CUMA_DRAW_CFRA;
		if (node->custom1 < node->custom2)
			cumap->sample[0] = (float)(CFRA - node->custom1) / (float)(node->custom2 - node->custom1);
	}
#endif

	uiTemplateCurveMapping(layout, ptr, "curve", 's', 0, 0);

	row = uiLayoutRow(layout, true);
	uiItemR(row, ptr, "frame_start", 0, IFACE_("Sta"), ICON_NONE);
	uiItemR(row, ptr, "frame_end", 0, IFACE_("End"), ICON_NONE);
}

static void node_buts_colorramp(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiTemplateColorRamp(layout, ptr, "color_ramp", 0);
}

static void node_buts_curvevec(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiTemplateCurveMapping(layout, ptr, "mapping", 'v', 0, 0);
}

#define SAMPLE_FLT_ISNONE FLT_MAX
static float _sample_col[4] = {SAMPLE_FLT_ISNONE};  /* bad bad, 2.5 will do better?... no it won't... */
void ED_node_sample_set(const float col[4])
{
	if (col) {
		copy_v4_v4(_sample_col, col);
	}
	else {
		copy_v4_fl(_sample_col, SAMPLE_FLT_ISNONE);
	}
}

static void node_buts_curvecol(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	bNode *node = ptr->data;
	CurveMapping *cumap = node->storage;

	if (_sample_col[0] != SAMPLE_FLT_ISNONE) {
		cumap->flag |= CUMA_DRAW_SAMPLE;
		copy_v3_v3(cumap->sample, _sample_col);
	}
	else {
		cumap->flag &= ~CUMA_DRAW_SAMPLE;
	}

	uiTemplateCurveMapping(layout, ptr, "mapping", 'c', 0, 0);
}

static void node_buts_normal(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	bNode *node = ptr->data;
	/* first output stores normal */
	bNodeSocket *output = node->outputs.first;
	PointerRNA sockptr;
	RNA_pointer_create(ptr->id.data, &RNA_NodeSocket, output, &sockptr);
	
	uiItemR(layout, &sockptr, "default_value", 0, "", ICON_NONE);
}

#if 0 /* not used in 2.5x yet */
static void node_browse_tex_cb(bContext *C, void *ntree_v, void *node_v)
{
	Main *bmain = CTX_data_main(C);
	bNodeTree *ntree = ntree_v;
	bNode *node = node_v;
	Tex *tex;
	
	if (node->menunr < 1) return;
	
	if (node->id) {
		node->id->us--;
		node->id = NULL;
	}
	tex = BLI_findlink(&bmain->tex, node->menunr - 1);

	node->id = &tex->id;
	id_us_plus(node->id);
	BLI_strncpy(node->name, node->id->name + 2, sizeof(node->name));
	
	nodeSetActive(ntree, node);
	
	if (ntree->type == NTREE_TEXTURE)
		ntreeTexCheckCyclics(ntree);
	
	// allqueue(REDRAWBUTSSHADING, 0);
	// allqueue(REDRAWNODE, 0);
	NodeTagChanged(ntree, node); 
	
	node->menunr = 0;
}
#endif

static void node_buts_texture(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	bNode *node = ptr->data;

	short multi = (
	    node->id &&
	    ((Tex *)node->id)->use_nodes &&
	    (node->type != CMP_NODE_TEXTURE) &&
	    (node->type != TEX_NODE_TEXTURE)
	    );
	
	uiItemR(layout, ptr, "texture", 0, "", ICON_NONE);
	
	if (multi) {
		/* Number Drawing not optimal here, better have a list*/
		uiItemR(layout, ptr, "node_output", 0, "", ICON_NONE);
	}
}

static void node_buts_math(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{ 
	uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
	uiItemR(layout, ptr, "use_clamp", 0, NULL, ICON_NONE);
}

static int node_resize_area_default(bNode *node, int x, int y)
{
	if (node->flag & NODE_HIDDEN) {
		rctf totr = node->totr;
		/* right part of node */
		totr.xmin = node->totr.xmax - 20.0f;
		if (BLI_rctf_isect_pt(&totr, x, y))
			return NODE_RESIZE_RIGHT;
		else
			return 0;
	}
	else {
		const float size = 10.0f;
		rctf totr = node->totr;
		int dir = 0;
		
		if (x >= totr.xmax - size && x < totr.xmax && y >= totr.ymin && y < totr.ymax)
			dir |= NODE_RESIZE_RIGHT;
		if (x >= totr.xmin && x < totr.xmin + size && y >= totr.ymin && y < totr.ymax)
			dir |= NODE_RESIZE_LEFT;
		return dir;
	}
}

/* ****************** BUTTON CALLBACKS FOR COMMON NODES ***************** */


static void node_draw_buttons_group(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiTemplateIDBrowse(layout, C, ptr, "node_tree", NULL, NULL, NULL);
}

/* XXX Does a bounding box update by iterating over all children.
 * Not ideal to do this in every draw call, but doing as transform callback doesn't work,
 * since the child node totr rects are not updated properly at that point.
 */
static void node_draw_frame_prepare(const bContext *UNUSED(C), bNodeTree *ntree, bNode *node)
{
	const float margin = 1.5f * U.widget_unit;
	NodeFrame *data = (NodeFrame *)node->storage;
	int bbinit;
	bNode *tnode;
	rctf rect, noderect;
	float xmax, ymax;
	
	/* init rect from current frame size */
	node_to_view(node, node->offsetx, node->offsety, &rect.xmin, &rect.ymax);
	node_to_view(node, node->offsetx + node->width, node->offsety - node->height, &rect.xmax, &rect.ymin);
	
	/* frame can be resized manually only if shrinking is disabled or no children are attached */
	data->flag |= NODE_FRAME_RESIZEABLE;
	/* for shrinking bbox, initialize the rect from first child node */
	bbinit = (data->flag & NODE_FRAME_SHRINK);
	/* fit bounding box to all children */
	for (tnode = ntree->nodes.first; tnode; tnode = tnode->next) {
		if (tnode->parent != node)
			continue;
		
		/* add margin to node rect */
		noderect = tnode->totr;
		noderect.xmin -= margin;
		noderect.xmax += margin;
		noderect.ymin -= margin;
		noderect.ymax += margin;
		
		/* first child initializes frame */
		if (bbinit) {
			bbinit = 0;
			rect = noderect;
			data->flag &= ~NODE_FRAME_RESIZEABLE;
		}
		else
			BLI_rctf_union(&rect, &noderect);
	}
	
	/* now adjust the frame size from view-space bounding box */
	node_from_view(node, rect.xmin, rect.ymax, &node->offsetx, &node->offsety);
	node_from_view(node, rect.xmax, rect.ymin, &xmax, &ymax);
	node->width = xmax - node->offsetx;
	node->height = -ymax + node->offsety;
	
	node->totr = rect;
}

static void node_draw_frame_label(bNodeTree *ntree, bNode *node, const float aspect)
{
	/* XXX font id is crap design */
	const int fontid = UI_GetStyle()->widgetlabel.uifont_id;
	NodeFrame *data = (NodeFrame *)node->storage;
	rctf *rct = &node->totr;
	int color_id = node_get_colorid(node);
	char label[MAX_NAME];
	/* XXX a bit hacky, should use separate align values for x and y */
	float width, ascender;
	float x, y;
	const int font_size = data->label_size / aspect;

	nodeLabel(ntree, node, label, sizeof(label));

	BLF_enable(fontid, BLF_ASPECT);
	BLF_aspect(fontid, aspect, aspect, 1.0f);
	BLF_size(fontid, MIN2(24, font_size), U.dpi); /* clamp otherwise it can suck up a LOT of memory */
	
	/* title color */
	UI_ThemeColorBlendShade(TH_TEXT, color_id, 0.4f, 10);

	width = BLF_width(fontid, label, sizeof(label));
	ascender = BLF_ascender(fontid);
	
	/* 'x' doesn't need aspect correction */
	x = BLI_rctf_cent_x(rct) - (0.5f * width);
	y = rct->ymax - (((NODE_DY / 4) / aspect) + (ascender * aspect));

	BLF_position(fontid, x, y, 0);
	BLF_draw(fontid, label, BLF_DRAW_STR_DUMMY_MAX);

	BLF_disable(fontid, BLF_ASPECT);
}

static void node_draw_frame(const bContext *C, ARegion *ar, SpaceNode *snode,
                            bNodeTree *ntree, bNode *node, bNodeInstanceKey UNUSED(key))
{
	rctf *rct = &node->totr;
	int color_id = node_get_colorid(node);
	unsigned char color[4];
	float alpha;
	
	/* skip if out of view */
	if (BLI_rctf_isect(&node->totr, &ar->v2d.cur, NULL) == false) {
		uiEndBlock(C, node->block);
		node->block = NULL;
		return;
	}

	UI_GetThemeColor4ubv(TH_NODE_FRAME, color);
	alpha = (float)(color[3]) / 255.0f;
	
	/* shadow */
	node_draw_shadow(snode, node, BASIS_RAD, alpha);
	
	/* body */
	if (node->flag & NODE_CUSTOM_COLOR)
		glColor4f(node->color[0], node->color[1], node->color[2], alpha);
	else
		UI_ThemeColor4(TH_NODE_FRAME);
	glEnable(GL_BLEND);
	uiSetRoundBox(UI_CNR_ALL);
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax, BASIS_RAD);
	glDisable(GL_BLEND);

	/* outline active and selected emphasis */
	if (node->flag & SELECT) {
		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);
		
		if (node->flag & NODE_ACTIVE)
			UI_ThemeColorShadeAlpha(TH_ACTIVE, 0, -40);
		else
			UI_ThemeColorShadeAlpha(TH_SELECT, 0, -40);
		uiSetRoundBox(UI_CNR_ALL);
		uiDrawBox(GL_LINE_LOOP,
		          rct->xmin, rct->ymin,
		          rct->xmax, rct->ymax, BASIS_RAD);
		
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}
	
	/* label */
	node_draw_frame_label(ntree, node, snode->aspect);
	
	UI_ThemeClearColor(color_id);
		
	uiEndBlock(C, node->block);
	uiDrawBlock(C, node->block);
	node->block = NULL;
}

static int node_resize_area_frame(bNode *node, int x, int y)
{
	const float size = 10.0f;
	NodeFrame *data = (NodeFrame *)node->storage;
	rctf totr = node->totr;
	int dir = 0;
	
	/* shrinking frame size is determined by child nodes */
	if (!(data->flag & NODE_FRAME_RESIZEABLE))
		return 0;
	
	if (x >= totr.xmax - size && x < totr.xmax && y >= totr.ymin && y < totr.ymax)
		dir |= NODE_RESIZE_RIGHT;
	if (x >= totr.xmin && x < totr.xmin + size && y >= totr.ymin && y < totr.ymax)
		dir |= NODE_RESIZE_LEFT;
	if (x >= totr.xmin && x < totr.xmax && y >= totr.ymax - size && y < totr.ymax)
		dir |= NODE_RESIZE_TOP;
	if (x >= totr.xmin && x < totr.xmax && y >= totr.ymin && y < totr.ymin + size)
		dir |= NODE_RESIZE_BOTTOM;
	
	return dir;
}

static void node_buts_frame_ex(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "label_size", 0, IFACE_("Label Size"), ICON_NONE);
	uiItemR(layout, ptr, "shrink", 0, IFACE_("Shrink"), ICON_NONE);
}


#define NODE_REROUTE_SIZE   8.0f

static void node_draw_reroute_prepare(const bContext *UNUSED(C), bNodeTree *UNUSED(ntree), bNode *node)
{
	bNodeSocket *nsock;
	float locx, locy;
	float size = NODE_REROUTE_SIZE;
	
	/* get "global" coords */
	node_to_view(node, 0.0f, 0.0f, &locx, &locy);
	
	/* reroute node has exactly one input and one output, both in the same place */
	nsock = node->outputs.first;
	nsock->locx = locx;
	nsock->locy = locy;

	nsock = node->inputs.first;
	nsock->locx = locx;
	nsock->locy = locy;

	node->width = size * 2;
	node->totr.xmin = locx - size;
	node->totr.xmax = locx + size;
	node->totr.ymax = locy + size;
	node->totr.ymin = locy - size;
}

static void node_draw_reroute(const bContext *C, ARegion *ar, SpaceNode *UNUSED(snode),
                              bNodeTree *ntree, bNode *node, bNodeInstanceKey UNUSED(key))
{
	bNodeSocket *sock;
	char showname[128]; /* 128 used below */
	rctf *rct = &node->totr;

#if 0   /* UNUSED */
	float size = NODE_REROUTE_SIZE;
#endif
	float socket_size = NODE_SOCKSIZE;

	/* skip if out of view */
	if (node->totr.xmax < ar->v2d.cur.xmin || node->totr.xmin > ar->v2d.cur.xmax ||
	    node->totr.ymax < ar->v2d.cur.ymin || node->totr.ymin > ar->v2d.cur.ymax)
	{
		uiEndBlock(C, node->block);
		node->block = NULL;
		return;
	}

	/* XXX only kept for debugging
	 * selection state is indicated by socket outline below!
	 */
#if 0
	/* body */
	uiSetRoundBox(UI_CNR_ALL);
	UI_ThemeColor4(TH_NODE);
	glEnable(GL_BLEND);
	uiRoundBox(rct->xmin, rct->ymin, rct->xmax, rct->ymax, size);
	glDisable(GL_BLEND);

	/* outline active and selected emphasis */
	if (node->flag & SELECT) {
		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);
		/* using different shades of TH_TEXT_HI for the empasis, like triangle */
		if (node->flag & NODE_ACTIVE)
			UI_ThemeColorShadeAlpha(TH_TEXT_HI, 0, -40);
		else
			UI_ThemeColorShadeAlpha(TH_TEXT_HI, -20, -120);
		uiDrawBox(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, size);

		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}
#endif

	if (node->label[0] != '\0') {
		/* draw title (node label) */
		BLI_strncpy(showname, node->label, sizeof(showname));
		uiDefBut(node->block, LABEL, 0, showname,
		         (int)(rct->xmin - NODE_DYS), (int)(rct->ymax),
		         (short)512, (short)NODE_DY,
		         NULL, 0, 0, 0, 0, NULL);
	}

	/* only draw input socket. as they all are placed on the same position.
	 * highlight also if node itself is selected, since we don't display the node body separately!
	 */
	for (sock = node->inputs.first; sock; sock = sock->next) {
		node_socket_circle_draw(C, ntree, node, sock, socket_size, (sock->flag & SELECT) || (node->flag & SELECT));
	}

	uiEndBlock(C, node->block);
	uiDrawBlock(C, node->block);
	node->block = NULL;
}

/* Special tweak area for reroute node.
 * Since this node is quite small, we use a larger tweak area for grabbing than for selection.
 */
static int node_tweak_area_reroute(bNode *node, int x, int y)
{
	/* square of tweak radius */
	static const float tweak_radius_sq = 576;  /* 24 * 24 */
	
	bNodeSocket *sock = node->inputs.first;
	float dx = sock->locx - x;
	float dy = sock->locy - y;
	return (dx * dx + dy * dy <= tweak_radius_sq);
}

static void node_common_set_butfunc(bNodeType *ntype)
{
	switch (ntype->type) {
		case NODE_GROUP:
			ntype->draw_buttons = node_draw_buttons_group;
			break;
		case NODE_FRAME:
			ntype->draw_nodetype = node_draw_frame;
			ntype->draw_nodetype_prepare = node_draw_frame_prepare;
			ntype->draw_buttons_ex = node_buts_frame_ex;
			ntype->resize_area_func = node_resize_area_frame;
			break;
		case NODE_REROUTE:
			ntype->draw_nodetype = node_draw_reroute;
			ntype->draw_nodetype_prepare = node_draw_reroute_prepare;
			ntype->tweak_area_func = node_tweak_area_reroute;
			break;
	}
}

/* ****************** BUTTON CALLBACKS FOR SHADER NODES ***************** */

static void node_buts_image_user(uiLayout *layout, bContext *C, PointerRNA *ptr,
                                 PointerRNA *imaptr, PointerRNA *iuserptr)
{
	uiLayout *col;
	int source;

	if (!imaptr->data)
		return;

	col = uiLayoutColumn(layout, false);
	
	uiItemR(col, imaptr, "source", 0, "", ICON_NONE);
	
	source = RNA_enum_get(imaptr, "source");

	if (source == IMA_SRC_SEQUENCE) {
		/* don't use iuser->framenr directly because it may not be updated if auto-refresh is off */
		Scene *scene = CTX_data_scene(C);
		ImageUser *iuser = iuserptr->data;
		/* Image *ima = imaptr->data; */  /* UNUSED */

		char numstr[32];
		const int framenr = BKE_image_user_frame_get(iuser, CFRA, 0, NULL);
		BLI_snprintf(numstr, sizeof(numstr), IFACE_("Frame: %d"), framenr);
		uiItemL(layout, numstr, ICON_NONE);
	}

	if (ELEM(source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
		col = uiLayoutColumn(layout, true);
		uiItemR(col, ptr, "frame_duration", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "frame_start", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "frame_offset", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "use_cyclic", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "use_auto_refresh", 0, NULL, ICON_NONE);
	}

	col = uiLayoutColumn(layout, false);

	if (RNA_enum_get(imaptr, "type") == IMA_TYPE_MULTILAYER)
		uiItemR(col, ptr, "layer", 0, NULL, ICON_NONE);
}

static void node_shader_buts_material(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;
	uiLayout *col;
	
	uiTemplateID(layout, C, ptr, "material", "MATERIAL_OT_new", NULL, NULL);
	
	if (!node->id) return;
	
	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "use_diffuse", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "use_specular", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "invert_normal", 0, NULL, ICON_NONE);
}

static void node_shader_buts_mapping(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row, *col, *sub;

	uiItemR(layout, ptr, "vector_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

	row = uiLayoutRow(layout, false);

	col = uiLayoutColumn(row, true);
	uiItemL(col, IFACE_("Location:"), ICON_NONE);
	uiItemR(col, ptr, "translation", 0, "", ICON_NONE);

	col = uiLayoutColumn(row, true);
	uiItemL(col, IFACE_("Rotation:"), ICON_NONE);
	uiItemR(col, ptr, "rotation", 0, "", ICON_NONE);

	col = uiLayoutColumn(row, true);
	uiItemL(col, IFACE_("Scale:"), ICON_NONE);
	uiItemR(col, ptr, "scale", 0, "", ICON_NONE);

	row = uiLayoutRow(layout, false);

	col = uiLayoutColumn(row, true);
	uiItemR(col, ptr, "use_min", 0, IFACE_("Min"), ICON_NONE);
	sub = uiLayoutColumn(col, true);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_min"));
	uiItemR(sub, ptr, "min", 0, "", ICON_NONE);

	col = uiLayoutColumn(row, true);
	uiItemR(col, ptr, "use_max", 0, IFACE_("Max"), ICON_NONE);
	sub = uiLayoutColumn(col, true);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_max"));
	uiItemR(sub, ptr, "max", 0, "", ICON_NONE);
}

static void node_shader_buts_vect_math(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{ 
	uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static void node_shader_buts_vect_transform(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{ 
	uiItemR(layout, ptr, "vector_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
	uiItemR(layout, ptr, "convert_from", 0, "", ICON_NONE);
	uiItemR(layout, ptr, "convert_to", 0, "", ICON_NONE);
}

static void node_shader_buts_geometry(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	PointerRNA obptr = CTX_data_pointer_get(C, "active_object");
	uiLayout *col;

	col = uiLayoutColumn(layout, false);

	if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
		PointerRNA dataptr = RNA_pointer_get(&obptr, "data");

		uiItemPointerR(col, ptr, "uv_layer", &dataptr, "uv_textures", "", ICON_NONE);
		uiItemPointerR(col, ptr, "color_layer", &dataptr, "vertex_colors", "", ICON_NONE);
	}
	else {
		uiItemR(col, ptr, "uv_layer", 0, IFACE_("UV"), ICON_NONE);
		uiItemR(col, ptr, "color_layer", 0, IFACE_("VCol"), ICON_NONE);
	}
}

static void node_shader_buts_lamp(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "lamp_object", 0, IFACE_("Lamp Object"), ICON_NONE);
}

static void node_shader_buts_attribute(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "attribute_name", 0, IFACE_("Name"), ICON_NONE);
}

static void node_shader_buts_wireframe(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "use_pixel_size", 0, NULL, 0);
}

static void node_shader_buts_tex_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	PointerRNA imaptr = RNA_pointer_get(ptr, "image");
	PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");

	uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
	uiTemplateID(layout, C, ptr, "image", NULL, "IMAGE_OT_open", NULL);
	uiItemR(layout, ptr, "color_space", 0, "", ICON_NONE);
	uiItemR(layout, ptr, "projection", 0, "", ICON_NONE);
	uiItemR(layout, ptr, "interpolation", 0, "", ICON_NONE);

	if (RNA_enum_get(ptr, "projection") == SHD_PROJ_BOX) {
		uiItemR(layout, ptr, "projection_blend", 0, "Blend", ICON_NONE);
	}

	/* note: image user properties used directly here, unlike compositor image node,
	 * which redefines them in the node struct RNA to get proper updates.
	 */
	node_buts_image_user(layout, C, &iuserptr, &imaptr, &iuserptr);
}

static void node_shader_buts_tex_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");
	uiTemplateImage(layout, C, ptr, "image", &iuserptr, 0);
}

static void node_shader_buts_tex_environment(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	PointerRNA imaptr = RNA_pointer_get(ptr, "image");
	PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");

	uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
	uiTemplateID(layout, C, ptr, "image", NULL, "IMAGE_OT_open", NULL);
	uiItemR(layout, ptr, "color_space", 0, "", ICON_NONE);
	uiItemR(layout, ptr, "projection", 0, "", ICON_NONE);

	node_buts_image_user(layout, C, &iuserptr, &imaptr, &iuserptr);
}

static void node_shader_buts_tex_sky(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiItemR(layout, ptr, "sky_type", 0, "", ICON_NONE);
	uiItemR(layout, ptr, "sun_direction", 0, "", ICON_NONE);
	uiItemR(layout, ptr, "turbidity", 0, NULL, ICON_NONE);

	if (RNA_enum_get(ptr, "sky_type") == SHD_SKY_NEW)
		uiItemR(layout, ptr, "ground_albedo", 0, NULL, ICON_NONE);
}

static void node_shader_buts_tex_gradient(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "gradient_type", 0, "", ICON_NONE);
}

static void node_shader_buts_tex_magic(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "turbulence_depth", 0, NULL, ICON_NONE);
}

static void node_shader_buts_tex_brick(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "offset", UI_ITEM_R_SLIDER, IFACE_("Offset"), ICON_NONE);
	uiItemR(col, ptr, "offset_frequency", 0, IFACE_("Frequency"), ICON_NONE);
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "squash", 0, IFACE_("Squash"), ICON_NONE);
	uiItemR(col, ptr, "squash_frequency", 0, IFACE_("Frequency"), ICON_NONE);
}

static void node_shader_buts_tex_wave(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "wave_type", 0, "", ICON_NONE);
}

static void node_shader_buts_tex_musgrave(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "musgrave_type", 0, "", ICON_NONE);
}

static void node_shader_buts_tex_voronoi(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "coloring", 0, "", ICON_NONE);
}

static void node_shader_buts_tex_coord(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "from_dupli", 0, NULL, 0);
}

static void node_shader_buts_bump(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "invert", 0, NULL, 0);
}

static void node_shader_buts_uvmap(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, ptr, "from_dupli", 0, NULL, 0);

	if (!RNA_boolean_get(ptr, "from_dupli")) {
		PointerRNA obptr = CTX_data_pointer_get(C, "active_object");

		if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
			PointerRNA dataptr = RNA_pointer_get(&obptr, "data");
			uiItemPointerR(layout, ptr, "uv_map", &dataptr, "uv_textures", "", ICON_NONE);
		}
	}
}

static void node_shader_buts_normal_map(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemR(layout, ptr, "space", 0, "", 0);

	if (RNA_enum_get(ptr, "space") == SHD_NORMAL_MAP_TANGENT) {
		PointerRNA obptr = CTX_data_pointer_get(C, "active_object");

		if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
			PointerRNA dataptr = RNA_pointer_get(&obptr, "data");
			uiItemPointerR(layout, ptr, "uv_map", &dataptr, "uv_textures", "", ICON_NONE);
		}
		else
			uiItemR(layout, ptr, "uv_map", 0, "", 0);
	}
}

static void node_shader_buts_tangent(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *split, *row;

	split = uiLayoutSplit(layout, 0.0f, false);

	uiItemR(split, ptr, "direction_type", 0, "", 0);

	row = uiLayoutRow(split, false);

	if (RNA_enum_get(ptr, "direction_type") == SHD_TANGENT_UVMAP) {
		PointerRNA obptr = CTX_data_pointer_get(C, "active_object");

		if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
			PointerRNA dataptr = RNA_pointer_get(&obptr, "data");
			uiItemPointerR(row, ptr, "uv_map", &dataptr, "uv_textures", "", ICON_NONE);
		}
		else
			uiItemR(row, ptr, "uv_map", 0, "", 0);
	}
	else
		uiItemR(row, ptr, "axis", UI_ITEM_R_EXPAND, NULL, 0);
}

static void node_shader_buts_glossy(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "distribution", 0, "", ICON_NONE);
}

static void node_shader_buts_subsurface(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	/* SSS does not work on GPU yet */
	PointerRNA scene = CTX_data_pointer_get(C, "scene");
	if (scene.data) {
		PointerRNA cscene = RNA_pointer_get(&scene, "cycles");
		if (cscene.data && (RNA_enum_get(&cscene, "device") == 1 && U.compute_device_type != 0))
			uiItemL(layout, IFACE_("SSS not supported on GPU"), ICON_ERROR);
	}

	uiItemR(layout, ptr, "falloff", 0, "", ICON_NONE);
}


static void node_shader_buts_volume(uiLayout *layout, bContext *C, PointerRNA *UNUSED(ptr))
{
	/* Volume does not work on GPU yet */
	PointerRNA scene = CTX_data_pointer_get(C, "scene");
	if (scene.data) {
		PointerRNA cscene = RNA_pointer_get(&scene, "cycles");

		if (cscene.data && (RNA_enum_get(&cscene, "device") == 1 && U.compute_device_type != 0))
			uiItemL(layout, IFACE_("Volumes not supported on GPU"), ICON_ERROR);
	}
}

static void node_shader_buts_toon(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "component", 0, "", ICON_NONE);
}

static void node_shader_buts_hair(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "component", 0, "", ICON_NONE);
}

static void node_shader_buts_script(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row;

	row = uiLayoutRow(layout, false);
	uiItemR(row, ptr, "mode", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

	row = uiLayoutRow(layout, true);

	if (RNA_enum_get(ptr, "mode") == NODE_SCRIPT_INTERNAL)
		uiItemR(row, ptr, "script", 0, "", ICON_NONE);
	else
		uiItemR(row, ptr, "filepath", 0, "", ICON_NONE);

	uiItemO(row, "", ICON_FILE_REFRESH, "node.shader_script_update");
}

static void node_shader_buts_script_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiItemS(layout);

	node_shader_buts_script(layout, C, ptr);

#if 0  /* not implemented yet */
	if (RNA_enum_get(ptr, "mode") == NODE_SCRIPT_EXTERNAL)
		uiItemR(layout, ptr, "use_auto_update", 0, NULL, ICON_NONE);
#endif
}

/* only once called */
static void node_shader_set_butfunc(bNodeType *ntype)
{
	switch (ntype->type) {
		case SH_NODE_MATERIAL:
		case SH_NODE_MATERIAL_EXT:
			ntype->draw_buttons = node_shader_buts_material;
			break;
		case SH_NODE_TEXTURE:
			ntype->draw_buttons = node_buts_texture;
			break;
		case SH_NODE_NORMAL:
			ntype->draw_buttons = node_buts_normal;
			break;
		case SH_NODE_CURVE_VEC:
			ntype->draw_buttons = node_buts_curvevec;
			break;
		case SH_NODE_CURVE_RGB:
			ntype->draw_buttons = node_buts_curvecol;
			break;
		case SH_NODE_MAPPING:
			ntype->draw_buttons = node_shader_buts_mapping;
			break;
		case SH_NODE_VALUE:
			ntype->draw_buttons = node_buts_value;
			break;
		case SH_NODE_RGB:
			ntype->draw_buttons = node_buts_rgb;
			break;
		case SH_NODE_MIX_RGB:
			ntype->draw_buttons = node_buts_mix_rgb;
			break;
		case SH_NODE_VALTORGB:
			ntype->draw_buttons = node_buts_colorramp;
			break;
		case SH_NODE_MATH: 
			ntype->draw_buttons = node_buts_math;
			break; 
		case SH_NODE_VECT_MATH: 
			ntype->draw_buttons = node_shader_buts_vect_math;
			break; 
		case SH_NODE_VECT_TRANSFORM: 
			ntype->draw_buttons = node_shader_buts_vect_transform;
			break; 
		case SH_NODE_GEOMETRY:
			ntype->draw_buttons = node_shader_buts_geometry;
			break;
		case SH_NODE_LAMP:
			ntype->draw_buttons = node_shader_buts_lamp;
			break;
		case SH_NODE_ATTRIBUTE:
			ntype->draw_buttons = node_shader_buts_attribute;
			break;
		case SH_NODE_WIREFRAME:
			ntype->draw_buttons = node_shader_buts_wireframe;
			break;
		case SH_NODE_TEX_SKY:
			ntype->draw_buttons = node_shader_buts_tex_sky;
			break;
		case SH_NODE_TEX_IMAGE:
			ntype->draw_buttons = node_shader_buts_tex_image;
			ntype->draw_buttons_ex = node_shader_buts_tex_image_ex;
			break;
		case SH_NODE_TEX_ENVIRONMENT:
			ntype->draw_buttons = node_shader_buts_tex_environment;
			break;
		case SH_NODE_TEX_GRADIENT:
			ntype->draw_buttons = node_shader_buts_tex_gradient;
			break;
		case SH_NODE_TEX_MAGIC:
			ntype->draw_buttons = node_shader_buts_tex_magic;
			break;
		case SH_NODE_TEX_BRICK:
			ntype->draw_buttons = node_shader_buts_tex_brick;
			break;
		case SH_NODE_TEX_WAVE:
			ntype->draw_buttons = node_shader_buts_tex_wave;
			break;
		case SH_NODE_TEX_MUSGRAVE:
			ntype->draw_buttons = node_shader_buts_tex_musgrave;
			break;
		case SH_NODE_TEX_VORONOI:
			ntype->draw_buttons = node_shader_buts_tex_voronoi;
			break;
		case SH_NODE_TEX_COORD:
			ntype->draw_buttons = node_shader_buts_tex_coord;
			break;
		case SH_NODE_BUMP:
			ntype->draw_buttons = node_shader_buts_bump;
			break;
		case SH_NODE_NORMAL_MAP:
			ntype->draw_buttons = node_shader_buts_normal_map;
			break;
		case SH_NODE_TANGENT:
			ntype->draw_buttons = node_shader_buts_tangent;
			break;
		case SH_NODE_BSDF_GLOSSY:
		case SH_NODE_BSDF_GLASS:
		case SH_NODE_BSDF_REFRACTION:
			ntype->draw_buttons = node_shader_buts_glossy;
			break;
		case SH_NODE_SUBSURFACE_SCATTERING:
			ntype->draw_buttons = node_shader_buts_subsurface;
			break;
		case SH_NODE_VOLUME_SCATTER:
			ntype->draw_buttons = node_shader_buts_volume;
			break;
		case SH_NODE_VOLUME_ABSORPTION:
			ntype->draw_buttons = node_shader_buts_volume;
			break;
		case SH_NODE_BSDF_TOON:
			ntype->draw_buttons = node_shader_buts_toon;
			break;
		case SH_NODE_BSDF_HAIR:
			ntype->draw_buttons = node_shader_buts_hair;
			break;
		case SH_NODE_SCRIPT:
			ntype->draw_buttons = node_shader_buts_script;
			ntype->draw_buttons_ex = node_shader_buts_script_ex;
			break;
		case SH_NODE_UVMAP:
			ntype->draw_buttons = node_shader_buts_uvmap;
			break;
	}
}

/* ****************** BUTTON CALLBACKS FOR COMPOSITE NODES ***************** */

static void node_composit_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;
	PointerRNA imaptr, iuserptr;
	
	RNA_pointer_create((ID *)ptr->id.data, &RNA_ImageUser, node->storage, &iuserptr);
	uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
	uiTemplateID(layout, C, ptr, "image", NULL, "IMAGE_OT_open", NULL);
	if (!node->id) return;
	
	imaptr = RNA_pointer_get(ptr, "image");

	node_buts_image_user(layout, C, ptr, &imaptr, &iuserptr);
}

static void node_composit_buts_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;
	PointerRNA iuserptr;

	RNA_pointer_create((ID *)ptr->id.data, &RNA_ImageUser, node->storage, &iuserptr);
	uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
	uiTemplateImage(layout, C, ptr, "image", &iuserptr, 0);
}

static void node_composit_buts_renderlayers(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;
	uiLayout *col, *row;
	PointerRNA op_ptr;
	PointerRNA scn_ptr;
	PropertyRNA *prop;
	const char *layer_name;
	char scene_name[MAX_ID_NAME - 2];
	wmOperatorType *ot = WM_operatortype_find("RENDER_OT_render", 1);

	BLI_assert(ot != 0);

	uiTemplateID(layout, C, ptr, "scene", NULL, NULL, NULL);
	
	if (!node->id) return;

	col = uiLayoutColumn(layout, false);
	row = uiLayoutRow(col, true);
	uiItemR(row, ptr, "layer", 0, "", ICON_NONE);
	
	prop = RNA_struct_find_property(ptr, "layer");
	if (!(RNA_property_enum_identifier(C, ptr, prop, RNA_property_enum_get(ptr, prop), &layer_name)))
		return;
	
	scn_ptr = RNA_pointer_get(ptr, "scene");
	RNA_string_get(&scn_ptr, "name", scene_name);
	
	WM_operator_properties_create_ptr(&op_ptr, ot);
	RNA_string_set(&op_ptr, "layer", layer_name);
	RNA_string_set(&op_ptr, "scene", scene_name);
	uiItemFullO_ptr(row, ot, "", ICON_RENDER_STILL, op_ptr.data, WM_OP_INVOKE_DEFAULT, 0);

}


static void node_composit_buts_blur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col, *row;
	int reference;
	int filter;
	
	col = uiLayoutColumn(layout, false);
	filter = RNA_enum_get(ptr, "filter_type");
	reference = RNA_boolean_get(ptr, "use_variable_size");

	uiItemR(col, ptr, "filter_type", 0, "", ICON_NONE);
	if (filter != R_FILTER_FAST_GAUSS) {
		uiItemR(col, ptr, "use_variable_size", 0, NULL, ICON_NONE);
		if (!reference) {
			uiItemR(col, ptr, "use_bokeh", 0, NULL, ICON_NONE);
		}
		uiItemR(col, ptr, "use_gamma_correction", 0, NULL, ICON_NONE);
	}
	
	uiItemR(col, ptr, "use_relative", 0, NULL, ICON_NONE);
	
	if (RNA_boolean_get(ptr, "use_relative")) {
		uiItemL(col, IFACE_("Aspect Correction"), ICON_NONE);
		row = uiLayoutRow(layout, true);
		uiItemR(row, ptr, "aspect_correction", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
		
		col = uiLayoutColumn(layout, true);
		uiItemR(col, ptr, "factor_x", 0, IFACE_("X"), ICON_NONE);
		uiItemR(col, ptr, "factor_y", 0, IFACE_("Y"), ICON_NONE);
	}
	else {
		col = uiLayoutColumn(layout, true);
		uiItemR(col, ptr, "size_x", 0, IFACE_("X"), ICON_NONE);
		uiItemR(col, ptr, "size_y", 0, IFACE_("Y"), ICON_NONE);
	}
}

static void node_composit_buts_dblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	uiItemR(layout, ptr, "iterations", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "use_wrap", 0, NULL, ICON_NONE);
	
	col = uiLayoutColumn(layout, true);
	uiItemL(col, IFACE_("Center:"), ICON_NONE);
	uiItemR(col, ptr, "center_x", 0, IFACE_("X"), ICON_NONE);
	uiItemR(col, ptr, "center_y", 0, IFACE_("Y"), ICON_NONE);
	
	uiItemS(layout);
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "distance", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "angle", 0, NULL, ICON_NONE);
	
	uiItemS(layout);
	
	uiItemR(layout, ptr, "spin", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "zoom", 0, NULL, ICON_NONE);
}

static void node_composit_buts_bilateralblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col;
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "iterations", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "sigma_color", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "sigma_space", 0, NULL, ICON_NONE);
}

static void node_composit_buts_defocus(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiLayout *sub, *col;
	
	col = uiLayoutColumn(layout, false);
	uiItemL(col, IFACE_("Bokeh Type:"), ICON_NONE);
	uiItemR(col, ptr, "bokeh", 0, "", ICON_NONE);
	uiItemR(col, ptr, "angle", 0, NULL, ICON_NONE);

	uiItemR(layout, ptr, "use_gamma_correction", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, false);
	uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_zbuffer") == true);
	uiItemR(col, ptr, "f_stop", 0, NULL, ICON_NONE);

	uiItemR(layout, ptr, "blur_max", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "threshold", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "use_preview", 0, NULL, ICON_NONE);

	uiTemplateID(layout, C, ptr, "scene", NULL, NULL, NULL);

	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "use_zbuffer", 0, NULL, ICON_NONE);
	sub = uiLayoutColumn(col, false);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_zbuffer") == false);
	uiItemR(sub, ptr, "z_scale", 0, NULL, ICON_NONE);
}

/* qdn: glare node */
static void node_composit_buts_glare(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiItemR(layout, ptr, "glare_type", 0, "", ICON_NONE);
	uiItemR(layout, ptr, "quality", 0, "", ICON_NONE);

	if (RNA_enum_get(ptr, "glare_type") != 1) {
		uiItemR(layout, ptr, "iterations", 0, NULL, ICON_NONE);
	
		if (RNA_enum_get(ptr, "glare_type") != 0)
			uiItemR(layout, ptr, "color_modulation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	}
	
	uiItemR(layout, ptr, "mix", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "threshold", 0, NULL, ICON_NONE);

	if (RNA_enum_get(ptr, "glare_type") == 2) {
		uiItemR(layout, ptr, "streaks", 0, NULL, ICON_NONE);
		uiItemR(layout, ptr, "angle_offset", 0, NULL, ICON_NONE);
	}
	if (RNA_enum_get(ptr, "glare_type") == 0 || RNA_enum_get(ptr, "glare_type") == 2) {
		uiItemR(layout, ptr, "fade", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		
		if (RNA_enum_get(ptr, "glare_type") == 0)
			uiItemR(layout, ptr, "use_rotate_45", 0, NULL, ICON_NONE);
	}
	if (RNA_enum_get(ptr, "glare_type") == 1) {
		uiItemR(layout, ptr, "size", 0, NULL, ICON_NONE);
	}
}

static void node_composit_buts_tonemap(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col;

	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "tonemap_type", 0, "", ICON_NONE);
	if (RNA_enum_get(ptr, "tonemap_type") == 0) {
		uiItemR(col, ptr, "key", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		uiItemR(col, ptr, "offset", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "gamma", 0, NULL, ICON_NONE);
	}
	else {
		uiItemR(col, ptr, "intensity", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "contrast", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		uiItemR(col, ptr, "adaptation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		uiItemR(col, ptr, "correction", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	}
}

static void node_composit_buts_lensdist(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;

	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "use_projector", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(col, false);
	uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_projector") == false);
	uiItemR(col, ptr, "use_jitter", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "use_fit", 0, NULL, ICON_NONE);
}

static void node_composit_buts_vecblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "samples", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "factor", 0, IFACE_("Blur"), ICON_NONE);
	
	col = uiLayoutColumn(layout, true);
	uiItemL(col, IFACE_("Speed:"), ICON_NONE);
	uiItemR(col, ptr, "speed_min", 0, IFACE_("Min"), ICON_NONE);
	uiItemR(col, ptr, "speed_max", 0, IFACE_("Max"), ICON_NONE);

	uiItemR(layout, ptr, "use_curved", 0, NULL, ICON_NONE);
}

static void node_composit_buts_filter(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "filter_type", 0, "", ICON_NONE);
}

static void node_composit_buts_flip(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "axis", 0, "", ICON_NONE);
}

static void node_composit_buts_crop(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;

	uiItemR(layout, ptr, "use_crop_size", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "relative", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, true);
	if (RNA_boolean_get(ptr, "relative")) {
		uiItemR(col, ptr, "rel_min_x", 0, IFACE_("Left"), ICON_NONE);
		uiItemR(col, ptr, "rel_max_x", 0, IFACE_("Right"), ICON_NONE);
		uiItemR(col, ptr, "rel_min_y", 0, IFACE_("Up"), ICON_NONE);
		uiItemR(col, ptr, "rel_max_y", 0, IFACE_("Down"), ICON_NONE);
	}
	else {
		uiItemR(col, ptr, "min_x", 0, IFACE_("Left"), ICON_NONE);
		uiItemR(col, ptr, "max_x", 0, IFACE_("Right"), ICON_NONE);
		uiItemR(col, ptr, "min_y", 0, IFACE_("Up"), ICON_NONE);
		uiItemR(col, ptr, "max_y", 0, IFACE_("Down"), ICON_NONE);
	}
}

static void node_composit_buts_splitviewer(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row, *col;
	
	col = uiLayoutColumn(layout, false);
	row = uiLayoutRow(col, false);
	uiItemR(row, ptr, "axis", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
	uiItemR(col, ptr, "factor", 0, NULL, ICON_NONE);
}

static void node_composit_buts_double_edge_mask(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;

	col = uiLayoutColumn(layout, false);

	uiItemL(col, IFACE_("Inner Edge:"), ICON_NONE);
	uiItemR(col, ptr, "inner_mode", 0, "", ICON_NONE);
	uiItemL(col, IFACE_("Buffer Edge:"), ICON_NONE);
	uiItemR(col, ptr, "edge_mode", 0, "", ICON_NONE);
}

static void node_composit_buts_map_range(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;

	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "use_clamp", 0, NULL, ICON_NONE);
}

static void node_composit_buts_map_value(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *sub, *col;
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "offset", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "size", 0, NULL, ICON_NONE);
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "use_min", 0, NULL, ICON_NONE);
	sub = uiLayoutColumn(col, false);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_min"));
	uiItemR(sub, ptr, "min", 0, "", ICON_NONE);
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "use_max", 0, NULL, ICON_NONE);
	sub = uiLayoutColumn(col, false);
	uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_max"));
	uiItemR(sub, ptr, "max", 0, "", ICON_NONE);
}

static void node_composit_buts_alphaover(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col;
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "use_premultiply", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "premul", 0, NULL, ICON_NONE);
}

static void node_composit_buts_zcombine(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col;
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "use_alpha", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "use_antialias_z", 0, NULL, ICON_NONE);
}


static void node_composit_buts_hue_sat(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "color_hue", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "color_saturation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "color_value", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_dilateerode(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "distance", 0, NULL, ICON_NONE);
	switch (RNA_enum_get(ptr, "mode")) {
		case CMP_NODE_DILATEERODE_DISTANCE_THRESH:
			uiItemR(layout, ptr, "edge", 0, NULL, ICON_NONE);
			break;
		case CMP_NODE_DILATEERODE_DISTANCE_FEATHER:
			uiItemR(layout, ptr, "falloff", 0, NULL, ICON_NONE);
			break;
	}
}

static void node_composit_buts_inpaint(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "distance", 0, NULL, ICON_NONE);
}

static void node_composit_buts_despeckle(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;

	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "threshold", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "threshold_neighbor", 0, NULL, ICON_NONE);
}

static void node_composit_buts_diff_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "tolerance", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "falloff", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_distance_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col, *row;
	
	col = uiLayoutColumn(layout, true);

	uiItemL(layout, IFACE_("Color Space:"), ICON_NONE);
	row = uiLayoutRow(layout, false);
	uiItemR(row, ptr, "channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

	uiItemR(col, ptr, "tolerance", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "falloff", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_color_spill(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row, *col;
	
	uiItemL(layout, IFACE_("Despill Channel:"), ICON_NONE);
	row = uiLayoutRow(layout, false);
	uiItemR(row, ptr, "channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "limit_method", 0, NULL, ICON_NONE);

	if (RNA_enum_get(ptr, "limit_method") == 0) {
		uiItemL(col, IFACE_("Limiting Channel:"), ICON_NONE);
		row = uiLayoutRow(col, false);
		uiItemR(row, ptr, "limit_channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
	}

	uiItemR(col, ptr, "ratio", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "use_unspill", 0, NULL, ICON_NONE);
	if (RNA_boolean_get(ptr, "use_unspill") == true) {
		uiItemR(col, ptr, "unspill_red", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		uiItemR(col, ptr, "unspill_green", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		uiItemR(col, ptr, "unspill_blue", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	}
}

static void node_composit_buts_chroma_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "tolerance", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "threshold", 0, NULL, ICON_NONE);
	
	col = uiLayoutColumn(layout, true);
	/*uiItemR(col, ptr, "lift", UI_ITEM_R_SLIDER, NULL, ICON_NONE);  Removed for now */
	uiItemR(col, ptr, "gain", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	/*uiItemR(col, ptr, "shadow_adjust", UI_ITEM_R_SLIDER, NULL, ICON_NONE);  Removed for now*/
}

static void node_composit_buts_color_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "color_hue", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "color_saturation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "color_value", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_channel_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{	
	uiLayout *col, *row;

	uiItemL(layout, IFACE_("Color Space:"), ICON_NONE);
	row = uiLayoutRow(layout, false);
	uiItemR(row, ptr, "color_space", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, false);
	uiItemL(col, IFACE_("Key Channel:"), ICON_NONE);
	row = uiLayoutRow(col, false);
	uiItemR(row, ptr, "matte_channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, false);

	uiItemR(col, ptr, "limit_method", 0, NULL, ICON_NONE);
	if (RNA_enum_get(ptr, "limit_method") == 0) {
		uiItemL(col, IFACE_("Limiting Channel:"), ICON_NONE);
		row = uiLayoutRow(col, false);
		uiItemR(row, ptr, "limit_channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
	}

	uiItemR(col, ptr, "limit_max", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "limit_min", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_luma_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "limit_max", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(col, ptr, "limit_min", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_map_uv(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "alpha", 0, NULL, ICON_NONE);
}

static void node_composit_buts_id_mask(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "index", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "use_antialiasing", 0, NULL, ICON_NONE);
}

static void node_composit_buts_file_output(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	PointerRNA imfptr = RNA_pointer_get(ptr, "format");
	int multilayer = (RNA_enum_get(&imfptr, "file_format") == R_IMF_IMTYPE_MULTILAYER);
	
	if (multilayer)
		uiItemL(layout, IFACE_("Path:"), ICON_NONE);
	else
		uiItemL(layout, IFACE_("Base Path:"), ICON_NONE);
	uiItemR(layout, ptr, "base_path", 0, "", ICON_NONE);
}
static void node_composit_buts_file_output_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	PointerRNA imfptr = RNA_pointer_get(ptr, "format");
	PointerRNA active_input_ptr, op_ptr;
	uiLayout *row, *col;
	int active_index;
	int multilayer = (RNA_enum_get(&imfptr, "file_format") == R_IMF_IMTYPE_MULTILAYER);
	
	node_composit_buts_file_output(layout, C, ptr);
	uiTemplateImageSettings(layout, &imfptr, false);
	
	uiItemS(layout);
	
	uiItemO(layout, IFACE_("Add Input"), ICON_ZOOMIN, "NODE_OT_output_file_add_socket");
	
	row = uiLayoutRow(layout, false);
	col = uiLayoutColumn(row, true);
	
	active_index = RNA_int_get(ptr, "active_input_index");
	/* using different collection properties if multilayer format is enabled */
	if (multilayer) {
		uiTemplateList(col, C, "UI_UL_list", "file_output_node", ptr, "layer_slots", ptr, "active_input_index",
		               0, 0, 0, 0);
		RNA_property_collection_lookup_int(ptr, RNA_struct_find_property(ptr, "layer_slots"),
		                                   active_index, &active_input_ptr);
	}
	else {
		uiTemplateList(col, C, "UI_UL_list", "file_output_node", ptr, "file_slots", ptr, "active_input_index",
		               0, 0, 0, 0);
		RNA_property_collection_lookup_int(ptr, RNA_struct_find_property(ptr, "file_slots"),
		                                   active_index, &active_input_ptr);
	}
	/* XXX collection lookup does not return the ID part of the pointer, setting this manually here */
	active_input_ptr.id.data = ptr->id.data;
	
	col = uiLayoutColumn(row, true);
	op_ptr = uiItemFullO(col, "NODE_OT_output_file_move_active_socket", "",
	                     ICON_TRIA_UP, NULL, WM_OP_INVOKE_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&op_ptr, "direction", 1);
	op_ptr = uiItemFullO(col, "NODE_OT_output_file_move_active_socket", "",
	                     ICON_TRIA_DOWN, NULL, WM_OP_INVOKE_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&op_ptr, "direction", 2);
	
	if (active_input_ptr.data) {
		if (multilayer) {
			col = uiLayoutColumn(layout, true);
			
			uiItemL(col, IFACE_("Layer:"), ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &active_input_ptr, "name", 0, "", ICON_NONE);
			uiItemFullO(row, "NODE_OT_output_file_remove_active_socket", "",
			            ICON_X, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_R_ICON_ONLY);
		}
		else {
			col = uiLayoutColumn(layout, true);
			
			uiItemL(col, IFACE_("File Subpath:"), ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &active_input_ptr, "path", 0, "", ICON_NONE);
			uiItemFullO(row, "NODE_OT_output_file_remove_active_socket", "",
			            ICON_X, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_R_ICON_ONLY);
			
			/* format details for individual files */
			imfptr = RNA_pointer_get(&active_input_ptr, "format");
			
			col = uiLayoutColumn(layout, true);
			uiItemL(col, IFACE_("Format:"), ICON_NONE);
			uiItemR(col, &active_input_ptr, "use_node_format", 0, NULL, ICON_NONE);
			
			col = uiLayoutColumn(layout, false);
			uiLayoutSetActive(col, RNA_boolean_get(&active_input_ptr, "use_node_format") == false);
			uiTemplateImageSettings(col, &imfptr, false);
		}
	}
}

static void node_composit_buts_scale(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "space", 0, "", ICON_NONE);

	if (RNA_enum_get(ptr, "space") == CMP_SCALE_RENDERPERCENT) {
		uiLayout *row;
		uiItemR(layout, ptr, "frame_method", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
		row = uiLayoutRow(layout, true);
		uiItemR(row, ptr, "offset_x", 0, "X", ICON_NONE);
		uiItemR(row, ptr, "offset_y", 0, "Y", ICON_NONE);
	}
}

static void node_composit_buts_rotate(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "filter_type", 0, "", ICON_NONE);
}

static void node_composit_buts_invert(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col = uiLayoutColumn(layout, false);
	uiItemR(col, ptr, "invert_rgb", 0, NULL, ICON_NONE);
	uiItemR(col, ptr, "invert_alpha", 0, NULL, ICON_NONE);
}

static void node_composit_buts_premulkey(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "mapping", 0, "", ICON_NONE);
}

static void node_composit_buts_view_levels(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "channel", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
}

static void node_composit_buts_colorbalance(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *split, *col, *row;
	
	uiItemR(layout, ptr, "correction_method", 0, NULL, ICON_NONE);
	
	if (RNA_enum_get(ptr, "correction_method") == 0) {
	
		split = uiLayoutSplit(layout, 0.0f, false);
		col = uiLayoutColumn(split, false);
		uiTemplateColorPicker(col, ptr, "lift", 1, 1, 0, 1);
		row = uiLayoutRow(col, false);
		uiItemR(row, ptr, "lift", 0, NULL, ICON_NONE);
		
		col = uiLayoutColumn(split, false);
		uiTemplateColorPicker(col, ptr, "gamma", 1, 1, 1, 1);
		row = uiLayoutRow(col, false);
		uiItemR(row, ptr, "gamma", 0, NULL, ICON_NONE);
		
		col = uiLayoutColumn(split, false);
		uiTemplateColorPicker(col, ptr, "gain", 1, 1, 1, 1);
		row = uiLayoutRow(col, false);
		uiItemR(row, ptr, "gain", 0, NULL, ICON_NONE);

	}
	else {
		
		split = uiLayoutSplit(layout, 0.0f, false);
		col = uiLayoutColumn(split, false);
		uiTemplateColorPicker(col, ptr, "offset", 1, 1, 0, 1);
		row = uiLayoutRow(col, false);
		uiItemR(row, ptr, "offset", 0, NULL, ICON_NONE);
		
		col = uiLayoutColumn(split, false);
		uiTemplateColorPicker(col, ptr, "power", 1, 1, 0, 1);
		row = uiLayoutRow(col, false);
		uiItemR(row, ptr, "power", 0, NULL, ICON_NONE);
		
		col = uiLayoutColumn(split, false);
		uiTemplateColorPicker(col, ptr, "slope", 1, 1, 0, 1);
		row = uiLayoutRow(col, false);
		uiItemR(row, ptr, "slope", 0, NULL, ICON_NONE);
	}

}
static void node_composit_buts_colorbalance_ex(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "correction_method", 0, NULL, ICON_NONE);

	if (RNA_enum_get(ptr, "correction_method") == 0) {

		uiTemplateColorPicker(layout, ptr, "lift", 1, 1, 0, 1);
		uiItemR(layout, ptr, "lift", 0, NULL, ICON_NONE);

		uiTemplateColorPicker(layout, ptr, "gamma", 1, 1, 1, 1);
		uiItemR(layout, ptr, "gamma", 0, NULL, ICON_NONE);

		uiTemplateColorPicker(layout, ptr, "gain", 1, 1, 1, 1);
		uiItemR(layout, ptr, "gain", 0, NULL, ICON_NONE);
	}
	else {
		uiTemplateColorPicker(layout, ptr, "offset", 1, 1, 0, 1);
		uiItemR(layout, ptr, "offset", 0, NULL, ICON_NONE);

		uiTemplateColorPicker(layout, ptr, "power", 1, 1, 0, 1);
		uiItemR(layout, ptr, "power", 0, NULL, ICON_NONE);

		uiTemplateColorPicker(layout, ptr, "slope", 1, 1, 0, 1);
		uiItemR(layout, ptr, "slope", 0, NULL, ICON_NONE);
	}
}


static void node_composit_buts_huecorrect(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	bNode *node = ptr->data;
	CurveMapping *cumap = node->storage;

	if (_sample_col[0] != SAMPLE_FLT_ISNONE) {
		cumap->flag |= CUMA_DRAW_SAMPLE;
		copy_v3_v3(cumap->sample, _sample_col);
	}
	else {
		cumap->flag &= ~CUMA_DRAW_SAMPLE;
	}

	uiTemplateCurveMapping(layout, ptr, "mapping", 'h', 0, 0);
}

static void node_composit_buts_ycc(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{ 
	uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_composit_buts_movieclip(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiTemplateID(layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL);
}

static void node_composit_buts_movieclip_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;
	PointerRNA clipptr;

	uiTemplateID(layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL);

	if (!node->id)
		return;

	clipptr = RNA_pointer_get(ptr, "clip");

	uiTemplateColorspaceSettings(layout, &clipptr, "colorspace_settings");
}

static void node_composit_buts_stabilize2d(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;

	uiTemplateID(layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL);

	if (!node->id)
		return;

	uiItemR(layout, ptr, "filter_type", 0, "", ICON_NONE);
}

static void node_composit_buts_translate(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "use_relative", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "wrap_axis", 0, NULL, ICON_NONE);
}

static void node_composit_buts_transform(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "filter_type", 0, "", ICON_NONE);
}

static void node_composit_buts_moviedistortion(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;

	uiTemplateID(layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL);

	if (!node->id)
		return;

	uiItemR(layout, ptr, "distortion_type", 0, "", ICON_NONE);
}

static void node_composit_buts_colorcorrection(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row;
	
	row = uiLayoutRow(layout, false);
	uiItemR(row, ptr, "red", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "green", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "blue", 0, NULL, ICON_NONE);

	row = uiLayoutRow(layout, false);
	uiItemL(row, "", ICON_NONE);
	uiItemL(row, IFACE_("Saturation"), ICON_NONE);
	uiItemL(row, IFACE_("Contrast"), ICON_NONE);
	uiItemL(row, IFACE_("Gamma"), ICON_NONE);
	uiItemL(row, IFACE_("Gain"), ICON_NONE);
	uiItemL(row, IFACE_("Lift"), ICON_NONE);

	row = uiLayoutRow(layout, false);
	uiItemL(row, IFACE_("Master"), ICON_NONE);
	uiItemR(row, ptr, "master_saturation", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "master_contrast", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "master_gamma", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "master_gain", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "master_lift", UI_ITEM_R_SLIDER, "", ICON_NONE);

	row = uiLayoutRow(layout, false);
	uiItemL(row, IFACE_("Highlights"), ICON_NONE);
	uiItemR(row, ptr, "highlights_saturation", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "highlights_contrast", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "highlights_gamma", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "highlights_gain", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "highlights_lift", UI_ITEM_R_SLIDER, "", ICON_NONE);

	row = uiLayoutRow(layout, false);
	uiItemL(row, IFACE_("Midtones"), ICON_NONE);
	uiItemR(row, ptr, "midtones_saturation", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "midtones_contrast", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "midtones_gamma", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "midtones_gain", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "midtones_lift", UI_ITEM_R_SLIDER, "", ICON_NONE);

	row = uiLayoutRow(layout, false);
	uiItemL(row, IFACE_("Shadows"), ICON_NONE);
	uiItemR(row, ptr, "shadows_saturation", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "shadows_contrast", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "shadows_gamma", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "shadows_gain", UI_ITEM_R_SLIDER, "", ICON_NONE);
	uiItemR(row, ptr, "shadows_lift", UI_ITEM_R_SLIDER, "", ICON_NONE);

	row = uiLayoutRow(layout, false);
	uiItemR(row, ptr, "midtones_start", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "midtones_end", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_colorcorrection_ex(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row;
	
	row = uiLayoutRow(layout, false);
	uiItemR(row, ptr, "red", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "green", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "blue", 0, NULL, ICON_NONE);
	row = layout;
	uiItemL(row, IFACE_("Saturation"), ICON_NONE);
	uiItemR(row, ptr, "master_saturation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "highlights_saturation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "midtones_saturation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "shadows_saturation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

	uiItemL(row, IFACE_("Contrast"), ICON_NONE);
	uiItemR(row, ptr, "master_contrast", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "highlights_contrast", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "midtones_contrast", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "shadows_contrast", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

	uiItemL(row, IFACE_("Gamma"), ICON_NONE);
	uiItemR(row, ptr, "master_gamma", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "highlights_gamma", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "midtones_gamma", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "shadows_gamma", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

	uiItemL(row, IFACE_("Gain"), ICON_NONE);
	uiItemR(row, ptr, "master_gain", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "highlights_gain", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "midtones_gain", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "shadows_gain", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	
	uiItemL(row, IFACE_("Lift"), ICON_NONE);
	uiItemR(row, ptr, "master_lift", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "highlights_lift", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "midtones_lift", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "shadows_lift", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

	row = uiLayoutRow(layout, false);
	uiItemR(row, ptr, "midtones_start", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "midtones_end", 0, NULL, ICON_NONE);
}

static void node_composit_buts_switch(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "check", 0, NULL, ICON_NONE);
}

static void node_composit_buts_boxmask(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row;
	
	row = uiLayoutRow(layout, true);
	uiItemR(row, ptr, "x", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "y", 0, NULL, ICON_NONE);
	
	row = uiLayoutRow(layout, true);
	uiItemR(row, ptr, "width", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "height", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

	uiItemR(layout, ptr, "rotation", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "mask_type", 0, NULL, ICON_NONE);
}

static void node_composit_buts_bokehimage(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "flaps", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "angle", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "rounding", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(layout, ptr, "catadioptric", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(layout, ptr, "shift", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_bokehblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "use_variable_size", 0, NULL, ICON_NONE);
	// uiItemR(layout, ptr, "f_stop", 0, NULL, ICON_NONE);  // UNUSED
	uiItemR(layout, ptr, "blur_max", 0, NULL, ICON_NONE);
}

static void node_composit_backdrop_viewer(SpaceNode *snode, ImBuf *backdrop, bNode *node, int x, int y)
{
//	node_composit_backdrop_canvas(snode, backdrop, node, x, y);
	if (node->custom1 == 0) {
		const float backdropWidth = backdrop->x;
		const float backdropHeight = backdrop->y;
		const float cx  = x + snode->zoom * backdropWidth * node->custom3;
		const float cy = y + snode->zoom * backdropHeight * node->custom4;

		glColor3f(1.0, 1.0, 1.0);

		glBegin(GL_LINES);
		glVertex2f(cx - 25, cy - 25);
		glVertex2f(cx + 25, cy + 25);
		glVertex2f(cx + 25, cy - 25);
		glVertex2f(cx - 25, cy + 25);
		glEnd();
	}
}

static void node_composit_backdrop_boxmask(SpaceNode *snode, ImBuf *backdrop, bNode *node, int x, int y)
{
	NodeBoxMask *boxmask = node->storage;
	const float backdropWidth = backdrop->x;
	const float backdropHeight = backdrop->y;
	const float aspect = backdropWidth / backdropHeight;
	const float rad = -boxmask->rotation;
	const float cosine = cosf(rad);
	const float sine = sinf(rad);
	const float halveBoxWidth = backdropWidth * (boxmask->width / 2.0f);
	const float halveBoxHeight = backdropHeight * (boxmask->height / 2.0f) * aspect;

	float cx, cy, x1, x2, x3, x4;
	float y1, y2, y3, y4;


	glColor3f(1.0, 1.0, 1.0);

	cx  = x + snode->zoom * backdropWidth * boxmask->x;
	cy = y + snode->zoom * backdropHeight * boxmask->y;

	x1 = cx - (cosine * halveBoxWidth + sine * halveBoxHeight) * snode->zoom;
	x2 = cx - (cosine * -halveBoxWidth + sine * halveBoxHeight) * snode->zoom;
	x3 = cx - (cosine * -halveBoxWidth + sine * -halveBoxHeight) * snode->zoom;
	x4 = cx - (cosine * halveBoxWidth + sine * -halveBoxHeight) * snode->zoom;
	y1 = cy - (-sine * halveBoxWidth + cosine * halveBoxHeight) * snode->zoom;
	y2 = cy - (-sine * -halveBoxWidth + cosine * halveBoxHeight) * snode->zoom;
	y3 = cy - (-sine * -halveBoxWidth + cosine * -halveBoxHeight) * snode->zoom;
	y4 = cy - (-sine * halveBoxWidth + cosine * -halveBoxHeight) * snode->zoom;

	glBegin(GL_LINE_LOOP);
	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glVertex2f(x3, y3);
	glVertex2f(x4, y4);
	glEnd();
}

static void node_composit_backdrop_ellipsemask(SpaceNode *snode, ImBuf *backdrop, bNode *node, int x, int y)
{
	NodeEllipseMask *ellipsemask = node->storage;
	const float backdropWidth = backdrop->x;
	const float backdropHeight = backdrop->y;
	const float aspect = backdropWidth / backdropHeight;
	const float rad = -ellipsemask->rotation;
	const float cosine = cosf(rad);
	const float sine = sinf(rad);
	const float halveBoxWidth = backdropWidth * (ellipsemask->width / 2.0f);
	const float halveBoxHeight = backdropHeight * (ellipsemask->height / 2.0f) * aspect;

	float cx, cy, x1, x2, x3, x4;
	float y1, y2, y3, y4;


	glColor3f(1.0, 1.0, 1.0);

	cx  = x + snode->zoom * backdropWidth * ellipsemask->x;
	cy = y + snode->zoom * backdropHeight * ellipsemask->y;

	x1 = cx - (cosine * halveBoxWidth + sine * halveBoxHeight) * snode->zoom;
	x2 = cx - (cosine * -halveBoxWidth + sine * halveBoxHeight) * snode->zoom;
	x3 = cx - (cosine * -halveBoxWidth + sine * -halveBoxHeight) * snode->zoom;
	x4 = cx - (cosine * halveBoxWidth + sine * -halveBoxHeight) * snode->zoom;
	y1 = cy - (-sine * halveBoxWidth + cosine * halveBoxHeight) * snode->zoom;
	y2 = cy - (-sine * -halveBoxWidth + cosine * halveBoxHeight) * snode->zoom;
	y3 = cy - (-sine * -halveBoxWidth + cosine * -halveBoxHeight) * snode->zoom;
	y4 = cy - (-sine * halveBoxWidth + cosine * -halveBoxHeight) * snode->zoom;

	glBegin(GL_LINE_LOOP);

	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glVertex2f(x3, y3);
	glVertex2f(x4, y4);
	glEnd();
}

static void node_composit_buts_ellipsemask(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *row;
	row = uiLayoutRow(layout, true);
	uiItemR(row, ptr, "x", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "y", 0, NULL, ICON_NONE);
	row = uiLayoutRow(layout, true);
	uiItemR(row, ptr, "width", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
	uiItemR(row, ptr, "height", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

	uiItemR(layout, ptr, "rotation", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "mask_type", 0, NULL, ICON_NONE);
}

static void node_composit_buts_composite(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "use_alpha", 0, NULL, ICON_NONE);
}

static void node_composit_buts_viewer(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "use_alpha", 0, NULL, ICON_NONE);
}

static void node_composit_buts_viewer_ex(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	uiItemR(layout, ptr, "use_alpha", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "tile_order", 0, NULL, ICON_NONE);
	if (RNA_enum_get(ptr, "tile_order") == 0) {
		col = uiLayoutColumn(layout, true);
		uiItemR(col, ptr, "center_x", 0, NULL, ICON_NONE);
		uiItemR(col, ptr, "center_y", 0, NULL, ICON_NONE);
	}
}

static void node_composit_buts_mask(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;

	uiTemplateID(layout, C, ptr, "mask", NULL, NULL, NULL);
	uiItemR(layout, ptr, "use_antialiasing", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "use_feather", 0, NULL, ICON_NONE);

	uiItemR(layout, ptr, "size_source", 0, "", ICON_NONE);

	if (node->custom1 & (CMP_NODEFLAG_MASK_FIXED | CMP_NODEFLAG_MASK_FIXED_SCENE)) {
		uiItemR(layout, ptr, "size_x", 0, NULL, ICON_NONE);
		uiItemR(layout, ptr, "size_y", 0, NULL, ICON_NONE);
	}

	uiItemR(layout, ptr, "use_motion_blur", 0, NULL, ICON_NONE);
	if (node->custom1 & CMP_NODEFLAG_MASK_MOTION_BLUR) {
		uiItemR(layout, ptr, "motion_blur_samples", 0, NULL, ICON_NONE);
		uiItemR(layout, ptr, "motion_blur_shutter", 0, NULL, ICON_NONE);
	}
}

static void node_composit_buts_keyingscreen(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;

	uiTemplateID(layout, C, ptr, "clip", NULL, NULL, NULL);

	if (node->id) {
		MovieClip *clip = (MovieClip *) node->id;
		uiLayout *col;
		PointerRNA tracking_ptr;

		RNA_pointer_create(&clip->id, &RNA_MovieTracking, &clip->tracking, &tracking_ptr);

		col = uiLayoutColumn(layout, true);
		uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);
	}
}

static void node_composit_buts_keying(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	/* bNode *node = ptr->data; */ /* UNUSED */

	uiItemR(layout, ptr, "blur_pre", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "screen_balance", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "despill_factor", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "despill_balance", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "edge_kernel_radius", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "edge_kernel_tolerance", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "clip_black", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "clip_white", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "dilate_distance", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "feather_falloff", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "feather_distance", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "blur_post", 0, NULL, ICON_NONE);
}

static void node_composit_buts_trackpos(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;

	uiTemplateID(layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL);

	if (node->id) {
		MovieClip *clip = (MovieClip *) node->id;
		MovieTracking *tracking = &clip->tracking;
		MovieTrackingObject *object;
		uiLayout *col;
		PointerRNA tracking_ptr;
		NodeTrackPosData *data = node->storage;

		RNA_pointer_create(&clip->id, &RNA_MovieTracking, tracking, &tracking_ptr);

		col = uiLayoutColumn(layout, false);
		uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);

		object = BKE_tracking_object_get_named(tracking, data->tracking_object);
		if (object) {
			PointerRNA object_ptr;

			RNA_pointer_create(&clip->id, &RNA_MovieTrackingObject, object, &object_ptr);

			uiItemPointerR(col, ptr, "track_name", &object_ptr, "tracks", "", ICON_ANIM_DATA);
		}
		else {
			uiItemR(layout, ptr, "track_name", 0, "", ICON_ANIM_DATA);
		}

		uiItemR(layout, ptr, "position", 0, NULL, ICON_NONE);

		if (ELEM(node->custom1, CMP_TRACKPOS_RELATIVE_FRAME, CMP_TRACKPOS_ABSOLUTE_FRAME)) {
			uiItemR(layout, ptr, "frame_relative", 0, NULL, ICON_NONE);
		}
	}
}

static void node_composit_buts_planetrackdeform(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;

	uiTemplateID(layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL);

	if (node->id) {
		MovieClip *clip = (MovieClip *) node->id;
		MovieTracking *tracking = &clip->tracking;
		MovieTrackingObject *object;
		uiLayout *col;
		PointerRNA tracking_ptr;
		NodeTrackPosData *data = node->storage;

		RNA_pointer_create(&clip->id, &RNA_MovieTracking, tracking, &tracking_ptr);

		col = uiLayoutColumn(layout, false);
		uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);

		object = BKE_tracking_object_get_named(tracking, data->tracking_object);
		if (object) {
			PointerRNA object_ptr;

			RNA_pointer_create(&clip->id, &RNA_MovieTrackingObject, object, &object_ptr);

			uiItemPointerR(col, ptr, "plane_track_name", &object_ptr, "plane_tracks", "", ICON_ANIM_DATA);
		}
		else {
			uiItemR(layout, ptr, "plane_track_name", 0, "", ICON_ANIM_DATA);
		}
	}
}

static void node_composit_buts_cornerpin(uiLayout *UNUSED(layout), bContext *UNUSED(C), PointerRNA *UNUSED(ptr))
{
}

/* only once called */
static void node_composit_set_butfunc(bNodeType *ntype)
{
	switch (ntype->type) {
		case CMP_NODE_IMAGE:
			ntype->draw_buttons = node_composit_buts_image;
			ntype->draw_buttons_ex = node_composit_buts_image_ex;
			break;
		case CMP_NODE_R_LAYERS:
			ntype->draw_buttons = node_composit_buts_renderlayers;
			break;
		case CMP_NODE_NORMAL:
			ntype->draw_buttons = node_buts_normal;
			break;
		case CMP_NODE_CURVE_VEC:
			ntype->draw_buttons = node_buts_curvevec;
			break;
		case CMP_NODE_CURVE_RGB:
			ntype->draw_buttons = node_buts_curvecol;
			break;
		case CMP_NODE_VALUE:
			ntype->draw_buttons = node_buts_value;
			break;
		case CMP_NODE_RGB:
			ntype->draw_buttons = node_buts_rgb;
			break;
		case CMP_NODE_FLIP:
			ntype->draw_buttons = node_composit_buts_flip;
			break;
		case CMP_NODE_SPLITVIEWER:
			ntype->draw_buttons = node_composit_buts_splitviewer;
			break;
		case CMP_NODE_MIX_RGB:
			ntype->draw_buttons = node_buts_mix_rgb;
			break;
		case CMP_NODE_VALTORGB:
			ntype->draw_buttons = node_buts_colorramp;
			break;
		case CMP_NODE_CROP:
			ntype->draw_buttons = node_composit_buts_crop;
			break;
		case CMP_NODE_BLUR:
			ntype->draw_buttons = node_composit_buts_blur;
			break;
		case CMP_NODE_DBLUR:
			ntype->draw_buttons = node_composit_buts_dblur;
			break;
		case CMP_NODE_BILATERALBLUR:
			ntype->draw_buttons = node_composit_buts_bilateralblur;
			break;
		case CMP_NODE_DEFOCUS:
			ntype->draw_buttons = node_composit_buts_defocus;
			break;
		case CMP_NODE_GLARE:
			ntype->draw_buttons = node_composit_buts_glare;
			break;
		case CMP_NODE_TONEMAP:
			ntype->draw_buttons = node_composit_buts_tonemap;
			break;
		case CMP_NODE_LENSDIST:
			ntype->draw_buttons = node_composit_buts_lensdist;
			break;
		case CMP_NODE_VECBLUR:
			ntype->draw_buttons = node_composit_buts_vecblur;
			break;
		case CMP_NODE_FILTER:
			ntype->draw_buttons = node_composit_buts_filter;
			break;
		case CMP_NODE_MAP_VALUE:
			ntype->draw_buttons = node_composit_buts_map_value;
			break;
		case CMP_NODE_MAP_RANGE:
			ntype->draw_buttons = node_composit_buts_map_range;
			break;
		case CMP_NODE_TIME:
			ntype->draw_buttons = node_buts_time;
			break;
		case CMP_NODE_ALPHAOVER:
			ntype->draw_buttons = node_composit_buts_alphaover;
			break;
		case CMP_NODE_HUE_SAT:
			ntype->draw_buttons = node_composit_buts_hue_sat;
			break;
		case CMP_NODE_TEXTURE:
			ntype->draw_buttons = node_buts_texture;
			break;
		case CMP_NODE_DILATEERODE:
			ntype->draw_buttons = node_composit_buts_dilateerode;
			break;
		case CMP_NODE_INPAINT:
			ntype->draw_buttons = node_composit_buts_inpaint;
			break;
		case CMP_NODE_DESPECKLE:
			ntype->draw_buttons = node_composit_buts_despeckle;
			break;
		case CMP_NODE_OUTPUT_FILE:
			ntype->draw_buttons = node_composit_buts_file_output;
			ntype->draw_buttons_ex = node_composit_buts_file_output_ex;
			break;
		case CMP_NODE_DIFF_MATTE:
			ntype->draw_buttons = node_composit_buts_diff_matte;
			break;
		case CMP_NODE_DIST_MATTE:
			ntype->draw_buttons = node_composit_buts_distance_matte;
			break;
		case CMP_NODE_COLOR_SPILL:
			ntype->draw_buttons = node_composit_buts_color_spill;
			break;
		case CMP_NODE_CHROMA_MATTE:
			ntype->draw_buttons = node_composit_buts_chroma_matte;
			break;
		case CMP_NODE_COLOR_MATTE:
			ntype->draw_buttons = node_composit_buts_color_matte;
			break;
		case CMP_NODE_SCALE:
			ntype->draw_buttons = node_composit_buts_scale;
			break;
		case CMP_NODE_ROTATE:
			ntype->draw_buttons = node_composit_buts_rotate;
			break;
		case CMP_NODE_CHANNEL_MATTE:
			ntype->draw_buttons = node_composit_buts_channel_matte;
			break;
		case CMP_NODE_LUMA_MATTE:
			ntype->draw_buttons = node_composit_buts_luma_matte;
			break;
		case CMP_NODE_MAP_UV:
			ntype->draw_buttons = node_composit_buts_map_uv;
			break;
		case CMP_NODE_ID_MASK:
			ntype->draw_buttons = node_composit_buts_id_mask;
			break;
		case CMP_NODE_DOUBLEEDGEMASK:
			ntype->draw_buttons = node_composit_buts_double_edge_mask;
			break;
		case CMP_NODE_MATH:
			ntype->draw_buttons = node_buts_math;
			break;
		case CMP_NODE_INVERT:
			ntype->draw_buttons = node_composit_buts_invert;
			break;
		case CMP_NODE_PREMULKEY:
			ntype->draw_buttons = node_composit_buts_premulkey;
			break;
		case CMP_NODE_VIEW_LEVELS:
			ntype->draw_buttons = node_composit_buts_view_levels;
			break;
		case CMP_NODE_COLORBALANCE:
			ntype->draw_buttons = node_composit_buts_colorbalance;
			ntype->draw_buttons_ex = node_composit_buts_colorbalance_ex;
			break;
		case CMP_NODE_HUECORRECT:
			ntype->draw_buttons = node_composit_buts_huecorrect;
			break;
		case CMP_NODE_ZCOMBINE:
			ntype->draw_buttons = node_composit_buts_zcombine;
			break;
		case CMP_NODE_COMBYCCA:
		case CMP_NODE_SEPYCCA:
			ntype->draw_buttons = node_composit_buts_ycc;
			break;
		case CMP_NODE_MOVIECLIP:
			ntype->draw_buttons = node_composit_buts_movieclip;
			ntype->draw_buttons_ex = node_composit_buts_movieclip_ex;
			break;
		case CMP_NODE_STABILIZE2D:
			ntype->draw_buttons = node_composit_buts_stabilize2d;
			break;
		case CMP_NODE_TRANSFORM:
			ntype->draw_buttons = node_composit_buts_transform;
			break;
		case CMP_NODE_TRANSLATE:
			ntype->draw_buttons = node_composit_buts_translate;
			break;
		case CMP_NODE_MOVIEDISTORTION:
			ntype->draw_buttons = node_composit_buts_moviedistortion;
			break;
		case CMP_NODE_COLORCORRECTION:
			ntype->draw_buttons = node_composit_buts_colorcorrection;
			ntype->draw_buttons_ex = node_composit_buts_colorcorrection_ex;
			break;
		case CMP_NODE_SWITCH:
			ntype->draw_buttons = node_composit_buts_switch;
			break;
		case CMP_NODE_MASK_BOX:
			ntype->draw_buttons = node_composit_buts_boxmask;
			ntype->draw_backdrop = node_composit_backdrop_boxmask;
			break;
		case CMP_NODE_MASK_ELLIPSE:
			ntype->draw_buttons = node_composit_buts_ellipsemask;
			ntype->draw_backdrop = node_composit_backdrop_ellipsemask;
			break;
		case CMP_NODE_BOKEHIMAGE:
			ntype->draw_buttons = node_composit_buts_bokehimage;
			break;
		case CMP_NODE_BOKEHBLUR:
			ntype->draw_buttons = node_composit_buts_bokehblur;
			break;
		case CMP_NODE_VIEWER:
			ntype->draw_buttons = node_composit_buts_viewer;
			ntype->draw_buttons_ex = node_composit_buts_viewer_ex;
			ntype->draw_backdrop = node_composit_backdrop_viewer;
			break;
		case CMP_NODE_COMPOSITE:
			ntype->draw_buttons = node_composit_buts_composite;
			break;
		case CMP_NODE_MASK:
			ntype->draw_buttons = node_composit_buts_mask;
			break;
		case CMP_NODE_KEYINGSCREEN:
			ntype->draw_buttons = node_composit_buts_keyingscreen;
			break;
		case CMP_NODE_KEYING:
			ntype->draw_buttons = node_composit_buts_keying;
			break;
		case CMP_NODE_TRACKPOS:
			ntype->draw_buttons = node_composit_buts_trackpos;
			break;
		case CMP_NODE_PLANETRACKDEFORM:
			ntype->draw_buttons = node_composit_buts_planetrackdeform;
			break;
		case CMP_NODE_CORNERPIN:
			ntype->draw_buttons = node_composit_buts_cornerpin;
			break;
	}
}

/* ****************** BUTTON CALLBACKS FOR TEXTURE NODES ***************** */

static void node_texture_buts_bricks(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiLayout *col;
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "offset", UI_ITEM_R_SLIDER, IFACE_("Offset"), ICON_NONE);
	uiItemR(col, ptr, "offset_frequency", 0, IFACE_("Frequency"), ICON_NONE);
	
	col = uiLayoutColumn(layout, true);
	uiItemR(col, ptr, "squash", 0, IFACE_("Squash"), ICON_NONE);
	uiItemR(col, ptr, "squash_frequency", 0, IFACE_("Frequency"), ICON_NONE);
}

static void node_texture_buts_proc(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	PointerRNA tex_ptr;
	bNode *node = ptr->data;
	ID *id = ptr->id.data;
	Tex *tex = (Tex *)node->storage;
	uiLayout *col, *row;
	
	RNA_pointer_create(id, &RNA_Texture, tex, &tex_ptr);

	col = uiLayoutColumn(layout, false);

	switch (tex->type) {
		case TEX_BLEND:
			uiItemR(col, &tex_ptr, "progression", 0, "", ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "use_flip_axis", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			break;

		case TEX_MARBLE:
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "marble_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "noise_basis", 0, "", ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "noise_basis_2", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			break;

		case TEX_MAGIC:
			uiItemR(col, &tex_ptr, "noise_depth", 0, NULL, ICON_NONE);
			break;

		case TEX_STUCCI:
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "stucci_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			uiItemR(col, &tex_ptr, "noise_basis", 0, "", ICON_NONE);
			break;

		case TEX_WOOD:
			uiItemR(col, &tex_ptr, "noise_basis", 0, "", ICON_NONE);
			uiItemR(col, &tex_ptr, "wood_type", 0, "", ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "noise_basis_2", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			row = uiLayoutRow(col, false);
			uiLayoutSetActive(row, !(ELEM(tex->stype, TEX_BAND, TEX_RING)));
			uiItemR(row, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			break;
			
		case TEX_CLOUDS:
			uiItemR(col, &tex_ptr, "noise_basis", 0, "", ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "cloud_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			row = uiLayoutRow(col, false);
			uiItemR(row, &tex_ptr, "noise_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			uiItemR(col, &tex_ptr, "noise_depth", UI_ITEM_R_EXPAND, IFACE_("Depth"), ICON_NONE);
			break;
			
		case TEX_DISTNOISE:
			uiItemR(col, &tex_ptr, "noise_basis", 0, "", ICON_NONE);
			uiItemR(col, &tex_ptr, "noise_distortion", 0, "", ICON_NONE);
			break;

		case TEX_MUSGRAVE:
			uiItemR(col, &tex_ptr, "musgrave_type", 0, "", ICON_NONE);
			uiItemR(col, &tex_ptr, "noise_basis", 0, "", ICON_NONE);
			break;
		case TEX_VORONOI:
			uiItemR(col, &tex_ptr, "distance_metric", 0, "", ICON_NONE);
			if (tex->vn_distm == TEX_MINKOVSKY) {
				uiItemR(col, &tex_ptr, "minkovsky_exponent", 0, NULL, ICON_NONE);
			}
			uiItemR(col, &tex_ptr, "color_mode", 0, "", ICON_NONE);
			break;
	}
}

static void node_texture_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	uiTemplateID(layout, C, ptr, "image", NULL, "IMAGE_OT_open", NULL);
}

static void node_texture_buts_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;
	PointerRNA iuserptr;

	RNA_pointer_create((ID *)ptr->id.data, &RNA_ImageUser, node->storage, &iuserptr);
	uiTemplateImage(layout, C, ptr, "image", &iuserptr, 0);
}

static void node_texture_buts_output(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
	uiItemR(layout, ptr, "filepath", 0, "", ICON_NONE);
}

/* only once called */
static void node_texture_set_butfunc(bNodeType *ntype)
{
	if (ntype->type >= TEX_NODE_PROC && ntype->type < TEX_NODE_PROC_MAX) {
		ntype->draw_buttons = node_texture_buts_proc;
	}
	else {
		switch (ntype->type) {

			case TEX_NODE_MATH:
				ntype->draw_buttons = node_buts_math;
				break;

			case TEX_NODE_MIX_RGB:
				ntype->draw_buttons = node_buts_mix_rgb;
				break;

			case TEX_NODE_VALTORGB:
				ntype->draw_buttons = node_buts_colorramp;
				break;

			case TEX_NODE_CURVE_RGB:
				ntype->draw_buttons = node_buts_curvecol;
				break;

			case TEX_NODE_CURVE_TIME:
				ntype->draw_buttons = node_buts_time;
				break;

			case TEX_NODE_TEXTURE:
				ntype->draw_buttons = node_buts_texture;
				break;

			case TEX_NODE_BRICKS:
				ntype->draw_buttons = node_texture_buts_bricks;
				break;

			case TEX_NODE_IMAGE:
				ntype->draw_buttons = node_texture_buts_image;
				ntype->draw_buttons_ex = node_texture_buts_image_ex;
				break;

			case TEX_NODE_OUTPUT:
				ntype->draw_buttons = node_texture_buts_output;
				break;
		}
	}
}

/* ******* init draw callbacks for all tree types, only called in usiblender.c, once ************* */

static void node_property_update_default(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	bNodeTree *ntree = ptr->id.data;
	ED_node_tag_update_nodetree(bmain, ntree);
}

static void node_socket_template_properties_update(bNodeType *ntype, bNodeSocketTemplate *stemp)
{
	StructRNA *srna = ntype->ext.srna;
	PropertyRNA *prop = RNA_struct_type_find_property(srna, stemp->identifier);
	
	if (prop)
		RNA_def_property_update_runtime(prop, node_property_update_default);
}

static void node_template_properties_update(bNodeType *ntype)
{
	bNodeSocketTemplate *stemp;
	
	if (ntype->inputs) {
		for (stemp = ntype->inputs; stemp->type >= 0; ++stemp)
			node_socket_template_properties_update(ntype, stemp);
	}
	if (ntype->outputs) {
		for (stemp = ntype->outputs; stemp->type >= 0; ++stemp)
			node_socket_template_properties_update(ntype, stemp);
	}
}

static void node_socket_undefined_draw(bContext *UNUSED(C), uiLayout *layout, PointerRNA *UNUSED(ptr), PointerRNA *UNUSED(node_ptr),
                                       const char *UNUSED(text))
{
	uiItemL(layout, IFACE_("Undefined Socket Type"), ICON_ERROR);
}

static void node_socket_undefined_draw_color(bContext *UNUSED(C), PointerRNA *UNUSED(ptr), PointerRNA *UNUSED(node_ptr), float *r_color)
{
	r_color[0] = 1.0f;
	r_color[1] = 0.0f;
	r_color[2] = 0.0f;
	r_color[3] = 1.0f;
}

static void node_socket_undefined_interface_draw(bContext *UNUSED(C), uiLayout *layout, PointerRNA *UNUSED(ptr))
{
	uiItemL(layout, IFACE_("Undefined Socket Type"), ICON_ERROR);
}

static void node_socket_undefined_interface_draw_color(bContext *UNUSED(C), PointerRNA *UNUSED(ptr), float *r_color)
{
	r_color[0] = 1.0f;
	r_color[1] = 0.0f;
	r_color[2] = 0.0f;
	r_color[3] = 1.0f;
}

void ED_node_init_butfuncs(void)
{
	/* Fallback types for undefined tree, nodes, sockets
	 * Defined in blenkernel, but not registered in type hashes.
	 */
	/*extern bNodeTreeType NodeTreeTypeUndefined;*/
	extern bNodeType NodeTypeUndefined;
	extern bNodeSocketType NodeSocketTypeUndefined;
	
	/* default ui functions */
	NodeTypeUndefined.draw_nodetype = node_draw_default;
	NodeTypeUndefined.draw_nodetype_prepare = node_update_default;
	NodeTypeUndefined.select_area_func = node_select_area_default;
	NodeTypeUndefined.tweak_area_func = node_tweak_area_default;
	NodeTypeUndefined.draw_buttons = NULL;
	NodeTypeUndefined.draw_buttons_ex = NULL;
	NodeTypeUndefined.resize_area_func = node_resize_area_default;
	
	NodeSocketTypeUndefined.draw = node_socket_undefined_draw;
	NodeSocketTypeUndefined.draw_color = node_socket_undefined_draw_color;
	NodeSocketTypeUndefined.interface_draw = node_socket_undefined_interface_draw;
	NodeSocketTypeUndefined.interface_draw_color = node_socket_undefined_interface_draw_color;
	
	/* node type ui functions */
	NODE_TYPES_BEGIN(ntype)
		/* default ui functions */
		ntype->draw_nodetype = node_draw_default;
		ntype->draw_nodetype_prepare = node_update_default;
		ntype->select_area_func = node_select_area_default;
		ntype->tweak_area_func = node_tweak_area_default;
		ntype->draw_buttons = NULL;
		ntype->draw_buttons_ex = NULL;
		ntype->resize_area_func = node_resize_area_default;
		
		node_common_set_butfunc(ntype);
		
		node_composit_set_butfunc(ntype);
		node_shader_set_butfunc(ntype);
		node_texture_set_butfunc(ntype);
		
		/* define update callbacks for socket properties */
		node_template_properties_update(ntype);
	NODE_TYPES_END
	
	/* tree type icons */
	ntreeType_Composite->ui_icon = ICON_RENDERLAYERS;
	ntreeType_Shader->ui_icon = ICON_MATERIAL;
	ntreeType_Texture->ui_icon = ICON_TEXTURE;
}

void ED_init_custom_node_type(bNodeType *ntype)
{
	/* default ui functions */
	ntype->draw_nodetype = node_draw_default;
	ntype->draw_nodetype_prepare = node_update_default;
	ntype->resize_area_func = node_resize_area_default;
	ntype->select_area_func = node_select_area_default;
	ntype->tweak_area_func = node_tweak_area_default;
}

void ED_init_custom_node_socket_type(bNodeSocketType *stype)
{
	/* default ui functions */
	stype->draw = node_socket_button_label;
}

/* maps standard socket integer type to a color */
static const float std_node_socket_colors[][4] = {
	{0.63, 0.63, 0.63, 1.0},    /* SOCK_FLOAT */
	{0.39, 0.39, 0.78, 1.0},    /* SOCK_VECTOR */
	{0.78, 0.78, 0.16, 1.0},    /* SOCK_RGBA */
	{0.39, 0.78, 0.39, 1.0},    /* SOCK_SHADER */
	{0.70, 0.65, 0.19, 1.0},    /* SOCK_BOOLEAN */
	{0.0, 0.0, 0.0, 1.0},       /*__SOCK_MESH (deprecated) */
	{0.06, 0.52, 0.15, 1.0},    /* SOCK_INT */
	{1.0, 1.0, 1.0, 1.0},       /* SOCK_STRING */
};

/* common color callbacks for standard types */
static void std_node_socket_draw_color(bContext *UNUSED(C), PointerRNA *ptr, PointerRNA *UNUSED(node_ptr), float *r_color)
{
	bNodeSocket *sock = ptr->data;
	int type = sock->typeinfo->type;
	copy_v4_v4(r_color, std_node_socket_colors[type]);
}
static void std_node_socket_interface_draw_color(bContext *UNUSED(C), PointerRNA *ptr, float *r_color)
{
	bNodeSocket *sock = ptr->data;
	int type = sock->typeinfo->type;
	copy_v4_v4(r_color, std_node_socket_colors[type]);
}

/* draw function for file output node sockets, displays only sub-path and format, no value button */
static void node_file_output_socket_draw(bContext *C, uiLayout *layout, PointerRNA *ptr, PointerRNA *node_ptr)
{
	bNodeTree *ntree = ptr->id.data;
	bNodeSocket *sock = ptr->data;
	uiLayout *row;
	PointerRNA inputptr, imfptr;
	int imtype;
	
	row = uiLayoutRow(layout, false);
	
	imfptr = RNA_pointer_get(node_ptr, "format");
	imtype = RNA_enum_get(&imfptr, "file_format");
	if (imtype == R_IMF_IMTYPE_MULTILAYER) {
		NodeImageMultiFileSocket *input = sock->storage;
		RNA_pointer_create(&ntree->id, &RNA_NodeOutputFileSlotLayer, input, &inputptr);
		
		uiItemL(row, input->layer, ICON_NONE);
	}
	else {
		NodeImageMultiFileSocket *input = sock->storage;
		PropertyRNA *imtype_prop;
		const char *imtype_name;
		uiBlock *block;
		RNA_pointer_create(&ntree->id, &RNA_NodeOutputFileSlotFile, input, &inputptr);
		
		uiItemL(row, input->path, ICON_NONE);
		
		if (!RNA_boolean_get(&inputptr, "use_node_format"))
			imfptr = RNA_pointer_get(&inputptr, "format");
		
		imtype_prop = RNA_struct_find_property(&imfptr, "file_format");
		RNA_property_enum_name((bContext *)C, &imfptr, imtype_prop,
		                       RNA_property_enum_get(&imfptr, imtype_prop), &imtype_name);
		block = uiLayoutGetBlock(row);
		uiBlockSetEmboss(block, UI_EMBOSSP);
		uiItemL(row, imtype_name, ICON_NONE);
		uiBlockSetEmboss(block, UI_EMBOSSN);
	}
}

static void std_node_socket_draw(bContext *C, uiLayout *layout, PointerRNA *ptr, PointerRNA *node_ptr, const char *text)
{
	bNode *node = node_ptr->data;
	bNodeSocket *sock = ptr->data;
	int type = sock->typeinfo->type;
	/*int subtype = sock->typeinfo->subtype;*/
	
	/* XXX not nice, eventually give this node its own socket type ... */
	if (node->type == CMP_NODE_OUTPUT_FILE) {
		node_file_output_socket_draw(C, layout, ptr, node_ptr);
		return;
	}
	
	if ((sock->in_out == SOCK_OUT) || (sock->flag & SOCK_IN_USE) || (sock->flag & SOCK_HIDE_VALUE)) {
		node_socket_button_label(C, layout, ptr, node_ptr, text);
		return;
	}
	
	switch (type) {
		case SOCK_FLOAT:
		case SOCK_INT:
		case SOCK_BOOLEAN:
			uiItemR(layout, ptr, "default_value", 0, text, 0);
			break;
		case SOCK_VECTOR:
			uiTemplateComponentMenu(layout, ptr, "default_value", text);
			break;
		case SOCK_RGBA:
		{
			uiLayout *row = uiLayoutRow(layout, false);
			uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_LEFT);
			/* draw the socket name right of the actual button */
			uiItemR(row, ptr, "default_value", 0, "", 0);
			uiItemL(row, text, 0);
			break;
		}
		case SOCK_STRING:
		{
			uiLayout *row = uiLayoutRow(layout, true);
			/* draw the socket name right of the actual button */
			uiItemR(row, ptr, "default_value", 0, "", 0);
			uiItemL(row, text, 0);
			break;
		}
		default:
			node_socket_button_label(C, layout, ptr, node_ptr, text);
			break;
	}
}

static void std_node_socket_interface_draw(bContext *UNUSED(C), uiLayout *layout, PointerRNA *ptr)
{
	bNodeSocket *sock = ptr->data;
	int type = sock->typeinfo->type;
	/*int subtype = sock->typeinfo->subtype;*/
	
	switch (type) {
		case SOCK_FLOAT:
		{
			uiLayout *row;
			uiItemR(layout, ptr, "default_value", 0, NULL, 0);
			row = uiLayoutRow(layout, true);
			uiItemR(row, ptr, "min_value", 0, IFACE_("Min"), 0);
			uiItemR(row, ptr, "max_value", 0, IFACE_("Max"), 0);
			break;
		}
		case SOCK_INT:
		{
			uiLayout *row;
			uiItemR(layout, ptr, "default_value", 0, NULL, 0);
			row = uiLayoutRow(layout, true);
			uiItemR(row, ptr, "min_value", 0, IFACE_("Min"), 0);
			uiItemR(row, ptr, "max_value", 0, IFACE_("Max"), 0);
			break;
		}
		case SOCK_BOOLEAN:
		{
			uiItemR(layout, ptr, "default_value", 0, NULL, 0);
			break;
		}
		case SOCK_VECTOR:
		{
			uiLayout *row;
			uiItemR(layout, ptr, "default_value", UI_ITEM_R_EXPAND, NULL, 0);
			row = uiLayoutRow(layout, true);
			uiItemR(row, ptr, "min_value", 0, IFACE_("Min"), 0);
			uiItemR(row, ptr, "max_value", 0, IFACE_("Max"), 0);
			break;
		}
		case SOCK_RGBA:
		{
			uiItemR(layout, ptr, "default_value", 0, NULL, 0);
			break;
		}
		case SOCK_STRING:
		{
			uiItemR(layout, ptr, "default_value", 0, NULL, 0);
			break;
		}
	}
}

void ED_init_standard_node_socket_type(bNodeSocketType *stype)
{
	stype->draw = std_node_socket_draw;
	stype->draw_color = std_node_socket_draw_color;
	stype->interface_draw = std_node_socket_interface_draw;
	stype->interface_draw_color = std_node_socket_interface_draw_color;
}

static void node_socket_virtual_draw_color(bContext *UNUSED(C), PointerRNA *UNUSED(ptr), PointerRNA *UNUSED(node_ptr), float *r_color)
{
	/* alpha = 0, empty circle */
	zero_v4(r_color);
}

void ED_init_node_socket_type_virtual(bNodeSocketType *stype)
{
	stype->draw = node_socket_button_label;
	stype->draw_color = node_socket_virtual_draw_color;
}

/* ************** Generic drawing ************** */

void draw_nodespace_back_pix(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeInstanceKey parent_key)
{
	bNodeInstanceKey active_viewer_key = (snode->nodetree ? snode->nodetree->active_viewer_key : NODE_INSTANCE_KEY_NONE);
	Image *ima;
	void *lock;
	ImBuf *ibuf;
	
	if (!(snode->flag & SNODE_BACKDRAW) || !ED_node_is_compositor(snode))
		return;
	
	if (parent_key.value != active_viewer_key.value)
		return;
	
	ima = BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);
	if (ibuf) {
		float x, y; 
		
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		
		/* somehow the offset has to be calculated inverse */
		
		glaDefine2DArea(&ar->winrct);
		/* ortho at pixel level curarea */
		wmOrtho2(-GLA_PIXEL_OFS, ar->winx - GLA_PIXEL_OFS, -GLA_PIXEL_OFS, ar->winy - GLA_PIXEL_OFS);
		
		x = (ar->winx - snode->zoom * ibuf->x) / 2 + snode->xof;
		y = (ar->winy - snode->zoom * ibuf->y) / 2 + snode->yof;
		
		if (ibuf->rect || ibuf->rect_float) {
			unsigned char *display_buffer = NULL;
			void *cache_handle = NULL;
			
			if (snode->flag & (SNODE_SHOW_R | SNODE_SHOW_G | SNODE_SHOW_B)) {
				int ofs;
				
				display_buffer = IMB_display_buffer_acquire_ctx(C, ibuf, &cache_handle);
				
#ifdef __BIG_ENDIAN__
				if      (snode->flag & SNODE_SHOW_R) ofs = 2;
				else if (snode->flag & SNODE_SHOW_G) ofs = 1;
				else                                 ofs = 0;
#else
				if      (snode->flag & SNODE_SHOW_R) ofs = 1;
				else if (snode->flag & SNODE_SHOW_G) ofs = 2;
				else                                 ofs = 3;
#endif
				
				glPixelZoom(snode->zoom, snode->zoom);
				/* swap bytes, so alpha is most significant one, then just draw it as luminance int */
				
				glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_LUMINANCE, GL_UNSIGNED_INT,
				                  display_buffer + ofs);
				
				glPixelZoom(1.0f, 1.0f);
			}
			else if (snode->flag & SNODE_SHOW_ALPHA) {
				display_buffer = IMB_display_buffer_acquire_ctx(C, ibuf, &cache_handle);
				
				glPixelZoom(snode->zoom, snode->zoom);
				/* swap bytes, so alpha is most significant one, then just draw it as luminance int */
#ifdef __BIG_ENDIAN__
				glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
#endif
				glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_LUMINANCE, GL_UNSIGNED_INT, display_buffer);
				
#ifdef __BIG_ENDIAN__
				glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
#endif
				glPixelZoom(1.0f, 1.0f);
			}
			else if (snode->flag & SNODE_USE_ALPHA) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glPixelZoom(snode->zoom, snode->zoom);
				
				glaDrawImBuf_glsl_ctx(C, ibuf, x, y, GL_NEAREST);
				
				glPixelZoom(1.0f, 1.0f);
				glDisable(GL_BLEND);
			}
			else {
				glPixelZoom(snode->zoom, snode->zoom);
				
				glaDrawImBuf_glsl_ctx(C, ibuf, x, y, GL_NEAREST);
				
				glPixelZoom(1.0f, 1.0f);
			}
			
			if (cache_handle)
				IMB_display_buffer_release(cache_handle);
		}
		
		/** @note draw selected info on backdrop */
		if (snode->edittree) {
			bNode *node = snode->edittree->nodes.first;
			rctf *viewer_border = &snode->nodetree->viewer_border;
			while (node) {
				if (node->flag & NODE_SELECT) {
					if (node->typeinfo->draw_backdrop) {
						node->typeinfo->draw_backdrop(snode, ibuf, node, x, y);
					}
				}
				node = node->next;
			}
			
			if ((snode->nodetree->flag & NTREE_VIEWER_BORDER) &&
			        viewer_border->xmin < viewer_border->xmax &&
			        viewer_border->ymin < viewer_border->ymax)
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				setlinestyle(3);
				cpack(0x4040FF);
				
				glRectf(x + snode->zoom * viewer_border->xmin * ibuf->x,
				        y + snode->zoom * viewer_border->ymin * ibuf->y,
				        x + snode->zoom * viewer_border->xmax * ibuf->x,
				        y + snode->zoom * viewer_border->ymax * ibuf->y);
				
				setlinestyle(0);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
		}
		
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
	}
	
	BKE_image_release_ibuf(ima, ibuf, lock);
}


/* if v2d not NULL, it clips and returns 0 if not visible */
int node_link_bezier_points(View2D *v2d, SpaceNode *snode, bNodeLink *link, float coord_array[][2], int resol)
{
	float dist, vec[4][2];
	float deltax, deltay;
	float cursor[2] = {0.0f, 0.0f};
	int toreroute, fromreroute;
	
	/* this function can be called with snode null (via cut_links_intersect) */
	/* XXX map snode->cursor back to view space */
	if (snode) {
		cursor[0] = snode->cursor[0] * UI_DPI_FAC;
		cursor[1] = snode->cursor[1] * UI_DPI_FAC;
	}
	
	/* in v0 and v3 we put begin/end points */
	if (link->fromsock) {
		vec[0][0] = link->fromsock->locx;
		vec[0][1] = link->fromsock->locy;
		fromreroute = (link->fromnode && link->fromnode->type == NODE_REROUTE);
	}
	else {
		if (snode == NULL) return 0;
		copy_v2_v2(vec[0], cursor);
		fromreroute = 0;
	}
	if (link->tosock) {
		vec[3][0] = link->tosock->locx;
		vec[3][1] = link->tosock->locy;
		toreroute = (link->tonode && link->tonode->type == NODE_REROUTE);
	}
	else {
		if (snode == NULL) return 0;
		copy_v2_v2(vec[3], cursor);
		toreroute = 0;
	}

	dist = UI_GetThemeValue(TH_NODE_CURVING) * 0.10f * fabsf(vec[0][0] - vec[3][0]);
	deltax = vec[3][0] - vec[0][0];
	deltay = vec[3][1] - vec[0][1];
	/* check direction later, for top sockets */
	if (fromreroute) {
		if (ABS(deltax) > ABS(deltay)) {
			vec[1][1] = vec[0][1];
			vec[1][0] = vec[0][0] + (deltax > 0 ? dist : -dist);
		}
		else {
			vec[1][0] = vec[0][0];
			vec[1][1] = vec[0][1] + (deltay > 0 ? dist : -dist);
		}
	}
	else {
		vec[1][0] = vec[0][0] + dist;
		vec[1][1] = vec[0][1];
	}
	if (toreroute) {
		if (ABS(deltax) > ABS(deltay)) {
			vec[2][1] = vec[3][1];
			vec[2][0] = vec[3][0] + (deltax > 0 ? -dist : dist);
		}
		else {
			vec[2][0] = vec[3][0];
			vec[2][1] = vec[3][1] + (deltay > 0 ? -dist : dist);
		}

	}
	else {
		vec[2][0] = vec[3][0] - dist;
		vec[2][1] = vec[3][1];
	}
	if (v2d && min_ffff(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) > v2d->cur.xmax) {
		/* clipped */
	}
	else if (v2d && max_ffff(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) < v2d->cur.xmin) {
		/* clipped */
	}
	else {
		/* always do all three, to prevent data hanging around */
		BKE_curve_forward_diff_bezier(vec[0][0], vec[1][0], vec[2][0], vec[3][0],
		                              coord_array[0] + 0, resol, sizeof(float) * 2);
		BKE_curve_forward_diff_bezier(vec[0][1], vec[1][1], vec[2][1], vec[3][1],
		                              coord_array[0] + 1, resol, sizeof(float) * 2);
		
		return 1;
	}
	return 0;
}

#define LINK_RESOL  24
#define LINK_ARROW  12  /* position of arrow on the link, LINK_RESOL/2 */
#define ARROW_SIZE 7
void node_draw_link_bezier(View2D *v2d, SpaceNode *snode, bNodeLink *link,
                           int th_col1, bool do_shaded, int th_col2, bool do_triple, int th_col3)
{
	float coord_array[LINK_RESOL + 1][2];
	
	if (node_link_bezier_points(v2d, snode, link, coord_array, LINK_RESOL)) {
		float dist, spline_step = 0.0f;
		int i;
		int drawarrow;
		/* store current linewidth */
		float linew;
		float arrow[2], arrow1[2], arrow2[2];
		glGetFloatv(GL_LINE_WIDTH, &linew);
		
		/* we can reuse the dist variable here to increment the GL curve eval amount*/
		dist = 1.0f / (float)LINK_RESOL;
		
		glEnable(GL_LINE_SMOOTH);
		
		drawarrow = ((link->tonode && (link->tonode->type == NODE_REROUTE)) &&
		             (link->fromnode && (link->fromnode->type == NODE_REROUTE)));

		if (drawarrow) {
			/* draw arrow in line segment LINK_ARROW */
			float d_xy[2], len;

			sub_v2_v2v2(d_xy, coord_array[LINK_ARROW], coord_array[LINK_ARROW - 1]);
			len = len_v2(d_xy);
			mul_v2_fl(d_xy, ARROW_SIZE / len);
			arrow1[0] = coord_array[LINK_ARROW][0] - d_xy[0] + d_xy[1];
			arrow1[1] = coord_array[LINK_ARROW][1] - d_xy[1] - d_xy[0];
			arrow2[0] = coord_array[LINK_ARROW][0] - d_xy[0] - d_xy[1];
			arrow2[1] = coord_array[LINK_ARROW][1] - d_xy[1] + d_xy[0];
			arrow[0] = coord_array[LINK_ARROW][0];
			arrow[1] = coord_array[LINK_ARROW][1];
		}
		if (do_triple) {
			UI_ThemeColorShadeAlpha(th_col3, -80, -120);
			glLineWidth(4.0f);
			
			glBegin(GL_LINE_STRIP);
			for (i = 0; i <= LINK_RESOL; i++) {
				glVertex2fv(coord_array[i]);
			}
			glEnd();
			if (drawarrow) {
				glBegin(GL_LINE_STRIP);
				glVertex2fv(arrow1);
				glVertex2fv(arrow);
				glVertex2fv(arrow);
				glVertex2fv(arrow2);
				glEnd();
			}
		}
		
		/* XXX using GL_LINES for shaded node lines is a workaround
		 * for Intel hardware, this breaks with GL_LINE_STRIP and
		 * changing color in begin/end blocks.
		 */
		glLineWidth(1.5f);
		if (do_shaded) {
			glBegin(GL_LINES);
			for (i = 0; i < LINK_RESOL; i++) {
				UI_ThemeColorBlend(th_col1, th_col2, spline_step);
				glVertex2fv(coord_array[i]);
				
				UI_ThemeColorBlend(th_col1, th_col2, spline_step + dist);
				glVertex2fv(coord_array[i + 1]);
				
				spline_step += dist;
			}
			glEnd();
		}
		else {
			UI_ThemeColor(th_col1);
			glBegin(GL_LINE_STRIP);
			for (i = 0; i <= LINK_RESOL; i++) {
				glVertex2fv(coord_array[i]);
			}
			glEnd();
		}
		
		if (drawarrow) {
			glBegin(GL_LINE_STRIP);
			glVertex2fv(arrow1);
			glVertex2fv(arrow);
			glVertex2fv(arrow);
			glVertex2fv(arrow2);
			glEnd();
		}
		
		glDisable(GL_LINE_SMOOTH);
		
		/* restore previuos linewidth */
		glLineWidth(1.0f);
	}
}

#if 0 /* not used in 2.5x yet */
static void node_link_straight_points(View2D *UNUSED(v2d), SpaceNode *snode, bNodeLink *link, float coord_array[][2])
{
	if (link->fromsock) {
		coord_array[0][0] = link->fromsock->locx;
		coord_array[0][1] = link->fromsock->locy;
	}
	else {
		if (snode == NULL) return;
		coord_array[0][0] = snode->mx;
		coord_array[0][1] = snode->my;
	}
	if (link->tosock) {
		coord_array[1][0] = link->tosock->locx;
		coord_array[1][1] = link->tosock->locy;
	}
	else {
		if (snode == NULL) return;
		coord_array[1][0] = snode->mx;
		coord_array[1][1] = snode->my;
	}
}

void node_draw_link_straight(View2D *v2d, SpaceNode *snode, bNodeLink *link,
                             int th_col1, int do_shaded, int th_col2, int do_triple, int th_col3)
{
	float coord_array[2][2];
	float linew;
	int i;
	
	node_link_straight_points(v2d, snode, link, coord_array);
	
	/* store current linewidth */
	glGetFloatv(GL_LINE_WIDTH, &linew);
	
	glEnable(GL_LINE_SMOOTH);
	
	if (do_triple) {
		UI_ThemeColorShadeAlpha(th_col3, -80, -120);
		glLineWidth(4.0f);
		
		glBegin(GL_LINES);
		glVertex2fv(coord_array[0]);
		glVertex2fv(coord_array[1]);
		glEnd();
	}
	
	UI_ThemeColor(th_col1);
	glLineWidth(1.5f);
	
	/* XXX using GL_LINES for shaded node lines is a workaround
	 * for Intel hardware, this breaks with GL_LINE_STRIP and
	 * changing color in begin/end blocks.
	 */
	if (do_shaded) {
		glBegin(GL_LINES);
		for (i = 0; i < LINK_RESOL - 1; ++i) {
			float t = (float)i / (float)(LINK_RESOL - 1);
			UI_ThemeColorBlend(th_col1, th_col2, t);
			glVertex2f((1.0f - t) * coord_array[0][0] + t * coord_array[1][0],
			           (1.0f - t) * coord_array[0][1] + t * coord_array[1][1]);
			
			t = (float)(i + 1) / (float)(LINK_RESOL - 1);
			UI_ThemeColorBlend(th_col1, th_col2, t);
			glVertex2f((1.0f - t) * coord_array[0][0] + t * coord_array[1][0],
			           (1.0f - t) * coord_array[0][1] + t * coord_array[1][1]);
		}
		glEnd();
	}
	else {
		glBegin(GL_LINE_STRIP);
		for (i = 0; i < LINK_RESOL; ++i) {
			float t = (float)i / (float)(LINK_RESOL - 1);
			glVertex2f((1.0f - t) * coord_array[0][0] + t * coord_array[1][0],
			           (1.0f - t) * coord_array[0][1] + t * coord_array[1][1]);
		}
		glEnd();
	}
	
	glDisable(GL_LINE_SMOOTH);
	
	/* restore previuos linewidth */
	glLineWidth(1.0f);
}
#endif

/* note; this is used for fake links in groups too */
void node_draw_link(View2D *v2d, SpaceNode *snode, bNodeLink *link)
{
	bool do_shaded = false;
	bool do_triple = false;
	int th_col1 = TH_HEADER, th_col2 = TH_HEADER, th_col3 = TH_WIRE;
	
	if (link->fromsock == NULL && link->tosock == NULL)
		return;
	
	/* new connection */
	if (!link->fromsock || !link->tosock) {
		th_col1 = TH_ACTIVE;
		do_triple = true;
	}
	else {
		/* going to give issues once... */
		if (link->tosock->flag & SOCK_UNAVAIL)
			return;
		if (link->fromsock->flag & SOCK_UNAVAIL)
			return;
		
		if (link->flag & NODE_LINK_VALID) {
			/* special indicated link, on drop-node */
			if (link->flag & NODE_LINKFLAG_HILITE) {
				th_col1 = th_col2 = TH_ACTIVE;
			}
			else {
				/* regular link */
				if (link->fromnode && link->fromnode->flag & SELECT)
					th_col1 = TH_EDGE_SELECT;
				if (link->tonode && link->tonode->flag & SELECT)
					th_col2 = TH_EDGE_SELECT;
			}
			do_shaded = true;
			do_triple = true;
		}
		else {
			th_col1 = TH_REDALERT;
		}
	}
	
	node_draw_link_bezier(v2d, snode, link, th_col1, do_shaded, th_col2, do_triple, th_col3);
//	node_draw_link_straight(v2d, snode, link, th_col1, do_shaded, th_col2, do_triple, th_col3);
}

void ED_node_draw_snap(View2D *v2d, const float cent[2], float size, NodeBorder border)
{
	glBegin(GL_LINES);
	
	if (border & (NODE_LEFT | NODE_RIGHT)) {
		glVertex2f(cent[0], v2d->cur.ymin);
		glVertex2f(cent[0], v2d->cur.ymax);
	}
	else {
		glVertex2f(cent[0], cent[1] - size);
		glVertex2f(cent[0], cent[1] + size);
	}
	
	if (border & (NODE_TOP | NODE_BOTTOM)) {
		glVertex2f(v2d->cur.xmin, cent[1]);
		glVertex2f(v2d->cur.xmax, cent[1]);
	}
	else {
		glVertex2f(cent[0] - size, cent[1]);
		glVertex2f(cent[0] + size, cent[1]);
	}
	
	glEnd();
}
