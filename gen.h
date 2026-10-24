// sinol-gen - input generators (ingen) for sinol / SIO2 task packages.
// https://github.com/kubat1234/sinol-gen - documentation and examples. MIT license, (c) Jakub Trela.
//
//   struct Test { int n; gen::Edges edges; };          // optional: the test as data
//   std::ostream& operator<<(std::ostream&, const Test&);
//   Test random_tree(gen::Rng& R, int n) { return {n, gen::trees::random(R, n)}; }
//
//   int main(int argc, char* argv[]) {
//       gen::Generator<Test> g("abc", argc, argv);
//       g.example("3\n1 2\n2 3\n");                    // abc0.in
//       g.group(1);
//       g.test(random_tree, 10) | gen::relabel;        // abc1a.in
//       g.test(random_tree, 1000).check([](const Test& t) { return t.n == 1000; });
//       return g.done();
//   }
//
//   ./abcingen           all tests        ./abcingen 2 2c 1ocen   only these
//   ./abcingen --list    test -> line in the ingen, arguments, seed, size
//
// Every test has its own random generator, seeded from its name ("2c") or from
// .seed(x). The output is the same on every machine and compiler. Needs C++17.

#ifndef SINOL_GEN_H
#define SINOL_GEN_H

#define SINOL_GEN_VERSION "1.0.0"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <climits>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace gen {

using ll = long long;
using ull = unsigned long long;

namespace detail {

inline ull splitmix64(ull& x) {
    ull z = (x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

inline ull fnv1a(const std::string& s) {
    ull h = 0xcbf29ce484222325ULL;
    for (char c : s) h = (h ^ static_cast<unsigned char>(c)) * 0x100000001b3ULL;
    return h;
}

template <class...> inline constexpr bool always_false = false;

template <class X, class = void> struct is_streamable : std::false_type {};
template <class X>
struct is_streamable<X, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const X&>())>>
    : std::true_type {};

}  // namespace detail

// ---------------------------------------------------------------------------
// Random numbers (xoshiro256**)
// ---------------------------------------------------------------------------

class Rng {
  public:
    explicit Rng(ull seed) {
        for (auto& x : s_) x = detail::splitmix64(seed);
    }
    Rng(const Rng&) = delete;
    Rng& operator=(const Rng&) = delete;

    ull u64() {
        ull result = rotl(s_[1] * 5, 7) * 9, t = s_[1] << 17;
        s_[2] ^= s_[0], s_[3] ^= s_[1], s_[1] ^= s_[2], s_[0] ^= s_[3], s_[2] ^= t, s_[3] = rotl(s_[3], 45);
        return result;
    }

    // Uniform integer from [lo, hi], both inclusive, no modulo bias. The result
    // has the common type of lo and hi: range(1, n) is int, range(1, 10'000'000'000LL) is ll.
    template <class A, class B> std::common_type_t<A, B> range(A lo, B hi) {
        static_assert(std::is_integral_v<A> && std::is_integral_v<B>, "range() takes integers");
        using T = std::common_type_t<A, B>;
        T l = static_cast<T>(lo), h = static_cast<T>(hi);
        assert(l <= h);
        ull span = static_cast<ull>(h) - static_cast<ull>(l) + 1;
        if (span == 0) return static_cast<T>(u64());  // the whole 64-bit range
        ull limit = ULLONG_MAX - ULLONG_MAX % span, x;
        do x = u64(); while (x >= limit);
        return static_cast<T>(static_cast<ull>(l) + x % span);
    }

    // Real number from [0, 1) / [lo, hi).
    double real() { return static_cast<double>(u64() >> 11) * 0x1.0p-53; }
    double real(double lo, double hi) { return lo + (hi - lo) * real(); }

    // True with probability num / den.
    bool chance(ll num, ll den) { return range(0LL, den - 1) < num; }

    // Around v: uniform from [v - spread, v]. For "almost maximal" tests: n = R.near(MAXN).
    ll near(ll v, ll spread = 10) { return range(v - spread, v); }

    template <class T> const T& pick(const std::vector<T>& v) {
        assert(!v.empty());
        return v[index(v.size())];
    }

    // Index i with probability w[i] / sum(w).
    size_t weighted(const std::vector<ll>& w) {
        ll total = 0;
        for (ll x : w) assert(x >= 0), total += x;
        assert(total > 0);
        ll r = range(0LL, total - 1);
        size_t i = 0;
        while (r >= w[i]) r -= w[i++];
        return i;
    }

    template <class It> void shuffle(It first, It last) {
        auto n = last - first;
        for (decltype(n) i = n - 1; i > 0; --i) std::iter_swap(first + i, first + range(decltype(n){0}, i));
    }
    template <class T> void shuffle(std::vector<T>& v) { shuffle(v.begin(), v.end()); }
    void shuffle(std::string& s) { shuffle(s.begin(), s.end()); }

    // k elements of v (without repetition), in random order.
    template <class T> std::vector<T> sample(std::vector<T> v, size_t k) {
        assert(k <= v.size());
        for (size_t i = 0; i < k; ++i) std::swap(v[i], v[i + index(v.size() - i)]);
        v.resize(k);
        return v;
    }

    // n values from [lo, hi].
    std::vector<ll> array(int n, ll lo, ll hi) {
        std::vector<ll> v(static_cast<size_t>(n));
        for (auto& x : v) x = range(lo, hi);
        return v;
    }

    // n pairwise distinct values from [lo, hi], in random order.
    std::vector<ll> distinct(int n, ll lo, ll hi) {
        ull span = static_cast<ull>(hi) - static_cast<ull>(lo) + 1;
        assert(n >= 0 && lo <= hi && span >= static_cast<ull>(n));
        std::vector<ll> v;
        if (span <= 4 * static_cast<ull>(n)) {
            for (ll x = lo; x <= hi; ++x) v.push_back(x);
            shuffle(v);
            v.resize(static_cast<size_t>(n));
        } else {
            std::set<ll> seen;
            while (v.size() < static_cast<size_t>(n)) {
                ll x = range(lo, hi);
                if (seen.insert(x).second) v.push_back(x);
            }
        }
        return v;
    }

