#!/bin/bash

# When the container can be run with multiple commands, it's good practice to provide
# an entrypoint script. See https://docs.docker.com/engine/userguide/eng-image/dockerfile_best-practices/#entrypoint

set -e
case "$1" in
'upload')
    make;
    exec make upload;
    ;;
'build')
    exec make;
    ;;
'monitor')
    exec make monitor;
    ;;
'bash')
    exec /bin/bash
    ;;
*)
    exec make
    ;;
esac
