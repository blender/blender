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

#ifndef _COM_ChannelInfo_h
#define _COM_ChannelInfo_h

#include <vector>
#include "BKE_text.h"
#include <string>
#include "DNA_node_types.h"
#include "BLI_rect.h"

using namespace std;

/**
  * @brief List of possible channel types
  * @ingroup Model
  */
typedef enum ChannelType {
	COM_CT_ColorComponent /** @brief this channel is contains color information. Specific used is determined by channelnumber, and in the future color space */,
	COM_CT_Alpha /** @brief this channel is contains transparency value */,
	COM_CT_Value /** @brief this channel is contains a value */,
	COM_CT_X /** @brief this channel is contains a X value */,
	COM_CT_Y /** @brief this channel is contains a Y value */,
	COM_CT_Z /** @brief this channel is contains a Z value */,
	COM_CT_W /** @brief this channel is contains a W value */,
	COM_CT_UNUSED /** @brief this channel is unused */
} ChannelType;

/**
  * @brief ChannelInfo holds information about a channel.
  *
  * Channels are transported from node to node via a SocketConnection.
  * ChannelInfo holds specific setting of these channels in order that the to-node of the connection
  * Can handle specific logic per channel setting.
  *
  * @note currently this is not used, but a future place to implement color spacing and other things.
  * @ingroup Model
  */
class ChannelInfo {
private:
	/**
	  * @brief the channel number, in the connection. [0-3]
	  */
	int number;

	/**
	  * @brief type of channel
	  */
	ChannelType type;

	/**
	  * @brieg Is this value in this channel premultiplied with its alpha
	  * @note only valid if type = ColorComponent;
	  */
	bool premultiplied;

//	/**
//	  * Color space of this value.
//	  * only valid when type = ColorComponent;
//	  */
//	string colorspacename;

public:
	/**
	  * @brief creates a new ChannelInfo and set default values
	  */
	ChannelInfo();

	/**
	  * @brief set the index of this channel in the SocketConnection
	  */
	void setNumber(const int number) { this->number = number; }

	/**
	  * @brief get the index of this channel in the SocketConnection
	  */
	const int getNumber() const {return this->number; }

	/**
	  * @brief set the type of channel
	  */
	void setType(const ChannelType type) { this->type = type; }

	/**
	  * @brief get the type of channel
	  */
	const ChannelType getType() const {return this->type; }

	/**
	  * @brief set the premultiplicatioin of this channel
	  */
	void setPremultiplied(const bool premultiplied) { this->premultiplied = premultiplied; }

	/**
	  * @brief is this channel premultiplied
	  */
	const bool isPremultiplied() const {return this->premultiplied;}
};


#endif