    // Random permutation of base, base+1, ..., base+n-1.
    std::vector<int> permutation(int n, int base = 1) {
        std::vector<int> p(static_cast<size_t>(n));
        std::iota(p.begin(), p.end(), base);
        shuffle(p);
        return p;
    }

    // k parts, each >= min_part, summing to total.
    std::vector<ll> partition(ll total, int k, ll min_part = 1) {
        assert(k >= 1 && total >= min_part * k);
        ll free = total - min_part * k;
        std::vector<ll> cuts = {0, free};
        for (int i = 0; i < k - 1; ++i) cuts.push_back(range(0LL, free));
        std::sort(cuts.begin(), cuts.end());
        std::vector<ll> parts(static_cast<size_t>(k));
        for (size_t i = 0; i < parts.size(); ++i) parts[i] = cuts[i + 1] - cuts[i] + min_part;
        return parts;
    }

    std::string str(int n, const std::string& alphabet = "abcdefghijklmnopqrstuvwxyz") {
        std::string s(static_cast<size_t>(n), ' ');
        for (auto& c : s) c = alphabet[index(alphabet.size())];
        return s;
    }

    // Uniformly random balanced bracket sequence of length 2n.
    std::string brackets(int n, char open = '(', char close = ')') {
        std::string s = std::string(static_cast<size_t>(n), open) + std::string(static_cast<size_t>(n) + 1, close);
        shuffle(s);
        // cycle lemma: rotate to start right after the first minimum of the prefix sums
        int sum = 0, best = 0;
        size_t start = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            sum += s[i] == open ? 1 : -1;
            if (sum < best) best = sum, start = i + 1;
        }
        std::rotate(s.begin(), s.begin() + static_cast<std::ptrdiff_t>(start), s.end());
        s.pop_back();
        return s;
    }

    size_t index(size_t size) { return static_cast<size_t>(range(0ULL, static_cast<ull>(size) - 1)); }

  private:
    ull s_[4];
    static ull rotl(ull x, int k) { return (x << k) | (x >> (64 - k)); }
};

// ---------------------------------------------------------------------------
// Edges. Vertices are 1..n. Generators return edges in a canonical,
// recognizable labeling - use gen::relabel / gen::shuffle_edges on the test
// (or relabeling() + relabel_edges() by hand) unless the labeling is part of the test.
// ---------------------------------------------------------------------------

using Edges = std::vector<std::pair<int, int>>;
struct WEdge {
    int u, v;
    ll w;
};
using WEdges = std::vector<WEdge>;

inline int& edge_u(std::pair<int, int>& e) { return e.first; }
inline int& edge_v(std::pair<int, int>& e) { return e.second; }
inline int& edge_u(WEdge& e) { return e.u; }
inline int& edge_v(WEdge& e) { return e.v; }

// Edges with random weights from [lo, hi].
inline WEdges with_weights(Rng& R, const Edges& e, ll lo, ll hi) {
    WEdges r;
    for (auto [u, v] : e) r.push_back({u, v, R.range(lo, hi)});
    return r;
}

// Random relabeling: p[v] = new label of v (p[0] unused).
inline std::vector<int> relabeling(Rng& R, int n) {
    std::vector<int> p = R.permutation(n);
    p.insert(p.begin(), 0);
    return p;
}

template <class E> void relabel_edges(std::vector<E>& edges, const std::vector<int>& p) {
    for (auto& e : edges) edge_u(e) = p[static_cast<size_t>(edge_u(e))], edge_v(e) = p[static_cast<size_t>(edge_v(e))];
}

// Moves per-vertex values along with a relabeling: values indexed 1..n
// (size n + 1) or 0..n-1 (size n, index i = vertex i + 1).
template <class T> void relabel_values(std::vector<T>& values, const std::vector<int>& p) {
    size_t n = p.size() - 1, off = values.size() == n + 1 ? 0 : 1;
    assert(values.size() == n + 1 || values.size() == n);
    std::vector<T> r = values;
    for (size_t v = 1; v <= n; ++v) r[static_cast<size_t>(p[v]) - off] = values[v - off];
    values = std::move(r);
}

