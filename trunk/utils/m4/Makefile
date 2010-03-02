# $Id$

TARGETS = \
	softHSM \
	hsmbully \
	OpenDNSSEC \
	OpenDNSSEC/conf \
	OpenDNSSEC/libhsm \
	OpenDNSSEC/enforcer \
	OpenDNSSEC/signer \
	OpenDNSSEC/auditor \
	OpenDNSSEC/xml \
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
