/*
 * Copyright 2012, Blender Foundation.
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
 * Contributor:
 *		Jeroen Bakker
 *		Monique Dewanchand
 *		Sergey Sharybin
 */

#ifndef __COM_KEYINGCLIPOPERATION_H__
#define __COM_KEYINGCLIPOPERATION_H__

#include "COM_NodeOperation.h"

/**
 * Class with implementation of black/white clipping for keying node
 */
class KeyingClipOperation : public NodeOperation {
protected:
	float m_clipBlack;
	float m_clipWhite;

	int m_kernelRadius;
	float m_kernelTolerance;

	bool m_isEdgeMatte;
public:
	KeyingClipOperation();

	void setClipBlack(float value) {this->m_clipBlack = value;}
	void setClipWhite(float value) {this->m_clipWhite = value;}

	void setKernelRadius(int value) {this->m_kernelRadius = value;}
	void setKernelTolerance(float value) {this->m_kernelTolerance = value;}

	void setIsEdgeMatte(bool value) {this->m_isEdgeMatte = value;}

	void *initializeTileData(rcti *rect);

	void executePixel(float output[4], int x, int y, void *data);

	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};

#endif
