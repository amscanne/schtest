//! Shared memory allocator.

use std::alloc::Layout;
use std::ptr;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

use crate::util::memfd::MemFd;
use anyhow::{anyhow, Result};

/// A bump allocator that uses a memfd as backing storage.
///
/// This allocator implements a simple bump allocation scheme where memory is
/// allocated sequentially from a contiguous block. It does not support deallocation
/// of individual allocations - all memory is freed when the allocator is dropped.
pub struct BumpAllocator {
    memfd: MemFd,
    next: AtomicUsize,
}

impl BumpAllocator {
    /// Create a new BumpAllocator with the given name and size.
    ///
    /// # Arguments
    ///
    /// * `name` - The name of the memfd
    /// * `size` - The size of the memfd in bytes
    ///
    /// # Returns
    ///
    /// A Result containing the new BumpAllocator or an Error.
    pub fn new(name: &str, size: usize) -> Result<Arc<Self>> {
        let memfd = MemFd::create(name, size)?;

        let allocator = Self {
            memfd,
            next: AtomicUsize::new(0),
        };

        Ok(Arc::new(allocator))
    }

    /// Allocate memory with the given layout.
    ///
    /// # Arguments
    ///
    /// * `layout` - The layout of the memory to allocate
    ///
    /// # Returns
    ///
    /// A pointer to the allocated memory, or null if allocation failed.
    ///
    /// # Safety
    ///
    /// This function is unsafe because it returns a raw pointer that the caller must
    /// properly manage.
    pub unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        // Calculate the next aligned address.
        let align = layout.align();
        let size = layout.size();

        // Get the current position and align it.
        let current = self.next.load(Ordering::Relaxed);
        let aligned = (current + align - 1) & !(align - 1);

        // Check if we have enough space.
        if aligned + size > self.memfd.size() {
            return ptr::null_mut();
        }

        // Try to update the next pointer.
        match self.next.compare_exchange(
            current,
            aligned + size,
            Ordering::SeqCst,
            Ordering::Relaxed,
        ) {
            Ok(_) => {
                // We successfully claimed this memory.
                self.memfd.data().add(aligned)
            }
            Err(_) => {
                // Someone else updated the pointer, try again.
                self.alloc(layout)
            }
        }
    }

    /// Get the amount of memory currently allocated.
    pub fn used(&self) -> usize {
        self.next.load(Ordering::Relaxed)
    }

    /// Get the amount of memory still available.
    pub fn available(&self) -> usize {
        self.memfd.size() - self.used()
    }
}

unsafe impl Send for BumpAllocator {}
unsafe impl Sync for BumpAllocator {}

/// A box-like container that allocates an object in a BumpAllocator.
///
/// This struct provides a safe wrapper around objects allocated in a BumpAllocator.
/// Note that memory is not freed when the box is dropped - it is only freed when
/// the allocator is reset or dropped.
///
/// # Safety
///
/// The lifecycle of the underlying object will be tied to the lifecycle of the
/// underlying shared memory. The allocator is kept alive as long as any SharedBox
/// referencing it exists.
pub struct SharedBox<T> {
    ptr: *mut T,
    _allocator: Arc<BumpAllocator>,
}

impl<T> Clone for SharedBox<T> {
    fn clone(&self) -> Self {
        Self {
            ptr: self.ptr,
            _allocator: self._allocator.clone(),
        }
    }
}

unsafe impl<T: Send> Send for SharedBox<T> {}
unsafe impl<T: Sync> Sync for SharedBox<T> {}

impl<T> Drop for SharedBox<T> {
    fn drop(&mut self) {
        assert!(!std::mem::needs_drop::<T>());
    }
}

impl<T> SharedBox<T> {
    /// Create a new SharedBox with the given value.
    ///
    /// # Arguments
    ///
    /// * `allocator` - Arc reference to the allocator to use
    /// * `value` - The value to store in the SharedBox
    ///
    /// # Returns
    ///
    /// A Result containing the new SharedBox or an Error.
    pub fn new(allocator: Arc<BumpAllocator>, value: T) -> Result<Self> {
        let layout = Layout::new::<T>();
        let ptr = unsafe { allocator.alloc(layout) };
        if ptr.is_null() {
            return Err(anyhow!("Failed to allocate memory"));
        }
        unsafe {
            ptr::write(ptr as *mut T, value);
        }
        Ok(Self {
            ptr: ptr as *mut T,
            _allocator: allocator,
        })
    }

