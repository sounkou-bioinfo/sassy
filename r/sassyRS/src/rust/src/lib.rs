use extendr_api::prelude::*;
use extendr_ffi::{
    ALTREP, DATAPTR_OR_NULL, DATAPTR_RO, R_CHAR, R_NaString, Rf_xlength, SEXPTYPE, STRING_ELT,
    TYPEOF, XLENGTH,
};
use sassy::profiles::{Ascii, Dna, Iupac};
use sassy::{Match as SassyMatch, Searcher, Strand};
use std::slice;

fn error<T>(message: impl Into<String>) -> extendr_api::Result<T> {
    Err(extendr_api::Error::Other(message.into()))
}

struct SequenceBytes<'a>(&'a [u8]);

fn parse_alpha(alphabet: &str, alpha: f64) -> extendr_api::Result<Option<f32>> {
    if alpha.is_nan() {
        return Ok(None);
    }
    if !(0.0..=1.0).contains(&alpha) {
        return error("alpha must be NaN or a value in [0, 1]");
    }
    if alphabet != "iupac" {
        return error("alpha overhang cost is only supported for alphabet = 'iupac'");
    }
    Ok(Some(alpha as f32))
}

fn sequence_bytes<'a>(obj: &'a Robj, arg: &str) -> extendr_api::Result<SequenceBytes<'a>> {
    let sexp = unsafe { obj.get() };

    match unsafe { TYPEOF(sexp) } {
        SEXPTYPE::RAWSXP => {
            let len = unsafe { XLENGTH(sexp) as usize };
            let bytes = if len == 0 {
                &[]
            } else {
                // For ALTREP raw vectors, first ask whether a data pointer is
                // already available. If not, DATAPTR_RO() lets R materialize a
                // read-only contiguous buffer through the ALTREP API.
                let mut ptr = if unsafe { ALTREP(sexp) != 0 } {
                    (unsafe { DATAPTR_OR_NULL(sexp) }) as *const u8
                } else {
                    (unsafe { DATAPTR_RO(sexp) }) as *const u8
                };
                if ptr.is_null() {
                    ptr = unsafe { DATAPTR_RO(sexp) } as *const u8;
                }
                if ptr.is_null() {
                    return error(format!("{arg} raw vector has no readable data pointer"));
                }
                unsafe { slice::from_raw_parts(ptr, len) }
            };
            Ok(SequenceBytes(bytes))
        }
        SEXPTYPE::STRSXP => {
            if obj.len() != 1 {
                return error(format!("{arg} must be a raw vector or a character scalar"));
            }
            let ch = unsafe { STRING_ELT(sexp, 0) };
            if ch == unsafe { R_NaString } {
                return error(format!("{arg} must not be NA"));
            }
            let len = unsafe { Rf_xlength(ch) as usize };
            let bytes = if len == 0 {
                &[]
            } else {
                let ptr = unsafe { R_CHAR(ch) } as *const u8;
                unsafe { slice::from_raw_parts(ptr, len) }
            };
            Ok(SequenceBytes(bytes))
        }
        _ => error(format!("{arg} must be a raw vector or a character scalar")),
    }
}

fn matches_to_data_frame(matches: Vec<SassyMatch>) -> Robj {
    let text_start: Vec<f64> = matches.iter().map(|m| m.text_start as f64).collect();
    let text_end: Vec<f64> = matches.iter().map(|m| m.text_end as f64).collect();
    let pattern_start: Vec<f64> = matches.iter().map(|m| m.pattern_start as f64).collect();
    let pattern_end: Vec<f64> = matches.iter().map(|m| m.pattern_end as f64).collect();
    let cost: Vec<i32> = matches.iter().map(|m| m.cost).collect();
    let strand: Vec<&'static str> = matches
        .iter()
        .map(|m| match m.strand {
            Strand::Fwd => "+",
            Strand::Rc => "-",
        })
        .collect();

    data_frame!(
        text_start = text_start,
        text_end = text_end,
        pattern_start = pattern_start,
        pattern_end = pattern_end,
        cost = cost,
        strand = strand
    )
}

/// Search approximate matches with the Rust `sassy` crate.
#[extendr]
fn rs_search(
    pattern: Robj,
    text: Robj,
    k: i32,
    alphabet: &str,
    rc: bool,
    alpha: f64,
    all: bool,
) -> extendr_api::Result<Robj> {
    if k < 0 {
        return error("k must be non-negative");
    }

    let alphabet = alphabet.to_ascii_lowercase();
    let alpha = parse_alpha(&alphabet, alpha)?;
    let pattern = sequence_bytes(&pattern, "pattern")?;
    let text = sequence_bytes(&text, "text")?;
    let k = k as usize;

    let matches = match alphabet.as_str() {
        "ascii" => {
            let mut searcher = Searcher::<Ascii>::new(rc, None);
            if all {
                searcher.search_all(pattern.0, text.0, k)
            } else {
                searcher.search(pattern.0, text.0, k)
            }
        }
        "dna" => {
            let mut searcher = Searcher::<Dna>::new(rc, None);
            if all {
                searcher.search_all(pattern.0, text.0, k)
            } else {
                searcher.search(pattern.0, text.0, k)
            }
        }
        "iupac" => {
            let mut searcher = Searcher::<Iupac>::new(rc, alpha);
            if all {
                searcher.search_all(pattern.0, text.0, k)
            } else {
                searcher.search(pattern.0, text.0, k)
            }
        }
        _ => return error(format!("unsupported alphabet: {alphabet}")),
    };

    Ok(matches_to_data_frame(matches))
}

extendr_module! {
    mod sassyrs;
    fn rs_search;
}
