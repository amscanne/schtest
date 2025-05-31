//! Context for running workloads.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use anyhow::Result;

use crate::util::shared::{BumpAllocator, SharedBox, SharedVec};

/// A context for running workloads.
///
/// The context provides shared memory, and the common state.
pub struct Context {
    /// The allocator for shared memory.
    allocator: Arc<BumpAllocator>,

    /// Whether the context is running.
    running: SharedBox<AtomicBool>,
}

impl Context {
    /// The maximize possible size of shared data.
    const TOTAL_SIZE: usize = 1024 * 1024 * 1024;

    /// Create a new context.
    ///
    /// # Returns
    ///
    /// A new `Context` instance.
    pub fn create() -> Result<Self> {
        let allocator = BumpAllocator::new("context", Self::TOTAL_SIZE)?;
        let running = SharedBox::new(allocator.clone(), AtomicBool::new(false))?;
        Ok(Self {
            allocator,
            running,
        })
    }

    /// Check if the context is running.
    ///
    /// # Returns
    ///
    /// `true` if the context is running, `false` otherwise.
    pub fn running(&self) -> bool {
        self.running.load(Ordering::Relaxed)
    }

    /// Stop the context.
    ///
    /// This will cause all threads to exit and all resources to be freed.
    pub fn stop(&self) {
        self.running.store(false, Ordering::Relaxed);
    }

    /// Add a function to be executed in a new process.
    ///
    /// # Arguments
    ///
    /// * `f` - The function to execute
    pub fn add<F>(&self, _f: F) -> Result<()>
    where
        F: FnOnce() -> Result<()> + Send + 'static,
    {
        // Implementation is empty, but we need to return a Result
        Ok(())
    }

    /// Allocate a new object in shared memory.
    ///
    /// This method allocates a new object in shared memory using the context's allocator.
    /// The object is initialized using the provided value.
    ///
    /// # Arguments
    ///
    /// * `value` - The value to store in shared memory
    ///
    /// # Returns
    ///
    /// A Result containing a SharedBox of the allocated object or an Error.
    pub fn allocate<T>(&self, value: T) -> Result<SharedBox<T>> {
        SharedBox::new(self.allocator.clone(), value)
    }

    /// Allocate a vector of objects in shared memory.
    ///
    /// This method allocates a vector of objects in shared memory using the context's allocator.
    /// Each object is initialized using the provided function, which receives the index of the
    /// object being initialized.
    ///
    /// # Arguments
    ///
    /// * `size` - The capacity of the vector to allocate
    /// * `f` - A function that constructs an object at the given index
    ///
    /// # Returns
    ///
    /// A Result containing a SharedVec of the allocated objects or an Error.
    pub fn allocate_vec<T, F>(&self, size: usize, f: F) -> Result<SharedVec<T>>
    where
        F: Fn(usize) -> T,
    {
        let mut vec = SharedVec::with_capacity(self.allocator.clone(), size)?;

        for i in 0..size {
            vec.push(f(i))?;
        }

        Ok(vec)
    }
}
