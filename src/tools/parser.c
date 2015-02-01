/*
 * parser.c
 *
 * Copyright (C) 2012 - 2014 James Booth <boothj5@gmail.com>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "common.h"

/* nextarg is like strtok, but it has its own delimiter set.
 * nextarg will never return NULL when cmd is non-NULL.
 * if cmd == NULL, then nextarg returns the next argument from the
 * previous call to nextarg.
 * the pointer returned from nextarg should be freed by the caller.
 * char *s = "a bc \"def' `ghi\"";
 * char *c = nextarg(s);
 * while(c){
 *     printf("%s\n", c);
 *     free(c);
 *     c = nextarg(NULL);
 * }
 * This code is maintained by incertia at incertia.net.
 * The OP can be found at
 * http://www.incertia.net/blog/simple-argument-parser-in-c/ */
char *cur = NULL;
char *s = NULL;
char * nextarg(const char *cmd){
    int i = 0, j = 0;
    gboolean in_token = FALSE;
    char *r, *b, quote = '\0';

    /* we operate on our own local copy cur */
    if(cmd){
        if(cur) free(cur);
        s = cur = strdup(cmd);
    }
    if(!cur || !s) return NULL;

    /* search s, which will point to the end of the current token */
    b = s;
    while(*s){
        if(!in_token){
            /* we encountered what might not be a token */
            if(!isspace(*s)){
                in_token = TRUE;
                if(*s == '"' || *s == '\'' || *s == '`') quote = *s, b = s;
                else i++, b = s; /* increment char count */
                if(*s == '\\'){
                    /* fix edge case where the last char is \ */
                    if(!*(s + 1)){
                        *s = '\0';
                        if(i > 1) break;
                        else {
                            free(cur);
                            return cur = s = NULL;
                        }
                    }
                    s++; /* skip the escaped character */
                }
            }
        } else {
            /* we are inside a token */
            if(quote){
                if(quote == *s){
                    // we found the end
                    quote = '\0';
                    s++;
                    continue;
                }
            } else {
                if(isspace(*s)){
                    // this is the end
                    break;
                } else if(*s == '"' || *s == '\'' || *s == '`'){
                    quote = *s++;
                    continue;
                }
            }
            if(*s == '\\'){
                /* this covers the other edge case where last char is \ */
                if(!*(s + 1)){
                    *s = '\0';
                    if(i) break;
                    else {
                        free(cur);
                        return cur = s = NULL;
                    }
                }
                s++; /* skip the escaped character */
            }
            i++;
        }
        s++;
    }

    /* allocate the string */
    r = malloc((i + 1) * sizeof(char));

    /* fill the string */
    r[i] = '\0';
    quote = '\0';
    while(j < i){
        if(*b == '\\'){
            b++;
            r[j++] = *b++;
        } else if(*b == '"' || *b == '\'' || *b == '`')
            if(!quote) quote = *b++;
            else if(quote == *b) quote = '\0', b++;
            else r[j++] = *b++;
        else r[j++] = *b++;
    }

    while(*s && isspace(*s)) s++;
    if(!*s){
        free(cur);
        s = cur = NULL;
    }

    return r;
}


/*
 * Take a full line of input and return an array of strings representing
 * the arguments of a command.
 * If the number of arguments found is less than min, or more than max
 * NULL is returned.
 *
 * inp - The line of input
 * min - The minimum allowed number of arguments
 * max - The maxmimum allowed number of arguments
 *
 * Returns - An NULL terminated array of strings representing the aguments
 * of the command, or NULL if the validation fails.
 *
 * E.g. the following input line:
 *
 * /cmd arg1 arg2
 *
 * Will return a pointer to the following array:
 *
 * { "arg1", "arg2", NULL }
 *
 */
