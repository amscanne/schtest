//! Memory-backed file descriptor (memfd) implementation.

use std::ffi::CString;
use std::io;
use std::num::NonZeroUsize;
use std::os::fd::{AsRawFd, BorrowedFd, RawFd};

use libc::{c_char, c_int, c_uint};
use nix::sys::mman::{mmap, munmap, MapFlags, ProtFlags};
use nix::unistd::{close, ftruncate};

/// A memory-backed file descriptor (memfd).
///
/// This struct represents a memory-backed file descriptor that can be used
/// for various purposes, such as shared memory between processes.
pub struct MemFd {
    fd: RawFd,
    data: *mut u8,
    size: usize,
}

extern "C" {
    fn memfd_create(name: *const c_char, flags: c_uint) -> c_int;
}

impl MemFd {
    /// Create a new memfd with the given name and size.
    ///
    /// # Arguments
    ///
    /// * `name` - The name of the memfd
    /// * `size` - The size of the memfd in bytes
    ///
    /// # Returns
    ///
    /// A Result containing the new MemFd or an Error.
    pub fn create(name: &str, size: usize) -> io::Result<Self> {
        // Get the page size and round up the requested size to a multiple of the page size
        let page_size = nix::unistd::sysconf(nix::unistd::SysconfVar::PAGE_SIZE)
            .map_err(|e| io::Error::other(format!("System error: {e}")))?
            .unwrap() as usize;

        let rounded_up = (size + page_size - 1) & !(page_size - 1);

        // Create a C string for the name.
        let c_name = CString::new(name).map_err(|e| {
            io::Error::new(
                io::ErrorKind::InvalidInput,
                format!("Invalid memfd name: {e}"),
            )
        })?;

        // Create the memfd.
        let fd = unsafe { memfd_create(c_name.as_ptr(), 0) };
        if fd == -1 {
            return Err(io::Error::last_os_error());
        }

        // Set the size of the memfd.
        if let Err(e) = ftruncate(unsafe { BorrowedFd::borrow_raw(fd) }, rounded_up as i64) {
            let _ = close(fd);
            return Err(io::Error::other(format!("Failed to set size: {e}")));
        }

        // Map the memfd into memory
        let data = unsafe {
            mmap(
                None,
                NonZeroUsize::new(rounded_up).unwrap(),
                ProtFlags::PROT_READ | ProtFlags::PROT_WRITE,
                MapFlags::MAP_SHARED,
                Some(BorrowedFd::borrow_raw(fd)),
                0,
            )
            .map_err(|e| {
                let _ = close(fd);
                io::Error::other(format!("Memory mapping failed: {e}"))
            })?
        };

        Ok(MemFd {
            fd,
            data: data as *mut u8,
            size: rounded_up,
        })
    }

    /// Get the size of the memfd.
    pub fn size(&self) -> usize {
        self.size
    }

    /// Get a pointer to the mapped memory.
    pub fn data(&self) -> *mut u8 {
        self.data
    }
}

impl AsRawFd for MemFd {
    fn as_raw_fd(&self) -> RawFd {
        self.fd
    }
}

impl Drop for MemFd {
    fn drop(&mut self) {
        if !self.data.is_null() {
            unsafe {
                let _ = munmap(self.data as *mut _, self.size);
            }
        }

        if self.fd != -1 {
            let _ = close(self.fd);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_memfd_create() {
        let memfd = MemFd::create("test", 4096).unwrap();
        assert!(memfd.size() >= 4096);
        assert!(!memfd.data().is_null());

        unsafe {
            let slice = std::slice::from_raw_parts_mut(memfd.data(), memfd.size());
            slice[0] = 42;
            slice[1] = 43;
            assert_eq!(slice[0], 42);
            assert_eq!(slice[1], 43);
        }
    }
}
