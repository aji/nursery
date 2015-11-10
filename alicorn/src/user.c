/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

struct a_user *a_user_create(struct a_user_ops *ops, void *priv)
{
	struct a_user *user;

	user = mowgli_alloc(sizeof(*user));
	if (!user)
		return NULL;

	user->acct = NULL;
	user->ops = ops;
	user->priv = priv;

	return user;
}

void a_user_destroy(struct a_user *user)
{
	mowgli_free(user);
}

void a_user_set_account(struct a_user *user, struct a_account *acct)
{
	user->acct = acct;
}
