# $Id$

# fetch trang from http://code.google.com/p/jing-trang/
TRANG=	 	java -jar trang.jar

# xmllint is part of Libxml2, http://www.xmlsoft.org/
XMLLINT=	xmllint


RNG=		kasp.rng zonelist.rng signconf.rng conf.rng
XSD=		kasp.xsd zonelist.xsd signconf.xsd conf.xsd



.SUFFIXES: .rnc .rng .xsd

all: rng regress

clean:
	rm -f $(RNG) $(XSD)

rng: $(RNG)

xsd: $(XSD)

regress: $(RNG)
	${XMLLINT} --noout --relaxng kasp.rng kasp.xml
	${XMLLINT} --noout --relaxng zonelist.rng zonelist.xml
	${XMLLINT} --noout --relaxng signconf.rng signconf.xml
	${XMLLINT} --noout --relaxng conf.rng conf.xml

.rnc.rng:
	${TRANG} $< $@

.rnc.xsd:
	${TRANG} $< $@
