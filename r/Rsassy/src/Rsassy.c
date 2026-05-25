#include <R.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>
/* R_ext/Connections.h is R's experimental connection API. Rsassy keeps all
 * connection access isolated in this file so higher-level R code does not need
 * readBin()/readChar() loops for streaming input. */
#include <R_ext/Connections.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

typedef struct RsassySearcher RsassySearcher;

typedef struct RsassyMatch {
    uintptr_t text_start;
    uintptr_t text_end;
    uintptr_t pattern_start;
    uintptr_t pattern_end;
    int32_t cost;
    uint8_t strand;
} RsassyMatch;

extern int rsassy_searcher_new(const char *alphabet, bool rc, float alpha, RsassySearcher **out);
extern void rsassy_searcher_free(RsassySearcher *searcher);
extern int rsassy_searcher_search(RsassySearcher *searcher,
                                  const uint8_t *pattern,
                                  uintptr_t pattern_len,
                                  const uint8_t *text,
                                  uintptr_t text_len,
                                  uintptr_t k,
                                  bool all_matches,
                                  RsassyMatch **out_matches,
                                  uintptr_t *out_len);
extern void rsassy_matches_free(RsassyMatch *matches, uintptr_t len);
extern const char *rsassy_last_error_message(void);

static void Rsassy_stop_last_error(void) {
    const char *message = rsassy_last_error_message();
    if (message == NULL || message[0] == '\0') {
        message = "unknown Rsassy native error";
    }
    Rf_error("%s", message);
}

static void Rsassy_searcher_finalizer(SEXP xp) {
    RsassySearcher *searcher = (RsassySearcher *)R_ExternalPtrAddr(xp);
    if (searcher != NULL) {
        rsassy_searcher_free(searcher);
        R_ClearExternalPtr(xp);
    }
}

