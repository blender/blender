/* Apache License, Version 2.0 */

#include "testing/testing.h"
#include "BLI_ressource_strings.h"

#define GHASH_INTERNAL_API

extern "C" {
#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_rand.h"
#include "BLI_string.h"
#include "PIL_time_utildefines.h"
}

/* Using http://corpora.uni-leipzig.de/downloads/eng_wikipedia_2010_1M-text.tar.gz
 * (1 million of words, about 122MB of text) from http://corpora.informatik.uni-leipzig.de/download.html */
//#define TEXT_CORPUS_PATH "/path/to/Téléchargements/eng_wikipedia_2010_1M-text/eng_wikipedia_2010_1M-sentences.txt"

/* Resizing the hash has a huge cost over global filling operation! */
//#define GHASH_RESERVE

/* Run the longest tests! */
//#define GHASH_RUN_BIG

/* Size of 'small case' ghash (number of entries). */
#define TESTCASE_SIZE_SMALL 17

#define PRINTF_GHASH_STATS(_gh) \
{ \
	double q, lf, var, pempty, poverloaded; \
	int bigb; \
	q = BLI_ghash_calc_quality_ex((_gh), &lf, &var, &pempty, &poverloaded, &bigb); \
	printf("GHash stats (%u entries):\n\t" \
	       "Quality (the lower the better): %f\n\tVariance (the lower the better): %f\n\tLoad: %f\n\t" \
	       "Empty buckets: %.2f%%\n\tOverloaded buckets: %.2f%% (biggest bucket: %d)\n", \
	       BLI_ghash_len(_gh), q, var, lf, pempty * 100.0, poverloaded * 100.0, bigb); \
} void (0)

/* Str: whole text, lines and words from a 'corpus' text. */

static void str_ghash_tests(GHash *ghash, const char *id)
{
	printf("\n========== STARTING %s ==========\n", id);

#ifdef TEXT_CORPUS_PATH
	size_t sz = 0;
	char *data;
	{
		struct stat st;
		if (stat(TEXT_CORPUS_PATH, &st) == 0)
			sz = st.st_size;
	}
	if (sz != 0) {
		FILE *f = fopen(TEXT_CORPUS_PATH, "r");

		data = (char *)MEM_mallocN(sz + 1, __func__);
		if (fread(data, sizeof(*data), sz, f) != sz) {
			printf("ERROR in reading file %s!", TEXT_CORPUS_PATH);
			MEM_freeN(data);
			data = BLI_strdup(words10k);
		}
		data[sz] = '\0';
		fclose(f);
	}
	else {
		data = BLI_strdup(words10k);
	}
#else
	char *data = BLI_strdup(words10k);
#endif
	char *data_p = BLI_strdup(data);
	char *data_w = BLI_strdup(data);
	char *data_bis = BLI_strdup(data);

	{
		char *p, *w, *c_p, *c_w;

		TIMEIT_START(string_insert);

#ifdef GHASH_RESERVE
		BLI_ghash_reserve(ghash, strlen(data) / 32);  /* rough estimation... */
#endif

		BLI_ghash_insert(ghash, data, POINTER_FROM_INT(data[0]));

		for (p = c_p = data_p, w = c_w = data_w; *c_w; c_w++, c_p++) {
			if (*c_p == '.') {
				*c_p = *c_w = '\0';
				if (!BLI_ghash_haskey(ghash, p)) {
					BLI_ghash_insert(ghash, p, POINTER_FROM_INT(p[0]));
				}
				if (!BLI_ghash_haskey(ghash, w)) {
					BLI_ghash_insert(ghash, w, POINTER_FROM_INT(w[0]));
				}
				p = c_p + 1;
				w = c_w + 1;
			}
			else if (*c_w == ' ') {
				*c_w = '\0';
				if (!BLI_ghash_haskey(ghash, w)) {
					BLI_ghash_insert(ghash, w, POINTER_FROM_INT(w[0]));
				}
				w = c_w + 1;
			}
		}

		TIMEIT_END(string_insert);
	}

	PRINTF_GHASH_STATS(ghash);

	{
		char *p, *w, *c;
		void *v;

		TIMEIT_START(string_lookup);

		v = BLI_ghash_lookup(ghash, data_bis);
		EXPECT_EQ(POINTER_AS_INT(v), data_bis[0]);

		for (p = w = c = data_bis; *c; c++) {
			if (*c == '.') {
				*c = '\0';
				v = BLI_ghash_lookup(ghash, w);
				EXPECT_EQ(POINTER_AS_INT(v), w[0]);
				v = BLI_ghash_lookup(ghash, p);
				EXPECT_EQ(POINTER_AS_INT(v), p[0]);
				p = w = c + 1;
			}
			else if (*c == ' ') {
				*c = '\0';
				v = BLI_ghash_lookup(ghash, w);
				EXPECT_EQ(POINTER_AS_INT(v), w[0]);
				w = c + 1;
			}
		}

		TIMEIT_END(string_lookup);
	}

	BLI_ghash_free(ghash, NULL, NULL);
	MEM_freeN(data);
	MEM_freeN(data_p);
	MEM_freeN(data_w);
	MEM_freeN(data_bis);

	printf("========== ENDED %s ==========\n\n", id);
}

