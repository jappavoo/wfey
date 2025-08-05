#!/bin/bash

lscpu | sed -e "s/\s*Socket(s):\s*\([0-9][0-9]*\)/\1/p" -n
