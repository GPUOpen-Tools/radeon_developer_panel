#!/bin/bash

if [[ "$2" == "" ]]; then
  python -msphinx -M $1 source build
else
  python -msphinx -M $1 source build -t $2
fi
rm build/html/ProfilingModes.html

