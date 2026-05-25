matches <- sassy_search("ACGT", "TTACGTAA", 0, alphabet = "dna", rc = FALSE)
expect_true(is.data.frame(matches))
expect_equal(nrow(matches), 1L)
expect_equal(matches$text_start, 2)
expect_equal(matches$text_end, 6)
expect_equal(matches$pattern_start, 0)
expect_equal(matches$pattern_end, 4)
expect_equal(matches$cost, 0L)
expect_equal(matches$strand, "+")

searcher <- sassy_searcher("dna", rc = TRUE)
rc_matches <- sassy_searcher_search(searcher, "ACGT", "TTACGTAA", 0)
expect_true(nrow(rc_matches) >= 1L)

raw_matches <- sassy_search(charToRaw("ACGT"), charToRaw("TTACGTAA"), 0, alphabet = "dna", rc = FALSE)
expect_equal(raw_matches, matches)

expect_error(sassy_search("ACGT", "TTACGTAA", -1, alphabet = "dna"))
expect_error(sassy_search("ACGT", "TTACGTAA", 0, alphabet = "dna", alpha = 0.5))
