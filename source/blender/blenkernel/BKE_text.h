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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_TEXT_H__
#define __BKE_TEXT_H__

/** \file BKE_text.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Text;
struct TextLine;
struct SpaceText;

void			BKE_text_free		(struct Text *text);
void 			txt_set_undostate	(int u);
int 			txt_get_undostate	(void);
struct Text*	add_empty_text	(const char *name);
int				txt_extended_ascii_as_utf8(char **str);
int				reopen_text		(struct Text *text);
struct Text*	add_text		(const char *file, const char *relpath); 
struct Text*	BKE_text_copy		(struct Text *ta);
void			BKE_text_unlink		(struct Main *bmain, struct Text *text);
void			clear_text(struct Text *text);
void			write_text(struct Text *text, const char *str);

char*	txt_to_buf			(struct Text *text);
void	txt_clean_text		(struct Text *text);
void	txt_order_cursors	(struct Text *text);
int		txt_find_string		(struct Text *text, const char *findstr, int wrap, int match_case);
int		txt_has_sel			(struct Text *text);
int		txt_get_span		(struct TextLine *from, struct TextLine *to);
int		txt_utf8_offset_to_index(const char *str, int offset);
int		txt_utf8_index_to_offset(const char *str, int index);
void	txt_move_up			(struct Text *text, short sel);
void	txt_move_down		(struct Text *text, short sel);
void	txt_move_left		(struct Text *text, short sel);
void	txt_move_right		(struct Text *text, short sel);
void	txt_jump_left		(struct Text *text, short sel);
void	txt_jump_right		(struct Text *text, short sel);
void	txt_move_bof		(struct Text *text, short sel);
void	txt_move_eof		(struct Text *text, short sel);
void	txt_move_bol		(struct Text *text, short sel);
void	txt_move_eol		(struct Text *text, short sel);
void	txt_move_toline		(struct Text *text, unsigned int line, short sel);
void	txt_move_to			(struct Text *text, unsigned int line, unsigned int ch, short sel);
void	txt_pop_sel			(struct Text *text);
void	txt_delete_char		(struct Text *text);
void	txt_delete_word		(struct Text *text);
void	txt_delete_selected	(struct Text *text);
void	txt_sel_all			(struct Text *text);
void	txt_sel_line		(struct Text *text);
char*	txt_sel_to_buf		(struct Text *text);
void	txt_insert_buf		(struct Text *text, const char *in_buffer);
void	txt_print_undo		(struct Text *text);
void	txt_undo_add_toop	(struct Text *text, int op, unsigned int froml, unsigned short fromc, unsigned int tol, unsigned short toc);
void	txt_do_undo			(struct Text *text);
void	txt_do_redo			(struct Text *text);
void	txt_split_curline	(struct Text *text);
void	txt_backspace_char	(struct Text *text);
void	txt_backspace_word	(struct Text *text);
int		txt_add_char		(struct Text *text, unsigned int add);
int		txt_add_raw_char	(struct Text *text, unsigned int add);
int		txt_replace_char	(struct Text *text, unsigned int add);
void	txt_unindent		(struct Text *text);
void 	txt_comment			(struct Text *text);
void 	txt_indent			(struct Text *text);
void	txt_uncomment		(struct Text *text);
void	txt_move_lines		(struct Text *text, const int direction);
void	txt_duplicate_line	(struct Text *text);
int	setcurr_tab_spaces	(struct Text *text, int space);

void	txt_add_marker						(struct Text *text, struct TextLine *line, int start, int end, const unsigned char color[4], int group, int flags);
short	txt_clear_marker_region				(struct Text *text, struct TextLine *line, int start, int end, int group, int flags);
short	txt_clear_markers					(struct Text *text, int group, int flags);
struct TextMarker	*txt_find_marker		(struct Text *text, struct TextLine *line, int curs, int group, int flags);
struct TextMarker	*txt_find_marker_region	(struct Text *text, struct TextLine *line, int start, int end, int group, int flags);
struct TextMarker	*txt_prev_marker		(struct Text *text, struct TextMarker *marker);
struct TextMarker	*txt_next_marker		(struct Text *text, struct TextMarker *marker);

/* utility functions, could be moved somewhere more generic but are python/text related  */
int text_check_bracket(const char ch);
int text_check_delim(const char ch);
int text_check_digit(const char ch);
int text_check_identifier(const char ch);
int text_check_whitespace(const char ch);

enum {
	TXT_MOVE_LINE_UP   = -1,
	TXT_MOVE_LINE_DOWN =  1
};


/* Undo opcodes */

/* Simple main cursor movement */
#define UNDO_CLEFT		001
#define UNDO_CRIGHT		002
#define UNDO_CUP		003
#define UNDO_CDOWN		004

/* Simple selection cursor movement */
#define UNDO_SLEFT		005
#define UNDO_SRIGHT		006
#define UNDO_SUP		007
#define UNDO_SDOWN		010

/* Complex movement (opcode is followed
 * by 4 character line ID + a 2 character
 * position ID and opcode (repeat)) */
#define UNDO_CTO		011
#define UNDO_STO		012

/* Complex editing */
/* 1 - opcode is followed by 1 byte for ascii character and opcode (repeat)) */
/* 2 - opcode is followed by 2 bytes for utf-8 character and opcode (repeat)) */
/* 3 - opcode is followed by 3 bytes for utf-8 character and opcode (repeat)) */
/* 4 - opcode is followed by 4 bytes for unicode character and opcode (repeat)) */
#define UNDO_INSERT_1   013
#define UNDO_INSERT_2   014
#define UNDO_INSERT_3   015
#define UNDO_INSERT_4   016

#define UNDO_BS_1       017
#define UNDO_BS_2       020
#define UNDO_BS_3       021
#define UNDO_BS_4       022

#define UNDO_DEL_1      023
#define UNDO_DEL_2      024
#define UNDO_DEL_3      025
#define UNDO_DEL_4      026

/* Text block (opcode is followed
 * by 4 character length ID + the text
 * block itself + the 4 character length
 * ID (repeat) and opcode (repeat)) */
#define UNDO_DBLOCK     027 /* Delete block */
#define UNDO_IBLOCK     030 /* Insert block */

/* Misc */
#define UNDO_SWAP       031	/* Swap cursors */

#define UNDO_INDENT     032
#define UNDO_UNINDENT   033
#define UNDO_COMMENT    034
#define UNDO_UNCOMMENT  035

#define UNDO_MOVE_LINES_UP      036
#define UNDO_MOVE_LINES_DOWN    037

#define UNDO_DUPLICATE  040

/* Marker flags */
#define TMARK_TEMP		0x01	/* Remove on non-editing events, don't save */
#define TMARK_EDITALL	0x02	/* Edit all markers of the same group as one */

#ifdef __cplusplus
}
#endif

#endif
