#!/bin/bash
if [[ "$OSTYPE" == "darwin"* ]]; then
    export DYLD_LIBRARY_PATH="$PWD/VKEngine/lib/":$DYLD_LIBRARY_PATH
    echo "DYLD_LIBRARY_PATH : "$DYLD_LIBRARY_PATH
elif [[ "OSTYPE" == "linux-gnu"* ]]; then
    export LD_LIBRARY_PATH="$PWD/VKEngine/lib/":$LD_LIBRARY_PATH
    sudo ldconfig
    echo "LD_LIBRARY_PATH : " $LD_LIBRARY_PATH
fi