SEXP RC_sassy_searcher_new(SEXP alphabet_s, SEXP rc_s, SEXP alpha_s) {
    if (TYPEOF(alphabet_s) != STRSXP || XLENGTH(alphabet_s) != 1 || STRING_ELT(alphabet_s, 0) == NA_STRING) {
        Rf_error("alphabet must be a non-missing character scalar");
    }
    if (TYPEOF(alpha_s) != REALSXP || XLENGTH(alpha_s) != 1) {
        Rf_error("alpha must be a numeric scalar");
    }

    int rc = Rf_asLogical(rc_s);
    if (rc == NA_LOGICAL) {
        Rf_error("rc must be TRUE or FALSE");
    }

    const char *alphabet = CHAR(STRING_ELT(alphabet_s, 0));
    double alpha = REAL(alpha_s)[0];
    float alpha_f = isnan(alpha) ? NAN : (float)alpha;

    RsassySearcher *searcher = NULL;
    if (rsassy_searcher_new(alphabet, rc == TRUE, alpha_f, &searcher) != 0) {
        Rsassy_stop_last_error();
    }

    SEXP xp = PROTECT(R_MakeExternalPtr(searcher, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(xp, Rsassy_searcher_finalizer, TRUE);
    UNPROTECT(1);
    return xp;
}

struct RsassySeqView {
    const uint8_t *data;
    uintptr_t len;
};

static struct RsassySeqView Rsassy_sequence_view(SEXP x, const char *arg) {
    struct RsassySeqView view;
    view.data = NULL;
    view.len = 0;

    if (TYPEOF(x) == RAWSXP) {
        R_xlen_t len = XLENGTH(x);
        if ((uint64_t)len > (uint64_t)SIZE_MAX) {
            Rf_error("%s is too large for this platform", arg);
        }
        view.len = (uintptr_t)len;
        if (len > 0) {
            /* DATAPTR_OR_NULL lets ALTREP raw-vector implementations expose an
             * existing contiguous buffer without forcing materialization. If no
             * contiguous pointer is available, DATAPTR_RO may materialize. */
            const void *ptr = DATAPTR_OR_NULL(x);
            if (ptr == NULL) {
                ptr = DATAPTR_RO(x);
            }
            view.data = (const uint8_t *)ptr;
        }
        return view;
    }

    if (TYPEOF(x) == STRSXP && XLENGTH(x) == 1 && STRING_ELT(x, 0) != NA_STRING) {
        SEXP ch = STRING_ELT(x, 0);
        R_xlen_t len = XLENGTH(ch);
        if ((uint64_t)len > (uint64_t)SIZE_MAX) {
            Rf_error("%s is too large for this platform", arg);
        }
        view.len = (uintptr_t)len;
        view.data = len > 0 ? (const uint8_t *)CHAR(ch) : NULL;
        return view;
    }

    Rf_error("%s must be a raw vector or a non-missing character scalar", arg);
    return view;
}

static RsassySearcher *Rsassy_searcher_from_xptr(SEXP xp) {
    if (TYPEOF(xp) != EXTPTRSXP) {
        Rf_error("searcher must be an external pointer created by sassy_searcher()");
    }
    RsassySearcher *searcher = (RsassySearcher *)R_ExternalPtrAddr(xp);
    if (searcher == NULL) {
        Rf_error("sassy searcher pointer is no longer valid");
    }
    return searcher;
}

struct RsassyMatchVec {
    RsassyMatch *data;
    uintptr_t len;
    uintptr_t cap;
};

static void Rsassy_match_vec_free(struct RsassyMatchVec *vec) {
    free(vec->data);
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static void Rsassy_match_vec_push(struct RsassyMatchVec *vec, RsassyMatch match) {
    if (vec->len == vec->cap) {
        uintptr_t new_cap = vec->cap == 0 ? 64 : vec->cap * 2;
        RsassyMatch *new_data = (RsassyMatch *)realloc(vec->data, (size_t)new_cap * sizeof(RsassyMatch));
        if (new_data == NULL) {
            Rsassy_match_vec_free(vec);
            Rf_error("failed to allocate match accumulator");
        }
        vec->data = new_data;
        vec->cap = new_cap;
    }
    vec->data[vec->len++] = match;
}

static SEXP Rsassy_matches_data_frame(const RsassyMatch *matches, uintptr_t n) {
    if ((uint64_t)n > (uint64_t)INT_MAX) {
        Rf_error("too many matches to return as an R data frame");
    }

    R_xlen_t rn = (R_xlen_t)n;
    SEXP text_start = PROTECT(Rf_allocVector(REALSXP, rn));
    SEXP text_end = PROTECT(Rf_allocVector(REALSXP, rn));
    SEXP pattern_start = PROTECT(Rf_allocVector(REALSXP, rn));
    SEXP pattern_end = PROTECT(Rf_allocVector(REALSXP, rn));
    SEXP cost = PROTECT(Rf_allocVector(INTSXP, rn));
    SEXP strand = PROTECT(Rf_allocVector(STRSXP, rn));

    for (R_xlen_t i = 0; i < rn; i++) {
        REAL(text_start)[i] = (double)matches[i].text_start;
        REAL(text_end)[i] = (double)matches[i].text_end;
        REAL(pattern_start)[i] = (double)matches[i].pattern_start;
        REAL(pattern_end)[i] = (double)matches[i].pattern_end;
        INTEGER(cost)[i] = (int)matches[i].cost;
        SET_STRING_ELT(strand, i, Rf_mkChar(matches[i].strand == 0 ? "+" : "-"));
    }

    SEXP out = PROTECT(Rf_allocVector(VECSXP, 6));
    SET_VECTOR_ELT(out, 0, text_start);
    SET_VECTOR_ELT(out, 1, text_end);
    SET_VECTOR_ELT(out, 2, pattern_start);
    SET_VECTOR_ELT(out, 3, pattern_end);
    SET_VECTOR_ELT(out, 4, cost);
    SET_VECTOR_ELT(out, 5, strand);

    SEXP names = PROTECT(Rf_allocVector(STRSXP, 6));
    SET_STRING_ELT(names, 0, Rf_mkChar("text_start"));
    SET_STRING_ELT(names, 1, Rf_mkChar("text_end"));
    SET_STRING_ELT(names, 2, Rf_mkChar("pattern_start"));
    SET_STRING_ELT(names, 3, Rf_mkChar("pattern_end"));
    SET_STRING_ELT(names, 4, Rf_mkChar("cost"));
    SET_STRING_ELT(names, 5, Rf_mkChar("strand"));
    Rf_setAttrib(out, R_NamesSymbol, names);

    SEXP row_names = PROTECT(Rf_allocVector(INTSXP, 2));
    INTEGER(row_names)[0] = NA_INTEGER;
    INTEGER(row_names)[1] = -(int)n;
    Rf_setAttrib(out, R_RowNamesSymbol, row_names);

    SEXP klass = PROTECT(Rf_mkString("data.frame"));
    Rf_setAttrib(out, R_ClassSymbol, klass);

    UNPROTECT(10);
    return out;
}

SEXP RC_sassy_searcher_search(SEXP searcher_s, SEXP pattern_s, SEXP text_s, SEXP k_s, SEXP all_s) {
    RsassySearcher *searcher = Rsassy_searcher_from_xptr(searcher_s);
    struct RsassySeqView pattern = Rsassy_sequence_view(pattern_s, "pattern");
    struct RsassySeqView text = Rsassy_sequence_view(text_s, "text");

    int k = Rf_asInteger(k_s);
    if (k == NA_INTEGER || k < 0) {
        Rf_error("k must be a non-negative integer");
    }
    int all = Rf_asLogical(all_s);
    if (all == NA_LOGICAL) {
        Rf_error("all must be TRUE or FALSE");
    }

    RsassyMatch *matches = NULL;
    uintptr_t n_matches = 0;
    if (rsassy_searcher_search(searcher,
                               pattern.data,
                               pattern.len,
                               text.data,
                               text.len,
                               (uintptr_t)k,
                               all == TRUE,
                               &matches,
                               &n_matches) != 0) {
        Rsassy_stop_last_error();
    }

    SEXP out = Rsassy_matches_data_frame(matches, n_matches);
    rsassy_matches_free(matches, n_matches);
    return out;
}

SEXP RC_sassy_searcher_search_connection(SEXP searcher_s,
                                         SEXP pattern_s,
                                         SEXP connection_s,
                                         SEXP k_s,
                                         SEXP all_s,
                                         SEXP chunk_size_s,
                                         SEXP overlap_s) {
    RsassySearcher *searcher = Rsassy_searcher_from_xptr(searcher_s);
    struct RsassySeqView pattern = Rsassy_sequence_view(pattern_s, "pattern");

    int k = Rf_asInteger(k_s);
    if (k == NA_INTEGER || k < 0) {
        Rf_error("k must be a non-negative integer");
    }
    int all = Rf_asLogical(all_s);
    if (all == NA_LOGICAL) {
        Rf_error("all must be TRUE or FALSE");
    }

    double chunk_size_d = Rf_asReal(chunk_size_s);
    double overlap_d = Rf_asReal(overlap_s);
    if (!R_FINITE(chunk_size_d) || chunk_size_d < 1 || chunk_size_d > (double)SIZE_MAX) {
        Rf_error("chunk_size must be a positive finite size");
    }
    if (!R_FINITE(overlap_d) || overlap_d < 0 || overlap_d > (double)SIZE_MAX) {
        Rf_error("overlap must be a non-negative finite size");
    }

    size_t chunk_size = (size_t)chunk_size_d;
    size_t overlap = (size_t)overlap_d;
    if (overlap > SIZE_MAX - chunk_size) {
        Rf_error("chunk_size + overlap is too large for this platform");
    }

    Rconnection con = R_GetConnection(connection_s);
    if (con == NULL) {
        Rf_error("con must be an R connection");
    }
    if (!con->isopen || !con->canread) {
        Rf_error("con must be an open readable connection, preferably opened in binary mode");
    }

    uint8_t *window = (uint8_t *)malloc(chunk_size + overlap);
    if (window == NULL) {
        Rf_error("failed to allocate streaming buffer");
    }

    struct RsassyMatchVec acc = {0};
    size_t carry_len = 0;
    uintptr_t bytes_read_total = 0;

    for (;;) {
        size_t n_read = R_ReadConnection(con, window + carry_len, chunk_size);
        if (n_read == 0) {
            break;
        }

        uintptr_t chunk_start = bytes_read_total - (uintptr_t)carry_len;
        uintptr_t new_bytes_start = bytes_read_total;
        uintptr_t chunk_len = (uintptr_t)(carry_len + n_read);
        bytes_read_total += (uintptr_t)n_read;

        RsassyMatch *matches = NULL;
        uintptr_t n_matches = 0;
        if (rsassy_searcher_search(searcher,
                                   pattern.data,
                                   pattern.len,
                                   window,
                                   chunk_len,
                                   (uintptr_t)k,
                                   all == TRUE,
                                   &matches,
                                   &n_matches) != 0) {
            free(window);
            Rsassy_match_vec_free(&acc);
            Rsassy_stop_last_error();
        }

        for (uintptr_t i = 0; i < n_matches; i++) {
            RsassyMatch match = matches[i];
            uintptr_t global_end = chunk_start + match.text_end;
            if (global_end <= new_bytes_start) {
                continue;
            }
            match.text_start += chunk_start;
            match.text_end = global_end;
            Rsassy_match_vec_push(&acc, match);
        }
        rsassy_matches_free(matches, n_matches);

        carry_len = overlap < (size_t)chunk_len ? overlap : (size_t)chunk_len;
        if (carry_len > 0) {
            memmove(window, window + chunk_len - carry_len, carry_len);
        }
    }

    free(window);
    SEXP out = Rsassy_matches_data_frame(acc.data, acc.len);
    Rsassy_match_vec_free(&acc);
    return out;
}

static const R_CallMethodDef CallEntries[] = {
    {"RC_sassy_searcher_new", (DL_FUNC)&RC_sassy_searcher_new, 3},
    {"RC_sassy_searcher_search", (DL_FUNC)&RC_sassy_searcher_search, 5},
    {"RC_sassy_searcher_search_connection", (DL_FUNC)&RC_sassy_searcher_search_connection, 7},
    {NULL, NULL, 0}
};

void R_init_Rsassy(DllInfo *dll) {
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