TEST(ghash, TextGHash)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, __func__);

	str_ghash_tests(ghash, "StrGHash - GHash");
}

TEST(ghash, TextMurmur2a)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_strhash_p_murmur, BLI_ghashutil_strcmp, __func__);

	str_ghash_tests(ghash, "StrGHash - Murmur");
}


/* Int: uniform 100M first integers. */

static void int_ghash_tests(GHash *ghash, const char *id, const unsigned int nbr)
{
	printf("\n========== STARTING %s ==========\n", id);

	{
		unsigned int i = nbr;

		TIMEIT_START(int_insert);

#ifdef GHASH_RESERVE
		BLI_ghash_reserve(ghash, nbr);
#endif

		while (i--) {
			BLI_ghash_insert(ghash, POINTER_FROM_UINT(i), POINTER_FROM_UINT(i));
		}

		TIMEIT_END(int_insert);
	}

	PRINTF_GHASH_STATS(ghash);

	{
		unsigned int i = nbr;

		TIMEIT_START(int_lookup);

		while (i--) {
			void *v = BLI_ghash_lookup(ghash, POINTER_FROM_UINT(i));
			EXPECT_EQ(POINTER_AS_UINT(v), i);
		}

		TIMEIT_END(int_lookup);
	}

	{
		void *k, *v;

		TIMEIT_START(int_pop);

		GHashIterState pop_state = {0};

		while (BLI_ghash_pop(ghash, &pop_state, &k, &v)) {
			EXPECT_EQ(k, v);
		}

		TIMEIT_END(int_pop);
	}
	EXPECT_EQ(BLI_ghash_len(ghash), 0);

	BLI_ghash_free(ghash, NULL, NULL);

	printf("========== ENDED %s ==========\n\n", id);
}

TEST(ghash, IntGHash12000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);

	int_ghash_tests(ghash, "IntGHash - GHash - 12000", 12000);
}

#ifdef GHASH_RUN_BIG
TEST(ghash, IntGHash100000000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);

	int_ghash_tests(ghash, "IntGHash - GHash - 100000000", 100000000);
}
#endif

TEST(ghash, IntMurmur2a12000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p_murmur, BLI_ghashutil_intcmp, __func__);

	int_ghash_tests(ghash, "IntGHash - Murmur - 12000", 12000);
}

#ifdef GHASH_RUN_BIG
TEST(ghash, IntMurmur2a100000000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p_murmur, BLI_ghashutil_intcmp, __func__);

	int_ghash_tests(ghash, "IntGHash - Murmur - 100000000", 100000000);
}
#endif

/* Int: random 50M integers. */

