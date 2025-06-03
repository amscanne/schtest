use crate::workloads::benchmark::BenchArgs;
use anyhow::Result;

#[derive(Debug)]
pub struct Test {
    pub name: fn() -> String,
    pub test_fn: fn() -> Result<()>,
    pub constraints: fn() -> Result<()>,
}

#[derive(Debug)]
pub struct Benchmark {
    pub name: fn() -> String,
    pub test_fn: fn(&BenchArgs) -> Result<()>,
    pub constraints: fn() -> Result<()>,
}

#[macro_export]
macro_rules! test {
    ($name:expr, $func:ident, ($($param:expr),+ $(,)?), $constraints:ident) => {
        $(
            inventory::submit! {
                $crate::cases::Test {
                    name: || format!("{}/{}", $name, $param),
                    test_fn: |c| $func(c, $param),
                    constraints: || $constraints($param),
                }
            }
        )+
    };
    ($name:expr, $func:ident, ($($param:expr),+ $(,)?)) => {
        $(
            inventory::submit! {
                $crate::cases::Test {
                    name: || format!("{}/{}", $name, $param),
                    test_fn: |c| $func(c, $param),
                    constraints: || Ok(()),
                }
            }
        )+
    };
    ($name:expr, $func:ident, $constraints:ident) => {
        inventory::submit! {
            $crate::cases::Test {
                name: || $name.to_string(),
                test_fn: || $func(),
                constraints: || $constraints(),
            }
        }
    };
    ($name:expr, $func:ident) => {
        inventory::submit! {
            $crate::cases::Test {
                name: || $name.to_string(),
                test_fn: || $func(),
                constraints: || Ok(())
            }
        }
    };
}

#[macro_export]
macro_rules! benchmark {
    ($name:expr, $func:ident, ($($param:expr),+ $(,)?), $constraints:ident) => {
        $(
            inventory::submit! {
                $crate::cases::Benchmark {
                    name: || format!("{}/{}", $name, $param),
                    test_fn: |c| $func(c, $param),
                    constraints: || $constraints($param),
                }
            }
        )+
    };
    ($name:expr, $func:ident, ($($param:expr),+ $(,)?)) => {
        $(
            inventory::submit! {
                $crate::cases::Benchmark {
                    name: || format!("{}/{}", $name, $param),
                    test_fn: |c| $func(c, $param),
                    constraints: || Ok(()),
                }
            }
        )+
    };
    ($name:expr, $func:ident, (), $constraints:ident) => {
        inventory::submit! {
            $crate::cases::Benchmark {
                name: || $name.to_string(),
                test_fn: |c| $func(c),
                constraints: || $constraints(),
            }
        }
    };
    ($name:expr, $func:ident, ()) => {
        inventory::submit! {
            $crate::cases::Benchmark {
                name: || $name.to_string(),
                test_fn: |c| $func(c),
                constraints: || Ok(()),
            }
        }
    };
}

pub mod basic;
pub mod topology;

inventory::collect!(Test);
inventory::collect!(Benchmark);
