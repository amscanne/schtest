//! Semaphore implementation.

use std::sync::atomic::{AtomicU32, Ordering};
use std::time::Duration;
use std::os::raw::c_int;
use std::mem::MaybeUninit;
use libc;

use crate::util::clock::SplitTimer;
use crate::util::stats::{ReservoirSampler, Distribution};

/// A lock-free semaphore for synchronizing threads.
pub struct Semaphore<const S: usize = 256, const R: usize = 1024> {
    /// The atomic counter for the semaphore.
    count: AtomicU32,

    /// The maximum count of the semaphore.
    max: u32,

    /// Timer for tracking wake-up latency.
    wake: SplitTimer<S>,

    /// Reservoir for collecting timing statistics.
    reservoir: ReservoirSampler<Duration, R>,
}

impl<const S: usize, const R: usize> Semaphore<S, R> {
    const WAITER: u32 = 0x80000000;

    /// Create a new semaphore.
    ///
    /// # Arguments
    ///
    /// * `max` - The maximum count of the semaphore
    ///
    /// # Returns
    ///
    /// A new `Semaphore` instance.
    pub fn new(max: u32) -> Self {
        Self {
            count: AtomicU32::new(0),
            max: max & !Self::WAITER,
            wake: SplitTimer::new(),
            reservoir: ReservoirSampler::new(),
        }
    }

    /// Collect wake times from the semaphore's reservoir and add them to a distribution.
    ///
    /// # Arguments
    ///
    /// * `dist` - The distribution to add the data to
    pub fn collect_wake_stats(&mut self, dist: &mut Distribution<Duration>) {
        dist.add_all(&self.reservoir);
        self.reservoir.reset();
    }

    /// Subtract value from the semaphore.
    ///
    /// This method will block until sufficient count is available or timeout occurs.
    ///
    /// # Arguments
    ///
    /// * `v` - The amount to consume
    /// * `wake` - The number of waiters to wake up (default: 1)
    /// * `timeout` - Optional timeout duration
    ///
    /// # Returns
    ///
    /// `true` if the operation completed successfully, `false` if it timed out.
    pub fn consume(&self, v: u32, wake: u32, timeout: Option<Duration>) -> bool {
        let mut cur = self.count.load(Ordering::Acquire);

        loop {
            let amount = cur & !Self::WAITER;
            let has_waiter = (cur & Self::WAITER) == Self::WAITER;

            if amount >= v {
                if self.count.compare_exchange_weak(
                    cur,
                    amount - v,
                    Ordering::AcqRel,
                    Ordering::Acquire
                ).is_ok() {
                    if has_waiter {
                        self.wake.reset();
                        unsafe {
                            libc::syscall(
                                libc::SYS_futex,
                                &self.count as *const AtomicU32 as *mut u32,
                                libc::FUTEX_WAKE,
                                wake as c_int,
                                std::ptr::null::<libc::timespec>(),
                                0,
                                0
                            );
                        }
                    }
                    return true;
                }
            } else {
                if !has_waiter {
                    if self.count.compare_exchange_weak(
                        cur,
                        cur | Self::WAITER,
                        Ordering::AcqRel,
                        Ordering::Acquire
                    ).is_err() {
                        cur = self.count.load(Ordering::Acquire);
                        continue;
                    }
                    cur |= Self::WAITER;
                }
                let mut ts = MaybeUninit::<libc::timespec>::uninit();
                let ts_ptr = if let Some(duration) = timeout {
                    unsafe {
                        let ts_ref = ts.assume_init_mut();
                        ts_ref.tv_sec = duration.as_secs() as libc::time_t;
                        ts_ref.tv_nsec = duration.subsec_nanos() as libc::c_long;
                        ts.as_mut_ptr()
                    }
                } else {
                    std::ptr::null::<libc::timespec>()
                };
                let cookie = self.wake.cookie();
                let rc = unsafe {
                    libc::syscall(
                        libc::SYS_futex,
                        &self.count as *const AtomicU32 as *mut u32,
                        libc::FUTEX_WAIT,
                        cur as c_int,
                        ts_ptr,
                        0,
                        0
                    )
                };

                if rc == -1 && unsafe { *libc::__errno_location() } == libc::ETIMEDOUT {
                    return false;
                } else if rc == 0 {
                    if S != 0 {
                        if let Some(elapsed) = self.wake.elapsed(cookie) {
                            if R != 0 {
                                self.reservoir.sample(elapsed);
                            }
                        }
                    }
                }
            }

            cur = self.count.load(Ordering::Acquire);
        }
    }