static void randint_ghash_tests(GHash *ghash, const char *id, const unsigned int nbr)
{
	printf("\n========== STARTING %s ==========\n", id);

	unsigned int *data = (unsigned int *)MEM_mallocN(sizeof(*data) * (size_t)nbr, __func__);
	unsigned int *dt;
	unsigned int i;

	{
		RNG *rng = BLI_rng_new(0);
		for (i = nbr, dt = data; i--; dt++) {
			*dt = BLI_rng_get_uint(rng);
		}
		BLI_rng_free(rng);
	}

	{
		TIMEIT_START(int_insert);

#ifdef GHASH_RESERVE
		BLI_ghash_reserve(ghash, nbr);
#endif

		for (i = nbr, dt = data; i--; dt++) {
			BLI_ghash_insert(ghash, POINTER_FROM_UINT(*dt), POINTER_FROM_UINT(*dt));
		}

		TIMEIT_END(int_insert);
	}

	PRINTF_GHASH_STATS(ghash);

	{
		TIMEIT_START(int_lookup);

		for (i = nbr, dt = data; i--; dt++) {
			void *v = BLI_ghash_lookup(ghash, POINTER_FROM_UINT(*dt));
			EXPECT_EQ(POINTER_AS_UINT(v), *dt);
		}

		TIMEIT_END(int_lookup);
	}

	BLI_ghash_free(ghash, NULL, NULL);

	printf("========== ENDED %s ==========\n\n", id);
}

TEST(ghash, IntRandGHash12000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);

	randint_ghash_tests(ghash, "RandIntGHash - GHash - 12000", 12000);
}

#ifdef GHASH_RUN_BIG
TEST(ghash, IntRandGHash50000000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);

	randint_ghash_tests(ghash, "RandIntGHash - GHash - 50000000", 50000000);
}
#endif

TEST(ghash, IntRandMurmur2a12000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p_murmur, BLI_ghashutil_intcmp, __func__);

	randint_ghash_tests(ghash, "RandIntGHash - Murmur - 12000", 12000);
}

#ifdef GHASH_RUN_BIG
TEST(ghash, IntRandMurmur2a50000000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p_murmur, BLI_ghashutil_intcmp, __func__);

	randint_ghash_tests(ghash, "RandIntGHash - Murmur - 50000000", 50000000);
}
#endif

static unsigned int ghashutil_tests_nohash_p(const void *p)
{
	return POINTER_AS_UINT(p);
}

static bool ghashutil_tests_cmp_p(const void *a, const void *b)
{
	return a != b;
}

TEST(ghash, Int4NoHash12000)
{
	GHash *ghash = BLI_ghash_new(ghashutil_tests_nohash_p, ghashutil_tests_cmp_p, __func__);

	randint_ghash_tests(ghash, "RandIntGHash - No Hash - 12000", 12000);
}

#ifdef GHASH_RUN_BIG
TEST(ghash, Int4NoHash50000000)
{
	GHash *ghash = BLI_ghash_new(ghashutil_tests_nohash_p, ghashutil_tests_cmp_p, __func__);

	randint_ghash_tests(ghash, "RandIntGHash - No Hash - 50000000", 50000000);
}
#endif

/* Int_v4: 20M of randomly-generated integer vectors. */

static void int4_ghash_tests(GHash *ghash, const char *id, const unsigned int nbr)
{
	printf("\n========== STARTING %s ==========\n", id);

	void *data_v = MEM_mallocN(sizeof(unsigned int[4]) * (size_t)nbr, __func__);
	unsigned int (*data)[4] = (unsigned int (*)[4])data_v;
	unsigned int (*dt)[4];
	unsigned int i, j;

	{
		RNG *rng = BLI_rng_new(0);
		for (i = nbr, dt = data; i--; dt++) {
			for (j = 4; j--; ) {
				(*dt)[j] = BLI_rng_get_uint(rng);
			}
		}
		BLI_rng_free(rng);
	}

	{
		TIMEIT_START(int_v4_insert);

#ifdef GHASH_RESERVE
		BLI_ghash_reserve(ghash, nbr);
#endif

		for (i = nbr, dt = data; i--; dt++) {
			BLI_ghash_insert(ghash, *dt, POINTER_FROM_UINT(i));
		}

		TIMEIT_END(int_v4_insert);
	}

	PRINTF_GHASH_STATS(ghash);

	{
		TIMEIT_START(int_v4_lookup);

		for (i = nbr, dt = data; i--; dt++) {
			void *v = BLI_ghash_lookup(ghash, (void *)(*dt));
			EXPECT_EQ(POINTER_AS_UINT(v), i);
		}

		TIMEIT_END(int_v4_lookup);
	}

	BLI_ghash_free(ghash, NULL, NULL);
	MEM_freeN(data);

	printf("========== ENDED %s ==========\n\n", id);
}

