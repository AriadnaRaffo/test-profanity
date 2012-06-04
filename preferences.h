/* 
 * preferences.h
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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <glib.h>

void prefs_load(void);

char * find_login(char *prefix);
void reset_login_search(void);

gboolean prefs_get_beep(void);
void prefs_set_beep(gboolean value);
gboolean prefs_get_flash(void);
void prefs_set_flash(gboolean value);
void prefs_add_login(const char *jid);
gboolean prefs_get_showsplash(void);
void prefs_set_showsplash(gboolean value);

#endif
