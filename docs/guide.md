# Writing an ingen with gen.h

```cpp
#include <bits/stdc++.h>
#include "gen.h"

int main(int argc, char* argv[]) {
    gen::Generator g("abc", argc, argv);   // or gen::Generator<Test>, see "Tests as data"
    ...                                    // the test list
    return g.done();                       // writes the tests
}
```

## The test list

| call | test |
|---|---|
| `g.example(x)` | examples from the statement: `abc0.in` if there is one, `abc0a.in`, `abc0b.in`, … if more |
| `g.ocen(x)` | `abc1ocen.in`, `abc2ocen.in`, … — public tests, contestants get them right away |
| `g.group(k)` | the following `g.test`s go to group k |
| `g.test(x)` | `abc<k>a.in`, `abc<k>b.in`, … in the order of the calls |

`x` is one of:

| x | |
|---|---|
| `"text"` | the content of the test (a missing final `\n` is added) |
| `{"line", "line", ...}` | one string per line |
| `gen::text(R"(...)")` | multi-line text indented with the code, see below |
| `f, args...` with `void f(gen::Rng& R, ostream& out, args...)` | f writes the test |
| `f, args...` with `Test f(gen::Rng& R, args...)` | f returns the test struct, or anything printable with `<<` |
| `Test{...}` | a test struct by hand |

`f` can take any arguments after `R` (and `out`). They are copied when the test
is added and passed to `f` when the test is generated, so a loop works as
expected:

```cpp
for (long long v : {10, 1000, MAXV})
    g.test(random_arr, MAXN, v);
```

A lambda is fine too: `g.test([](gen::Rng& R, ostream& out) { ... })`.

### Random arguments

So that not every test has exactly `n = MAXN`:

```cpp
g.test(random_arr, gen::near(MAXN), MAXV);            // n from [MAXN - 10, MAXN]
g.test(random_arr, gen::near(MAXN, 1000), MAXV);      // n from [MAXN - 1000, MAXN]
g.test(random_arr, gen::between(1000, MAXN), MAXV);   // any n from the range
g.test(random_arr, gen::one_of({1, 2, MAXN}), MAXV);  // one of these
```

They are drawn from the test's own generator (left to right, the same on every
compiler) when the test is generated; the function gets a plain value, and
`--list` shows the drawn one. The type follows the bounds as in `R.range`:
`gen::near(MAXN)` is an `int`, `gen::between(1, 10'000'000'000LL)` a `long long`.
Values that depend on each other (`m` from the drawn `n`) belong in the function.

### Hand-written tests

They belong to the ingen as well: the whole `in/` is generated, nothing has to be
committed and nothing gets lost.

```cpp
g.test("1\n5\n");                 // short ones
g.test({"3", "1 2 3"});            // one string per line
g.test(gen::text(R"(
    3 4
    #..#
    ....
    .##.
)"));                              // bigger ones, e.g. boards: "3 4\n#..#\n....\n.##.\n"
```

`R"( ... )"` is a C++ raw string: everything between `R"(` and `)"` is taken
literally, newlines included. `gen::text` drops its first (empty) line, the
last line if it is only spaces, and the indentation common to all lines — so the
test can be indented with the code and still looks exactly like the file.

Every text test ends with `\n` (it is added when missing, a test without it
would not pass the input verifier); a test ending with an empty line gets a warning.

To change one detail of a random test, use a modifier:
`g.test(random_tree, 10).then([](Test& t) { t.edges[0] = {1, 2}; });`

## Seeds

Each test gets its own `gen::Rng`, seeded from the test's name (`"2c"`), so:

- the output is the same on every machine and compiler;
- adding a test at the end of a group does not change the others;
- `./abcingen 2c` regenerates exactly that test.

`.seed(x)` uses `x` instead of the name: to re-roll one bad test, or to get the
same test in another package — `g.test(f, args...).seed(x)` gives the same test anywhere.

