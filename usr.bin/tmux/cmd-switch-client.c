/* $OpenBSD: cmd-switch-client.c,v 1.48 2017/02/06 15:00:41 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Switch client to a different session.
 */

static enum cmd_retval	cmd_switch_client_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_switch_client_entry = {
	.name = "switch-client",
	.alias = "switchc",

	.args = { "lc:Enpt:rT:", 0, 0 },
	.usage = "[-Elnpr] [-c target-client] [-t target-session] "
		 "[-T key-table]",

	.cflag = CMD_CLIENT,
	.tflag = CMD_SESSION_WITHPANE,

	.flags = CMD_READONLY,
	.exec = cmd_switch_client_exec
};

static enum cmd_retval
cmd_switch_client_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct cmd_state	*state = &item->state;
	struct client		*c = state->c;
	struct session		*s = item->state.tflag.s;
	struct window_pane	*wp;
	const char		*tablename;
	struct key_table	*table;

	if (args_has(args, 'r'))
		c->flags ^= CLIENT_READONLY;

	tablename = args_get(args, 'T');
	if (tablename != NULL) {
		table = key_bindings_get_table(tablename, 0);
		if (table == NULL) {
			cmdq_error(item, "table %s doesn't exist", tablename);
			return (CMD_RETURN_ERROR);
		}
		table->references++;
		key_bindings_unref_table(c->keytable);
		c->keytable = table;
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'n')) {
		if ((s = session_next_session(c->session)) == NULL) {
			cmdq_error(item, "can't find next session");
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'p')) {
		if ((s = session_previous_session(c->session)) == NULL) {
			cmdq_error(item, "can't find previous session");
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'l')) {
		if (c->last_session != NULL && session_alive(c->last_session))
			s = c->last_session;
		else
			s = NULL;
		if (s == NULL) {
			cmdq_error(item, "can't find last session");
			return (CMD_RETURN_ERROR);
		}
	} else {
		if (item->client == NULL)
			return (CMD_RETURN_NORMAL);
		if (state->tflag.wl != NULL) {
			wp = state->tflag.wp;
			if (wp != NULL)
				window_set_active_pane(wp->window, wp);
			session_set_current(s, state->tflag.wl);
		}
	}

	if (!args_has(args, 'E'))
		environ_update(s->options, c->environ, s->environ);

	if (c->session != NULL && c->session != s)
		c->last_session = c->session;
	c->session = s;
	if (!item->repeat)
		server_client_set_key_table(c, NULL);
	status_timer_start(c);
	session_update_activity(s, NULL);
	gettimeofday(&s->last_attached_time, NULL);

	recalculate_sizes();
	server_check_unattached();
	server_redraw_client(c);
	s->curw->flags &= ~WINLINK_ALERTFLAGS;
	alerts_check_session(s);

	return (CMD_RETURN_NORMAL);
}
