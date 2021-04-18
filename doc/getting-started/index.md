## Getting started

### Installation

#### Building from source

```sh
make clean all
```

##### Build options

The available build options are:

* `UNICODE` &ndash; Enable Unicode support (requires ICU).
* `GUMBO` &ndash; Enable support for HTML manipulation (requires Gumbo).
* `IMAGEMAGICK` &ndash; Enable support for automatic resizing and conversion of images (requires ImageMagick 7).
* `MUSL` &ndash; Enable compatibility with musl libc.
* `STATIC_MD4C` &ndash; Build with md4c source in lib instead of dynamiccaly linking with md4c.

By default, the following options are enabled:

```sh
make UNICODE=1 GUMBO=1 IMAGEMAGICK=1 STATIC_MD4C=0 MUSL=0 all
```

### Basic usage

In this section we will create a basic blog consisting of a sorted list of blog posts, and a page for each blog post.

First create a new empty directory, then open a terminal in that directory and type the following:

```sh
plet init
```

This will create an empty `index.plet` file in the directory. The directory containing `index.plet` will henceforth be referred to as the *source root* (`SRC_ROOT` in code).

`index.plet` is the entry script that Plet evaluates whenever you run `plet build`. Since the script is currently empty, the only thing that happens if you run `plet build` is that an empty `dist` directory is created in the source root.

#### Creating templates

We'll need to create three templates for our blog in a new directory called `templates`:

* `templates/list.plet.html` &ndash; This will be the index page displaying a sorted list of blog posts.
* `templates/post.plet.html` &ndash; This template will be used to display a single blog post.
* `templates/layout.plet.html` &ndash; The layout shared by the two other templates.

The `templates/layout.plet.html` template is defined as follows:

```html+plet
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8"/>
    <title>My Blog</title>
  </head>
  <body>
    <header>
      <h1><a href="{'/' | link}">My Blog</a></h1>
    </header>
    <article>
      {CONTENT}
    </article>
  </body>
</html>
```

The template contains mostly HTML with two bits of Plet code surrounded by curly brackets:

* `{'/' | link}` evaluates to a link to the index page.
* `{CONTENT}` evaluates to the value of the special `CONTENT` variable, which contains the output of whatever template is using `templates/layout.plet.html` as a layout.

The `'/' | link` expression makes use of the pipe operator (`|`) which is just another way of writing chained function calls. The expression `a | b` is equivalent to `b(a)`, i.e. &ldquo;apply the function `b` to argument `a`&rdquo;. Thus we could have written `link('/')` instead, but the use of the pipe operator makes some expressions easier to read, especially in templates.

We'll use the layout template in `templates/list.plet.html`:

```html+plet
{LAYOUT = 'layout.plet.html'}
<p>Welcome to my blog.</p>
<ul>
  {for post in posts}
  <li>
    <a href="{post.link | link}">
      {post.title | h}
    </a>
    &ndash; Published {post.published | date('%Y-%m-%d')}
  </li>
  {end for}
</ul>
```

The first line selects a layout by assigning the relative path to the layout template to the special `LAYOUT` variable.

We then loop through an array of posts stored in the `posts` variable (we'll make sure that this variable has a value later) using the `{for ITEM in ARRAY}....{end for}` looping construct. We expect each `post` to be an *object* containing the properties `link`, `title`, and `published` (these are explained in the next section). The properties are accessed using dot notation, e.g. `post.title`. Aside from the previously used `link`-function, we also use two new functions:

* `post.title | h`  encodes special characters for use in HTML documents, e.g. `<` to `&lt;` and `&` to `&amp;`.
* `post.published | date('%Y-%m-%d')` formats a date/time.

The expression `a | b(c)` is equivalent to `b(a, c)`, in fact the pipe operator can be used with any number of arguments, e.g. `a | b(c, d, e)` is the same as `b(a, c, d, e)`.

The `templates/post.plet.html` template is a bit more simple:

```html+plet
{LAYOUT = 'layout.plet.html'}
<h1>{post.title | h}</h1>
{post.html | no_title | links | html}
```

This time we expect the variable `post` to contain an object with the properties `title` and `html`. The last line is an example of chaining multiple function calls using the pipe operator and is equivalent to `{html(links(no_title(post.html)))}`. `post.html` contains a syntax tree representing the content of the post that can be used for various transformations. `no_title` is one such transformation which simply removes the title (the first heading) from the document. `links` walks through the document and converts all internal links to absolute paths. Finally the `html` function converts the syntax tree to a raw HTML string that can be appended to the output.

#### Creating content

To get posts on our blog, we'll create a new directory called `posts` and add two Markdown files.

We'll call the first file `first.md`:

```markdown
{
  published: '2021-04-10 12:00' | time,
}
# My first blog post
This is a blog post written using Markdown.
```

The first three lines (delimited by curly brackets) of the above Markdown file is the *front matter*. The front matter uses Plet object notation and is parsed and evaluated when we later load the file via the `find_content` function. Any property specified in the front matter will later be accessible in the file's *content object*. In this case we assign a timestamp (the `time` function converts an ISO 8601 formatted date string to a timestamp) to the `published` property so that we can display it in our post list template.

We'll create another file called `second.md`:

```markdown
{
  published: '2021-04-11 12:00' | time,
}
# The second post
This is the second blog post.

We can easily link to the [first blog post](first.md).
```

In the second Markdown file we link to the first one via a relative path to `first.md`. This link will later be converted to an absolute path to the HTML document created from `first.md`.

#### Adding pages

At this point the directory structure of the source root should look like this:

* index.plet
* posts/
  * first.md
  * second.md
* templates/
  * layout.plet.html
  * list.plet.html
  * post.plet.html  

We can now glue everything together in `index.plet`:

```plet
# We start by collecting all the Markdown files in the posts-directory
posts = list_content('posts', {suffix: '.md'}) | sort_by_desc(.published)
for post in posts
  # We give each post a link using its name (filename without extension):
  post.link = "posts/{post.name}/index.html"
  # Then we add a page to the site map using the post-template:
  add_page(post.link, 'templates/post.plet.html', {post: post})
  # And add a reverse path:
  add_reverse(post.path, post.link)
end for
# Finally we add the frontpage containing the list of blog posts:
add_page('index.html', 'templates/list.plet.html', {posts: posts})
```

The `list_content` function will look for files in the specified directory and return an array of content objects created from the matching files. We use the `sort_by_desc` function to sort the found posts in descending order by the their `published`-property (`.prop` is syntactic sugar for `x => x.prop` which is an anonymous function that accepts an object and returns the value of the `prop` property of that object).

The main purpose of `index.plet` is to add pages to the site map. One way of doing that is by using the `add_page` function. The `add_page` function accepts three arguments: 1) the destination file path (should end in `.html` for HTML files), 2) the source template, and 3) data for the template.

We also use `add_reverse` to connect the input Markdown file (`post.path`) to the output path (`post.link`). This is what makes it possible to have a link to `first.md` inside `second.md`.

#### Testing and deploying

To test our blog we can run:

```sh
plet serve
```

This starts a local web server on [http://localhost:6500](http://localhost:6500) which can be used to quickly check the output while developing templates or writing content. The page reloads automatically when changes are detected in the source files.

To build our blog we can run:

```sh
plet build
```

This creates the following structure in the `dist` directory:

* index.html
* posts/
  * first/
    * index.html
  * second/
    * index.html

We can copy the content of the `dist` directory to any web server in order to deploy the site.


