# $Id$

TARGETS = \
	softHSM \
	pkcs11-testing \
	hsmbully \
	OpenDNSSEC \
	OpenDNSSEC/auditor

all:

install:
	@for dir in $(TARGETS); do \
		target=../../$$dir/m4; \
		for macro in *.m4; do \
			if [ -f $$target/$$macro ]; then \
				echo "updating $$target/$$macro"; \
				cp $$macro $$target; \
			fi \
		done \
	done
