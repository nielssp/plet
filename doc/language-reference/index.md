## Language reference

### Lexical structure

```
tokenStream   ::= [bom] {text | comment | command}

bom           ::= "\xEF\xBB\xBF"     -- ignored

command       ::= commandStart {commandToken | lf | skip} commandEnd

commandToken  ::= token | paren | bracket | brace | comment | commentSingle

commandStart  ::= "{"  -- ignored
commandEnd    ::= "}"  -- ignored

comment       ::= "{#" {any - "#}"} "#}"   -- ignored

commentSingle ::= '#' {any - lf}   -- ignored

lf            ::= "\n"
skip          ::= " " | "\t" | "\r"    -- ignored

paren         ::= "(" {commandToken | lf | skip} ")"
bracket       ::= "[" {commandToken | lf | skip} "]"
brace         ::= "{" {commandToken | lf | skip} "}"

quote         ::= '"' {quoteText | command} '"'

text          ::= {any - (commandStart | commandEnd)}
quoteChar     ::= any - (commandStart | commandEnd | '"')
                | "\\" (commandStart | commandEnd | escape)
quoteText     ::= {quoteChar}

token         ::= keyword
                | operator
                | int
                | float
                | string
                | verbatim
                | name

keyword       ::= "if" | "then" | "else" | "end" | "for" | "in" | "switch" | "case" | "default"
                | "do" | "and" | "or" | "not" | "export" | "return" | "break" | "continue"

operator      ::= "." | "," | ":" | "=>"
                | "==" | "!=" | "<=" | ">=" | "<" | ">"
                | "=" | "+=" | "-=" | "*=" | "/="
                | "+" | "-" | "*" | "/" | "%"

name          ::= (nameStart {nameFollow}) - keyword

nameStart     ::= "a" | ... | "z"
                | "A" | ... | "z"
                | "_"
                | "\x80" | ... | "\xFF"

nameFollow    ::= nameStart | digit

digit         ::= "0" | ... | "9"

hex           ::= digit
                | "a" | ... | "f"
                | "A" | ... | "F"
hex2          ::= hex hex
hex4          ::= hex2 hex2
hex8          ::= hex4 hex4

int           ::= digit {digit}

exponent      ::= ("e" | "E") ["-" | "+"] int

float         ::= int ["." int] exponent

escape        ::= '"'
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

string        ::= "'" {(any - ("\\" | "'")) | '\\' escape} "'"

verbatim      ::= '"""'  {any - '"""'} '"""'
```

### Syntax

```
Template      ::= {Statements | text}

Block         ::= (lf | text) Template (lf | text)
                | text

Statements    ::= {lf} [Statement {lf {lf} Statement}] {lf}

Statement     ::= If
                | For
                | Switch
                | Export
                | "return" [Expression]
                | "break" [int]
                | "continue" [int]
                | Assignment

If            ::= "if" Expression Block
                  {"else" "if" Expression Block}
                  ["else" Block] "end" "if"
                | "if" Expression "then" Expression "else" Statement

For           ::= "for" name [":" name] "in" Expression Block
                 ["else" Block] "end" "for"

Switch        ::= "switch" Expression (lf | {lf} [text])
                  {"case" Expression Block}
                  ["default" Block] "end" "switch"

Assignment    ::= Expression [("=" | "+=" | "-=" | "*=" | "/=") Expression]

Export        ::= "export" name ["=" Expression]

Expression    ::= "." name {"." name}
                | FatArrow

FatArrow      ::= Tuple "=>" Statement
                | PipeLine

Tuple         ::= name
                | "(" [name {"," name} [","]] ")"

PipeLine      ::= PipeLine "|" name ["(" [Expression {"," Expression} [","]] ")"]
                | LogicalOr

LogicalOr     ::= LogicalOr "or" LogicalAnd
                | LogicalAnd

LogicalAnd    ::= Logical "and" LogicalNot
                | LogicalNot

LogicalNot    ::= "not" LogicalNot
                | Comparison

Comparison    ::= Comparison ("<" | ">" | "<=" | ">=" | "==" | "!=") MulDiv
                | MulDiv

MulDiv        ::= MulDiv ("*" | "/" | "%") AddSub
                | AddSub

AddSub        ::= AddSub ("+" | "-") Negate
                | Negate

Negate        ::= "-" Negate
                | ApplyDot

ApplyDot      ::= ApplyDot "(" [Expression {"," Expression} [","]] ")"
                | ApplyDot "." name ["?"] 
                | ApplyDot "[" Expression "]" ["?"] 
                | Atom

Key           ::= int
                | float
                | string
                | name

Atom          ::= "[" [Expression {"," Expression} [","]] "]"   -- ignore lf
                | "(" Expression ")"   -- ignore lf
                | "{" [Key ":" Expression {"," Key ":" Expression} [","]] "}"   -- ignore lf
                | '"' Template '"'
                | "do" Block "end" "do"   -- don't ignore lf
                | int
                | float
                | string
                | name ["?"]
```
