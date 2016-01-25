/*
 * rosterwin.c
 *
 * Copyright (C) 2012 - 2015 James Booth <boothj5@gmail.com>
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
 * In addition, as a special exception, the copyright holders give permission to
 * link the code of portions of this program with the OpenSSL library under
 * certain conditions as described in each individual source file, and
 * distribute linked combinations including the two.
 *
 * You must obey the GNU General Public License in all respects for all of the
 * code used other than OpenSSL. If you modify file(s) with this exception, you
 * may extend this exception to your version of the file(s), but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version. If you delete this exception statement from all
 * source files in the program, then also delete it here.
 *
 */

#include <assert.h>
#include <stdlib.h>

#include "contact.h"
#include "ui/ui.h"
#include "ui/window.h"
#include "window_list.h"
#include "config/preferences.h"
#include "roster_list.h"

typedef enum {
    ROSTER_CONTACT,
    ROSTER_CONTACT_ACTIVE,
    ROSTER_CONTACT_UNREAD
} roster_contact_theme_t;

static void
_rosterwin_presence(ProfLayoutSplit *layout, theme_item_t colour, const char *presence, const char *status,
    int current_indent)
{
    // don't show presence for offline contacts
    gboolean is_offline = g_strcmp0(presence, "offline") == 0;
    if (is_offline) {
        return;
    }

    char *by = prefs_get_string(PREF_ROSTER_BY);
    gboolean by_presence = g_strcmp0(by, "presence") == 0;
    prefs_free_string(by);

    int presence_indent = prefs_get_roster_presence_indent();
    if (presence_indent > 0) {
        current_indent += presence_indent;
    }

    gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);

    // show only status when grouped by presence
    if (by_presence) {
        if (status && prefs_get_boolean(PREF_ROSTER_STATUS)) {
            wattron(layout->subwin, theme_attrs(colour));
            if (presence_indent == -1) {
                GString *msg = g_string_new("");
                g_string_append_printf(msg, ": \"%s\"", status);
                win_sub_print(layout->subwin, msg->str, FALSE, wrap, current_indent);
                g_string_free(msg, TRUE);
                wattroff(layout->subwin, theme_attrs(colour));
            } else {
                GString *msg = g_string_new(" ");
                while (current_indent > 0) {
                    g_string_append(msg, " ");
                    current_indent--;
                }
                g_string_append_printf(msg, "\"%s\"", status);
                win_sub_newline_lazy(layout->subwin);
                win_sub_print(layout->subwin, msg->str, FALSE, wrap, current_indent);
                g_string_free(msg, TRUE);
                wattroff(layout->subwin, theme_attrs(colour));
            }
        }

    // show both presence and status when not grouped by presence
    } else if (prefs_get_boolean(PREF_ROSTER_PRESENCE) || (status && prefs_get_boolean(PREF_ROSTER_STATUS))) {
        wattron(layout->subwin, theme_attrs(colour));
        if (presence_indent == -1) {
            GString *msg = g_string_new("");
            if (prefs_get_boolean(PREF_ROSTER_PRESENCE)) {
                g_string_append_printf(msg, ": %s", presence);
                if (status && prefs_get_boolean(PREF_ROSTER_STATUS)) {
                    g_string_append_printf(msg, " \"%s\"", status);
                }
            } else if (status && prefs_get_boolean(PREF_ROSTER_STATUS)) {
                g_string_append_printf(msg, ": \"%s\"", status);
            }
            win_sub_print(layout->subwin, msg->str, FALSE, wrap, current_indent);
            g_string_free(msg, TRUE);
            wattroff(layout->subwin, theme_attrs(colour));
        } else {
            GString *msg = g_string_new(" ");
            while (current_indent > 0) {
                g_string_append(msg, " ");
                current_indent--;
            }
            if (prefs_get_boolean(PREF_ROSTER_PRESENCE)) {
                g_string_append(msg, presence);
                if (status && prefs_get_boolean(PREF_ROSTER_STATUS)) {
                    g_string_append_printf(msg, " \"%s\"", status);
                }
            } else if (status && prefs_get_boolean(PREF_ROSTER_STATUS)) {
                g_string_append_printf(msg, "\"%s\"", status);
            }
            win_sub_newline_lazy(layout->subwin);
            win_sub_print(layout->subwin, msg->str, FALSE, wrap, current_indent);
            g_string_free(msg, TRUE);
            wattroff(layout->subwin, theme_attrs(colour));
        }
    }
}

