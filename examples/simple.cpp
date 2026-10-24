// Array task: "n, then n numbers". Tests are written straight to the stream,
// no test struct needed (gen::Generator without <Test>).
#include <bits/stdc++.h>
#include "../gen.h"
using namespace std;
using gen::ll;

const int MAXN = 200'000;
const ll MAXV = 1'000'000'000;

// f(gen::Rng&, std::ostream&, your arguments...)
void random_arr(gen::Rng& R, ostream& out, int n, ll lo, ll hi) {
    gen::line(out, n);
    gen::print_line(out, R.array(n, lo, hi));
}

void sorted_arr(gen::Rng& R, ostream& out, int n, ll hi) {
    auto a = R.array(n, 1, hi);
    sort(a.begin(), a.end());
    gen::line(out, n);
    gen::print_line(out, a);
}

// Or return anything printable - here a string.
string constant(gen::Rng&, int n, ll v) {
    ostringstream o;
    gen::line(o, n);
    gen::print_line(o, vector<ll>(static_cast<size_t>(n), v));
    return o.str();
}

int main(int argc, char* argv[]) {
    gen::Generator g("arr", argc, argv);

    g.example("3\n1 2 3\n");                          // arr0a.in - two examples: 0a, 0b (one would be arr0.in)
    g.example("1\n7\n");                              // arr0b.in
    g.ocen(random_arr, 10, 1, 10);                    // arr1ocen.in

    g.group(1);                                       // n <= 1000
    g.test("1\n1\n");                                 // the smallest test, by hand
    for (ll hi : {10LL, 1'000LL, MAXV}) g.test(random_arr, 1000, 1, hi);
    g.test(sorted_arr, 1000, MAXV);

    g.group(2);                                       // no extra limits
    g.test(random_arr, gen::near(MAXN, 100), 1, MAXV);   // n from [MAXN - 100, MAXN]
    g.test(random_arr, gen::between(1000, MAXN), 1, gen::one_of({10LL, MAXV}));
    g.test(random_arr, MAXN, MAXV - 1000, MAXV);
    g.test(sorted_arr, MAXN, MAXV).seed(2026);
    g.test(constant, MAXN, MAXV);

    return g.done();
}
