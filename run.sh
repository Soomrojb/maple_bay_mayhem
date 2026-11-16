#!/bin/sh
# Builds (if needed) and launches Maple Bay Mayhem.
cd "$(dirname "$0")" || exit 1
make -s maplebay || exit 1
exec ./maplebay "$@"
