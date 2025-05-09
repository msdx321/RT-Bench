.PHONY: setup clean all

CURRENT_BRANCH ?= $(shell git rev-parse --abbrev-ref HEAD)
BRANCH_FOLDER =$(subst +,plus,$(subst /,-,$(CURRENT_BRANCH)))

all: html

pdf: clean-pdf
	@mkdir -p pdf/attachments
	@cp -r source/attachments pdf/
	@cd conf && sed 's/GENERATE_LATEX\s*=\s*NO/GENERATE_LATEX=YES/' Doxyfile | doxygen -
	@make -C latex
	@mv latex/refman.pdf pdf/Documentation.pdf
	@rm -r latex


print: clean-pdf
	@sed -i 's/\\documentclass\[oneside\]/\\documentclass\[twoside\]/' conf/latex-header.tex
	@cd conf && sed -e 's/GENERATE_LATEX\s*=\s*NO/GENERATE_LATEX=YES/' -e 's|PROJECT_NUMBER\s*=.*|PROJECT_NUMBER=$(CURRENT_BRANCH)|' Doxyfile | doxygen -
	@sed -i 's/\\documentclass\[twoside\]/\\documentclass\[oneside\]/' conf/latex-header.tex
	@mkdir -p pdf/attachments
	@cp -r source/attachments pdf/
	@make -C latex
	@mv latex/refman.pdf pdf/Documentation.pdf
	@rm -r latex

html: setup
	@# copy the pdf folder into the html documentation, so that i will be included in the commit & push
	@mkdir -p html/$(BRANCH_FOLDER)/attachments
	@cp -r source/attachments html/$(BRANCH_FOLDER)/attachments
	@cd conf && sed -e 's/GENERATE_HTML\s*=\s*NO/GENERATE_HTML=YES/' -e 's|HTML_OUTPUT\s*=.*|HTML_OUTPUT=html\/$(BRANCH_FOLDER)|' -e 's|PROJECT_NUMBER\s*=.*|PROJECT_NUMBER=$(CURRENT_BRANCH)|' Doxyfile | doxygen -
	@cd html && touch .nojekyll
	@sed -i -e '/^.dropdown-content {/a z-index: 9999;' -e '/^.dropdown-content {/a overflow: auto;' -e '/^.dropdown-content {/a max-height: 500px;' html/$(BRANCH_FOLDER)/dropdown.css
	@sed -i -e 's/\(z-index:\)9999/\19998/' html/$(BRANCH_FOLDER)/tabs.css
ifeq ("$(wildcard Documentation.html)","")
	@ln -s html/$(BRANCH_FOLDER)/index.html Documentation.html
endif

setup:
ifeq ("$(wildcard conf/doxygen-awesome/doxygen-awesome.css)", "")
	@git submodule update --init --recursive conf/doxygen-awesome
endif
ifeq ("$(wildcard conf/doxyvers/selectversion.js)", "")
	@git submodule update --init --recursive conf/doxyvers
endif
	@sed -i -e 's/\(sort()\)\.reverse()/\1/' -e 's/let\surlroot\s=.*/let urlroot = "\/rt-bench\/branch"/' -e 's/let\smaster\s=.*/let master = "main"/' -e "s/let\sthisvers\s=.*/let thisvers=\"$(BRANCH_FOLDER)\"\;/" conf/doxyvers/selectversion.js

clean: clean-pdf clean-html

clean-html:
	@echo removing generated documentation pages...
	@rm -rf html Documentation.html

clean-pdf:
	@rm -rf latex pdf