## --- General Options --- ##

project = "SerdeTk"
author = "Chubak Bidpaa <chubakbidpaa@riseup.net>"
copyright = "2026, Unlicense"

version = "1.0"
release = "1.0.0"

source_suffix = ".rst"

language = "en"

extensions = []

master_doc = "index"
root_doc = "index"

## ---- HTML Options ----- ##

html_theme = "alabaster"
html_title = "SerdeTk Manual"
html_short_title = "SerdeTkMan"

html_css_files = [
    "_static/stkman.css"
]

html_js_files = [
    "_static/stkman.js"
]

## --- LaTeX Options --- ##

latex_engine = 'lualatex'
latex_use_xindy = True

latex_elements = {
    "papersize": "a4paper",
    "pointsize": "11pt"
}

latex_documents = [
    (
        "index",
        "SerdeTk.tex",
        "SerdeTk Manual",
        author,
        "manual",
    ),
]

## --- Syntax Options --- ##

highlight_language = "cpp"
pyments_style = "sphinx"
pygments_dark_style = "monoki"

## --- Search Options --- ##

html_search_language = 'en'

## --- Misc --- ##

primary_domain = "cpp"
default_role = "any"


