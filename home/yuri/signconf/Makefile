DOCS = signconf.pdf
DOTS = tree.pdf

ALL: $(DOCS)

$(DOCS): %.pdf: %.tex $(DOTS) $<
	pdflatex $< && pdflatex $<

$(DOTS): %.pdf: %.dot
	dot -Tpdf $< -o $@
	#~ circo -Tpdf $< -o $@

clean:
	rm -f *.toc *.aux *.log $(DOTS) $(DOCS)

.PHONY: ALL
