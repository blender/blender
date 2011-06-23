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

#include "BL_ActionManager.h"

BL_ActionManager::BL_ActionManager(class KX_GameObject *obj)
{
	for (int i=0; i<MAX_ACTION_LAYERS; ++i)
		m_layers[i] = new BL_Action(obj);
}

BL_ActionManager::~BL_ActionManager()
{
	for (int i=0; i<MAX_ACTION_LAYERS; ++i)
		if (m_layers[i])
			delete m_layers[i];
}

float BL_ActionManager::GetActionFrame(short layer)
{
	if (m_layers[layer])
		return m_layers[layer]->GetFrame();

	return 0.f;
}

void BL_ActionManager::SetActionFrame(short layer, float frame)
{
	if (m_layers[layer])
		m_layers[layer]->SetFrame(frame);
}

void BL_ActionManager::PlayAction(const char* name,
								float start,
								float end,
								short layer,
								short priority,
								float blendin,
								short play_mode,
								short blend_mode,
								short ipo_flags,
								float playback_speed)
{
	m_layers[layer]->Play(name, start, end, priority, blendin, play_mode, blend_mode, ipo_flags, playback_speed);
}

void BL_ActionManager::StopAction(short layer)
{
	m_layers[layer]->Stop();
}

bool BL_ActionManager::IsActionDone(short layer)
{
	if (m_layers[layer])
		return m_layers[layer]->IsDone();

	return true;
}

void BL_ActionManager::Update(float curtime)
{
	for (int i=0; i<MAX_ACTION_LAYERS; ++i)
	{
		if (!m_layers[i]->IsDone())
		{
			m_layers[i]->Update(curtime);
		}
	}
}