theme_item_t
_get_roster_theme(roster_contact_theme_t theme_type, const char *presence)
{
    switch (theme_type) {
    case ROSTER_CONTACT:        return theme_roster_presence_attrs(presence);
    case ROSTER_CONTACT_ACTIVE: return theme_roster_active_presence_attrs(presence);
    case ROSTER_CONTACT_UNREAD: return theme_roster_unread_presence_attrs(presence);
    default:                    return theme_roster_presence_attrs(presence);
    }
}

static void
_rosterwin_resources(ProfLayoutSplit *layout, PContact contact, int current_indent, roster_contact_theme_t theme_type, int unread)
{
    gboolean join = prefs_get_boolean(PREF_ROSTER_RESOURCE_JOIN);

    GList *resources = p_contact_get_available_resources(contact);
    if (resources) {
        if (join && (g_list_length(resources) == 1)) {
            Resource *resource = resources->data;
            const char *resource_presence = string_from_resource_presence(resource->presence);
            theme_item_t resource_presence_colour = _get_roster_theme(theme_type, resource_presence);

            wattron(layout->subwin, theme_attrs(resource_presence_colour));
            GString *msg = g_string_new("");
            char ch = prefs_get_roster_resource_char();
            if (ch) {
                g_string_append_printf(msg, "%c", ch);
            } else {
                g_string_append(msg, " ");
            }
            g_string_append(msg, resource->name);
            if (prefs_get_boolean(PREF_ROSTER_PRIORITY)) {
                g_string_append_printf(msg, " %d", resource->priority);
            }

            char *unreadpos = prefs_get_string(PREF_ROSTER_UNREAD);
            if ((g_strcmp0(unreadpos, "after") == 0) && unread > 0) {
                g_string_append_printf(msg, " (%d)", unread);
            }
            prefs_free_string(unreadpos);

            gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);
            win_sub_print(layout->subwin, msg->str, FALSE, wrap, 0);
            g_string_free(msg, TRUE);
            wattroff(layout->subwin, theme_attrs(resource_presence_colour));

            if (prefs_get_boolean(PREF_ROSTER_PRESENCE) || prefs_get_boolean(PREF_ROSTER_STATUS)) {
                _rosterwin_presence(layout, resource_presence_colour, resource_presence, resource->status, current_indent);
            }
        } else {
            gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);

            char *unreadpos = prefs_get_string(PREF_ROSTER_UNREAD);
            if ((g_strcmp0(unreadpos, "after") == 0) && unread > 0) {
                GString *unreadmsg = g_string_new("");
                g_string_append_printf(unreadmsg, " (%d)", unread);

                const char *presence = p_contact_presence(contact);
                theme_item_t presence_colour = _get_roster_theme(theme_type, presence);

                wattron(layout->subwin, theme_attrs(presence_colour));
                win_sub_print(layout->subwin, unreadmsg->str, FALSE, wrap, current_indent);
                wattroff(layout->subwin, theme_attrs(presence_colour));
            }
            prefs_free_string(unreadpos);

            int resource_indent = prefs_get_roster_resource_indent();
            if (resource_indent > 0) {
                current_indent += resource_indent;
            }

            GList *curr_resource = resources;
            while (curr_resource) {
                Resource *resource = curr_resource->data;
                const char *resource_presence = string_from_resource_presence(resource->presence);
                theme_item_t resource_presence_colour = _get_roster_theme(theme_type, resource_presence);

                wattron(layout->subwin, theme_attrs(resource_presence_colour));
                GString *msg = g_string_new(" ");
                int this_indent = current_indent;
                while (this_indent > 0) {
                    g_string_append(msg, " ");
                    this_indent--;
                }
                char ch = prefs_get_roster_resource_char();
                if (ch) {
                    g_string_append_printf(msg, "%c", ch);
                }
                g_string_append(msg, resource->name);
                if (prefs_get_boolean(PREF_ROSTER_PRIORITY)) {
                    g_string_append_printf(msg, " %d", resource->priority);
                }
                win_sub_newline_lazy(layout->subwin);
                win_sub_print(layout->subwin, msg->str, FALSE, wrap, current_indent);
                g_string_free(msg, TRUE);
                wattroff(layout->subwin, theme_attrs(resource_presence_colour));

                if (prefs_get_boolean(PREF_ROSTER_PRESENCE) || prefs_get_boolean(PREF_ROSTER_STATUS)) {
                    _rosterwin_presence(layout, resource_presence_colour, resource_presence, resource->status, current_indent);
                }

                curr_resource = g_list_next(curr_resource);
            }
        }
    } else if (prefs_get_boolean(PREF_ROSTER_PRESENCE) || prefs_get_boolean(PREF_ROSTER_STATUS)) {
        const char *presence = p_contact_presence(contact);
        const char *status = p_contact_status(contact);
        theme_item_t presence_colour = _get_roster_theme(theme_type, presence);
        gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);

        char *unreadpos = prefs_get_string(PREF_ROSTER_UNREAD);
        if ((g_strcmp0(unreadpos, "after") == 0) && unread > 0) {
            GString *unreadmsg = g_string_new("");
            g_string_append_printf(unreadmsg, " (%d)", unread);

            wattron(layout->subwin, theme_attrs(presence_colour));
            win_sub_print(layout->subwin, unreadmsg->str, FALSE, wrap, current_indent);
            wattroff(layout->subwin, theme_attrs(presence_colour));
        }
        prefs_free_string(unreadpos);
        _rosterwin_presence(layout, presence_colour, presence, status, current_indent);
    } else {
        gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);

        char *unreadpos = prefs_get_string(PREF_ROSTER_UNREAD);
        if ((g_strcmp0(unreadpos, "after") == 0) && unread > 0) {
            GString *unreadmsg = g_string_new("");
            g_string_append_printf(unreadmsg, " (%d)", unread);
            const char *presence = p_contact_presence(contact);
            theme_item_t presence_colour = _get_roster_theme(theme_type, presence);

            wattron(layout->subwin, theme_attrs(presence_colour));
            win_sub_print(layout->subwin, unreadmsg->str, FALSE, wrap, current_indent);
            wattroff(layout->subwin, theme_attrs(presence_colour));
        }
        prefs_free_string(unreadpos);
    }

    g_list_free(resources);

}

