/* Main command processing loop.

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2012 Todd M. Lewis and Sebastiano Vigna

	This file is part of ne, the nice editor.

	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or (at your
	option) any later version.

	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
	for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <http://www.gnu.org/licenses/>.  */


#include "ne.h"
#include "version.h"
#include <limits.h>

/* ne's temporary file name template for the THROUGH command. */

#define NE_TMP "netmp.XXXXXX"

/* Turns an unspecified integer argument (-1) to 1. This
is what most commands require. */

#define NORMALIZE(x)  { x = (x)<0 ? 1 : (x); }

/* The length of the static message buffer. It must be larger by a factor
	of about three of the maximum screen width as UTF-8 encoded characters
	might take several characters per screen position. */

#define MAX_MESSAGE_SIZE (1024)

/* Here, given a mask represent a user flag and an integer i, we do as follows:
	i < 0 : toggle flag;
	i = 0 : clear flag;
	i > 0 : set flag;
*/

#define SET_USER_FLAG(b,i,x) {\
	if ((i)<0) (b)->x = !(b)->x;\
	else (b)->x = ((i) != 0);\
}
#define SET_GLOBAL_FLAG(c,f) {\
	if ((c)<0) (f) = !(f);\
	else (f) = ((c) != 0);\
}


/* Converts a non-positive result from request_number() to OK if the
function was aborted or not-a-number error if an invalid number was read. */

#define NUMERIC_ERROR(c) ((c) == ABORT ? OK : NOT_A_NUMBER)




/* This is the dispatcher of all actions that have some effect on the text.

	The arguments are an action to be executed, a possible integer parameter and
	a possible string parameter. -1 and NULL are, respectively, reserved values
	meaning "no argument". For most operations, the integer argument is the
	number of repetitions. When an on/off choice is required, nonzero means on,
	zero means off, no argument means toggle.

	If there is a string argument (i.e. p != NULL), it is assumed that the
	action will consume p -- it ends up being free()d or stored
	somewhere. Though efficient, this has lead to some memory leaks (can you
	find them?). */


