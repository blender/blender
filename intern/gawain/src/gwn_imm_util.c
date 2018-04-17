
// Gawain immediate mode drawing utilities
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gwn_imm_util.h"
#include "gwn_immediate.h"


void immRectf(unsigned pos, float x1, float y1, float x2, float y2)
	{
	immBegin(GWN_PRIM_TRI_FAN, 4);
	immVertex2f(pos, x1, y1);
	immVertex2f(pos, x2, y1);
	immVertex2f(pos, x2, y2);
	immVertex2f(pos, x1, y2);
	immEnd();
	}

void immRecti(unsigned pos, int x1, int y1, int x2, int y2)
	{
	immBegin(GWN_PRIM_TRI_FAN, 4);
	immVertex2i(pos, x1, y1);
	immVertex2i(pos, x2, y1);
	immVertex2i(pos, x2, y2);
	immVertex2i(pos, x1, y2);
	immEnd();
	}

void immRectf_fast_with_color(unsigned pos, unsigned col, float x1, float y1, float x2, float y2, const float color[4])
	{
	immAttrib4fv(col, color);
	immVertex2f(pos, x1, y1);
	immAttrib4fv(col, color);
	immVertex2f(pos, x2, y1);
	immAttrib4fv(col, color);
	immVertex2f(pos, x2, y2);

	immAttrib4fv(col, color);
	immVertex2f(pos, x1, y1);
	immAttrib4fv(col, color);
	immVertex2f(pos, x2, y2);
	immAttrib4fv(col, color);
	immVertex2f(pos, x1, y2);
	}

void immRecti_fast_with_color(unsigned pos, unsigned col, int x1, int y1, int x2, int y2, const float color[4])
	{
	immAttrib4fv(col, color);
	immVertex2i(pos, x1, y1);
	immAttrib4fv(col, color);
	immVertex2i(pos, x2, y1);
	immAttrib4fv(col, color);
	immVertex2i(pos, x2, y2);

	immAttrib4fv(col, color);
	immVertex2i(pos, x1, y1);
	immAttrib4fv(col, color);
	immVertex2i(pos, x2, y2);
	immAttrib4fv(col, color);
	immVertex2i(pos, x1, y2);
	}

#if 0 // more complete version in case we want that
void immRecti_complete(int x1, int y1, int x2, int y2, const float color[4])
	{
	Gwn_VertFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", GWN_COMP_I32, 2, GWN_FETCH_INT_TO_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor4fv(color);
	immRecti(pos, x1, y1, x2, y2);
	immUnbindProgram();
	}
#endif
