//! Clock and timer utilities.

use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{Duration, Instant};

/// A thread-safe timer for measuring elapsed time.
pub struct Timer {
    /// The start time of the timer, stored as nanoseconds.
    start: AtomicU64,

    /// The reference time.
    reference: Instant,
}

impl Timer {
    /// Create a new timer.
    ///
    /// # Returns
    ///
    /// A new `Timer` instance initialized with the current time.
    pub fn new() -> Self {
        let reference = Instant::now();
        Self {
            start: AtomicU64::new(Instant::now().duration_since(reference).as_nanos() as u64),
            reference: reference,
        }
    }

    /// Get the elapsed time since the timer was reset.
    ///
    /// # Returns
    ///
    /// The elapsed time as a `Duration`.
    pub fn elapsed(&self) -> Duration {
        let now = Instant::now().duration_since(self.reference).as_nanos() as u64;
        let start = self.start.load(Ordering::SeqCst);
        if now <= start {
            return Duration::from_nanos(0);
        }
        Duration::from_nanos(now - start)
    }

    /// Reset the timer to the current time.
    ///
    /// This method can be called from multiple threads safely.
    pub fn reset(&self) {
        self.start.store(Instant::now().duration_since(self.reference).as_nanos() as u64, Ordering::SeqCst);
    }
}

/// A robust timer that can handle multiple resets between elapsed time checks.
///
/// This timer maintains an array of `Timer` instances and cycles through them,
/// allowing it to track multiple timing operations concurrently.
///
/// The type parameter `S` determines the number of internal timers.
pub struct SplitTimer<const S: usize> {
    /// Array of internal timers.
    timers: [Timer; S],
    /// Current index in the timer array.
    index: AtomicU64,
}

impl<const S: usize> SplitTimer<S> {
    /// Create a new split timer.
    ///
    /// # Returns
    ///
    /// A new `SplitTimer` instance.
    pub fn new() -> Self {
        if S == 0 {
            return Self {
                timers: unsafe { std::mem::zeroed() },
                index: AtomicU64::new(0),
            };
        }

        // Create an array of S timers.
        let mut timers = Vec::with_capacity(S);
        for _ in 0..S {
            timers.push(Timer::new());
        }
        Self {
            timers: timers.try_into().unwrap_or_else(|_| panic!("Failed to create timer array")),
            index: AtomicU64::new(0),
        }
    }

    /// Reset the timer.
    pub fn reset(&self) {
        if S == 0 {
            return;
        }
        let orig_index = self.index.fetch_add(1, Ordering::SeqCst);
        self.timers[orig_index as usize % S].reset();
    }

    /// Get the current cookie value. This value can be used to check against the next
    /// call to `reset()` while ignoring any subsequent calls to `reset()`.
    ///
    /// # Returns
    ///
    /// The current cookie value.
    pub fn cookie(&self) -> u64 {
        self.index.load(Ordering::SeqCst)
    }

    /// Get the elapsed time since the timer was reset for the given cookie.
    ///
    /// # Arguments
    ///
    /// * `cookie` - The cookie value returned by `cookie()`.
    ///
    /// # Returns
    ///
    /// The elapsed time as a `Duration` if the cookie is still valid, or `None` if the cookie is too old.
    pub fn elapsed(&self, cookie: u64) -> Option<Duration> {
        // Return None if S = 0
        if S == 0 {
            return None;
        }

        let v = self.timers[cookie as usize % S].elapsed();
        let current_index = self.index.load(Ordering::SeqCst);
        // Check if the cookie is too old.
        if current_index >= cookie + S as u64 {
            return None;
        }
        Some(v)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    #[test]
    fn test_timer() {
        let timer = Timer::new();
        timer.reset();

        // Sleep for a short time.
        std::thread::sleep(Duration::from_millis(10));

        // Check that the elapsed time is at least 10ms.
        assert!(timer.elapsed() >= Duration::from_millis(10));

        // Reset the timer.
        timer.reset();

        // Check that the elapsed time is close to 0.
        assert!(timer.elapsed() < Duration::from_millis(10));
    }

    #[test]
    fn test_now() {
        let t1 = Instant::now();
        std::thread::sleep(Duration::from_millis(10));
        let t2 = Instant::now();

        // Check that time is monotonically increasing.
        assert!(t2 > t1);
    }

    #[test]
    fn test_robust_timer() {
        let timer = SplitTimer::<3>::new();

        // Test basic functionality.
        let cookie1 = timer.cookie();
        timer.reset();
        std::thread::sleep(Duration::from_millis(10));
        let elapsed = timer.elapsed(cookie1).unwrap();
        assert!(elapsed >= Duration::from_millis(10));

        // Test multiple resets.
        let cookie2 = timer.cookie();
        timer.reset();
        assert_ne!(cookie1, cookie2);
        std::thread::sleep(Duration::from_millis(15));

        // Both timers should still be valid.
        let elapsed1 = timer.elapsed(cookie1).unwrap();
        let elapsed2 = timer.elapsed(cookie2).unwrap();
        assert!(elapsed1 >= Duration::from_millis(25));
        assert!(elapsed2 >= Duration::from_millis(15));

        // Test cookie expiration.
        let timer = SplitTimer::<2>::new();
        let cookie1 = timer.cookie();
        timer.reset();
        timer.reset();
        timer.reset();
        assert!(timer.elapsed(cookie1).is_none());
    }
}
