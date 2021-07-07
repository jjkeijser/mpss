/*
 * Copyright 2010-2017 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * Disclaimer: The codes contained in these modules may be specific to
 * the Intel Software Development Platform codenamed Knights Ferry,
 * and the Intel product codenamed Knights Corner, and are not backward
 * compatible with other Intel products. Additionally, Intel will NOT
 * support the codes or instruction set in future products.
 *
 * Intel offers no warranty of any kind regarding the code. This code is
 * licensed on an "AS IS" basis and Intel is not obligated to provide
 * any support, assistance, installation, training, or other services
 * of any kind. Intel is also not obligated to provide any updates,
 * enhancements or extensions. Intel specifically disclaims any warranty
 * of merchantability, non-infringement, fitness for any particular
 * purpose, and any other warranty.
 *
 * Further, Intel disclaims all liability of any kind, including but
 * not limited to liability for infringement of any proprietary rights,
 * relating to the use of the code, even if Intel is notified of the
 * possibility of such liability. Except as expressly stated in an Intel
 * license agreement provided with this code and agreed upon with Intel,
 * no license, express or implied, by estoppel or otherwise, to any
 * intellectual property rights is granted herein.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#ifdef WAIT_FOR_THE_3_4_RELEASE
#include <ncurses.h>
#endif

void dump_help(char *buffer);
int index_hlines(char *buffer);

char *hlines[4096];

#define COMMAND_PROMPT "[q]uit, PageDown:Space, PageUp, ArrowUp:p:k, ArrowDown:n:j >>"

void
micctrl_help(char *buffer)
{
#ifndef WAIT_FOR_THE_3_4_RELEASE
	dump_help(buffer);
#else
	WINDOW *display;
	WINDOW *input;
	char *errprompt;
	int ch;
	int rows;
	int cols;
	int offset = 0;
	int total;
	int cur;
	int row;

	if (!isatty(fileno(stdout)))
		dump_help(buffer);

	total = index_hlines(buffer);

	initscr();
	getmaxyx(stdscr, rows, cols);

	if (total <= rows) {
		endwin();
		dump_help(buffer);
	}

	start_color();
	use_default_colors();
	assume_default_colors(-1, -1);
	init_pair(1, COLOR_RED, -1);
	raw();
	noecho();

	display = newwin(rows - 1, cols, 0, 0);

	input = newwin(1, COLS, LINES - 1, 0);
	keypad(input, TRUE);

	for (cur = offset, row = 0; (cur < (offset + rows - 1)) && (cur < total); cur++, row++)
		mvwprintw(display, row, 0, "%s", hlines[offset + row]);

	wrefresh(display);
	mvwprintw(input, 0, 0, "%s", COMMAND_PROMPT);
	wrefresh(input);

	while ((ch = wgetch(input)) != 'q') {
		errprompt = "";

		switch (ch) {
		case KEY_NPAGE:
		case 32:
			offset += rows - 1;

			if ((offset + rows - 1) > total)
				offset = total - rows + 2;

			if (offset < 0)
				offset = 0;

			break;

		case KEY_PPAGE:
			offset -= rows - 1;

			if (offset < 0)
				offset = 0;

			break;

		case KEY_UP:
		case 'p':
		case 'P':
		case 'k':
			if (offset)
				offset--;

			break;

		case KEY_DOWN:
		case 'n':
		case 'N':
		case 'j':
			offset++;

			if ((offset + rows) > total)
				offset = total - rows + 2;

			if (offset < 0)
				offset = 0;

			break;

		case KEY_END:
			offset = total - rows + 2;

			if (offset)
				offset--;

			break;

		default:
			werase(input);

			if (isprint(ch))
				errprompt = "Invalid character '%c':: ";
			else
				errprompt = "Invalid ASCII '%d':: ";

			wrefresh(input);
			break;
		}

		werase(display);

		for (cur = offset, row = 0; (cur < (offset + rows - 1)) && (cur < total); cur++, row++)
			mvwprintw(display, row, 0, "%s", hlines[offset + row]);

		wrefresh(display);
		wclear(input);
		wmove(input, 0, 0);

		if (strlen(errprompt)) {
			wattrset(input, COLOR_PAIR(1));
			wprintw(input, errprompt, ch);
			wattrset(input, COLOR_PAIR(0));
		}

		wprintw(input, "%s", COMMAND_PROMPT);
		wrefresh(input);
	}

	endwin();
#endif
	exit(0);
}

int
index_hlines(char *buffer)
{
	char *end;
	int l = 0;
	int total = 0;

	memset(hlines, 0, sizeof(hlines));

	if ((end = (char *) malloc(strlen(buffer))) == NULL)
		return 0;

	memcpy(end, buffer, strlen(buffer));

	while (end != NULL) {
		hlines[l++] = end;

		if ((end = strchr(end, '\n')) == NULL)
			continue;

		*end = '\0';
		end++;
		total++;
	}

	return total;
}

void
dump_help(char *buffer)
{
	printf("%s\n", buffer);
	exit(0);
}

