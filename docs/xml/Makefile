# $Id$

TRANG=	 	trang.jar
SCHEMAS=	kasp.rng zonelist.rng signconf.rng


.SUFFIXES: .rnc .rng .xsd


all: $(SCHEMAS)

regress: $(SCHEMAS)
	xmllint --noout --relaxng kasp.rng kasp.xml
	xmllint --noout --relaxng zonelist.rng zonelist.xml
	xmllint --noout --relaxng signconf.rng signconf.xml

.rnc.rng:
	java -jar ${TRANG} $< $@
