# $Id$

BUILDDIR =	obj
SRCDIR=		../../../..


all: build

build: build-softHSM build-hsmbully build-OpenDNSSEC
    
install: install-softHSM install-hsmbully install-OpenDNSSEC


build-OpenDNSSEC:
	-mkdir -p $(BUILDDIR)/OpenDNSSEC
	cd $(BUILDDIR)/OpenDNSSEC; $(SRCDIR)/OpenDNSSEC/configure $(CONFIGURE_ARGS)
	cd $(BUILDDIR)/OpenDNSSEC; $(MAKE)

build-softHSM:
	-mkdir -p $(BUILDDIR)/softHSM
	cd $(BUILDDIR)/softHSM; $(SRCDIR)/softHSM/configure $(CONFIGURE_ARGS)
	cd $(BUILDDIR)/softHSM; $(MAKE)
	
build-hsmbully:
	-mkdir -p $(BUILDDIR)/hsmbully
	cd $(BUILDDIR)/hsmbully; $(SRCDIR)/hsmbully/configure $(CONFIGURE_ARGS)
	cd $(BUILDDIR)/hsmbully; $(MAKE)


install-OpenDNSSEC:
	cd $(BUILDDIR)/OpenDNSSEC; $(SUDO) $(MAKE) install

install-softHSM:
	cd $(BUILDDIR)/softHSM; $(SUDO) $(MAKE) install

install-hsmbully:
	cd $(BUILDDIR)/hsmbully; $(SUDO) $(MAKE) install

clean:
	rm -fr $(BUILDDIR)
