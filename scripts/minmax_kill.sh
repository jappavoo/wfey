#!/bin/bash

#set -x

kill -2 $(ps -a | grep minmax | cut -f1 -d' ')
