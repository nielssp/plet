## Content management

### Front matter

### Finding content

```plet
list_content('pages', {suffix: '.md', recursive: true})
```

### Content objects

```markdown
{
  my_custom_field: 'foo',
}
# Title

Content
```

```plet
{
  path: '/home/user/mypletsite/pages/subdir/file.md',
  relative_path: 'subdir',
  name: 'file',
  type: 'md',
  modified: time('2021-04-28T18:06:13Z'),
  content: '<h1>Title</h1><p>Content</p>',
  html: {
    type: symbol('fragment'),
    children: [
      {
        type: symbol('element'),
        tag: symbol('h1'),
        attributes: {},
        children: ['Title'],
      },
      {
        type: symbol('element'),
        tag: symbol('p'),
        attributes: {},
        children: ['Content'],
      },
    ],
  },
  title: 'Title',
  read_more: false,
  my_custom_field: 'foo',
}
```

### Relative paths

### Handling images

### Automatic table of contents

### Reverse links

### Pagination

### Custom transformations
