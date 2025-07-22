/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_rand.h"
#include "BLI_string.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

/* Note that 'common' UTF8 variants of string functions (like copy, etc.) are tested in
 * `BLI_string_test.cc` However, tests below are specific UTF8 conformance ones,
 * and since they eat quite their share of lines, they deserved their own file. */

/**
 * Multi byte defies, use when the exact character isn't important,
 * and when proper handling of multi-byte sequences is needed.
 *
 * Define the first text character found in 1-4 bytes.
 * We might want to generate other string tables too.
 *
 * \code{.py}
 * unicode_max = 0x110000
 * bytes_max = 4
 * sample_chars = dict()
 * for i in range(unicode_max):
 *     ch = chr(i)
 *     try:
 *         byte = ch.encode('utf-8')
 *         byte_len = len(byte)
 *     except UnicodeEncodeError:
 *         continue
 *     if byte_len not in sample_chars:
 *         # Not essential but useful if the character is printable.
 *         if ch.isalpha():
 *             sample_chars[byte_len] = byte
 *         if byte_len == bytes_max:
 *             break
 *
 * assert tuple(sorted(sample_chars.keys())) == tuple(range(1, bytes_max + 1))
 * for i in range(1, bytes_max + 1):
 *     byte = sample_chars[i]
 *     print("#define STR_MB_ALPHA_%d \"" % i, end="")
 *     for x in byte:
 *         print("\\x%02x" % x, end="")
 *     print("\"")
 * \endcode
 */

#define STR_MB_ALPHA_1 "\x41"
#define STR_MB_ALPHA_2 "\xc2\xaa"
#define STR_MB_ALPHA_3 "\xe0\xa0\x80"
#define STR_MB_ALPHA_4 "\xf0\x90\x80\x80"

/* These don't decode into valid code-points and wont work in all UTF8 functions.
 * Use them for functions which support up to #BLI_UTF8_MAX, where failure to test
 * 5 & 6 byte sequences would cause test coverage to be incomplete.
 * See https://stackoverflow.com/a/35027139 for details. */
