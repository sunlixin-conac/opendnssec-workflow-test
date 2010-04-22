# $Id$

BUILDDIR =	obj
SRCDIR=		../../../../..

SOFTHSM=	softHSM
OPENDNSSEC=	OpenDNSSEC


all: build

build: build-softhsm build-opendnssec
    
install: install-softhsm install-opendnssec


opendnssec: build-opendnssec

softhsm: build-softhsm

hsmbully: build-hsmbully


configure-opendnssec:
	-mkdir -p $(BUILDDIR)/$(OPENDNSSEC)
	cd $(BUILDDIR)/$(OPENDNSSEC); $(SRCDIR)/$(OPENDNSSEC)/configure $(CONFIGURE_ARGS)

configure-softhsm:
	-mkdir -p $(BUILDDIR)/$(SOFTHSM)
	cd $(BUILDDIR)/$(SOFTHSM); $(SRCDIR)/$(SOFTHSM)/configure $(CONFIGURE_ARGS)
	
configure-hsmbully:
	-mkdir -p $(BUILDDIR)/hsmbully
	cd $(BUILDDIR)/hsmbully; $(SRCDIR)/hsmbully/configure $(CONFIGURE_ARGS)


build-opendnssec: configure-opendnssec
	cd $(BUILDDIR)/$(OPENDNSSEC); $(MAKE)

build-softhsm: configure-softhsm
	cd $(BUILDDIR)/$(SOFTHSM); $(MAKE)
	
build-hsmbully: configure-hsmbully
	cd $(BUILDDIR)/hsmbully; $(MAKE)


install-opendnssec:
	cd $(BUILDDIR)/$(OPENDNSSEC); $(SUDO) $(MAKE) install

install-softhsm:
	cd $(BUILDDIR)/$(SOFTHSM); $(SUDO) $(MAKE) install

install-hsmbully:
	cd $(BUILDDIR)/hsmbully; $(SUDO) $(MAKE) install


clean:
	rm -fr $(BUILDDIR)
