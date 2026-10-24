// Tree task: "n, then n - 1 edges, then a weight for every vertex".
// The test is built as a struct, modified (relabel, shuffled edges) and checked.
#include <bits/stdc++.h>
#include "../gen.h"
using namespace std;
using gen::ll;

const int MAXN = 200'000;
const ll MAXW = 1'000'000'000;

struct Test {
    int n;
    gen::Edges edges;
    vector<ll> w;  // w[0] = weight of vertex 1
    friend ostream& operator<<(ostream& out, const Test& t) {
        out << t.n << '\n';
        gen::print_edges(out, t.edges);
        gen::print_line(out, t.w);
        return out;
    }
};

Test make(gen::Rng& R, int n, gen::Edges edges, ll maxw) { return {n, std::move(edges), R.array(n, 1, maxw)}; }

Test random_tree(gen::Rng& R, int n, ll maxw) { return make(R, n, gen::trees::random(R, n), maxw); }
Test path(gen::Rng& R, int n, ll maxw) { return make(R, n, gen::trees::path(n), maxw); }
Test broom(gen::Rng& R, int n, int handle, ll maxw) { return make(R, n, gen::trees::broom(n, handle), maxw); }

// relabel moves the edges only, so this task's own modifier also moves the weights
void relabel_all(gen::Rng& R, Test& t) {
    auto p = gen::relabeling(R, t.n);
    gen::relabel_edges(t.edges, p);
    gen::relabel_values(t.w, p);
}

bool is_max(const Test& t) { return t.n == MAXN; }

int main(int argc, char* argv[]) {
    gen::Generator<Test> g("tre", argc, argv);

    g.example("3\n1 2\n2 3\n5 1 7\n");
    g.ocen(random_tree, 10, 10);
    g.ocen(path, 1000, MAXW) | relabel_all;

    g.group(1) | relabel_all | gen::shuffle_edges;   // every test of group 1
    g.test("1\n\n7\n");
    for (ll w : {10LL, 1'000LL, MAXW}) g.test(random_tree, 1000, w);
    g.test(broom, 1000, 500, MAXW);

    g.group(2) | gen::check([](const Test& t) { return t.n <= MAXN; }, "n <= MAXN");
    g.test(random_tree, MAXN, MAXW) | relabel_all | gen::check(is_max, "maximal");
    g.test(path, MAXN, MAXW).then(relabel_all).then(gen::flip_edges);
    g.test(broom, MAXN, MAXN / 2, MAXW).seed(7) | relabel_all;
    g.test(Test{2, {{1, 2}}, {MAXW, MAXW}});

    return g.done();
}
