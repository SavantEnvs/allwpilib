#!/usr/bin/env bash
#
# mayhem/test.sh — RUN allwpilib's own Catch2 suites for the fuzzed libraries (datalog, wpiutil,
# wpinet, ntcore), already built by mayhem/build.sh into build-tests/bin with normal flags.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Each binary prints a Catch2 summary: "All tests passed (N assertions in M test cases)" or
# "test cases: M | P passed | F failed [| S skipped]". Parse the test-case counts from that line itself
# (never trust the exit code alone): a binary that is missing, prints no summary or exits non-zero
# without reporting failures counts as a failure.
passed=0 ; failed=0 ; skipped=0
for t in datalog_test wpiutil_test wpinet_test ntcore_test; do
  bin="$SRC/build-tests/bin/$t"
  if [ ! -x "$bin" ]; then echo "MISSING test runner $bin (mayhem/build.sh should have built it)" >&2; failed=$((failed + 1)); continue; fi
  out="$("$bin" 2>&1)" ; rc=$?
  printf '%s\n' "$out" | tail -n 5
  if line="$(printf '%s\n' "$out" | grep -E '^All tests passed \(' | tail -n 1)" && [ -n "$line" ]; then
    n="$(printf '%s' "$line" | sed -E 's/.* in ([0-9]+) test cases?\).*/\1/')"
    p=$n ; f=0 ; s=0
  elif line="$(printf '%s\n' "$out" | grep -E '^test cases: ' | tail -n 1)" && [ -n "$line" ]; then
    p="$(printf '%s' "$line" | grep -oE '[0-9]+ passed' | grep -oE '[0-9]+' || true)" ; p=${p:-0}
    f="$(printf '%s' "$line" | grep -oE '[0-9]+ failed' | grep -oE '[0-9]+' || true)" ; f=${f:-0}
    s="$(printf '%s' "$line" | grep -oE '[0-9]+ skipped' | grep -oE '[0-9]+' || true)" ; s=${s:-0}
  else
    echo "$t: no Catch2 summary line in output (rc=$rc)" >&2
    failed=$((failed + 1)) ; continue
  fi
  if [ "$rc" -ne 0 ] && [ "$f" -eq 0 ]; then f=1; fi
  if [ "$p" -eq 0 ] && [ "$f" -eq 0 ]; then echo "$t: ran zero test cases" >&2; f=1; fi
  echo "$t: $p passed, $f failed, $s skipped"
  passed=$((passed + p)) ; failed=$((failed + f)) ; skipped=$((skipped + s))
done

emit_ctrf "catch2" "$passed" "$failed" "$skipped"
