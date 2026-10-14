# .latexmkrc — build configuration for "Jiterati: A Vademecum"
#
# Engine: XeLaTeX (Unicode-aware, good font handling for a textbook).
# Invoke latexmk from this directory; generated artifacts land in ./build/.

# --- Engine selection -------------------------------------------------------
# pdf_mode = 5  =>  run xelatex (latexmk's documented convention).
$pdf_mode = 5;
$xelatex = 'xelatex -file-line-error -synctex=1 -interaction=nonstopmode %O %S';

# --- Auxiliary tools --------------------------------------------------------
# The vademecum is self-contained (inline footnotes rather than a .bib),
# but biber is wired in case a bibliography is added later.
$bibtex_use = 2;                 # 2 = run bibtex/biber only when needed
$biber = 'biber %O %S';
$makeindex = 'makeindex -q %O -o %D';

# --- Paths ------------------------------------------------------------------
# Keep generated files out of the source tree.
$outdir = 'build';
$aux_dir = $outdir;
$do_cd = 0;                      # do not chdir; keep paths relative to here

# --- The document -----------------------------------------------------------
@default_files = ('Jiterati-Vademcum.tex');

# --- Failure handling -------------------------------------------------------
# Stop after a bounded number of repeats on error rather than looping forever.
$max_repeat = 5;

# --- Clean ------------------------------------------------------------------
# Extra extensions removed by `latexmk -c` (keep .pdf) / `-C` (also .pdf).
$clean_ext = 'synctex.gz run.xml bbl bcf fdb_latexmk fls log aux out toc '
           . 'lof lot idx ind ilg nav snm vrb xdv';

# --- chktex integration -----------------------------------------------------
# Compilation and linting are kept as independent, inspectable steps.
# `make check` (and build.sh) call chktex directly on the source file,
# per the project requirement to run `chktex Jiterati-Vademcum.tex`.

1;   # a .latexmkrc must return true
