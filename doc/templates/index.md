## Templates and scripts

Templates are written using the Plet programming language. Plet is a high-level dynamically-typed imperative programming language. There are two types of Plet programs: *Plet scripts* and *Plet templates*. Plet scripts start out in *command mode* while Plet templates start out in *text mode*.

In text mode the only byte that has any special meaning is `{` (left curly bracket, U+007B) which enters command mode, or &ndash; if it's immediately followed by a `#` (U+0023) &ndash; enters *comment mode*. Comment mode can also be entered from within command mode with the same `{#` sequence. In comment mode the `#}` sequence exits to the most recent mode.

In command mode an unmatched `}` (right curly bracket, U+007D) will enter template mode. This means that any Plet template can be made into a Plet script by prepending a `}` and any Plet script can be made into a Plet template by prepending a `{`:

```txt+plet
this is a valid Plet template
```

```plet
}this is a valid Plet script
```

In command mode a `#` that is not preceded by a `{` opens a single line comment:

```plet
# this is a single line comment
command()
{# this is a 
   multiline comment #}
command()
```

A Plet program is a sequence of text nodes and statements. A text node is a string of bytes consumed while in text mode, it may be empty. Two statements must be separated by at least one text node or newline (U+000A).
```plet
command()
command()
```

```txt+plet
{command()}{command()}
```

Some statement can contain multiple nested statements and text nodes. The *top-level* text nodes and statements are not contained within another statement.

The *return value* of a Plet program &ndash; unless otherwise specified using the `return` keyword &ndash; is the byte string resulting from concatenating all top-level text nodes with the *values* resulting from evaluating all top-level statements.

### Values

All expressions and statements in Plet evaluate to a value. A value has a type. Plet has 10 built-in types:

* nil
* bool
* int
* float
* time
* symbol
* string
* array
* object
* function

#### Nil

The nil type has only one value:

```plet
nil
```

It is used to represent the absence of data, e.g. as a return value from a function called for its side effects.

#### Booleans

The bool type has two values:

```plet
true
false
```

Plet has three boolean operators:

```plet
a or b
a and b
not a
```

#### Integers

The int type in Plet holds signed 64-bit integers.

```plet
12345
```

The following operators work on ints:

```plet
-25      # => -25   (unary minus)
12 + 56  # => 68    (integer addition)
14 - 5   # => 9     (integer subtraction)
10 * 3   # => 30    (integer multiplication)
7 / 3    # => 2     (integer division)
7 % 3    # => 1     (remainder)
7 < 3    # => false (less than)
7 > 3    # => true  (greater than)
4 <= 4   # => true  (less than or equal to)
4 >= 5   # => false (greater than or equal to)
```

#### Floating point numbers

The float type in Plet holds 64-bit double-precision floating point numbers.

```plet
123.45
123e4
123.5e-4
```

The following operators work on floats:

```plet
-25.5     # => -25.5 (unary minus)
1.2 + 5.6 # => 6.8   (floating point addition)
1.4 - 5.0 # => -3.6  (floating point subtraction)
2.5 * 2.0 # => 5.0   (floating point multiplication)
7.0 / 2.0 # => 2.5   (floating point division)
```

The binary operators above accept both int and float operands. If either operand is a float then the result is always a float:

```plet
5 / 2   # => 2
5 / 2.0 # => 2.5
5.0 / 2 # => 2.5
```

#### Time

```plet
time('2021-04-11T21:10:17Z')
```

#### Symbols