namespace trees {

// Uniformly random labeled tree (Pruefer sequence).
inline Edges random(Rng& R, int n) {
    Edges e;
    if (n <= 1) return e;
    if (n == 2) return {{1, 2}};
    std::vector<int> code(static_cast<size_t>(n) - 2), deg(static_cast<size_t>(n) + 1, 1);
    for (auto& c : code) c = R.range(1, n), deg[static_cast<size_t>(c)]++;
    int ptr = 1;
    while (deg[static_cast<size_t>(ptr)] != 1) ptr++;
    int leaf = ptr;
    for (int v : code) {
        e.push_back({leaf, v});
        if (--deg[static_cast<size_t>(v)] == 1 && v < ptr) {
            leaf = v;
        } else {
            ptr++;
            while (deg[static_cast<size_t>(ptr)] != 1) ptr++;
            leaf = ptr;
        }
    }
    e.push_back({leaf, n});
    return e;
}

// Parent of i is uniform in [max(1, i - k), i - 1]. Small k: long, path-like
// trees (depth ~ 2n / k); k = n: random recursive tree (depth ~ log n).
inline Edges deep(Rng& R, int n, int k) {
    Edges e;
    for (int i = 2; i <= n; ++i) e.push_back({R.range(std::max(1, i - k), i - 1), i});
    return e;
}
inline Edges recursive(Rng& R, int n) { return deep(R, n, n); }

inline Edges path(int n) {
    Edges e;
    for (int i = 2; i <= n; ++i) e.push_back({i - 1, i});
    return e;
}

inline Edges star(int n) {
    Edges e;
    for (int i = 2; i <= n; ++i) e.push_back({1, i});
    return e;
}

// Complete binary tree in heap order.
inline Edges binary(int n) {
    Edges e;
    for (int i = 2; i <= n; ++i) e.push_back({i / 2, i});
    return e;
}

// Random binary tree: every vertex has at most 2 children (root 1).
inline Edges binary_random(Rng& R, int n) {
    Edges e;
    std::vector<int> open = {1, 1};  // one entry per free child slot
    for (int i = 2; i <= n; ++i) {
        size_t j = R.index(open.size());
        e.push_back({open[j], i});
        open[j] = open.back(), open.pop_back();
        open.push_back(i), open.push_back(i);
    }
    return e;
}

// Random tree with every degree <= d (d >= 2).
inline Edges max_degree(Rng& R, int n, int d) {
    assert(d >= 2 || n <= 2);
    Edges e;
    std::vector<int> deg(static_cast<size_t>(n) + 1, 0), open = {1};
    for (int i = 2; i <= n; ++i) {
        size_t j = R.index(open.size());
        int p = open[j];
        e.push_back({p, i});
        if (++deg[static_cast<size_t>(p)] == d) open[j] = open.back(), open.pop_back();
        if (++deg[static_cast<size_t>(i)] < d) open.push_back(i);
    }
    return e;
}

// Random tree of depth exactly `depth` (root 1): path 1..depth+1, the rest
// attached to random vertices of depth < depth.
inline Edges with_depth(Rng& R, int n, int depth) {
    assert(1 <= depth && depth < n);
    Edges e = path(depth + 1);
    std::vector<int> open;
    for (int v = 1; v <= depth; ++v) open.push_back(v);
    std::vector<int> dep(static_cast<size_t>(n) + 1, 0);
    for (int v = 1; v <= depth + 1; ++v) dep[static_cast<size_t>(v)] = v - 1;
    for (int i = depth + 2; i <= n; ++i) {
        int p = R.pick(open);
        e.push_back({p, i});
        dep[static_cast<size_t>(i)] = dep[static_cast<size_t>(p)] + 1;
        if (dep[static_cast<size_t>(i)] < depth) open.push_back(i);
    }
    return e;
}

// Spine 1..spine, the remaining vertices hang off random spine vertices.
inline Edges caterpillar(Rng& R, int n, int spine) {
    assert(1 <= spine && spine <= n);
    Edges e = path(spine);
    for (int i = spine + 1; i <= n; ++i) e.push_back({R.range(1, spine), i});
    return e;
}

// Path 1..handle, the rest attached to vertex `handle`.
inline Edges broom(int n, int handle) {
    assert(1 <= handle && handle <= n);
    Edges e = path(handle);
    for (int i = handle + 1; i <= n; ++i) e.push_back({handle, i});
    return e;
}

// k paths ("legs") of length ~n/k hanging from vertex 1.
inline Edges spider(int n, int k) {
    Edges e;
    for (int i = 2; i <= n; ++i) e.push_back({i - 1 <= k ? 1 : i - k, i});
    return e;
}

// Parent array for a tree rooted at `root`, relabeled so that parent[i] < i
// (the "p_2 ... p_n" input format). Children are visited in random order.
// Returns a vector of size n + 1; entries 2..n are meaningful.
inline std::vector<int> parents_increasing(Rng& R, const Edges& e, int n, int root = 1) {
    std::vector<std::vector<int>> adj(static_cast<size_t>(n) + 1);
    for (auto [u, v] : e) adj[static_cast<size_t>(u)].push_back(v), adj[static_cast<size_t>(v)].push_back(u);
    std::vector<int> label(static_cast<size_t>(n) + 1, 0), par(static_cast<size_t>(n) + 1, 0), order = {root};
    label[static_cast<size_t>(root)] = 1;
    for (size_t i = 0; i < order.size(); ++i) {
        int v = order[i];
        R.shuffle(adj[static_cast<size_t>(v)]);
        for (int u : adj[static_cast<size_t>(v)])
            if (!label[static_cast<size_t>(u)]) {
                label[static_cast<size_t>(u)] = static_cast<int>(order.size()) + 1;
                par[static_cast<size_t>(label[static_cast<size_t>(u)])] = label[static_cast<size_t>(v)];
                order.push_back(u);
            }
    }
    assert(static_cast<int>(order.size()) == n);
    return par;
}

}  // namespace trees

// ---------------------------------------------------------------------------
// Graphs (simple: no loops, no multi-edges)
// ---------------------------------------------------------------------------

