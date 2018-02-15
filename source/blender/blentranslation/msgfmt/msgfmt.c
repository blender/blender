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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Bastien Montagne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
 * Based on C++ version by Sergey Sharybin <sergey.vfx@gmail.com>.
 * Based on Python script msgfmt.py from Python source code tree, which was written by
 * Martin v. Löwis <loewis@informatik.hu-berlin.de>
 *
 * Generate binary message catalog from textual translation description.
 *
 * This program converts a textual Uniforum-style message catalog (.po file) into a binary GNU catalog (.mo file).
 * This is essentially the same function as the GNU msgfmt program, however, it is a simpler implementation.
 *
 * Usage: msgfmt input.po output.po
 */

#include <string.h>
#include <stdlib.h>

#include "BLI_utildefines.h"
#include "BLI_dynstr.h"
#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"

#include "MEM_guardedalloc.h"


/* Stupid stub necessary because some BLI files includes winstuff.h, which uses G a bit... */
#ifdef WIN32
  typedef struct Global {
  	void *dummy;
  } Global;

  Global G;
#endif


/* We cannot use NULL char until ultimate step, would give nightmare to our C string processing...
 * Using one of the UTF-8 invalid bytes (as per our BLI string_utf8.c) */
#define NULLSEP_STR "\xff"
#define NULLSEP_CHR '\xff'

typedef enum {
	SECTION_NONE = 0,
	SECTION_CTX  = 1,
	SECTION_ID   = 2,
	SECTION_STR  = 3,
} eSectionType;

typedef struct Message {
	DynStr *ctxt;
	DynStr *id;
	DynStr *str;

	bool is_fuzzy;
} Message;

static char *trim(char *str)
{
	const size_t len = strlen(str);
	size_t i;

	if (len == 0) {
		return str;
	}

	for (i = 0; i < len && ELEM(str[0], ' ', '\t', '\n'); str++, i++);

	char *end = &str[len - 1 - i];
	for (i = len; i > 0 && ELEM(end[0], ' ', '\t', '\n'); end--, i--);
	end[1] = '\0';

	return str;
}

static char *unescape(char *str)
{
	char *curr, *next;
	for (curr = next = str; next[0] != '\0'; curr++, next++) {
		if (next[0] == '\\') {
			switch (next[1]) {
				case '\0':
					/* Get rid of trailing escape char... */
					curr--;
					break;
				case '\\':
					*curr = '\\';
					next++;
					break;
				case 'n':
					*curr = '\n';
					next++;
					break;
				case 't':
					*curr = '\t';
					next++;
					break;
				default:
					/* Get rid of useless escape char. */
					next++;
					*curr = *next;
			}
		}
		else if (curr != next) {
			*curr = *next;
		}
	}
	*curr = '\0';

	if (str[0] == '"' && *(curr - 1) == '"') {
		*(curr - 1) = '\0';
		return str + 1;
	}
	return str;
}

static int qsort_str_cmp(const void *a, const void *b)
{
	return strcmp(*(const char **)a, *(const char **)b);
}

static char **get_keys_sorted(GHash *messages, const uint32_t num_keys)
{
	GHashIterator iter;

	char **keys = MEM_mallocN(sizeof(*keys) * num_keys, __func__);
	char **k = keys;

	GHASH_ITER(iter, messages) {
		*k = BLI_ghashIterator_getKey(&iter);
		k++;
	}

	qsort(keys, num_keys, sizeof(*keys), qsort_str_cmp);

	return keys;
}

BLI_INLINE size_t uint32_to_bytes(const int value, char *bytes)
{
	size_t i;
	for (i = 0; i < sizeof(value); i++) {
		bytes[i] = (char) ((value >> ((int)i * 8)) & 0xff);
	}
	return i;
}

BLI_INLINE size_t msg_to_bytes(char *msg, char *bytes, uint32_t size)
{
	/* Note that we also perform replacing of our NULLSEP placeholder by real NULL char... */
	size_t i;
	for (i = 0; i < size; i++, msg++, bytes++) {
		*bytes = (*msg == NULLSEP_CHR) ? '\0' : *msg;
	}
	return i;
}

typedef struct Offset {
	uint32_t key_offset, key_len, val_offset, val_len;
} Offset;

