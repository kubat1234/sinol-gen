// The example from README.md. Task: a tree with n vertices.
// Group 1: n <= 1000, group 2: the tree is a path, group 3: n <= 200 000.
#include <bits/stdc++.h>
#include "../gen.h"
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
