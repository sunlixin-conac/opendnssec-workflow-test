# $Id$

PREFIX = /usr/local

CONFIGURE_ARGS = \
	--with-sqlite3=/usr/local \
	--with-cunit=/usr/local \
	--with-ldns=/opt/ldns \
	--with-libxml2=/usr \
	--with-libcurl=/usr/local \
	--with-botan=/usr/local \
	--disable-auditor \
	--enable-timeshift \
	--enable-eppclient

SUDO = sudo
MAKE = make -j 4

include ../Makefile.inc