static void
_rosterwin_contact(ProfLayoutSplit *layout, PContact contact)
{
    const char *name = p_contact_name_or_jid(contact);
    const char *presence = p_contact_presence(contact);
    const char *status = p_contact_status(contact);
    const char *barejid = p_contact_barejid(contact);
    int unread = 0;

    roster_contact_theme_t theme_type = ROSTER_CONTACT;
    ProfChatWin *chatwin = wins_get_chat(barejid);
    if (chatwin) {
        if (chatwin->unread > 0) {
            theme_type = ROSTER_CONTACT_UNREAD;
            unread = chatwin->unread;
        } else {
            theme_type = ROSTER_CONTACT_ACTIVE;
        }
    }

    theme_item_t presence_colour = _get_roster_theme(theme_type, presence);

    wattron(layout->subwin, theme_attrs(presence_colour));
    GString *msg = g_string_new(" ");
    int indent = prefs_get_roster_contact_indent();
    int current_indent = 0;
    if (indent > 0) {
        current_indent += indent;
        while (indent > 0) {
            g_string_append(msg, " ");
            indent--;
        }
    }
    char ch = prefs_get_roster_contact_char();
    if (ch) {
        g_string_append_printf(msg, "%c", ch);
    }

    char *unreadpos = prefs_get_string(PREF_ROSTER_UNREAD);
    if ((g_strcmp0(unreadpos, "before") == 0) && unread > 0) {
        g_string_append_printf(msg, "(%d) ", unread);
        unread = 0;
    }
    g_string_append(msg, name);
    if (g_strcmp0(unreadpos, "after") == 0) {
        if (!prefs_get_boolean(PREF_ROSTER_RESOURCE)) {
            if (unread > 0) {
                g_string_append_printf(msg, " (%d)", unread);
            }
            unread = 0;
        }
    }
    prefs_free_string(unreadpos);

    win_sub_newline_lazy(layout->subwin);
    gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);
    win_sub_print(layout->subwin, msg->str, FALSE, wrap, current_indent);
    g_string_free(msg, TRUE);
    wattroff(layout->subwin, theme_attrs(presence_colour));

    if (prefs_get_boolean(PREF_ROSTER_RESOURCE)) {
        _rosterwin_resources(layout, contact, current_indent, theme_type, unread);
    } else if (prefs_get_boolean(PREF_ROSTER_PRESENCE) || prefs_get_boolean(PREF_ROSTER_STATUS)) {
        if (unread > 0) {
            GString *unreadmsg = g_string_new("");
            g_string_append_printf(unreadmsg, " (%d)", unread);
            wattron(layout->subwin, theme_attrs(presence_colour));
            win_sub_print(layout->subwin, unreadmsg->str, FALSE, wrap, current_indent);
            g_string_free(unreadmsg, TRUE);
            wattroff(layout->subwin, theme_attrs(presence_colour));
        }

        _rosterwin_presence(layout, presence_colour, presence, status, current_indent);
    }
}