namespace graphs {

inline ll max_edges(int n) { return static_cast<ll>(n) * (n - 1) / 2; }

// Adds random new edges to `e` until it has m edges.
inline Edges fill(Rng& R, Edges e, int n, ll m) {
    assert(m <= max_edges(n) && static_cast<ll>(e.size()) <= m);
    std::set<std::pair<int, int>> have;
    for (auto [u, v] : e) have.insert({std::min(u, v), std::max(u, v)});
    if (m * 2 > max_edges(n)) {  // dense: enumerate all pairs
        Edges rest;
        for (int u = 1; u <= n; ++u)
            for (int v = u + 1; v <= n; ++v)
                if (!have.count({u, v})) rest.push_back({u, v});
        R.shuffle(rest);
        for (size_t i = 0; static_cast<ll>(e.size()) < m; ++i) e.push_back(rest[i]);
    } else {
        while (static_cast<ll>(e.size()) < m) {
            int u = R.range(1, n), v = R.range(1, n);
            if (u == v) continue;
            if (u > v) std::swap(u, v);
            if (have.insert({u, v}).second) e.push_back({u, v});
        }
    }
    return e;
}

// Random simple graph, maybe disconnected (random labels and edge order).
inline Edges random(Rng& R, int n, ll m) {
    Edges e = fill(R, {}, n, m);
    R.shuffle(e);
    return e;
}

// Random simple connected graph (random tree + random extra edges).
inline Edges connected(Rng& R, int n, ll m) {
    assert(m >= n - 1);
    Edges e = fill(R, trees::random(R, n), n, m);
    R.shuffle(e);
    return e;
}

// Random DAG: (u, v) means u -> v; edges respect a hidden random order.
inline Edges dag(Rng& R, int n, ll m) {
    Edges e = fill(R, {}, n, m);  // u < v
    std::vector<int> p = relabeling(R, n);
    relabel_edges(e, p);
    R.shuffle(e);
    return e;
}

// Random bipartite graph: parts 1..a and a+1..a+b, m edges.
inline Edges bipartite(Rng& R, int a, int b, ll m) {
    assert(m <= static_cast<ll>(a) * b);
    std::vector<ll> ids = R.distinct(static_cast<int>(m), 0, static_cast<ll>(a) * b - 1);
    Edges e;
    for (ll id : ids) e.push_back({static_cast<int>(id / b) + 1, a + static_cast<int>(id % b) + 1});
    return e;
}

inline Edges cycle(int n) {
    Edges e = trees::path(n);
    if (n >= 3) e.push_back({n, 1});
    return e;
}

inline Edges complete(int n) {
    Edges e;
    for (int u = 1; u <= n; ++u)
        for (int v = u + 1; v <= n; ++v) e.push_back({u, v});
    return e;
}

// r x c grid, vertex (i, j) = (i - 1) * c + j.
inline Edges grid(int r, int c) {
    Edges e;
    for (int i = 1; i <= r; ++i)
        for (int j = 1; j <= c; ++j) {
            int v = (i - 1) * c + j;
            if (j < c) e.push_back({v, v + 1});
            if (i < r) e.push_back({v, v + c});
        }
    return e;
}

// k cliques of size s joined into a chain by single edges (bridges, articulation points).
inline Edges clique_chain(int k, int s) {
    Edges e;
    for (int i = 0; i < k; ++i) {
        for (auto [u, v] : complete(s)) e.push_back({u + i * s, v + i * s});
        if (i > 0) e.push_back({i * s, i * s + 1});
    }
    return e;
}

// Disjoint union: b's vertices become na+1 .. na+nb.
inline Edges merge(Edges a, int na, const Edges& b) {
    for (auto [u, v] : b) a.push_back({u + na, v + na});
    return a;
}

}  // namespace graphs

// ---------------------------------------------------------------------------
// Tests against typical shortcuts
// ---------------------------------------------------------------------------

namespace anti {

// Thue-Morse word: breaks polynomial hashing modulo 2^64 (unsigned overflow)
// for any base. Use n >= 2^11 or so.
inline std::string thue_morse(int n, char a = 'a', char b = 'b') {
    std::string s(static_cast<size_t>(n), a);
    for (size_t i = 0; i < s.size(); ++i)
        if (__builtin_popcountll(static_cast<ull>(i)) % 2) s[i] = b;
    return s;
}

// Keys that make std::unordered_map of libstdc++ (gcc 7+) degrade to O(n) per
// operation: multiples of one of its bucket counts. 107897 works for ~10^5
// elements; for other sizes / gcc versions try 172933, 126271.
inline std::vector<ll> unordered_map_keys(int n, ll prime = 107897) {
    std::vector<ll> k;
    for (int i = 1; i <= n; ++i) k.push_back(i * prime);
    return k;
}

}  // namespace anti

// ---------------------------------------------------------------------------
// Modifiers: g.test(...) | gen::relabel | gen::shuffle_edges
// They work on any test struct with `n` and `edges` (Edges or WEdges). Per-vertex
// data in the struct is NOT moved by relabel - write your own modifier with
// relabeling() + relabel_edges() + relabel_values() then.
// ---------------------------------------------------------------------------

struct Relabel {
    template <class E> void operator()(Rng& R, std::vector<E>& edges, int n) const { relabel_edges(edges, relabeling(R, n)); }
    template <class T> auto operator()(Rng& R, T& t) const -> decltype((void)t.n, (void)t.edges, void()) {
        relabel_edges(t.edges, relabeling(R, static_cast<int>(t.n)));
    }
};
inline constexpr Relabel relabel{};

struct ShuffleEdges {
    template <class E> void operator()(Rng& R, std::vector<E>& edges) const { R.shuffle(edges); }
    template <class T> auto operator()(Rng& R, T& t) const -> decltype((void)t.edges, void()) { R.shuffle(t.edges); }
};
inline constexpr ShuffleEdges shuffle_edges{};

// Random orientation of every edge.
struct FlipEdges {
    template <class E> void operator()(Rng& R, std::vector<E>& edges) const {
        for (auto& e : edges)
            if (R.chance(1, 2)) std::swap(edge_u(e), edge_v(e));
    }
    template <class T> auto operator()(Rng& R, T& t) const -> decltype((void)t.edges, void()) { (*this)(R, t.edges); }
};
inline constexpr FlipEdges flip_edges{};

// ---------------------------------------------------------------------------
// Random arguments of g.test: drawn from the test's own generator, left to
// right, when the test is generated. The function gets a plain value.
//     g.test(random_arr, gen::near(MAXN), MAXV);        n from [MAXN - 10, MAXN]
//     g.test(random_arr, gen::between(1000, MAXN), MAXV);
//     g.test(random_arr, gen::one_of({1, 2, MAXN}), MAXV);
// ---------------------------------------------------------------------------

template <class T> struct Between {
    T lo, hi;
    friend std::ostream& operator<<(std::ostream& o, const Between& b) { return o << "between(" << b.lo << ", " << b.hi << ")"; }
};
template <class T> struct OneOf {
    std::vector<T> v;
    friend std::ostream& operator<<(std::ostream& o, const OneOf& x) {
        o << "one_of(";
        for (size_t i = 0; i < x.v.size(); ++i) o << (i ? ", " : "") << x.v[i];
        return o << ")";
    }
};