int do_action(buffer *b, action a, int c, unsigned char *p) {
	static char msg[MAX_MESSAGE_SIZE];
	line_desc *next_ld;
	HIGHLIGHT_STATE next_line_state;
	int i, error = OK, recording, col;
	char *q;

	assert_buffer(b);
	assert_buffer_content(b);
	assert(b->encoding != ENC_UTF8 || b->cur_pos >= b->cur_line_desc->line_len || utf8len(b->cur_line_desc->line[b->cur_pos]) > 0);

	stop = FALSE;

	if (b->recording) record_action(b->cur_macro, a, c, p, verbose_macros);

	switch(a) {
		
	case EXIT_A:
		if (save_all_modified_buffers()) {
			print_error(CANT_SAVE_EXIT_SUSPENDED);
			return ERROR;
		}
		else {
			close_history();
			unset_interactive_mode();
			exit(0);
		}
		return OK;

	case PUSHPREFS_A:
		NORMALIZE(c);
		for (i = 0; i < c && !(error = push_prefs(b)) && !stop; i++);
		return stop ? STOPPED : error ;

	case POPPREFS_A:
		NORMALIZE(c);
		for (i = 0; i < c && !(error = pop_prefs(b)) && !stop; i++);
		return stop ? STOPPED : error ;

	case QUIT_A:
		if (modified_buffers() && !request_response(b, info_msg[SOME_DOCUMENTS_ARE_NOT_SAVED], FALSE)) return ERROR;
		close_history();
		unset_interactive_mode();
		exit(0);

	case LINEUP_A:
		NORMALIZE(c);
		for(i = 0; i < c && !(error = line_up(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case LINEDOWN_A:
	
		NORMALIZE(c);
		for(i = 0; i < c && !(error = line_down(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case PREVPAGE_A:
	
		NORMALIZE(c);
		for(i = 0; i < c && !(error = prev_page(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case NEXTPAGE_A:
		
		NORMALIZE(c);
		for(i = 0; i < c && !(error = next_page(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case MOVELEFT_A:
		NORMALIZE(c);
		for(i = 0; i < c && !(error = char_left(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case MOVERIGHT_A:
		NORMALIZE(c);
		for(i = 0; i < c && !(error = char_right(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case MOVESOL_A:
		move_to_sol(b);
		return OK;

	case MOVEEOL_A:
		move_to_eol(b);
		return OK;

	case MOVESOF_A:
		move_to_sof(b);
		return OK;

	case MOVEEOF_A:
		delay_update();
		move_to_bof(b);
		move_to_eol(b);
		return OK;

	case PAGEUP_A:
		NORMALIZE(c);
		for(i = 0; i < c && !(error = page_up(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case PAGEDOWN_A:
		NORMALIZE(c);
		for(i = 0; i < c && !(error = page_down(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case MOVETOS_A:
		error = move_tos(b);
		return error;

	case MOVEBOS_A:
		error = move_bos(b);
		return error;

	case ADJUSTVIEW_A:
		NORMALIZE(c);
		error = adjust_view(b,p);
		if (p) free(p);
		return error;

	case TOGGLESEOF_A:
		toggle_sof_eof(b);
		return OK;

	case TOGGLESEOL_A:
		toggle_sol_eol(b);
		return OK;

	case NEXTWORD_A:
		NORMALIZE(c);
		for(i = 0; i < c && !(error = search_word(b, 1)) && !stop; i++);
		return stop ? STOPPED : error;

	case PREVWORD_A:
		NORMALIZE(c);
		for(i = 0; i < c && !(error = search_word(b, -1)) && !stop; i++);
		return stop ? STOPPED : error;

   case DELETEPREVWORD_A:
		recording = b->recording;
		b->recording = 0;
		NORMALIZE(c);
		delay_update();
		start_undo_chain(b);
		for(i = 0; i < c && !error && !stop; i++) {
				int right_line = b->cur_line;
				int right_pos  = b->cur_pos;
				if(!(error = do_action(b, PREVWORD_A, 1, NULL))) {
					int left_line = b->cur_line;
					int left_pos  = b->cur_pos;
						goto_line(b, right_line);
					goto_pos(b, right_pos);
					while(!error && !stop && (b->cur_line > left_line || b->cur_pos > left_pos)) {
						error = do_action(b, BACKSPACE_A, 1, NULL);
					}
				}
			}
		end_undo_chain(b);
		b->recording = recording;
		return stop ? STOPPED : error;

   case DELETENEXTWORD_A:
		recording = b->recording;
		b->recording = 0;
		NORMALIZE(c);
		delay_update();
		start_undo_chain(b);
		for(i = 0; i < c && !error && !stop; i++) {
				int left_line = b->cur_line;
				int left_pos  = b->cur_pos;
				if(!(error = do_action(b, NEXTWORD_A, 1, NULL))) {
					while(!error && !stop && (b->cur_line > left_line || b->cur_pos > left_pos)) {
						error = do_action(b, BACKSPACE_A, 1, NULL);
					}
				}
			}
		end_undo_chain(b);
		b->recording = recording;
		return stop ? STOPPED : error;

	case MOVEEOW_A:
		move_to_eow(b);
		return OK;

	case MOVEINCUP_A:
		move_inc_up(b);
		return OK;

	case MOVEINCDOWN_A:
		move_inc_down(b);
		return OK;

	case UNSETBOOKMARK_A:
		if (p && p[0]=='*' && !p[1]) { /* Special parm "*" for UNSETBOOKMARK_A */
			b->bookmark_mask = b->cur_bookmark = 0;
			print_message("All BookMarks cleared.");
			free(p);
			return OK;
		} /* Intentionally fall through to regular BOOKMARK parm parsing. */
	case SETBOOKMARK_A:
	case GOTOBOOKMARK_A:
		{
			int relative = 0;
			/* *p can be  "", "-", "0".."9", "+1","-1", for which, respectively, */
			/*  c becomes  1,  0,   1 .. 10, next,prev. Anything else is out of range. */
			if (p) {
				if ((p[0]=='+' || p[0]=='-') && p[1]=='1') {
					if (b->cur_bookmark<1 || b->cur_bookmark>=NUM_BOOKMARKS) b->cur_bookmark = 1;
					for (i=0; i<NUM_BOOKMARKS-1; i++) {
						b->cur_bookmark = (b->cur_bookmark-1+NUM_BOOKMARKS-1+(p[0]=='+'?1:-1))%(NUM_BOOKMARKS-1)+1;
						if ((a==SETBOOKMARK_A?~b->bookmark_mask:b->bookmark_mask) & (1<<b->cur_bookmark)) {
							c = b->cur_bookmark;
							relative = 1;
							break;
						}
					}
					if (i==NUM_BOOKMARKS-1) {
						free(p);
						switch (a) {
						case SETBOOKMARK_A:
							return NO_UNSET_BOOKMARKS_TO_SET;
						case GOTOBOOKMARK_A:
							return NO_SET_BOOKMARKS_TO_GOTO;
						default:
							return NO_SET_BOOKMARKS_TO_UNSET;
						}
					}
				} else if (p[0]) {
					if (!p[1]) {
						if (*p == '-') c = 0;
						else c = *p - '0' + 1;
					}
					else c = -1;
				}
				else c = 1;
				free(p);
				if (c < 0 || c >= NUM_BOOKMARKS) return INVALID_BOOKMARK_DESIGNATION;
			}
			else 
				c = 1;
			switch(a) {
			case SETBOOKMARK_A:
				b->bookmark[c].pos = b->cur_pos;
				b->bookmark[c].line = b->cur_line;
				b->bookmark[c].cur_y = b->cur_y;
				b->bookmark_mask |= (1 << c);
				b->cur_bookmark = c;
				snprintf(msg, MAX_MESSAGE_SIZE, "Bookmark %c set", c>0?'0'+c-1 : '-');
				print_message(msg);
				break;
			case UNSETBOOKMARK_A:
				if (! (b->bookmark_mask & (1 << c)))
					return BOOKMARK_NOT_SET;
				b->bookmark_mask &= ~(1 << c);
				snprintf(msg, MAX_MESSAGE_SIZE, "Bookmark %c unset", c>0?'0'+c-1 : '-');
				print_message(msg);
				break;
			case GOTOBOOKMARK_A:
				if (! (b->bookmark_mask & (1 << c))) return BOOKMARK_NOT_SET;
				else {
					const int prev_line = b->cur_line;
					const int prev_pos  = b->cur_pos;
					const int cur_y     = b->cur_y;
					b->cur_bookmark = c;
					int  avshift;
					delay_update();
					goto_line(b, b->bookmark[c].line);
					goto_pos(b, b->bookmark[c].pos);
					if (avshift = b->cur_y - b->bookmark[c].cur_y) {
						snprintf(msg, MAX_MESSAGE_SIZE, "%c%d", avshift > 0 ? 'T' :'B', avshift > 0 ? avshift : -avshift);
						adjust_view(b,msg);
					}
					b->bookmark[0].line  = prev_line;
					b->bookmark[0].pos   = prev_pos;
					b->bookmark[0].cur_y = cur_y;
					b->bookmark_mask |= 1;
					if (relative) {
						snprintf(msg, MAX_MESSAGE_SIZE, "At Bookmark %c", c>0?'0'+c-1 : '-');
						print_message(msg);
					}
				}
			}
			return OK;
		}
	
	case GOTOLINE_A:
		if (c < 0 && (c = request_number("Line", b->cur_line + 1))<0) return NUMERIC_ERROR(c);
		if (c == 0 || c > b->num_lines) c = b->num_lines;
		goto_line(b, --c);
		return OK;
	
	
	case GOTOCOLUMN_A:
		if (c < 0 && (c = request_number("Column", b->cur_x + b->win_x + 1))<0) return NUMERIC_ERROR(c);
		goto_column(b, c ? --c : 0);
		return OK;
	
	case INSERTSTRING_A:
		/* Since we are going to call another action, we do not want to record this insertion twice. */
		recording= b->recording;
		b->recording = 0;
		error = ERROR;
		if (p || (p = request_string("String", NULL, FALSE, FALSE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
			encoding_type encoding = detect_encoding(p, strlen(p));
			error = OK;
			start_undo_chain(b);
			
			/* We cannot rely on encoding promotion done by INSERTCHAR_A, because it could work
				just for part of the string if UTF-8 auto-detection is not enabled. */
			
			if (b->encoding == ENC_ASCII || encoding == ENC_ASCII || (b->encoding == encoding)) {
				if (b->encoding == ENC_ASCII) b->encoding = encoding;
				for(i = 0; p[i] && error == OK; i = next_pos(p, i, encoding)) error = do_action(b, INSERTCHAR_A, get_char(&p[i], encoding), NULL);
			}
			else error = INVALID_STRING;
			end_undo_chain(b);
			free(p);
		}
		b->recording = recording;
		return error;	

	case TABS_A:
		SET_USER_FLAG(b, c, opt.tabs);
		return OK;

	case SHIFTTABS_A:
		SET_USER_FLAG(b, c, opt.shift_tabs);
		return OK;

	case AUTOMATCHBRACKET_A:
		if (c < 0 && (c = request_number("Match mode (sum of 0:none, 1:brightness, 2:inverse, 4:bold, 8:underline)", b->opt.automatch))<0||c>15) return ((c) == ABORT ? OK : INVALID_MATCH_MODE);
		b->opt.automatch = c;
		return OK;

	case INSERTTAB_A:
		recording = b->recording;
		b->recording = 0;
		NORMALIZE(c);
		start_undo_chain(b);
		if (b->opt.tabs) {
			while (c-- > 0) {
				error = do_action(b, INSERTCHAR_A, '\t', NULL);
			}
		}
		else {
			while (c-- > 0) {
				do {
					error = do_action(b, INSERTCHAR_A, ' ', NULL);
				} while (b->opt.tab_size && (b->win_x + b->cur_x) % b->opt.tab_size);
			}
		}
		end_undo_chain(b);
		b->recording = recording;
		return error;

	case INSERTCHAR_A: {

		static int last_inserted_char = ' ';
		int deleted_char, old_char, error = ERROR;

		if (b->opt.read_only) return FILE_IS_READ_ONLY;
		
		if (c < 0 && (c = request_number("Char Code", last_inserted_char))<0) return NUMERIC_ERROR(c);
		if (c == 0) return CANT_INSERT_0;

		if (b->encoding == ENC_ASCII) {
			if (c > 0xFF) b->encoding = ENC_UTF8;
			else if (c > 0x7F) b->encoding = b->opt.utf8auto ? ENC_UTF8 : ENC_8_BIT;
		}
		if (c > 0xFF && b->encoding == ENC_8_BIT) return INVALID_CHARACTER;
		
		last_inserted_char = c;
		
		old_char = b->cur_pos < b->cur_line_desc->line_len ? get_char(&b->cur_line_desc->line[b->cur_pos], b->encoding) : 0;

		/* Freeze the line attributes before any real update. */
		if (b->syn && b->attr_len < 0) freeze_attributes(b, b->cur_line_desc);
			
		start_undo_chain(b);

		if (deleted_char = !b->opt.insert && b->cur_pos < b->cur_line_desc->line_len) delete_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos);
		if (b->cur_pos > b->cur_line_desc->line_len) {
			/* We insert spaces to reach the insertion position. */
			insert_spaces(b, b->cur_line_desc, b->cur_line, b->cur_line_desc->line_len, b->cur_pos - b->cur_line_desc->line_len);
			if (b->syn) update_line(b, b->cur_y, TRUE, TRUE);
		}
		
		insert_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos, c);
		
		end_undo_chain(b); 
		need_attr_update = TRUE;
		
		/* At this point the line has been modified: note that if we are in overwrite mode and write a character
			at or beyond the length of the current line, we are actually doing an insertion. */
		
		if (!deleted_char) update_inserted_char(b, c, b->cur_line_desc, b->cur_pos, b->cur_char, b->cur_y, b->cur_x);
		else update_overwritten_char(b, old_char, c, b->cur_line_desc, b->cur_pos, b->cur_char, b->cur_y, b->cur_x);

		char_right(b);

		/* Note the use of ne_columns-1. This avoids a double horizontal scrolling each time a
			word wrap happens with b->opt.right_margin = 0. */

		if (b->opt.word_wrap && b->win_x + b->cur_x >= (b->opt.right_margin ? b->opt.right_margin : ne_columns - 1)) error = word_wrap(b);


		if (error == ERROR) {
			assert_buffer_content(b);
			/* No word wrap. */
			if (b->syn) update_line(b, b->cur_y, TRUE, FALSE);
			assert_buffer_content(b);
		}		
		else {		
			/* Fixes in case of word wrapping. */
			int wont_scroll = b->win_x == 0;
			int a = 0;
			if (b->syn) update_line(b, b->cur_y, FALSE, TRUE);
			else update_partial_line(b, b->cur_y, calc_width(b->cur_line_desc, b->cur_line_desc->line_len, b->opt.tab_size, b->encoding) - b->win_x, FALSE, FALSE);	

			need_attr_update = FALSE;
			/* Poke the correct state into the next line. */
			if (b->syn) ((line_desc *)b->cur_line_desc->ld_node.next)->highlight_state = b->next_state;

			if (b->opt.auto_indent) a = auto_indent_line(b, b->cur_line + 1, (line_desc *)b->cur_line_desc->ld_node.next, INT_MAX);
			move_to_sol(b);
			line_down(b);
			goto_pos(b, error + a);
                                                               
			if (wont_scroll) {
				if (b->cur_line == b->num_lines - 1) update_line(b, b->cur_y, FALSE, FALSE);
				else scroll_window(b, b->cur_y, 1);
			}

			need_attr_update = TRUE;
			assert_buffer_content(b);
		}

		assert_buffer_content(b);
		return OK;
	}


	case BACKSPACE_A:
	case DELETECHAR_A:
		
		if (b->opt.read_only) return FILE_IS_READ_ONLY;

		NORMALIZE(c);
		
		start_undo_chain(b);
		for(i = 0; i < c && !stop; i++) {
			if (a == BACKSPACE_A) {
				if (b->cur_pos == 0) {
					if (b->cur_line == 0) {
						/* Start of buffer. We just return an error. */
						end_undo_chain(b);
						return ERROR;
					}
					/* We turn a backspace at the start of a line into a delete 
						at the end of the previous line. */
					char_left(b);
				}
				else {
					if (!b->opt.tabs && (b->win_x + b->cur_x) % b->opt.tab_size == 0
						&& (b->cur_pos > b->cur_line_desc->line_len || b->cur_line_desc->line[b->cur_pos - 1] == ' ')) {
						/* We are deleting one or more spaces from a tabbing position. We go left until the
						previous tabbing, or when spaces end. */
						do char_left(b); while((b->win_x + b->cur_x) % b->opt.tab_size != 0 
															&& (b->cur_pos > b->cur_line_desc->line_len || b->cur_line_desc->line[b->cur_pos - 1] == ' '));
					}
					else char_left(b);
					/* If we are not over text, we are in free form mode; the backspace
						is turned into moving to the left. */
					if (b->cur_pos >= b->cur_line_desc->line_len) continue;
				}
			}
			
			/* From here, we just implement a delete. */
			
			if (!b->opt.tabs && b->cur_pos < b->cur_line_desc->line_len && b->cur_line_desc->line[b->cur_pos] == ' ' && 
				((b->win_x + b->cur_x) % b->opt.tab_size == 0 || b->cur_line_desc->line[b->cur_pos - 1] != ' ')) {
				col = 0;
				do col++; while((b->win_x + b->cur_x + col) % b->opt.tab_size != 0 
					&& b->cur_pos + col < b->cur_line_desc->line_len && b->cur_line_desc->line[b->cur_pos + col] == ' ');
				/* We are positioned at the start of the block of col spaces. If there is at most
					one character to delete, we can just go on. Otherwise, we replace the block with a 
					TAB, doing some magick to keep everything in sync. */
				if (col > 1 && (b->win_x + b->cur_x + col) % b->opt.tab_size == 0) {
					if (b->syn) {
						freeze_attributes(b, b->cur_line_desc);
						memmove(b->attr_buf + b->cur_pos + 1, b->attr_buf + b->cur_pos + col, b->attr_len - (b->cur_pos + col));
						b->attr_buf[b->cur_pos] = -1;
						b->attr_len -= (col - 1);
					}
					delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, col);
					insert_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos, '\t');
					if (b->syn) update_partial_line(b, b->cur_y, b->cur_x, TRUE, TRUE);	
				}
			}

			if (b->cur_pos > b->cur_line_desc->line_len) {
				col = b->win_x + b->cur_x;
				/* We are not over text; we must be in FreeForm mode.
				   We're deleting past the end of the line, so if we aren't on the last line
				   we need to pad this line with space up to col, then fall through to the
				   delete_one_char() below. */
				if (b->cur_line_desc->ld_node.next->next == NULL) continue;
				if (b->cur_line_desc->line_len == 0) {
					auto_indent_line(b, b->cur_line, b->cur_line_desc, col);
					resync_pos(b);
				}
				/* We need spaces if the line was not empty, or if we were sitting in the middle of a TAB. */
				insert_spaces(b, b->cur_line_desc, b->cur_line, b->cur_line_desc->line_len, col - calc_width(b->cur_line_desc, b->cur_line_desc->line_len, b->opt.tab_size, b->encoding));
				if (b->syn) freeze_attributes(b, b->cur_line_desc);
			}

			if (b->syn && b->attr_len < 0) freeze_attributes(b, b->cur_line_desc);

			if (b->cur_pos < b->cur_line_desc->line_len) {
				/* Deletion inside a line. */
				const int old_char = b->encoding == ENC_UTF8 ? utf8char(&b->cur_line_desc->line[b->cur_pos]) : b->cur_line_desc->line[b->cur_pos];
				const int old_attr = b->syn ? b->attr_buf[b->cur_pos] : 0;
				delete_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos);
				
				update_deleted_char(b, old_char, old_attr, b->cur_line_desc, b->cur_pos, b->cur_char, b->cur_y, b->cur_x);	
				if (b->syn) update_line(b, b->cur_y, TRUE, TRUE);
			}
			else {
				/* Here we handle the case in which two lines are joined. Note that if the first line is empty,
					it is just deleted by delete_one_char(), so we must store its initial state and restore
					it after the deletion. */
				if (b->syn && b->cur_pos == 0) next_line_state = b->cur_line_desc->highlight_state;
				delete_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos);
				if (b->syn && b->cur_pos == 0) b->cur_line_desc->highlight_state = next_line_state;
				
				if (b->syn) {
					b->next_state = parse(b->syn, b->cur_line_desc, b->cur_line_desc->highlight_state, b->encoding == ENC_UTF8); 
					update_line(b, b->cur_y, FALSE, TRUE);
				}	
				else update_partial_line(b, b->cur_y, b->cur_x, TRUE, FALSE);
					
				if (b->cur_y < ne_lines - 2) scroll_window(b, b->cur_y + 1, -1);
			}			
		}
		need_attr_update = TRUE;
		end_undo_chain(b);
		return stop ? STOPPED : 0;

	case INSERTLINE_A:
		if (b->opt.read_only) return FILE_IS_READ_ONLY;
		NORMALIZE(c);

		for(i = 0; i < c && !stop; i++) {
			if (b->syn && b->attr_len < 0) freeze_attributes(b, b->cur_line_desc);

			if (insert_one_line(b, b->cur_line_desc, b->cur_line, b->cur_pos > b->cur_line_desc->line_len ? b->cur_line_desc->line_len : b->cur_pos) == OK) {		

				if (b->win_x) {
					int a = -1;
					/* If b->win_x is nonzero, the move_to_sol() call will
					refresh the entire video, so we shouldn't do anything. However, we 
					must poke into the next line initial state the correct state. */
					if (b->syn) {
						freeze_attributes(b, b->cur_line_desc);
						((line_desc *)b->cur_line_desc->ld_node.next)->highlight_state = b->next_state;
					}

					assert(b->cur_line_desc->ld_node.next->next != NULL);
					if (b->opt.auto_indent) a = auto_indent_line(b, b->cur_line + 1, (line_desc *)b->cur_line_desc->ld_node.next, INT_MAX);

					move_to_sol(b);
					line_down(b);
					if (a != -1) goto_pos(b, a);					
				}
				else {
					int a = -1;
					if (b->syn) update_line(b, b->cur_y, FALSE, TRUE);
					else update_partial_line(b, b->cur_y, b->cur_x, FALSE, FALSE);
					/* We need to avoid updates until we fix the next line. */
					need_attr_update = FALSE;
					/* We poke into the next line initial state the correct state. */
					if (b->syn) ((line_desc *)b->cur_line_desc->ld_node.next)->highlight_state = b->next_state;

					assert(b->cur_line_desc->ld_node.next->next != NULL);
					if (b->opt.auto_indent) a = auto_indent_line(b, b->cur_line + 1, (line_desc *)b->cur_line_desc->ld_node.next, INT_MAX);

					move_to_sol(b);
					line_down(b);
					if (a != -1) goto_pos(b, a);					

					if (b->cur_line == b->num_lines - 1) update_line(b, b->cur_y, FALSE, FALSE);
					else scroll_window(b, b->cur_y, 1);

					need_attr_update = TRUE;
				}
			}
		}
		
		return stop ? STOPPED : 0;

	case DELETELINE_A:
			
		if (b->opt.read_only) return FILE_IS_READ_ONLY;
		NORMALIZE(c);
		
		col = b->win_x + b->cur_x;
		start_undo_chain(b);
		for(i = 0; i < c && !stop; i++) {
			if (error = delete_one_line(b, b->cur_line_desc, b->cur_line)) break;
			scroll_window(b, b->cur_y, -1);
		}
		end_undo_chain(b);
		if (b->syn) {
			update_line(b, b->cur_y, FALSE, FALSE);
			need_attr_update = TRUE;
		}
		resync_pos(b);
		goto_column(b, col);
		
		return stop ? STOPPED : error;

	case UNDELLINE_A:
		
		if (b->opt.read_only) return FILE_IS_READ_ONLY;

		NORMALIZE(c);
		
		next_ld = (line_desc *)b->cur_line_desc->ld_node.next;

		start_undo_chain(b);
		for(i = 0; i < c && !stop; i++) {
			/* This is a bit tricky. First of all, if we are undeleting for
				the first time and the local attribute buffer is not valid
				we fill it. */
			if (i == 0 && b->syn && b->attr_len < 0) freeze_attributes(b, b->cur_line_desc);
			if (error = undelete_line(b)) break;
			if (i == 0) {
				if (b->syn) {
					/* Now the only valid part of the local attribute buffer is before b->cur_pos. 
						We perform a differential update so that if we undelete in the middle of
						a line we avoid to rewrite the part up to b->cur_pos. */
					b->attr_len = b->cur_pos;
					update_line(b, b->cur_y, FALSE, TRUE);
					next_line_state = b->next_state;
				}
				else update_partial_line(b, b->cur_y, b->cur_x, FALSE, FALSE);
			}
			if (b->syn) {
				assert(b->cur_line_desc->ld_node.next->next != NULL);
				/* For each undeletion, we must poke into the next line its correct initial state. */
				((line_desc *)b->cur_line_desc->ld_node.next)->highlight_state = next_line_state;
			}
			/* We actually scroll down the remaining lines, if necessary. */
			if (b->cur_y < ne_lines - 2) scroll_window(b, b->cur_y + 1, 1);
		}
		if (b->syn) {
			/* Finally, we force the update of the initial states of all following lines up to next_ld. */
			need_attr_update = TRUE;
			update_syntax_states(b, b->cur_y, b->cur_line_desc, next_ld);
		}
		end_undo_chain(b);
		return stop ? STOPPED : error;

	case DELETEEOL_A:

		if (b->opt.read_only) return FILE_IS_READ_ONLY;
		if (b->syn && b->attr_len < 0) freeze_attributes(b, b->cur_line_desc);
		delete_to_eol(b, b->cur_line_desc, b->cur_line, b->cur_pos);
		if (b->syn) update_line(b, b->cur_y, FALSE, TRUE);
		else update_partial_line(b, b->cur_y, b->cur_x, FALSE, FALSE);
		need_attr_update = TRUE;

		return OK;

	case SAVE_A:
		p = str_dup(b->filename);

	case SAVEAS_A:
		if (p || (q = p = request_file(b, "Filename", b->filename))) {
			print_info(SAVING);

			error = save_buffer_to_file(b, p);

			if (!print_error(error)) {
				const int load_syntax = b->filename == NULL || ! same_str(extension(p), extension(b->filename));
				change_filename(b, p);
				if (load_syntax && extension(p)) {
					load_syntax_by_name(b, extension(p));
					load_auto_prefs(b, extension(p));
					reset_window();
				}
				print_info(SAVED);
			}
			else {
				free(p);
				return ERROR;
			}
		}
		b->undo.last_save_step = b->undo.cur_step;
		return OK;

	case KEYCODE_A:
		print_message(info_msg[PRESS_A_KEY]);
		c = get_key_code();
		i = CHAR_CLASS(c);
		col = (c < 0) ? -c-1 : c;
		snprintf(msg, MAX_MESSAGE_SIZE, "Key Code: 0x%02x,  Input Class: %s,  Assigned Command: %s", col, input_class_names[i],
		         (key_binding[col] && key_binding[col][0]) ? key_binding[col] : "(none)" );
		print_message(msg);
		return OK;

	case CLEAR_A:
		if ((b->is_modified) && !request_response(b, info_msg[THIS_DOCUMENT_NOT_SAVED], FALSE)) return ERROR;
		clear_buffer(b);
		reset_window();
		return OK;

	case OPENNEW_A:
		b = new_buffer();
		reset_window();

	case OPEN_A:
		if ((b->is_modified) && !request_response(b, info_msg[THIS_DOCUMENT_NOT_SAVED], FALSE)) {
			if (a == OPENNEW_A) do_action(b, CLOSEDOC_A, 1, NULL);
			return ERROR;
		}

		if (p || (p = request_file(b, "Filename", b->filename))) {
			static int dprompt = 0; /* Set to true if we ever respond 'yes' to the prompt. */

			buffer *dup = get_buffer_named(p);

			/* 'c' -- flag meaning "Don't prompt if we've ever responded 'yes'." */
			if (!dup || dup == b || (dprompt && !c) || (dprompt = request_response(b, info_msg[SAME_NAME], FALSE))) {
				b->syn = NULL; /* So that autoprefs will load the right syntax. */
				if (b->opt.auto_prefs && extension(p)) load_auto_prefs(b, extension(p));
				error = load_file_in_buffer(b, p);
				if (error != FILE_IS_MIGRATED 
					&& error != FILE_IS_DIRECTORY 
					&& error != IO_ERROR 
					&& error != FILE_IS_TOO_LARGE 
					&& error != OUT_OF_MEMORY) change_filename(b, p);
				print_error(error);
				reset_window();
				return OK;
			}
			free(p);
		}
		if (a == OPENNEW_A) do_action(b, CLOSEDOC_A, 1, NULL);
		return ERROR;

	case ABOUT_A:
	   about(1);
	   c = get_key_code();
	   about(0);
		return OK;

	case REFRESH_A:
		clear_entire_screen();
		ttysize();
		keep_cursor_on_screen(cur_buffer);
		reset_window();
		return OK;

	case FIND_A:
	case FINDREGEXP_A:
		if (p || (p = request_string(a == FIND_A ? "Find" : "Find RegExp", b->find_string, FALSE, FALSE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {

			const encoding_type encoding = detect_encoding(p, strlen(p));

			if (encoding != ENC_ASCII && b->encoding != ENC_ASCII && encoding != b->encoding) {
				free(p);
				return INCOMPATIBLE_SEARCH_STRING_ENCODING;
			}

			free(b->find_string);
			b->find_string = p;
			b->find_string_changed = 1;
			print_error(error = (a == FIND_A ? find : find_regexp)(b, NULL, FALSE));
		}

		b->last_was_replace = 0;
		b->last_was_regexp = (a == FINDREGEXP_A);
		return error ? ERROR : 0;

	case REPLACE_A:
	case REPLACEONCE_A:
	case REPLACEALL_A:

		if (b->opt.read_only) {
			free(p);
			return FILE_IS_READ_ONLY;
		}

		if ((q = b->find_string) || (q = request_string(b->last_was_regexp ? "Find RegExp" : "Find", NULL, FALSE, FALSE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {

			const encoding_type search_encoding = detect_encoding(q, strlen(q));

			if (search_encoding != ENC_ASCII && b->encoding != ENC_ASCII && search_encoding != b->encoding) {
				free(p);
				free(q);
				return INCOMPATIBLE_SEARCH_STRING_ENCODING;
			}

			if (q != b->find_string) {
				free(b->find_string);
				b->find_string = q;
				b->find_string_changed = 1;
			}

			if (p || (p = request_string(b->last_was_regexp ? "Replace RegExp" : "Replace", b->replace_string, TRUE, FALSE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
				const encoding_type replace_encoding = detect_encoding(p, strlen(p));
				int first_search = TRUE, num_replace = 0;

				if (replace_encoding != ENC_ASCII && b->encoding != ENC_ASCII && replace_encoding != b->encoding ||
					search_encoding != ENC_ASCII && replace_encoding != ENC_ASCII && search_encoding != replace_encoding) {
					free(p);
					return INCOMPATIBLE_REPLACE_STRING_ENCODING;
				}

				c = 0;
				b->last_was_replace = 1;

				free(b->replace_string);
				b->replace_string = p;

				if (a == REPLACEALL_A) start_undo_chain(b);

				while(!stop && 
						!(error = (b->last_was_regexp ? find_regexp : find)(b, NULL, !first_search && a != REPLACEALL_A && c != 'A' && c != 'Y'))) {

					if (c != 'A' && a != REPLACEALL_A && a != REPLACEONCE_A) {
						refresh_window(b);
						c = request_char(b, b->opt.search_back ? "Replace (Yes/No/Last/All/Quit/Forward)" : "Replace (Yes/No/Last/All/Quit/Backward)", 'n');
						if (c == 'Q') break;
						if (c == 'A') start_undo_chain(b);
					}

					if (c == 'A' || c == 'Y' || c == 'L' || a == REPLACEONCE_A || a == REPLACEALL_A) {
						/* We delay buffer encoding promotion until it is really necessary. */
						if (b->encoding == ENC_ASCII) b->encoding = replace_encoding;

						if (b->last_was_regexp) error = replace_regexp(b, p);
						else error = replace(b, strlen(b->find_string), p);
						
						if (!error) {
							update_line(b, b->cur_y, FALSE, FALSE);
							if (b->syn) {
								need_attr_update = TRUE;
								update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
							}
						
							num_replace++;

							if (last_replace_empty_match)
								if (b->opt.search_back) error = char_left(cur_buffer);
								else error = char_right(cur_buffer);
						}					
	
						if (print_error(error)) {
							if (a == REPLACEALL_A || c == 'A') end_undo_chain(b);
							return ERROR;
						}
					}

					if (c == 'B' && !(b->opt.search_back) || c == 'F' && (b->opt.search_back)) {
						b->opt.search_back = !b->opt.search_back;
						b->find_string_changed = 1;
					}

					if (a == REPLACEONCE_A || c == 'L') break;

					first_search = FALSE;
				}

				if (a == REPLACEALL_A || c == 'A') end_undo_chain(b);

				if (num_replace) {
					snprintf(msg, MAX_MESSAGE_SIZE, "%d replacement%s made.", num_replace, num_replace > 1 ? "s" : "");
					print_message(msg);
				}
				if (stop) return STOPPED;

				if ((c != 'A' && a != REPLACEALL_A || first_search) && error || error != NOT_FOUND) {
					print_error(error);
					return ERROR;
				}
				return OK;
			}
		}
		return ERROR;

	case REPEATLAST_A:
		if (b->opt.read_only && b->last_was_replace) return FILE_IS_READ_ONLY;
		if (!b->find_string) return NO_SEARCH_STRING;
		else if ((b->last_was_replace) && !b->replace_string) return NO_REPLACE_STRING;
		else {
			int return_code = 0;

			const encoding_type search_encoding = detect_encoding(b->find_string, strlen(b->find_string));
			if (search_encoding != ENC_ASCII && b->encoding != ENC_ASCII && search_encoding != b->encoding) return INCOMPATIBLE_SEARCH_STRING_ENCODING;
			if (b->last_was_replace) {
				const encoding_type replace_encoding = detect_encoding(b->replace_string, strlen(b->replace_string));
				if (replace_encoding != ENC_ASCII && b->encoding != ENC_ASCII && replace_encoding != b->encoding ||
					search_encoding != ENC_ASCII && replace_encoding != ENC_ASCII && search_encoding != replace_encoding)
					return INCOMPATIBLE_REPLACE_STRING_ENCODING;
			}

			NORMALIZE(c);

			for(i = 0; i < c; i++) {
				if (!print_error((b->last_was_regexp ? find_regexp : find)(b, NULL, !b->last_was_replace))) {
					if (b->last_was_replace) {
						if (b->last_was_regexp) error = replace_regexp(b, b->replace_string);
						else error = replace(b, strlen(b->find_string), b->replace_string);

						if (! error) {
							update_line(b, b->cur_y, FALSE, FALSE);
							if (b->syn) {
								need_attr_update = TRUE;
								update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
							}
						
							if (last_replace_empty_match)
								if (b->opt.search_back) error = char_left(cur_buffer);
								else error = char_right(cur_buffer);
						}	

						if (print_error(error)) {
							return_code = ERROR;
							break;
						}
					}
				}
				else {
					return_code = ERROR;
					break;
				}
			}

			return return_code;
		}
		return ERROR;

	case MATCHBRACKET_A:
		return print_error(match_bracket(b)) ? ERROR : 0;

	case ALERT_A:
		alert();
		return OK;

	case BEEP_A:
		ring_bell();
		return OK;

	case FLASH_A:
		do_flash();
		return OK;

	case ESCAPETIME_A:
		if (c < 0 && (c = request_number("Timeout (1/10s)", -1))<0) return NUMERIC_ERROR(c);
		if (c < 256) {
			set_escape_time(c);
			return OK;
		}
		else return ESCAPE_TIME_OUT_OF_RANGE;

	case TABSIZE_A:
		if (c < 0 && (c = request_number("TAB Size", b->opt.tab_size))<=0) return NUMERIC_ERROR(c);
		if (c < ne_columns / 2) {
			move_to_sol(b);
			b->opt.tab_size = c;
			reset_window();
			return OK;
		}
		return TAB_SIZE_OUT_OF_RANGE;

	case TURBO_A:
		if (c < 0 && (c = request_number("Turbo Threshold", turbo))<0) return NUMERIC_ERROR(c);
		turbo = c;
		return OK;

	case CLIPNUMBER_A:
		if (c < 0 && (c = request_number("Clip Number", b->opt.cur_clip))<0) return NUMERIC_ERROR(c);
		b->opt.cur_clip = c;
		return OK;

	case RIGHTMARGIN_A:
		if (c < 0 && (c = request_number("Right Margin", b->opt.right_margin))<0) return NUMERIC_ERROR(c);
		b->opt.right_margin = c;
		return OK;

	case FREEFORM_A:
		SET_USER_FLAG(b, c, opt.free_form);
		return OK;

	case PRESERVECR_A:
		SET_USER_FLAG(b, c, opt.preserve_cr);
		return OK;

	case CRLF_A:
		SET_USER_FLAG(b, c, is_CRLF);
		return OK;

	case VISUALBELL_A:
		SET_USER_FLAG(b, c, opt.visual_bell);
		return OK;

	case STATUSBAR_A:
		SET_GLOBAL_FLAG(c, status_bar);
		reset_status_bar();
		return OK;

	case HEXCODE_A:
		SET_USER_FLAG(b, c, opt.hex_code);
		reset_status_bar();
		return OK;

	case FASTGUI_A:
		SET_GLOBAL_FLAG(c, fast_gui);
		reset_status_bar();
		return OK;

	case INSERT_A:
		SET_USER_FLAG(b, c, opt.insert);
		return OK;

	case WORDWRAP_A:
		SET_USER_FLAG(b, c, opt.word_wrap);
		return OK;

	case AUTOINDENT_A:
		SET_USER_FLAG(b, c, opt.auto_indent);
		return OK;

	case VERBOSEMACROS_A:
		SET_GLOBAL_FLAG(c, verbose_macros);
		return OK;

	case AUTOPREFS_A:
		SET_USER_FLAG(b, c, opt.auto_prefs);
		return OK;

	case BINARY_A:
		SET_USER_FLAG(b, c, opt.binary);
		return OK;

	case NOFILEREQ_A:
		SET_USER_FLAG(b, c, opt.no_file_req);
		return OK;

	case REQUESTORDER_A:
		SET_GLOBAL_FLAG(c, req_order);
		return OK;

	case UTF8AUTO_A:
		SET_USER_FLAG(b, c, opt.utf8auto);
		return OK;

	case UTF8_A: {
		const encoding_type old_encoding = b->encoding, encoding = detect_buffer_encoding(b);

		if (c < 0 && b->encoding != ENC_UTF8 || c > 0) {
			if (encoding == ENC_ASCII || encoding == ENC_UTF8) b->encoding = ENC_UTF8;
			else return BUFFER_IS_NOT_UTF8;
		}
		else b->encoding = encoding == ENC_ASCII ? ENC_ASCII : ENC_8_BIT;
		if (old_encoding != b->encoding) {
			reset_syntax_states(b);
			reset_undo_buffer(&b->undo);
		}
		b->attr_len = -1;
		need_attr_update = FALSE;
		move_to_sol(b);
		reset_window();
		return OK;
	}

	case MODIFIED_A:
		SET_USER_FLAG(b, c, is_modified);
		return OK;

	case UTF8IO_A:
		if (c < 0) io_utf8 = ! io_utf8;
		else io_utf8 = c != 0;
		reset_window();
		return OK;

	case DOUNDO_A:
		SET_USER_FLAG(b, c, opt.do_undo);
		if (!(b->opt.do_undo)) {
			reset_undo_buffer(&b->undo);
			b->atomic_undo = 0;
		}
		return OK;

	case READONLY_A:
		SET_USER_FLAG(b, c, opt.read_only);
		return OK;

	case CASESEARCH_A:
		SET_USER_FLAG(b, c, opt.case_search);
		b->find_string_changed = 1;
		return OK;

	case SEARCHBACK_A:
		SET_USER_FLAG(b, c, opt.search_back);
		b->find_string_changed = 1;
		return OK;

	case ATOMICUNDO_A:
		if (b->opt.do_undo) {
			/* set c to the desired b->link_undos */
			if (!p) {
				c = b->link_undos ? b->link_undos - 1 : 1;
			} else if (p[0]=='0') {
				c = 0;
			} else if (p[0]=='-') {
				c = b->link_undos ? b->link_undos - 1 : 0;
			} else if (p[0]=='+' || p[0]=='1') {  /* Kindly allow undocumented "AtomicUndo 1" also. */
				c = b->link_undos + 1;
			} else return INVALID_LEVEL;
			while(c > b->link_undos) start_undo_chain(b);
			while(c < b->link_undos) end_undo_chain(b);
			b->atomic_undo = (c > 0) ? 1 : 0;
			snprintf(msg, MAX_MESSAGE_SIZE, "AtomicUndo level: %d", c);
			print_message(msg);
			return OK;
		} else return UNDO_NOT_ENABLED;

	case RECORD_A:
		recording = b->recording;
		SET_USER_FLAG(b, c, recording);
		if (b->recording && !recording) {
			b->cur_macro = reset_stream(b->cur_macro);
			print_message(info_msg[STARTING_MACRO_RECORDING]);
		}
		else if (!b->recording && recording) print_message(info_msg[MACRO_RECORDING_COMPLETED]);
		return OK;

	case PLAY_A:
		if (!b->recording && !b->executing_internal_macro) {
			if (c < 0 && (c = request_number("Times", 1))<=0) return NUMERIC_ERROR(c);
			b->executing_internal_macro = 1;
			for(i = 0; i < c && !(error = play_macro(b, b->cur_macro)); i++);
			b->executing_internal_macro = 0;
			return print_error(error) ? ERROR : 0;
		}
		else return ERROR;

	case SAVEMACRO_A:
		if (p || (p = request_file(b, "Macro Name", NULL))) {
			print_info(SAVING);
			optimize_macro(b->cur_macro, verbose_macros);
			if ((error = print_error(save_stream(b->cur_macro, p, b->is_CRLF, FALSE))) == OK) print_info(SAVED);
			free(p);
			return error ? ERROR : 0;
		}
		return ERROR;

	case OPENMACRO_A:
		if (p || (p = request_file(b, "Macro Name", NULL))) {
			char_stream *cs;
			cs = load_stream(b->cur_macro, p, FALSE, FALSE);
			if (cs) b->cur_macro = cs;
			free(p);
			return cs ? 0 : ERROR;
		}
		return ERROR;

	case MACRO_A:
		if (p || (p = request_file(b, "Macro Name", NULL))) {
			error = print_error(execute_macro(b, p));
			free(p);
			return error ? ERROR : 0;
		}
		return ERROR;

	case UNLOADMACROS_A:
		unload_macros();
		return OK;

	case NEWDOC_A:
		new_buffer();
		reset_window();
		return OK;

	case CLOSEDOC_A: 
		if ((b->is_modified) && !request_response(b, info_msg[THIS_DOCUMENT_NOT_SAVED], FALSE)) return ERROR;
		if (!delete_buffer()) {
			close_history();
			unset_interactive_mode();
			exit(0);
		}
		keep_cursor_on_screen(cur_buffer);
		reset_window();

		/* We always return ERROR after a buffer has been deleted. Otherwise,
			the calling routines (and macros) could work on an unexisting buffer. */

		return ERROR;

	case NEXTDOC_A: /* Was NEXT_BUFFER: */
		if (b->b_node.next->next) cur_buffer = (buffer *)b->b_node.next;
		else cur_buffer = (buffer *)buffers.head;
		keep_cursor_on_screen(cur_buffer);
		reset_window();
		need_attr_update = FALSE;
		b->attr_len = -1;
		return OK;

	case PREVDOC_A:
		if (b->b_node.prev->prev) cur_buffer = (buffer *)b->b_node.prev;
		else cur_buffer = (buffer *)buffers.tail_pred;
		keep_cursor_on_screen(cur_buffer);
		reset_window();
		need_attr_update = FALSE;
		b->attr_len = -1;
		return OK;

	case SELECTDOC_A:
		if ((i = request_document()) < 0 || !(b = get_nth_buffer(i))) return ERROR;
		cur_buffer = b;
		keep_cursor_on_screen(cur_buffer);
		reset_window();
		need_attr_update = FALSE;
		b->attr_len = -1;
		return OK;

	case MARK_A:
	case MARKVERT_A:
		SET_USER_FLAG(b, c, marking);
		if (!b->marking) return(OK);
		print_message(info_msg[a==MARK_A ? BLOCK_START_MARKED : VERTICAL_BLOCK_START_MARKED]);
		b->mark_is_vertical = (a == MARKVERT_A);
		b->block_start_line = b->cur_line;
		b->block_start_col = b->win_x + b->cur_x;
		return OK;

	case CUT_A:
		if (b->opt.read_only) return FILE_IS_READ_ONLY;

	case COPY_A:
		if (!(error = print_error((b->mark_is_vertical ? copy_vert_to_clip : copy_to_clip)(b, c < 0 ? b->opt.cur_clip : c, a == CUT_A)))) {
			b->marking = 0;
			update_window_lines(b, b->cur_y, ne_lines - 2, FALSE);
		}
		return error ? ERROR : 0;

	case ERASE_A:
		if (b->opt.read_only) return FILE_IS_READ_ONLY;
		if (!(error = print_error((b->mark_is_vertical ? erase_vert_block : erase_block)(b)))) {
			b->marking = 0;
			update_window_lines(b, b->cur_y, ne_lines - 2, FALSE);
		}
		return OK;

	case PASTE_A:
	case PASTEVERT_A:
		if (b->opt.read_only) return FILE_IS_READ_ONLY;
		if (!(error = print_error((a == PASTE_A ? paste_to_buffer : paste_vert_to_buffer)(b, c < 0 ? b->opt.cur_clip : c)))) update_window_lines(b, b->cur_y, ne_lines - 2, FALSE);
		assert_buffer_content(b);
		return error ? ERROR : 0;

	case GOTOMARK_A:
		if (b->marking) {
			delay_update();
			goto_line(b, b->block_start_line);
			goto_column(b, b->block_start_col);
			return OK;
		}
		print_error(MARK_BLOCK_FIRST);
		return ERROR;

	case OPENCLIP_A:
		if (p || (p = request_file(b, "Clip Name", NULL))) {
			error = print_error(load_clip(b->opt.cur_clip, p, b->opt.preserve_cr, b->opt.binary));
			free(p);
			return error ? ERROR : 0;
		}
		return ERROR;

	case SAVECLIP_A:
		if (p || (p = request_file(b, "Clip Name", NULL))) {
			print_info(SAVING);
			if ((error = print_error(save_clip(b->opt.cur_clip, p, b->is_CRLF, b->opt.binary))) == OK) print_info(SAVED);
			free(p);
			return error ? ERROR : 0;
		}
		return ERROR;

	case EXEC_A:
		if (p || (p = request_string("Command", b->command_line, FALSE, TRUE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
			free(b->command_line);
			b->command_line = p;
			return print_error(execute_command_line(b, p)) ? ERROR : 0;
		}
		return ERROR;

	case SYSTEM_A:
		if (p || (p = request_string("Shell command", NULL, FALSE, TRUE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {

			unset_interactive_mode();
			if (system(p)) error = EXTERNAL_COMMAND_ERROR;
			set_interactive_mode();

			free(p);
			ttysize();
			keep_cursor_on_screen(cur_buffer);
			reset_window();
			return print_error(error) ? ERROR : OK;
		}
		return ERROR;

	case THROUGH_A:

		if (b->opt.read_only) return FILE_IS_READ_ONLY;

		if (!b->marking) b->mark_is_vertical = 0;

		if (p || (p = request_string("Filter", NULL, FALSE, TRUE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
			int fin = -1, fout = -1;

			char tmpnam1[strlen(P_tmpdir)+strlen(NE_TMP)+2], tmpnam2[strlen(P_tmpdir)+strlen(NE_TMP)+2], *command;

			strcat(strcat(strcpy(tmpnam1, P_tmpdir), "/"), NE_TMP);
			strcat(strcat(strcpy(tmpnam2, P_tmpdir), "/"), NE_TMP);
			if ((fin = mkstemp(tmpnam1)) != -1) close(fin);
			if ((fout = mkstemp(tmpnam2)) != -1) close(fout);
			if (fin != -1 && fout != -1) {

				realloc_clip_desc(get_nth_clip(INT_MAX), INT_MAX, 0);

				if (!b->marking || !(error = (b->mark_is_vertical ? copy_vert_to_clip : copy_to_clip)(b, INT_MAX, FALSE))) {

					if (!(error = save_clip(INT_MAX, tmpnam1, b->is_CRLF, b->opt.binary))) {
						if (command = malloc(strlen(p) + strlen(tmpnam1) + strlen(tmpnam2) + 16)) {

							strcat(strcat(strcat(strcat(strcat(strcpy(command, "( "), p), " ) <"), tmpnam1), " >"), tmpnam2);

							unset_interactive_mode();
							if (system(command)) error = EXTERNAL_COMMAND_ERROR;
							set_interactive_mode();
							
							if (!error) {

								if (!(error = load_clip(INT_MAX, tmpnam2, b->opt.preserve_cr, b->opt.binary))) {

									start_undo_chain(b);

									if (b->marking) (b->mark_is_vertical ? erase_vert_block : erase_block)(b);
									error = (b->mark_is_vertical ? paste_vert_to_buffer : paste_to_buffer)(b, INT_MAX);

									end_undo_chain(b);

									b->marking = 0;

									realloc_clip_desc(get_nth_clip(INT_MAX), INT_MAX, 0);
								}
							}
							free(command);
						}
						else error = OUT_OF_MEMORY;
					}
				}
			}
			else error = CANT_OPEN_TEMPORARY_FILE;
			
			remove(tmpnam1);
			remove(tmpnam2);

			ttysize();
			keep_cursor_on_screen(cur_buffer);
			reset_window();
			return print_error(error) ? ERROR : OK;
		}
		return ERROR;

	case TOUPPER_A:

		if (b->opt.read_only) return FILE_IS_READ_ONLY;

		NORMALIZE(c);

		start_undo_chain(b);
		for(i = 0; i < c && !(error = to_upper(b)) && !stop; i++);
		end_undo_chain(b);
		if (stop) error = STOPPED;
		return print_error(error) ? ERROR : 0;

	case TOLOWER_A:

		if (b->opt.read_only) return FILE_IS_READ_ONLY;

		NORMALIZE(c);

		start_undo_chain(b);
		for(i = 0; i < c && !(error = to_lower(b)) && !stop; i++);
		end_undo_chain(b);
		if (stop) error = STOPPED;
		return print_error(error) ? ERROR : 0;

	case CAPITALIZE_A:

		if (b->opt.read_only) return FILE_IS_READ_ONLY;

		NORMALIZE(c);

		start_undo_chain(b);
		for(i = 0; i < c && !(error = capitalize(b)) && !stop; i++);
		end_undo_chain(b);
		if (stop) error = STOPPED;
		return print_error(error) ? ERROR : 0;


	case CENTER_A:

		if (b->opt.read_only) return FILE_IS_READ_ONLY;

		NORMALIZE(c);

		start_undo_chain(b);
		for(i = 0; i < c && !(error = center(b)) && !stop; i++) {
			need_attr_update = TRUE;
			b->attr_len = -1;
			update_line(b, b->cur_y, FALSE, FALSE);
			move_to_sol(b);
			if (line_down(b) != OK) break;
		}
		end_undo_chain(b);

		if (stop) error = STOPPED;
		return print_error(error) ? ERROR : 0;

	case PARAGRAPH_A:
		if (b->opt.read_only) return FILE_IS_READ_ONLY;

		NORMALIZE(c);

		for(i = 0; i < c && !(error = paragraph(b)) && !stop; i++);

		if (stop) error = STOPPED;
		return print_error(error) ? ERROR : 0;

	case SHIFT_A:
		if (b->opt.read_only) return FILE_IS_READ_ONLY;

		error = shift(b, p, &msg[0], MAX_MESSAGE_SIZE);
		if (stop) error = STOPPED;
		if (p) free(p);
		return print_error(error) ? ERROR : 0;

	case LOADPREFS_A:
		if (p || (p = request_file(b, "Prefs Name", NULL))) {
			error = print_error(load_prefs(b, p));
			free(p);
			return error ? ERROR : OK;
		}
		return ERROR;

	case SAVEPREFS_A:
		if (p || (p = request_file(b, "Prefs Name", NULL))) {
			error = print_error(save_prefs(b, p));
			free(p);
			return error ? ERROR : OK;
		}
		return ERROR;

	case LOADAUTOPREFS_A:
		return print_error(load_auto_prefs(b, NULL)) ? ERROR : OK;

	case SAVEAUTOPREFS_A:
		return print_error(save_auto_prefs(b, NULL)) ? ERROR : OK;

	case SAVEDEFPREFS_A:
		return print_error(save_auto_prefs(b, DEF_PREFS_NAME)) ? ERROR : OK;

	case SYNTAX_A:
		if (!do_syntax) return SYNTAX_NOT_ENABLED;
		if (p || (p = request_string("Syntax",  b->syn ? b->syn->name : NULL, TRUE, FALSE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
			if (!strcmp(p, "*")) b->syn = NULL;
			else error = print_error(load_syntax_by_name(b, p));
			if (error == OK) reset_window();
			free(p);
			return error ? ERROR : OK;
		}
		return ERROR;

	case ESCAPE_A:
		handle_menus();
		return OK;

	case UNDO_A:
		if (b->opt.read_only) return FILE_IS_READ_ONLY;
		if (!b->opt.do_undo) return UNDO_NOT_ENABLED;

		NORMALIZE(c);
		delay_update();

		if (b->atomic_undo) {
			b->atomic_undo = 0;
			while (b->link_undos) end_undo_chain(b);
			print_message("AtomicUndo level: 0");
		}

		for(i = 0; i < c && !(error = undo(b)) && !stop; i++);
		if (stop) error = STOPPED;
		b->is_modified = b->undo.cur_step != b->undo.last_save_step;
		update_window(b);
		return print_error(error) ? ERROR : 0;

	case REDO_A:
		if (b->opt.read_only) return FILE_IS_READ_ONLY;
		if (!b->opt.do_undo) return UNDO_NOT_ENABLED;

		NORMALIZE(c);
		delay_update();

		for(i = 0; i < c && !(error = redo(b)) && !stop; i++);

		if (stop) error = STOPPED;
		b->is_modified = b->undo.cur_step != b->undo.last_save_step;
		update_window(b);
		return print_error(error) ? ERROR : 0;

	case FLAGS_A:
		help("FLAGS");
		reset_window();
		return OK;

	case HELP_A:
		help(p);
		reset_window();
		return OK;

	case SUSPEND_A:
		stop_ne();
		keep_cursor_on_screen(cur_buffer);
		return OK;

	case AUTOCOMPLETE_A:
		/* Since we are going to call other actions (INSERTSTRING_A and DELETEPREVWORD_A),
			we do not want to record this insertion twice. Also, we are counting on 
			INSERTSTRING_A to handle character encoding issues. */
		recording = b->recording;

		i = b->cur_pos;
		
		if ( !p ) { /* no prefix given; find one left of the cursor. */
			if ( context_prefix(b, &p, &i, b->encoding) ) return OUT_OF_MEMORY;
		}	
      
		snprintf(msg, MAX_MESSAGE_SIZE, "AutoComplete: prefix \"%s\"", p);
			
		if (p = autocomplete(p, msg, TRUE, &col)) {
			b->recording = 0;
			start_undo_chain(b);
			if (i >= b->cur_pos || (error = do_action(b, DELETEPREVWORD_A, 1, NULL)) == OK) error = do_action(b, INSERTSTRING_A, 0, p);
			end_undo_chain(b);
			b->recording = recording;
			print_message(info_msg[col]);
		}
		else if (stop) error = STOPPED;
		else if (col == AUTOCOMPLETE_NO_MATCH) print_message(info_msg[AUTOCOMPLETE_NO_MATCH]);
		
		return print_error(error) ? ERROR : 0;

	default:
		if (p) free(p);
		return OK;
	}
}
