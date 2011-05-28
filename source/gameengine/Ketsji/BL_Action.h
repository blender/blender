/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BL_ACTION
#define __BL_ACTION

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif


class BL_Action
{
private:
	struct bAction* m_action;
	struct bPose* m_pose;
	struct bPose* m_blendpose;
	struct PointerRNA *m_ptrrna;
	class SG_Controller *m_sg_contr;
	class KX_GameObject* m_obj;

	float m_startframe;
	float m_endframe;
	float m_starttime;
	float m_endtime;
	float m_localtime;

	float m_blendin;
	float m_blendframe;
	float m_blendstart;

	float m_speed;

	short m_playmode;
	short m_blendmode;

	bool m_done;

	void InitIPO();
	void SetLocalTime(float curtime);
public:
	BL_Action(class KX_GameObject* gameobj,
			const char* name,
			float start,
			float end,
			float blendin,
			short play_mode,
			short blend_mode,
			float playback_speed);
	~BL_Action();

	bool IsDone() {return m_done;}
	void Update(float curtime);

	enum 
	{
		ACT_MODE_PLAY = 0,
		ACT_MODE_LOOP,
		ACT_MODE_PING_PONG,
		ACT_MODE_MAX,
	};

	enum
	{
		ACT_BLEND_NONE = 0,
		ACT_BLEND_MIX,
		ACT_BLEND_MAX,
	};

#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:BL_Action"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //BL_ACTION

