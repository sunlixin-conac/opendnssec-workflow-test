# $Id$

BUILDDIR =	obj
SRCDIR=		../../../..


all: build

configure: $(BUILDDIR)/Makefile

$(BUILDDIR)/Makefile: $(BUILDDIR)
	cd $(BUILDDIR)/softHSM;    $(SRCDIR)/softHSM/configure $(CONFIGURE_ARGS)
	cd $(BUILDDIR)/hsmbully;   $(SRCDIR)/hsmbully/configure $(CONFIGURE_ARGS)
	cd $(BUILDDIR)/OpenDNSSEC; $(SRCDIR)/OpenDNSSEC/configure $(CONFIGURE_ARGS)

build: configure
	cd $(BUILDDIR)/softHSM;    $(MAKE)
	cd $(BUILDDIR)/hsmbully;   $(MAKE)
	cd $(BUILDDIR)/OpenDNSSEC; $(MAKE)
    
install:
	cd $(BUILDDIR)/softHSM;    $(SUDO) $(MAKE) install
	cd $(BUILDDIR)/hsmbully;   $(SUDO) $(MAKE) install
	cd $(BUILDDIR)/OpenDNSSEC; $(SUDO) $(MAKE) install

$(BUILDDIR):
	-mkdir $(BUILDDIR)
	-mkdir $(BUILDDIR)/softHSM
	-mkdir $(BUILDDIR)/hsmbully
	-mkdir $(BUILDDIR)/OpenDNSSEC

clean:
	rm -fr $(BUILDDIR)