static void
_rosterwin_contacts_by_presence(ProfLayoutSplit *layout, const char *const presence, char *title, gboolean newline)
{
    GSList *contacts = roster_get_contacts_by_presence(presence);

    // if this group has contacts, or if we want to show empty groups
    if (contacts || prefs_get_boolean(PREF_ROSTER_EMPTY)) {
        if (newline) {
            win_sub_newline_lazy(layout->subwin);
        }
        wattron(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));
        GString *title_str = g_string_new(" ");
        char ch = prefs_get_roster_header_char();
        if (ch) {
            g_string_append_printf(title_str, "%c", ch);
        }
        g_string_append(title_str, title);
        if (prefs_get_boolean(PREF_ROSTER_COUNT)) {
            g_string_append_printf(title_str, " (%d)", g_slist_length(contacts));
        }
        gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);
        win_sub_print(layout->subwin, title_str->str, FALSE, wrap, 1);
        g_string_free(title_str, TRUE);
        wattroff(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));
    }

    if (contacts) {
        GSList *curr_contact = contacts;
        while (curr_contact) {
            PContact contact = curr_contact->data;
            _rosterwin_contact(layout, contact);
            curr_contact = g_slist_next(curr_contact);
        }
    }
    g_slist_free(contacts);
}

static void
_rosterwin_contacts_by_group(ProfLayoutSplit *layout, char *group, gboolean newline)
{
    GSList *contacts = NULL;

    char *order = prefs_get_string(PREF_ROSTER_ORDER);
    gboolean offline = prefs_get_boolean(PREF_ROSTER_OFFLINE);
    if (g_strcmp0(order, "presence") == 0) {
        contacts = roster_get_group(group, ROSTER_ORD_PRESENCE, offline);
    } else {
        contacts = roster_get_group(group, ROSTER_ORD_NAME, offline);
    }
    prefs_free_string(order);

    if (contacts || prefs_get_boolean(PREF_ROSTER_EMPTY)) {
        if (newline) {
            win_sub_newline_lazy(layout->subwin);
        }
        wattron(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));
        GString *title = g_string_new(" ");
        char ch = prefs_get_roster_header_char();
        if (ch) {
            g_string_append_printf(title, "%c", ch);
        }
        g_string_append(title, group);
        if (prefs_get_boolean(PREF_ROSTER_COUNT)) {
            g_string_append_printf(title, " (%d)", g_slist_length(contacts));
        }
        gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);
        win_sub_print(layout->subwin, title->str, FALSE, wrap, 1);
        g_string_free(title, TRUE);
        wattroff(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));

        GSList *curr_contact = contacts;
        while (curr_contact) {
            PContact contact = curr_contact->data;
            _rosterwin_contact(layout, contact);
            curr_contact = g_slist_next(curr_contact);
        }
    }
    g_slist_free(contacts);
}

