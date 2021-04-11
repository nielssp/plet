## Templates

Templates are written using the Plet programming language.

### Values

#### Primitives

```plet
12345    # int
123.45   # float
123e4    # float
123.5e-4 # float
```

#### Strings

```plet
'Hello, World! \U0001F44D'
```

```plet
"two plus two is {2 + 2}"
```

```plet
"""Hello, "World"!"""
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
