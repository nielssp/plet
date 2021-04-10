## CLI

### init

`plet init` creates a new empty `index.plet` file in the current working directory. In the future this command may get more options.

### build

`plet build` finds the nearest `index.plet` file and evaluates it.

### watch

`plet watch` first builds the site like `plet build`, then watches all source files for changes. When changes are detected, the site is built again.

### serve

`plet serve [-p <port>]` runs a built-in web server that builds pages on demand and automatically reloads when changes are detected.

### clean

`plet clean` recursively deletes the `dist` directory.

### eval

`plet eval <file>` evaluates a Plet script.

`plet eval -t <file>` evaluates a Plet template.
