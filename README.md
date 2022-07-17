# Sol project 2021/2022
A file storage server that saves files in main memory.

Clients connects to the server through a unix domain socket and can write or read files from it.

# Prerequisites

You need to install `valgrind` and `make` tools in addition to the `gcc` C compiler

# Configuaration

A [sample_configuration.txt](https://github.com/Rodrigo-Casella/file-storage-server/blob/master/sample_config.txt) is provided as a configuration
exemple for the server.

# Usage

To build the application run:

`make all`

To start the server run:

`bin/server path-to-configuration-file.txt`

To start the client run:

`bin/client -f server-socket [options]`

You can see which options you can use by running `bin/client -h`

To clean the working directory run:

`make clean`

# Tests

Three test are provided to measure the performance of the server:

`make test1` will run the server with `valgrind leak-check=full`

`make test2` will run the server with four different eviction policies: `FIFO`, `LRU`, `LFU` and `Second-chance`

`make test3` will run a stress test for the server

To clean the working directory from the files produced by the tests run:

`make cleanall`
