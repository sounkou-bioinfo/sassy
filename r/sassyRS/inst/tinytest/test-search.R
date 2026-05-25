matches <- sassy_search("ACGT", "TTACGTAA", 0, alphabet = "dna", rc = FALSE)
expect_true(is.data.frame(matches))
expect_equal(nrow(matches), 1L)
expect_equal(matches$text_start, 2)
expect_equal(matches$text_end, 6)
expect_equal(matches$pattern_start, 0)
expect_equal(matches$pattern_end, 4)
expect_equal(matches$cost, 0L)
expect_equal(matches$strand, "+")

raw_matches <- sassy_search(charToRaw("ACGT"), charToRaw("TTACGTAA"), 0, alphabet = "dna", rc = FALSE)
expect_equal(raw_matches, matches)

nul_raw_matches <- sassy_search(as.raw(c(0x41, 0x00, 0x42)), as.raw(c(0x58, 0x41, 0x00, 0x42, 0x59)), 0, alphabet = "ascii", rc = FALSE)
expect_equal(nul_raw_matches$text_start, 1)
expect_equal(nul_raw_matches$text_end, 4)

expect_error(sassy_search("ACGT", "TTACGTAA", -1, alphabet = "dna"))
expect_error(sassy_search("ACGT", "TTACGTAA", 0, alphabet = "dna", alpha = 0.5))
