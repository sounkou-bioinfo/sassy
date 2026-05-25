desc <- read.dcf("DESCRIPTION")

if (!"SystemRequirements" %in% colnames(desc)) {
  stop("`SystemRequirements` must mention Cargo and rustc", call. = FALSE)
}

sysreqs <- desc[, "SystemRequirements"]
if (!grepl("cargo", sysreqs, ignore.case = TRUE) || !grepl("rustc", sysreqs, ignore.case = TRUE)) {
  stop("`SystemRequirements` must mention Cargo and rustc", call. = FALSE)
}

parts <- strsplit(sysreqs, ", ")[[1]]
rustc_ver <- parts[grepl("rustc", parts)]

Sys.setenv(PATH = paste(Sys.getenv("PATH"), file.path(Sys.getenv("HOME"), ".cargo", "bin"), sep = .Platform$path.sep))

rustc_version <- tryCatch(system("rustc --version", intern = TRUE), error = function(e) {
  stop("rustc was not found on PATH; install Rust from https://www.rust-lang.org/tools/install", call. = FALSE)
})
cargo_version <- tryCatch(system("cargo --version", intern = TRUE), error = function(e) {
  stop("cargo was not found on PATH; install Rust from https://www.rust-lang.org/tools/install", call. = FALSE)
})

extract_semver <- function(ver) {
  if (grepl("\\d+\\.\\d+(\\.\\d+)?", ver)) {
    sub(".*?(\\d+\\.\\d+(\\.\\d+)?).*", "\\1", ver)
  } else {
    NA_character_
  }
}

msrv <- extract_semver(rustc_ver)
current <- extract_semver(rustc_version)
if (!is.na(msrv) && !is.na(current) && utils::compareVersion(msrv, current) == 1) {
  stop(sprintf("Minimum supported Rust version is %s; installed Rust version is %s", msrv, current), call. = FALSE)
}

message(sprintf("Using %s\nUsing %s", cargo_version, rustc_version))
