/* 
 * jabber.c
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

#include <strophe.h>

#include "common.h"
#include "jabber.h"
#include "log.h"
#include "preferences.h"
#include "profanity.h"

#define PING_INTERVAL 120000 // 2 minutes

static struct _jabber_conn_t {
    xmpp_log_t *log;
    xmpp_ctx_t *ctx;
    xmpp_conn_t *conn;
    jabber_conn_status_t conn_status;
    jabber_presence_t presence;
    int tls_disabled;
} jabber_conn;

static log_level_t _get_log_level(xmpp_log_level_t xmpp_level);
static xmpp_log_level_t _get_xmpp_log_level();
static void _xmpp_file_logger(void * const userdata, 
    const xmpp_log_level_t level, const char * const area, 
    const char * const msg);
static xmpp_log_t * _xmpp_get_file_logger();

// XMPP event handlers
static void _connection_handler(xmpp_conn_t * const conn, 
    const xmpp_conn_event_t status, const int error, 
    xmpp_stream_error_t * const stream_error, void * const userdata);
static int _message_handler(xmpp_conn_t * const conn, 
    xmpp_stanza_t * const stanza, void * const userdata);
static int _roster_handler(xmpp_conn_t * const conn, 
    xmpp_stanza_t * const stanza, void * const userdata);
static int _presence_handler(xmpp_conn_t * const conn, 
    xmpp_stanza_t * const stanza, void * const userdata);
static int _ping_timed_handler(xmpp_conn_t * const conn, void * const userdata);

void
jabber_init(const int disable_tls)
{
    log_info("Initialising XMPP");
    jabber_conn.conn_status = JABBER_STARTED;
    jabber_conn.presence = PRESENCE_OFFLINE;
    jabber_conn.tls_disabled = disable_tls;
}

void
jabber_shutdown(void)
{
    // free memory for connection and context
    xmpp_conn_release(jabber_conn.conn);
    xmpp_ctx_free(jabber_conn.ctx);

    // shutdown libstrophe
    xmpp_shutdown();
}

jabber_conn_status_t
jabber_connect(const char * const user, 
    const char * const passwd)
{
    log_info("Connecting as %s", user);
    xmpp_initialize();

    jabber_conn.log = _xmpp_get_file_logger();
    jabber_conn.ctx = xmpp_ctx_new(NULL, jabber_conn.log);
    jabber_conn.conn = xmpp_conn_new(jabber_conn.ctx);

    xmpp_conn_set_jid(jabber_conn.conn, user);
    xmpp_conn_set_pass(jabber_conn.conn, passwd);

    if (jabber_conn.tls_disabled)
        xmpp_conn_disable_tls(jabber_conn.conn);

    int connect_status = xmpp_connect_client(jabber_conn.conn, NULL, 0, 
        _connection_handler, jabber_conn.ctx);

    if (connect_status == 0)
        jabber_conn.conn_status = JABBER_CONNECTING;
    else  
        jabber_conn.conn_status = JABBER_DISCONNECTED;

    return jabber_conn.conn_status;
}

gboolean
jabber_disconnect(void)
{
    // if connected, send end stream and wait for response
    if (jabber_conn.conn_status == JABBER_CONNECTED) {
        log_info("Closing connection");
        xmpp_disconnect(jabber_conn.conn);
        jabber_conn.conn_status = JABBER_DISCONNECTING;
        return TRUE;

    // if disconnected dont wait just shutdown
    } else if (jabber_conn.conn_status == JABBER_DISCONNECTED) {
        log_info("No connection open");
        return FALSE;

    // any other states, just shutdown
    } else {
        return FALSE;
    }
}

void
jabber_process_events(void)
{
    if (jabber_conn.conn_status == JABBER_CONNECTED 
            || jabber_conn.conn_status == JABBER_CONNECTING
            || jabber_conn.conn_status == JABBER_DISCONNECTING)
        xmpp_run_once(jabber_conn.ctx, 10);
}

void
jabber_send(const char * const msg, const char * const recipient)
{
    char *coded_msg = str_replace(msg, "&", "&amp;");
    char *coded_msg2 = str_replace(coded_msg, "<", "&lt;");
    char *coded_msg3 = str_replace(coded_msg2, ">", "&gt;");

    xmpp_stanza_t *reply, *body, *text, *active;
    
    active = xmpp_stanza_new(jabber_conn.ctx);
    xmpp_stanza_set_name(active, "active");
    xmpp_stanza_set_ns(active, "http://jabber.org/protocol/chatstates");

    reply = xmpp_stanza_new(jabber_conn.ctx);
    xmpp_stanza_set_name(reply, "message");
    xmpp_stanza_set_type(reply, "chat");
    xmpp_stanza_set_attribute(reply, "to", recipient);

    body = xmpp_stanza_new(jabber_conn.ctx);
    xmpp_stanza_set_name(body, "body");

    text = xmpp_stanza_new(jabber_conn.ctx);
    xmpp_stanza_set_text(text, coded_msg3);
    xmpp_stanza_add_child(reply, active);
    xmpp_stanza_add_child(body, text);
    xmpp_stanza_add_child(reply, body);

    xmpp_send(jabber_conn.conn, reply);
    xmpp_stanza_release(reply);
    free(coded_msg);
    free(coded_msg2);
    free(coded_msg3);
}

void
jabber_roster_request(void)
{
    xmpp_stanza_t *iq, *query;

    iq = xmpp_stanza_new(jabber_conn.ctx);
    xmpp_stanza_set_name(iq, "iq");
    xmpp_stanza_set_type(iq, "get");
    xmpp_stanza_set_id(iq, "roster");

    query = xmpp_stanza_new(jabber_conn.ctx);
    xmpp_stanza_set_name(query, "query");
    xmpp_stanza_set_ns(query, XMPP_NS_ROSTER);

    xmpp_stanza_add_child(iq, query);
    xmpp_stanza_release(query);
    xmpp_send(jabber_conn.conn, iq);
    xmpp_stanza_release(iq);
}

void
jabber_update_presence(jabber_presence_t status, const char * const msg)
{
    jabber_conn.presence = status;

    xmpp_stanza_t *pres, *show;

    pres = xmpp_stanza_new(jabber_conn.ctx);
    xmpp_stanza_set_name(pres, "presence");
    
    if (status != PRESENCE_ONLINE) {
        show = xmpp_stanza_new(jabber_conn.ctx);
        xmpp_stanza_set_name(show, "show");
        xmpp_stanza_t *text = xmpp_stanza_new(jabber_conn.ctx);

        if (status == PRESENCE_AWAY)
            xmpp_stanza_set_text(text, "away");
        else if (status == PRESENCE_DND)
            xmpp_stanza_set_text(text, "dnd");
        else if (status == PRESENCE_CHAT)
            xmpp_stanza_set_text(text, "chat");
        else if (status == PRESENCE_XA)
            xmpp_stanza_set_text(text, "xa");
        else 
            xmpp_stanza_set_text(text, "online");

        xmpp_stanza_add_child(show, text);
        xmpp_stanza_add_child(pres, show);
        xmpp_stanza_release(text);
        xmpp_stanza_release(show);
    }

    if (msg != NULL) {
        xmpp_stanza_t *status = xmpp_stanza_new(jabber_conn.ctx);
        xmpp_stanza_set_name(status, "status");
        xmpp_stanza_t *text = xmpp_stanza_new(jabber_conn.ctx);

        xmpp_stanza_set_text(text, msg);

        xmpp_stanza_add_child(status, text);
        xmpp_stanza_add_child(pres, status);
        xmpp_stanza_release(text);
        xmpp_stanza_release(status);
    }

    xmpp_send(jabber_conn.conn, pres);
    xmpp_stanza_release(pres);
}

jabber_conn_status_t
jabber_get_connection_status(void)
{
    return (jabber_conn.conn_status);
}

const char *
jabber_get_jid(void)
{
    return xmpp_conn_get_jid(jabber_conn.conn);
}

void
jabber_free_resources(void)
{
	xmpp_conn_release(jabber_conn.conn);
	xmpp_ctx_free(jabber_conn.ctx);
	xmpp_shutdown();
}

static int
_message_handler(xmpp_conn_t * const conn, 
    xmpp_stanza_t * const stanza, void * const userdata)
{
    xmpp_stanza_t *body = xmpp_stanza_get_child_by_name(stanza, "body");

    // if no message, check for chatstates
    if (body == NULL) {

        if (prefs_get_typing()) {
            if (xmpp_stanza_get_child_by_name(stanza, "active") != NULL) {
                // active
            } else if (xmpp_stanza_get_child_by_name(stanza, "composing") != NULL) {
                // composing
                char *from = xmpp_stanza_get_attribute(stanza, "from");
                prof_handle_typing(from);
            }
        }
        
        return 1;
    }

    // message body recieved
    char *type = xmpp_stanza_get_attribute(stanza, "type");
    if(strcmp(type, "error") == 0)
        return 1;

    char *message = xmpp_stanza_get_text(body);
    char *from = xmpp_stanza_get_attribute(stanza, "from");
    prof_handle_incoming_message(from, message);

    return 1;
}

static void
_connection_handler(xmpp_conn_t * const conn, 
    const xmpp_conn_event_t status, const int error, 
    xmpp_stream_error_t * const stream_error, void * const userdata)
{
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;

    if (status == XMPP_CONN_CONNECT) {
        const char *jid = xmpp_conn_get_jid(conn);
        prof_handle_login_success(jid);    

        xmpp_stanza_t* pres;
        xmpp_handler_add(conn, _message_handler, NULL, "message", NULL, ctx);
        xmpp_handler_add(conn, _presence_handler, NULL, "presence", NULL, ctx);
        xmpp_id_handler_add(conn, _roster_handler, "roster", ctx);
        xmpp_timed_handler_add(conn, _ping_timed_handler, PING_INTERVAL, ctx);

        pres = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(pres, "presence");
        xmpp_send(conn, pres);
        xmpp_stanza_release(pres);

        jabber_conn.conn_status = JABBER_CONNECTED;
        jabber_conn.presence = PRESENCE_ONLINE;
        jabber_roster_request();
    } else {
    
        // received close stream response from server after disconnect
        if (jabber_conn.conn_status == JABBER_DISCONNECTING) {
            jabber_conn.conn_status = JABBER_DISCONNECTED;
            jabber_conn.presence = PRESENCE_OFFLINE;
            
        // lost connection for unkown reason
        } else if (jabber_conn.conn_status == JABBER_CONNECTED) {
            prof_handle_lost_connection();    
            xmpp_stop(ctx);
            jabber_conn.conn_status = JABBER_DISCONNECTED;
            jabber_conn.presence = PRESENCE_OFFLINE;

        // login attempt failed
        } else {
            prof_handle_failed_login();
            xmpp_stop(ctx);
            jabber_conn.conn_status = JABBER_DISCONNECTED;
            jabber_conn.presence = PRESENCE_OFFLINE;
        }
    }
}

static int
_roster_handler(xmpp_conn_t * const conn, 
    xmpp_stanza_t * const stanza, void * const userdata)
{
    xmpp_stanza_t *query, *item;
    char *type = xmpp_stanza_get_type(stanza);
    
    if (strcmp(type, "error") == 0)
        log_error("Roster query failed");
    else {
        query = xmpp_stanza_get_child_by_name(stanza, "query");
        GSList *roster = NULL;
        item = xmpp_stanza_get_children(query);
        
        while (item != NULL) {
            const char *name = xmpp_stanza_get_attribute(item, "name");
            const char *jid = xmpp_stanza_get_attribute(item, "jid");

            jabber_roster_entry *entry = malloc(sizeof(jabber_roster_entry));

            if (name != NULL) {
                entry->name = strdup(name);
            } else {
                entry->name = NULL;
            }
            entry->jid = strdup(jid);

            roster = g_slist_append(roster, entry);
            item = xmpp_stanza_get_next(item);
        }

        prof_handle_roster(roster);
    }
    
    return 1;
}

static int
_ping_timed_handler(xmpp_conn_t * const conn, void * const userdata)
{
    if (jabber_conn.conn_status == JABBER_CONNECTED) {
        xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;
        
        xmpp_stanza_t *iq, *ping;

        iq = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(iq, "iq");
        xmpp_stanza_set_type(iq, "get");
        xmpp_stanza_set_id(iq, "c2s1");

        ping = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(ping, "ping");

        xmpp_stanza_set_ns(ping, "urn:xmpp:ping");

        xmpp_stanza_add_child(iq, ping);
        xmpp_stanza_release(ping);
        xmpp_send(conn, iq);
        xmpp_stanza_release(iq);
    }

    return 1;
}

static int
_presence_handler(xmpp_conn_t * const conn, 
    xmpp_stanza_t * const stanza, void * const userdata)
{
    const char *jid = xmpp_conn_get_jid(jabber_conn.conn);
    char jid_cpy[strlen(jid) + 1];
    strcpy(jid_cpy, jid);
    char *short_jid = strtok(jid_cpy, "/");
    
    char *from = xmpp_stanza_get_attribute(stanza, "from");
    char *short_from = strtok(from, "/");
    char *type = xmpp_stanza_get_attribute(stanza, "type");

    char *show_str, *status_str;
   
    xmpp_stanza_t *show = xmpp_stanza_get_child_by_name(stanza, "show");
    if (show != NULL)
        show_str = xmpp_stanza_get_text(show);
    else
        show_str = NULL;

    xmpp_stanza_t *status = xmpp_stanza_get_child_by_name(stanza, "status");
    if (status != NULL)    
        status_str = xmpp_stanza_get_text(status);
    else 
        status_str = NULL;

    if (strcmp(short_jid, short_from) !=0) {
        if (type == NULL) {
            prof_handle_contact_online(short_from, show_str, status_str);
        } else {
            prof_handle_contact_offline(short_from, show_str, status_str);
        }
    }

    return 1;
}

static log_level_t
_get_log_level(xmpp_log_level_t xmpp_level)
{
    if (xmpp_level == XMPP_LEVEL_DEBUG) {
        return PROF_LEVEL_DEBUG;
    } else if (xmpp_level == XMPP_LEVEL_INFO) {
        return PROF_LEVEL_INFO;
    } else if (xmpp_level == XMPP_LEVEL_WARN) {
        return PROF_LEVEL_WARN;
    } else {
        return PROF_LEVEL_ERROR;
    }
}

static xmpp_log_level_t
_get_xmpp_log_level()
{
    log_level_t prof_level = log_get_filter();

    if (prof_level == PROF_LEVEL_DEBUG) {
        return XMPP_LEVEL_DEBUG;
    } else if (prof_level == PROF_LEVEL_INFO) {
        return XMPP_LEVEL_INFO;
    } else if (prof_level == PROF_LEVEL_WARN) {
        return XMPP_LEVEL_WARN;
    } else {
        return XMPP_LEVEL_ERROR;
    }
}

static void
_xmpp_file_logger(void * const userdata, const xmpp_log_level_t level,
    const char * const area, const char * const msg)
{
    log_level_t prof_level = _get_log_level(level);
    log_msg(prof_level, area, msg);
}

static xmpp_log_t *
_xmpp_get_file_logger()
{
    xmpp_log_level_t level = _get_xmpp_log_level();
    xmpp_log_t *file_log = malloc(sizeof(xmpp_log_t));

    file_log->handler = _xmpp_file_logger;
    file_log->userdata = &level;

    return file_log;
}

