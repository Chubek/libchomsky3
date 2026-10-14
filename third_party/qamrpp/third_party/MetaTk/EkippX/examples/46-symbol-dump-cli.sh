#!/usr/bin/env sh
set -eu
ekippx-cli --dump-symtbl /tmp/ekippx-symbols.json --symtbl-format json --eval '@plugin(text)@emitln(&reverseText(stressed))'