static void
_rosterwin_contacts_by_no_group(ProfLayoutSplit *layout, gboolean newline)
{
    GSList *contacts = NULL;

    char *order = prefs_get_string(PREF_ROSTER_ORDER);
    gboolean offline = prefs_get_boolean(PREF_ROSTER_OFFLINE);
    if (g_strcmp0(order, "presence") == 0) {
        contacts = roster_get_nogroup(ROSTER_ORD_PRESENCE, offline);
    } else {
        contacts = roster_get_nogroup(ROSTER_ORD_NAME, offline);
    }
    prefs_free_string(order);

    if (contacts || prefs_get_boolean(PREF_ROSTER_EMPTY)) {
        if (newline) {
            win_sub_newline_lazy(layout->subwin);
        }
        wattron(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));
        GString *title = g_string_new(" ");
        char ch = prefs_get_roster_header_char();
        if (ch) {
            g_string_append_printf(title, "%c", ch);
        }
        g_string_append(title, "no group");

        if (prefs_get_boolean(PREF_ROSTER_COUNT)) {
            g_string_append_printf(title, " (%d)", g_slist_length(contacts));
        }
        gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);
        win_sub_print(layout->subwin, title->str, FALSE, wrap, 1);
        g_string_free(title, TRUE);
        wattroff(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));

        GSList *curr_contact = contacts;
        while (curr_contact) {
            PContact contact = curr_contact->data;
            _rosterwin_contact(layout, contact);
            curr_contact = g_slist_next(curr_contact);
        }
    }
    g_slist_free(contacts);
}