Symbols are [interned strings](https://en.wikipedia.org/wiki/String_interning) which means that they are faster to compare than regular strings.

```plet
symbol('foo')
```

#### Strings

Plet strings are arrays of bytes. There are three types of string literals:

```plet
# Single quote strings (no interpolation)
'Hello, World! \U0001F44D'
# Double quote string (interpolation)
"two plus two is {2 + 2}"
# Verbatim string (no interpolation or escape sequences)
"""Hello, "World"! \ and { and } are ignored."""
```

Single quote and double quote strings both support the following escape sequences:

* `\'` &ndash; single quotation mark
* `\"` &ndash; double quotation mark
* `\\` &ndash; backslash
* `\/` &ndash; forward slash
* `\{` &ndash; left curly bracket (only in double quote strings)
* `\}` &ndash; right curly bracket (only in double quote strings)
* `\b` &ndash; backspace
* `\f` &ndash; formfeed
* `\n` &ndash; newline
* `\r` &ndash; carriage return
* `\t` &ndash; horizontal tab
* `\xhh` &ndash; byte value given by hexadecimal number `hh`
* `\uhhhh` &ndash; Unicode code point given by hexadecimal number `hhhh`
* `\Uhhhhhhhh` &ndash; Unicode code point given by hexadecimal number `hhhhhhhh`

Unicode code points (specified using `\u` or `\U`) higher than U+007F are encoded using UTF-8.

Double quote strings additionally support string interpolation with full Plet template support:

```plet
"foo {if x > 0}bar{else}baz{end if}"
```

The `length` of a string is always its byte length:

```plet
length('\U0001F44D') # => 4
```

#### Arrays

Plet arrays are dynamically typed mutable 0-indexed sequences of values:

```plet
a = [1, 'foo', false]
a[1]      # => 'foo'
length(a) # => 3
```

Trailing commas are allowed:

```plet
[
  'foo',
  'bar',
  'baz',
]
```

Items can be added to the end of an array using the `push` function:

```plet
a = []
a | push('foo')
a | push('bar')
a[0]      # => 'foo'
a[0] = 'baz'
a[0]      # => 'baz'
length(a) # => 2
```

#### Objects

Plet objects are dynamically typed mutable sets of key-value pairs:

```plet
obj = {
  foo: 25,
  bar: 'Test',
}
obj.foo     # => 25
length(obj) # => 2
```

The keys of an object can be of any type. Entries can be added and replaced usng the assignment operator:

```plet
obj = {}
obj.a = 'foo'
obj.b = 'bar'
obj.a = 'baz'
obj.a       # => 'baz'
length(obj) # => 2
```

The dot-operator can only be used to access entries when the keys are symbols. For other types, the array access operator can be used:

```plet
obj = {
  'a': 'foo', # string key
  15: 'bar',  # int key
  sym: 'baz'  # symbol key
}
obj['a']           # => 'foo'
obj[15]            # => 'bar'
obj[symbol('sym')] # => 'baz'
obj.sym            # => 'baz'
```

#### Functions

Functions in Plet are created using the `=>` operator (&ldquo;fat arrow&rdquo;). The left side of the arrow is a tuple specifying the names of the parameters that the function accepts. The right side is the body of the function (a single expression).

```plet
# a function that accepts no parameters and always returns nil:
() => nil
# identity function that returns whatever value is passed to it:
(x) => x
# parentheses are optional when there's exactly one parameter:
x => x
# a function that accepts two parameters and returns the result of adding them together:
(x, y) => x + y
```

Functions can be assigned to variables with the assignment operator:

```plet
foo = () => 15
bar = x => X + 1
baz = (x, y, z) => x + y + z
```

There are two styles of function application. The first is prefix notation:

```plet
foo() # Parentheses are required
bar(5) 
baz(2, 4, 6)
```

The second style is infix notation using the pipe operator where the first parameter is written before the function name (the function must have at least one argument):

```plet
5 | bar()
5 | bar    # With only one parameter the parentheses are optional
2 | baz(4, 6)
```

The second style can be used to chain several function calls without nested pairs of parentheses. The following two lines are equivalent:

```plet
foo() | bar | baz(1, 2) | baz(3, 4)
baz(baz(bar(foo()), 1, 2), 3, 4)
```

Functions in Plet are [first-class citizens](https://en.wikipedia.org/wiki/First-class_function) meaning they can be passed as arguments to other functions. A *higher-order function* is a function that takes another function as an argument.

```plet
func1 = x => x + 1
func2 = (f, operand) => f(operand)
func2(func1, 5)      # => 6
func2(x => x + 2, 5) # => 7
```

Plet has several built-in higher-order functions. One example is `map` which applies a function to all elements of an array and returns the resulting array (withour modifying the existing array):

```plet
[1, 2, 3, 4] | map(x => x * 2)
# => [2, 4, 6, 8]
```

### Variables and scope

`=` is the assignment operator.

### Control flow

#### Blocks

```plet
do
  foo()
  bar()
end do
```

#### Conditional

```plet
if x > 5
  info('x is greater than 5')
else if x < 5
  info('x is less than 5')
else
  info('x is equal to 5')
end if
```

```html+plet
{if x > 5}
x is greater than 5
{else if x < 5}
x is less than 5
{else}
x is equal to 5
{end if}
```

```html+plet
{if x > y then 'x' else 'y'}
is greater than
{if x <= y then 'x' else 'y'}
```

##### Falsiness

In conditionals the following values are considered &ldquo;falsy&rdquo;:

```plet
nil
false
0
0.0
[]    # the empty array
{}    # the empty object
''    # the empty string
```

So instead of writing `if length(my_array) > 0` we can write `if my_array`.

#### Switch

```plet
switch x
  case 5
    info('x is 5') 
  case 6
    info('x is 6') 
  default
    info('x is not 5 or 6') 
end switch 
```

```html+plet
{switch x}
  {case 5} x is 5
  {case 6} x is 6
  {default} x is not 5 or 6
{end switch}
```

#### Loops

```plet
for item in items
  info("item: {item}")
else
  info('array is empty')
end for
```

```plet
for i: item in items
  info("{i}: {item}")
end for
```

```plet
for key: value in obj
  info("{key}: {value}")
end for
```

### Modules