    /// Get the raw pointer to the object.
    pub fn as_ptr(&self) -> *const T {
        self.ptr
    }

    /// Get the mutable raw pointer to the object.
    pub fn as_mut_ptr(&mut self) -> *mut T {
        self.ptr
    }
}

impl<T> std::ops::Deref for SharedBox<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        unsafe { &*self.ptr }
    }
}

impl<T> std::ops::DerefMut for SharedBox<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { &mut *self.ptr }
    }
}

/// A vector-like container that allocates elements in a BumpAllocator.
///
/// This struct provides a vector-like interface for storing elements in a BumpAllocator.
pub struct SharedVec<T> {
    ptr: *mut T,
    len: usize,
    capacity: usize,
    _allocator: Arc<BumpAllocator>,
}

unsafe impl<T: Send> Send for SharedVec<T> {}
unsafe impl<T: Sync> Sync for SharedVec<T> {}

impl<T> SharedVec<T> {
    /// Create a new SharedVec with the given capacity.
    ///
    /// # Arguments
    ///
    /// * `allocator` - Arc reference to the allocator to use
    /// * `capacity` - The initial capacity of the vector
    ///
    /// # Returns
    ///
    /// A Result containing the new SharedVec or an Error.
    pub fn with_capacity(allocator: Arc<BumpAllocator>, capacity: usize) -> Result<Self> {
        let layout = Layout::array::<T>(capacity)
            .map_err(|_| anyhow!("Failed to create layout for SharedVec"))?;
        let ptr = unsafe { allocator.alloc(layout) };
        if ptr.is_null() {
            return Err(anyhow!("Failed to allocate memory for SharedVec"));
        }

        Ok(Self {
            ptr: ptr as *mut T,
            len: 0,
            capacity,
            _allocator: allocator,
        })
    }

    /// Push a value onto the end of the vector.
    ///
    /// # Arguments
    ///
    /// * `value` - The value to push onto the vector
    ///
    /// # Returns
    ///
    /// A Result containing () or an Error.
    pub fn push(&mut self, value: T) -> Result<()> {
        if self.len == self.capacity {
            return Err(anyhow!("SharedVec capacity exceeded"));
        }
        unsafe {
            ptr::write(self.ptr.add(self.len), value);
        }
        self.len += 1;

        Ok(())
    }

    /// Get the length of the vector.
    pub fn len(&self) -> usize {
        self.len
    }

    /// Check if the vector is empty.
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Get the capacity of the vector.
    pub fn capacity(&self) -> usize {
        self.capacity
    }

    /// Get a reference to the element at the given index.
    ///
    /// # Arguments
    ///
    /// * `index` - The index of the element to get
    ///
    /// # Returns
    ///
    /// An Option containing a reference to the element, or None if the index is out of bounds.
    pub fn get(&self, index: usize) -> Option<&T> {
        if index >= self.len {
            None
        } else {
            unsafe { Some(&*self.ptr.add(index)) }
        }
    }

    /// Get a mutable reference to the element at the given index.
    ///
    /// # Arguments
    ///
    /// * `index` - The index of the element to get
    ///
    /// # Returns
    ///
    /// An Option containing a mutable reference to the element, or None if the index is out of bounds.
    pub fn get_mut(&mut self, index: usize) -> Option<&mut T> {
        if index >= self.len {
            None
        } else {
            unsafe { Some(&mut *self.ptr.add(index)) }
        }
    }
}

impl<T> Clone for SharedVec<T> {
    fn clone(&self) -> Self {
        Self {
            ptr: self.ptr,
            len: self.len,
            capacity: self.capacity,
            _allocator: self._allocator.clone(),
        }
    }
}

