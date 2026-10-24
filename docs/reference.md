# gen.h reference

Everything is in namespace `gen`; `ll` = `long long`. Vertices are 1..n.
How to use it in an ingen: [guide.md](guide.md).

## Random numbers — `gen::Rng R`

| | |
|---|---|
| `R.range(lo, hi)` | integer from [lo, hi]; the result has the common type of the arguments: `range(1, n)` is `int`, `range(1, 10'000'000'000LL)` is `ll` |
| `R.real()`, `R.real(lo, hi)` | real number from [0, 1) / [lo, hi) |
| `R.chance(a, b)` | true with probability a / b |
| `R.near(v, spread = 10)` | from [v − spread, v] — for almost maximal tests |
| `R.pick(v)` | random element of a vector |
| `R.weighted(w)` | index i with probability w[i] / Σw |
| `R.index(size)` | random index from [0, size) |
| `R.shuffle(v)` | shuffle a vector or a string (also `shuffle(first, last)`) |
| `R.sample(v, k)` | k elements of v without repetition, random order |
| `R.array(n, lo, hi)` | `vector<ll>` of n values from [lo, hi] |
| `R.distinct(n, lo, hi)` | n pairwise distinct values from [lo, hi], random order |
| `R.permutation(n, base = 1)` | random permutation of base … base + n − 1 |
| `R.partition(total, k, min = 1)` | k parts ≥ min summing to total |
| `R.str(n, alphabet = "a…z")` | random string |
| `R.brackets(n)` | uniformly random balanced bracket sequence of length 2n |
| `R.u64()` | 64 random bits |

## Trees — `gen::trees`, return `gen::Edges`

| | |
|---|---|
| `random(R, n)` | uniformly random labeled tree |
| `recursive(R, n)` | parent of i uniform in [1, i − 1] (depth ~ log n) |
| `deep(R, n, k)` | parent of i from [i − k, i − 1]: small k = long, path-like |
| `path(n)`, `star(n)`, `binary(n)` | path, star, complete binary tree |
| `binary_random(R, n)` | random tree with ≤ 2 children per vertex (root 1) |
| `max_degree(R, n, d)` | random tree with every degree ≤ d |
| `with_depth(R, n, d)` | random tree of depth exactly d (root 1) |
| `caterpillar(R, n, spine)` | path 1..spine, the rest hang off it |
| `broom(n, handle)` | path 1..handle, the rest attached to `handle` |
| `spider(n, k)` | k paths hanging from vertex 1 |
| `parents_increasing(R, edges, n, root = 1)` | parent array with `p[i] < i` (the "p₂ … pₙ" format), size n + 1 |

Shapes come in a recognizable labeling (path 1-2-3-…): use `gen::relabel` and
`gen::shuffle_edges`, unless the labeling is part of the test.

## Graphs — `gen::graphs`, simple (no loops, no multi-edges), return `gen::Edges`

| | |
|---|---|
| `random(R, n, m)` | random graph with m edges, maybe disconnected |
| `connected(R, n, m)` | random connected graph (random tree + random edges) |
| `dag(R, n, m)` | random DAG, edge `u v` = u → v |
| `bipartite(R, a, b, m)` | parts 1..a and a+1..a+b |
| `cycle(n)`, `complete(n)` | cycle, clique |
| `grid(r, c)` | r × c grid, vertex (i, j) = (i − 1)·c + j |
| `clique_chain(k, s)` | k cliques of size s in a chain: bridges and articulation points |
| `fill(R, edges, n, m)` | add random new edges until there are m |
| `merge(a, na, b)` | disjoint union: b's vertices become na + 1 … |
| `max_edges(n)` | n(n − 1) / 2 |

## Edges and relabeling

| | |
|---|---|
| `gen::Edges` | `vector<pair<int, int>>` |
| `gen::WEdges` | `vector<gen::WEdge>`, `WEdge{u, v, w}` |
| `with_weights(R, edges, lo, hi)` | `WEdges` with random weights from [lo, hi] |
| `relabeling(R, n)` | random relabeling `p`: `p[v]` = new label of v |
| `relabel_edges(edges, p)` | apply it to `Edges` / `WEdges` |
| `relabel_values(values, p)` | move per-vertex values with it (indexed 1..n or 0..n−1) |

## Modifiers — `g.test(...) | m`

Work on any test struct with `n` and `edges`; also callable directly, e.g.
`gen::relabel(R, edges, n)`, `gen::shuffle_edges(R, edges)`.

| | |
|---|---|
| `gen::relabel` | random vertex labels (edges only) |
| `gen::shuffle_edges` | random edge order |
| `gen::flip_edges` | random orientation of every edge |

## Tests against shortcuts — `gen::anti`

| | |
|---|---|
| `thue_morse(n, a = 'a', b = 'b')` | breaks polynomial hashing modulo 2⁶⁴ with any base (n ≳ 2000) |
| `unordered_map_keys(n, prime = 107897)` | multiples of a bucket count of gcc's `unordered_map`: O(n) per operation for ~10⁵ keys; try 172933, 126271 for other sizes / gcc versions |

## Random arguments of `g.test`

| | |
|---|---|
| `near(v, spread = 10)` | from [v − spread, v] |
| `between(lo, hi)` | from [lo, hi] |
| `one_of({a, b, c})` | one of the values |

## Hand-written tests

| | |
|---|---|
| `g.test({"3", "1 2 3"})` | one string per line |
| `text(R"(...)")` | raw string without the first empty line, the last blank line and the common indentation |

## Printing

| | |
|---|---|
| `line(out, a, b, ...)` | values separated by spaces, then a newline |
| `print_line(out, v)` | a vector on one line |
| `print_edges(out, edges)` | one edge per line (`u v` or `u v w`) |
| `print_matrix(out, rows)` | a `vector<vector<T>>`, row by row |

## Generator — `gen::Generator<Test> g(id, argc, argv)`

| | |
|---|---|
| `g.example(x)`, `g.ocen(x)`, `g.group(k)`, `g.test(x)` | the test list, see [guide.md](guide.md#the-test-list) |
| `.then(m)`, `\| m` | modifier, on a test or a group |
| `.check(c, "what")`, `\| gen::check(c, "what")` | check, on a test or a group |
| `.seed(x)` | fixed seed instead of the one from the name |
| `g.max_size_mb(x)` | error for tests bigger than x MB |
| `g.done()` | write (or `--list`) the tests; return it from `main` |
| `SINOL_GEN_VERSION` | version string |
