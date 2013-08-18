/*
 * plugins.h
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

#ifndef PLUGINS_H
#define PLUGINS_H

typedef enum {
    PYTHON,
    C
} lang_t;

typedef struct prof_plugin_t {
    char *name;
    lang_t lang;
    void *module;
    void (*init_func)(struct prof_plugin_t* plugin, const char * const version,
        const char * const status);
    void (*on_start_func)(struct prof_plugin_t* plugin);
    void (*on_connect_func)(struct prof_plugin_t* plugin);
    void (*on_message_received_func)(struct prof_plugin_t* plugin,
        const char * const jid, const char * const message);
} ProfPlugin;

void plugins_init(void);
void plugins_on_start(void);
void plugins_on_connect(void);
void plugins_on_message_received(const char * const jid, const char * const message);
void plugins_shutdown(void);
gboolean plugins_run_command(const char * const cmd);
void plugins_run_timed(void);

#endif
