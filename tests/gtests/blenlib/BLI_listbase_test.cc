/* Apache License, Version 2.0 */

#include "testing/testing.h"

extern "C" {
#include "BLI_listbase.h"
#include "MEM_guardedalloc.h"
}

TEST(listbase, FindLinkOrIndex)
{
	ListBase lb;
	void *link1 = MEM_callocN(sizeof(Link), "link1");
	void *link2 = MEM_callocN(sizeof(Link), "link2");

	/* Empty list */
	BLI_listbase_clear(&lb);
	EXPECT_EQ(NULL, BLI_findlink(&lb, -1));
	EXPECT_EQ(NULL, BLI_findlink(&lb, 0));
	EXPECT_EQ(NULL, BLI_findlink(&lb, 1));
	EXPECT_EQ(NULL, BLI_rfindlink(&lb, -1));
	EXPECT_EQ(NULL, BLI_rfindlink(&lb, 0));
	EXPECT_EQ(NULL, BLI_rfindlink(&lb, 1));
	EXPECT_EQ(-1, BLI_findindex(&lb, link1));

	/* One link */
	BLI_addtail(&lb, link1);
	EXPECT_EQ(link1, BLI_findlink(&lb, 0));
	EXPECT_EQ(link1, BLI_rfindlink(&lb, 0));
	EXPECT_EQ(0, BLI_findindex(&lb, link1));

	/* Two links */
	BLI_addtail(&lb, link2);
	EXPECT_EQ(link2, BLI_findlink(&lb, 1));
	EXPECT_EQ(link2, BLI_rfindlink(&lb, 0));
	EXPECT_EQ(1, BLI_findindex(&lb, link2));

	BLI_freelistN(&lb);
}
