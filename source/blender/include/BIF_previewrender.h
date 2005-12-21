/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

struct SpaceButs;
struct RenderInfo;
struct Image;
struct ScrArea;

typedef void (*VectorDrawFunc)(int x, int y, int w, int h, float alpha);

/* stores rendered preview  - is also used for icons */
typedef struct RenderInfo {
	int pr_rectx;
	int pr_recty;		
	unsigned int* rect; 
	short cury;
} RenderInfo;

/* Set the previewrect for drawing */
void BIF_set_previewrect(int win, int xmin, int ymin, int xmax, int ymax, short pr_rectx, short pr_recty);
void BIF_end_previewrect(void);

void 	BIF_all_preview_changed(void);
void    BIF_preview_changed		(struct SpaceButs *sbuts);
void	BIF_previewrender_buts	(struct SpaceButs *sbuts);
/* Render the preview
 * a) into the ri->rect
 * b) draw it in the area using the block UIMat 

 if doDraw is false, the preview is not drawn and the function is not dynamic,
 so no events are processed. Hopefully fast enough for 64x64 rendering or 
 at least 32x32 */
void	BIF_previewrender		(struct ID* id, struct RenderInfo *ri, struct ScrArea *area, int doDraw);
void	BIF_previewdraw		(void);
void	BIF_previewdraw_render(struct RenderInfo* ri, struct ScrArea* area);

void	BIF_calcpreview_image(struct Image* img, struct RenderInfo* ri, unsigned int w, unsigned int h);	
