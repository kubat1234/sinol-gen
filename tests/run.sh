#!/bin/sh
# Tests of gen.h: unit tests, examples, command line behaviour, gcc == clang.
# Usage: tests/run.sh        (needs g++; clang++ is used when present)
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd)
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
    g.group(1); if (!std::strcmp(mode, "extra")) g.test(writer, 999);
    g.test(make, 10);
    { auto r = g.test(writer, 1000000); if (std::getenv("PIN")) r.seed(std::strtoull(std::getenv("PIN"), nullptr, 10)); }
    g.group(2); if (!std::strcmp(mode, "rep")) { g.test(writer, 1000000); g.test(writer, 1000000); } if (!std::strcmp(mode, "dupseed")) { g.test(writer, 5).seed(9); g.test(writer, 6).seed(9); }
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
[ "$(ls)" = "cli1b.in" ] && [ "$(cat cli1b.in)" = "$first" ] || fail "single test"
# golden: the automatic seed must never change within 2.x
[ "$first" = "54290" ] || fail "automatic seed changed: $first"
seed=$("$T/cli" --list | awk '$1 == "cli1b" { print $3 }')
rm ./*.in && PIN=$seed "$T/cli" 1b >/dev/null
[ "$(cat cli1b.in)" = "$first" ] || fail ".seed(<seed from --list>) must keep the test"
PIN=$seed "$T/cli" --list | grep -q "cli1b .* $seed fixed" || fail "--list must mark a fixed seed"
rm ./*.in && MODE=extra "$T/cli" >/dev/null
[ "$(cat cli1c.in)" = "$first" ] || fail "a test inserted before changed a later test"
rm ./*.in && MODE=rep "$T/cli" >/dev/null || fail "identical calls must get different seeds"
[ "$(cat cli2a.in)" != "$first" ] || fail "the group must be part of the seed"
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
MODE=dupseed "$T/cli" >/dev/null 2>"$T/err" && fail "duplicate seed must fail"
grep -q "cli2b: the same .seed(9) as cli2a" "$T/err" || fail "duplicate seed message: $(cat "$T/err")"
MODE=nodone "$T/cli" 2>"$T/err" >/dev/null
grep -q "g.done() was not called" "$T/err" || fail "missing done() warning"

# --- the automatic seed of every kind of argument: the same from every
# compiler, and never changing within 2.x (golden). f and g have the same body:
# identical code folding must not make them one function.
cat >"$T/keys.cpp" <<'EOF'
#include <bits/stdc++.h>
#include "gen.h"
void f(gen::Rng& R, std::ostream& out, int n) { out << R.range(1, n) << '\n'; }
void g(gen::Rng& R, std::ostream& out, int n) { out << R.range(1, n) << '\n'; }
void s(gen::Rng& R, std::ostream& out, const std::string& w, double x) { out << w << ' ' << x << ' ' << R.u64() << '\n'; }
int main(int argc, char* argv[]) {
    gen::Generator<> gen_("key", argc, argv);
    gen_.example("1\n");
    gen_.ocen(f, 100);
    gen_.group(1);
    gen_.test(f, 1000000);
    gen_.test(f, 1000000);
    gen_.test(g, 1000000);
    gen_.test(f, gen::between(10, 20));
    gen_.test(f, gen::near(1000000));
    gen_.test(f, gen::one_of({5, 50, 500}));
    gen_.test(s, "a string longer than twenty characters", 0.1);
    gen_.test(s, std::string("a string longer than twenty characters!"), 0.1);
    return gen_.done();
}
EOF
golden="key0 17140341910512901480
key1ocen 12850621129123314982
key1a 8485187109482800914
key1b 8485188208994429125
key1c 8485187109482800914
key1d 2464526024968879699
key1e 2275578229240683954
key1f 13028781354010478165
key1g 13720613122717612743
key1h 11318283608144379270"
for cxx in $COMPILERS; do
	$cxx -std=c++17 $FLAGS -I"$ROOT" "$T/keys.cpp" -o "$T/keys-$cxx"
	"$T/keys-$cxx" --list 2>/dev/null | awk 'NR > 1 { print $1, $3 }' >"$T/keys-$cxx.txt" || true
done
[ "$(cat "$T/keys-g++.txt")" = "$golden" ] || fail "automatic seeds changed:
$(cat "$T/keys-g++.txt")"
for cxx in $COMPILERS; do diff "$T/keys-g++.txt" "$T/keys-$cxx.txt" >/dev/null || fail "$cxx gives other seeds"; done
awk '$1 == "key1c" { c = $2 } $1 == "key1a" { a = $2 } END { exit a == c ? 0 : 1 }' "$T/keys-g++.txt" || fail "same arguments, different functions: same seed expected"

echo "all tests passed"
