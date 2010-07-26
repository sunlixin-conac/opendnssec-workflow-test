# $Id$

TARGETS = \
	softHSM \
	hsmbully \
	OpenDNSSEC \
	OpenDNSSEC/auditor \
	OpenDNSSEC/plugins/eppclient

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
