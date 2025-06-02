# schtest

This is a scheduler testing and benchmarking framework.

## Project Structure

The project is organized into several modules:

- `cases`: Test cases and benchmarks
- `workloads`: Implementations of various workloads for testing scheduler behavior
- `util`: Utility functions and types for system operations, statistics, etc.

## Building the Project

### Prerequisites

- Rust 1.70 or later
- Cargo (Rust's package manager)
- Linux kernel with sched_ext support (for full functionality)

### Build Instructions

The project is a standard Cargo-based project, which will work with `cargo build` and `cargo test`. Some tests may require root privileges.

## Running tests

Run the `schtest` binary to run the tests. Use `--help` to see all available options.

### Running with a custom scheduler

To run with a custom scheduler:

```
sudo schtest [options] -- /path/to/scheduler [scheduler args...]
```

This will:
1. Run the specified scheduler binary
2. Wait for it to install a custom scheduler
3. Run the tests against that scheduler
4. Kill the scheduler when done
