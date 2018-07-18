/*
 * Copyright 2018, Blender Foundation.
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
 * Contributor: Lukas Stockner, Stefan Werner
 */

#ifndef _COM_CryptomatteOperation_h
#define _COM_CryptomatteOperation_h
#include "COM_NodeOperation.h"


class CryptomatteOperation : public NodeOperation {
private:
	std::vector<float> m_objectIndex;
public:
	std::vector<SocketReader *> inputs;

	CryptomatteOperation(size_t num_inputs = 6);

	void initExecution();
	void executePixel(float output[4], int x, int y, void *data);

	void addObjectIndex(float objectIndex);

};
#endif
