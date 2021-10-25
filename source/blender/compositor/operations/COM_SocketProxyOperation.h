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

#ifndef _COM_SocketProxyOperation_h_
#define _COM_SocketProxyOperation_h_

#include "COM_NodeOperation.h"

class SocketProxyOperation : public NodeOperation {
public:
	SocketProxyOperation(DataType type, bool use_conversion);
	
	bool isProxyOperation() const { return true; }
	bool useDatatypeConversion() const { return m_use_conversion; }
	
	bool getUseConversion() const { return m_use_conversion; }
	void setUseConversion(bool use_conversion) { m_use_conversion = use_conversion; }
	
private:
	bool m_use_conversion;
};

#endif
