#include <R.h>
#include <R_ext/Rdynload.h>
#include <Rinternals.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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

static const R_CallMethodDef CallEntries[] = {
    {"RC_sassy_searcher_new", (DL_FUNC)&RC_sassy_searcher_new, 3},
    {"RC_sassy_searcher_search", (DL_FUNC)&RC_sassy_searcher_search, 5},
    {NULL, NULL, 0}
};

void R_init_Rsassy(DllInfo *dll) {
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
