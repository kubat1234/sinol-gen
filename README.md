# sinol-gen

One header, `gen.h`, for input generators (`ingen`) of
[sinol](https://github.com/sio2project/sinol-make) / SIO2 task packages.

- tests are plain function calls `g.test(f, args...)`, in loops if you like, with random
  arguments too: `g.test(f, gen::near(MAXN))`;
- names `abc0`, `abc1ocen`, `abc1a`, `abc1b` … are given automatically;
- every test has its own random generator: the same test on every machine, and
  adding a test does not change the others;
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
test       line              seed     size   time  arguments
tre0       treingen.cpp:25   name     10 B    0ms  text
tre1ocen   treingen.cpp:26   name     40 B    0ms  (10)
tre1a      treingen.cpp:29   name      2 B    0ms  text
tre1b      treingen.cpp:31   name     41 B    0ms  (10)
tre1c      treingen.cpp:31   name    585 B    0ms  (100)
tre1d      treingen.cpp:31   name   7.6 KB    0ms  (1000)
tre1e      treingen.cpp:32   name   7.7 KB    0ms  (1000, 500)
tre2a      treingen.cpp:35   name   2.5 MB   18ms  (200000)
tre2b      treingen.cpp:36   name   2.5 MB   18ms  (199999)
tre3a      treingen.cpp:39   7      2.5 MB   18ms  (200000)
tre3b      treingen.cpp:40   name   2.4 MB   14ms  (200000, 100000)
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

`tests/run.sh` runs everything on gcc and clang, C++17/20/23. A new version never
changes the tests that an existing ingen generates (golden values in
`tests/test.cpp`); if that is ever unavoidable, the major version in
`SINOL_GEN_VERSION` changes. MIT license.
