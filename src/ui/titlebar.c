/*
 * titlebar.c
 *
 * Copyright (C) 2012, 2013 James Booth <boothj5@gmail.com>
 *
 * This file is part of Profanity.
 *
 * Profanity is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Profanity is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Profanity.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "config/theme.h"
#include "ui/ui.h"

static WINDOW *title_bar;
static char *current_title = NULL;
static const char *recipient = NULL;
static GTimer *typing_elapsed;
static int dirty;
static contact_presence_t current_status;

static void _title_bar_draw_title(void);
static void _title_bar_draw_status(void);

void
create_title_bar(void)
{
    int cols = getmaxx(stdscr);

    title_bar = newwin(1, cols, 0, 0);
    wbkgd(title_bar, COLOUR_TITLE_TEXT);
    title_bar_title();
    title_bar_set_status(CONTACT_OFFLINE);
    dirty = TRUE;
}

void
title_bar_title(void)
{
    werase(title_bar);
    recipient = NULL;
    typing_elapsed = NULL;
    title_bar_show("Profanity. Type /help for help information.");
    _title_bar_draw_status();
    dirty = TRUE;
}

void
title_bar_resize(void)
{
    int cols = getmaxx(stdscr);

    wresize(title_bar, 1, cols);
    wbkgd(title_bar, COLOUR_TITLE_TEXT);
    werase(title_bar);
    _title_bar_draw_title();
    _title_bar_draw_status();
    dirty = TRUE;
}

void
title_bar_refresh(void)
{
    if (recipient != NULL) {

        if (typing_elapsed != NULL) {
            gdouble seconds = g_timer_elapsed(typing_elapsed, NULL);

            if (seconds >= 10) {

                if (current_title != NULL) {
                    free(current_title);
                }

                current_title = (char *) malloc(strlen(recipient) + 1);
                strcpy(current_title, recipient);

                title_bar_draw();

                g_timer_destroy(typing_elapsed);
                typing_elapsed = NULL;

                dirty = TRUE;
            }
        }
    }

    if (dirty) {
        wrefresh(title_bar);
        inp_put_back();
        dirty = FALSE;
    }
}

void
title_bar_show(const char * const title)
{
    if (current_title != NULL)
        free(current_title);

    current_title = (char *) malloc(strlen(title) + 1);
    strcpy(current_title, title);
    _title_bar_draw_title();
}

void
title_bar_set_status(contact_presence_t status)
{
    current_status = status;
    _title_bar_draw_status();
}

void
title_bar_set_recipient(const char * const from)
{
    if (typing_elapsed != NULL) {
        g_timer_destroy(typing_elapsed);
        typing_elapsed = NULL;
    }
    recipient = from;

    if (current_title != NULL) {
        free(current_title);
    }

    current_title = (char *) malloc(strlen(from) + 1);
    strcpy(current_title, from);

    dirty = TRUE;
}

void
title_bar_set_typing(gboolean is_typing)
{
    if (is_typing) {
        if (typing_elapsed != NULL) {
            g_timer_start(typing_elapsed);
        } else {
            typing_elapsed = g_timer_new();
        }
    }

    if (current_title != NULL) {
        free(current_title);
    }

    if (is_typing) {
        current_title = (char *) malloc(strlen(recipient) + 13);
        sprintf(current_title, "%s (typing...)", recipient);
    } else {
        current_title = (char *) malloc(strlen(recipient) + 1);
        strcpy(current_title, recipient);
    }

    dirty = TRUE;
}

void
title_bar_draw(void)
{
    werase(title_bar);
    _title_bar_draw_status();
    _title_bar_draw_title();
}

static void
_title_bar_draw_status(void)
{
    int cols = getmaxx(stdscr);

    wattron(title_bar, COLOUR_TITLE_BRACKET);
    mvwaddch(title_bar, 0, cols - 14, '[');
    wattroff(title_bar, COLOUR_TITLE_BRACKET);

    switch (current_status)
    {
        case CONTACT_ONLINE:
            mvwprintw(title_bar, 0, cols - 13, " ...online ");
            break;
        case CONTACT_AWAY:
            mvwprintw(title_bar, 0, cols - 13, " .....away ");
            break;
        case CONTACT_DND:
            mvwprintw(title_bar, 0, cols - 13, " ......dnd ");
            break;
        case CONTACT_CHAT:
            mvwprintw(title_bar, 0, cols - 13, " .....chat ");
            break;
        case CONTACT_XA:
            mvwprintw(title_bar, 0, cols - 13, " .......xa ");
            break;
        case CONTACT_OFFLINE:
            mvwprintw(title_bar, 0, cols - 13, " ..offline ");
            break;
    }

    wattron(title_bar, COLOUR_TITLE_BRACKET);
    mvwaddch(title_bar, 0, cols - 2, ']');
    wattroff(title_bar, COLOUR_TITLE_BRACKET);

    dirty = TRUE;
}

static void
_title_bar_draw_title(void)
{
    wmove(title_bar, 0, 0);
    int i;
    for (i = 0; i < 45; i++)
        waddch(title_bar, ' ');
    mvwprintw(title_bar, 0, 0, " %s", current_title);

    dirty = TRUE;
}
