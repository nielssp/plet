## Module reference

The Plet standard library is split into several modules.

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

### exec
```
shell_escape(value: any): string
exec(command: string, ... args: any): string
```