/* Return the generated binary output. */
static char *generate(GHash *messages, size_t *r_output_size)
{
	const uint32_t num_keys = BLI_ghash_len(messages);

	/* Get list of sorted keys. */
	char **keys = get_keys_sorted(messages, num_keys);
	char **vals = MEM_mallocN(sizeof(*vals) * num_keys, __func__);
	uint32_t tot_keys_len = 0;
	uint32_t tot_vals_len = 0;

	Offset *offsets = MEM_mallocN(sizeof(*offsets) * num_keys, __func__);

	for (int i = 0; i < num_keys; i++) {
		Offset *off = &offsets[i];

		vals[i] = BLI_ghash_lookup(messages, keys[i]);

		/* For each string, we need size and file offset.
		 * Each string is NULL terminated; the NULL does not count into the size. */
		off->key_offset = tot_keys_len;
		off->key_len = (uint32_t)strlen(keys[i]);
		tot_keys_len += off->key_len + 1;

		off->val_offset = tot_vals_len;
		off->val_len = (uint32_t)strlen(vals[i]);
		tot_vals_len += off->val_len + 1;
	}

	/* The header is 7 32-bit unsigned integers. then comes the keys index table, then the values index table. */
	const uint32_t idx_keystart = 7 * 4;
	const uint32_t idx_valstart = idx_keystart + 8 * num_keys;
	/* We don't use hash tables, so the keys start right after the index tables. */
	const uint32_t keystart = idx_valstart + 8 * num_keys;
	/* and the values start after the keys */
	const uint32_t valstart = keystart + tot_keys_len;

	/* Final buffer representing the binary MO file. */
	*r_output_size = valstart + tot_vals_len;
	char *output = MEM_mallocN(*r_output_size, __func__);
	char *h = output;
	char *ik = output + idx_keystart;
	char *iv = output + idx_valstart;
	char *k = output + keystart;
	char *v = output + valstart;

	h += uint32_to_bytes(0x950412de, h);  /* Magic */
	h += uint32_to_bytes(0x0, h);  /* Version */
	h += uint32_to_bytes(num_keys, h);  /* Number of entries */
	h += uint32_to_bytes(idx_keystart, h);  /* Start of key index */
	h += uint32_to_bytes(idx_valstart, h);  /* Start of value index */
	h += uint32_to_bytes(0, h);  /* Size of hash table */
	h += uint32_to_bytes(0, h);  /* Offset of hash table */

	BLI_assert(h == ik);

	for (int i = 0; i < num_keys; i++) {
		Offset *off = &offsets[i];

		/* The index table first has the list of keys, then the list of values.
		 * Each entry has first the size of the string, then the file offset. */
		ik += uint32_to_bytes(off->key_len, ik);
		ik += uint32_to_bytes(off->key_offset + keystart, ik);
		iv += uint32_to_bytes(off->val_len, iv);
		iv += uint32_to_bytes(off->val_offset + valstart, iv);

		k += msg_to_bytes(keys[i], k, off->key_len + 1);
		v += msg_to_bytes(vals[i], v, off->val_len + 1);
	}

	BLI_assert(ik == output + idx_valstart);
	BLI_assert(iv == output + keystart);
	BLI_assert(k == output + valstart);

	MEM_freeN(keys);
	MEM_freeN(vals);
	MEM_freeN(offsets);

	return output;
}

/* Add a non-fuzzy translation to the dictionary. */
static void add(GHash *messages, MemArena *memarena, const Message *msg)
{
	const size_t msgctxt_len = (size_t)BLI_dynstr_get_len(msg->ctxt);
	const size_t msgid_len = (size_t)BLI_dynstr_get_len(msg->id);
	const size_t msgstr_len = (size_t)BLI_dynstr_get_len(msg->str);
	const size_t msgkey_len = msgid_len  + ((msgctxt_len == 0) ? 0 : msgctxt_len + 1);

	if (!msg->is_fuzzy && msgstr_len != 0) {
		char *msgkey = BLI_memarena_alloc(memarena, sizeof(*msgkey) * (msgkey_len + 1));
		char *msgstr = BLI_memarena_alloc(memarena, sizeof(*msgstr) * (msgstr_len + 1));

		if (msgctxt_len != 0) {
			BLI_dynstr_get_cstring_ex(msg->ctxt, msgkey);
			msgkey[msgctxt_len] = '\x04';  /* Context/msgid separator */
			BLI_dynstr_get_cstring_ex(msg->id, &msgkey[msgctxt_len + 1]);
		}
		else {
			BLI_dynstr_get_cstring_ex(msg->id, msgkey);
		}

		BLI_dynstr_get_cstring_ex(msg->str, msgstr);

		BLI_ghash_insert(messages, msgkey, msgstr);
	}
}


static void clear(Message *msg)
{
	BLI_dynstr_clear(msg->ctxt);
	BLI_dynstr_clear(msg->id);
	BLI_dynstr_clear(msg->str);
	msg->is_fuzzy = false;
}

