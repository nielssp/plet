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

#### Integers

The int type in Plet holds signed 64-bit integers.

```plet
12345
```

```plet
-25      # => -25   (unary minus)
12 + 56  # => 68    (integer addition)
14 - 5   # => 9     (integer subtraction)
10 * 3   # => 30    (integer multiplication)
7 / 3    # => 2     (integer division)
7 % 3    # => 1     (remainder)
```

#### Floating point numbers

The float type in Plet holds 64-bit double-precision floating point numbers.

```plet
123.45
123e4
123.5e-4
```

```plet
-25.5     # => -25.5 (unary minus)
1.2 + 5.6 # => 6.8   (floating point addition)
1.4 - 5.0 # => -3.6  (floating point subtraction)
2.5 * 2.0 # => 5.0   (floating point multiplication)
7.0 / 2.0 # => 2.5   (floating point division)
```

#### Time

```plet
time('2021-04-11T21:10:17Z')
```

#### Symbols

```plet
symbol('foo')
```

#### Strings

```plet
'Hello, World! \U0001F44D'
```

```plet
"two plus two is {2 + 2}"
```

```plet
"""Hello, "World"! \ and { and } are ignored."""
```

#### Arrays

```plet
[1, 2, 3]
```

#### Objects

```plet
{
  foo: 25,
  bar: 'Test',
}
```

#### Functions

```plet
print_something = () => info('something')
print_something()
increment = x => x + 1
info("1 + 1 is {1 | increment}")
plus = (a, b) => a + b
info("3 + 5 is {3 | plus(5)}")
```

### Variables

### Control flow

#### Blocks

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
  case 10
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
