# $Id$

BUILDDIR =	obj
SRCDIR=		../../../../..


all: build

build: build-softHSM build-OpenDNSSEC
    
install: install-softHSM install-OpenDNSSEC


OpenDNSSEC: build-OpenDNSSEC

softHSM: build-softHSM

hsmbully: build-hsmbully


configure-OpenDNSSEC:
	-mkdir -p $(BUILDDIR)/OpenDNSSEC
	cd $(BUILDDIR)/OpenDNSSEC; $(SRCDIR)/OpenDNSSEC/configure $(CONFIGURE_ARGS)

configure-softHSM:
	-mkdir -p $(BUILDDIR)/softHSM
	cd $(BUILDDIR)/softHSM; $(SRCDIR)/softHSM/configure $(CONFIGURE_ARGS)
	
configure-hsmbully:
	-mkdir -p $(BUILDDIR)/hsmbully
	cd $(BUILDDIR)/hsmbully; $(SRCDIR)/hsmbully/configure $(CONFIGURE_ARGS)


build-OpenDNSSEC: configure-OpenDNSSEC
	cd $(BUILDDIR)/OpenDNSSEC; $(MAKE)

build-softHSM: configure-softHSM
	cd $(BUILDDIR)/softHSM; $(MAKE)
	
build-hsmbully: configure-hsmbully
	cd $(BUILDDIR)/hsmbully; $(MAKE)


install-OpenDNSSEC:
	cd $(BUILDDIR)/OpenDNSSEC; $(SUDO) $(MAKE) install

install-softHSM:
	cd $(BUILDDIR)/softHSM; $(SUDO) $(MAKE) install

install-hsmbully:
	cd $(BUILDDIR)/hsmbully; $(SUDO) $(MAKE) install


clean:
	rm -fr $(BUILDDIR)
