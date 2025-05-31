//use hdrhistogram::Histogram;
//use std::HashMap;
use crate::workloads::context::Context;

#[derive(Debug)]
pub struct Test {
   pub name: &'static str,
   pub test_fn: fn(&mut Context) -> Result<(), String>,
}

/*
#[derive(Debug)]
pub struct Benchmark {
   pub name: &'static str,
   pub test_fn: fn(&mut Context, &Duration) -> HashMap<String, Histogram<f64>>,
}
   */

pub mod basic;
// pub mod hyperthreads;

inventory::collect!(Test);
// inventory::collect!(Benchmark);
