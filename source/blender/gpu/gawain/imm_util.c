
// Gawain immediate mode drawing utilities
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "imm_util.h"
#include "immediate.h"


void immRectf(unsigned pos, float x1, float y1, float x2, float y2)
{
	immBegin(PRIM_TRIANGLE_FAN, 4);
	immVertex2f(pos, x1, y1);
	immVertex2f(pos, x2, y1);
	immVertex2f(pos, x2, y2);
	immVertex2f(pos, x1, y2);
	immEnd();
}

void immRecti(unsigned pos, int x1, int y1, int x2, int y2)
{
	immBegin(PRIM_TRIANGLE_FAN, 4);
	immVertex2i(pos, x1, y1);
	immVertex2i(pos, x2, y1);
	immVertex2i(pos, x2, y2);
	immVertex2i(pos, x1, y2);
	immEnd();
}

#if 0 // more complete version in case we want that
void immRecti_complete(int x1, int y1, int x2, int y2, const float color[4])
{
	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", COMP_I32, 2, CONVERT_INT_TO_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor4fv(color);
	immRecti(pos, x1, y1, x2, y2);
	immUnbindProgram();
}
#endif
