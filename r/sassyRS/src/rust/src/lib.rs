use extendr_api::prelude::*;
use sassy::profiles::{Ascii, Dna, Iupac};
use sassy::{Match as SassyMatch, Searcher, Strand};

fn error<T>(message: impl Into<String>) -> extendr_api::Result<T> {
    Err(extendr_api::Error::Other(message.into()))
}

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
    pattern: &str,
    text: &str,
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
    let pattern = pattern.as_bytes();
    let text = text.as_bytes();
    let k = k as usize;

    let matches = match alphabet.as_str() {
        "ascii" => {
            let mut searcher = Searcher::<Ascii>::new(rc, None);
            if all {
                searcher.search_all(pattern, text, k)
            } else {
                searcher.search(pattern, text, k)
            }
        }
        "dna" => {
            let mut searcher = Searcher::<Dna>::new(rc, None);
            if all {
                searcher.search_all(pattern, text, k)
            } else {
                searcher.search(pattern, text, k)
            }
        }
        "iupac" => {
            let mut searcher = Searcher::<Iupac>::new(rc, alpha);
            if all {
                searcher.search_all(pattern, text, k)
            } else {
                searcher.search(pattern, text, k)
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
