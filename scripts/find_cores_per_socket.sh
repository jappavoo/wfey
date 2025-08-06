#!/bin/bash

lscpu | sed -e "s/\s*Core(s) per socket:\s*\([0-9][0-9]*\)/\1/p" -n