// Any value from [lo, hi]; the type is the common type of lo and hi, as in Rng::range.
template <class A, class B> Between<std::common_type_t<A, B>> between(A lo, B hi) {
    using T = std::common_type_t<A, B>;
    assert(static_cast<T>(lo) <= static_cast<T>(hi));
    return {static_cast<T>(lo), static_cast<T>(hi)};
}
// From [v - spread, v]: an almost maximal value.
template <class T, class S = T> Between<T> near(T v, S spread = 10) {
    static_assert(std::is_integral_v<T> && std::is_integral_v<S>, "near() takes integers");
    return {static_cast<T>(v - spread), v};
}
// One of the given values.
template <class T> OneOf<T> one_of(std::initializer_list<T> v) {
    assert(v.size() > 0);
    return {std::vector<T>(v)};
}

namespace detail {
template <class X> struct Drawn { using type = X&; };  // an ordinary argument is passed as it is
template <class T> struct Drawn<Between<T>> { using type = T; };
template <class T> struct Drawn<OneOf<T>> { using type = T; };
template <class X> using drawn_t = typename Drawn<std::decay_t<X>>::type;
template <class X> X& draw(Rng&, X& x) { return x; }
template <class T> T draw(Rng& R, Between<T>& b) { return R.range(b.lo, b.hi); }
template <class T> T draw(Rng& R, OneOf<T>& o) { return R.pick(o.v); }
}  // namespace detail

// ---------------------------------------------------------------------------
// Hand-written tests
// ---------------------------------------------------------------------------

// A multi-line test written in the code as a raw string, indented with the code:
//     g.test(gen::text(R"(
//         3 4
//         #..#
//     )"));                       -> "3 4\n#..#\n"
// Drops the first line if it is empty and the last one if it is only spaces,
// then the indentation common to all non-empty lines.
inline std::string text(const std::string& raw) {
    std::vector<std::string> lines;
    std::istringstream in(raw);
    for (std::string l; std::getline(in, l);) lines.push_back(l);
    auto blank = [](const std::string& l) { return l.find_first_not_of(" \t\r") == std::string::npos; };
    if (!lines.empty() && lines.front().empty()) lines.erase(lines.begin());
    if (!lines.empty() && blank(lines.back()) && (raw.empty() || raw.back() != '\n')) lines.pop_back();
    size_t indent = std::string::npos;
    for (auto& l : lines)
        if (!blank(l)) indent = std::min(indent, l.find_first_not_of(" \t"));
    std::string r;
    for (auto& l : lines) r += (blank(l) ? "" : l.substr(indent)) + "\n";
    return r;
}

// ---------------------------------------------------------------------------
// Printing
// ---------------------------------------------------------------------------

// Arguments separated by spaces, then '\n': gen::line(out, n, m).
template <class... A> void line(std::ostream& out, const A&... a) {
    const char* sep = "";
    ((out << sep << a, sep = " "), ...);
    out << '\n';
}

template <class T> void print_line(std::ostream& out, const std::vector<T>& v) {
    for (size_t i = 0; i < v.size(); ++i) out << (i ? " " : "") << v[i];
    out << '\n';
}

inline void print_edges(std::ostream& out, const Edges& e) {
    for (auto [u, v] : e) out << u << ' ' << v << '\n';
}
inline void print_edges(std::ostream& out, const WEdges& e) {
    for (auto& x : e) out << x.u << ' ' << x.v << ' ' << x.w << '\n';
}

template <class T> void print_matrix(std::ostream& out, const std::vector<std::vector<T>>& m) {
    for (auto& row : m) print_line(out, row);
}

// ---------------------------------------------------------------------------
// Test list
// ---------------------------------------------------------------------------

namespace detail {

// Line of the caller, captured by a default argument. `explicit`, so that a
// test argument can never be taken for it.
struct Here {
    int line;
    explicit Here(int l) : line(l) {}
};

template <class X> std::string show(const X& x) {
    if constexpr (std::is_convertible_v<const X&, std::string>) {
        std::string s = x;
        return s.size() > 20 ? "\"" + s.substr(0, 17) + "...\"" : "\"" + s + "\"";
    } else if constexpr (is_streamable<X>::value) {
        std::ostringstream o;
        o << x;
        return o.str();
    } else {
        return "?";
    }
}

inline std::string human_size(size_t b) {
    char buf[32];
    if (b < 1024) std::snprintf(buf, sizeof buf, "%zu B", b);
    else if (b < 1024 * 1024) std::snprintf(buf, sizeof buf, "%.1f KB", static_cast<double>(b) / 1024);
    else std::snprintf(buf, sizeof buf, "%.1f MB", static_cast<double>(b) / 1024 / 1024);
    return buf;
}

inline char crash_info[256];
inline void on_abort(int) {
    std::fputs(crash_info, stderr);
    std::signal(SIGABRT, SIG_DFL);
    std::raise(SIGABRT);
}

struct NoValue {};

}  // namespace detail

// Checks the finished test: g.test(...) | gen::check(pred, "what").
template <class P> struct Check {
    P pred;
    std::string what;
    int line;
};
template <class P> Check<P> check(P pred, std::string what = "", detail::Here h = detail::Here(__builtin_LINE())) {
    return {std::move(pred), std::move(what), h.line};
}

