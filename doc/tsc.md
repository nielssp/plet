# TEXSTEP Template Compiler

## Token grammar

```
tokenStream ::= {text | comment | command}

command ::= commandStart {commandToken | lf | skip} commandEnd

commandToken = token | paren | bracket | brace | comment | commentSingle

commandStart = "{"  -- ignored
commandEnd = "}"  -- ignored

comment ::= "{#" {any \ "#}"} "#}"   -- ignored

commentSingle ::= '#' {any \ lf}   -- ignored

lf ::= "\n"
skip ::= " " | "\t" | "\r"    -- ignored

paren ::= "(" {commandToken | lf | skip} ")"
bracket ::= "[" {commandToken | lf |  skip} "]"
brace ::= "{" {commandToken | lf | skip} "}"

quote ::= '"' {quoteText | command} '"'

text ::= {any \ (commandStart | commandEnd)}
quoteText ::= {any \ (commandStart | commandEnd | '"') | "\\" (commandStart | commandEnd | escape)}

token ::= keyword
        | operator
        | int
        | float
        | string
        | verbatim
        | name

keyword ::= "if" | "else" | "end" | "for" | "in" | "switch" | "case" | "default"
          | "do" | "and" | "or" | "not" | "export" | "return" | "break" | "continue"

operator ::= "." | "," | ":" | "=>"
           | "==" | "!=" | "<=" | ">=" | "<" | ">"
           | "=" | "+=" | "-=" | "*=" | "/="
           | "+" | "-" | "*" | "/" | "%"

name ::= (nameStart {nameFollow}) \ keyword

nameStart ::= "a" | ... | "z"
            | "A" | ... | "z"
            | "_"

nameFollow ::= nameStart | digit

digit ::= "0" | ... | "9"

hex ::= digit
      | "a" | ... | "f"
      | "A" | ... | "F"
hex2 ::= hex hex
hex4 ::= hex2 hex2
hex8 ::= hex4 hex4

int ::= digit {digit}

exponent ::= ("e" | "E") ["-" | "+"] int

float ::= int ["." int] exponent

escape ::= '"'
         | "'"
         | '\\'
         | '/'
         | 'b'
         | 'f'
         | 'n'
         | 'r'
         | 't'
         | 'x' hex2
         | 'u' hex4
         | 'U' hex8

string ::= "'" {(any / ("\\" | "'")) | '\\' escape} "'"

verbatim ::= '"""'  {any / '"""'} '"""'
```

## Syntax

```
Template ::= {Statements | text}

Block ::= (lf | text) Template (lf | text)
        | text

Statements ::= {lf} [Statement {lf {lf} Statement}] {lf}

Statement ::= If
            | For
            | Switch
            | Export
            | "return" [Expression]
            | "break" [int]
            | "continue" [int]
            | Assignment

If ::= "if" Expression Block
       {"else" "if" Expression Block}
       ["else" Block] "end" "if"
     | "if" Expression "then" Expression "else" Statement

For ::= "for" name [":" name] "in" Expression Block
       ["else" Block] "end" "for"

Switch ::= "switch" Expression (lf | {lf} [text])
           {"case" Expression Block}
           ["default" Block] "end" "switch"

Assignment ::= Expression [("=" | "+=" | "-=" | "*=" | "/=") Expression]

Export ::= "export" name ["=" Expression]

Expression ::= "." name {"." name}
             | FatArrow

FatArrow ::= Tuple "=>" Statement
           | PipeLine

Tuple ::= name
        | "(" [name {"," name} [","]] ")"

PipeLine ::= PipeLine "|" name ["(" [Expression {"," Expression} [","]] ")"]
           | LogicalOr

LogicalOr ::= LogicalOr "or" LogicalAnd
            | LogicalAnd

LogicalAnd ::= Logical "and" LogicalNot
             | LogicalNot

LogicalNot ::= "not" LogicalNot
             | Comparison

Comparison ::= Comparison ("<" | ">" | "<=" | ">=" | "==" | "!=") MulDiv
             | MulDiv

MulDiv ::= MulDiv ("*" | "/" | "%") AddSub
         | AddSub

AddSub ::= AddSub ("+" | "-") Negate
         | Negate

Negate ::= "-" Negate
         | ApplyDot

ApplyDot ::= ApplyDot "(" [Expression {"," Expression} [","]] ")"
           | ApplyDot "." name ["?"] 
           | ApplyDot "[" Expression "]" ["?"] 
           | Atom

Key ::= int
      | float
      | string
      | name

Atom ::= "[" [Expression {"," Expression} [","]] "]"   -- ignore lf
       | "(" Expression ")"   -- ignore lf
       | "{" [Key ":" Expression {"," Key ":" Expression} [","]] "}"   -- ignore lf
       | '"' Template '"'
       | "do" Block "end" "do"   -- don't ignore lf
       | int
       | float
       | string
       | name ["?"]
```

