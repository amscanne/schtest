//! Child process management, including function execution.

use crate::util::user::User;
use anyhow::{anyhow, Context, Result};
use libc;
use nix::sched::CloneFlags;
use nix::sys::signal::{self, Signal};
use nix::sys::wait::{waitpid, WaitPidFlag, WaitStatus};
use nix::unistd::Pid;
use std::ffi::CString;
use std::os::unix::io::RawFd;

/// A wrapper around a notification file descriptor.
///
/// This provides the ability to serialize and deserialize results between
/// parent and child processes.
struct NotificationFd {
    /// The read end of the notification pipe.
    read_fd: Option<RawFd>,

    /// The write end of the notification pipe.
    write_fd: Option<RawFd>,
}

impl NotificationFd {
    /// Create a new notification pipe.
    fn new() -> Result<Self> {
        let (read_fd, write_fd) = nix::unistd::pipe2(nix::fcntl::OFlag::O_CLOEXEC)
            .with_context(|| "Failed to create notification pipe")?;

        Ok(Self {
            read_fd: Some(read_fd),
            write_fd: Some(write_fd),
        })
    }

    /// Close the write end of the notification pipe.
    fn close_write(&mut self) {
        if let Some(fd) = self.write_fd.take() {
            let _ = nix::unistd::close(fd);
        }
    }

    /// Close the read end of the notification pipe.
    fn close_read(&mut self) {
        if let Some(fd) = self.read_fd.take() {
            let _ = nix::unistd::close(fd);
        }
    }
}

impl Drop for NotificationFd {
    fn drop(&mut self) {
        self.close_write();
        self.close_read();
    }
}

/// A wrapper around a child process.
///
/// This provides the ability to fork and execute a function in a child process. This
/// asserts that only a single thread is running at the time of the fork, to ensure that
/// it is a safe operation. It is the callers responsibility to ensure that other aspects
/// are safe (for example, the control flow is structured correctly).
pub struct Child {
    /// The process ID of the child.
    pid: Pid,

    /// The notification file descriptor for communicating results.
    notification: NotificationFd,

    /// The cached result from the child process.
    result: Option<Result<()>>,
}

impl Child {
    /// Create a new Child instance with the given PID.
    fn new(pid: Pid, notification: NotificationFd) -> Self {
        Self {
            pid,
            notification,
            result: None,
        }
    }

    /// Run a function in a child process.
    ///
    /// # Arguments
    ///
    /// * `f` - The function to run in the child process. This function can return a Result
    ///         where the Ok variant indicates success and the Err variant contains an Error.
    ///         If the function returns an Err, it will be serialized and passed back to the parent.
    /// * `extra_flags` - Extra flags to pass to the clone system call.
    ///
    /// # Returns
    ///
    /// A new `Child` instance if the process was spawned successfully, or an error otherwise
    ///
    /// # Safety
    ///
    /// This function is unsafe because forking a process in Rust can lead to undefined behavior
    /// if the child process uses any resources that were initialized in the parent process.
    /// The caller must ensure that the function `f` does not use any resources that could
    /// cause issues when duplicated in the child process.
    pub unsafe fn run<F>(f: F, extra_flags: Option<CloneFlags>) -> Result<Self>
    where
        F: FnOnce() -> Result<()> + Send + 'static,
    {
        // Create a notification pipe for result communication.
        let mut notification = NotificationFd::new()?;
        let write_fd = notification.write_fd.unwrap();

        // Call clone with the given flags.
        let flags = extra_flags.unwrap_or(CloneFlags::empty());
        let pid = libc::syscall(
            libc::SYS_clone as i64,
            libc::SIGCHLD | flags.bits(),
            0,
            0,
            0,
            0,
            0,
        ) as i32;
        if pid == 0 {
            // This is the child process. We run the given function, and serialize
            // the result to the write end of the pipe.
            notification.close_read();

            let result = f();
            match result {
                Ok(()) => {
                    let _ = nix::unistd::close(write_fd);
                }
                Err(error) => {
                    // Error case: serialize the error message as a string.
                    let error_msg = error.to_string();
                    let _ = nix::unistd::write(write_fd, error_msg.as_bytes());
                }
            }
            std::process::exit(0);
        }
        if pid < 0 {
            return Err(anyhow!(
                "Failed to clone process: {}",
                std::io::Error::last_os_error()
            ));
        }

        // This is the parent process. We close the write end of the pipe.
        notification.close_write();

        // Create a new Child instance.
        let child = Self::new(Pid::from_raw(pid), notification);

        Ok(child)
    }

