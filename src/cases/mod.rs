use crate::{
    util::constraints::Constraints,
    workloads::benchmark::BenchArgs,
};
use anyhow::Result;

#[derive(Debug)]
pub struct Test {
    pub name: &'static str,
    pub test_fn: fn() -> Result<()>,
    pub constraints: Option<Constraints>,
}

#[derive(Debug)]
pub struct Benchmark {
    pub name: &'static str,
    pub test_fn: fn(&BenchArgs) -> Result<()>,
    pub constraints: Option<Constraints>,
}

#[macro_export]
macro_rules! test {
    ($name:expr, $func:ident, $constraints:expr) => {
        inventory::submit! {
            $crate::cases::Test {
                name: $name,
                test_fn: || $func(),
                constraints: $constraints,
            }
        }
    };
}

#[macro_export]
macro_rules! benchmark {
    ($name:expr, $func:ident, ($($param:expr),+ $(,)?), $constraints:expr) => {
        $(
            inventory::submit! {
                $crate::cases::Benchmark {
                    name: format!("{}/{}", $name, $param),
                    test_fn: |c| $func(c, $param),
                    constraints: $constraints,
                    benchmark: true,
                }
            }
        )+
    };
    ($name:expr, $func:ident, (), $constraints:expr) => {
        inventory::submit! {
            $crate::cases::Benchmark {
                name: $name,
                test_fn: |c| $func(c),
                constraints: $constraints,
            }
        }
    };
}

pub mod basic;
pub mod topology;

inventory::collect!(Test);
inventory::collect!(Benchmark);