#define STR_MB_ALPHA_5 "\xf8\x80\x80\x80\x80"
#define STR_MB_ALPHA_6 "\xfc\x80\x80\x80\x80\x80"

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_utf8_invalid_strip
 * \{ */

/* Breaking strings is confusing here, prefer over-long lines. */
/* clang-format off */

/* Each test is made of a 79 bytes (80 with null char) string to test, expected string result after
 * stripping invalid UTF8 bytes, and a single-byte string encoded with expected number of errors.
 *
 * Based on UTF8 decoder stress-test (https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt)
 *     by Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/> - 2015-08-28 - CC BY 4.0
 */
static const char *utf8_invalid_tests[][3] = {
/*    1  Some correct UTF-8 text. */
    {"You should see the Greek word 'kosme':       \"\xce\xba\xe1\xbd\xb9\xcf\x83\xce\xbc\xce\xb5\"                    |",
     "You should see the Greek word 'kosme':       \"\xce\xba\xe1\xbd\xb9\xcf\x83\xce\xbc\xce\xb5\"                    |", "\x00"},

/*    2  Boundary condition test cases
 *    Note that those will pass for us, those are not erroneous unicode code points
 *    (aside from \x00, which is only valid as string terminator).
 *    2.1  First possible sequence of a certain length */
    {"2.1.1  1 byte  (U-00000000):        \"\x00\"                                       |",
     "2.1.1  1 byte  (U-00000000):        \"\"                                       |", "\x01"},
    {"2.1.2  2 bytes (U-00000080):        \"\xc2\x80\"                                      |",
     "2.1.2  2 bytes (U-00000080):        \"\xc2\x80\"                                      |", "\x00"},
    {"2.1.3  3 bytes (U-00000800):        \"\xe0\xa0\x80\"                                     |",
     "2.1.3  3 bytes (U-00000800):        \"\xe0\xa0\x80\"                                     |", "\x00"},
    {"2.1.4  4 bytes (U-00010000):        \"\xf0\x90\x80\x80\"                                    |",
     "2.1.4  4 bytes (U-00010000):        \"\xf0\x90\x80\x80\"                                    |", "\x00"},
    {"2.1.5  5 bytes (U-00200000):        \"\xf8\x88\x80\x80\x80\"                                   |",
     "2.1.5  5 bytes (U-00200000):        \"\xf8\x88\x80\x80\x80\"                                   |", "\x00"},
    {"2.1.6  6 bytes (U-04000000):        \"\xfc\x84\x80\x80\x80\x80\"                                  |",
     "2.1.6  6 bytes (U-04000000):        \"\xfc\x84\x80\x80\x80\x80\"                                  |", "\x00"},
/*    2.2  Last possible sequence of a certain length */
    {"2.2.1  1 byte  (U-0000007F):        \"\x7f\"                                       |",
     "2.2.1  1 byte  (U-0000007F):        \"\x7f\"                                       |", "\x00"},
    {"2.2.2  2 bytes (U-000007FF):        \"\xdf\xbf\"                                      |",
     "2.2.2  2 bytes (U-000007FF):        \"\xdf\xbf\"                                      |", "\x00"},
    {"2.2.3  3 bytes (U-0000FFFF):        \"\xef\xbf\xbf\"                                     |",
     "2.2.3  3 bytes (U-0000FFFF):        \"\"                                     |", "\x03"},  /* matches one of 5.3 sequences... */
    {"2.2.4  4 bytes (U-001FFFFF):        \"\xf7\xbf\xbf\xbf\"                                    |",
     "2.2.4  4 bytes (U-001FFFFF):        \"\xf7\xbf\xbf\xbf\"                                    |", "\x00"},
    {"2.2.5  5 bytes (U-03FFFFFF):        \"\xfb\xbf\xbf\xbf\xbf\"                                   |",
     "2.2.5  5 bytes (U-03FFFFFF):        \"\xfb\xbf\xbf\xbf\xbf\"                                   |", "\x00"},
    {"2.2.6  6 bytes (U-7FFFFFFF):        \"\xfd\xbf\xbf\xbf\xbf\xbf\"                                  |",
     "2.2.6  6 bytes (U-7FFFFFFF):        \"\xfd\xbf\xbf\xbf\xbf\xbf\"                                  |", "\x00"},
/*    2.3  Other boundary conditions */
    {"2.3.1  U-0000D7FF = ed 9f bf = \"\xed\x9f\xbf\"                                          |",
     "2.3.1  U-0000D7FF = ed 9f bf = \"\xed\x9f\xbf\"                                          |", "\x00"},
    {"2.3.2  U-0000E000 = ee 80 80 = \"\xee\x80\x80\"                                          |",
     "2.3.2  U-0000E000 = ee 80 80 = \"\xee\x80\x80\"                                          |", "\x00"},
    {"2.3.3  U-0000FFFD = ef bf bd = \"\xef\xbf\xbd\"                                          |",
     "2.3.3  U-0000FFFD = ef bf bd = \"\xef\xbf\xbd\"                                          |", "\x00"},
    {"2.3.4  U-0010FFFF = f4 8f bf bf = \"\xf4\x8f\xbf\xbf\"                                      |",
     "2.3.4  U-0010FFFF = f4 8f bf bf = \"\xf4\x8f\xbf\xbf\"                                      |", "\x00"},
    {"2.3.5  U-00110000 = f4 90 80 80 = \"\xf4\x90\x80\x80\"                                      |",
     "2.3.5  U-00110000 = f4 90 80 80 = \"\xf4\x90\x80\x80\"                                      |", "\x00"},

/*    3  Malformed sequences
 *    3.1  Unexpected continuation bytes
 *         Each unexpected continuation byte should be separately signaled as a malformed sequence of its own. */
    {"3.1.1  First continuation byte 0x80: \"\x80\"                                      |",
     "3.1.1  First continuation byte 0x80: \"\"                                      |", "\x01"},
    {"3.1.2  Last  continuation byte 0xbf: \"\xbf\"                                      |",
     "3.1.2  Last  continuation byte 0xbf: \"\"                                      |", "\x01"},
    {"3.1.3  2 continuation bytes: \"\x80\xbf\"                                             |",
     "3.1.3  2 continuation bytes: \"\"                                             |", "\x02"},
    {"3.1.4  3 continuation bytes: \"\x80\xbf\x80\"                                            |",
     "3.1.4  3 continuation bytes: \"\"                                            |", "\x03"},
    {"3.1.5  4 continuation bytes: \"\x80\xbf\x80\xbf\"                                           |",
     "3.1.5  4 continuation bytes: \"\"                                           |", "\x04"},
    {"3.1.6  5 continuation bytes: \"\x80\xbf\x80\xbf\x80\"                                          |",
     "3.1.6  5 continuation bytes: \"\"                                          |", "\x05"},
    {"3.1.7  6 continuation bytes: \"\x80\xbf\x80\xbf\x80\xbf\"                                         |",
     "3.1.7  6 continuation bytes: \"\"                                         |", "\x06"},
    {"3.1.8  7 continuation bytes: \"\x80\xbf\x80\xbf\x80\xbf\x80\"                                        |",
     "3.1.8  7 continuation bytes: \"\"                                        |", "\x07"},
/*    3.1.9  Sequence of all 64 possible continuation bytes (0x80-0xbf):            | */
    {"3.1.9      \"\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
                  "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
                  "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
                  "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\" |",
     "3.1.9      \"\" |", "\x40"}, /* NOLINT: modernize-raw-string-literal. */
/*    3.2  Lonely start characters
 *    3.2.1  All 32 first bytes of 2-byte sequences (0xc0-0xdf), each followed by a space character: */
    {"3.2.1      \"\xc0 \xc1 \xc2 \xc3 \xc4 \xc5 \xc6 \xc7 \xc8 \xc9 \xca \xcb \xcc \xcd \xce \xcf "
                  "\xd0 \xd1 \xd2 \xd3 \xd4 \xd5 \xd6 \xd7 \xd8 \xd9 \xda \xdb \xdc \xdd \xde \xdf \" |",
     "3.2.1      \"                                \" |", "\x20"}, /* NOLINT: modernize-raw-string-literal. */
/*    3.2.2  All 16 first bytes of 3-byte sequences (0xe0-0xef), each followed by a space character: */
    {"3.2.2      \"\xe0 \xe1 \xe2 \xe3 \xe4 \xe5 \xe6 \xe7 \xe8 \xe9 \xea \xeb \xec \xed \xee \xef \"                                 |",
     "3.2.2      \"                \"                                 |", "\x10"},
/*    3.2.3  All 8 first bytes of 4-byte sequences (0xf0-0xf7), each followed by a space character: */
    {"3.2.3      \"\xf0 \xf1 \xf2 \xf3 \xf4 \xf5 \xf6 \xf7 \"                                                 |",
     "3.2.3      \"        \"                                                 |", "\x08"},
/*    3.2.4  All 4 first bytes of 5-byte sequences (0xf8-0xfb), each followed by a space character: */
    {"3.2.4      \"\xf8 \xf9 \xfa \xfb \"                                                         |",
     "3.2.4      \"    \"                                                         |", "\x04"},
/*    3.2.5  All 2 first bytes of 6-byte sequences (0xfc-0xfd), each followed by a space character: */
    {"3.2.4      \"\xfc \xfd \"                                                             |",
     "3.2.4      \"  \"                                                             |", "\x02"},
/*    3.3  Sequences with last continuation byte missing
 *         All bytes of an incomplete sequence should be signaled as a single malformed sequence,
 *         i.e., you should see only a single replacement character in each of the next 10 tests.
 *         (Characters as in section 2) */
    {"3.3.1  2-byte sequence with last byte missing (U+0000):     \"\xc0\"               |",
     "3.3.1  2-byte sequence with last byte missing (U+0000):     \"\"               |", "\x01"},
    {"3.3.2  3-byte sequence with last byte missing (U+0000):     \"\xe0\x80\"              |",
     "3.3.2  3-byte sequence with last byte missing (U+0000):     \"\"              |", "\x02"},
    {"3.3.3  4-byte sequence with last byte missing (U+0000):     \"\xf0\x80\x80\"             |",
     "3.3.3  4-byte sequence with last byte missing (U+0000):     \"\"             |", "\x03"},
    {"3.3.4  5-byte sequence with last byte missing (U+0000):     \"\xf8\x80\x80\x80\"            |",
     "3.3.4  5-byte sequence with last byte missing (U+0000):     \"\"            |", "\x04"},
    {"3.3.5  6-byte sequence with last byte missing (U+0000):     \"\xfc\x80\x80\x80\x80\"           |",
     "3.3.5  6-byte sequence with last byte missing (U+0000):     \"\"           |", "\x05"},
    {"3.3.6  2-byte sequence with last byte missing (U-000007FF): \"\xdf\"               |",
     "3.3.6  2-byte sequence with last byte missing (U-000007FF): \"\"               |", "\x01"},
    {"3.3.7  3-byte sequence with last byte missing (U-0000FFFF): \"\xef\xbf\"              |",
     "3.3.7  3-byte sequence with last byte missing (U-0000FFFF): \"\"              |", "\x02"},
    {"3.3.8  4-byte sequence with last byte missing (U-001FFFFF): \"\xf7\xbf\xbf\"             |",
     "3.3.8  4-byte sequence with last byte missing (U-001FFFFF): \"\"             |", "\x03"},
    {"3.3.9  5-byte sequence with last byte missing (U-03FFFFFF): \"\xfb\xbf\xbf\xbf\"            |",
     "3.3.9  5-byte sequence with last byte missing (U-03FFFFFF): \"\"            |", "\x04"},
    {"3.3.10 6-byte sequence with last byte missing (U-7FFFFFFF): \"\xfd\xbf\xbf\xbf\xbf\"           |",
     "3.3.10 6-byte sequence with last byte missing (U-7FFFFFFF): \"\"           |", "\x05"},
/*    3.4  Concatenation of incomplete sequences
 *         All the 10 sequences of 3.3 concatenated, you should see 10 malformed sequences being signaled: */
    {"3.4      \"\xc0\xe0\x80\xf0\x80\x80\xf8\x80\x80\x80\xfc\x80\x80\x80\x80"
                "\xdf\xef\xbf\xf7\xbf\xbf\xfb\xbf\xbf\xbf\xfd\xbf\xbf\xbf\xbf\""
                "                                     |",
     "3.4      \"\"                                     |", "\x1e"},
/*    3.5  Impossible bytes
 *         The following two bytes cannot appear in a correct UTF-8 string */
    {"3.5.1  fe = \"\xfe\"                                                               |",
     "3.5.1  fe = \"\"                                                               |", "\x01"},
    {"3.5.2  ff = \"\xff\"                                                               |",
     "3.5.2  ff = \"\"                                                               |", "\x01"},
    {"3.5.3  fe fe ff ff = \"\xfe\xfe\xff\xff\"                                                   |",
     "3.5.3  fe fe ff ff = \"\"                                                   |", "\x04"},

/*    4  Overlong sequences
 *       The following sequences are not malformed according to the letter of the Unicode 2.0 standard.
 *       However, they are longer then necessary and a correct UTF-8 encoder is not allowed to produce them.
 *       A "safe UTF-8 decoder" should reject them just like malformed sequences for two reasons:
 *       (1) It helps to debug applications if overlong sequences are not treated as valid representations
 *       of characters, because this helps to spot problems more quickly. (2) Overlong sequences provide
 *       alternative representations of characters, that could maliciously be used to bypass filters that check
 *       only for ASCII characters. For instance, a 2-byte encoded line feed (LF) would not be caught by a
 *       line counter that counts only 0x0a bytes, but it would still be processed as a line feed by an unsafe
 *       UTF-8 decoder later in the pipeline. From a security point of view, ASCII compatibility of UTF-8
 *       sequences means also, that ASCII characters are *only* allowed to be represented by ASCII bytes
 *       in the range 0x00-0x7f. To ensure this aspect of ASCII compatibility, use only "safe UTF-8 decoders"
 *       that reject overlong UTF-8 sequences for which a shorter encoding exists.
 *
 *    4.1  Examples of an overlong ASCII character
 *         With a safe UTF-8 decoder, all of the following five overlong representations of the ASCII character
 *         slash ("/") should be rejected like a malformed UTF-8 sequence, for instance by substituting it with
 *         a replacement character. If you see a slash below, you do not have a safe UTF-8 decoder! */
    {"4.1.1  U+002F     = c0 af             = \"\xc0\xaf\"                                  |",
     "4.1.1  U+002F     = c0 af             = \"\"                                  |", "\x02"},
    {"4.1.2  U+002F     = e0 80 af          = \"\xe0\x80\xaf\"                                 |",
     "4.1.2  U+002F     = e0 80 af          = \"\"                                 |", "\x03"},
    {"4.1.3  U+002F     = f0 80 80 af       = \"\xf0\x80\x80\xaf\"                                |",
     "4.1.3  U+002F     = f0 80 80 af       = \"\"                                |", "\x04"},
    {"4.1.4  U+002F     = f8 80 80 80 af    = \"\xf8\x80\x80\x80\xaf\"                               |",
     "4.1.4  U+002F     = f8 80 80 80 af    = \"\"                               |", "\x05"},
    {"4.1.5  U+002F     = fc 80 80 80 80 af = \"\xfc\x80\x80\x80\x80\xaf\"                              |",
     "4.1.5  U+002F     = fc 80 80 80 80 af = \"\"                              |", "\x06"},
/*    4.2  Maximum overlong sequences
 *         Below you see the highest Unicode value that is still resulting in an overlong sequence if represented
 *         with the given number of bytes. This is a boundary test for safe UTF-8 decoders. All five characters
 *         should be rejected like malformed UTF-8 sequences. */
    {"4.2.1  U-0000007F = c1 bf             = \"\xc1\xbf\"                                  |",
     "4.2.1  U-0000007F = c1 bf             = \"\"                                  |", "\x02"},
    {"4.2.2  U-000007FF = e0 9f bf          = \"\xe0\x9f\xbf\"                                 |",
     "4.2.2  U-000007FF = e0 9f bf          = \"\"                                 |", "\x03"},
    {"4.2.3  U-0000FFFF = f0 8f bf bf       = \"\xf0\x8f\xbf\xbf\"                                |",
     "4.2.3  U-0000FFFF = f0 8f bf bf       = \"\"                                |", "\x04"},
    {"4.2.4  U-001FFFFF = f8 87 bf bf bf    = \"\xf8\x87\xbf\xbf\xbf\"                               |",
     "4.2.4  U-001FFFFF = f8 87 bf bf bf    = \"\"                               |", "\x05"},
    {"4.2.5  U+0000     = fc 83 bf bf bf bf = \"\xfc\x83\xbf\xbf\xbf\xbf\"                              |",
     "4.2.5  U+0000     = fc 83 bf bf bf bf = \"\"                              |", "\x06"},
/*    4.3  Overlong representation of the NUL character
 *         The following five sequences should also be rejected like malformed UTF-8 sequences and should not be
 *         treated like the ASCII NUL character. */
    {"4.3.1  U+0000     = c0 80             = \"\xc0\x80\"                                  |",
     "4.3.1  U+0000     = c0 80             = \"\"                                  |", "\x02"},
    {"4.3.2  U+0000     = e0 80 80          = \"\xe0\x80\x80\"                                 |",
     "4.3.2  U+0000     = e0 80 80          = \"\"                                 |", "\x03"},
    {"4.3.3  U+0000     = f0 80 80 80       = \"\xf0\x80\x80\x80\"                                |",
     "4.3.3  U+0000     = f0 80 80 80       = \"\"                                |", "\x04"},
    {"4.3.4  U+0000     = f8 80 80 80 80    = \"\xf8\x80\x80\x80\x80\"                               |",
     "4.3.4  U+0000     = f8 80 80 80 80    = \"\"                               |", "\x05"},
    {"4.3.5  U+0000     = fc 80 80 80 80 80 = \"\xfc\x80\x80\x80\x80\x80\"                              |",
     "4.3.5  U+0000     = fc 80 80 80 80 80 = \"\"                              |", "\x06"},

/*    5  Illegal code positions
 *       The following UTF-8 sequences should be rejected like malformed sequences, because they never represent
 *       valid ISO 10646 characters and a UTF-8 decoder that accepts them might introduce security problems
 *       comparable to overlong UTF-8 sequences.
 *    5.1 Single UTF-16 surrogates */
    {"5.1.1  U+D800 = ed a0 80 = \"\xed\xa0\x80\"                                              |",
     "5.1.1  U+D800 = ed a0 80 = \"\"                                              |", "\x03"},
    {"5.1.2  U+DB7F = ed ad bf = \"\xed\xad\xbf\"                                              |",
     "5.1.2  U+DB7F = ed ad bf = \"\"                                              |", "\x03"},
    {"5.1.3  U+DB80 = ed ae 80 = \"\xed\xae\x80\"                                              |",
     "5.1.3  U+DB80 = ed ae 80 = \"\"                                              |", "\x03"},
    {"5.1.4  U+DBFF = ed af bf = \"\xed\xaf\xbf\"                                              |",
     "5.1.4  U+DBFF = ed af bf = \"\"                                              |", "\x03"},
    {"5.1.5  U+DC00 = ed b0 80 = \"\xed\xb0\x80\"                                              |",
     "5.1.5  U+DC00 = ed b0 80 = \"\"                                              |", "\x03"},
    {"5.1.6  U+DF80 = ed be 80 = \"\xed\xbe\x80\"                                              |",
     "5.1.6  U+DF80 = ed be 80 = \"\"                                              |", "\x03"},
    {"5.1.7  U+DFFF = ed bf bf = \"\xed\xbf\xbf\"                                              |",
     "5.1.7  U+DFFF = ed bf bf = \"\"                                              |", "\x03"},
/*    5.2 Paired UTF-16 surrogates */
    {"5.2.1  U+D800 U+DC00 = ed a0 80 ed b0 80 = \"\xed\xa0\x80\xed\xb0\x80\"                           |",
     "5.2.1  U+D800 U+DC00 = ed a0 80 ed b0 80 = \"\"                           |", "\x06"},
    {"5.2.2  U+D800 U+DFFF = ed a0 80 ed bf bf = \"\xed\xa0\x80\xed\xbf\xbf\"                           |",
     "5.2.2  U+D800 U+DFFF = ed a0 80 ed bf bf = \"\"                           |", "\x06"},
    {"5.2.3  U+DB7F U+DC00 = ed ad bf ed b0 80 = \"\xed\xad\xbf\xed\xb0\x80\"                           |",
     "5.2.3  U+DB7F U+DC00 = ed ad bf ed b0 80 = \"\"                           |", "\x06"},
    {"5.2.4  U+DB7F U+DFFF = ed ad bf ed bf bf = \"\xed\xad\xbf\xed\xbf\xbf\"                           |",
     "5.2.4  U+DB7F U+DFFF = ed ad bf ed bf bf = \"\"                           |", "\x06"},
    {"5.2.5  U+DB80 U+DC00 = ed ae 80 ed b0 80 = \"\xed\xae\x80\xed\xb0\x80\"                           |",
     "5.2.5  U+DB80 U+DC00 = ed ae 80 ed b0 80 = \"\"                           |", "\x06"},
    {"5.2.6  U+DB80 U+DFFF = ed ae 80 ed bf bf = \"\xed\xae\x80\xed\xbf\xbf\"                           |",
     "5.2.6  U+DB80 U+DFFF = ed ae 80 ed bf bf = \"\"                           |", "\x06"},
    {"5.2.7  U+DBFF U+DC00 = ed af bf ed b0 80 = \"\xed\xaf\xbf\xed\xb0\x80\"                           |",
     "5.2.7  U+DBFF U+DC00 = ed af bf ed b0 80 = \"\"                           |", "\x06"},
    {"5.2.8  U+DBFF U+DFFF = ed af bf ed bf bf = \"\xed\xaf\xbf\xed\xbf\xbf\"                           |",
     "5.2.8  U+DBFF U+DFFF = ed af bf ed bf bf = \"\"                           |", "\x06"},
/*    5.3 Non-character code positions
 *        The following "non-characters" are "reserved for internal use" by applications, and according to older versions
 *        of the Unicode Standard "should never be interchanged". Unicode Corrigendum #9 dropped the latter restriction.
 *        Nevertheless, their presence in incoming UTF-8 data can remain a potential security risk, depending
 *        on what use is made of these codes subsequently. Examples of such internal use:
 *          - Some file APIs with 16-bit characters may use the integer value -1 = U+FFFF to signal
 *            an end-of-file (EOF) or error condition.
 *          - In some UTF-16 receivers, code point U+FFFE might trigger a byte-swap operation
 *            (to convert between UTF-16LE and UTF-16BE).
 *        With such internal use of non-characters, it may be desirable and safer to block those code points in
 *        UTF-8 decoders, as they should never occur legitimately in incoming UTF-8 data, and could trigger
 *        unsafe behavior in subsequent processing.
 *
 *        Particularly problematic non-characters in 16-bit applications: */
    {"5.3.1  U+FFFE = ef bf be = \"\xef\xbf\xbe\"                                              |",
     "5.3.1  U+FFFE = ef bf be = \"\"                                              |", "\x03"},
    {"5.3.2  U+FFFF = ef bf bf = \"\xef\xbf\xbf\"                                              |",
     "5.3.2  U+FFFF = ef bf bf = \"\"                                              |", "\x03"},
    /* For now, we ignore those, they do not seem to be crucial anyway... */
/*    5.3.3  U+FDD0 .. U+FDEF
 *    5.3.4  U+nFFFE U+nFFFF (for n = 1..10) */
    {nullptr, nullptr, nullptr},
};
/* clang-format on */

/* BLI_str_utf8_invalid_strip (and indirectly, BLI_str_utf8_invalid_byte). */
TEST(string, Utf8InvalidBytesStrip)
{
  for (int i = 0; utf8_invalid_tests[i][0] != nullptr; i++) {
    const char *tst = utf8_invalid_tests[i][0];
    const char *tst_stripped = utf8_invalid_tests[i][1];
    const int errors_num = int(utf8_invalid_tests[i][2][0]);

    char buff[80];
    memcpy(buff, tst, sizeof(buff));

    const int errors_found_num = BLI_str_utf8_invalid_strip(buff, sizeof(buff) - 1);

    printf("[%02d] -> [%02d] \"%s\"  ->  \"%s\"\n", errors_num, errors_found_num, tst, buff);
    EXPECT_EQ(errors_found_num, errors_num);
    EXPECT_STREQ(buff, tst_stripped);
  }
}

/* BLI_str_utf8_invalid_substitute (and indirectly, BLI_str_utf8_invalid_byte). */
TEST(string, Utf8InvalidBytesSubstitute)
{
  for (int i = 0; utf8_invalid_tests[i][0] != nullptr; i++) {
    const char *tst = utf8_invalid_tests[i][0];
    const int errors_num = int(utf8_invalid_tests[i][2][0]);

    char buff[80];
    memcpy(buff, tst, sizeof(buff));

    const int errors_found_num = BLI_str_utf8_invalid_substitute(buff, sizeof(buff) - 1, '?');

    EXPECT_EQ(errors_found_num, errors_num);
    EXPECT_EQ(BLI_str_utf8_invalid_byte(buff, sizeof(buff) - 1), -1);
    EXPECT_EQ(strlen(buff), sizeof(buff) - 1);
  }
}

TEST(string, Utf8InvalidBytesSubstitutePatterns)
{
#define TEST_SIMPLE(src_chars, expected_error_count, expected_str) \
  { \
    char buff[] = src_chars; \
    EXPECT_EQ(BLI_str_utf8_invalid_substitute(buff, strlen(buff), '?'), expected_error_count); \
    EXPECT_STREQ(buff, expected_str); \
  } \
  ((void)0)

#define ARRAY_ARG(...) __VA_ARGS__

  /* Empty string. */
  TEST_SIMPLE(ARRAY_ARG({0x0}), 0, "");
  /* One good. */
  TEST_SIMPLE(ARRAY_ARG({'A', 0x0}), 0, "A");
  /* One bad. */
  TEST_SIMPLE(ARRAY_ARG({0xff, 0x0}), 1, "?");

  /* Additional patterns. */
  TEST_SIMPLE(ARRAY_ARG({0xe0, 0xef, 0x0}), 2, "??");
  TEST_SIMPLE(ARRAY_ARG({0xe0, 'A', 0x0}), 1, "?A");
  TEST_SIMPLE(ARRAY_ARG({'A', 0xef, 0x0}), 1, "A?");
  TEST_SIMPLE(ARRAY_ARG({0xe0, 'A', 0xed, 0x0}), 2, "?A?");

#undef ARRAY_ARG
#undef TEST_SIMPLE
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_strnlen_utf8
 * \{ */

TEST(string, StringNLenUTF8)
{
  EXPECT_EQ(BLI_strnlen_utf8("", 0), 0);
  EXPECT_EQ(BLI_strnlen_utf8("", 1), 0);
  EXPECT_EQ(BLI_strnlen_utf8(STR_MB_ALPHA_6, 6), 1);
}

TEST(string, StringNLenUTF8_Incomplete)
{
  const char *ref_str =
      STR_MB_ALPHA_1 STR_MB_ALPHA_2 STR_MB_ALPHA_3 STR_MB_ALPHA_4 STR_MB_ALPHA_5 STR_MB_ALPHA_6;
  char buf[22];
  const size_t ref_str_len = 21;

#define EXPECT_BYTE_OFFSET(truncate_ofs, expect_nchars) \
  { \
    size_t buf_ofs = 0; \
    STRNCPY(buf, ref_str); \
    buf[truncate_ofs] = '\0'; \
    EXPECT_EQ(BLI_strnlen_utf8_ex(buf, ref_str_len, &buf_ofs), expect_nchars); \
    EXPECT_EQ(buf_ofs, truncate_ofs); \
  }

  EXPECT_BYTE_OFFSET(0, 0);
  EXPECT_BYTE_OFFSET(1, 1);

  EXPECT_BYTE_OFFSET(2, 2);
  EXPECT_BYTE_OFFSET(3, 2);

  EXPECT_BYTE_OFFSET(4, 3);
  EXPECT_BYTE_OFFSET(5, 3);
  EXPECT_BYTE_OFFSET(6, 3);

  EXPECT_BYTE_OFFSET(7, 4);
  EXPECT_BYTE_OFFSET(8, 4);
  EXPECT_BYTE_OFFSET(9, 4);
  EXPECT_BYTE_OFFSET(10, 4);

  EXPECT_BYTE_OFFSET(11, 5);
  EXPECT_BYTE_OFFSET(12, 5);
  EXPECT_BYTE_OFFSET(13, 5);
  EXPECT_BYTE_OFFSET(14, 5);
  EXPECT_BYTE_OFFSET(15, 5);

  EXPECT_BYTE_OFFSET(16, 6);
  EXPECT_BYTE_OFFSET(17, 6);
  EXPECT_BYTE_OFFSET(18, 6);
  EXPECT_BYTE_OFFSET(19, 6);
  EXPECT_BYTE_OFFSET(20, 6);
  EXPECT_BYTE_OFFSET(21, 6);

#undef EXPECT_BYTE_OFFSET
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_strncpy_utf8
 * \{ */

TEST(string, StrCopyUTF8_ASCII)
{
#define STRNCPY_UTF8_ASCII(...) \
  { \
    const char src[] = {__VA_ARGS__, 0}; \
    char dst[sizeof(src)]; \
    memset(dst, 0xff, sizeof(dst)); \
    STRNCPY_UTF8(dst, src); \
    EXPECT_EQ(strlen(dst), sizeof(dst) - 1); \
    EXPECT_STREQ(dst, src); \
  }

  STRNCPY_UTF8_ASCII('a');
  STRNCPY_UTF8_ASCII('a', 'b', 'c');

#undef STRNCPY_UTF8_ASCII
}

TEST(string, StrCopyUTF8_ASCII_Truncate)
{
#define STRNCPY_UTF8_ASCII_TRUNCATE(maxncpy, ...) \
  { \
    char src[] = {__VA_ARGS__}; \
    char dst[sizeof(src)]; \
    memset(dst, 0xff, sizeof(dst)); \
    BLI_strncpy_utf8(dst, src, maxncpy); \
    int len_expect = std::min<int>(sizeof(src), maxncpy) - 1; \
    src[len_expect] = '\0'; /* To be able to use `EXPECT_STREQ`. */ \
    EXPECT_EQ(strlen(dst), len_expect); \
    EXPECT_STREQ(dst, src); \
  }

  STRNCPY_UTF8_ASCII_TRUNCATE(1, '\0');
  STRNCPY_UTF8_ASCII_TRUNCATE(3, 'A', 'A', 'A', 'A');

#undef STRNCPY_UTF8_ASCII_TRUNCATE
}

TEST(string, StrCopyUTF8_TruncateEncoding)
{
  /* Ensure copying one byte less than the code-point results in it being ignored entirely. */
#define STRNCPY_UTF8_TRUNCATE(byte_size, ...) \
  { \
    const char src[] = {__VA_ARGS__, 0}; \
    EXPECT_EQ(BLI_str_utf8_size_or_error(src), byte_size); \
    char dst[sizeof(src)]; \
    memset(dst, 0xff, sizeof(dst)); \
    STRNCPY_UTF8(dst, src); \
    EXPECT_EQ(strlen(dst), sizeof(dst) - 1); \
    EXPECT_STREQ(dst, src); \
    BLI_strncpy_utf8(dst, src, sizeof(dst) - 1); \
    EXPECT_STREQ(dst, ""); \
  }

  STRNCPY_UTF8_TRUNCATE(6, 252, 1, 1, 1, 1, 1);
  STRNCPY_UTF8_TRUNCATE(5, 248, 1, 1, 1, 1);
  STRNCPY_UTF8_TRUNCATE(4, 240, 1, 1, 1);
  STRNCPY_UTF8_TRUNCATE(3, 224, 1, 1);
  STRNCPY_UTF8_TRUNCATE(2, 192, 1);
  STRNCPY_UTF8_TRUNCATE(1, 96);

#undef STRNCPY_UTF8_TRUNCATE
}

TEST(string, StrCopyUTF8_TruncateEncodingMulti)
{
#define STRNCPY_UTF8_TRUNC_EXPECT(src, dst_expect, dst_maxncpy) \
  { \
    char dst[dst_maxncpy + 1]; \
    dst[dst_maxncpy] = 0xff; \
    size_t len = BLI_strncpy_utf8_rlen(dst, src, dst_maxncpy); \
    EXPECT_EQ(len, strlen(dst)); \
    EXPECT_STREQ(dst, dst_expect); \
    EXPECT_EQ(dst[dst_maxncpy], 0xff); \
  }

  /* Single characters. */
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_1, STR_MB_ALPHA_1, 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_1, "", 1);

  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2, STR_MB_ALPHA_2, 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2, "", 1);

  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3, STR_MB_ALPHA_3, 4);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3, "", 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3, "", 1);

  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, STR_MB_ALPHA_4, 5);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, "", 4);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, "", 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, "", 1);

  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, STR_MB_ALPHA_5, 6);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 5);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 4);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 1);

  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, STR_MB_ALPHA_6, 7);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 6);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 5);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 4);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 1);

  /* Multiple characters. */
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2 STR_MB_ALPHA_2, STR_MB_ALPHA_2 STR_MB_ALPHA_2, 5);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2 STR_MB_ALPHA_2, STR_MB_ALPHA_2, 4);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2 STR_MB_ALPHA_2, STR_MB_ALPHA_2, 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2, "", 1);

  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3 STR_MB_ALPHA_3, STR_MB_ALPHA_3 STR_MB_ALPHA_3, 7);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3 STR_MB_ALPHA_3, STR_MB_ALPHA_3, 6);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3 STR_MB_ALPHA_3, STR_MB_ALPHA_3, 5);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3 STR_MB_ALPHA_3, STR_MB_ALPHA_3, 4);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3 STR_MB_ALPHA_3, "", 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3 STR_MB_ALPHA_3, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3 STR_MB_ALPHA_3, "", 1);

  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4 STR_MB_ALPHA_4, STR_MB_ALPHA_4 STR_MB_ALPHA_4, 9);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4 STR_MB_ALPHA_4, STR_MB_ALPHA_4, 8);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4 STR_MB_ALPHA_4, STR_MB_ALPHA_4, 7);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4 STR_MB_ALPHA_4, STR_MB_ALPHA_4, 6);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4 STR_MB_ALPHA_4, STR_MB_ALPHA_4, 5);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4 STR_MB_ALPHA_4, "", 4);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4 STR_MB_ALPHA_4, "", 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4 STR_MB_ALPHA_4, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4 STR_MB_ALPHA_4, "", 1);

  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, STR_MB_ALPHA_5 STR_MB_ALPHA_5, 11);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, STR_MB_ALPHA_5, 10);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, STR_MB_ALPHA_5, 9);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, STR_MB_ALPHA_5, 8);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, STR_MB_ALPHA_5, 7);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, STR_MB_ALPHA_5, 6);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, "", 5);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, "", 4);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, "", 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5 STR_MB_ALPHA_5, "", 1);

  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, STR_MB_ALPHA_6 STR_MB_ALPHA_6, 13);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, STR_MB_ALPHA_6, 12);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, STR_MB_ALPHA_6, 11);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, STR_MB_ALPHA_6, 10);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, STR_MB_ALPHA_6, 9);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, STR_MB_ALPHA_6, 8);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, STR_MB_ALPHA_6, 7);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, "", 6);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, "", 5);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, "", 4);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, "", 3);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, "", 2);
  STRNCPY_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6 STR_MB_ALPHA_6, "", 1);

