# Object Drag

Measures object drag

## Usage

In src/, build with

    $ mkdir obj/
    $ make PIN_ROOT=</path/to/Pin> obj/drag.so

Run with

    $ </path/to/Pin> -t obj/drag.so -- </path/to/executable> <executable_args>

Enable instrumentation on startup with -i option:

    $ </path/to/Pin> -t obj/drag.so -i 1 -- </path/to/executable> <executable_args>

By default, instrumentation is disabled on startup. Instrumentation can be enabled
by sending SIGUSR1 to the process and stopped by sending SIGUSR2. Signals can be
sent with

    $ kill -s <SIGUSR1/SIGUSR2> <PID>

or raised within the application by manually calling raise(3).