    /// Return the process ID of the child.
    pub fn pid(&self) -> Pid {
        self.pid
    }

    /// Check if the child process is still alive.
    ///
    /// # Returns
    ///
    /// `true` if the child process is still alive, `false` otherwise.
    pub fn alive(&mut self) -> bool {
        // Check if we already have a cached result.
        if self.result.is_some() {
            return false;
        }

        // Attempt to reap the child process.
        self.wait(false, false).is_none()
    }

    /// Wait for the child process to exit and return the result.
    ///
    /// # Arguments
    ///
    /// * `block` - Whether to block until the child exits. If false and the child
    ///   hasn't exited yet, returns None.
    /// * `all` - Whether to wait for all child processes. If true, waits for any child process.
    ///
    /// # Returns
    ///
    /// The result of the child process execution, or an error if waiting failed.
    pub fn wait(&mut self, block: bool, all: bool) -> Option<Result<()>> {
        // Check if we already have a cached result.
        if let Some(result) = &self.result {
            // We can't clone the Result directly because anyhow::Error doesn't implement Clone
            // So we return a new Result with the same state
            return match result {
                Ok(_) => Some(Ok(())),
                Err(e) => Some(Err(anyhow!("{}", e))),
            };
        }

        loop {
            // Wait for the child process.
            let wait_pid = if all { Pid::from_raw(-1) } else { self.pid };
            let wait_flags = if block {
                None
            } else {
                Some(WaitPidFlag::WNOHANG)
            };
            let wait_status = match waitpid(wait_pid, wait_flags) {
                Ok(WaitStatus::Exited(pid, code)) => Some((pid, Some(code), None)),
                Ok(WaitStatus::Signaled(pid, signal, _)) => Some((pid, None, Some(signal))),
                Ok(WaitStatus::StillAlive) => None,
                _ => None,
            };

            // Check if something got found.
            if wait_status.is_none() {
                if !block {
                    return None; // Not exited yet and not blocking.
                }
                continue; // Keep waiting.
            }

            let (pid, exit_code, signal) = wait_status.unwrap();

            // Is this our pid?
            if pid != self.pid {
                continue;
            }

            // Read from the notification pipe.
            let mut buffer = Vec::new();
            let mut temp_buf = [0u8; 1024];

            loop {
                match nix::unistd::read(self.notification.read_fd.unwrap(), &mut temp_buf) {
                    Ok(0) => break, // End of file.
                    Ok(n) => buffer.extend_from_slice(&temp_buf[..n]),
                    Err(nix::errno::Errno::EINTR) => continue, // Interrupted, try again.
                    Err(_) => break,
                }
            }

            // Determine the result based on exit code and pipe contents.
            let result = if exit_code.is_some() && exit_code.unwrap() == 0 {
                if buffer.is_empty() {
                    Ok(())
                } else {
                    let buffer_str = String::from_utf8_lossy(&buffer);
                    Err(anyhow!("Child process error: {}", buffer_str))
                }
            } else if exit_code.is_some() {
                Err(anyhow!(
                    "Child process exited with code {}",
                    exit_code.unwrap()
                ))
            } else if signal.is_some() {
                Err(anyhow!(
                    "Child process was killed by signal {}",
                    signal.unwrap()
                ))
            } else {
                Err(anyhow!("Child process exited with unknown status"))
            };
            self.result = Some(result);
            return self.wait(false, false);
        }
    }

