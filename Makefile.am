SUBDIRS=src extworker module/webdav 
#module/vhs_mysql module/vhs_sqlite module/whm/whmdso/vhsnode
EXTRA_DIST=module/access 
wwwdir=$(prefix)/www
incdir=$(includedir)
webadmindir=$(DEFAULT_WEBADMIN_DIR)
dist_sysconf_DATA=$(sysconf_DATA) etc/config.xml etc/config-default.xml 
dist_www_DATA=www/index.html 
dist_webadmin_DATA=webadmin/kangle.css webadmin/core.whm webadmin/logo.gif webadmin/lang.xml webadmin/lang_en.xml webadmin/lang_zh.xml webadmin/lang_tw.xml
dist_inc_DATA=src/ksapi.h
#dist_doc_DATA=doc/zh_CN/*.html doc/zh_CN/*.css
install-dist_sysconfDATA:
	@$(NORMAL_INSTALL)
	test -z "$(sysconfdir)" || $(mkdir_p) "$(DESTDIR)$(sysconfdir)"
	@list='$(dist_sysconf_DATA)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f=`echo $$p | sed -e 's|^.*/||'`;\
	  if test -f "$(DESTDIR)$(sysconfdir)/$$f" ; then echo "$(DESTDIR)$(sysconfdir)/$$f exsit.."; \
	  else echo "$(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(sysconfdir)/$$f'"; \
	  $(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(sysconfdir)/$$f"; fi;\
	done
