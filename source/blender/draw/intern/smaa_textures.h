/**
 * Copyright (C) 2013 Jorge Jimenez <jorge@iryoku.com>
 * Copyright (C) 2013 Jose I. Echevarria <joseignacioechevarria@gmail.com>
 * Copyright (C) 2013 Belen Masia <bmasia@unizar.es>
 * Copyright (C) 2013 Fernando Navarro <fernandn@microsoft.com>
 * Copyright (C) 2013 Diego Gutierrez <diegog@unizar.es>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. As clarification, there
 * is no requirement that the copyright notice and permission be included in
 * binary distributions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#define AREATEX_WIDTH 160
#define AREATEX_HEIGHT 560
#define AREATEX_PITCH (AREATEX_WIDTH * 2)
#define AREATEX_SIZE (AREATEX_HEIGHT * AREATEX_PITCH)

/**
 * Stored in R8G8 format. Load it in the following format:
 *  - DX10: DXGI_FORMAT_R8G8_UNORM
 */
extern const unsigned char areaTexBytes[];

#define SEARCHTEX_WIDTH 64
#define SEARCHTEX_HEIGHT 16
#define SEARCHTEX_PITCH SEARCHTEX_WIDTH
#define SEARCHTEX_SIZE (SEARCHTEX_HEIGHT * SEARCHTEX_PITCH)

/**
 * Stored in R8 format. Load it in the following format:
 *  - DX10: DXGI_FORMAT_R8_UNORM
 */
extern const unsigned char searchTexBytes[];