void
_rosterwin_room(ProfLayoutSplit *layout, ProfMucWin *mucwin)
{
    GString *msg = g_string_new(" ");

    if (mucwin->unread_mentions) {
        wattron(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_MENTION));
    } else if (mucwin->unread_triggers) {
        wattron(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_TRIGGER));
    } else if (mucwin->unread > 0) {
        wattron(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_UNREAD));
    } else {
        wattron(layout->subwin, theme_attrs(THEME_ROSTER_ROOM));
    }

    int indent = prefs_get_roster_contact_indent();
    int current_indent = 0;
    if (indent > 0) {
        current_indent += indent;
        while (indent > 0) {
            g_string_append(msg, " ");
            indent--;
        }
    }
    char ch = prefs_get_roster_room_char();
    if (ch) {
        g_string_append_printf(msg, "%c", ch);
    }

    char *unreadpos = prefs_get_string(PREF_ROSTER_ROOMS_UNREAD);
    if ((g_strcmp0(unreadpos, "before") == 0) && mucwin->unread > 0) {
        g_string_append_printf(msg, "(%d) ", mucwin->unread);
    }
    g_string_append(msg, mucwin->roomjid);
    if ((g_strcmp0(unreadpos, "after") == 0) && mucwin->unread > 0) {
        g_string_append_printf(msg, " (%d)", mucwin->unread);
    }
    prefs_free_string(unreadpos);

    win_sub_newline_lazy(layout->subwin);
    gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);
    win_sub_print(layout->subwin, msg->str, FALSE, wrap, current_indent);
    g_string_free(msg, TRUE);

    if (mucwin->unread_mentions) {
        wattroff(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_MENTION));
    } else if (mucwin->unread_triggers) {
        wattroff(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_TRIGGER));
    } else if (mucwin->unread > 0) {
        wattroff(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_UNREAD));
    } else {
        wattroff(layout->subwin, theme_attrs(THEME_ROSTER_ROOM));
    }

    char *privpref = prefs_get_string(PREF_ROSTER_PRIVATE);
    if (g_strcmp0(privpref, "room") == 0) {
        GList *privs = wins_get_private_chats(mucwin->roomjid);
        GList *curr = privs;
        while (curr) {
            ProfPrivateWin *privwin = curr->data;
            win_sub_newline_lazy(layout->subwin);

            GString *privmsg = g_string_new(" ");
            indent = prefs_get_roster_contact_indent();
            current_indent = 0;
            if (indent > 0) {
                current_indent += indent;
                while (indent > 0) {
                    g_string_append(privmsg, " ");
                    indent--;
                }
            }

            // TODO add preference
            indent = prefs_get_roster_resource_indent();
            if (indent > 0) {
                current_indent += indent;
                while (indent > 0) {
                    g_string_append(privmsg, " ");
                    indent--;
                }
            }

            unreadpos = prefs_get_string(PREF_ROSTER_ROOMS_UNREAD);
            if ((g_strcmp0(unreadpos, "before") == 0) && privwin->unread > 0) {
                g_string_append_printf(privmsg, "(%d) ", privwin->unread);
            }

            ch = prefs_get_roster_private_char();
            if (ch) {
                g_string_append_printf(privmsg, "%c", ch);
            }

            g_string_append(privmsg, privwin->fulljid + strlen(mucwin->roomjid) + 1);

            if ((g_strcmp0(unreadpos, "after") == 0) && privwin->unread > 0) {
                g_string_append_printf(privmsg, " (%d)", privwin->unread);
            }
            prefs_free_string(unreadpos);

            if (privwin->unread > 0) {
                wattron(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_UNREAD));
            } else {
                wattron(layout->subwin, theme_attrs(THEME_ROSTER_ROOM));
            }

            win_sub_print(layout->subwin, privmsg->str, FALSE, wrap, current_indent);

            if (privwin->unread > 0) {
                wattroff(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_UNREAD));
            } else {
                wattroff(layout->subwin, theme_attrs(THEME_ROSTER_ROOM));
            }

            g_string_free(privmsg, TRUE);
            curr = g_list_next(curr);
        }

        g_list_free(privs);
    }

    prefs_free_string(privpref);
}

static int
_compare_rooms_name(ProfMucWin *a, ProfMucWin *b)
{
    return g_strcmp0(a->roomjid, b->roomjid);
}

static int
_compare_rooms_unread(ProfMucWin *a, ProfMucWin *b)
{
    if (a->unread > b->unread) {
        return -1;
    } else if (a->unread == b->unread) {
        return g_strcmp0(a->roomjid, b->roomjid);
    } else {
        return 1;
    }
}

