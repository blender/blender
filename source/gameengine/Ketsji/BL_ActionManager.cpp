/*
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
 * Contributor(s): Mitchell Stokes.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BL_ActionManager.cpp
 *  \ingroup ketsji
 */

#include "BL_ActionManager.h"
#include "BL_Action.h"

BL_ActionManager::BL_ActionManager(class KX_GameObject *obj)
{
	for (int i=0; i<MAX_ACTION_LAYERS; ++i)
		m_layers[i] = new BL_Action(obj);
}

BL_ActionManager::~BL_ActionManager()
{
	for (int i=0; i<MAX_ACTION_LAYERS; ++i)
		delete m_layers[i];
}

float BL_ActionManager::GetActionFrame(short layer)
{
	return m_layers[layer]->GetFrame();
}

void BL_ActionManager::SetActionFrame(short layer, float frame)
{
	m_layers[layer]->SetFrame(frame);
}

struct bAction *BL_ActionManager::GetCurrentAction(short layer)
{
	return m_layers[layer]->GetAction();
}

void BL_ActionManager::SetPlayMode(short layer, short mode)
{
	m_layers[layer]->SetPlayMode(mode);
}

void BL_ActionManager::SetTimes(short layer, float start, float end)
{
	m_layers[layer]->SetTimes(start, end);
}

bool BL_ActionManager::PlayAction(const char* name,
								float start,
								float end,
								short layer,
								short priority,
								float blendin,
								short play_mode,
								float layer_weight,
								short ipo_flags,
								float playback_speed,
								short blend_mode)
{
	// Disable layer blending on the first layer
	if (layer == 0) layer_weight = -1.f;

	return m_layers[layer]->Play(name, start, end, priority, blendin, play_mode, layer_weight, ipo_flags, playback_speed, blend_mode);
}

void BL_ActionManager::StopAction(short layer)
{
	m_layers[layer]->Stop();
}

bool BL_ActionManager::IsActionDone(short layer)
{
	return m_layers[layer]->IsDone();
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
