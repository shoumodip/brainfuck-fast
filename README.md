# Brainfuck Fast
Brainfuck compiler for Linux.

## Quick Start
Install [FASM](https://flatassembler.net/)

```console
$ ./build.sh
$ ./brainfuck examples/hello_world.brainfuck
$ ./examples/hello_world
```

## Speed
The Mandelbrot example by Erik Bosman.

```console
$ ./brainfuck examples/mandelbrot.brainfuck
$ time ./examples/mandelbrot
real 0m1.324s
user 0m1.306s
sys  0m0.008s
```
