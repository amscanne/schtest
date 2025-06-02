//! Benchmarking utilities for the scheduler testing framework
//!
//! This module provides utilities for benchmarking workloads and measuring
//! performance metrics.

use std::time::{Duration, Instant};

use crate::util::stats::Distribution;
use anyhow::Result;
use criterion::{Criterion, SamplingMode, Throughput};

/// Represents the result of a benchmark measurement.
pub enum BenchResult {
    Latency(Distribution<std::time::Duration>),
    Count(u64),
}

// BenchArgs are benchmark arguments.
#[derive(Copy, Clone, Debug)]
pub struct BenchArgs {
    /// The function name.
    pub name: &'static str,

    /// The number of samples to collect for the benchmark.
    pub sample_size: usize,

    /// The significance level for the benchmark.
    pub significance_level: f64,

    /// The percentile to use for the benchmark.
    pub percentile: f64,
}

unsafe impl Send for BenchArgs {}
unsafe impl Sync for BenchArgs {}

/// Run a function with the given context and measurement function, until a basic
/// convergence criterion is met or the time limit is reached.
///
/// # Arguments
///
/// * `ctx` - The context in which to run the function
/// * `measure` - A function that performs measurements and returns a metric in [0.0, 1.0]
/// * `min_time` - The minimum time to run the function for (default: 1 second)
/// * `max_time` - The maximum time to run the function for (default: 10 seconds)
/// * `threshold` - The confidence threshold for convergence (default: 0.95)
///
/// # Returns
///
/// The last metric value collected from the function. The convergence will conclude
/// when the metric returned by `measure` is greater than or equal to `threshold` and
/// the minimum time has been reached.
pub fn converge<F>(
    min_time: Option<Duration>,
    max_time: Option<Duration>,
    threshold: Option<f64>,
    mut measure: F,
) -> Result<f64>
where
    F: FnMut(u32) -> Result<f64>,
{
    let min_time = min_time.unwrap_or(Duration::from_secs(1));
    let max_time = max_time.unwrap_or(Duration::from_secs(10));
    let threshold = threshold.unwrap_or(0.95);

    eprintln!("");
    eprintln!("converge: min_time={:.2}, max_time={:.2}, threshold={:.2}", min_time.as_secs_f64(), max_time.as_secs_f64(), threshold);
    let mut iters = 1u32;
    loop {
        let iter_start = Instant::now();
        let metric = measure(iters)?;
        let elapsed = iter_start.elapsed();
        eprintln!("converge: iters={:}, elapsed={:.2}, metric={:.2}", iters, elapsed.as_secs_f64(), metric);
        if metric >= threshold && elapsed >= min_time {
            return Ok(metric);
        }
        if elapsed > max_time {
            break;
        }
        if elapsed < min_time && iters < u32::MAX / 2 {
            let prev_iters = iters;
            if elapsed.as_nanos() == 0 {
                iters = iters.saturating_mul(100);
            } else {
                let min_time_ns = min_time.as_nanos() as f64;
                let elapsed_ns = elapsed.as_nanos() as f64;
                let mut next_iters = ((iters as f64) * min_time_ns / elapsed_ns).ceil() as u32;
                next_iters = next_iters
                    .max(prev_iters + 1)
                    .min(prev_iters.saturating_mul(100));
                iters = next_iters;
            }
            continue;
        } else if elapsed < max_time && iters < u32::MAX / 2 {
            iters = iters.saturating_mul(2);
        }
    }
    Ok(0.0)
}

/// Used as the default parameter type for single-case benchmarks.
#[derive(Clone, Debug)]
pub struct DefaultParam;

impl std::fmt::Display for DefaultParam {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "default")
    }
}