Never use `rand()`, `random_device`, `std::shuffle` or `std::uniform_int_distribution`
in an ingen: their results differ between compilers. Use `R`.

## Tests as data

For anything more than a line of numbers, make the test a struct with
`operator<<` and use `gen::Generator<Test>`:

```cpp
struct Test {
    int n;
    gen::Edges edges;
    friend ostream& operator<<(ostream& out, const Test& t) {
        gen::line(out, t.n);
        gen::print_edges(out, t.edges);
        return out;
    }
};

Test random_tree(gen::Rng& R, int n) { return {n, gen::trees::random(R, n)}; }
```

The input format is then written once, and the test can be modified and checked
before it is printed.

### Modifiers

`void m(gen::Rng& R, Test& t)` or `void m(Test& t)`:

```cpp
g.test(broom, MAXN, 10) | gen::relabel | gen::shuffle_edges;
g.test(broom, MAXN, 10).then(gen::relabel).then(gen::shuffle_edges);   // the same
```

They run left to right with the test's own `R`. Built-in ones work on any struct
with `n` and `edges` (`gen::Edges` or `gen::WEdges`):

| | |
|---|---|
| `gen::relabel` | random vertex labels |
| `gen::shuffle_edges` | random order of the edges |
| `gen::flip_edges` | random orientation of every edge |

`gen::relabel` moves only the edges. If the test has per-vertex data (weights,
colors), write your own modifier:

```cpp
void relabel_all(gen::Rng& R, Test& t) {
    auto p = gen::relabeling(R, t.n);
    gen::relabel_edges(t.edges, p);
    gen::relabel_values(t.w, p);
}
```

### Checks

`bool c(const Test& t)`, run after the modifiers:

```cpp
g.test(random_tree, MAXN) | gen::check(is_max, "maximal");
g.test(no_answer, 1000).check(has_no_answer, "answer is -1");
```

A failed check is an error with the line of the test:
`abcingen.cpp:44: abc2c: check failed: maximal (check at line 44)`.

Checks are for what a test is *meant* to be — maximal, without an answer, a
deep tree — the things that silently go wrong when a parameter is mistyped. The
limits of the groups belong to the input verifier (`abcinwer.cpp`), which also
checks the format of the file.

### Whole groups

```cpp
g.group(3) | gen::relabel | gen::check(is_connected, "connected");
```

applies to every test of group 3 made as a struct (not to `"text"` or stream
tests), after the test's own modifiers and checks.

## Running

```
./abcingen               all tests into the current directory (sinol-make gen does this)
./abcingen 2 2c 0 ocen   only group 2, test 2c, the examples, the ocen tests
./abcingen --list        test, line in the ingen, seed, size, time, arguments
```

`--list` is the way back from a test to the code: when `abc2c` kills a
solution or fails the verifier, it shows the line and the arguments that made it.
sinol-make keeps the compiled ingen in `.cache/executables/abcingen.cpp.e`.

Errors — the exit code is then 1, so `sinol-make gen` fails:

- two identical tests (`abc2d: identical to abc2b`);
- a failed check;
- a test bigger than `g.max_size_mb(x)` (no limit by default);
- an exception or a crash (`gen: crashed while generating abc2c (abcingen.cpp:53)`);
- `g.test` before any `g.group`.

## Pitfalls

- sinol-make compiles with `-Wconversion -Werror`: `g.test(f, 1e5)` with an
  `int` parameter is an error (`1e5` is a `double`). Write `100'000`.
- A generator function named like a C library function (`random`, `write`,
  `index`, `time`) clashes with it: `<unresolved overloaded function type>`.
  Pick another name.
- At most 8 arguments after `f` (bundle more into a struct).
- `gen::relabel` moves only the edges, see above.
- sio2 compiles the ingen with its own compiler when the package is uploaded,
  so `gen.h` sticks to C++17. Check what C++ your sio2 uses.
