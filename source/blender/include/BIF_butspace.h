/**
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BIF_BUTSPACE_H
#define BIF_BUTSPACE_H

/* all internal/external calls and event codes for buttons space */
/* should be split in 2 parts... */

struct Base;
struct ID;

/* external, butspace.c */
extern void do_butspace(unsigned short event);
extern void redraw_test_buttons(struct Object *new);

extern char *image_type_pup(void);

/* buttons_editing.c */
extern void validate_editbonebutton_cb(void *bonev, void *namev);

/* buts->mainb old */
#define BUTS_VIEW			0
#define BUTS_LAMP			1
#define BUTS_MAT			2
#define BUTS_TEX			3
#define BUTS_ANIM			4
#define BUTS_WORLD			5
#define BUTS_RENDER			6
#define BUTS_EDIT			7
#define BUTS_GAME			8
#define BUTS_FPAINT			9
#define BUTS_RADIO			10
#define BUTS_SCRIPT			11
#define BUTS_SOUND			12
#define BUTS_CONSTRAINT		13
#define BUTS_EFFECTS		14

/* warning: the values of these defines are used in sbuts->tabs[7] */
/* buts->mainb new */
#define CONTEXT_SCENE	0
#define CONTEXT_OBJECT	1
#define CONTEXT_TYPES	2
#define CONTEXT_SHADING	3
#define CONTEXT_EDITING	4
#define CONTEXT_SCRIPT	5
#define CONTEXT_LOGIC	6

/* buts->tab new */
#define TAB_SHADING_MAT 	0
#define TAB_SHADING_TEX 	1
#define TAB_SHADING_RAD 	2
#define TAB_SHADING_WORLD	3
#define TAB_SHADING_LAMP	4

#define TAB_OBJECT_OBJECT	0
#define TAB_OBJECT_PHYSICS 	1
#define TAB_OBJECT_PARTICLE	2

#define TAB_SCENE_RENDER	0
#define TAB_SCENE_WORLD     	1
#define TAB_SCENE_ANIM		2
#define TAB_SCENE_SOUND		3
#define TAB_SCENE_SEQUENCER	4


/* buts->scaflag */		
#define BUTS_SENS_SEL		1
#define BUTS_SENS_ACT		2
#define BUTS_SENS_LINK		4
#define BUTS_CONT_SEL		8
#define BUTS_CONT_ACT		16
#define BUTS_CONT_LINK		32
#define BUTS_ACT_SEL		64
#define BUTS_ACT_ACT		128
#define BUTS_ACT_LINK		256
#define BUTS_SENS_STATE		512
#define BUTS_ACT_STATE		1024


/* buttons grid */
#define PANELX		320
#define PANELY		0
#define PANELW		318
#define PANELH		204
  
#define BUTW1		300
#define BUTW2		145
#define BUTW3		93
#define BUTW4		67
#define ICONBUTW	20
#define BUTH		22
 
#define YSPACE		6
#define XSPACE		10
#define PANEL_YMAX	210
#define PANEL_XMAX	310
 
#define X1CLM1		10
 
#define X2CLM1		X1CLM1
#define X2CLM2		165
 
#define X3CLM1		X1CLM1
#define X3CLM2		113
#define X3CLM3		217

#define X4CLM1		X1CLM1
#define X4CLM2		77
#define X4CLM3		165
#define X4CLM4		232

#if 0
/* start buttons grid doc/reclacement version
/* With this system rows can easily have 1 to 4 buttons
   or create perfectly aligned 1 to 4 columns layouts

     < - - -    PANEL_XMAX  - - - >|
     < - - -      PANELX     - - - >

      .--  XSPACE side padding  --.
      |                           |
      |.- All CLM1                |
      ||     X2CLM2 -.  X3CLM3    |
      ||             |    |       |
     +v|-------------|----|-------v+
     <-/             |    |        |
     | [           But1          ] |     1 button of BUTW1 size
     <-             -/    |        |
     | [   But1    ]=[   But2    ] |     2 buttons of BUTW2 size
     <-                  -/        |
  v  | [ But1  ]=[ But2 ]#[ But3 ] |     3 buttons of BUTW3 size
  |  |                             |  v
  ^  | [But1]#[But2]=[But3]#[But4] |  |  4 buttons of BUTW3 size
  |  <-      ^      ^      -\      |  ^
  |  +-------|------|-------|------+  |
  |          |      |       |         |
  '- YSPACE  '---.--'       '- X4CLM4 '- BUTH
 (row to row)    |
              Padding based in XSPACE (= normal, # +1 pix to make all match)

   Calls like uiBlockBeginAlign/uiBlockEndAlign will make the button
   to button space disappear if needed, forming a compact group, in some themes

   TODO: Figure relations, meaning and usage of
         PANELY, PANEL_YMAX, PANELW, PANELH
 */
#define PANELX		320
#define PANELY		0
#define PANELW		318
#define PANELH		204

#define XSPACE		10
#define YSPACE		6
#define PANEL_XMAX	(PANELX - XSPACE)
#define PANEL_YMAX	210

/* The widths follow 300, 150, 100 and 75, which is nice (discarding spacing)
   sadly spacers and integer rounding make 3 and 4 column complex cases
   so they better be manually set and checked following the comments */
#define BUTW1		(PANELX - (2 * XSPACE))
#define BUTW2		((BUTW1 / 2) - XSPACE)
/* Manual calc so BUTW3 + XSPACE + BUTW3 + (XSPACE+1) + BUTW3 = BUTW1
   Could be something like ((BUTW1/3)-(1+(2*XSPACE)) if starting with 300 */
#define BUTW3		93
/* This time BUTW4 + (XSPACE+1) + BUTW4 + XSPACE + BUTW4 + (XSPACE+1) + BUTW4 = BUTW1
   That would be ((BUTW1/4)-(2+(3*XSPACE)) if starting with 300 */
#define BUTW4		67
/* NOTE: Again, BUTW3 and BUTW4 values and formulas include manual tuning,
   retune if base BUTW1 stops being 300 pixels. You have been warned */
#define ICONBUTW	20
#define BUTH		22

/* X axis start positions of column presets
   First number declares how many columns total
   Second number declares the exact column it controls
   So X3CLM2 means X start position of 2nd button for a row of 3 buttons */
#define X1CLM1		XSPACE

#define X2CLM1		X1CLM1
#define X2CLM2		(X2CLM1 + BUTW2 + XSPACE)

#define X3CLM1		X1CLM1
#define X3CLM2		(X3CLM1 + BUTW3 + XSPACE)
/* By substracting from end we already get the extra 1 pix */
#define X3CLM3		(PANEL_XMAX - BUTW3)

#define X4CLM1		X1CLM1
/* Extra pix to reach the BUTW1 total size */
#define X4CLM2		(X4CLM1 + BUTW4 + XSPACE + 1)
#define X4CLM3		(X4CLM2 + BUTW4 + XSPACE)
/* By substracting from end we already get the other extra 1 pix */
#define X4CLM4		(PANEL_XMAX - BUTW4)
/* end buttons grid doc/replacement version */
#endif /* if 0 */

#endif
