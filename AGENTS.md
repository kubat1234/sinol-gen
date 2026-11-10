# sinol-gen

One header, `gen.h`, for ingens of sinol / SIO2 packages. Copied into every
package (`prog/gen.h`) by contest-template's `contest.py update gen`, which
takes the newest tag; its docs are bundled in the sinol-package skill
(synced daily from the newest tag).

## Rules

- **Same tests everywhere:** an ingen must generate byte-identical tests on
  every machine, compiler (gcc, clang) and C++ standard (17, 20, 23). No
  `rand()`, `<random>` distributions, `std::shuffle`, unordered iteration or
  anything implementation-defined in what reaches a test.
- **Never change the tests of existing ingens** in a minor or patch release.
  The golden values in `tests/test.cpp` and `tests/run.sh` (RNG output,
  automatic seeds of every kind of argument) guard this — a failing golden
  value means a major release, never just updating the value.
- Must compile with sinol-make's flags: `-O2 -Werror -Wall -Wextra -Wshadow
  -Wconversion -Wfloat-equal` (+ `-pedantic` in the tests). C++17.

## Releasing

Change `SINOL_GEN_VERSION` in `gen.h` and push to `main`: the `release`
workflow runs the tests, tags `v<version>` and creates the GitHub release.
Never tag by hand. Major = existing ingens give other tests or don't compile;
minor = something new; patch = fixes, docs.

## Testing

`tests/run.sh` (gcc, and clang when installed; a few minutes): unit tests,
examples, command line, identical output from every compiler and standard.
Keep `docs/guide.md`, `docs/reference.md` and `README.md` in step with
`gen.h`; the examples in `examples/` must compile and run.
