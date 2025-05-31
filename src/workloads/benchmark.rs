//! Benchmarking utilities for the scheduler testing framework
//!
//! This module provides utilities for benchmarking workloads and measuring
//! performance metrics.

use std::time::{Duration, Instant};
use std::thread;

use crate::util::stats::Distribution;
use crate::workloads::context::Context;

/// Run a benchmark with the given context and measurement function
///
/// # Arguments
///
/// * `ctx` - The context in which to run the benchmark
/// * `measure` - A function that performs measurements and returns statistics
/// * `min_time` - The minimum time to run the benchmark for (default: 1 second)
/// * `min_iterations` - The minimum number of iterations to run (default: 3)
///
/// # Returns
///
/// The statistics collected from the benchmark
pub fn benchmark<F, T>(
    ctx: &Context,
    measure: F,
    min_time: Option<Duration>,
    min_iterations: Option<usize>,
) -> T
where
    F: Fn() -> T,
{
    // Set default values
    let min_time = min_time.unwrap_or(Duration::from_secs(1));
    let min_iterations = min_iterations.unwrap_or(3);

    // Run the benchmark
    let start = Instant::now();
    let mut iterations = 0;
    let mut last_result = None;

    while iterations < min_iterations || start.elapsed() < min_time {
        // Run the workload for a short time
        ctx.stop();
        thread::sleep(Duration::from_millis(10));

        // Start the context again
        *ctx = Context::new();

        // Measure the performance
        let result = measure();
        last_result = Some(result);

        // Increment the iteration count
        iterations += 1;
    }

    // Return the last result
    last_result.unwrap()
}

/// Run a benchmark with the given context and measurement function
///
/// This is a convenience function that calls `benchmark` with default parameters.
///
/// # Arguments
///
/// * `ctx` - The context in which to run the benchmark
/// * `measure` - A function that performs measurements and returns statistics
///
/// # Returns
///
/// The statistics collected from the benchmark
pub fn benchmark_default<F, T>(ctx: &Context, measure: F) -> T
where
    F: Fn() -> T,
{
    benchmark(ctx, measure, None, None)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::sync::Arc;

    #[test]
    fn test_benchmark() {
        let ctx = Context::new();
        let counter = Arc::new(AtomicUsize::new(0));

        // Run a benchmark that increments a counter
        let counter_clone = counter.clone();
        let result = benchmark(
            &ctx,
            move || {
                counter_clone.fetch_add(1, Ordering::Relaxed);
                counter_clone.load(Ordering::Relaxed)
            },
            Some(Duration::from_millis(100)),
            Some(2),
        );

        // Check that the counter was incremented at least twice
        assert!(result >= 2);
        assert!(counter.load(Ordering::Relaxed) >= 2);
    }

    #[test]
    fn test_benchmark_default() {
        let ctx = Context::new();
        let counter = Arc::new(AtomicUsize::new(0));

        // Run a benchmark that increments a counter
        let counter_clone = counter.clone();
        let result = benchmark_default(&ctx, move || {
            counter_clone.fetch_add(1, Ordering::Relaxed);
            counter_clone.load(Ordering::Relaxed)
        });

        // Check that the counter was incremented at least once
        assert!(result >= 1);
        assert!(counter.load(Ordering::Relaxed) >= 1);
    }
}