#undef STRNCPY_UTF8_TRUNC_EXPECT
}

TEST(string, StrCopyUTF8_TerminateEncodingEarly)
{
  /* A UTF8 sequence that has a null byte before the sequence ends.
   * Ensure the UTF8 sequence does not step over the null byte. */
#define STRNCPY_UTF8_TERMINATE_EARLY(byte_size, ...) \
  { \
    char src[] = {__VA_ARGS__, 0}; \
    EXPECT_EQ(BLI_str_utf8_size_or_error(src), byte_size); \
    char dst[sizeof(src)]; \
    memset(dst, 0xff, sizeof(dst)); \
    STRNCPY_UTF8(dst, src); \
    EXPECT_EQ(strlen(dst), sizeof(dst) - 1); \
    EXPECT_STREQ(dst, src); \
    for (int i = sizeof(dst) - 1; i > 1; i--) { \
      src[i] = '\0'; \
      memset(dst, 0xff, sizeof(dst)); \
      const int dst_copied = STRNCPY_UTF8_RLEN(dst, src); \
      EXPECT_STREQ(dst, src); \
      EXPECT_EQ(strlen(dst), i); \
      EXPECT_EQ(dst_copied, i); \
    } \
  }

  STRNCPY_UTF8_TERMINATE_EARLY(6, 252, 1, 1, 1, 1, 1);
  STRNCPY_UTF8_TERMINATE_EARLY(5, 248, 1, 1, 1, 1);
  STRNCPY_UTF8_TERMINATE_EARLY(4, 240, 1, 1, 1);
  STRNCPY_UTF8_TERMINATE_EARLY(3, 224, 1, 1);
  STRNCPY_UTF8_TERMINATE_EARLY(2, 192, 1);
  STRNCPY_UTF8_TERMINATE_EARLY(1, 96);

#undef STRNCPY_UTF8_TERMINATE_EARLY
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_snprintf_utf8
 * \{ */

TEST(string, StrPrintfUTF8_ASCII)
{
#define SNPRINTF_UTF8_ASCII(...) \
  { \
    const char src[] = {__VA_ARGS__, 0}; \
    char dst[sizeof(src)]; \
    memset(dst, 0xff, sizeof(dst)); \
    SNPRINTF_UTF8(dst, "%s", src); \
    EXPECT_EQ(strlen(dst), sizeof(dst) - 1); \
    EXPECT_STREQ(dst, src); \
  }

  SNPRINTF_UTF8_ASCII('a');
  SNPRINTF_UTF8_ASCII('a', 'b', 'c');

#undef SNPRINTF_UTF8_ASCII
}

TEST(string, StrPrintfUTF8_TerminateEncodingEarly)
{
  /* A UTF8 sequence that has a null byte before the sequence ends.
   * Ensure the UTF8 sequence does not step over the null byte. */
#define SNPRINTF_UTF8_TERMINATE_EARLY(byte_size, ...) \
  { \
    char src[] = {__VA_ARGS__, 0}; \
    EXPECT_EQ(BLI_str_utf8_size_or_error(src), byte_size); \
    char dst[sizeof(src)]; \
    memset(dst, 0xff, sizeof(dst)); \
    SNPRINTF_UTF8(dst, "%s", src); \
    EXPECT_EQ(strlen(dst), sizeof(dst) - 1); \
    EXPECT_STREQ(dst, src); \
    for (int i = sizeof(dst) - 1; i > 1; i--) { \
      src[i] = '\0'; \
      memset(dst, 0xff, sizeof(dst)); \
      const int dst_copied = SNPRINTF_UTF8_RLEN(dst, "%s", src); \
      EXPECT_STREQ(dst, src); \
      EXPECT_EQ(strlen(dst), i); \
      EXPECT_EQ(dst_copied, i); \
    } \
  }

  SNPRINTF_UTF8_TERMINATE_EARLY(6, 252, 1, 1, 1, 1, 1);
  SNPRINTF_UTF8_TERMINATE_EARLY(5, 248, 1, 1, 1, 1);
  SNPRINTF_UTF8_TERMINATE_EARLY(4, 240, 1, 1, 1);
  SNPRINTF_UTF8_TERMINATE_EARLY(3, 224, 1, 1);
  SNPRINTF_UTF8_TERMINATE_EARLY(2, 192, 1);
  SNPRINTF_UTF8_TERMINATE_EARLY(1, 96);

#undef STRNCPY_UTF8_TERMINATE_EARLY
}

TEST(string, StrPrintfUTF8_TruncateEncodingMulti)
{
#define SNPRINTF_UTF8_TRUNC_EXPECT(src, dst_expect, dst_maxncpy) \
  { \
    char dst[dst_maxncpy + 1]; \
    dst[dst_maxncpy] = 0xff; \
    size_t len = BLI_snprintf_utf8_rlen(dst, dst_maxncpy, "%s", src); \
    EXPECT_EQ(len, strlen(dst)); \
    EXPECT_STREQ(dst, dst_expect); \
    EXPECT_EQ(dst[dst_maxncpy], 0xff); \
  }

  /* Single characters. */
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_1, STR_MB_ALPHA_1, 2);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_1, "", 1);

  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2, STR_MB_ALPHA_2, 3);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2, "", 2);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_2, "", 1);

  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3, STR_MB_ALPHA_3, 4);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3, "", 3);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3, "", 2);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_3, "", 1);

  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, STR_MB_ALPHA_4, 5);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, "", 4);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, "", 3);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, "", 2);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_4, "", 1);

  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, STR_MB_ALPHA_5, 6);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 5);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 4);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 3);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 2);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_5, "", 1);

  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, STR_MB_ALPHA_6, 7);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 6);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 5);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 4);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 3);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 2);
  SNPRINTF_UTF8_TRUNC_EXPECT(STR_MB_ALPHA_6, "", 1);