// T: the test struct (printed with operator<<), or void when every test is
// written to the stream directly.
template <class T = void> class Generator {
    using Value = std::conditional_t<std::is_void_v<T>, detail::NoValue, T>;
    using Mod = std::function<void(Rng&, Value&)>;
    struct CheckFn {
        std::function<bool(const Value&)> pred;
        std::string what;
        int line;
    };
    struct Rules {
        std::vector<Mod> mods;
        std::vector<CheckFn> checks;
    };
    struct Entry {
        std::string name, what;
        int group = 0, line = 0, example = -1;  // example: index among g.example()s
        bool has_seed = false;
        ull seed = 0;
        std::function<void(Rng&, std::ostream&)> write;  // text and stream tests
        std::function<Value(Rng&)> make;                 // tests made as T
        Rules rules;
    };

  public:
    Generator(std::string id, int argc, char* argv[], const char* file = __builtin_FILE())
        : id_(std::move(id)), args_(argv + 1, argv + argc), file_(file) {
        file_ = file_.substr(file_.find_last_of("/\\") + 1);
    }
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    ~Generator() {
        if (!done_) std::cerr << "gen: g.done() was not called, no tests were written\n";
    }

    // Handle of one test: .then(f) / | f, .check(p) / | gen::check(p), .seed(x).
    class Ref {
      public:
        template <class F> Ref& then(F f) {
            static_assert(!std::is_void_v<T>, "modifiers need the test struct: gen::Generator<Test>");
            g_->require_value(i_);
            e().rules.mods.push_back(to_mod(std::move(f)));
            return *this;
        }
        template <class P> Ref& check(P pred, std::string what = "", detail::Here h = detail::Here(__builtin_LINE())) {
            static_assert(!std::is_void_v<T>, "checks need the test struct: gen::Generator<Test>");
            g_->require_value(i_);
            e().rules.checks.push_back({to_pred(std::move(pred)), std::move(what), h.line});
            return *this;
        }
        // A fixed seed instead of the one from the test name: re-rolls a bad test,
        // and the same call gives the same test in any package.
        Ref& seed(ull s) {
            e().has_seed = true, e().seed = s;
            return *this;
        }
        template <class P> friend Ref operator|(Ref r, Check<P> c) { return r.check(std::move(c.pred), std::move(c.what), detail::Here(c.line)); }
        template <class F> friend Ref operator|(Ref r, F f) { return r.then(std::move(f)); }

      private:
        friend class Generator;
        Ref(Generator* g, size_t i) : g_(g), i_(i) {}
        Entry& e() { return g_->entries_[i_]; }
        Generator* g_;
        size_t i_;
    };

    // Modifiers and checks for every test of a group: g.group(3) | gen::relabel.
    class Group {
      public:
        template <class F> Group& then(F f) {
            static_assert(!std::is_void_v<T>, "modifiers need the test struct: gen::Generator<Test>");
            rules().mods.push_back(to_mod(std::move(f)));
            return *this;
        }
        template <class P> Group& check(P pred, std::string what = "", detail::Here h = detail::Here(__builtin_LINE())) {
            static_assert(!std::is_void_v<T>, "checks need the test struct: gen::Generator<Test>");
            rules().checks.push_back({to_pred(std::move(pred)), std::move(what), h.line});
            return *this;
        }
        template <class P> friend Group operator|(Group gr, Check<P> c) { return gr.check(std::move(c.pred), std::move(c.what), detail::Here(c.line)); }
        template <class F> friend Group operator|(Group gr, F f) { return gr.then(std::move(f)); }

      private:
        friend class Generator;
        Group(Generator* g, int k) : g_(g), k_(k) {}
        Rules& rules() { return g_->groups_[k_]; }
        Generator* g_;
        int k_;
    };

    // The tests below go to group k: abc<k>a.in, abc<k>b.in, ...
    Group group(int k) {
        assert(k >= 1);
        current_ = k;
        return Group(this, k);
    }

  private:
    enum Kind { EXAMPLE, OCEN, TEST };

    // g.test / g.example / g.ocen: one function object per kind, so that the
    // overloads (and the caller's line) are written only once.
    template <Kind K> class Adder {
      public:
        explicit Adder(Generator* g) : g_(g) {}
#define SINOL_GEN_H_ ::gen::detail::Here here_ = ::gen::detail::Here(__builtin_LINE())
        template <class X> Ref operator()(X&& x, SINOL_GEN_H_) { return g_->add(K, here_.line, std::forward<X>(x)); }
        // g.test({"3", "1 2 3"}): one string per line
        Ref operator()(std::initializer_list<std::string> lines, SINOL_GEN_H_) {
            std::string s;
            for (auto& l : lines) s += l + "\n";
            return g_->add(K, here_.line, std::move(s));
        }
        template <class X, class A1> Ref operator()(X&& x, A1&& a1, SINOL_GEN_H_) {
            return g_->add(K, here_.line, std::forward<X>(x), std::forward<A1>(a1));
        }
        template <class X, class A1, class A2> Ref operator()(X&& x, A1&& a1, A2&& a2, SINOL_GEN_H_) {
            return g_->add(K, here_.line, std::forward<X>(x), std::forward<A1>(a1), std::forward<A2>(a2));
        }
        template <class X, class A1, class A2, class A3> Ref operator()(X&& x, A1&& a1, A2&& a2, A3&& a3, SINOL_GEN_H_) {
            return g_->add(K, here_.line, std::forward<X>(x), std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3));
        }
        template <class X, class A1, class A2, class A3, class A4>
        Ref operator()(X&& x, A1&& a1, A2&& a2, A3&& a3, A4&& a4, SINOL_GEN_H_) {
            return g_->add(K, here_.line, std::forward<X>(x), std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
                           std::forward<A4>(a4));
        }
        template <class X, class A1, class A2, class A3, class A4, class A5>
        Ref operator()(X&& x, A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, SINOL_GEN_H_) {
            return g_->add(K, here_.line, std::forward<X>(x), std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
                           std::forward<A4>(a4), std::forward<A5>(a5));
        }
        template <class X, class A1, class A2, class A3, class A4, class A5, class A6>
        Ref operator()(X&& x, A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6, SINOL_GEN_H_) {
            return g_->add(K, here_.line, std::forward<X>(x), std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
                           std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6));
        }
        template <class X, class A1, class A2, class A3, class A4, class A5, class A6, class A7>
        Ref operator()(X&& x, A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6, A7&& a7, SINOL_GEN_H_) {
            return g_->add(K, here_.line, std::forward<X>(x), std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
                           std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6), std::forward<A7>(a7));
        }
        template <class X, class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8>
        Ref operator()(X&& x, A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6, A7&& a7, A8&& a8, SINOL_GEN_H_) {
            return g_->add(K, here_.line, std::forward<X>(x), std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
                           std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6), std::forward<A7>(a7),
                           std::forward<A8>(a8));
        }
