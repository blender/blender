/*
 * Copyright 2011, Blender Foundation.
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
 */

#ifndef _COM_ConvertOperation_h
#define _COM_ConvertOperation_h

#include "COM_NodeOperation.h"


class ConvertBaseOperation : public NodeOperation {
protected:
	SocketReader *m_inputOperation;
	
public:
	ConvertBaseOperation();
	
	void initExecution();
	void deinitExecution();
};


class ConvertValueToColorOperation : public ConvertBaseOperation {
public:
	ConvertValueToColorOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertColorToValueOperation : public ConvertBaseOperation {
public:
	ConvertColorToValueOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertColorToBWOperation : public ConvertBaseOperation {
public:
	ConvertColorToBWOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertColorToVectorOperation : public ConvertBaseOperation {
public:
	ConvertColorToVectorOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertValueToVectorOperation : public ConvertBaseOperation {
public:
	ConvertValueToVectorOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertVectorToColorOperation : public ConvertBaseOperation {
public:
	ConvertVectorToColorOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertVectorToValueOperation : public ConvertBaseOperation {
public:
	ConvertVectorToValueOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertRGBToYCCOperation : public ConvertBaseOperation {
private:
	/** YCbCr mode (Jpeg, ITU601, ITU709) */
	int m_mode;
public:
	ConvertRGBToYCCOperation();

	void executePixel(float output[4], float x, float y, PixelSampler sampler);

	/** Set the YCC mode */
	void setMode(int mode);
};


class ConvertYCCToRGBOperation : public ConvertBaseOperation {
private:
	/** YCbCr mode (Jpeg, ITU601, ITU709) */
	int m_mode;
public:
	ConvertYCCToRGBOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
	
	/** Set the YCC mode */
	void setMode(int mode);
};


class ConvertRGBToYUVOperation : public ConvertBaseOperation {
public:
	ConvertRGBToYUVOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertYUVToRGBOperation : public ConvertBaseOperation {
public:
	ConvertYUVToRGBOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertRGBToHSVOperation : public ConvertBaseOperation {
public:
	ConvertRGBToHSVOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertHSVToRGBOperation : public ConvertBaseOperation {
public:
	ConvertHSVToRGBOperation();
	
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertPremulToStraightOperation : public ConvertBaseOperation {
public:
	ConvertPremulToStraightOperation();

	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class ConvertStraightToPremulOperation : public ConvertBaseOperation {
public:
	ConvertStraightToPremulOperation();

	void executePixel(float output[4], float x, float y, PixelSampler sampler);
};


class SeparateChannelOperation : public NodeOperation {
private:
	SocketReader *m_inputOperation;
	int m_channel;
public:
	SeparateChannelOperation();
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
	
	void initExecution();
	void deinitExecution();
	
	void setChannel(int channel) { this->m_channel = channel; }
};


class CombineChannelsOperation : public NodeOperation {
private:
	SocketReader *m_inputChannel1Operation;
	SocketReader *m_inputChannel2Operation;
	SocketReader *m_inputChannel3Operation;
	SocketReader *m_inputChannel4Operation;
public:
	CombineChannelsOperation();
	void executePixel(float output[4], float x, float y, PixelSampler sampler);
	
	void initExecution();
	void deinitExecution();
};

#endif