impl<T> Drop for SharedVec<T> {
    fn drop(&mut self) {
        for i in 0..self.len {
            unsafe {
                ptr::drop_in_place(self.ptr.add(i));
            }
        }
    }
}

impl<T> std::ops::Index<usize> for SharedVec<T> {
    type Output = T;

    fn index(&self, index: usize) -> &Self::Output {
        self.get(index).expect("Index out of bounds")
    }
}

impl<T> std::ops::IndexMut<usize> for SharedVec<T> {
    fn index_mut(&mut self, index: usize) -> &mut Self::Output {
        self.get_mut(index).expect("Index out of bounds")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bump_allocator() {
        let allocator = BumpAllocator::new("test", 4096).unwrap();
        let layout1 = Layout::from_size_align(128, 8).unwrap();
        let ptr1 = unsafe { allocator.alloc(layout1) };
        assert!(!ptr1.is_null());
        let layout2 = Layout::from_size_align(256, 16).unwrap();
        let ptr2 = unsafe { allocator.alloc(layout2) };
        assert!(!ptr2.is_null());
        assert_ne!(ptr1, ptr2);
        assert!(ptr1 < ptr2);
        unsafe {
            let slice1 = std::slice::from_raw_parts_mut(ptr1, 128);
            slice1[0] = 42;
            slice1[1] = 43;
            let slice2 = std::slice::from_raw_parts_mut(ptr2, 256);
            slice2[0] = 44;
            slice2[1] = 45;
            assert_eq!(slice1[0], 42);
            assert_eq!(slice1[1], 43);
            assert_eq!(slice2[0], 44);
            assert_eq!(slice2[1], 45);
        }
    }

    #[test]
    fn test_shared_box() {
        let allocator = BumpAllocator::new("test_box", 4096).unwrap();
        let mut box1 = SharedBox::new(allocator.clone(), 42).unwrap();
        assert_eq!(*box1, 42);
        *box1 = 100;
        assert_eq!(*box1, 100);
    }

    #[test]
    fn test_shared_vec() {
        let allocator = BumpAllocator::new("test_vec", 4096).unwrap();
        let mut vec = SharedVec::with_capacity(allocator.clone(), 10).unwrap();
        vec.push(1).unwrap();
        vec.push(2).unwrap();
        vec.push(3).unwrap();
        assert_eq!(vec.len(), 3);
        assert_eq!(vec.capacity(), 10);
        assert_eq!(vec[0], 1);
        assert_eq!(vec[1], 2);
        assert_eq!(vec[2], 3);
        for i in 3..10 {
            vec.push(i).unwrap();
        }
        assert!(vec.push(11).is_err());
    }

    #[test]
    fn test_shared_vec_push() {
        let allocator = BumpAllocator::new("test_push_vec", 4096).unwrap();
        let mut vec = SharedVec::with_capacity(allocator.clone(), 10).unwrap();
        vec.push(1).unwrap();
        vec.push(2).unwrap();
        vec.push(3).unwrap();
        assert_eq!(vec.len(), 3);
        assert_eq!(vec.capacity(), 10);
        assert_eq!(vec[0], 1);
        assert_eq!(vec[1], 2);
        assert_eq!(vec[2], 3);
    }

    #[test]
    fn test_shared_box_complex_type() {
        #[derive(Debug, PartialEq, Clone, Copy)]
        struct TestStruct {
            a: i32,
            b: i32,
            c: [i32; 3],
        }
        let test_struct = TestStruct {
            a: 42,
            b: 100,
            c: [1, 2, 3],
        };
        let allocator = BumpAllocator::new("test_complex", 4096).unwrap();
        let box1 = SharedBox::new(allocator.clone(), test_struct).unwrap();
        assert_eq!(box1.a, 42);
        assert_eq!(box1.b, 100);
        assert_eq!(box1.c, [1, 2, 3]);
    }

    #[test]
    fn test_shared_box_clone() {
        let allocator = BumpAllocator::new("test_clone", 4096).unwrap();
        let box1 = SharedBox::new(allocator.clone(), 42).unwrap();
        let box2 = box1.clone();
        assert_eq!(*box1, 42);
        assert_eq!(*box2, 42);
    }
}
