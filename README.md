# Object Drag

Measures object drag

## Usage

In src/, build with

    $ mkdir obj/
    $ make PIN_ROOT=</path/to/Pin> obj/drag.so

Run with

    $ </path/to/Pin> -t obj/drag.so -- </path/to/executable> <executable_args>

Print additional information including backtraces with -v option:

    $ </path/to/Pin> -t obj/drag.so -v 1 -- </path/to/executable> <executable_args>
