# elmo

**elmo** (/ˈɛl.moʊ/) is a text editor focused on viewing and editing simple configuration and source code files.

It is derived from [kilo](https://github.com/antirez/kilo), as I was following this excellent [tutorial](https://viewsourcecode.org/snaptoken/kilo) while building it. While most of the front-end features are similar to **kilo**, **elmo** has its own buffer back-end design.

See [documentation](./DOCS.md) about the data structure used in the back-end.

## Build

### Requirements
- `gcc`
- `git`
- `make`

Run:

```sh
make
```

It will generate the `elmo` binary.

## Third-party notices

This project is based on third-party code. See the [NOTICE](./NOTICE) file for attribution and licensing details.