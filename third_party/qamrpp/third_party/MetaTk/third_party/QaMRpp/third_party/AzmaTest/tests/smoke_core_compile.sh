#!/usr/bin/env sh
set -eu

cc -std=c11 -Wall -Wextra -Werror -I. -c AzmaIDL.c -o /tmp/azmaidl.o
cc -std=c11 -Wall -Wextra -Werror -I. -c Main.c -o /tmp/azma-main.o
cc -std=c11 -Wall -Wextra -Werror -I. -x c - -c -o /tmp/azma-common-smoke.o <<'SRC'
#include "Common.h"
int main(void) { return 0; }
SRC
cc -std=c11 -Wall -Wextra -Werror -I. -x c - -c -o /tmp/azma-idl-smoke.o <<'SRC'
#include "AzmaIDL.h"
int main(void) { return 0; }
SRC

echo "core compile smoke: ok"
