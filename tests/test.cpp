// Unit tests of gen.h. Run: tests/run.sh
#include <bits/stdc++.h>
#include "../gen.h"
using namespace std;
using gen::ll;

static int failures = 0;
#define EXPECT(cond)                                                          \
    do {                                                                      \
        if (!(cond)) std::cerr << __LINE__ << ": FAILED " #cond "\n", failures++; \
    } while (0)

// n vertices 1..n, n - 1 edges, connected
static bool is_tree(int n, const gen::Edges& e) {
    if (static_cast<int>(e.size()) != n - 1) return false;
    vector<int> p(static_cast<size_t>(n) + 1);
    iota(p.begin(), p.end(), 0);
    function<int(int)> find = [&](int x) { return p[static_cast<size_t>(x)] == x ? x : p[static_cast<size_t>(x)] = find(p[static_cast<size_t>(x)]); };
    for (auto [u, v] : e) {
        if (u < 1 || u > n || v < 1 || v > n || find(u) == find(v)) return false;
        p[static_cast<size_t>(find(u))] = find(v);
    }
    return true;
}

static bool is_simple(int n, const gen::Edges& e) {
    set<pair<int, int>> s;
    for (auto [u, v] : e)
        if (u == v || u < 1 || v < 1 || u > n || v > n || !s.insert({min(u, v), max(u, v)}).second) return false;
    return true;
}

static vector<int> degrees(int n, const gen::Edges& e) {
    vector<int> d(static_cast<size_t>(n) + 1);
    for (auto [u, v] : e) d[static_cast<size_t>(u)]++, d[static_cast<size_t>(v)]++;
    return d;
}

static int depth(int n, const gen::Edges& e, int root = 1) {
    vector<vector<int>> adj(static_cast<size_t>(n) + 1);
    for (auto [u, v] : e) adj[static_cast<size_t>(u)].push_back(v), adj[static_cast<size_t>(v)].push_back(u);
    vector<int> d(static_cast<size_t>(n) + 1, -1);
    d[static_cast<size_t>(root)] = 0;
    queue<int> q;
    q.push(root);
    int best = 0;
    while (!q.empty()) {
        int v = q.front();
        q.pop();
        for (int u : adj[static_cast<size_t>(v)])
            if (d[static_cast<size_t>(u)] < 0) d[static_cast<size_t>(u)] = d[static_cast<size_t>(v)] + 1, best = max(best, d[static_cast<size_t>(u)]), q.push(u);
    }
    return best;
}