#undef STRNCPY_UTF8_TRUNC_EXPECT
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_utf8_offset_from_index
 * \{ */

TEST(string, Utf8OffsetFromIndex_ClampedIndex)
{
  /* Ensure an index that exceeds the number of multi-byte characters in the
   * string has the same behavior as an index which is clamped by the number of code-points. */
  const char *test_strings[] = {
      "",
      "TEST",
      STR_MB_ALPHA_1,
      STR_MB_ALPHA_2,
      STR_MB_ALPHA_3,
      STR_MB_ALPHA_4,
      STR_MB_ALPHA_1 STR_MB_ALPHA_2 STR_MB_ALPHA_3 STR_MB_ALPHA_4,
      STR_MB_ALPHA_4 STR_MB_ALPHA_3 STR_MB_ALPHA_2 STR_MB_ALPHA_1,
  };
  for (int i = 0; i < ARRAY_SIZE(test_strings); i++) {
    const char *str = test_strings[i];
    const size_t str_len = strlen(str);
    const int str_len_utf8 = BLI_strlen_utf8(str);

    const int str_offset = BLI_str_utf8_offset_from_index(str, str_len, str_len_utf8);
    EXPECT_EQ(BLI_str_utf8_offset_from_index(str, str_len, str_len_utf8 + 1), str_offset);
    EXPECT_EQ(BLI_str_utf8_offset_from_index(str, str_len, str_len_utf8 + 10), str_offset);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_utf8_as_unicode_step_safe
 * \{ */

static size_t utf8_as_char32(const char *str, const char str_len, char32_t *r_result)
{
  size_t i = 0, result_len = 0;
  while ((i < str_len) && (str[i] != '\0')) {
    r_result[result_len++] = BLI_str_utf8_as_unicode_step_safe(str, str_len, &i);
  }
  return i;
}

template<size_t Size, size_t SizeWithPadding>
void utf8_as_char32_test_compare_with_pad_bytes(const char utf8_src[Size])
{
  char utf8_src_with_pad[SizeWithPadding] = {0};

  memcpy(utf8_src_with_pad, utf8_src, Size);

  char32_t unicode_dst_a[Size], unicode_dst_b[Size];

  memset(unicode_dst_a, 0xff, sizeof(unicode_dst_a));
  const size_t index_a = utf8_as_char32(utf8_src, Size, unicode_dst_a);

  /* Test with padded and un-padded size,
   * to ensure that extra available space doesn't yield a different result. */
  for (int pass = 0; pass < 2; pass++) {
    memset(unicode_dst_b, 0xff, sizeof(unicode_dst_b));
    const size_t index_b = utf8_as_char32(
        utf8_src_with_pad, pass ? Size : SizeWithPadding, unicode_dst_b);

    /* Check the resulting content matches. */
    EXPECT_EQ_ARRAY(unicode_dst_a, unicode_dst_b, Size);
    /* Check the index of the source strings match. */
    EXPECT_EQ(index_a, index_b);
  }
}

template<size_t Size> void utf8_as_char32_test_compare(const char utf8_src[Size])
{
  /* Note that 7 is a little arbitrary,
   * chosen since it's the maximum length of multi-byte character + 1
   * to account for any errors that read past null bytes. */
  utf8_as_char32_test_compare_with_pad_bytes<Size, Size + 1>(utf8_src);
  utf8_as_char32_test_compare_with_pad_bytes<Size, Size + 7>(utf8_src);
}

template<size_t Size> void utf8_as_char32_test_at_buffer_size()
{
  char utf8_src[Size];

  /* Test uniform bytes, also with offsets ascending & descending. */
  for (int i = 0; i <= 0xff; i++) {
    memset(utf8_src, i, sizeof(utf8_src));
    utf8_as_char32_test_compare<Size>(utf8_src);

    /* Offset trailing bytes up and down in steps of 1, 2, 4 .. etc. */
    if (Size > 1) {
      for (int mul = 1; mul < 256; mul *= 2) {
        for (int ofs = 1; ofs < int(Size); ofs++) {
          utf8_src[ofs] = char(i + (ofs * mul));
        }
        utf8_as_char32_test_compare<Size>(utf8_src);

        for (int ofs = 1; ofs < int(Size); ofs++) {
          utf8_src[ofs] = char(i - (ofs * mul));
        }
        utf8_as_char32_test_compare<Size>(utf8_src);
      }
    }
  }

  /* Random bytes. */
  RNG *rng = BLI_rng_new(1);
  for (int i = 0; i < 256; i++) {
    BLI_rng_get_char_n(rng, utf8_src, sizeof(utf8_src));
    utf8_as_char32_test_compare<Size>(utf8_src);
  }
  BLI_rng_free(rng);
}

TEST(string, Utf8AsUnicodeStep)
{

  /* Run tests at different buffer sizes. */
  utf8_as_char32_test_at_buffer_size<1>();
  utf8_as_char32_test_at_buffer_size<2>();
  utf8_as_char32_test_at_buffer_size<3>();
  utf8_as_char32_test_at_buffer_size<4>();
  utf8_as_char32_test_at_buffer_size<5>();
  utf8_as_char32_test_at_buffer_size<6>();
  utf8_as_char32_test_at_buffer_size<7>();
  utf8_as_char32_test_at_buffer_size<8>();
  utf8_as_char32_test_at_buffer_size<9>();
  utf8_as_char32_test_at_buffer_size<10>();
  utf8_as_char32_test_at_buffer_size<11>();
  utf8_as_char32_test_at_buffer_size<12>();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf32_empty
 * \{ */

TEST(string, StrCursorStepNextUtf32Empty)
{
  const char32_t empty[] = U"";
  const size_t len = 0;
  int pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf32(empty, len, &pos));
  pos = 1;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf32(empty, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf32_single
 * \{ */

TEST(string, StrCursorStepNextUtf32Single)

{
  const char32_t single[] = U"0";
  const size_t len = 1;
  int pos = 0;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(single, len, &pos) && pos == 1);
  EXPECT_FALSE(BLI_str_cursor_step_next_utf32(single, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf32_simple
 * \{ */

TEST(string, StrCursorStepNextUtf32Simple)
{
  const char32_t simple[] = U"012";
  const size_t len = 3;
  int pos = 0;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(simple, len, &pos) && pos == 1);
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(simple, len, &pos) && pos == 2);
  EXPECT_FALSE(BLI_str_cursor_step_next_utf32(simple, len - 1, &pos));
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(simple, len, &pos) && pos == 3);
  EXPECT_FALSE(BLI_str_cursor_step_next_utf32(simple, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf32_allcombining
 * \{ */

TEST(string, StrCursorStepNextUtf32AllCombining)
{
  const char32_t allcombining[] = U"\u0300\u0300\u0300";
  const size_t len = 3;
  int pos = 0;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(allcombining, len, &pos) && pos == 3);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(allcombining, len, &pos) && pos == 3);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(allcombining, len, &pos) && pos == 3);
  pos = 3;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf32(allcombining, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf32_complex
 * \{ */

TEST(string, StrCursorStepNextUtf32Complex)
{
  /* Combining character, "A", two combining characters, "B". */
  const char32_t complex[] = U"\u0300\u0041\u0300\u0320\u0042";
  const size_t len = 5;
  int pos = 0;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(complex, len, &pos) && pos == 1);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(complex, len, &pos) && pos == 4);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(complex, len, &pos) && pos == 4);
  pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(complex, len, &pos) && pos == 4);
  pos = 4;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(complex, len, &pos) && pos == 5);
  pos = 5;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf32(complex, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf32_invalid
 * \{ */

TEST(string, StrCursorStepNextUtf32Invalid)
{
  /* Latin1 "À", tab, carriage return, linefeed, separated by combining characters. */
  const char32_t invalid[] = U"\u00C0\u0300\u0009\u0300\u000D\u0300\u000A\u0300";
  const size_t len = 8;
  int pos = 0;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(invalid, len, &pos) && pos == 2);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(invalid, len, &pos) && pos == 2);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(invalid, len, &pos) && pos == 4);
  pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(invalid, len, &pos) && pos == 4);
  pos = 4;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(invalid, len, &pos) && pos == 6);
  pos = 5;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(invalid, len, &pos) && pos == 6);
  pos = 6;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(invalid, len, &pos) && pos == 8);
  pos = 7;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf32(invalid, len, &pos) && pos == 8);
  pos = 8;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf32(invalid, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf32_empty
 * \{ */

TEST(string, StrCursorStepPrevUtf32Empty)
{
  const char32_t emtpy[] = U"";
  const size_t len = 0;
  int pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf32(emtpy, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf32_single
 * \{ */

TEST(string, StrCursorStepPrevUtf32Single)
{
  const char32_t single[] = U"0";
  const size_t len = 1;
  int pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(single, len, &pos) && pos == 0);
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf32(single, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf32_simple
 * \{ */

TEST(string, StrCursorStepPrevUtf32Simple)
{
  const char32_t simple[] = U"012";
  const size_t len = 3;
  int pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(simple, len, &pos) && pos == 2);
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(simple, len, &pos) && pos == 1);
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(simple, len, &pos) && pos == 0);
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf32(simple, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf32_allcombining
 * \{ */

TEST(string, StrCursorStepPrevUtf32AllCombining)
{
  const char32_t allcombining[] = U"\u0300\u0300\u0300";
  const size_t len = 3;
  int pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(allcombining, len, &pos) && pos == 0);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(allcombining, len, &pos) && pos == 0);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(allcombining, len, &pos) && pos == 0);
  pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf32(allcombining, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf32_complex
 * \{ */

TEST(string, StrCursorStepPrevUtf32Complex)
{
  /* Combining character, "A", two combining characters, "B". */
  const char32_t complex[] = U"\u0300\u0041\u0300\u0320\u0042";
  const size_t len = 5;
  int pos = 5;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(complex, len, &pos) && pos == 4);
  pos = 4;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(complex, len, &pos) && pos == 1);
  pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(complex, len, &pos) && pos == 1);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(complex, len, &pos) && pos == 1);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(complex, len, &pos) && pos == 0);
  pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf32(complex, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf32_invalid
 * \{ */

TEST(string, StrCursorStepPrevUtf32Invalid)
{
  /* Latin1 "À", tab, carriage return, linefeed, separated by combining characters. */
  const char32_t invalid[] = U"\u00C0\u0300\u0009\u0300\u000D\u0300\u000A\u0300";
  const size_t len = 8;
  int pos = 8;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(invalid, len, &pos) && pos == 6);
  pos = 7;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(invalid, len, &pos) && pos == 6);
  pos = 6;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(invalid, len, &pos) && pos == 4);
  pos = 5;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(invalid, len, &pos) && pos == 4);
  pos = 4;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(invalid, len, &pos) && pos == 2);
  pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(invalid, len, &pos) && pos == 2);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(invalid, len, &pos) && pos == 0);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf32(invalid, len, &pos) && pos == 0);
  pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf32(invalid, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf8_empty
 * \{ */
TEST(string, StrCursorStepNextUtf8Empty)
{
  const char empty[] = "";
  const size_t len = 0;
  int pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf8(empty, len, &pos));
  pos = 1;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf8(empty, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf8_single
 * \{ */
TEST(string, StrCursorStepNextUtf8Single)
{
  const char single[] = "0";
  const size_t len = 1;
  int pos = 0;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(single, len, &pos) && pos == 1);
  EXPECT_FALSE(BLI_str_cursor_step_next_utf8(single, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf8_simple
 * \{ */

TEST(string, StrCursorStepNextUtf8Simple)
{
  const char simple[] = "012";
  const size_t len = 3;
  int pos = 0;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(simple, len, &pos) && pos == 1);
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(simple, len, &pos) && pos == 2);
  EXPECT_FALSE(BLI_str_cursor_step_next_utf8(simple, len - 1, &pos));
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(simple, len, &pos) && pos == 3);
  EXPECT_FALSE(BLI_str_cursor_step_next_utf8(simple, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf8_allcombining
 * \{ */

TEST(string, StrCursorStepNextUtf8AllCombining)
{
  const char allcombining[] = "\xCC\x80\xCC\x80\xCC\x80";
  const size_t len = 6;
  int pos = 0;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(allcombining, len, &pos) && pos == 6);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(allcombining, len, &pos) && pos == 6);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(allcombining, len, &pos) && pos == 6);
  pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(allcombining, len, &pos) && pos == 6);
  pos = 4;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(allcombining, len, &pos) && pos == 6);
  pos = 5;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(allcombining, len, &pos) && pos == 6);
  pos = 6;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf8(allcombining, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf8_complex
 * \{ */

TEST(string, StrCursorStepNextUtf8AllComplex)
{
  /* Combining character, "A", "©", two combining characters, "B". */
  const char complex[] = "\xCC\x80\x41\xC2\xA9\xCC\x80\xCC\xA0\x42";
  const size_t len = 10;
  int pos = 0;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 2);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 2);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 3);
  pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 9);
  pos = 4;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 9);
  pos = 5;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 9);
  pos = 6;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 9);
  pos = 7;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 9);
  pos = 8;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 9);
  pos = 9;
  EXPECT_TRUE(BLI_str_cursor_step_next_utf8(complex, len, &pos) && pos == 10);
  pos = 10;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf8(complex, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_next_utf8_invalid
 * \{ */

TEST(string, StrCursorStepNextUtf8Invalid)
{
  /* Latin1 "À", combining, tab, carriage return, linefeed, combining. */
  const char invalid[] = "\xC0\xCC\x80\x09\x0D\x0A\xCC\x80";
  const size_t len = 8;
  int pos = 0;
  EXPECT_EQ(BLI_str_cursor_step_next_utf8(invalid, len, &pos) ? pos : -1, 3);
  pos = 1;
  EXPECT_EQ(BLI_str_cursor_step_next_utf8(invalid, len, &pos) ? pos : -1, 3);
  pos = 2;
  EXPECT_EQ(BLI_str_cursor_step_next_utf8(invalid, len, &pos) ? pos : -1, 3);
  pos = 3;
  EXPECT_EQ(BLI_str_cursor_step_next_utf8(invalid, len, &pos) ? pos : -1, 4);
  pos = 4;
  EXPECT_EQ(BLI_str_cursor_step_next_utf8(invalid, len, &pos) ? pos : -1, 5);
  pos = 5;
  EXPECT_EQ(BLI_str_cursor_step_next_utf8(invalid, len, &pos) ? pos : -1, 8);
  pos = 6;
  EXPECT_EQ(BLI_str_cursor_step_next_utf8(invalid, len, &pos) ? pos : -1, 8);
  pos = 7;
  EXPECT_EQ(BLI_str_cursor_step_next_utf8(invalid, len, &pos) ? pos : -1, 8);
  pos = 8;
  EXPECT_FALSE(BLI_str_cursor_step_next_utf8(invalid, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf8_empty
 * \{ */

TEST(string, StrCursorStepPrevUtf8Empty)
{
  const char empty[] = "";
  const size_t len = 0;
  int pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf8(empty, len, &pos));
  pos = 1;
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf8(empty, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf8_single
 * \{ */

TEST(string, StrCursorStepPrevUtf8Single)
{
  const char single[] = "0";
  const size_t len = 1;
  int pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(single, len, &pos) && pos == 0);
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf8(single, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf8_single
 * \{ */

TEST(string, StrCursorStepPrevUtf8Simple)
{
  const char simple[] = "012";
  const size_t len = 3;
  int pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(simple, len, &pos) && pos == 2);
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(simple, len, &pos) && pos == 1);
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(simple, len, &pos) && pos == 0);
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf8(simple, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf8_allcombining
 * \{ */

TEST(string, StrCursorStepPrevUtf8AllCombining)
{
  const char allcombining[] = "\xCC\x80\xCC\x80\xCC\x80";
  const size_t len = 6;
  int pos = 6;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(allcombining, len, &pos) && pos == 0);
  pos = 5;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(allcombining, len, &pos) && pos == 0);
  pos = 4;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(allcombining, len, &pos) && pos == 0);
  pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(allcombining, len, &pos) && pos == 0);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(allcombining, len, &pos) && pos == 0);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(allcombining, len, &pos) && pos == 0);
  pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf8(allcombining, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf8_complex
 * \{ */

TEST(string, StrCursorStepPrevUtf8Complex)
{
  /* Combining character, "A", "©", two combining characters, "B". */
  const char complex[] = "\xCC\x80\x41\xC2\xA9\xCC\x80\xCC\xA0\x42";
  const size_t len = 10;
  int pos = 10;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 9);
  pos = 9;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 3);
  pos = 8;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 3);
  pos = 7;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 3);
  pos = 6;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 3);
  pos = 5;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 3);
  pos = 4;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 3);
  pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 2);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 0);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(complex, len, &pos) && pos == 0);
  pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf8(complex, len, &pos));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test #BLI_str_cursor_step_prev_utf8_invalid
 * \{ */

TEST(string, StrCursorStepPrevUtf8Invalid)
{
  /* Latin1 "À", combining, tab, carriage return, linefeed, combining. */
  const char invalid[] = "\xC0\xCC\x80\x09\x0D\x0A\xCC\x80";
  const size_t len = 8;
  int pos = 8;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(invalid, len, &pos) && pos == 5);
  pos = 7;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(invalid, len, &pos) && pos == 5);
  pos = 6;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(invalid, len, &pos) && pos == 5);
  pos = 5;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(invalid, len, &pos) && pos == 4);
  pos = 4;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(invalid, len, &pos) && pos == 3);
  pos = 3;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(invalid, len, &pos) && pos == 0);
  pos = 2;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(invalid, len, &pos) && pos == 0);
  pos = 1;
  EXPECT_TRUE(BLI_str_cursor_step_prev_utf8(invalid, len, &pos) && pos == 0);
  pos = 0;
  EXPECT_FALSE(BLI_str_cursor_step_prev_utf8(invalid, len, &pos));
}

/** \} */
