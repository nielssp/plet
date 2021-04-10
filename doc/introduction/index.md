## Introduction

Plet is a flexible static site generator, web framework, and programming language.
Static websites (or [static web pages](https://en.wikipedia.org/wiki/Static_web_page)) are collections of HTML files and assets that are served exactly as they are stored on the web server. This provides better security, performance, and stability.

```html+plet
{# This is a Plet template #}
{for post in posts}
<h2>{post.title | h}</h2>
<div>{post.content}</div>
{else}
<p>There are no posts.</p>
{end for}

{if x == nil then 5 else "test {x + 5}"}
```

```plet
# This is a Plet entry script
export CONFIG = {
  title: 'My website',
}

export ROOT_PATH = '/'
export ROOT_URL = 'http://example.com'

add_static('favicon.ico')

add_page('404.html', 'templates/404.tss.html')

pages = list_content('pages', {suffix: '.md'})
for page in pages
  if page.name == 'index'
    page.link = "{page.relative_path}/index.html"
  else
    page.link = "{page.relative_path}/{page.name}/index.html"
  end if
  add_page(page.link, 'templates/page.tss.html', {
    page: page
  })
  add_reverse(page.path, page.link)
end for

"hello {"te{if x then 2 else 3}st" | foo} world"
```
