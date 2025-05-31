//! Workload implementations for the scheduler testing framework
//!
//! This module provides various workload implementations for testing scheduler
//! functionality, including spinners, semaphores, and benchmarking utilities.

//pub mod benchmark;
pub mod context;
pub mod process;
pub mod semaphore;
pub mod spinner;

// Re-export commonly used items
//pub use self::benchmark::benchmark;
//pub use self::context::Context;
//pub use self::semaphore::Semaphore;
//pub use self::spinner::Spinner;
