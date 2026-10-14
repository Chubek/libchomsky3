#!/usr/bin/env sh
set -eu

CC_BIN="${CC:-cc}"
CFLAGS="-std=c11 -Wall -Wextra -Werror -I."

cat > /tmp/azma_unit_header_smoke.c <<'SRC'
#include "AzmaUnit.h"
int main(int argc, char **argv) { return azma_unit_main(argc, argv); }
SRC

cat > /tmp/azma_fuzz_header_smoke.c <<'SRC'
#include "AzmaFuzz.h"
static AzmaStatus target(void *user, const uint8_t *data, size_t size) {
    (void)user; (void)data; (void)size;
    return AZMA_STATUS_OK;
}
int main(void) {
    AzmaFuzzOptions opt = azma_fuzz_options_default();
    AzmaFuzzStats stats;
    return (int)azma_fuzz_run(target, NULL, NULL, 0, &opt, &stats);
}
SRC

$CC_BIN $CFLAGS /tmp/azma_unit_header_smoke.c -o /tmp/azma_unit_header_smoke
$CC_BIN $CFLAGS /tmp/azma_fuzz_header_smoke.c -o /tmp/azma_fuzz_header_smoke

echo "unit/fuzz header smoke: ok"
