extern crate cmake;

use std::env;
use std::fs;
use std::path::{Path, PathBuf};


fn copy_all<P1, P2, I>(src_dir: P1, dst_dir: P2, files: I)
    where P1: AsRef<Path>,
          P2: AsRef<Path>,
          I: IntoIterator,
          I::Item: AsRef<Path>
{
    for f in files.into_iter() {
        let src = {
            let mut p = src_dir.as_ref().to_path_buf();
            p.push(f.as_ref());
            p
        };
        let dst = {
            let mut p = dst_dir.as_ref().to_path_buf();
            p.push(f.as_ref());
            p
        };
        fs::copy(&src, &dst).unwrap();
    }
}

fn main() {
    cmake::Config::new("..")
        .define("BUILD_RDIFF", "OFF")
        .define("ENABLE_COMPRESSION", "OFF")
        .build();

    let src = {
        let mut src = PathBuf::new();
        src.push(env::var("OUT_DIR").unwrap());
        src.push("build/src");
        src
    };
    let dst = {
        let mut dst = PathBuf::new();
        dst.push("../static");
        dst.push(env::var("TARGET").unwrap());
        dst
    };
    fs::create_dir_all(&dst).unwrap();
    copy_all(&src, &dst, vec!["config.h", "librsync-config.h"]);
    copy_all(&src, "../static", vec!["prototab.c", "prototab.h"]);
}
