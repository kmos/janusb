ACLOCAL_AMFLAGS = -I m4
CFLAGS = $(shell pkg-config --cflags glib-2.0)
CXXFLAGS = $(CFLAGS)
lib_LTLIBRARIES = plugin/libjanus_serial.la
libserial_la_SOURCES = plugin/libjanus_serial.c
libserial_la_LDFLAGS = -version-info 0:0:1 -ljansson $(shell pkg-config --libs glib-2.0)

LIBS = $(shell pkg-config --libs glib-2.0)

confdir = $(exec_prefix)/etc/janus
libdir = $(exec_prefix)/lib/janus/plugins
conf_DATA = conf/janus.plugin.serial.cfg.sample

##
# Configuration
##

config:
	$(MKDIR_P) $(DESTDIR)$(confdir)
	$(foreach config,$(conf_DATA),cp "$(CURDIR)/$(config)" "$(DESTDIR)$(confdir)/$(notdir $(basename $(config) .sample))";)