static int make(const char *input_file_name, const char *output_file_name)
{
	GHash *messages = BLI_ghash_new(BLI_ghashutil_strhash_p_murmur, BLI_ghashutil_strcmp, __func__);
	MemArena *msgs_memarena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

	const char *msgctxt_kw = "msgctxt";
	const char *msgid_kw = "msgid";
	const char *msgid_plural_kw = "msgid_plural";
	const char *msgstr_kw = "msgstr";
	const size_t msgctxt_len = strlen(msgctxt_kw);
	const size_t msgid_len = strlen(msgid_kw);
	const size_t msgid_plural_len = strlen(msgid_plural_kw);
	const size_t msgstr_len = strlen(msgstr_kw);

	/* Note: For now, we assume file encoding is always utf-8. */

	eSectionType section = SECTION_NONE;
	bool is_plural = false;

	Message msg = {
	    .ctxt = BLI_dynstr_new_memarena(),
	    .id = BLI_dynstr_new_memarena(),
	    .str = BLI_dynstr_new_memarena(),
	    .is_fuzzy = false,
	};

	LinkNode *input_file_lines = BLI_file_read_as_lines(input_file_name);
	LinkNode *ifl = input_file_lines;

	/* Parse the catalog. */
	for (int lno = 1; ifl; ifl = ifl->next, lno++) {
		char *l = ifl->link;
		const bool is_comment = (l[0] == '#');
		/* If we get a comment line after a msgstr, this is a new entry. */
		if (is_comment) {
			if (section == SECTION_STR) {
				add(messages, msgs_memarena, &msg);
				clear(&msg);
				section = SECTION_NONE;
			}
			/* Record a fuzzy mark. */
			if (l[1] == ',' && strstr(l, "fuzzy") != NULL) {
				msg.is_fuzzy = true;
			}
			/* Skip comments */
			continue;
		}
		if (strstr(l, msgctxt_kw) == l) {
			if (section == SECTION_STR) {
				/* New message, output previous section. */
				add(messages, msgs_memarena, &msg);
			}
			if (!ELEM(section, SECTION_NONE, SECTION_STR)) {
				printf("msgctxt not at start of new message on %s:%d\n", input_file_name, lno);
				return EXIT_FAILURE;
			}
			section = SECTION_CTX;
			l = l + msgctxt_len;
			clear(&msg);
		}
		else if (strstr(l, msgid_plural_kw) == l) {
			/* This is a message with plural forms. */
			if (section != SECTION_ID) {
				printf("msgid_plural not preceeded by msgid on %s:%d\n", input_file_name, lno);
				return EXIT_FAILURE;
			}
			l = l + msgid_plural_len;
			BLI_dynstr_append(msg.id, NULLSEP_STR);  /* separator of singular and plural */
			is_plural = true;
		}
		else if (strstr(l, msgid_kw) == l) {
			if (section == SECTION_STR) {
				add(messages, msgs_memarena, &msg);
			}
			if (section != SECTION_CTX) {
				clear(&msg);
			}
			section = SECTION_ID;
			l = l + msgid_len;
			is_plural = false;
		}
		else if (strstr(l, msgstr_kw) == l) {
			l = l + msgstr_len;
			// Now we are in a msgstr section
			section = SECTION_STR;
			if (l[0] == '[') {
				if (!is_plural) {
					printf("plural without msgid_plural on %s:%d\n", input_file_name, lno);
					return EXIT_FAILURE;
				}
				if ((l = strchr(l, ']')) == NULL) {
					printf("Syntax error on %s:%d\n", input_file_name, lno);
					return EXIT_FAILURE;
				}
				if (BLI_dynstr_get_len(msg.str) != 0) {
					BLI_dynstr_append(msg.str, NULLSEP_STR);  /* Separator of the various plural forms. */
				}
			}
			else {
				if (is_plural) {
					printf("indexed msgstr required for plural on %s:%d\n", input_file_name, lno);
					return EXIT_FAILURE;
				}
			}
		}
		/* Skip empty lines. */
		l = trim(l);
		if (l[0] == '\0') {
			if (section == SECTION_STR) {
				add(messages, msgs_memarena, &msg);
				clear(&msg);
			}
			section = SECTION_NONE;
			continue;
		}
		l = unescape(l);
		if (section == SECTION_CTX) {
			BLI_dynstr_append(msg.ctxt, l);
		}
		else if (section == SECTION_ID) {
			BLI_dynstr_append(msg.id, l);
		}
		else if (section == SECTION_STR) {
			BLI_dynstr_append(msg.str, l);
		}
		else {
			printf("Syntax error on %s:%d\n", input_file_name, lno);
			return EXIT_FAILURE;
		}
	}
	/* Add last entry */
	if (section == SECTION_STR) {
		add(messages, msgs_memarena, &msg);
	}

	BLI_dynstr_free(msg.ctxt);
	BLI_dynstr_free(msg.id);
	BLI_dynstr_free(msg.str);
	BLI_file_free_lines(input_file_lines);

	/* Compute output */
	size_t output_size;
	char *output = generate(messages, &output_size);

	FILE *fp = BLI_fopen(output_file_name, "wb");
	fwrite(output, 1, output_size, fp);
	fclose(fp);

	MEM_freeN(output);
	BLI_ghash_free(messages, NULL, NULL);
	BLI_memarena_free(msgs_memarena);

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: %s <input.po> <output.mo>\n", argv[0]);
		return EXIT_FAILURE;
	}
	const char *input_file = argv[1];
	const char *output_file = argv[2];

	return make(input_file, output_file);
}
