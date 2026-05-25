use sassy::profiles::{Ascii, Dna, Iupac};
use sassy::{Match as SassyMatch, Searcher, Strand};
use std::cell::RefCell;
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::ptr;
use std::slice;

thread_local! {
    static LAST_ERROR: RefCell<Option<CString>> = const { RefCell::new(None) };
}

enum SearcherType {
    Ascii(Searcher<Ascii>),
    Dna(Searcher<Dna>),
    Iupac(Searcher<Iupac>),
}

pub struct RsassySearcher {
    inner: SearcherType,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct RsassyMatch {
    pub text_start: usize,
    pub text_end: usize,
    pub pattern_start: usize,
    pub pattern_end: usize,
    pub cost: i32,
    /// 0 = forward, 1 = reverse-complement.
    pub strand: u8,
}

impl From<SassyMatch> for RsassyMatch {
    fn from(value: SassyMatch) -> Self {
        Self {
            text_start: value.text_start,
            text_end: value.text_end,
            pattern_start: value.pattern_start,
            pattern_end: value.pattern_end,
            cost: value.cost,
            strand: match value.strand {
                Strand::Fwd => 0,
                Strand::Rc => 1,
            },
        }
    }
}

fn set_last_error(message: impl Into<String>) -> c_int {
    let message = message.into().replace('\0', "\\0");
    LAST_ERROR.with(|slot| {
        *slot.borrow_mut() = Some(CString::new(message).expect("interior NULs were replaced"));
    });
    1
}

fn clear_last_error() {
    LAST_ERROR.with(|slot| {
        *slot.borrow_mut() = None;
    });
}

fn guard_ffi(fun: impl FnOnce() -> Result<(), String>) -> c_int {
    clear_last_error();
    match catch_unwind(AssertUnwindSafe(fun)) {
        Ok(Ok(())) => 0,
        Ok(Err(err)) => set_last_error(err),
        Err(payload) => {
            let message = if let Some(msg) = payload.downcast_ref::<&str>() {
                (*msg).to_string()
            } else if let Some(msg) = payload.downcast_ref::<String>() {
                msg.clone()
            } else {
                "Rust panic crossing Rsassy FFI boundary".to_string()
            };
            set_last_error(message)
        }
    }
}

unsafe fn bytes_from_raw<'a>(ptr: *const u8, len: usize, arg: &str) -> Result<&'a [u8], String> {
    if len == 0 {
        return Ok(&[]);
    }
    if ptr.is_null() {
        return Err(format!(
            "{arg} pointer must not be NULL when length is non-zero"
        ));
    }
    Ok(unsafe { slice::from_raw_parts(ptr, len) })
}

fn parse_alphabet_and_alpha(
    alphabet: *const c_char,
    alpha: f32,
) -> Result<(String, Option<f32>), String> {
    if alphabet.is_null() {
        return Err("alphabet pointer must not be NULL".to_string());
    }
    let alphabet = unsafe { CStr::from_ptr(alphabet) }
        .to_str()
        .map_err(|_| "alphabet must be valid UTF-8".to_string())?
        .to_ascii_lowercase();

    let alpha = if alpha.is_nan() {
        None
    } else {
        if !(0.0..=1.0).contains(&alpha) {
            return Err("alpha must be NaN or in [0, 1]".to_string());
        }
        if alphabet != "iupac" {
            return Err("alpha overhang cost is only supported for alphabet = 'iupac'".to_string());
        }
        Some(alpha)
    };

    Ok((alphabet, alpha))
}

#[no_mangle]
pub extern "C" fn rsassy_searcher_new(
    alphabet: *const c_char,
    rc: bool,
    alpha: f32,
    out: *mut *mut RsassySearcher,
) -> c_int {
    guard_ffi(|| {
        if out.is_null() {
            return Err("out searcher pointer must not be NULL".to_string());
        }

        let (alphabet, alpha) = parse_alphabet_and_alpha(alphabet, alpha)?;
        let inner = match alphabet.as_str() {
            "ascii" => SearcherType::Ascii(Searcher::<Ascii>::new(rc, None)),
            "dna" => SearcherType::Dna(Searcher::<Dna>::new(rc, None)),
            "iupac" => SearcherType::Iupac(Searcher::<Iupac>::new(rc, alpha)),
            _ => return Err(format!("unsupported alphabet: {alphabet}")),
        };

        let boxed = Box::new(RsassySearcher { inner });
        unsafe {
            *out = Box::into_raw(boxed);
        }
        Ok(())
    })
}

#[no_mangle]
pub extern "C" fn rsassy_searcher_free(searcher: *mut RsassySearcher) {
    if !searcher.is_null() {
        unsafe {
            drop(Box::from_raw(searcher));
        }
    }
}

#[no_mangle]
pub extern "C" fn rsassy_searcher_search(
    searcher: *mut RsassySearcher,
    pattern: *const u8,
    pattern_len: usize,
    text: *const u8,
    text_len: usize,
    k: usize,
    all_matches: bool,
    out_matches: *mut *mut RsassyMatch,
    out_len: *mut usize,
) -> c_int {
    guard_ffi(|| {
        if searcher.is_null() {
            return Err("searcher pointer must not be NULL".to_string());
        }
        if out_matches.is_null() || out_len.is_null() {
            return Err("output match pointers must not be NULL".to_string());
        }

        let pattern = unsafe { bytes_from_raw(pattern, pattern_len, "pattern")? };
        let text = unsafe { bytes_from_raw(text, text_len, "text")? };
        let searcher = unsafe { &mut *searcher };

        let matches: Vec<SassyMatch> = match &mut searcher.inner {
            SearcherType::Ascii(searcher) => {
                if all_matches {
                    searcher.search_all(pattern, text, k)
                } else {
                    searcher.search(pattern, text, k)
                }
            }
            SearcherType::Dna(searcher) => {
                if all_matches {
                    searcher.search_all(pattern, text, k)
                } else {
                    searcher.search(pattern, text, k)
                }
            }
            SearcherType::Iupac(searcher) => {
                if all_matches {
                    searcher.search_all(pattern, text, k)
                } else {
                    searcher.search(pattern, text, k)
                }
            }
        };

        let mut matches: Vec<RsassyMatch> = matches.into_iter().map(RsassyMatch::from).collect();
        if matches.is_empty() {
            unsafe {
                *out_matches = ptr::null_mut();
                *out_len = 0;
            }
            return Ok(());
        }

        matches.shrink_to_fit();
        let len = matches.len();
        let ptr = matches.as_mut_ptr();
        std::mem::forget(matches);

        unsafe {
            *out_matches = ptr;
            *out_len = len;
        }
        Ok(())
    })
}

#[no_mangle]
pub extern "C" fn rsassy_matches_free(matches: *mut RsassyMatch, len: usize) {
    if !matches.is_null() {
        unsafe {
            drop(Vec::from_raw_parts(matches, len, len));
        }
    }
}

#[no_mangle]
pub extern "C" fn rsassy_last_error_message() -> *const c_char {
    static EMPTY: &[u8] = b"\0";
    LAST_ERROR.with(|slot| match slot.borrow().as_ref() {
        Some(message) => message.as_ptr(),
        None => EMPTY.as_ptr().cast(),
    })
}
