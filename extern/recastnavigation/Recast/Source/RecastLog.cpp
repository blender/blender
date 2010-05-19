//
// Copyright (c) 2009 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "RecastLog.h"
#include <stdio.h>
#include <stdarg.h>

static rcLog* g_log = 0;
static rcBuildTimes* g_btimes = 0;

rcLog::rcLog() :
	m_messageCount(0),
	m_textPoolSize(0)
{
}

rcLog::~rcLog()
{
	if (g_log == this)
		g_log = 0;
}

void rcLog::log(rcLogCategory category, const char* format, ...)
{
	if (m_messageCount >= MAX_MESSAGES)
		return;
	char* dst = &m_textPool[m_textPoolSize];
	int n = TEXT_POOL_SIZE - m_textPoolSize;
	if (n < 2)
		return;
	// Store category
	*dst = (char)category;
	n--;
	// Store message
	va_list ap;
	va_start(ap, format);
	int ret = vsnprintf(dst+1, n-1, format, ap);
	va_end(ap);
	if (ret > 0)
		m_textPoolSize += ret+2;
	m_messages[m_messageCount++] = dst;
}

void rcSetLog(rcLog* log)
{
	g_log = log;
}

rcLog* rcGetLog()
{
	return g_log;
}

void rcSetBuildTimes(rcBuildTimes* btimes)
{
	g_btimes = btimes;
}

rcBuildTimes* rcGetBuildTimes()
{
	return g_btimes;
}
