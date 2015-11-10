/*
   Alicorn - Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"


int a_config_hash(const char *config_fname)
{
	mowgli_config_file_t *config, *load;
	mowgli_config_file_entry_t *curr;
	int error = -1;
	char hookname[1024];

	a_log(LINFO, "Loading config file %s...", config_fname);
	load = mowgli_config_file_load(config_fname);

	if (load == NULL) {
		a_log(LERROR, "Failed to load config file");
		return -1;
	}

	mowgli_hook_call("conf.start", NULL);

	config = load;
	while (config) {
		curr = config->entries;

		while (curr) {
			snprintf(hookname, 1024, "conf.item.%s", curr->varname);
			
			a_log(LDEBUG, "Calling configuration hook %s", hookname);
			mowgli_hook_call(hookname, curr);

			curr = curr->next;
		}

		config = config->next;
	}

	mowgli_hook_call("conf.end", NULL); /* move actions to here */

	mowgli_config_file_free(load);

	return 0;
}


