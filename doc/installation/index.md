## Installation

### Building from source

```sh
make clean all
```

#### Build options

Defaults:

```sh
make UNICODE=1 MARKDOWN=1 GUMBO=1 IMAGEMAGICK=1 all
```

For compatibility with [musl libc](https://musl.libc.org/) use:

```sh
make MUSL=1
```
