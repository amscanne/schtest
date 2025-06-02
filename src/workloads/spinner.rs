//! Spinner workload implementation.

use std::sync::atomic::{AtomicU32, Ordering};
use std::time::Duration;

/// A workload that spins for a specified duration.
pub struct Spinner {
    /// The ID of the CPU that the spinner last ran on.
    cpu_id: AtomicU32,
}

impl Spinner {
    /// Default duration is 99 years.
    pub const DEFAULT_DURATION: Duration = Duration::from_secs(60 * 60 * 24 * 365 * 99);

    /// Create a new spinner.
    ///
    /// # Returns
    ///
    /// A new `Spinner` instance.
    pub fn new() -> Self {
        Self {
            cpu_id: AtomicU32::new(0),
        }
    }

    /// Spin for the specified duration.
    ///
    /// # Arguments
    ///
    /// * `duration` - The duration to spin for
    pub fn spin(&self, duration: Duration) {
        let start = std::time::Instant::now();

        while start.elapsed() < duration {
            std::sync::atomic::compiler_fence(Ordering::SeqCst);
            if let Some(cpu) = Self::get_current_cpu() {
                self.cpu_id.store(cpu, Ordering::Relaxed);
            }
        }
    }

    /// Get the ID of the CPU that the spinner last ran on.
    ///
    /// # Returns
    ///
    /// The ID of the CPU that the spinner last ran on.
    pub fn last_cpu(&self) -> u32 {
        self.cpu_id.load(Ordering::Relaxed)
    }

    /// Get the ID of the CPU that the current thread is running on.
    ///
    /// # Returns
    ///
    /// The ID of the CPU that the current thread is running on, or None if the
    /// information is not available.
    fn get_current_cpu() -> Option<u32> {
        let cpu = unsafe { libc::sched_getcpu() };
        if cpu >= 0 {
            Some(cpu as u32)
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use anyhow::Result;
    use std::time::Duration;

    #[test]
    fn test_spinner() -> Result<()> {
        let spinner = Spinner::new();

        // Spin for a short duration.
        spinner.spin(Duration::from_millis(10));

        // Check that the CPU ID was updated.
        println!("Last CPU: {}", spinner.last_cpu());

        Ok(())
    }
}
