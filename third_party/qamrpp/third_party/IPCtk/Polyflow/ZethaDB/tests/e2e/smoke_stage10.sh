#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${ROOT}/build"
MEM_BIN="${BUILD}/zetha-mem"
RDB_BIN="${BUILD}/zetha-rdb"
TMP_MEM="/tmp/zetha_stage10_mem.zdb"
TMP_RDB="/tmp/zetha_stage10_rdb.zdb"
TMP_DUMP="/tmp/zetha_stage10_dump.zdb"
TMP_SCHEMA="/tmp/zetha_stage10_schema.txt"
TMP_MEM_SCRIPT="/tmp/zetha_stage10_mem.cli"
TMP_RDB_SCRIPT="/tmp/zetha_stage10_rdb.cli"

rm -f "${TMP_MEM}" "${TMP_RDB}" "${TMP_DUMP}" "${TMP_SCHEMA}" "${TMP_MEM_SCRIPT}" "${TMP_RDB_SCRIPT}"

if [[ ! -f "${MEM_BIN}" || ! -f "${RDB_BIN}" ]]; then
  echo "missing cli binaries in ${BUILD}" >&2
  exit 1
fi
cp "${MEM_BIN}" /tmp/zetha-mem
cp "${RDB_BIN}" /tmp/zetha-rdb
MEM_BIN="/tmp/zetha-mem"
RDB_BIN="/tmp/zetha-rdb"

cat > "${TMP_SCHEMA}" <<'EOF'
database appdb;
table users ( id text primary key, name text, age integer );
EOF

cat > "${TMP_MEM_SCRIPT}" <<EOF
create
open ${TMP_MEM}
put users/orders id=7 status=open
put users/quoted id=9 note='single quote value'
put users/escaped id=10 note=\"escape quote\"
put users/orders2 id=8 status="in progress"
query /users/orders[@id=7]
query "/users/orders2[@status='in progress']"
get users/quoted
get users/escaped
dump ${TMP_DUMP}
load ${TMP_DUMP}
get users/orders
erase users/orders
exit
EOF
"${MEM_BIN}" --script "${TMP_MEM_SCRIPT}"

cat > "${TMP_RDB_SCRIPT}" <<EOF
create ${TMP_RDB} database appdb; table users ( id text primary key, name text, age integer );
schema-load "database appdb; table users ( id text primary key, name text, age integer );"
put users u1 name=alice age=31
put users u2 name="alice smith" age=29
query /users[@age=31]
query "/users[@name='alice smith']"
query '/users[@name="alice smith"]'
get users u1
maintain
erase users u1
query /users[@id='u1']
exit
EOF
"${RDB_BIN}" --script "${TMP_RDB_SCRIPT}"

TMP_BAD_SCRIPT="/tmp/zetha_stage10_bad.cli"
cat > "${TMP_BAD_SCRIPT}" <<'EOF'
query "unterminated
EOF
if "${MEM_BIN}" --script "${TMP_BAD_SCRIPT}" >/tmp/zetha_bad.out 2>/tmp/zetha_bad.err; then
  echo "expected unterminated quote failure" >&2
  exit 1
fi
if ! grep -q 'error\[usage\]: unterminated quote' /tmp/zetha_bad.err; then
  echo "missing unterminated quote diagnostic" >&2
  exit 1
fi
if ! grep -q '"kind":"usage"' /tmp/zetha_bad.err; then
  echo "missing machine-readable diagnostic payload" >&2
  exit 1
fi

echo "stage10-e2e-ok"
