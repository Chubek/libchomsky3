" Vim syntax file for the Satie DSL / DIMACS.
if exists("b:current_syntax")
  finish
endif

" DIMACS comment lines
syn match satieComment "^c\>.*$"
syn match satieComment "#.*$"

" DIMACS problem line
syn match satieKeyword "^p\>.*$"

" negation
syn match satieNegate "[~!]"

" clause operators
syn match satieOperator "[&|]"
syn match satieOperator "\^"
syn match satieOperator "\bv\b"

" DIMACS terminator
syn match satieNumber "\<0\>"
syn match satieNumber "[0-9]\+"

" parentheses
syn match satieParen "[()]"

" identifiers (variables)
syn match satieVariable "[A-Za-z_][A-Za-z0-9_]*"

hi def link satieComment  Comment
hi def link satieKeyword  Keyword
hi def link satieNegate   Special
hi def link satieOperator Operator
hi def link satieNumber   Number
hi def link satieParen    Delimiter
hi def link satieVariable Identifier

let b:current_syntax = "satie"
