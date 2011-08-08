SUBDIRS = libmary

EXTRA_DIST = \
	local_conifg.h.in	\
	libmary-1.0.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmary-1.0.pc

clean-local:
	make -C tests clean

