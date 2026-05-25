#!/usr/bin/env Rscript

# Vendor Rust crates for CRAN/offline installation.
# Run from the sassyRS package root: Rscript tools/vendor-rust.R

root <- normalizePath(file.path("."), mustWork = TRUE)
src <- file.path(root, "src")
rust <- file.path(src, "rust")
manifest <- file.path(rust, "Cargo.toml")

if (!file.exists(manifest)) {
  stop("Cannot find ", manifest, call. = FALSE)
}

run <- function(cmd, args, wd = root, stdout = "", stderr = "") {
  message("$ ", cmd, " ", paste(args, collapse = " "))
  status <- system2(cmd, args, stdout = stdout, stderr = stderr, cwd = wd)
  if (!identical(status, 0L)) {
    stop("Command failed: ", cmd, call. = FALSE)
  }
}

run("cargo", c("generate-lockfile", "--manifest-path", manifest))

unlink(file.path(src, "vendor"), recursive = TRUE, force = TRUE)
unlink(file.path(rust, "vendor.tar.xz"), force = TRUE)

config <- system2(
  "cargo",
  c("vendor", "--locked", "--manifest-path", manifest, "--versioned-dirs", "vendor"),
  stdout = TRUE,
  stderr = ""
)
status <- attr(config, "status")
if (!is.null(status) && status != 0L) {
  stop("cargo vendor failed", call. = FALSE)
}
writeLines(config, file.path(rust, "vendor-config.toml"))

run("tar", c("-cJf", "rust/vendor.tar.xz", "vendor"), wd = src)
unlink(file.path(src, "vendor"), recursive = TRUE, force = TRUE)

message("Wrote ", file.path(rust, "vendor.tar.xz"))
