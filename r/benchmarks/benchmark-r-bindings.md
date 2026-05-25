Rsassy vs sassyRS benchmark
================

- [Package setup](#package-setup)
- [Machine and build details](#machine-and-build-details)
- [Benchmark configuration](#benchmark-configuration)
- [Benchmark](#benchmark)
- [Summary](#summary)

This report benchmarks the two R bindings in this repository:

- **Rsassy**: a C/R API boundary intended for explicit raw-vector and
  future ALTREP access.
- **sassyRS**: an `extendr` boundary prototype.

The benchmark uses a fixed DNA pattern and target text, then reports
elapsed seconds per call.

``` r
get_int_env <- function(name, default) {
  value <- Sys.getenv(name, unset = "")
  if (identical(value, "")) {
    return(as.integer(default))
  }
  value <- suppressWarnings(as.integer(value))
  if (is.na(value) || value <= 0L) {
    stop(name, " must be a positive integer", call. = FALSE)
  }
  value
}

is_absolute_path <- function(path) {
  grepl("^/", path) || grepl("^[A-Za-z]:[/\\\\]", path)
}

find_repo_root <- function(start) {
  path <- normalizePath(start, mustWork = TRUE)
  repeat {
    if (file.exists(file.path(path, "Cargo.toml")) && dir.exists(file.path(path, "r"))) {
      return(path)
    }
    parent <- dirname(path)
    if (identical(parent, path)) {
      stop("Could not locate repository root from ", start, call. = FALSE)
    }
    path <- parent
  }
}

input <- knitr::current_input()
input_dir <- if (!is.null(input) && file.exists(input)) dirname(normalizePath(input)) else getwd()
repo_root <- find_repo_root(input_dir)

batches <- get_int_env("SASSY_R_BENCH_BATCHES", params$batches)
iterations <- get_int_env("SASSY_R_BENCH_ITERS", params$iterations)
text_multiplier <- get_int_env("SASSY_R_BENCH_TEXT_MULTIPLIER", params$text_multiplier)
warmup <- get_int_env("SASSY_R_BENCH_WARMUP", params$warmup)
output <- Sys.getenv("SASSY_R_BENCH_OUT", params$output)
if (!is_absolute_path(output)) {
  output <- file.path(repo_root, output)
}

run <- function(cmd, args) {
  message("$ ", cmd, " ", paste(args, collapse = " "))
  status <- system2(cmd, args)
  if (!identical(status, 0L)) {
    stop("Command failed: ", cmd, call. = FALSE)
  }
}
```

## Package setup

``` r
if (isTRUE(params$install)) {
  bench_lib <- tempfile("sassy-r-bindings-lib-")
  dir.create(bench_lib, recursive = TRUE)
  .libPaths(c(bench_lib, .libPaths()))
  message("Installing benchmark packages into ", bench_lib)
  run(file.path(R.home("bin"), "R"), c("CMD", "INSTALL", "-l", bench_lib, file.path(repo_root, "r", "Rsassy")))
  run(file.path(R.home("bin"), "R"), c("CMD", "INSTALL", "-l", bench_lib, file.path(repo_root, "r", "sassyRS")))
}
#> Installing benchmark packages into /tmp/RtmpheLVTf/sassy-r-bindings-lib-2f18c82c5fb109
#> $ /usr/lib/R/bin/R CMD INSTALL -l /tmp/RtmpheLVTf/sassy-r-bindings-lib-2f18c82c5fb109 /root/sassy/r/Rsassy
#> $ /usr/lib/R/bin/R CMD INSTALL -l /tmp/RtmpheLVTf/sassy-r-bindings-lib-2f18c82c5fb109 /root/sassy/r/sassyRS

if (!requireNamespace("Rsassy", quietly = TRUE)) {
  stop("Package 'Rsassy' is not installed. Render with params = list(install = TRUE).", call. = FALSE)
}
if (!requireNamespace("sassyRS", quietly = TRUE)) {
  stop("Package 'sassyRS' is not installed. Render with params = list(install = TRUE).", call. = FALSE)
}
```

## Machine and build details

The benchmark workflow sets `RUSTFLAGS=-C target-cpu=native` and
`SASSY_RUST_FEATURES=native-simd` so the Rust crates are compiled for
the runner CPU instead of the portable scalar fallback used for
CRAN-style checks.

``` r
safe_system <- function(cmd, args = character()) {
  if (!nzchar(Sys.which(cmd))) {
    return(character())
  }
  out <- tryCatch(system2(cmd, args, stdout = TRUE, stderr = TRUE), error = function(e) character())
  out[!is.na(out)]
}

machine <- data.frame(
  field = c(
    "system",
    "release",
    "machine",
    "R",
    "platform",
    "NOT_CRAN",
    "RUSTFLAGS",
    "SASSY_RUST_FEATURES",
    "cargo",
    "rustc"
  ),
  value = c(
    unname(Sys.info()[["sysname"]]),
    unname(Sys.info()[["release"]]),
    unname(Sys.info()[["machine"]]),
    R.version.string,
    R.version$platform,
    Sys.getenv("NOT_CRAN", unset = ""),
    Sys.getenv("RUSTFLAGS", unset = ""),
    Sys.getenv("SASSY_RUST_FEATURES", unset = ""),
    paste(safe_system("cargo", "--version"), collapse = "\n"),
    paste(safe_system("rustc", "--version"), collapse = "\n")
  ),
  stringsAsFactors = FALSE
)
machine
#>                  field                               value
#> 1               system                               Linux
#> 2              release                    6.8.0-78-generic
#> 3              machine                              x86_64
#> 4                    R        R version 4.6.0 (2026-04-24)
#> 5             platform                 x86_64-pc-linux-gnu
#> 6             NOT_CRAN                                true
#> 7            RUSTFLAGS                -C target-cpu=native
#> 8  SASSY_RUST_FEATURES                         native-simd
#> 9                cargo cargo 1.91.1 (ea2d97820 2025-10-10)
#> 10               rustc rustc 1.91.1 (ed61e7d7e 2025-11-07)

rust_verbose <- safe_system("rustc", "-vV")
if (length(rust_verbose) > 0L) {
  cat(paste(rust_verbose, collapse = "\n"), "\n")
}
#> rustc 1.91.1 (ed61e7d7e 2025-11-07)
#> binary: rustc
#> commit-hash: ed61e7d7e242494fb7057f2657300d9e77bb4fcb
#> commit-date: 2025-11-07
#> host: x86_64-unknown-linux-gnu
#> release: 1.91.1
#> LLVM version: 21.1.2

if (identical(Sys.info()[["sysname"]], "Linux")) {
  lscpu <- safe_system("lscpu")
  if (length(lscpu) > 0L) {
    cat(paste(lscpu, collapse = "\n"), "\n")
  }
  cpuinfo <- "/proc/cpuinfo"
  if (file.exists(cpuinfo)) {
    flags <- grep("^(flags|Features)[[:space:]]*:", readLines(cpuinfo, warn = FALSE), value = TRUE)[1]
    if (!is.na(flags)) {
      simd_flags <- unlist(strsplit(sub("^[^:]+:[[:space:]]*", "", flags), "[[:space:]]+"), use.names = FALSE)
      simd_flags <- grep("^(sse|avx|fma|bmi|neon|asimd)", simd_flags, value = TRUE)
      cat("SIMD-related CPU flags:\n", paste(simd_flags, collapse = " "), "\n", sep = "")
    }
  }
} else if (identical(Sys.info()[["sysname"]], "Darwin")) {
  sysctl <- safe_system("sysctl", c("-n", "machdep.cpu.brand_string"))
  if (length(sysctl) > 0L) {
    cat("CPU: ", paste(sysctl, collapse = " "), "\n", sep = "")
  }
  features <- safe_system("sysctl", c("-a"))
  features <- grep("machdep.cpu.features|machdep.cpu.leaf7_features", features, value = TRUE)
  if (length(features) > 0L) {
    cat(paste(features, collapse = "\n"), "\n")
  }
}
#> Architecture:                         x86_64
#> CPU op-mode(s):                       32-bit, 64-bit
#> Address sizes:                        46 bits physical, 48 bits virtual
#> Byte Order:                           Little Endian
#> CPU(s):                               20
#> On-line CPU(s) list:                  0-19
#> Vendor ID:                            GenuineIntel
#> BIOS Vendor ID:                       Intel(R) Corporation
#> Model name:                           13th Gen Intel(R) Core(TM) i5-13500
#> BIOS Model name:                      13th Gen Intel(R) Core(TM) i5-13500 To Be Filled By O.E.M. CPU @ 2.4GHz
#> BIOS CPU family:                      205
#> CPU family:                           6
#> Model:                                191
#> Thread(s) per core:                   2
#> Core(s) per socket:                   14
#> Socket(s):                            1
#> Stepping:                             2
#> CPU(s) scaling MHz:                   31%
#> CPU max MHz:                          4800.0000
#> CPU min MHz:                          800.0000
#> BogoMIPS:                             4992.00
#> Flags:                                fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl xtopology nonstop_tsc cpuid aperfmperf tsc_known_freq pni pclmulqdq dtes64 monitor ds_cpl vmx smx est tm2 ssse3 sdbg fma cx16 xtpr pdcm sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c rdrand lahf_lm abm 3dnowprefetch cpuid_fault epb ssbd ibrs ibpb stibp ibrs_enhanced tpr_shadow flexpriority ept vpid ept_ad fsgsbase tsc_adjust bmi1 avx2 smep bmi2 erms invpcid rdseed adx smap clflushopt clwb intel_pt sha_ni xsaveopt xsavec xgetbv1 xsaves split_lock_detect user_shstk avx_vnni dtherm ida arat pln pts hwp hwp_notify hwp_act_window hwp_epp hwp_pkg_req hfi vnmi umip pku ospke waitpkg gfni vaes vpclmulqdq tme rdpid movdiri movdir64b fsrm md_clear serialize pconfig arch_lbr ibt flush_l1d arch_capabilities
#> Virtualization:                       VT-x
#> L1d cache:                            544 KiB (14 instances)
#> L1i cache:                            704 KiB (14 instances)
#> L2 cache:                             11.5 MiB (8 instances)
#> L3 cache:                             24 MiB (1 instance)
#> NUMA node(s):                         1
#> NUMA node0 CPU(s):                    0-19
#> Vulnerability Gather data sampling:   Not affected
#> Vulnerability Itlb multihit:          Not affected
#> Vulnerability L1tf:                   Not affected
#> Vulnerability Mds:                    Not affected
#> Vulnerability Meltdown:               Not affected
#> Vulnerability Mmio stale data:        Not affected
#> Vulnerability Reg file data sampling: Mitigation; Clear Register File
#> Vulnerability Retbleed:               Not affected
#> Vulnerability Spec rstack overflow:   Not affected
#> Vulnerability Spec store bypass:      Mitigation; Speculative Store Bypass disabled via prctl
#> Vulnerability Spectre v1:             Mitigation; usercopy/swapgs barriers and __user pointer sanitization
#> Vulnerability Spectre v2:             Mitigation; Enhanced / Automatic IBRS; IBPB conditional; RSB filling; PBRSB-eIBRS SW sequence; BHI BHI_DIS_S
#> Vulnerability Srbds:                  Not affected
#> Vulnerability Tsx async abort:        Not affected
#> SIMD-related CPU flags:
#> sse sse2 fma sse4_1 sse4_2 avx bmi1 avx2 bmi2 avx_vnni
```

## Benchmark configuration

``` r
pattern <- "AAGGGGA"
unit_text <- "CCCCCCCCCAAGGGGACCCCCAAGGCGACCCCCCCCC"
text <- paste(rep(unit_text, text_multiplier), collapse = "")
raw_pattern <- charToRaw(pattern)
raw_text <- charToRaw(text)
k <- 1L

config <- data.frame(
  batches = batches,
  iterations_per_batch = iterations,
  warmup = warmup,
  text_length = nchar(text, type = "bytes"),
  pattern_length = nchar(pattern, type = "bytes"),
  k = k
)
config
#>   batches iterations_per_batch warmup text_length pattern_length k
#> 1       7                   50      3       37000              7 1
```

## Benchmark

``` r
validate <- function(result, label) {
  required <- c("text_start", "text_end", "pattern_start", "pattern_end", "cost", "strand")
  if (!is.data.frame(result) || !all(required %in% names(result))) {
    stop(label, " did not return the expected matches data frame", call. = FALSE)
  }
  if (nrow(result) <= 0L) {
    stop(label, " returned no matches on benchmark input", call. = FALSE)
  }
  invisible(result)
}

bench_one <- function(label, fun) {
  for (i in seq_len(warmup)) {
    validate(fun(), label)
  }

  gc()
  seconds_per_call <- numeric(batches)
  match_count <- NA_integer_

  for (batch in seq_len(batches)) {
    last <- NULL
    elapsed <- system.time({
      for (i in seq_len(iterations)) {
        last <- fun()
      }
    })[["elapsed"]]
    validate(last, label)
    match_count <- nrow(last)
    seconds_per_call[[batch]] <- elapsed / iterations
  }

  data.frame(
    binding = label,
    batches = batches,
    iterations_per_batch = iterations,
    text_length = nchar(text, type = "bytes"),
    pattern_length = nchar(pattern, type = "bytes"),
    k = k,
    matches = match_count,
    min_seconds = min(seconds_per_call),
    median_seconds = unname(stats::median(seconds_per_call)),
    mean_seconds = mean(seconds_per_call),
    p95_seconds = unname(stats::quantile(seconds_per_call, probs = 0.95, names = FALSE)),
    max_seconds = max(seconds_per_call),
    calls_per_second = 1 / unname(stats::median(seconds_per_call)),
    stringsAsFactors = FALSE
  )
}

rsassy_searcher_chr <- Rsassy::sassy_searcher("dna", rc = FALSE)
rsassy_searcher_raw <- Rsassy::sassy_searcher("dna", rc = FALSE)

benchmarks <- list(
  "Rsassy reusable character" = function() {
    Rsassy::sassy_searcher_search(rsassy_searcher_chr, pattern, text, k)
  },
  "Rsassy reusable raw" = function() {
    Rsassy::sassy_searcher_search(rsassy_searcher_raw, raw_pattern, raw_text, k)
  },
  "Rsassy one-shot character" = function() {
    Rsassy::sassy_search(pattern, text, k, alphabet = "dna", rc = FALSE)
  },
  "Rsassy one-shot raw" = function() {
    Rsassy::sassy_search(raw_pattern, raw_text, k, alphabet = "dna", rc = FALSE)
  },
  "sassyRS one-shot character" = function() {
    sassyRS::sassy_search(pattern, text, k, alphabet = "dna", rc = FALSE)
  },
  "sassyRS one-shot raw" = function() {
    sassyRS::sassy_search(raw_pattern, raw_text, k, alphabet = "dna", rc = FALSE)
  }
)

message(
  "Benchmarking ", length(benchmarks), " cases; ", batches, " batches x ",
  iterations, " iterations; text length ", nchar(text, type = "bytes"), " bytes."
)
#> Benchmarking 6 cases; 7 batches x 50 iterations; text length 37000 bytes.
results <- do.call(rbind, Map(bench_one, names(benchmarks), benchmarks))
row.names(results) <- NULL
results <- results[order(results$median_seconds), ]
results
#>                      binding batches iterations_per_batch text_length
#> 1  Rsassy reusable character       7                   50       37000
#> 2        Rsassy reusable raw       7                   50       37000
#> 4        Rsassy one-shot raw       7                   50       37000
#> 3  Rsassy one-shot character       7                   50       37000
#> 5 sassyRS one-shot character       7                   50       37000
#> 6       sassyRS one-shot raw       7                   50       37000
#>   pattern_length k matches min_seconds median_seconds mean_seconds p95_seconds
#> 1              7 1    2000     0.00042        0.00042 0.0004200000    0.000420
#> 2              7 1    2000     0.00042        0.00042 0.0004285714    0.000440
#> 4              7 1    2000     0.00044        0.00044 0.0004428571    0.000454
#> 3              7 1    2000     0.00044        0.00044 0.0004400000    0.000440
#> 5              7 1    2000     0.00062        0.00062 0.0006200000    0.000620
#> 6              7 1    2000     0.00066        0.00068 0.0006742857    0.000694
#>   max_seconds calls_per_second
#> 1     0.00042         2380.952
#> 2     0.00044         2380.952
#> 4     0.00046         2272.727
#> 3     0.00044         2272.727
#> 5     0.00062         1612.903
#> 6     0.00070         1470.588
```

## Summary

``` r
summary_table <- results[, c("binding", "median_seconds", "calls_per_second", "matches")]
summary_table
#>                      binding median_seconds calls_per_second matches
#> 1  Rsassy reusable character        0.00042         2380.952    2000
#> 2        Rsassy reusable raw        0.00042         2380.952    2000
#> 4        Rsassy one-shot raw        0.00044         2272.727    2000
#> 3  Rsassy one-shot character        0.00044         2272.727    2000
#> 5 sassyRS one-shot character        0.00062         1612.903    2000
#> 6       sassyRS one-shot raw        0.00068         1470.588    2000

dir.create(dirname(output), recursive = TRUE, showWarnings = FALSE)
utils::write.csv(results, output, row.names = FALSE)
message("Wrote benchmark results to ", output)
#> Wrote benchmark results to /root/sassy/r/benchmarks/results/r-bindings.csv

summary_path <- Sys.getenv("GITHUB_STEP_SUMMARY", unset = "")
if (!identical(summary_path, "")) {
  lines <- c(
    "## Rsassy vs sassyRS benchmark",
    "",
    sprintf("Machine: `%s %s (%s)`.", Sys.info()[["sysname"]], Sys.info()[["release"]], Sys.info()[["machine"]]),
    sprintf("Rust: `%s`; cargo: `%s`.", machine$value[machine$field == "rustc"], machine$value[machine$field == "cargo"]),
    sprintf("Build flags: `RUSTFLAGS=%s`; `SASSY_RUST_FEATURES=%s`.", Sys.getenv("RUSTFLAGS", unset = ""), Sys.getenv("SASSY_RUST_FEATURES", unset = "")),
    sprintf("Text length: `%s` bytes; pattern length: `%s`; k: `%s`.", nchar(text, type = "bytes"), nchar(pattern, type = "bytes"), k),
    sprintf("Each row reports the median over `%s` batches of `%s` calls.", batches, iterations),
    "",
    "| Binding | Median seconds/call | Calls/second | Matches |",
    "|---|---:|---:|---:|",
    sprintf(
      "| %s | %.6f | %.2f | %d |",
      results$binding,
      results$median_seconds,
      results$calls_per_second,
      results$matches
    ),
    "",
    sprintf("CSV artifact: `%s`", output)
  )
  writeLines(lines, summary_path, useBytes = TRUE)
}
```
