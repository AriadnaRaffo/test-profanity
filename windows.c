/* 
 * windows.c
 *
 * Copyright (C) 2012 James Booth <boothj5@gmail.com>
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

#include <string.h>
#include <stdlib.h>

#include <ncurses.h>
#include <glib.h>

#include "ui.h"
#include "util.h"
#include "contact.h"
#include "preferences.h"

#define CONS_WIN_TITLE "_cons"
#define PAD_SIZE 200
#define NUM_WINS 10

// holds console at index 0 and chat wins 1 through to 9
static struct prof_win _wins[NUM_WINS];

// the window currently being displayed
static int _curr_prof_win = 0;

// shortcut pointer to console window
static WINDOW * _cons_win = NULL;

// current window state
static int dirty;

// max columns for main windows, never resize below
static int max_cols = 0;

static void _create_windows(void);
static void _print_splash_logo(WINDOW *win);
static int _find_prof_win_index(const char * const contact);
static int _new_prof_win(const char * const contact);
static void _current_window_refresh(void);
static void _win_switch_if_active(const int i);
static void _win_show_time(WINDOW *win);
static void _win_show_user(WINDOW *win, const char * const user, const int colour);
static void _win_show_message(WINDOW *win, const char * const message);
static void _show_status_string(WINDOW *win, const char * const from, 
    const char * const show, const char * const status, const char * const pre, 
    const char * const default_show);
static void _cons_show_incoming_message(const char * const short_from, 
    const int win_index);
static void _win_handle_switch(const int * const ch);
static void _win_handle_page(const int * const ch);
static void _win_resize_all(void);

void gui_init(void)
{
    initscr();
    cbreak();
    keypad(stdscr, TRUE);

    if (has_colors()) {    
        use_default_colors();
        start_color();
        
        init_pair(1, COLOR_WHITE, -1);
        init_pair(2, COLOR_GREEN, -1);
        init_pair(3, COLOR_WHITE, COLOR_BLUE);
        init_pair(4, COLOR_CYAN, COLOR_BLUE);
        init_pair(5, COLOR_CYAN, -1);
        init_pair(6, COLOR_RED, -1);
        init_pair(7, COLOR_MAGENTA, -1);
        init_pair(8, COLOR_YELLOW, -1);
    }

    refresh();

    create_title_bar();
    create_status_bar();
    create_input_window();
    _create_windows();

    dirty = TRUE;
}

void gui_refresh(void)
{
    title_bar_refresh();
    status_bar_refresh();

    if (dirty) {
        _current_window_refresh();
        dirty = FALSE;
    }

    inp_put_back();
}

void gui_close(void)
{
    endwin();
}

void gui_resize(const int ch, const char * const input, const int size)
{
    title_bar_resize();
    status_bar_resize();
    _win_resize_all();
    inp_win_resize(input, size);
    dirty = TRUE;
}

int win_close_win(void)
{
    if (win_in_chat()) {
        // reset the chat win to unused
        strcpy(_wins[_curr_prof_win].from, "");
        wclear(_wins[_curr_prof_win].win);

        // set it as inactive in the status bar
        status_bar_inactive(_curr_prof_win);
        
        // go back to console window
        _curr_prof_win = 0;
        title_bar_title();

        dirty = TRUE;
    
        // success
        return 1;
    } else {
        // didn't close anything
        return 0;
    }
}

int win_in_chat(void)
{
    return ((_curr_prof_win != 0) && 
        (strcmp(_wins[_curr_prof_win].from, "") != 0));
}

char *win_get_recipient(void)
{
    struct prof_win current = _wins[_curr_prof_win];
    char *recipient = (char *) malloc(sizeof(char) * (strlen(current.from) + 1));
    strcpy(recipient, current.from);
    return recipient;
}

void win_show_incomming_msg(const char * const from, const char * const message) 
{
    char from_cpy[strlen(from) + 1];
    strcpy(from_cpy, from);
    char *short_from = strtok(from_cpy, "/");

    int win_index = _find_prof_win_index(short_from);
    if (win_index == NUM_WINS)
        win_index = _new_prof_win(short_from);

    WINDOW *win = _wins[win_index].win;
    _win_show_time(win);
    _win_show_user(win, short_from, 1);
    _win_show_message(win, message);
    
    if (win_index == _curr_prof_win) {
        status_bar_active(win_index);
        dirty = TRUE;
    } else {
        status_bar_new(win_index);
        _cons_show_incoming_message(short_from, win_index);
    }

    if (prefs_get_beep())
        beep();
}

void win_show_outgoing_msg(const char * const from, const char * const to, 
    const char * const message)
{
    int win_index = _find_prof_win_index(to);
    if (win_index == NUM_WINS) 
        win_index = _new_prof_win(to);

    WINDOW *win = _wins[win_index].win;
    _win_show_time(win);
    _win_show_user(win, from, 0);
    _win_show_message(win, message);
    
    status_bar_active(win_index);
    
    if (win_index == _curr_prof_win) {
        dirty = TRUE;
    } else {
        status_bar_new(win_index);
    }
}

void win_contact_online(const char * const from, const char * const show, 
    const char * const status)
{
    _show_status_string(_cons_win, from, show, status, "++", "online");

    int win_index = _find_prof_win_index(from);
    if (win_index != NUM_WINS) {
        WINDOW *win = _wins[win_index].win;
        _show_status_string(win, from, show, status, "++", "online");
    }
    
    if (win_index == _curr_prof_win)
        dirty = TRUE;
}

void win_contact_offline(const char * const from, const char * const show, 
    const char * const status)
{
    _show_status_string(_cons_win, from, show, status, "--", "offline");

    int win_index = _find_prof_win_index(from);
    if (win_index != NUM_WINS) {
        WINDOW *win = _wins[win_index].win;
        _show_status_string(win, from, show, status, "--", "offline");
    }
    
    if (win_index == _curr_prof_win)
        dirty = TRUE;
}

void win_disconnected(void)
{
    int i;
    // show message in all active chats
    for (i = 1; i < NUM_WINS; i++) {
        if (strcmp(_wins[i].from, "") != 0) {
            WINDOW *win = _wins[_curr_prof_win].win;
            _win_show_time(win);
            wattron(win, COLOR_PAIR(6));
            wprintw(win, "%s\n", "Lost connection.");
            wattroff(win, COLOR_PAIR(6));
    
            // if current win, set dirty
            if (i == _curr_prof_win) {
                dirty = TRUE;
            }
        }
    }
}

void cons_help(void)
{
    cons_show("");
    cons_show("Basic Commands:");
    cons_show("");
    cons_show("/help                : This help.");
    cons_show("/connect user@host   : Login to jabber.");
    cons_show("/msg user@host mesg  : Send mesg to user.");
    cons_show("/close               : Close a chat window.");
    cons_show("/who                 : Find out who is online.");
    cons_show("/ros                 : List all contacts.");
    cons_show("/quit                : Quit Profanity.");
    cons_show("");
    cons_show("Settings:");
    cons_show("");
    cons_show("/beep <on/off>       : Enable/disable sound notification");
    cons_show("/flash <on/off>      : Enable/disable screen flash notification");
    cons_show("/showsplash <on/off> : Enable/disable splash logo on startup");
    cons_show("");
    cons_show("Status changes (msg is optional):");
    cons_show("");
    cons_show("/away <msg>          : Set status to away.");
    cons_show("/online <msg>        : Set status to online.");
    cons_show("/dnd <msg>           : Set status to dnd (do not disturb).");
    cons_show("/chat <msg>          : Set status to chat (available for chat).");
    cons_show("/xa <msg>            : Set status to xa (extended away).");
    cons_show("");
    cons_show("Keys:");
    cons_show("");
    cons_show("F1                   : This console window.");
    cons_show("F2-F10               : Chat windows.");
    cons_show("UP, DOWN             : Navigate input history.");
    cons_show("LEFT, RIGHT          : Edit current input.");
    cons_show("TAB                  : Autocomplete recipient.");
    cons_show("PAGE UP, PAGE DOWN   : Page the chat window.");
    cons_show("");

    if (_curr_prof_win == 0)
        dirty = TRUE;
}

void cons_show_online_contacts(GSList *list)
{
    _win_show_time(_cons_win);
    wprintw(_cons_win, "Online contacts:\n");

    GSList *curr = list;

    while(curr) {
        PContact contact = curr->data;
        _win_show_time(_cons_win);
        wattron(_cons_win, COLOR_PAIR(2));
        wprintw(_cons_win, "%s", p_contact_name(contact));
        if (p_contact_show(contact))
            wprintw(_cons_win, " is %s", p_contact_show(contact));
        if (p_contact_status(contact))
            wprintw(_cons_win, ", \"%s\"", p_contact_status(contact));
        wprintw(_cons_win, "\n");
        wattroff(_cons_win, COLOR_PAIR(2));

        curr = g_slist_next(curr);
    }
}

void cons_bad_show(const char * const msg)
{
    _win_show_time(_cons_win);
    wattron(_cons_win, COLOR_PAIR(6));
    wprintw(_cons_win, "%s\n", msg);
    wattroff(_cons_win, COLOR_PAIR(6));
    
    if (_curr_prof_win == 0)
        dirty = TRUE;
}

void cons_show(const char * const msg)
{
    _win_show_time(_cons_win);
    wprintw(_cons_win, "%s\n", msg); 
    
    if (_curr_prof_win == 0)
        dirty = TRUE;
}

void cons_bad_command(const char * const cmd)
{
    _win_show_time(_cons_win);
    wprintw(_cons_win, "Unknown command: %s\n", cmd);
    
    if (_curr_prof_win == 0)
        dirty = TRUE;
}

void win_handle_special_keys(const int * const ch)
{
    _win_handle_switch(ch);
    _win_handle_page(ch);
}

void win_page_off(void)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    _wins[_curr_prof_win].paged = 0;
    
    int y, x;
    getyx(_wins[_curr_prof_win].win, y, x);

    int size = rows - 3;

    _wins[_curr_prof_win].y_pos = y - (size - 1);
    if (_wins[_curr_prof_win].y_pos < 0)
        _wins[_curr_prof_win].y_pos = 0;

    dirty = TRUE;
}

static void _create_windows(void)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    max_cols = cols;

    // create the console window in 0
    struct prof_win cons;
    strcpy(cons.from, CONS_WIN_TITLE);
    cons.win = newpad(PAD_SIZE, cols);
    cons.y_pos = 0;
    cons.paged = 0;
    scrollok(cons.win, TRUE);

    _wins[0] = cons;
    _cons_win = _wins[0].win;
    
    wattrset(_cons_win, A_BOLD);
    _win_show_time(_cons_win);
    if (prefs_get_showsplash()) {
        _print_splash_logo(_cons_win);
    } else {
        wprintw(_cons_win, "Welcome to Profanity.\n");
    }
    prefresh(_cons_win, 0, 0, 1, 0, rows-3, cols-1);

    dirty = TRUE;
    
    // create the chat windows
    int i;
    for (i = 1; i < NUM_WINS; i++) {
        struct prof_win chat;
        strcpy(chat.from, "");
        chat.win = newpad(PAD_SIZE, cols);
        chat.y_pos = 0;
        chat.paged = 0;
        wattrset(chat.win, A_BOLD);
        scrollok(chat.win, TRUE);
        _wins[i] = chat;
    }    
}

static void _print_splash_logo(WINDOW *win)
{
    wprintw(win, "Welcome to\n");
    wattron(win, COLOR_PAIR(5));
    wprintw(win, "                   ___            _           \n");
    wprintw(win, "                  / __)          (_)_         \n");
    wprintw(win, " ____   ____ ___ | |__ ____ ____  _| |_ _   _ \n");
    wprintw(win, "|  _ \\ / ___) _ \\|  __) _  |  _ \\| |  _) | | |\n");
    wprintw(win, "| | | | |  | |_| | | ( ( | | | | | | |_| |_| |\n");
    wprintw(win, "| ||_/|_|   \\___/|_|  \\_||_|_| |_|_|\\___)__  |\n");
    wprintw(win, "|_|                                    (____/ \n");
    wattroff(win, COLOR_PAIR(5));
}

static int _find_prof_win_index(const char * const contact)
{
    // find the chat window for recipient
    int i;
    for (i = 1; i < NUM_WINS; i++)
        if (strcmp(_wins[i].from, contact) == 0)
            break;

    return i;
}

static int _new_prof_win(const char * const contact)
{
    int i;
    // find the first unused one
    for (i = 1; i < NUM_WINS; i++)
        if (strcmp(_wins[i].from, "") == 0)
            break;

    // set it up
    strcpy(_wins[i].from, contact);
    wclear(_wins[i].win);

    return i;
}
static void _win_switch_if_active(const int i)
{
    win_page_off();
    if (strcmp(_wins[i].from, "") != 0) {
        _curr_prof_win = i;
        win_page_off();

        if (i == 0) {
            title_bar_title();
        } else {
            title_bar_show(_wins[i].from);
            status_bar_active(i);
        }
    }

    dirty = TRUE;
}

static void _win_show_time(WINDOW *win)
{
    char tstmp[80];
    get_time(tstmp);
    wprintw(win, "%s - ", tstmp);
}

static void _win_show_user(WINDOW *win, const char * const user, const int colour)
{
    if (colour)
        wattron(win, COLOR_PAIR(2));
    wprintw(win, "%s: ", user);
    if (colour)
        wattroff(win, COLOR_PAIR(2));
}

static void _win_show_message(WINDOW *win, const char * const message)
{
    wattroff(win, A_BOLD);
    wprintw(win, "%s\n", message);
    wattron(win, A_BOLD);
}

static void _current_window_refresh(void)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    WINDOW *current = _wins[_curr_prof_win].win;
    prefresh(current, _wins[_curr_prof_win].y_pos, 0, 1, 0, rows-3, cols-1);
}

void _win_resize_all(void)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // only make the pads bigger, to avoid data loss on cropping
    if (cols > max_cols) {
        max_cols = cols;

        int i;
        for (i = 0; i < NUM_WINS; i++) {
            wresize(_wins[i].win, PAD_SIZE, cols);
        }
    }
    
    WINDOW *current = _wins[_curr_prof_win].win;
    prefresh(current, _wins[_curr_prof_win].y_pos, 0, 1, 0, rows-3, cols-1);
}

static void _show_status_string(WINDOW *win, const char * const from, 
    const char * const show, const char * const status, const char * const pre, 
    const char * const default_show)
{
    _win_show_time(win);    
    if (strcmp(default_show, "online") == 0) {
        wattron(win, COLOR_PAIR(2));
    } else {
        wattron(win, COLOR_PAIR(5));
        wattroff(win, A_BOLD);
    }

    wprintw(win, "%s %s", pre, from);

    if (show != NULL) 
        wprintw(win, " is %s", show);
    else
        wprintw(win, " is %s", default_show);
        
    if (status != NULL)
        wprintw(win, ", \"%s\"", status);
    
    wprintw(win, "\n");
    
    if (strcmp(default_show, "online") == 0) {
        wattroff(win, COLOR_PAIR(2));
    } else {
        wattroff(win, COLOR_PAIR(5));
        wattron(win, A_BOLD);
    }
}


static void _cons_show_incoming_message(const char * const short_from, const int win_index)
{
    _win_show_time(_cons_win);
    wattron(_cons_win, COLOR_PAIR(8));
    wprintw(_cons_win, "<< incoming from %s (%d)\n", short_from, win_index + 1);
    wattroff(_cons_win, COLOR_PAIR(8));
}

static void _win_handle_switch(const int * const ch)
{
    if (*ch == KEY_F(1)) {
        _win_switch_if_active(0);
    } else if (*ch == KEY_F(2)) {
        _win_switch_if_active(1);
    } else if (*ch == KEY_F(3)) {
        _win_switch_if_active(2);
    } else if (*ch == KEY_F(4)) {
        _win_switch_if_active(3);
    } else if (*ch == KEY_F(5)) {
        _win_switch_if_active(4);
    } else if (*ch == KEY_F(6)) {
        _win_switch_if_active(5);
    } else if (*ch == KEY_F(7)) {
        _win_switch_if_active(6);
    } else if (*ch == KEY_F(8)) {
        _win_switch_if_active(7);
    } else if (*ch == KEY_F(9)) {
        _win_switch_if_active(8);
    } else if (*ch == KEY_F(10)) {
        _win_switch_if_active(9);
    }
}

static void _win_handle_page(const int * const ch)
{
    int rows, cols, y, x;
    getmaxyx(stdscr, rows, cols);
    getyx(_wins[_curr_prof_win].win, y, x);

    int page_space = rows - 4;
    int *page_start = &_wins[_curr_prof_win].y_pos;
    
    // page up
    if (*ch == KEY_PPAGE) {
        *page_start -= page_space;
    
        // went past beginning, show first page
        if (*page_start < 0)
            *page_start = 0;
       
        _wins[_curr_prof_win].paged = 1;
        dirty = TRUE;

    // page down
    } else if (*ch == KEY_NPAGE) {
        *page_start += page_space;

        // only got half a screen, show full screen
        if ((y - (*page_start)) < page_space)
            *page_start = y - page_space;

        // went past end, show full screen
        else if (*page_start >= y)
            *page_start = y - page_space;
           
        _wins[_curr_prof_win].paged = 1;
        dirty = TRUE;
    }
}