void
_rosterwin_private_chats(ProfLayoutSplit *layout)
{
    GList *privs = wins_get_private_chats(NULL);
    if (privs || prefs_get_boolean(PREF_ROSTER_EMPTY)) {
        win_sub_newline_lazy(layout->subwin);
        wattron(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));
        GString *title_str = g_string_new(" ");
        char ch = prefs_get_roster_header_char();
        if (ch) {
            g_string_append_printf(title_str, "%c", ch);
        }
        g_string_append(title_str, "Private chats");
        if (prefs_get_boolean(PREF_ROSTER_COUNT)) {
            g_string_append_printf(title_str, " (%d)", g_list_length(privs));
        }
        gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);
        win_sub_print(layout->subwin, title_str->str, FALSE, wrap, 1);
        g_string_free(title_str, TRUE);
        wattroff(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));

        GList *curr = privs;
        while (curr) {
            ProfPrivateWin *privwin = curr->data;
            win_sub_newline_lazy(layout->subwin);

            GString *privmsg = g_string_new(" ");
            int indent = prefs_get_roster_contact_indent();
            int current_indent = 0;
            if (indent > 0) {
                current_indent += indent;
                while (indent > 0) {
                    g_string_append(privmsg, " ");
                    indent--;
                }
            }

            char *unreadpos = prefs_get_string(PREF_ROSTER_ROOMS_UNREAD);
            if ((g_strcmp0(unreadpos, "before") == 0) && privwin->unread > 0) {
                g_string_append_printf(privmsg, "(%d) ", privwin->unread);
            }

            ch = prefs_get_roster_private_char();
            if (ch) {
                g_string_append_printf(privmsg, "%c", ch);
            }

            g_string_append(privmsg, privwin->fulljid);

            if ((g_strcmp0(unreadpos, "after") == 0) && privwin->unread > 0) {
                g_string_append_printf(privmsg, " (%d)", privwin->unread);
            }
            prefs_free_string(unreadpos);

            if (privwin->unread > 0) {
                wattron(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_UNREAD));
            } else {
                wattron(layout->subwin, theme_attrs(THEME_ROSTER_ROOM));
            }

            win_sub_print(layout->subwin, privmsg->str, FALSE, wrap, current_indent);

            if (privwin->unread > 0) {
                wattroff(layout->subwin, theme_attrs(THEME_ROSTER_ROOM_UNREAD));
            } else {
                wattroff(layout->subwin, theme_attrs(THEME_ROSTER_ROOM));
            }

            g_string_free(privmsg, TRUE);
            curr = g_list_next(curr);
        }

        g_list_free(privs);
    }
}

void
_rosterwin_rooms(ProfLayoutSplit *layout, gboolean newline)
{
    GList *rooms = muc_rooms();
    GList *rooms_sorted = NULL;
    GList *curr_room = rooms;
    while (curr_room) {
        ProfMucWin *mucwin = wins_get_muc(curr_room->data);
        if (mucwin) {
            char *order = prefs_get_string(PREF_ROSTER_ROOMS_ORDER);
            if (g_strcmp0(order, "unread") == 0) {
                rooms_sorted = g_list_insert_sorted(rooms_sorted, mucwin, (GCompareFunc)_compare_rooms_unread);
            } else {
                rooms_sorted = g_list_insert_sorted(rooms_sorted, mucwin, (GCompareFunc)_compare_rooms_name);
            }
            prefs_free_string(order);
        }
        curr_room = g_list_next(curr_room);
    }
    g_list_free(rooms);

    // if there are active rooms, or if we want to show empty groups
    if (rooms_sorted || prefs_get_boolean(PREF_ROSTER_EMPTY)) {
        if (newline) {
            win_sub_newline_lazy(layout->subwin);
        }
        wattron(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));
        GString *title_str = g_string_new(" ");
        char ch = prefs_get_roster_header_char();
        if (ch) {
            g_string_append_printf(title_str, "%c", ch);
        }
        g_string_append(title_str, "Rooms");
        if (prefs_get_boolean(PREF_ROSTER_COUNT)) {
            g_string_append_printf(title_str, " (%d)", g_list_length(rooms_sorted));
        }
        gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);
        win_sub_print(layout->subwin, title_str->str, FALSE, wrap, 1);
        g_string_free(title_str, TRUE);
        wattroff(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));

        GList *curr_room = rooms_sorted;
        while (curr_room) {
            _rosterwin_room(layout, curr_room->data);
            curr_room = g_list_next(curr_room);
        }
    }

    g_list_free(rooms_sorted);

    char *privpref = prefs_get_string(PREF_ROSTER_PRIVATE);
    if (g_strcmp0(privpref, "group") == 0) {
        _rosterwin_private_chats(layout);
    }
    prefs_free_string(privpref);
}

