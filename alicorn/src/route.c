/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root.
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

static void route_incoming(struct a_route_ctx *ctx)
{
	mowgli_node_t *n;
	if (ctx->eat_message)
		return;
	MOWGLI_LIST_FOREACH(n, ctx->net->clients->head)
		a_client_printf(n->data, "%s", ctx->msg_raw);
}

static void route_outgoing(struct a_route_ctx *ctx)
{
	if (ctx->eat_message || ctx->cli->net == NULL)
		return;
	a_network_printf(ctx->cli->net, "%s", ctx->msg_raw);
}

void a_route(void *source, bool incoming, char *msg, size_t n)
{
	struct a_route_ctx ctx;

	memset(&ctx.msg, 0, sizeof(ctx.msg));

	ctx.source = source;

	if (n > 512)
		n = 512;

	memcpy(ctx.msg_raw, msg, n);
	ctx.msg_raw[n] = '\0';

	if (irc_message_parse(&ctx.msg, ctx.msg_raw) < 0) {
		a_log(LWARN, "Failed to parse %s message: %s",
				incoming ? "incoming" : "outgoing", msg);
		return;
	}

	ctx.eat_message = false;

	if (incoming) {
		irc_hook_ext_dispatch(alicorn.h_incoming, &ctx.msg, &ctx);
		route_incoming(&ctx);
	} else {
		irc_hook_ext_dispatch(alicorn.h_outgoing, &ctx.msg, &ctx);
		route_outgoing(&ctx);
	}
}