gchar **
parse_args(const char * const inp, int min, int max, gboolean *result)
{
    if (inp == NULL) {
        *result = FALSE;
        return NULL;
    }

    char *token = nextarg(inp);
    GSList *tokens = g_slist_append(NULL, token);
    while((token = nextarg(NULL))) tokens = g_slist_append(tokens, token);

    int num = g_slist_length(tokens) - 1;

    // if num args not valid return NULL
    if ((num < min) || (num > max)) {
        g_slist_free_full(tokens, free);
        *result = FALSE;
        return NULL;

    // if min allowed is 0 and 0 found, return empty char* array
    } else if (min == 0 && num == 0) {
        g_slist_free_full(tokens, free);
        gchar **args = malloc((num + 1) * sizeof(*args));
        args[0] = NULL;
        *result = TRUE;
        return args;

    // otherwise return args array
    } else {
        gchar **args = malloc((num + 1) * sizeof(*args));
        GSList *token = tokens;
        token = g_slist_next(token);
        int arg_count = 0;

        while (token != NULL) {
            args[arg_count++] = strdup(token->data);
            token = g_slist_next(token);
        }

        args[arg_count] = NULL;
        g_slist_free_full(tokens, free);
        *result = TRUE;
        return args;
    }
}

/*
 * Take a full line of input and return an array of strings representing
 * the arguments of a command.  This function handles when the last parameter
 * to the command is free text e.g.
 *
 * /msg user@host here is a message
 *
 * If the number of arguments found is less than min, or more than max
 * NULL is returned.
 *
 * inp - The line of input
 * min - The minimum allowed number of arguments
 * max - The maxmimum allowed number of arguments
 *
 * Returns - An NULL terminated array of strings representing the aguments
 * of the command, or NULL if the validation fails.
 *
 * E.g. the following input line:
 *
 * /cmd arg1 arg2 some free text
 *
 * Will return a pointer to the following array:
 *
 * { "arg1", "arg2", "some free text", NULL }
 *
 */
gchar **
parse_args_with_freetext(const char * const inp, int min, int max, gboolean *result)
{
    if (inp == NULL) {
        *result = FALSE;
        return NULL;
    }

    // copy and strip input of leading/trailing whitepsace
    char *copy = strdup(inp);
    g_strstrip(copy);

    int inp_size = g_utf8_strlen(copy, -1);
    gboolean in_token = FALSE;
    gboolean in_freetext = FALSE;
    gboolean in_quotes = FALSE;
    char *token_start = &copy[0];
    int token_size = 0;
    int num_tokens = 0;
    GSList *tokens = NULL;

    // add tokens to GSList
    int i;
    for (i = 0; i < inp_size; i++) {
        gchar *curr_ch = g_utf8_offset_to_pointer(copy, i);
        gunichar curr_uni = g_utf8_get_char(curr_ch);

        if (!in_token) {
            if (curr_uni == ' ') {
                continue;
            } else {
                in_token = TRUE;
                num_tokens++;
                if ((num_tokens == max + 1) && (curr_uni != '"')) {
                    in_freetext = TRUE;
                } else if (curr_uni == '"') {
                    in_quotes = TRUE;
                    i++;
                    gchar *next_ch = g_utf8_next_char(curr_ch);
                    gunichar next_uni = g_utf8_get_char(next_ch);
                    token_start = next_ch;
                    token_size += g_unichar_to_utf8(next_uni, NULL);
                }
                if (curr_uni == '"') {
                    gchar *next_ch = g_utf8_next_char(curr_ch);
                    token_start = next_ch;
                } else {
                    token_start = curr_ch;
                    token_size += g_unichar_to_utf8(curr_uni, NULL);
                }
            }
        } else {
            if (in_quotes) {
                if (curr_uni == '"') {
                    tokens = g_slist_append(tokens, g_strndup(token_start,
                        token_size));
                    token_size = 0;
                    in_token = FALSE;
                    in_quotes = FALSE;
                } else {
                    if (curr_uni != '"') {
                        token_size += g_unichar_to_utf8(curr_uni, NULL);
                    }
                }
            } else {
                if (in_freetext) {
                    token_size += g_unichar_to_utf8(curr_uni, NULL);
                } else if (curr_uni == ' ') {
                    tokens = g_slist_append(tokens, g_strndup(token_start,
                        token_size));
                    token_size = 0;
                    in_token = FALSE;
                } else if (curr_uni != '"') {
                    token_size += g_unichar_to_utf8(curr_uni, NULL);
                }
            }
        }
    }

    if (in_token) {
        tokens = g_slist_append(tokens, g_strndup(token_start, token_size));
    }

    free(copy);

    int num = g_slist_length(tokens) - 1;

    // if num args not valid return NULL
    if ((num < min) || (num > max)) {
        g_slist_free_full(tokens, free);
        *result = FALSE;
        return NULL;

    // if min allowed is 0 and 0 found, return empty char* array
    } else if (min == 0 && num == 0) {
        g_slist_free_full(tokens, free);
        gchar **args = malloc((num + 1) * sizeof(*args));
        args[0] = NULL;
        *result = TRUE;
        return args;

    // otherwise return args array
    } else {
        gchar **args = malloc((num + 1) * sizeof(*args));
        GSList *token = tokens;
        token = g_slist_next(token);
        int arg_count = 0;

        while (token != NULL) {
            args[arg_count++] = strdup(token->data);
            token = g_slist_next(token);
        }

        args[arg_count] = NULL;
        g_slist_free_full(tokens, free);
        *result = TRUE;
        return args;
    }
}

