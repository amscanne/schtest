use std::fs;
use std::io::Read;
use std::path::Path;

use anyhow::{anyhow, Context, Result};

/// Scheduler utilities for setting process scheduling parameters.
pub struct Sched;

impl Sched {
    /// Set the scheduler policy and priority for the current process.
    ///
    /// # Arguments
    ///
    /// * `policy` - The scheduler policy to set
    /// * `priority` - The priority to set
    ///
    /// # Returns
    ///
    /// A Result indicating success or failure.
    pub fn set_scheduler(policy: i32, priority: i32) -> Result<()> {
        let param = libc::sched_param {
            sched_priority: priority,
        };
        let scheduler = SchedExt::installed();
        if scheduler.is_ok_and(|s| s.is_some()) {
            let rc = unsafe { libc::sched_setscheduler(0, policy, &param) };
            if rc < 0 {
                let err = std::io::Error::last_os_error();
                return Err(anyhow!("failed to set scheduler policy: {}", err));
            }
        } else {
            let rc = unsafe { libc::sched_setparam(0, &param) };
            if rc < 0 {
                let err = std::io::Error::last_os_error();
                return Err(anyhow!("failed to set scheduler parameters: {}", err));
            }
        }
        Ok(())
    }
}

pub struct SchedExt;

impl SchedExt {
    /// Path to the sched_ext sysfs directory.
    const SCHED_EXT_PATH: &'static str = "/sys/kernel/sched_ext";

    /// Path to the sched_ext status file.
    const SCHED_EXT_STATUS_PATH: &'static str = "/sys/kernel/sched_ext/state";

    /// Path to the root ops files.
    const SCHED_EXT_ROOT_OPS_PATH: &'static str = "/sys/kernel/sched_ext/root/ops";

    /// Check if the sched_ext framework is available.
    ///
    /// # Returns
    ///
    /// `Ok(true)` if sched_ext is available, `Ok(false)` otherwise,
    /// or an error if the check failed.
    pub fn available() -> Result<bool> {
        Ok(Path::new(Self::SCHED_EXT_PATH).exists())
    }

    /// Check if a custom scheduler is installed.
    ///
    /// # Returns
    ///
    /// `Ok(Some(name))` if a custom scheduler is installed, where `name` is the
    /// name of the scheduler, `Ok(None)` if no custom scheduler is installed,
    /// or an error if the check failed.
    pub fn installed() -> Result<Option<String>> {
        // Check if sched_ext is available.
        if !Self::available()? {
            return Ok(None);
        }

        // Read the status file.
        let status_path = Path::new(Self::SCHED_EXT_STATUS_PATH);
        if !status_path.exists() {
            return Ok(None);
        }
        let mut status_file =
            fs::File::open(status_path).with_context(|| "Failed to open sched_ext status file")?;
        let mut status_content = String::new();
        status_file
            .read_to_string(&mut status_content)
            .with_context(|| "Failed to read sched_ext status file")?;
        let status = status_content.trim();

        // Check if a scheduler is installed.
        if status == "disabled" {
            return Ok(None);
        } else if status == "enabling" {
            return Ok(None);
        } else if status != "enabled" {
            return Err(anyhow!("Unexpected status: {}", status));
        }

        // Read the ops file.
        let ops_path = Path::new(Self::SCHED_EXT_ROOT_OPS_PATH);
        if !ops_path.exists() {
            return Ok(None);
        }
        let mut ops_file =
            fs::File::open(ops_path).with_context(|| "Failed to open sched_ext ops file")?;
        let mut ops_content = String::new();
        ops_file
            .read_to_string(&mut ops_content)
            .with_context(|| "Failed to read sched_ext ops file")?;
        let ops = ops_content.trim();
        Ok(Some(ops.to_string()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_available() {
        let _ = SchedExt::available();
    }

    #[test]
    fn test_installed() {
        let _ = SchedExt::installed();
    }
}
