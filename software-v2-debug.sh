#!/bin/bash

sdcc -mmcs51 ./software-v2.c -DDEBUG --out-fmt-ihx -o ./test/software-v2.hex