int main() {
    // --- golden values: the generator must never change for existing ingens
    {
        gen::Rng R(42);
        gen::ull h = 0;
        for (int i = 0; i < 1000; i++) h = h * 1000003 + R.u64();
        cout << "golden u64: " << h << "\n";
        EXPECT(h == 16306265535207258383ULL);
        gen::Rng R2(gen::detail::fnv1a("2c"));
        string s;
        for (int i = 0; i < 20; i++) s += to_string(R2.range(1, 100)) + " ";
        cout << "golden range: " << s << "\n";
        EXPECT(s == "87 7 92 30 15 62 46 24 76 12 1 40 85 47 96 64 30 8 18 50 ");
    }

    gen::Rng R(1);
    // --- Rng
    for (int i = 0; i < 10000; i++) {
        int x = R.range(-3, 3);
        EXPECT(-3 <= x && x <= 3);
        ll y = R.range(1, 10'000'000'000LL);
        EXPECT(1 <= y && y <= 10'000'000'000LL);
        double r = R.real();
        EXPECT(0 <= r && r < 1);
        ll z = R.near(100, 5);
        EXPECT(95 <= z && z <= 100);
    }
    static_assert(is_same_v<decltype(R.range(1, 2)), int>);
    static_assert(is_same_v<decltype(R.range(1, 2LL)), ll>);
    EXPECT(R.range(LLONG_MIN, LLONG_MAX) || true);  // full range must not hang
    {
        auto d = R.distinct(1000, 1, 1000);
        EXPECT(set<ll>(d.begin(), d.end()).size() == 1000);
        auto d2 = R.distinct(1000, 1, 1'000'000'000'000LL);
        EXPECT(set<ll>(d2.begin(), d2.end()).size() == 1000);
        auto p = R.permutation(50);
        auto q = p;
        sort(q.begin(), q.end());
        vector<int> id(50);
        iota(id.begin(), id.end(), 1);
        EXPECT(q == id);
        auto parts = R.partition(100, 7, 3);
        EXPECT(accumulate(parts.begin(), parts.end(), 0LL) == 100);
        EXPECT(*min_element(parts.begin(), parts.end()) >= 3);
        auto smp = R.sample(vector<int>{1, 2, 3, 4, 5}, 3);
        EXPECT(smp.size() == 3 && set<int>(smp.begin(), smp.end()).size() == 3);
        vector<int> hits(3);
        for (int i = 0; i < 30000; i++) hits[R.weighted({1, 0, 2})]++;
        EXPECT(hits[1] == 0 && hits[2] > hits[0] * 3 / 2);
    }
    {  // brackets: balanced, and every sequence of length 6 appears (5 of them)
        map<string, int> seen;
        for (int i = 0; i < 5000; i++) {
            string b = R.brackets(3);
            int bal = 0;
            bool ok = b.size() == 6;
            for (char c : b) ok &= (bal += c == '(' ? 1 : -1) >= 0;
            EXPECT(ok && bal == 0);
            seen[b]++;
        }
        EXPECT(seen.size() == 5);
        for (auto& [b, k] : seen) EXPECT(k > 800);  // uniform: ~1000 each
    }
    // --- trees
    for (int n : {1, 2, 3, 10, 1000}) {
        EXPECT(is_tree(n, gen::trees::random(R, n)));
        EXPECT(is_tree(n, gen::trees::recursive(R, n)));
        EXPECT(is_tree(n, gen::trees::deep(R, n, 3)));
        EXPECT(is_tree(n, gen::trees::path(n)));
        EXPECT(is_tree(n, gen::trees::star(n)));
        EXPECT(is_tree(n, gen::trees::binary(n)));
        EXPECT(is_tree(n, gen::trees::binary_random(R, n)));
        EXPECT(is_tree(n, gen::trees::caterpillar(R, n, max(1, n / 2))));
        EXPECT(is_tree(n, gen::trees::broom(n, max(1, n / 2))));
        EXPECT(is_tree(n, gen::trees::spider(n, 3)));
        if (n >= 3) {
            auto e = gen::trees::max_degree(R, n, 3);
            EXPECT(is_tree(n, e));
            auto d = degrees(n, e);
            EXPECT(*max_element(d.begin(), d.end()) <= 3);
            auto b = gen::trees::binary_random(R, n);
            auto db = degrees(n, b);
            EXPECT(db[1] <= 2 && *max_element(db.begin(), db.end()) <= 3);
            auto w = gen::trees::with_depth(R, n, n / 2);
            EXPECT(is_tree(n, w) && depth(n, w) == n / 2);
            auto par = gen::trees::parents_increasing(R, gen::trees::random(R, n), n);
            bool inc = true;
            for (int i = 2; i <= n; i++) inc &= 1 <= par[static_cast<size_t>(i)] && par[static_cast<size_t>(i)] < i;
            EXPECT(inc);
        }
    }
    // --- graphs
    {
        auto g = gen::graphs::random(R, 100, 1000);
        EXPECT(g.size() == 1000 && is_simple(100, g));
        auto c = gen::graphs::connected(R, 100, 99);
        EXPECT(is_tree(100, c));
        auto dense = gen::graphs::connected(R, 50, 1200);
        EXPECT(dense.size() == 1200 && is_simple(50, dense));
        auto dag = gen::graphs::dag(R, 100, 500);
        EXPECT(is_simple(100, dag));
        auto b = gen::graphs::bipartite(R, 10, 20, 150);
        bool bi = b.size() == 150 && is_simple(30, b);
        for (auto [u, v] : b) bi &= u <= 10 && v > 10;
        EXPECT(bi);
        EXPECT(gen::graphs::cycle(5).size() == 5 && is_simple(5, gen::graphs::cycle(5)));
        EXPECT(gen::graphs::complete(6).size() == 15);
        EXPECT(gen::graphs::grid(3, 4).size() == 17 && is_simple(12, gen::graphs::grid(3, 4)));
        auto cc = gen::graphs::clique_chain(4, 5);
        EXPECT(cc.size() == 4 * 10 + 3 && is_simple(20, cc));
        auto m = gen::graphs::merge(gen::trees::path(3), 3, gen::trees::path(2));
        EXPECT((m == gen::Edges{{1, 2}, {2, 3}, {4, 5}}));
    }
    // --- relabeling keeps the graph, moves per-vertex values with it
    {
        int n = 6;
        gen::Edges e = gen::trees::star(n);  // center 1
        vector<ll> w = {100, 1, 2, 3, 4, 5}; // w[0] belongs to vertex 1 (the center)
        auto p = gen::relabeling(R, n);
        gen::relabel_edges(e, p);
        gen::relabel_values(w, p);
        auto d = degrees(n, e);
        int center = int(max_element(d.begin(), d.end()) - d.begin());
        EXPECT(w[static_cast<size_t>(center) - 1] == 100);
        EXPECT(is_tree(n, e));
    }
    // --- anti
    {
        string t = gen::anti::thue_morse(8);
        EXPECT(t == "abbabaab");
        EXPECT(gen::anti::unordered_map_keys(3, 7) == (vector<ll>{7, 14, 21}));
    }
    // --- hand-written text
    {
        EXPECT(gen::text(R"(
            3 4
            #..#

              .##.
        )") == "3 4\n#..#\n\n  .##.\n");
        EXPECT(gen::text("1 2\n3\n") == "1 2\n3\n");
        EXPECT(gen::text("\n\t5\n\t6") == "5\n6\n");
    }
    // --- printing
    {
        ostringstream o;
        gen::line(o, 3, "x", 2.5);
        gen::print_line(o, vector<int>{1, 2});
        gen::print_edges(o, gen::WEdges{{1, 2, 9}});
        EXPECT(o.str() == "3 x 2.5\n1 2\n1 2 9\n");
    }

    cout << (failures ? "FAILED: " + to_string(failures) : string("all tests passed")) << "\n";
    return failures ? 1 : 0;
}
