OK = output_knowngood
OUT = out

DOC = enforcer_rules

all: $(DOC).pdf

$(DOC).pdf: statemachine.pdf $(DOC).tex $(DOC).aux
	pdflatex $(DOC).tex

#~ This rule causes pdflatex to run twice to fix references
$(DOC).aux: $(DOC).tex
	pdflatex $(DOC).tex

statemachine.pdf: states.dot
	circo -Tpdf states.dot > statemachine.pdf

test: $(OUT)
	diff -q $(OK) $(OUT) && echo "Testrun OK" || diff -y $(OK) $(OUT)
$(OUT): prototype.py
	python prototype.py 2>/dev/null > $(OUT) || (rm $(OUT) && false)

realclean: clean
	rm -f *.pdf

clean:
	rm -f $(DOC).toc $(DOC).aux $(DOC).log $(OUT)

