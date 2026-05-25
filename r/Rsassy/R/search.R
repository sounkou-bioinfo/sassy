#' @useDynLib Rsassy, .registration = TRUE
NULL

as_sassy_sequence <- function(x, arg) {
  if (is.raw(x)) {
    return(x)
  }
  if (!is.character(x) || length(x) != 1L || is.na(x)) {
    stop(arg, " must be a raw vector or a non-missing character scalar", call. = FALSE)
  }
  enc2utf8(x)
}

check_sassy_k <- function(k) {
  if (!is.numeric(k) || length(k) != 1L || is.na(k) || k < 0 || k != floor(k)) {
    stop("k must be a single non-negative integer", call. = FALSE)
  }
  as.integer(k)
}

check_sassy_alpha <- function(alpha) {
  if (is.null(alpha)) {
    return(NaN)
  }
  if (!is.numeric(alpha) || length(alpha) != 1L || is.na(alpha)) {
    stop("alpha must be NULL or a single number in [0, 1]", call. = FALSE)
  }
  alpha <- as.numeric(alpha)
  if (alpha < 0 || alpha > 1) {
    stop("alpha must be NULL or a single number in [0, 1]", call. = FALSE)
  }
  alpha
}

sassy_sequence_nbytes <- function(x, arg) {
  if (is.raw(x)) {
    return(length(x))
  }
  if (!is.character(x) || length(x) != 1L || is.na(x)) {
    stop(arg, " must be a raw vector or a non-missing character scalar", call. = FALSE)
  }
  nchar(enc2utf8(x), type = "bytes")
}

empty_sassy_matches <- function() {
  out <- data.frame(
    text_start = numeric(),
    text_end = numeric(),
    pattern_start = numeric(),
    pattern_end = numeric(),
    cost = integer(),
    strand = character(),
    stringsAsFactors = FALSE
  )
  class(out) <- c("sassy_matches", "data.frame")
  out
}

sassy_tail_raw <- function(x, n) {
  n <- min(length(x), n)
  if (n <= 0L) {
    return(raw())
  }
  x[seq.int(length(x) - n + 1L, length(x))]
}

rbind_sassy_matches <- function(parts) {
  if (!length(parts)) {
    return(empty_sassy_matches())
  }
  out <- do.call(rbind, unname(parts))
  row.names(out) <- NULL
  class(out) <- c("sassy_matches", "data.frame")
  out
}

#' Create a reusable 'sassy' searcher
#'
#' @param alphabet Alphabet profile. One of `"dna"`, `"iupac"`, or `"ascii"`.
#' @param rc If `TRUE`, search reverse-complement strand as well where supported.
#' @param alpha Optional IUPAC overhang cost in `[0, 1]`. Use `NULL` to disable.
#' @return An external pointer with class `sassy_searcher`.
#' @export
sassy_searcher <- function(alphabet = c("dna", "iupac", "ascii"), rc = TRUE, alpha = NULL) {
  alphabet <- match.arg(alphabet)
  ptr <- .Call("RC_sassy_searcher_new", alphabet, isTRUE(rc), check_sassy_alpha(alpha), PACKAGE = "Rsassy")
  class(ptr) <- "sassy_searcher"
  ptr
}

#' Search with a reusable 'sassy' searcher
#'
#' @param searcher A searcher created by [sassy_searcher()].
#' @param pattern,text Raw vectors or character scalars containing the query and target text.
#' @param k Maximum edit distance.
#' @param all If `FALSE`, return local-minimum matches. If `TRUE`, return all end positions with score <= `k`.
#' @return A data frame with 0-based half-open coordinates: `text_start`, `text_end`, `pattern_start`, `pattern_end`, `cost`, and `strand`.
#' @export
sassy_searcher_search <- function(searcher, pattern, text, k, all = FALSE) {
  if (!inherits(searcher, "sassy_searcher")) {
    stop("searcher must be created by sassy_searcher()", call. = FALSE)
  }
  out <- .Call(
    "RC_sassy_searcher_search",
    searcher,
    as_sassy_sequence(pattern, "pattern"),
    as_sassy_sequence(text, "text"),
    check_sassy_k(k),
    isTRUE(all),
    PACKAGE = "Rsassy"
  )
  class(out) <- c("sassy_matches", "data.frame")
  out
}

#' Search approximate matches with 'sassy'
#'
#' @inheritParams sassy_searcher
#' @param pattern,text Raw vectors or character scalars containing the query and target text.
#' @param k Maximum edit distance.
#' @param all If `FALSE`, return local-minimum matches. If `TRUE`, return all end positions with score <= `k`.
#' @return A data frame with 0-based half-open coordinates: `text_start`, `text_end`, `pattern_start`, `pattern_end`, `cost`, and `strand`.
#' @export
sassy_search <- function(pattern, text, k, alphabet = c("dna", "iupac", "ascii"), rc = TRUE, alpha = NULL, all = FALSE) {
  searcher <- sassy_searcher(alphabet = alphabet, rc = rc, alpha = alpha)
  sassy_searcher_search(searcher, pattern = pattern, text = text, k = k, all = all)
}

#' Search an R connection with 'sassy'
#'
#' Streams bytes from an already-open readable R connection through the C/R API
#' boundary. This avoids an R-level `readBin()`/`readChar()` loop while still
#' preserving matches that cross chunk boundaries by keeping an overlap window.
#'
#' @inheritParams sassy_search
#' @param pattern Raw vector or character scalar containing the query pattern.
#' @param con An open readable R connection, preferably opened in binary mode.
#' @param chunk_size Number of new bytes to read per native chunk.
#' @param overlap Number of bytes to carry from one chunk to the next. Defaults
#'   to `pattern_bytes + k` without overhang, and `2 * pattern_bytes + k` when
#'   `alpha` is set.
#' @return A data frame of matches with coordinates relative to the full stream.
#' @export
sassy_search_connection <- function(pattern,
                                    con,
                                    k,
                                    alphabet = c("dna", "iupac", "ascii"),
                                    rc = TRUE,
                                    alpha = NULL,
                                    all = FALSE,
                                    chunk_size = 1024 * 1024,
                                    overlap = NULL) {
  if (!inherits(con, "connection")) {
    stop("con must be an R connection", call. = FALSE)
  }
  if (!isOpen(con, "read")) {
    stop("con must be open for reading, preferably in binary mode", call. = FALSE)
  }

  pattern_bytes <- sassy_sequence_nbytes(pattern, "pattern")
  k <- check_sassy_k(k)
  alpha <- check_sassy_alpha(alpha)
  if (is.null(overlap)) {
    overlap <- if (is.nan(alpha)) pattern_bytes + k else 2L * pattern_bytes + k
  }
  if (!is.numeric(chunk_size) || length(chunk_size) != 1L || is.na(chunk_size) || chunk_size < 1) {
    stop("chunk_size must be a positive number", call. = FALSE)
  }
  if (!is.numeric(overlap) || length(overlap) != 1L || is.na(overlap) || overlap < 0) {
    stop("overlap must be a non-negative number", call. = FALSE)
  }

  searcher <- sassy_searcher(alphabet = alphabet, rc = rc, alpha = if (is.nan(alpha)) NULL else alpha)
  out <- .Call(
    "RC_sassy_searcher_search_connection",
    searcher,
    as_sassy_sequence(pattern, "pattern"),
    con,
    k,
    isTRUE(all),
    as.numeric(chunk_size),
    as.numeric(overlap),
    PACKAGE = "Rsassy"
  )
  class(out) <- c("sassy_matches", "data.frame")
  out
}
