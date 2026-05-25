Rsassy, sassyRS, and Python binding benchmark
================

- [Machine and build details](#machine-and-build-details)
- [Input data](#input-data)
- [Benchmarked calls](#benchmarked-calls)
- [Summary](#summary)

This report compares the R-facing overhead of three bindings to `sassy`:

- **Rsassy**: R bindings through R’s native C API.
- **sassyRS**: an `extendr` prototype.
- **Python sassy via reticulate**: the published `sassy-rs` Python
  binding called from R with `reticulate`.

The benchmark uses a deterministic 1 Mbp DNA text by default, not the
tiny toy string used in examples. Exact and two-mismatch copies of the
query are inserted every 100 kbp so each binding has real matches to
return.

## Machine and build details

The native benchmark build is expected to be installed before rendering
with:

``` bash
RUSTFLAGS="-C target-cpu=native" SASSY_RUST_FEATURES=native-simd R CMD INSTALL r/Rsassy
RUSTFLAGS="-C target-cpu=native" SASSY_RUST_FEATURES=native-simd R CMD INSTALL r/sassyRS
uv venv --python 3.11 .venv-benchmark
uv pip install --python .venv-benchmark sassy-rs
export RETICULATE_PYTHON="$PWD/.venv-benchmark/bin/python"
```

| field               | value                                                  |
|:--------------------|:-------------------------------------------------------|
| system              | Linux                                                  |
| release             | 6.8.0-78-generic                                       |
| machine             | x86_64                                                 |
| cpu_model           | 13th Gen Intel(R) Core(TM) i5-13500                    |
| R                   | R version 4.6.0 (2026-04-24)                           |
| platform            | x86_64-pc-linux-gnu                                    |
| RUSTFLAGS           | -C target-cpu=native                                   |
| SASSY_RUST_FEATURES | native-simd                                            |
| RETICULATE_PYTHON   | /root/sassy/.venv-benchmark/bin/python                 |
| cargo               | cargo 1.91.1 (ea2d97820 2025-10-10)                    |
| rustc               | rustc 1.91.1 (ed61e7d7e 2025-11-07)                    |
| python              | Python 3.11.14                                         |
| simd_flags          | sse sse2 fma sse4_1 sse4_2 avx bmi1 avx2 bmi2 avx_vnni |

## Input data

| batches | iterations_per_batch | warmup | text_length | pattern_length |   k | inserted_matches | insertion_spacing |
|--------:|---------------------:|-------:|------------:|---------------:|----:|-----------------:|------------------:|
|       7 |                 1000 |      2 |       1e+06 |             42 |   2 |               10 |             1e+05 |

## Benchmarked calls

| binding                     | batches | iterations_per_batch | text_length | pattern_length |   k | matches | min_seconds | median_seconds | mean_seconds | p95_seconds | max_seconds | calls_per_second |
|:----------------------------|--------:|---------------------:|------------:|---------------:|----:|--------:|------------:|---------------:|-------------:|------------:|------------:|-----------------:|
| Rsassy reusable raw         |       7 |                 1000 |       1e+06 |             42 |   2 |      10 |    0.000267 |       0.000269 |    0.0002686 |   0.0002704 |    0.000271 |         3717.472 |
| Rsassy reusable character   |       7 |                 1000 |       1e+06 |             42 |   2 |      10 |    0.000273 |       0.000273 |    0.0002736 |   0.0002754 |    0.000276 |         3663.004 |
| Rsassy one-shot raw         |       7 |                 1000 |       1e+06 |             42 |   2 |      10 |    0.000277 |       0.000278 |    0.0002784 |   0.0002804 |    0.000281 |         3597.122 |
| sassyRS one-shot raw        |       7 |                 1000 |       1e+06 |             42 |   2 |      10 |    0.000409 |       0.000409 |    0.0004107 |   0.0004154 |    0.000416 |         2444.988 |
| Python sassy via reticulate |       7 |                 1000 |       1e+06 |             42 |   2 |      10 |    0.000488 |       0.000490 |    0.0004901 |   0.0004930 |    0.000493 |         2040.816 |

## Summary

| binding                     | median_seconds | calls_per_second | matches |
|:----------------------------|---------------:|-----------------:|--------:|
| Rsassy reusable raw         |       0.000269 |         3717.472 |      10 |
| Rsassy reusable character   |       0.000273 |         3663.004 |      10 |
| Rsassy one-shot raw         |       0.000278 |         3597.122 |      10 |
| sassyRS one-shot raw        |       0.000409 |         2444.988 |      10 |
| Python sassy via reticulate |       0.000490 |         2040.816 |      10 |

    #> Wrote benchmark results to /root/sassy/r/benchmarks/results/r-bindings.csv
