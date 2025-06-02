//! Context for running workloads.

use anyhow::Result;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

use crate::util::shared::{BumpAllocator, SharedBox, SharedVec};
use crate::workloads::process::Process;

/// Whether the context is running.
#[derive(Clone)]
pub struct Running {
    data: SharedBox<AtomicBool>,
}

impl Running {
    pub fn running(&self) -> bool {
        return self.data.load(Ordering::Relaxed);
    }
    pub fn start(&self) {
        self.data.store(true, Ordering::Relaxed);
    }
    pub fn stop(&self) {
        self.data.store(false, Ordering::Relaxed);
    }
}

/// A context for running workloads.
///
/// The context provides shared memory, and the common state.
pub struct Context {
    /// The allocator for shared memory.
    allocator: Arc<BumpAllocator>,

    /// Whether the context is running.
    running: Running,

    // All registered processes.
    processes: Vec<Process>,
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
        let running = Running{
            data: SharedBox::new(allocator.clone(), AtomicBool::new(false))?,
        };
        Ok(Self { allocator, running, processes: vec![] })
    }

    /// Adds a process to the context.
    pub fn add(&mut self, process: Process) -> &Process {
        self.processes.push(process);
        self.processes.last().unwrap()
    }

    /// Starts all processes.
    pub fn start(&mut self, iters: u32) {
        self.running.start();
        for p in self.processes.iter_mut() {
            p.start(iters);
        }
    }

    /// Waits for all processes.
    ///
    /// Note that after at least one process is stopped, the running flag will be set to
    /// false. Therefore, ensure that processes are added in a well-defined order: the
    /// should always use iters directly, while others are permitted to spin until the
    /// running flag returns false.
    pub fn wait(&mut self) -> Result<()> {
        for p in self.processes.iter_mut() {
            p.wait()?;
            self.running.stop(); // See above.
        }
        self.running.stop();
        Ok(())
    }

    /// Runs the given number of iterations.
    pub fn run(&mut self, iters: u32) -> Result<()> {
        self.start(iters);
        self.wait()?;
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
