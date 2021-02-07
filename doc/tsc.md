# TEXSTEP Template Compiler

## Token grammar

```
tokenStream ::= {text | comment | command}

command ::= commandStart {token | paren | bracket | brace | lf | comment | skip} commandEnd

commandStart = "{"  -- ignored
commandEnd = "}"  -- ignored

comment ::= "{" "#" {any \ "#" "}"} "#" "}"   -- ignored

lf ::= "\n"
skip ::= " " | "\t" | "\r"    -- ignored
skipLf ::= skip | lf         -- ignored

paren ::= "(" {token | paren | bracket | brace | comment | skipLf} ")"
bracket ::= "[" {token | paren | bracket | brace | comment |  skipLf} "]"
brace ::= "{" {token | paren | bracket | brace | comment | skipLf} "}"

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
          | "fn" | "do" | "and" | "or" | "not"

operator ::= "." | "," | ":" | "->"
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
            | Assignment

If ::= "if" Expression Block
       {"else" "if" Expression Block}
       ["else" Block] "end" "if"

For ::= "for" name [":" name] "in" Expression Block
       ["else" Block] "end" "for"

Switch ::= "switch" Expression (lf | {lf} [text])
           {"case" Expression Block}
           ["default" Block] "end" "switch"

Assignment ::= Expression [("=" | "+=" | "-=" | "*=" | "/=") Expression]

Expression ::= "fn" [name {"," name} [","]] "->" Expression
             | "." name {"." name}
             | PipeLine

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
           | ApplyDot "." name
           | ApplyDot "[" Expression "]"
           | Atom

Key ::= int
      | float
      | string
      | name

Atom ::= "[" [Expression {"," Expression} [","]] "]"
       | "(" Expression ")"
       | "{" [Key ":" Expression {"," Key ":" Expression} [","]] "}"
       | '"' Template '"'
       | "do" Block "end" "do"
       | int
       | float
       | string
       | name
```

### Syntax transformations

```
"." name_1 {"." name_n}
=>
"fn" newName -> newName "." name_1 {"." name_n}
```

```
PipeLine "|" name ["(" [Expression_1 {"," Expression_n} [","]] ")"]
=>
name "(" PipeLine ["," Expression_1 {"," Expression_n}] ")"
```

## Types
nil
true
int
float
string
array
object
time

any

### Boolean

Falsy valus: nil, 0, 0.0, [], {}, ''

## Modules

### core
```
nil: nil
false: nil
true: true
import(name: string): nil
type(val: any): string
string(val: any): string
```

### strings
```
lower(str: string): string
upper(str: string): string
starts_with(str: string, prefix: string): nil|true
ends_with(str: string, suffix: string): nil|true
json(var: any): string
```

### collections

```
length(collection: array|object|string): int
keys(obj: object): array
values(obj: object): array
map(collection: array|object, f: func): array
map_keys(obj: object, f: func): object
filter(collection: array|object, predicate: func): array
exclude(collection: array|object, predicate: func): array
sort(array: array): array
sort_with(array: array, comparator: func): array
sort_by(array: array, f: func): array
sort_by_desc(array: array, f: func): array
group_by(array: array, f: func): array
take(array: array, n: int): array
drop(array: array, n: int): array
pop(array: array): any
push(array: array, element: any): array
push_all(array: array, elements: array): array
shift(array: array): any
unshift(array: array, element: any): array
delete(obj: object, key: string): obj
```

### time

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
is_current(link: string?): nil|true
read(file: string): string
page_list(n: int, page: int? = PAGE.page, pages: int? = PAGE.pages): array
page_link(page: int, path: string? = PAGE.path): string
filter_content(content: object, filters: array?): string
```

### html
```
h(str: string): string
href(link: string?): string
```

### sitemap
```
add_static(path: string): nil
add_page(path: string, template: string, data: object?): nil
paginate(items: array, per_page: int, path: string, template: string, data: object?): nil
```

### contentmap

```
list_content(path: string, options: {recursive: boolean}?): array
save_content(content: object): nil
```