/// Run a benchmark for a single (non-parameterized) case.
///
/// # Arguments
///
/// * `name` - The name of the benchmark group
/// * `measure` - A closure that takes the iteration count (`u64`)
///
/// # Returns
///
/// Returns `Ok(())` if the benchmark runs successfully, or an error otherwise.
pub fn measure<F>(args: &BenchArgs, name: &str, mut measure: F) -> Result<()>
where
    F: FnMut(u32) -> Result<BenchResult>,
{
    let mut c = Criterion::default();
    c = c.sample_size(args.sample_size);
    c = c.significance_level(args.significance_level);

    eprintln!("measure: {}/{}", args.name, name);
    match measure(0) {
        Ok(BenchResult::Latency(_)) => {
            let mut group = c.benchmark_group(args.name);
            group.sampling_mode(SamplingMode::Flat);
            group.bench_function(name, |b| {
                b.iter_custom(|iters| {
                    let result = match measure(iters as u32) {
                        Ok(v) => v,
                        Err(e) => panic!("Benchmark failed: {:?}", e),
                    };
                    if let BenchResult::Latency(dist) = result {
                        let latency =
                            dist.estimates()
                                .percentile(args.percentile)
                                .unwrap_or(Duration::ZERO)
                                .as_secs_f64();
                        eprintln!("p{} latency: {:?}", (args.percentile * 100.0) as u32, Duration::from_secs_f64(latency));
                        Duration::from_secs_f64(latency * (iters as f64))
                    } else {
                        Duration::ZERO
                    }
                })
            });
            group.finish();
        }
        Ok(BenchResult::Count(_)) => {
            use std::sync::{Arc, Mutex};
            let last_throughput = Arc::new(Mutex::new(None));
            let last_throughput_clone = last_throughput.clone();
            let mut group = c.benchmark_group(args.name);
            group.bench_function(name, |b| {
                let last_throughput_clone = last_throughput_clone.clone();
                b.iter_custom(|iters| {
                    let start = Instant::now();
                    let result = match measure(iters as u32) {
                        Ok(v) => v,
                        Err(e) => panic!("Benchmark failed: {:?}", e),
                    };
                    let elapsed = start.elapsed();
                    if let BenchResult::Count(v) = result {
                        let mut lock = last_throughput_clone.lock().unwrap();
                        let val = v.clone();
                        eprintln!("throughput: {}/s", val as f64 / elapsed.as_secs_f64());
                        *lock = Some(val);
                    }
                    elapsed
                });
            });
            let count = last_throughput.lock().unwrap().clone().unwrap_or(1);
            group.throughput(Throughput::Elements(count as u64));
            group.finish();
        }
        _ => {
            panic!("Unsupported benchmark result type")
        }
    }
    Ok(())
}

#[macro_export]
macro_rules! converge {
    (($min:expr, $max:expr), $threshold:expr, $measure:expr) => {{
        let min_time = std::time::Duration::from_secs_f64($min);
        let max_time = std::time::Duration::from_secs_f64($max);
        let result = $crate::workloads::benchmark::converge(
            Some(min_time),
            Some(max_time),
            Some($threshold),
            $measure,
        );
        match result {
            Ok(metric) => {
                assert!(
                    metric >= $threshold,
                    "metric {} did not reach threshold {}",
                    metric,
                    $threshold
                );
                metric
            }
            Err(e) => panic!("converge error: {:?}", e),
        }
    }};
}

#[macro_export]
macro_rules! measure {
    ($ctx:expr, $args:expr, $name:expr, ($($var:ident),*), $func:expr) => {{
        $(let $var = $var.clone();)*
        $crate::workloads::benchmark::measure($args, $name, move |iters: u32| {
            $ctx.start(iters);
            $ctx.wait()?;
            $func(iters)
        })
    }};
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{thread, time::Duration};

    #[test]
    fn test_converge_reaches_threshold() {
        let measure = |iters: u32| {
            thread::sleep(Duration::from_millis(iters as u64));
            Ok(1.0)
        };
        let result = converge(
            Some(Duration::from_millis(1)),
            Some(Duration::from_millis(100)),
            Some(0.9),
            measure,
        );
        assert!(result.is_ok());
        assert!(result.unwrap() >= 0.9);
    }

    #[test]
    fn test_converge_never_reaches_threshold() {
        let measure = |iters: u32| {
            thread::sleep(Duration::from_millis(iters as u64));
            Ok(0.0)
        };
        let result = converge(
            Some(Duration::from_millis(1)),
            Some(Duration::from_millis(10)),
            Some(0.9),
            measure,
        );
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 0.0);
    }

    #[test]
    fn test_converge_macro_success() {
        let metric = converge!((0.001, 0.1), 0.5, |iters| {
            thread::sleep(Duration::from_millis(iters as u64));
            Ok(0.6)
        });
        assert!(metric >= 0.5);
    }

    #[test]
    fn test_benchmark_runs() {
        let dist = Distribution::<std::time::Duration>::default();
        let args = BenchArgs {
            name: "dummy_bench",
            sample_size: 10,
            significance_level: 0.05,
            percentile: 0.5,
        };
        let result = measure(&args, "fn", move |_iters| {
            Ok(BenchResult::Latency(dist.clone()))
        });
        assert!(result.is_ok());
    }
}
