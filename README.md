# schtest - Scheduler Testing Framework

This is a Rust port of the SchTest scheduler testing framework, which provides tools for testing and benchmarking Linux scheduler functionality.

## Overview

SchTest is designed to test the functionality and performance of Linux schedulers, particularly those using the sched_ext framework. It provides:

- A framework for running various workloads to test scheduler behavior
- Tools for measuring wake-up latency and other performance metrics
- Utilities for spawning and managing child processes
- Statistical analysis of performance data

## Project Structure

The project is organized into several modules:

- `sched`: Interfaces for interacting with the Linux scheduler
- `util`: Utility functions and types for system operations, statistics, etc.
- `workloads`: Implementations of various workloads for testing scheduler behavior
- `tests`: Test cases for the scheduler

## Building the Project

### Prerequisites

- Rust 1.70 or later
- Cargo (Rust's package manager)
- Linux kernel with sched_ext support (for full functionality)

### Build Instructions

1. Clone the repository:
   ```
   git clone <repository-url>
   cd schtest
   ```

2. Build the project:
   ```
   cargo build
   ```

3. Run the tests:
   ```
   cargo test
   ```

## Running SchTest

The main binary can be run with:

```
sudo cargo run -- [options] [-- <scheduler-binary> [args...]]
```

### Command Line Options

- `--list`: List all available tests and benchmarks
- `--filter <pattern>`: Filter tests and benchmarks by name
- `--min_time <seconds>`: Minimum time to run benchmarks (default: 1.0)

### Running with a Custom Scheduler

To run with a custom scheduler:

```
sudo cargo run -- -- /path/to/scheduler [args...]
```

This will:
1. Run the specified scheduler binary
2. Wait for it to install a custom scheduler
3. Run the tests against that scheduler
4. Kill the scheduler when done

## Writing Tests

Tests can be written using Rust's standard testing framework. See the `src/tests/basic_test.rs` file for examples.

Basic test structure:

```rust
#[test]
fn test_my_scheduler_feature() {
    let ctx = Context::create();

    // Set up workloads and synchronization
    let sem1: &Semaphore = ctx.allocate();
    let sem2: &Semaphore = ctx.allocate();

    // Add threads with workloads
    ctx.add(move || {
        // Thread logic here
        Ok(())
    }).unwrap();

    // Run benchmark and collect statistics
    let stats = benchmark(&ctx, || {
        // Measurement logic here
        let mut distribution = Distribution::new();
        // ...
        distribution.estimates()
    });

    println!("Test results: {}", stats);
}
```

## License

This project is licensed under the same terms as the original C++ SchTest project.
