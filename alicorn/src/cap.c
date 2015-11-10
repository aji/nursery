/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

static void on_register(void *net, void *priv)
{
	a_network_printf(net, "CAP LS");
}

static int on_cap_ls(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	char caps[1024];
	char reqs[1024];
	unsigned n;
	char *cap, *cap_r;

	if (parc < 1)
		return 0;

	a_log(LDEBUG, "Available capabilites: %s", parv[0]);

	snprintf(caps, 1024, "%s", parv[0]);
	n = 0;
	reqs[n] = '\0';

	cap = strtok_r(caps, " ", &cap_r);
	while (cap) {
		a_log(LDEBUG, "Considering %s", cap);

		if (!strcmp(cap, "multi-prefix")) {
			if (n != 0) {
				reqs[n++] = ' ';
				reqs[n] = '\0';
			}
			n += snprintf(reqs + n, 1024 - n, "%s", cap);
		}

		cap = strtok_r(NULL, " ", &cap_r);
	}

	if (n == 0) {
		a_log(LDEBUG, "Requesting no capabilities");
		a_network_printf(ctx->net, "CAP END");
	} else {
		a_log(LDEBUG, "Requesting capabilities: %s", reqs);
		a_network_printf(ctx->net, "CAP REQ :%s", reqs);
	}

	return 0;
}

static int on_cap_ack(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	a_network_printf(ctx->net, "CAP END");

	return 0;
}

static int cap_trampoline(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	if (parc < 5) {
		a_log(LWARN, "Recieved bad CAP command, ignoring");
		return 0;
	}

	ctx->eat_message = true;

	if (!strcmp(parv[4], "LS"))
		on_cap_ls(parc - 5, parv + 5, priv);
	else if (!strcmp(parv[4], "ACK"))
		on_cap_ack(parc - 5, parv + 5, priv);
	else
		a_log(LWARN, "Unknown CAP subcommand %s", parv[4]);

	return 0;
}

bool a_cap_init(void)
{
	mowgli_hook_associate("network.register", on_register, NULL);

	irc_hook_add(alicorn.h_incoming, "CAP", cap_trampoline);

	return true;
}

void a_cap_deinit(void)
{
	mowgli_hook_dissociate("network.register", on_register);

	irc_hook_del(alicorn.h_incoming, "CAP", cap_trampoline);
}