TEST(ghash, Int4GHash2000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_uinthash_v4_p, BLI_ghashutil_uinthash_v4_cmp, __func__);

	int4_ghash_tests(ghash, "Int4GHash - GHash - 2000", 2000);
}

#ifdef GHASH_RUN_BIG
TEST(ghash, Int4GHash20000000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_uinthash_v4_p, BLI_ghashutil_uinthash_v4_cmp, __func__);

	int4_ghash_tests(ghash, "Int4GHash - GHash - 20000000", 20000000);
}
#endif

TEST(ghash, Int4Murmur2a2000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_uinthash_v4_p_murmur, BLI_ghashutil_uinthash_v4_cmp, __func__);

	int4_ghash_tests(ghash, "Int4GHash - Murmur - 2000", 2000);
}

#ifdef GHASH_RUN_BIG
TEST(ghash, Int4Murmur2a20000000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_uinthash_v4_p_murmur, BLI_ghashutil_uinthash_v4_cmp, __func__);

	int4_ghash_tests(ghash, "Int4GHash - Murmur - 20000000", 20000000);
}
#endif

/* MultiSmall: create and manipulate a lot of very small ghashes (90% < 10 items, 9% < 100 items, 1% < 1000 items). */

static void multi_small_ghash_tests_one(GHash *ghash, RNG *rng, const unsigned int nbr)
{
	unsigned int *data = (unsigned int *)MEM_mallocN(sizeof(*data) * (size_t)nbr, __func__);
	unsigned int *dt;
	unsigned int i;

	for (i = nbr, dt = data; i--; dt++) {
		*dt = BLI_rng_get_uint(rng);
	}

#ifdef GHASH_RESERVE
	BLI_ghash_reserve(ghash, nbr);
#endif

	for (i = nbr, dt = data; i--; dt++) {
		BLI_ghash_insert(ghash, POINTER_FROM_UINT(*dt), POINTER_FROM_UINT(*dt));
	}

	for (i = nbr, dt = data; i--; dt++) {
		void *v = BLI_ghash_lookup(ghash, POINTER_FROM_UINT(*dt));
		EXPECT_EQ(POINTER_AS_UINT(v), *dt);
	}

	BLI_ghash_clear(ghash, NULL, NULL);
}

static void multi_small_ghash_tests(GHash *ghash, const char *id, const unsigned int nbr)
{
	printf("\n========== STARTING %s ==========\n", id);

	RNG *rng = BLI_rng_new(0);

	TIMEIT_START(multi_small_ghash);

	unsigned int i = nbr;
	while (i--) {
		const int nbr = 1 + (BLI_rng_get_int(rng) % TESTCASE_SIZE_SMALL) * (!(i % 100) ? 100 : (!(i % 10) ? 10 : 1));
		multi_small_ghash_tests_one(ghash, rng, nbr);
	}

	TIMEIT_END(multi_small_ghash);

	TIMEIT_START(multi_small2_ghash);

	unsigned int i = nbr;
	while (i--) {
		const int nbr = 1 + (BLI_rng_get_int(rng) % TESTCASE_SIZE_SMALL) / 2 * (!(i % 100) ? 100 : (!(i % 10) ? 10 : 1));
		multi_small_ghash_tests_one(ghash, rng, nbr);
	}

	TIMEIT_END(multi_small2_ghash);

	BLI_ghash_free(ghash, NULL, NULL);
	BLI_rng_free(rng);

	printf("========== ENDED %s ==========\n\n", id);
}

TEST(ghash, MultiRandIntGHash2000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);

	multi_small_ghash_tests(ghash, "MultiSmall RandIntGHash - GHash - 2000", 2000);
}

TEST(ghash, MultiRandIntGHash200000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);

	multi_small_ghash_tests(ghash, "MultiSmall RandIntGHash - GHash - 200000", 200000);
}

TEST(ghash, MultiRandIntMurmur2a2000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p_murmur, BLI_ghashutil_intcmp, __func__);

	multi_small_ghash_tests(ghash, "MultiSmall RandIntGHash - Murmur2a - 2000", 2000);
}

TEST(ghash, MultiRandIntMurmur2a200000)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p_murmur, BLI_ghashutil_intcmp, __func__);

	multi_small_ghash_tests(ghash, "MultiSmall RandIntGHash - Murmur2a - 200000", 200000);
}
