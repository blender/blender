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

BL_ActionManager::BL_ActionManager()
{
	for (int i=0; i<MAX_ACTION_LAYERS; ++i)
		m_layers[i] = 0;
}

BL_ActionManager::~BL_ActionManager()
{
	for (int i=0; i<MAX_ACTION_LAYERS; ++i)
		if (m_layers[i])
			StopAction(i);
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

void BL_ActionManager::PlayAction(class KX_GameObject* gameobj,
								const char* name,
								float start,
								float end,
								short layer,
								float blendin,
								short play_mode,
								short blend_mode,
								float playback_speed)
{
	// Remove a currently running action on this layer if there is one
	if (m_layers[layer])
		StopAction(layer);

	// Create a new action
	m_layers[layer] = new BL_Action(gameobj, name, start, end, blendin, play_mode, blend_mode, playback_speed);
}

void BL_ActionManager::StopAction(short layer)
{
	delete m_layers[layer];
	m_layers[layer] = 0;
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
		if (m_layers[i])
		{
			if (m_layers[i]->IsDone())
				StopAction(i);
			else
				m_layers[i]->Update(curtime);
		}
	}
}
