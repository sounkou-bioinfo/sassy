
<!-- README.md is generated from README.Rmd. Please edit that file. -->

# Rsassy

R bindings to Sassy using R’s native C API.

## Install

From the repository root:

``` sh
R CMD INSTALL r/Rsassy
```

Requirements: Cargo, `rustc >= 1.91`, and `xz`.

## Usage

``` r
library(Rsassy)

pattern <- charToRaw("ATCGATCG")
text <- charToRaw("GGGGATCGATCGTTTT")

searcher <- sassy_searcher("dna") # "ascii", "dna", or "iupac"
matches <- sassy_searcher_search(searcher, pattern, text, k = 1)
matches
#>   text_start text_end pattern_start pattern_end cost strand
#> 1          4       12             0           8    0      +
#> 2          6       14             0           8    1      -
#> 3          2       10             0           8    1      -
```

`matches` is a data frame with 0-based, half-open coordinates:
`text_start`, `text_end`, `pattern_start`, `pattern_end`, `cost`, and
`strand`.

One-shot use:

``` r
sassy_search(pattern, text, k = 1, alphabet = "dna")
#>   text_start text_end pattern_start pattern_end cost strand
#> 1          4       12             0           8    0      +
#> 2          6       14             0           8    1      -
#> 3          2       10             0           8    1      -
```

R strings are also accepted:

``` r
sassy_search("ATCGATCG", "GGGGATCGATCGTTTT", k = 1, alphabet = "dna")
#>   text_start text_end pattern_start pattern_end cost strand
#> 1          4       12             0           8    0      +
#> 2          6       14             0           8    1      -
#> 3          2       10             0           8    1      -
```

Open R connections can be searched without an R-level read loop:

``` r
tmp <- tempfile(fileext = ".gz")
con <- gzfile(tmp, "wb")
writeBin(charToRaw("GGGGATCGATCGTTTT"), con)
close(con)

con <- gzfile(tmp, "rb")
sassy_search_connection("ATCG", con, k = 1, alphabet = "dna")
#>   text_start text_end pattern_start pattern_end cost strand
#> 1          4        8             0           4    0      +
#> 2          8       12             0           4    0      +
#> 3         10       14             0           4    1      -
#> 4          6       10             0           4    0      -
#> 5          2        6             0           4    1      -
close(con)
unlink(tmp)
```

## Check

``` sh
R CMD build r/Rsassy
R CMD check --no-manual --no-vignettes Rsassy_*.tar.gz
```
