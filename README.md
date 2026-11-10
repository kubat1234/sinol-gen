# sinol-gen

One header, `gen.h`, for input generators (`ingen`) of
[sinol](https://github.com/sio2project/sinol-make) / SIO2 task packages.

- tests are plain function calls `g.test(f, args...)`, in loops if you like, with random
  arguments too: `g.test(f, gen::near(MAXN))`;
- names `abc0`, `abc1ocen`, `abc1a`, `abc1b` … are given automatically;
- every test has its own random generator: the same test on every machine and
  compiler;
- a test can be a struct: modify it (`| gen::relabel`), check it
  (`| gen::check(...)`), print it in one place;
- all tests live in the ingen, statement examples and ocen tests too;
- `./abcingen --list` shows which line of the ingen made each test;
- random trees, graphs, permutations, partitions, bracket sequences, anti-hash tests.

## Example

A tree task: group 1 `n ≤ 1000`, group 2 the tree is a path, group 3 `n ≤ 200 000`.

```cpp
#include <bits/stdc++.h>
#include "gen.h"
using namespace std;

const int MAXN = 200'000;

struct Test {  // the test as data, printed in one place
    int n;
    gen::Edges edges;
    friend ostream& operator<<(ostream& out, const Test& t) {
        gen::line(out, t.n);
        gen::print_edges(out, t.edges);
        return out;
    }
};

Test random_tree(gen::Rng& R, int n) { return {n, gen::trees::random(R, n)}; }
Test path(gen::Rng&, int n) { return {n, gen::trees::path(n)}; }
Test broom(gen::Rng&, int n, int handle) { return {n, gen::trees::broom(n, handle)}; }
bool is_max(const Test& t) { return t.n == MAXN; }

int main(int argc, char* argv[]) {
    gen::Generator<Test> g("tre", argc, argv);

    g.example("3\n1 2\n2 3\n");                      // tre0.in      the example from the statement
    g.ocen(random_tree, 10);                         // tre1ocen.in  public test

    g.group(1) | gen::relabel | gen::shuffle_edges;  // n <= 1000; hide the shapes in every test
    g.test("1\n");                                   // tre1a.in     by hand
    for (int n : {10, 100, 1000})
        g.test(random_tree, n);                      // tre1b.in, tre1c.in, tre1d.in
    g.test(broom, 1000, 500);                        // tre1e.in

    g.group(2) | gen::relabel | gen::shuffle_edges;  // paths
    g.test(path, MAXN) | gen::check(is_max, "maximal");
    g.test(path, MAXN - 1);

    g.group(3) | gen::relabel | gen::shuffle_edges;  // no extra limits
    g.test(random_tree, MAXN).seed(7);               // re-rolled with its own seed
    g.test(broom, MAXN, MAXN / 2) | gen::check(is_max, "maximal");

    return g.done();
}
```

```
$ ./treingen --list
test       line                   seed                             size     time  arguments
tre0       treingen.cpp:27        7593397132965728532              10 B      0ms  text
tre1ocen   treingen.cpp:28        10617943107078974372             40 B      0ms  (10)
tre1a      treingen.cpp:31        1159845466485833395               2 B      0ms  text
tre1b      treingen.cpp:33        7170536774046083736              40 B      0ms  (10)
tre1c      treingen.cpp:33        11632814458572847234            580 B      0ms  (100)
tre1d      treingen.cpp:33        1607719976114452912            7.6 KB      0ms  (1000)
tre1e      treingen.cpp:34        3598331830210562909            7.7 KB      0ms  (1000, 500)
tre2a      treingen.cpp:37        1353413588987421006            2.5 MB     39ms  (200000)
tre2b      treingen.cpp:38        16118672018387402406           2.5 MB     24ms  (199999)
tre3a      treingen.cpp:41        7 fixed                        2.5 MB     27ms  (200000)
tre3b      treingen.cpp:42        3485285868203599972            2.5 MB     22ms  (200000, 100000)
```

For a simple task the functions can write the test directly, without a struct:
[examples/simple.cpp](examples/simple.cpp).

## Use

Copy `gen.h` into `prog/` of the package, next to the ingen. Packages must be
self-contained, because sio2 compiles the ingen itself when a package is
uploaded. Needs C++17 and compiles with sinol-make's `-Wconversion -Werror`.

- [docs/guide.md](docs/guide.md): how to write an ingen (test list, seeds, test structs, modifiers, checks, running, pitfalls)
- [docs/reference.md](docs/reference.md): every function (`Rng`, trees, graphs, anti-hash tests, printing)
- [examples/](examples/): `simple.cpp`, `tree.cpp` (per-vertex data, own modifier), `readme.cpp`

## Development

`tests/run.sh` runs everything on gcc and clang, C++17/20/23. MIT license.

**Releasing:** change `SINOL_GEN_VERSION` in `gen.h` (the current version is
there) and push to `main`; the `release` workflow runs the tests, tags
`v<version>` and creates the GitHub release. `contest.py update gen` takes the
newest tag. Which number to raise:

- **major** (`2.0.0` → `3.0.0`): an existing ingen generates other tests or no
  longer compiles. The golden values in `tests/` fail — never just update them
  in a minor or patch release;
- **minor** (`2.0.0` → `2.1.0`): something new (a function, an option); every
  existing ingen gives the same tests;
- **patch** (`2.0.0` → `2.0.1`): a fix or docs; the same tests.

Several changes can wait on `main` and go out in one release.
