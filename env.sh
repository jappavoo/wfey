#!/bin/bash

export PAPI_DIR=$PWD/papi/src/install
export PATH=${PAPI_DIR}/bin:$PATH
export LD_LIBRARY_PATH=${PAPI_DIR}/lib:$LD_LIBRARY_PATH