    /// Kill the child process with the given signal.
    ///
    /// # Arguments
    ///
    /// * `signal` - The signal to send to the child process (default: SIGKILL)
    ///
    /// # Returns
    ///
    /// `Ok(())` if the signal was sent successfully, or an error otherwise.
    pub fn kill(&mut self, signal: Signal) -> Result<()> {
        if !self.alive() {
            return Ok(());
        }

        // Send the signal directly.
        signal::kill(self.pid, signal).with_context(|| "Failed to kill child process")?;

        Ok(())
    }

    /// Spawn a new child process in a new PID namespace and execute a command.
    ///
    /// # Arguments
    ///
    /// * `args` - The command and arguments to spawn the child process with
    ///
    /// # Returns
    ///
    /// A new `Child` instance if the process was spawned successfully, or an error otherwise.
    pub fn spawn(args: &[String]) -> Result<Self> {
        if args.is_empty() {
            return Err(anyhow!("No command specified"));
        }

        // Convert args to CStrings.
        let c_args: Vec<CString> = args
            .iter()
            .map(|arg| CString::new(arg.as_str()).unwrap())
            .collect();

        // Safely run a function in a child process. If we are root, then we isolate
        // in a PID namespace to prevent any kind of sneaky daemonization.
        let flags = if User::is_root() {
            CloneFlags::CLONE_NEWPID
        } else {
            CloneFlags::empty()
        };
        unsafe {
            Self::run(
                move || {
                    // Set up death signal to ensure child dies when parent dies.
                    if let Err(e) = nix::sys::prctl::set_pdeathsig(Signal::SIGKILL) {
                        return Err(anyhow!("Failed to set death signal: {}", e));
                    }
                    // Become the repear for any other subprocesses.
                    if let Err(e) = nix::sys::prctl::set_child_subreaper(true) {
                        return Err(anyhow!("Failed to set child subreaper: {}", e));
                    }
                    let mut child = Self::run(
                        move || {
                            let result = nix::unistd::execvp(&c_args[0], &c_args);
                            match result {
                                Ok(_) => Err(anyhow!("Failed to execute command")),
                                Err(error) => Err(anyhow!("Failed to exec: {}", error)),
                            }
                        },
                        None,
                    )?;
                    child.wait(true, true).unwrap()
                },
                Some(flags),
            )
        }
    }
}

impl Drop for Child {
    fn drop(&mut self) {
        let _ = self.kill(Signal::SIGKILL);
        let _ = self.wait(true, false);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_spawn_and_kill() {
        let args = vec!["sleep".to_string(), "10".to_string()];
        let mut child = Child::spawn(&args).unwrap();
        assert!(child.alive());
        child.kill(Signal::SIGKILL).unwrap();
        let result = child.wait(true, false);
        assert!(!child.alive());
        assert!(result.is_some());
        assert!(result.unwrap().is_err());
    }

    #[test]
    fn test_spawn_and_wait() {
        let args = vec!["sleep".to_string(), "0.1".to_string()];
        let mut child = Child::spawn(&args).unwrap();
        let result = child.wait(true, false).unwrap();
        assert!(result.is_ok());
    }

    #[test]
    fn test_run_success() {
        let mut child = unsafe { Child::run(move || Ok(()), None).unwrap() };
        let result = child.wait(true, false).unwrap();
        assert!(result.is_ok());
    }

    #[test]
    fn test_run_error() {
        let mut child = unsafe { Child::run(move || Err(anyhow!("Test error")), None) }.unwrap();

        let result = child.wait(true, false).unwrap();
        assert!(!result.is_ok());
        let err = result.unwrap_err();
        assert!(err.to_string().contains("Test error"));
    }
}
