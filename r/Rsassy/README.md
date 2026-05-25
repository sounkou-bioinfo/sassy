
<!-- README.md is generated from README.Rmd. Please edit that file. -->

# Rsassy

`Rsassy` provides R bindings to the Rust
[`sassy`](https://github.com/RagnarGrootKoerkamp/sassy) approximate
string matcher through R’s native C API. This package is the lower-level
R binding: the C boundary is where raw-vector access and future
ALTREP-aware paths can be handled explicitly.

## Installation

From the repository root:

``` sh
R CMD INSTALL r/Rsassy
```

For local benchmarking on the current machine, compile with native CPU
features:

``` sh
RUSTFLAGS="-C target-cpu=native" \
SASSY_RUST_FEATURES=native-simd \
R CMD INSTALL r/Rsassy
```

System requirements are Cargo, `rustc >= 1.91`, and `xz`.

## Usage

``` r
library(Rsassy)

sassy_search(
  pattern = "ACGT",
  text = "TTACGTAA",
  k = 0,
  alphabet = "dna",
  rc = FALSE
)
#>   text_start text_end pattern_start pattern_end cost strand
#> 1          2        6             0           4    0      +
```

For repeated searches, create and reuse a searcher:

``` r
searcher <- sassy_searcher("dna", rc = FALSE)
sassy_searcher_search(searcher, "AAGGGGA", "CCCCCCCCCAAGGGGACCCCCAAGGCGACCCCCCCCC", k = 1)
#>   text_start text_end pattern_start pattern_end cost strand
#> 1          9       16             0           7    0      +
#> 2         21       28             0           7    1      +
```

Raw vectors are accepted without converting through R strings:

``` r
sassy_search(
  charToRaw("ACGT"),
  charToRaw("TTACGTAA"),
  k = 0,
  alphabet = "dna",
  rc = FALSE
)
#>   text_start text_end pattern_start pattern_end cost strand
#> 1          2        6             0           4    0      +
```

Open R connections can also be searched through the C boundary. Rsassy
uses R’s experimental connection API internally here, so the R side does
not loop over `readBin()` or `readChar()`:

``` r
con <- rawConnection(charToRaw("TTACGTAA"), "rb")
sassy_search_connection("ACGT", con, k = 0, alphabet = "dna", rc = FALSE, chunk_size = 3)
#>   text_start text_end pattern_start pattern_end cost strand
#> 1          2        6             0           4    0      +
close(con)
```

The returned data frame uses 0-based, half-open coordinates:
`text_start`, `text_end`, `pattern_start`, and `pattern_end`.

## Testing

`Rsassy` uses `tinytest`:

``` sh
Rscript -e 'tinytest::test_package("Rsassy")'
```

## Benchmark

The binding benchmark is in
[`../benchmarks/benchmark-r-bindings.Rmd`](../benchmarks/benchmark-r-bindings.Rmd),
with a rendered GitHub Markdown report at
[`../benchmarks/benchmark-r-bindings.md`](../benchmarks/benchmark-r-bindings.md).
