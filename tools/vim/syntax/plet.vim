" Vim syntax file
" Language:     Plet
" Maintainer:   niels@nielssp.dk
" Last Change:  Mon Apr 12 18:25:12 CET 2021
" Filenames:    *.plet
"
if exists('b:current_syntax')
  finish
endif

if !exists('main_syntax')
  let main_syntax = 'plet'
endif

syn case match

syn match pletComment '#.*$' contains=@Spell
syn region pletComment start='{#' end='#}' contains=@Spell

syn match  pletSpecial contained "\\[\\bfnrt\'\"]\|\\[xX][0-9a-fA-F]\{2}\|\\u[0-9a-fA-F]\{4}\|\\U[0-9a-fA-F]\{8}"
syn region pletString  start=+'+ end=+'+ skip=+\\\\\|\\'+ contains=pletSpecial,@Spell

syn region pletCommand start='{' end='}' contains=ALLBUT

syn region pletTemplateString start=+"+ end=+"+ skip=+\\\\\|\\"+ contains=pletSpecial,pletCommand,@Spell

syn match pletNumber "\<\d\+\>"
syn match pletNumber  "\<\d\+\.\d*\%([eE][-+]\=\d\+\)\=\>"
syn match pletNumber  "\<\d\+[eE][-+]\=\d\+\>"

syn keyword pletKeyword if then else end for in switch case default do
syn keyword pletKeyword continue break return export
syn keyword pletOperator and or not
syn keyword pletConstant nil true false

hi def link pletComment Comment
hi def link pletString String
hi def link pletTemplateString String
hi def link pletNumber Number
hi def link pletKeyword Keyword
hi def link pletOperator Operator
hi def link pletConstant Constant
hi def link pletSpecial SpecialChar
hi def link pletCommand Statement

let b:current_syntax = 'plet'

if exists('main_syntax') && main_syntax == 'plet'
  unlet main_syntax
endif
