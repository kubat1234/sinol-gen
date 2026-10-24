#!/bin/sh
# Tests of gen.h: unit tests, examples, command line behaviour, gcc == clang.
# Usage: tests/run.sh        (needs g++; clang++ is used when present)
set -eu
cd "$(dirname "$0")/.."
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
# the flags sinol-make compiles ingens with (+ -pedantic)
FLAGS="-O2 -Werror -Wall -Wextra -Wshadow -Wconversion -Wno-unused-result -Wfloat-equal -pedantic"
COMPILERS="g++"
command -v clang++ >/dev/null && COMPILERS="g++ clang++"
fail() { echo "FAIL: $*"; exit 1; }

for cxx in $COMPILERS; do
	for std in 17 20 23; do
		echo "== $cxx c++$std"
		$cxx -std=c++$std $FLAGS tests/test.cpp -o "$T/unit"
		"$T/unit" | tail -n 1
		for ex in examples/*.cpp; do
			name=$(basename "$ex" .cpp)
			$cxx -std=c++$std $FLAGS "$ex" -o "$T/$name"
			mkdir -p "$T/out-$cxx$std-$name"
			(cd "$T/out-$cxx$std-$name" && "$T/$name" >/dev/null)
		done
	done
done

echo "== same tests from every compiler and standard"
first=""
for d in "$T"/out-*; do
	key=${d##*-}
	ref=$(ls -d "$T"/out-*-"$key" | head -n 1)
	diff -r "$ref" "$d" >/dev/null || fail "$d differs from $ref"
	first=1
done
[ -n "$first" ] || fail "no examples"

# --- command line
cat >"$T/cli.cpp" <<'EOF'
#include <bits/stdc++.h>
#include "gen.h"
struct Test { int n; friend std::ostream& operator<<(std::ostream& o, const Test& t) { return o << t.n << '\n'; } };
Test make(gen::Rng&, int n) { return {n}; }
void writer(gen::Rng& R, std::ostream& out, int n) { out << R.range(1, n) << '\n'; }
int main(int argc, char* argv[]) {
    const char* mode = std::getenv("MODE") ? std::getenv("MODE") : "";
    gen::Generator<Test> g("cli", argc, argv);
    g.example("1\n");
    g.ocen(make, 5);
    g.group(1);
    g.test(make, 10);
    g.test(writer, 1000000);
    g.group(2);
    g.test(make, 20).check([](const Test& t) { return t.n == 20; }, "twenty");
    if (!std::strcmp(mode, "dup")) g.test(make, 10);
    if (!std::strcmp(mode, "check")) g.test(make, 21) | gen::check([](const Test& t) { return t.n == 20; }, "twenty");
    if (!std::strcmp(mode, "crash")) g.test([](gen::Rng&) -> Test { assert(false); return {0}; });
    if (!std::strcmp(mode, "two")) g.example("2\n");
    if (!std::strcmp(mode, "hand")) {
        g.group(3);
        g.test({"3", "1 2 3"});
        g.test("5");
        g.test(gen::text(R"(
            2 2
            ..
        )"));
        g.test("7\n\n");
    }
    if (!std::strcmp(mode, "nodone")) return 0;
    return g.done();
}
EOF
g++ -std=c++17 $FLAGS -I. "$T/cli.cpp" -o "$T/cli"
cd "$T" && mkdir run && cd run

"$T/cli" >/dev/null
[ "$(ls | tr '\n' ' ')" = "cli0.in cli1a.in cli1b.in cli1ocen.in cli2a.in " ] || fail "names: $(ls)"
first=$(cat cli1b.in)
rm ./*.in && "$T/cli" 1b >/dev/null
[ "$(ls)" = "cli1b.in" ] && [ "$(cat cli1b.in)" = "$first" ] || fail "single test or seed from name"
rm ./*.in && "$T/cli" 1 ocen >/dev/null
[ "$(ls | tr '\n' ' ')" = "cli1a.in cli1b.in cli1ocen.in " ] || fail "group selection: $(ls)"
rm ./*.in && "$T/cli" 0 >/dev/null
[ "$(ls)" = "cli0.in" ] || fail "examples selection"
"$T/cli" 7 >/dev/null 2>&1 && fail "unknown group must fail"
"$T/cli" --list | grep -q "cli2a .*cli.cpp:15 .*(20)" || fail "--list line numbers"

rm ./*.in && MODE=two "$T/cli" >/dev/null
[ "$(ls | tr '\n' ' ')" = "cli0a.in cli0b.in cli1a.in cli1b.in cli1ocen.in cli2a.in " ] || fail "two examples: $(ls)"
[ "$(cat cli0a.in)" = "1" ] && [ "$(cat cli0b.in)" = "2" ] || fail "two examples content"
rm ./*.in
MODE=hand "$T/cli" 3 >/dev/null 2>"$T/err"
[ "$(cat cli3a.in)" = "$(printf '3\n1 2 3')" ] && [ "$(od -c cli3a.in | tail -2 | head -1 | tr -s ' ')" != "" ] || fail "list of lines"
[ "$(cat cli3b.in | od -An -c | tr -d ' ')" = '5\n' ] || fail "final newline: $(od -c cli3b.in)"
[ "$(od -An -c cli3c.in | tr -d ' \n')" = '22\n..\n' ] || fail "gen::text: $(od -c cli3c.in)"
grep -q "warning: the test ends with an empty line" "$T/err" || fail "empty line warning"
rm ./*.in
MODE=dup "$T/cli" >/dev/null 2>"$T/err" && fail "duplicate must fail"
grep -q "cli.cpp:16: cli2b: identical to cli1a" "$T/err" || fail "duplicate message: $(cat "$T/err")"
MODE=check "$T/cli" >/dev/null 2>"$T/err" && fail "check must fail"
grep -q "cli.cpp:17: cli2b: check failed: twenty (check at line 17)" "$T/err" || fail "check message: $(cat "$T/err")"
MODE=crash "$T/cli" >/dev/null 2>"$T/err" && fail "crash must fail"
grep -q "crashed while generating cli2b (cli.cpp:18)" "$T/err" || fail "crash message: $(cat "$T/err")"
MODE=nodone "$T/cli" 2>"$T/err" >/dev/null
grep -q "g.done() was not called" "$T/err" || fail "missing done() warning"

echo "all tests passed"
