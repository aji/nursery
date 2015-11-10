SUBDIRS = src modules
CLEANDIRS = ${SUBDIRS}
DISTCLEAN = extra.mk buildsys.mk config.log config.status aclocal.m4 configure config.sub config.guess install-sh

include extra.mk
include buildsys.mk

install-extra:
	i="help files"; \
	${INSTALL_STATUS}; \
	mkdir -p ${DESTDIR}${prefix}/help; \
	if cp -r help ${DESTDIR}${prefix}/help; then \
		${INSTALL_OK}; \
	else \
		${INSTALL_FAILED}; \
	fi

uninstall-extra:
	i="help files"; \
	if rm -rf ${DESTDIR}${prefix}/help; then \
		${DELETE_OK}; \
	else \
		${DELETE_FAILED}; \
	fi
