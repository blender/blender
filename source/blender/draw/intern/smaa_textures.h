/* SPDX-License-Identifier: MIT
 * Copyright 2013 Jorge Jimenez <jorge@iryoku.com>
 *           2013 Jose I. Echevarria <joseignacioechevarria@gmail.com>
 *           2013 Belen Masia <bmasia@unizar.es>
 *           2013 Fernando Navarro <fernandn@microsoft.com>
 *           2013 Diego Gutierrez <diegog@unizar.es> */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
