//! Helpers for creating and managing cgroups.

use std::fs::{self, File};
use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::process;

use anyhow::{anyhow, Context, Result};
use rand::Rng;

/// CgroupInfo contains information about a cgroup.
///
/// This does not own the underlying path or cgroup.
#[derive(Clone)]
pub struct CgroupInfo {
    path: PathBuf,
}

impl CgroupInfo {
    /// Creates a new CgroupInfo with the given path.
    pub fn new(path: PathBuf) -> Self {
        CgroupInfo { path }
    }

    /// Returns the path to the cgroup.
    pub fn path(&self) -> PathBuf {
        self.path.clone()
    }

    /// Enters the cgroup with the current thread.
    pub fn enter(&self) -> Result<()> {
        // Add the current process to the new cgroup.
        let tasks_path = self.path.join("tasks");
        let mut tasks_file = File::create(&tasks_path).context("failed to open tasks file")?;

        // Write the current process ID to the tasks file.
        write!(tasks_file, "{}", process::id()).context("failed to write to tasks file")?;

        Ok(())
    }
}

/// Cgroup is wraps the creation of a unique child cgroup.
///
/// This owns the underlying cgroup.
#[derive(Clone)]
pub struct Cgroup {
    info: CgroupInfo,
}

impl Cgroup {
    /// Creates a new cgroup as a sub-cgroup of the current process.
    pub fn create() -> Result<Self> {
        // Read /proc/self/cgroup to find the current cgroup.
        let cgroup_file =
            File::open("/proc/self/cgroup").context("failed to open /proc/self/cgroup")?;
        let reader = BufReader::new(cgroup_file);
        let mut current_cgroup = None;

        // Parse the file to get the cgroup path.
        // Format: hierarchy-ID:controller-list:cgroup-path
        for line in reader.lines() {
            let line = line?;
            if let Some(pos) = line.rfind(':') {
                current_cgroup = Some(line[(pos + 1)..].to_string());
                break;
            }
        }
        let current_cgroup =
            current_cgroup.ok_or_else(|| anyhow!("failed to determine current cgroup"))?;

        // Generate a new unique name, based on a random number.
        let mut rng = rand::thread_rng();
        let name = format!("schtest-{}", rng.gen::<u32>());

        // Create the new cgroup path.
        let cgroup_mount = PathBuf::from("/sys/fs/cgroup");
        let new_cgroup_path = cgroup_mount.join(&current_cgroup).join(&name);
        fs::create_dir_all(&new_cgroup_path).context("failed to create cgroup directory")?;

        Ok(Cgroup {
            info: CgroupInfo::new(new_cgroup_path),
        })
    }

    /// Returns a reference to the CgroupInfo.
    pub fn info(&self) -> &CgroupInfo {
        &self.info
    }
}

impl Drop for Cgroup {
    fn drop(&mut self) {
        let path = &self.info.path;
        if !path.as_os_str().is_empty() {
            // Move all processes from this cgroup back to the parent.
            let parent_path = path.parent().unwrap_or(Path::new("/"));

            if let Ok(tasks_file) = File::open(path.join("tasks")) {
                if let Ok(mut parent_tasks_file) = File::create(parent_path.join("tasks")) {
                    let reader = BufReader::new(tasks_file);
                    for line in reader.lines().map_while(Result::ok) {
                        let _ = writeln!(parent_tasks_file, "{line}");
                    }
                }
            }

            // Remove the cgroup directory.
            let _ = fs::remove_dir_all(path);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::super::user::User;
    use super::*;

    #[test]
    fn test_cgroup_create_and_drop() {
        // Skip if not root.
        if !User::is_root() {
            return;
        }
        let cgroup = Cgroup::create().unwrap();
        let path = cgroup.info().path().to_path_buf();
        assert!(path.exists());
        drop(cgroup);
        assert!(!path.exists());
    }
}