int
count_tokens(const char * const string)
{
    int length = g_utf8_strlen(string, -1);
    gboolean in_quotes = FALSE;
    int num_tokens = 0;
    int i = 0;

    // include first token
    num_tokens++;

    for (i = 0; i < length; i++) {
        gchar *curr_ch = g_utf8_offset_to_pointer(string, i);
        gunichar curr_uni = g_utf8_get_char(curr_ch);

        if (curr_uni == ' ') {
            if (!in_quotes) {
                num_tokens++;
            }
        } else if (curr_uni == '"') {
            if (in_quotes) {
                in_quotes = FALSE;
            } else {
                in_quotes = TRUE;
            }
        }
    }

    return num_tokens;
}

char *
get_start(const char * const string, int tokens)
{
    GString *result = g_string_new("");
    int length = g_utf8_strlen(string, -1);
    gboolean in_quotes = FALSE;
    char *result_str = NULL;
    int num_tokens = 0;
    int i = 0;

    // include first token
    num_tokens++;

    for (i = 0; i < length; i++) {
        gchar *curr_ch = g_utf8_offset_to_pointer(string, i);
        gunichar curr_uni = g_utf8_get_char(curr_ch);

        if (num_tokens < tokens) {
            gchar *uni_char = malloc(7);
            int len = g_unichar_to_utf8(curr_uni, uni_char);
            uni_char[len] = '\0';
            g_string_append(result, uni_char);
        }
        if (curr_uni == ' ') {
            if (!in_quotes) {
                num_tokens++;
            }
        } else if (curr_uni == '"') {
            if (in_quotes) {
                in_quotes = FALSE;
            } else {
                in_quotes = TRUE;
            }
        }
    }

    result_str = result->str;
    g_string_free(result, FALSE);

    return result_str;
}

GHashTable *
parse_options(gchar **args, gchar **opt_keys, gboolean *res)
{
    GList *keys = NULL;
    int i;
    for (i = 0; i < g_strv_length(opt_keys); i++) {
        keys = g_list_append(keys, opt_keys[i]);
    }

    GHashTable *options = NULL;

    // no options found, success
    if (args[0] == NULL) {
        options = g_hash_table_new(g_str_hash, g_str_equal);
        *res = TRUE;
        g_list_free(keys);
        return options;
    }

    // validate options
    int curr;
    GList *found_keys = NULL;
    for (curr = 0; curr < g_strv_length(args); curr+= 2) {
        // check if option valid
        if (g_list_find_custom(keys, args[curr], (GCompareFunc)g_strcmp0) == NULL) {
            *res = FALSE;
            g_list_free(keys);
            return options;
        }

        // check if duplicate
        if (g_list_find_custom(found_keys, args[curr], (GCompareFunc)g_strcmp0) != NULL) {
            *res = FALSE;
            g_list_free(keys);
            return options;
        }

        // check value given
        if (args[curr+1] == NULL) {
            *res = FALSE;
            g_list_free(keys);
            return options;
        }

        found_keys = g_list_append(found_keys, args[curr]);
    }
    g_list_free(found_keys);
    g_list_free(keys);

    // create map
    options = g_hash_table_new(g_str_hash, g_str_equal);
    *res = TRUE;
    for (curr = 0; curr < g_strv_length(args); curr+=2) {
        g_hash_table_insert(options, args[curr], args[curr+1]);
    }

    return options;
}

void
options_destroy(GHashTable *options)
{
    if (options != NULL) {
        g_hash_table_destroy(options);
    }
}
