/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

static mowgli_patricia_t *modules;

bool a_module_init(void)
{
	modules = mowgli_patricia_create(NULL);

	mowgli_hook_associate("conf.start", a_module_conf_start, NULL);
	mowgli_hook_associate("conf.item.module", a_module_conf_module, NULL);
	mowgli_hook_associate("conf.end", a_module_conf_end, NULL);

	return true;
}

void a_module_deinit_destroy_cb(const char *key, void *data, void *unused)
{
	struct a_module *m = data;
	m->unload();
}
void a_module_deinit(void)
{
	mowgli_patricia_destroy(modules, a_module_deinit_destroy_cb, NULL);

	mowgli_hook_dissociate("conf.start", a_module_conf_start);
	mowgli_hook_dissociate("conf.item.module", a_module_conf_module);
	mowgli_hook_dissociate("conf.end", a_module_conf_end);
}


int a_module_locate(char *buf, size_t len, char *name)
{
	struct stat mstat;

	/* TODO: don't use .so explicitly */
	snprintf(buf, len, "modules/%s.so", name);

	if (stat(buf, &mstat) < 0) {
		a_log(LERROR, "Failed to stat %s: %s", buf, strerror(errno));
		return -1;
	}

	if (mstat.st_uid != getuid())
		a_log(LWARN, "Warning: we don't own %s", buf);

	if ((mstat.st_mode & 0111) == 0)
		a_log(LWARN, "Warning: Executable bit on %s not set", buf);

	return 0;
}

struct a_module *a_module_load(char *name)
{
	struct a_module *m;
	char location[8192];

	a_log(LINFO, "Load module %s", name);

	m = mowgli_patricia_retrieve(modules, name);
	if (m != NULL) {
		a_log(LWARN, "Attempted to load already loaded module %s", name);
		return m;
	}

	if (a_module_locate(location, 8192, name) < 0) {
		a_log(LERROR, "Could not locate module %s", name);
		return NULL;
	}

	m = mowgli_alloc(sizeof(*m));
	if (m == NULL) {
		a_log(LFATAL, "Could not allocate memory for module");
		return NULL;
	}

	m->name = mowgli_strdup(name);
	if (m->name == NULL) {
		a_log(LFATAL, "mowgli_strdup(name) failed (name=%s)", name);
		goto fail_strdup;
	}

	m->syms = mowgli_module_open(location);
	if (m->syms == NULL) {
		a_log(LFATAL, "Failed to open file %s as module", location);
		goto fail_open;
	}

	m->load = mowgli_module_symbol(m->syms, "module_load");
	m->unload = mowgli_module_symbol(m->syms, "module_unload");

	if (m->load == NULL || m->unload == NULL) {
		a_log(LERROR, "%s does not have both module_load and module_unload", location);
		goto fail_syms;
	}

	if (!m->load()) {
		a_log(LERROR, "%s module_load failed!", location);
		goto fail_load;
	}

	m->keep = true;

	mowgli_patricia_add(modules, name, m);

	a_log(LDEBUG, "Loaded module %s successfully", m->name);

	return m;


fail_load:
fail_syms:
	mowgli_module_close(m->syms);
fail_open:
	mowgli_free(m->name);
fail_strdup:
	mowgli_free(m);
	return NULL;
}

void a_module_unload(char *name)
{
	struct a_module *m;

	a_log(LINFO, "Unload module %s", name);

	m = mowgli_patricia_retrieve(modules, name);

	if (m == NULL) {
		a_log(LWARN, "Attempted to unload not loaded module %s", name);
		return;
	}

	mowgli_patricia_delete(modules, name);

	mowgli_module_close(m->syms);
	mowgli_free(m->name);
	mowgli_free(m);

	a_log(LDEBUG, "Unloaded module %s successfully", m->name);
}


int a_module_conf_start_foreach_cb(const char *key, void *data, void *priv)
{
	struct a_module *m = data;
	m->keep = false;
	return 0;
}
void a_module_conf_start(void *unused, void *priv)
{
	a_log(LDEBUG, "Marking all loaded modules for deletion");
	mowgli_patricia_foreach(modules, a_module_conf_start_foreach_cb, NULL);
}

void a_module_conf_module(void *data, void *priv)
{
	struct a_module *m;
	mowgli_config_file_entry_t *conf = data;

	m = mowgli_patricia_retrieve(modules, conf->vardata);

	if (m != NULL) {
		a_log(LDEBUG, "Marking %s for keep", m->name);
		m->keep = true;
	} else {
		m = a_module_load(conf->vardata);
	}
}

int a_module_conf_end_foreach_cb(const char *key, void *data, void *priv)
{
	struct a_module *m = data;

	if (!m->keep) {
		a_log(LDEBUG, "Deleting module %s marked for deletion", key);
		a_module_unload(m->name);
	}

	return 0;
}
void a_module_conf_end(void *unused, void *priv)
{
	a_log(LDEBUG, "Deleting all marked modules");
	mowgli_patricia_foreach(modules, a_module_conf_end_foreach_cb, NULL);
}
