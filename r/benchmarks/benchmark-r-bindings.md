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
|       7 |                   20 |      2 |       1e+06 |             42 |   2 |               10 |             1e+05 |

## Benchmarked calls

| binding                     | batches | iterations_per_batch | text_length | pattern_length |   k | matches | min_seconds | median_seconds | mean_seconds | p95_seconds | max_seconds | calls_per_second |
|:----------------------------|--------:|---------------------:|------------:|---------------:|----:|--------:|------------:|---------------:|-------------:|------------:|------------:|-----------------:|
| Rsassy reusable character   |       7 |                   20 |       1e+06 |             42 |   2 |      10 |     0.00025 |        0.00025 |    0.0002714 |     0.00030 |     0.00030 |         4000.000 |
| Rsassy reusable raw         |       7 |                   20 |       1e+06 |             42 |   2 |      10 |     0.00025 |        0.00025 |    0.0002714 |     0.00030 |     0.00030 |         4000.000 |
| Rsassy one-shot raw         |       7 |                   20 |       1e+06 |             42 |   2 |      10 |     0.00025 |        0.00030 |    0.0002929 |     0.00030 |     0.00030 |         3333.333 |
| sassyRS one-shot raw        |       7 |                   20 |       1e+06 |             42 |   2 |      10 |     0.00045 |        0.00050 |    0.0004786 |     0.00050 |     0.00050 |         2000.000 |
| Python sassy via reticulate |       7 |                   20 |       1e+06 |             42 |   2 |      10 |     0.00050 |        0.00050 |    0.0005214 |     0.00055 |     0.00055 |         2000.000 |

## Summary

| binding                     | median_seconds | calls_per_second | matches |
|:----------------------------|---------------:|-----------------:|--------:|
| Rsassy reusable character   |        0.00025 |         4000.000 |      10 |
| Rsassy reusable raw         |        0.00025 |         4000.000 |      10 |
| Rsassy one-shot raw         |        0.00030 |         3333.333 |      10 |
| sassyRS one-shot raw        |        0.00050 |         2000.000 |      10 |
| Python sassy via reticulate |        0.00050 |         2000.000 |      10 |

    #> Wrote benchmark results to /root/sassy/r/benchmarks/results/r-bindings.csv