void
rosterwin_roster(void)
{
    ProfWin *console = wins_get_console();
    if (!console) {
        return;
    }

    jabber_conn_status_t conn_status = jabber_get_connection_status();
    if (conn_status != JABBER_CONNECTED) {
        return;
    }

    ProfLayoutSplit *layout = (ProfLayoutSplit*)console->layout;
    assert(layout->memcheck == LAYOUT_SPLIT_MEMCHECK);
    werase(layout->subwin);

    gboolean newline = FALSE;
    char *roomspos = prefs_get_string(PREF_ROSTER_ROOMS_POS);
    if (prefs_get_boolean(PREF_ROSTER_ROOMS) && (g_strcmp0(roomspos, "first") == 0)) {
        _rosterwin_rooms(layout, newline);
        newline = TRUE;
    }

    if (prefs_get_boolean(PREF_ROSTER_CONTACTS)) {
        char *by = prefs_get_string(PREF_ROSTER_BY);
        if (g_strcmp0(by, "presence") == 0) {
            _rosterwin_contacts_by_presence(layout, "chat", "Available for chat", newline);
            _rosterwin_contacts_by_presence(layout, "online", "Online", TRUE);
            _rosterwin_contacts_by_presence(layout, "away", "Away", TRUE);
            _rosterwin_contacts_by_presence(layout, "xa", "Extended Away", TRUE);
            _rosterwin_contacts_by_presence(layout, "dnd", "Do not disturb", TRUE);
            if (prefs_get_boolean(PREF_ROSTER_OFFLINE)) {
                _rosterwin_contacts_by_presence(layout, "offline", "Offline", TRUE);
            }
        } else if (g_strcmp0(by, "group") == 0) {
            GSList *groups = roster_get_groups();
            GSList *curr_group = groups;
            while (curr_group) {
                _rosterwin_contacts_by_group(layout, curr_group->data, newline);
                newline = TRUE;
                curr_group = g_slist_next(curr_group);
            }
            g_slist_free_full(groups, free);
            _rosterwin_contacts_by_no_group(layout, newline);
        } else {
            GSList *contacts = NULL;

            char *order = prefs_get_string(PREF_ROSTER_ORDER);
            gboolean offline = prefs_get_boolean(PREF_ROSTER_OFFLINE);
            if (g_strcmp0(order, "presence") == 0) {
                contacts = roster_get_contacts(ROSTER_ORD_PRESENCE, offline);
            } else {
                contacts = roster_get_contacts(ROSTER_ORD_NAME, offline);
            }
            prefs_free_string(order);

            wattron(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));
            GString *title = g_string_new(" ");
            char ch = prefs_get_roster_header_char();
            if (ch) {
                g_string_append_printf(title, "%c", ch);
            }
            if (newline) {
                win_sub_newline_lazy(layout->subwin);
            }

            g_string_append(title, "Roster");
            if (prefs_get_boolean(PREF_ROSTER_COUNT)) {
                g_string_append_printf(title, " (%d)", g_slist_length(contacts));
            }
            gboolean wrap = prefs_get_boolean(PREF_ROSTER_WRAP);
            win_sub_print(layout->subwin, title->str, FALSE, wrap, 1);
            g_string_free(title, TRUE);
            wattroff(layout->subwin, theme_attrs(THEME_ROSTER_HEADER));

            if (contacts) {
                GSList *curr_contact = contacts;
                while (curr_contact) {
                    PContact contact = curr_contact->data;
                    _rosterwin_contact(layout, contact);
                    curr_contact = g_slist_next(curr_contact);
                }
            }
            g_slist_free(contacts);
        }
        prefs_free_string(by);
    }

    if (prefs_get_boolean(PREF_ROSTER_ROOMS) && (g_strcmp0(roomspos, "last") == 0)) {
        _rosterwin_rooms(layout, TRUE);
    }

    prefs_free_string(roomspos);
}
