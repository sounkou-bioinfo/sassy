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

#' Search all approximate-match end positions with 'sassy'
#'
#' Convenience wrapper around [sassy_search()] with `all = TRUE`.
#'
#' @inheritParams sassy_search
#' @return A data frame of matches.
#' @export
sassy_search_all <- function(pattern, text, k, alphabet = c("dna", "iupac", "ascii"), rc = TRUE, alpha = NULL) {
  sassy_search(pattern = pattern, text = text, k = k, alphabet = alphabet, rc = rc, alpha = alpha, all = TRUE)
}
