
// Gawain immediate mode drawing utilities
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


// Draw 2D rectangles (replaces glRect functions)
// caller is reponsible for vertex format & shader
void immRectf(unsigned pos, float x1, float y1, float x2, float y2);
void immRecti(unsigned pos, int x1, int y1, int x2, int y2);
