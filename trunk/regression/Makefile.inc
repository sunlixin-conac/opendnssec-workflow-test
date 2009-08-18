# $Id$

BUILDDIR = obj


all: build

configure: $(BUILDDIR)/Makefile

$(BUILDDIR)/Makefile: $(BUILDDIR)
	cd $(BUILDDIR)/softHSM; ../../../../softHSM/configure $(CONFIGURE_ARGS)
	cd $(BUILDDIR)/hsmbully; ../../../../hsmbully/configure $(CONFIGURE_ARGS)
	cd $(BUILDDIR); ../../../OpenDNSSEC/configure $(CONFIGURE_ARGS)

build: configure
	cd $(BUILDDIR)/softHSM; $(MAKE)
	cd $(BUILDDIR)/hsmbully; $(MAKE)
	cd $(BUILDDIR); $(MAKE)
    
install:
	cd $(BUILDDIR)/softHSM; $(SUDO) $(MAKE) install
	cd $(BUILDDIR)/hsmbully; $(SUDO) $(MAKE) install
	cd $(BUILDDIR); $(SUDO) $(MAKE) install

$(BUILDDIR):
	-mkdir $(BUILDDIR)
	-mkdir $(BUILDDIR)/softHSM
	-mkdir $(BUILDDIR)/hsmbully

clean:
	rm -fr $(BUILDDIR)
