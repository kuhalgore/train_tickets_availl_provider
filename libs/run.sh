#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$DIR/libs:$LD_LIBRARY_PATH"
exec "$DIR/HtmlJsonExtractor" "$@"