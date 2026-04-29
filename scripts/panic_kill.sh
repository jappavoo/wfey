#!/bin/bash

kill $(ps -aux | grep run_wfey | cut -d ' ' -f 6)
