all: enforcer_rules.pdf

enforcer_rules.pdf: statemachine.pdf enforcer_rules.tex enforcer_rules.aux
	pdflatex enforcer_rules.tex

#~ This rule causes pdflatex to run twice to fix references
enforcer_rules.aux: enforcer_rules.tex
	pdflatex enforcer_rules.tex

statemachine.pdf: states.dot
	circo -Tpdf states.dot > statemachine.pdf

realclean: clean
	rm -f *.pdf

clean:
	rm -f enforcer_rules.aux enforcer_rules.log