### Syntax transformations

```
"." name_1 {"." name_n}
=>
newName => newName "." name_1 {"." name_n}
```

```
PipeLine "|" name ["(" [Expression_1 {"," Expression_n} [","]] ")"]
=>
name "(" PipeLine ["," Expression_1 {"," Expression_n}] ")"
```

## Types
nil
true
false
int
float
string
array
object
time
function
symbol

any

### Boolean

Falsy valus: nil, false, 0, 0.0, [], {}, ''

## Modules

### core
```
nil: nil
false: false
true: true
import(name: string): nil
copy(val: any): any
type(val: any): string
string(val: any): string
bool(val: any): true|false
error(message: string): nil
warning(message: string): nil
info(message: string): nil
```

### strings
```
lower(str: string): string
upper(str: string): string
title(str: string): string
starts_with(str: string, prefix: string): true|false
ends_with(str: string, suffix: string): true|false
symbol(str: string): symbol
json(var: any): string
```

### collections

```
length(collection: array|object|string): int
keys(obj: object): array
values(obj: object): array
map(collection: array|object, f: func): array|object
map_keys(obj: object, f: func): object
flat_map(collection: array, f: func): array
filter(collection: array|object, predicate: func): array
exclude(collection: array|object, predicate: func): array
sort(array: array): array
sort_with(array: array, comparator: func): array
sort_by(array: array, f: func): array
sort_by_desc(array: array, f: func): array
group_by(array: array, f: func): array
take(array: array|string, n: int): array|string
drop(array: array|string, n: int): array|string
pop(array: array): any
push(array: array, element: any): array
push_all(array: array, elements: array): array
shift(array: array): any
unshift(array: array, element: any): array
contains(obj: array|object, key: any): true|false
delete(obj: object, key: any): true|false
```

### datetime

```
now(): time
time(time: time|string|int): time
date(time: time|string|int, format: string): string
iso8601(time: time|string|int): string
rfc2822(time: time|string|int): string
```

### template
```
embed(name: string, data: object?): string
link(link: string?): string
url(link: string?): string
is_current(link: string?): true|false
read(file: string): string
page_list(n: int, page: int? = PAGE.page, pages: int? = PAGE.pages): array
page_link(page: int, path: string? = PAGE.path): string
filter_content(content: object, filters: array?): string
```

### html
```
h(str: string): string
href(link: string?, class: string?): string
html(node: html_node): string
no_title(node: html_node): html_node
links(node: html_node): html_node
urls(node: html_node): html_node
read_more(node: html_node): html_node
text_content(node: html_node): string
parse_html(src: string): html_node
```

### sitemap
```
add_static(path: string): nil
add_reverse(content_path: string, path: string): nil
add_page(path: string, template: string, data: object?): nil
paginate(items: array, per_page: int, path: string, template: string, data: object?): nil
```

### contentmap

```
list_content(path: string, options: {recursive: boolean, suffix: string}?): array
read_content(path: string): object
```

## exec
```
shell_escape(value: any): string
exec(command: string, ... args: any): string
```
