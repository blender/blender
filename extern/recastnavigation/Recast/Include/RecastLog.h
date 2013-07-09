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

#ifndef RECAST_LOG_H
#define RECAST_LOG_H

enum rcLogCategory
{
	RC_LOG_PROGRESS = 1,
	RC_LOG_WARNING,
	RC_LOG_ERROR,
};

class rcLog
{
public:
	rcLog();
	~rcLog();
	
	void log(rcLogCategory category, const char* format, ...);
	inline void clear() { m_messageCount = 0; m_textPoolSize = 0; }
	inline int getMessageCount() const { return m_messageCount; }
	inline char getMessageType(int i) const { return *m_messages[i]; }
	inline const char* getMessageText(int i) const { return m_messages[i]+1; }

private:
	static const int MAX_MESSAGES = 1000;
	const char* m_messages[MAX_MESSAGES];
	int m_messageCount;
	static const int TEXT_POOL_SIZE = 8000;
	char m_textPool[TEXT_POOL_SIZE];
	int m_textPoolSize;
};

struct rcBuildTimes
{
	int rasterizeTriangles;
	int buildCompact;
	int buildContours;
	int buildContoursTrace;
	int buildContoursSimplify;
	int filterBorder;
	int filterWalkable;
	int filterMarkReachable;
	int buildPolymesh;
	int buildDistanceField;
	int buildDistanceFieldDist;
	int buildDistanceFieldBlur;
	int buildRegions;
	int buildRegionsReg;
	int buildRegionsExp;
	int buildRegionsFlood;
	int buildRegionsFilter;
	int buildDetailMesh;
	int mergePolyMesh;
	int mergePolyMeshDetail;
};

void rcSetLog(rcLog* log);
rcLog* rcGetLog();

void rcSetBuildTimes(rcBuildTimes* btimes);
rcBuildTimes* rcGetBuildTimes();

#endif // RECAST_LOG_H
