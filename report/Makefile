NAME = document

TEX = $(NAME).tex
SRC = $(TEX)
PDF = $(TEX:.tex=.pdf)

TEXMK = latexmk -shell-escape -lualatex -interaction=nonstopmode
QPDF  = qpdf --linearize --replace-input

all: $(PDF)

$(PDF): %.pdf: %.tex
	$(TEXMK) $<
	@$(QPDF) $@ 2>/dev/null |:

EXTS = aux fdb_latexmk fls log nav out snm synctex.gz toc
clean:
	rm -rf $(PDF) _minted-$(NAME)/ $(foreach ext,$(EXTS),$(NAME).$(ext))