#undef SINOL_GEN_H_

      private:
        Generator* g_;
    };

  public:
    // Test in the current group. x is one of:
    //   "text"                  the test's content (a missing final \n is added)
    //   {"line", "line", ...}   one string per line
    //   gen::text(R"(...)")     indented multi-line text, see gen::text
    //   Test{...}               a test struct (with Generator<Test>)
    //   f, args...              f(gen::Rng&, args...) -> Test, or -> anything printable,
    //                           or f(gen::Rng&, std::ostream&, args...) writing the test
    Adder<TEST> test{this};
    // Statement examples: abc0.in if there is one, abc0a.in, abc0b.in, ... if more (same arguments as test).
    Adder<EXAMPLE> example{this};
    // Public tests abc1ocen.in, abc2ocen.in, ... (same arguments as test).
    Adder<OCEN> ocen{this};

    // Fails if any test is bigger than mb megabytes.
    void max_size_mb(double mb) { max_bytes_ = static_cast<size_t>(mb * 1024 * 1024); }

    // Writes the tests (or lists them). Return it from main().
    int done() {
        done_ = true;
        for (auto& e : entries_)  // one example: abc0.in; more: abc0a.in, abc0b.in, ...
            if (e.example >= 0) e.name = count_[0] == 1 ? "0" : "0" + letters(e.example);
        bool list = false;
        std::set<std::string> only;
        for (auto& a : args_) {
            if (a == "--list" || a == "-l") list = true;
            else if (a == "--help" || a == "-h") return help();
            else only.insert(a);
        }
        for (auto& o : only)
            if (!selected_by(o)) std::cerr << "gen: no test or group " << o << "\n", errors_++;
        std::signal(SIGABRT, detail::on_abort);
        std::map<std::string, std::string> seen;  // content -> name
        size_t written = 0;
        if (list) std::printf("%-10s %-22s %-18s %10s %8s  %s\n", "test", "line", "seed", "size", "time", "arguments");
        for (auto& e : entries_) {
            if (!only.empty() && !std::any_of(only.begin(), only.end(), [&](auto& o) { return matches(e, o); })) continue;
            std::snprintf(detail::crash_info, sizeof detail::crash_info, "gen: crashed while generating %s%s (%s:%d)\n",
                          id_.c_str(), e.name.c_str(), file_.c_str(), e.line);
            auto t0 = std::chrono::steady_clock::now();
            std::string content;
            shown_.clear();
            try {
                content = generate(e);
            } catch (std::exception& ex) {
                error(e, std::string("exception: ") + ex.what());
                continue;
            }
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            if (auto [it, fresh] = seen.insert({content, e.name}); !fresh) error(e, "identical to " + id_ + it->second);
            if (max_bytes_ && content.size() > max_bytes_) error(e, "bigger than the limit: " + detail::human_size(content.size()));
            if (list) {
                std::printf("%-10s %-22s %-18s %10s %6.0fms  %s\n", (id_ + e.name).c_str(),
                            (file_ + ":" + std::to_string(e.line)).c_str(), seed_text(e).c_str(),
                            detail::human_size(content.size()).c_str(), ms,
                            (shown_.empty() ? e.what : "(" + shown_ + ")").c_str());
            } else {
                std::ofstream(id_ + e.name + ".in", std::ios::binary) << content;
                written++;
            }
        }
        if (!list) std::cout << "gen: " << written << " tests written\n";
        if (errors_) std::cerr << "gen: " << errors_ << " error(s)\n";
        return errors_ ? 1 : 0;
    }

  private:
    std::string id_;
    std::vector<std::string> args_;
    std::string file_;
    std::vector<Entry> entries_;
    std::map<int, Rules> groups_;
    std::map<int, int> count_;  // group -> tests so far (0 = examples, -1 = ocen)
    int current_ = 0, errors_ = 0;
    size_t max_bytes_ = 0;
    bool done_ = false;
    std::string shown_;  // the arguments of the last generated test, random ones drawn

    template <class... A> Ref add(Kind kind, int line, A&&... a) {
        Entry e;
        e.line = line;
        if (kind == TEST) {
            if (current_ == 0) {
                std::cerr << file_ << ":" << line << ": gen: g.test() before g.group(k)\n";
                std::abort();
            }
            e.group = current_;
            e.name = std::to_string(current_) + letters(count_[current_]++);
        } else if (kind == EXAMPLE) {
            e.example = count_[0]++;  // named in done(), when the number of examples is known
        } else {
            e.name = std::to_string(++count_[-1]) + "ocen";
        }
        set_maker(e, std::forward<A>(a)...);
        entries_.push_back(std::move(e));
        return Ref(this, entries_.size() - 1);
    }

    template <class X, class... A> void set_maker(Entry& e, X&& x, A&&... a) {
        using D = std::decay_t<X>;
        if constexpr (sizeof...(A) == 0 && std::is_convertible_v<X, std::string>) {
            std::string text = std::forward<X>(x);
            if (!text.empty() && text.back() != '\n') text += '\n';  // every line of a test ends with \n
            if (text.size() >= 2 && text.compare(text.size() - 2, 2, "\n\n") == 0)
                std::cerr << file_ << ":" << e.line << ": gen: warning: the test ends with an empty line\n";
            e.what = "text";
            e.write = [text](Rng&, std::ostream& out) { out << text; };
        } else if constexpr (sizeof...(A) == 0 && !std::is_void_v<T> && std::is_same_v<D, Value>) {
            Value v = std::forward<X>(x);
            e.what = "value";
            e.make = [v](Rng&) { return v; };
        } else {
            static_assert(std::is_invocable_v<D&, Rng&, std::ostream&, detail::drawn_t<A>...> ||
                              std::is_invocable_v<D&, Rng&, detail::drawn_t<A>...>,
                          "g.test(f, args...): f must be f(gen::Rng&, args...) or f(gen::Rng&, std::ostream&, args...)");
            e.what = "(" + describe(a...) + ")";
            auto call = [this, f = D(std::forward<X>(x)), args = std::make_tuple(std::decay_t<A>(std::forward<A>(a))...)](
                            Rng& R, auto&... extra) mutable {
                auto copy = args;
                return std::apply(
                    [&](auto&... v) {
                        // braces: random arguments are drawn left to right on every compiler
                        std::tuple<detail::drawn_t<decltype(v)>...> drawn{detail::draw(R, v)...};
                        shown_ = std::apply([](auto&... d) { return describe(d...); }, drawn);
                        return std::apply([&](auto&... d) { return f(R, extra..., d...); }, drawn);
                    },
                    copy);
            };
            if constexpr (std::is_invocable_v<D&, Rng&, std::ostream&, detail::drawn_t<A>...>) {
                e.write = [call](Rng& R, std::ostream& out) mutable { call(R, out); };
            } else {
                using Res = std::invoke_result_t<D&, Rng&, detail::drawn_t<A>...>;
                if constexpr (!std::is_void_v<T> && std::is_same_v<std::decay_t<Res>, Value>) {
                    e.make = [call](Rng& R) mutable -> Value { return call(R); };
                } else {
                    static_assert(detail::is_streamable<Res>::value,
                                  "g.test(f, args...): f returns something that cannot be printed with <<");
                    e.write = [call](Rng& R, std::ostream& out) mutable { out << call(R); };
                }
            }
        }
    }

    template <class... A> static std::string describe(const A&... a) {
        std::string s;
        ((s += (s.empty() ? "" : ", ") + detail::show(a)), ...);
        return s;
    }

    template <class F> static Mod to_mod(F f) {
        if constexpr (std::is_invocable_v<F&, Rng&, Value&>) {
            return [f](Rng& R, Value& v) mutable { f(R, v); };
        } else if constexpr (std::is_invocable_v<F&, Value&>) {
            return [f](Rng&, Value& v) mutable { f(v); };
        } else {
            static_assert(detail::always_false<F>, "modifier must be f(gen::Rng&, Test&) or f(Test&)");
            return {};
        }
    }

    template <class P> static std::function<bool(const Value&)> to_pred(P p) {
        static_assert(std::is_invocable_r_v<bool, P&, const Value&>, "check must be bool f(const Test&)");
        return [p](const Value& v) mutable { return static_cast<bool>(p(v)); };
    }

    void require_value(size_t i) {
        if (!entries_[i].make) {
            std::cerr << file_ << ":" << entries_[i].line << ": gen: modifiers and checks work only on tests made as "
                      << "the test struct (not on text or stream tests)\n";
            std::abort();
        }
    }

    std::string generate(Entry& e) {
        Rng R(e.has_seed ? e.seed : detail::fnv1a(e.name));
        std::ostringstream out;
        if constexpr (!std::is_void_v<T>) {
            if (e.make) {
                Value v = e.make(R);
                auto it = groups_.find(e.group);
                for (auto& m : e.rules.mods) m(R, v);
                if (it != groups_.end())
                    for (auto& m : it->second.mods) m(R, v);
                for (auto& c : e.rules.checks) run_check(e, c, v);
                if (it != groups_.end())
                    for (auto& c : it->second.checks) run_check(e, c, v);
                out << v;
                return out.str();
            }
        }
        e.write(R, out);
        return out.str();
    }

    void run_check(const Entry& e, const CheckFn& c, const Value& v) {
        if (!c.pred(v)) error(e, "check failed" + (c.what.empty() ? "" : ": " + c.what) + " (check at line " + std::to_string(c.line) + ")");
    }

    void error(const Entry& e, const std::string& msg) {
        std::cerr << file_ << ":" << e.line << ": " << id_ << e.name << ": " << msg << "\n";
        errors_++;
    }

    std::string seed_text(const Entry& e) const {
        char buf[32];
        std::snprintf(buf, sizeof buf, e.has_seed ? "%llu" : "name", e.has_seed ? e.seed : 0ULL);
        return buf;
    }

    // "2" = group 2, "0" = the examples, "ocen" = all ocen tests, "2c" / "0a" / "1ocen" = one test
    static bool matches(const Entry& e, const std::string& o) {
        if (o == e.name || (o == "ocen" && e.name.size() > 4 && e.name.compare(e.name.size() - 4, 4, "ocen") == 0)) return true;
        bool is_ocen = e.name.find("ocen") != std::string::npos;
        return !is_ocen && std::all_of(o.begin(), o.end(), ::isdigit) && std::to_string(e.group) == o;
    }
    bool selected_by(const std::string& o) const {
        return std::any_of(entries_.begin(), entries_.end(), [&](auto& e) { return matches(e, o); });
    }

    int help() const {
        std::cout << "usage: ./" << id_ << "ingen [--list] [group | test ...]\n"
                  << "  no arguments   write all tests to the current directory\n"
                  << "  2 2c 1ocen 0   only group 2, test 2c, test 1ocen, the examples\n"
                  << "  ocen           all ocen tests\n"
                  << "  --list         where each test comes from (line in the ingen), its size and time\n";
        return 0;
    }

    // 0 -> a, 25 -> z, 26 -> aa, ...
    static std::string letters(int i) {
        std::string s;
        for (i++; i > 0; i = (i - 1) / 26) s.insert(s.begin(), static_cast<char>('a' + (i - 1) % 26));
        return s;
    }
};

}  // namespace gen

#endif  // SINOL_GEN_H
