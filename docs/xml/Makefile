# $Id$

# fetch trang from http://code.google.com/p/jing-trang/
TRANG=	 	java -jar trang.jar

# xmllint is part of Libxml2, http://www.xmlsoft.org/
XMLLINT=	xmllint


SCHEMAS=	kasp.rng zonelist.rng signconf.rng


.SUFFIXES: .rnc .rng .xsd


all: $(SCHEMAS)

regress: $(SCHEMAS)
	${XMLLINT} --noout --relaxng kasp.rng kasp.xml
	${XMLLINT} --noout --relaxng zonelist.rng zonelist.xml
	${XMLLINT} --noout --relaxng signconf.rng signconf.xml

.rnc.rng:
	${TRANG} $< $@