    /// Add value to the semaphore.
    ///
    /// This method will block if adding would exceed the maximum count or timeout occurs.
    ///
    /// # Arguments
    ///
    /// * `v` - The number of tokens to produce
    /// * `wake` - The number of waiters to wake up (default: 1)
    /// * `timeout` - Optional timeout duration
    ///
    /// # Returns
    ///
    /// `true` if the operation completed successfully, `false` if it timed out.
    pub fn produce(&self, v: u32, wake: u32, timeout: Option<Duration>) -> bool {
        let mut cur = self.count.load(Ordering::Acquire);

        loop {
            let amount = cur & !Self::WAITER;

            if amount + v <= self.max {
                let has_waiter = (cur & Self::WAITER) == Self::WAITER;

                if self.count.compare_exchange_weak(
                    cur,
                    amount + v,
                    Ordering::AcqRel,
                    Ordering::Acquire
                ).is_ok() {
                    if has_waiter {
                        self.wake.reset();
                        unsafe {
                            libc::syscall(
                                libc::SYS_futex,
                                &self.count as *const AtomicU32 as *mut u32,
                                libc::FUTEX_WAKE,
                                wake as c_int,
                                std::ptr::null::<libc::timespec>(),
                                0,
                                0
                            );
                        }
                    }
                    return true;
                }
            } else {
                let has_waiter = (cur & Self::WAITER) == Self::WAITER;

                if !has_waiter {
                    if self.count.compare_exchange_weak(
                        cur,
                        cur | Self::WAITER,
                        Ordering::AcqRel,
                        Ordering::Acquire
                    ).is_err() {
                        cur = self.count.load(Ordering::Acquire);
                        continue;
                    }

                    cur |= Self::WAITER;
                }

                let cookie = self.wake.cookie();

                // Convert timeout to timespec if provided
                let mut ts = MaybeUninit::<libc::timespec>::uninit();
                let ts_ptr = if let Some(duration) = timeout {
                    unsafe {
                        let ts_ref = ts.assume_init_mut();
                        ts_ref.tv_sec = duration.as_secs() as libc::time_t;
                        ts_ref.tv_nsec = duration.subsec_nanos() as libc::c_long;
                        ts.as_mut_ptr()
                    }
                } else {
                    std::ptr::null::<libc::timespec>()
                };

                let rc = unsafe {
                    libc::syscall(
                        libc::SYS_futex,
                        &self.count as *const AtomicU32 as *mut u32,
                        libc::FUTEX_WAIT,
                        cur as c_int,
                        ts_ptr,
                        0,
                        0
                    )
                };

                if rc == -1 && unsafe { *libc::__errno_location() } == libc::ETIMEDOUT {
                    return false;
                } else if rc == 0 {
                    if S != 0 {
                        if let Some(elapsed) = self.wake.elapsed(cookie) {
                            if R != 0 {
                                self.reservoir.sample(elapsed);
                            }
                        }
                    }
                }
            }

            cur = self.count.load(Ordering::Acquire);
        }
    }

}

impl<const S: usize> Default for Semaphore<S> {
    fn default() -> Self {
        Self::new(0x7fffffff)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::thread;

    #[test]
    fn test_semaphore_produce_consume() {
        let sem = Semaphore::<256, 1024>::new(10);
        assert!(sem.produce(5, 1, None));
        assert!(sem.consume(3, 1, None));
        assert!(sem.consume(2, 1, None));
    }

    #[test]
    fn test_semaphore_blocking() {
        let sem = Semaphore::<256, 1024>::new(10);
        thread::scope(|s| {
            let handle = s.spawn(|| {
                sem.consume(5, 1, None)
            });
            assert!(sem.produce(5, 1, None));
            assert!(handle.join().unwrap());
        });
    }

    #[test]
    fn test_semaphore_timeout() {
        let sem = Semaphore::<256, 1024>::new(10);
        assert!(sem.produce(3, 1, None));
        let result = sem.consume(5, 1, Some(Duration::from_millis(10)));
        assert!(!result);
    }
}
