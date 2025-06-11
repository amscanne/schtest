use nix::unistd::Pid;
use std::fs;
use std::io::Read;
use std::path::Path;
use std::time::Duration;
use procfs::{process::Process, ticks_per_second};

use anyhow::{anyhow, Context, Result};

/// Scheduler statistics for a thread.
#[derive(Debug, Default, Clone)]
pub struct SchedStats {
    pub system_time: Duration,
    pub user_time: Duration,
    pub total_time: Duration,

    pub nr_migrations: u64,
    pub nr_failed_migrations: u64,
    pub nr_forced_migrations: u64,
    pub nr_voluntary_switches: u64,
    pub nr_involuntary_switches: u64,
    pub nr_switches: u64,

    pub nr_preemptions: u64,
    pub nr_wakeups: u64,
    pub nr_wakeups_sync: u64,
    pub nr_wakeups_migrate: u64,
    pub nr_wakeups_local: u64,
    pub nr_wakeups_remote: u64,
    pub nr_yields: u64,

    pub wait_sum: f64,
    pub wait_max: f64,
    pub wait_count: u64,
}

/// Scheduler utilities for setting process scheduling parameters.
pub struct Sched;

impl Sched {
    /// Get scheduler statistics for a specific thread.
    ///
    /// # Arguments
    ///
    /// * `pid` - Process ID (None for current process)
    /// * `tid` - Thread ID (None for current thread)
    ///
    /// # Returns
    ///
    /// A Result containing the scheduler statistics.
    pub fn get_thread_stats(tid: Option<Pid>) -> Result<SchedStats> {
        let tid_val = if let Some(tid) = tid {
            tid.to_string()
        } else {
            "self".to_string()
        };

        // Path to the scheduler stats file.
        let path = format!("/proc/{}/sched", tid_val);

        // Read the scheduler stats for the given pid.
        let mut file = fs::File::open(&path)
            .with_context(|| format!("Failed to open scheduler stats file: {}", path))?;
        let mut content = String::new();
        file.read_to_string(&mut content)
            .with_context(|| format!("Failed to read scheduler stats file: {}", path))?;

        let mut stats = SchedStats::default();
        for line in content.lines() {
            if let Some(pos) = line.find(':') {
                let key = line[..pos].trim().to_string();
                let value = line[pos + 1..].trim().to_string();
                match key.as_str() {
                    "nr_migrations" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_migrations = val;
                        }
                    }
                    "nr_failed_migrations" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_failed_migrations = val;
                        }
                    }
                    "nr_forced_migrations" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_forced_migrations = val;
                        }
                    }
                    "nr_voluntary_switches" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_voluntary_switches = val;
                        }
                    }
                    "nr_involuntary_switches" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_involuntary_switches = val;
                        }
                    }
                    "nr_switches" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_switches = val;
                        }
                    }
                    "nr_preemptions" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_preemptions = val;
                        }
                    }
                    "nr_wakeups" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_wakeups = val;
                        }
                    }
                    "nr_wakeups_sync" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_wakeups_sync = val;
                        }
                    }
                    "nr_wakeups_migrate" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_wakeups_migrate = val;
                        }
                    }
                    "nr_wakeups_local" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_wakeups_local = val;
                        }
                    }
                    "nr_wakeups_remote" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_wakeups_remote = val;
                        }
                    }
                    "nr_yields" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.nr_yields = val;
                        }
                    }
                    "wait_sum" => {
                        if let Ok(val) = value.parse::<f64>() {
                            stats.wait_sum = val;
                        }
                    }
                    "wait_max" => {
                        if let Ok(val) = value.parse::<f64>() {
                            stats.wait_max = val;
                        }
                    }
                    "wait_count" => {
                        if let Ok(val) = value.parse::<u64>() {
                            stats.wait_count = val;
                        }
                    }
                    _ => {}
                }
            }
        }

        let tid_val = if let Some(tid) = tid {
            Process::new(tid.as_raw())
        } else {
            Process::myself()

        };
        let proc = tid_val.with_context(|| "Failed to get process information")?;
        let stat = proc.stat().with_context(|| "Failed to read process stat information")?;
        let ticks_per_sec = ticks_per_second();
        stats.system_time = Duration::from_secs_f64(stat.stime as f64 / ticks_per_sec as f64);
        stats.user_time = Duration::from_secs_f64(stat.utime as f64 / ticks_per_sec as f64);
        stats.total_time = stats.system_time + stats.user_time;

        Ok(stats)
    }

    /// Get scheduler statistics for the current thread.
    ///
    /// # Returns
    ///
    /// A Result containing the scheduler statistics.
    pub fn get_current_thread_stats() -> Result<SchedStats> {
        Self::get_thread_stats(None)
    }

    /// Get aggregated scheduler statistics for all threads in a process.
    ///
    /// # Arguments
    ///
    /// * `pid` - Process ID (None for current process)
    ///
    /// # Returns
    ///
    /// A Result containing the aggregated scheduler statistics.
    pub fn get_process_thread_stats(pid: Option<Pid>) -> Result<SchedStats> {
        let pid_val = if let Some(pid) = pid {
            pid.to_string()
        } else {
            "self".to_string()
        };
        let task_dir = format!("/proc/{}/task", pid_val);
        let entries = fs::read_dir(&task_dir)
            .with_context(|| format!("Failed to read task directory: {}", task_dir))?;

        // Aggregate from all threads.
        let mut aggregated_stats = SchedStats::default();
        for entry in entries {
            let entry = entry?;
            let file_name = entry.file_name();
            let tid_str = file_name.to_string_lossy();
            if let Ok(tid) = tid_str.parse::<i32>() {
                let stats = Self::get_thread_stats(Some(Pid::from_raw(tid)))?;
                aggregated_stats.system_time += stats.system_time;
                aggregated_stats.user_time += stats.user_time;
                aggregated_stats.total_time += stats.total_time;
                aggregated_stats.nr_migrations += stats.nr_migrations;
                aggregated_stats.nr_failed_migrations += stats.nr_failed_migrations;
                aggregated_stats.nr_forced_migrations += stats.nr_forced_migrations;
                aggregated_stats.nr_voluntary_switches += stats.nr_voluntary_switches;
                aggregated_stats.nr_involuntary_switches += stats.nr_involuntary_switches;
                aggregated_stats.nr_switches += stats.nr_switches;
                aggregated_stats.nr_preemptions += stats.nr_preemptions;
                aggregated_stats.nr_wakeups += stats.nr_wakeups;
                aggregated_stats.nr_wakeups_sync += stats.nr_wakeups_sync;
                aggregated_stats.nr_wakeups_migrate += stats.nr_wakeups_migrate;
                aggregated_stats.nr_wakeups_local += stats.nr_wakeups_local;
                aggregated_stats.nr_wakeups_remote += stats.nr_wakeups_remote;
                aggregated_stats.nr_yields += stats.nr_yields;
                aggregated_stats.wait_sum += stats.wait_sum;
                aggregated_stats.wait_count += stats.wait_count;
                if stats.wait_max > aggregated_stats.wait_max {
                    aggregated_stats.wait_max = stats.wait_max;
                }
            }
        }

        Ok(aggregated_stats)
    }

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
        if status == "disabled" || status == "enabling" {
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
    use std::thread;
    use std::time::Duration;
    use more_asserts::assert_gt;

    #[test]
    fn test_available() {
        let _ = SchedExt::available();
    }

    #[test]
    fn test_installed() {
        let _ = SchedExt::installed();
    }

    #[test]
    fn test_get_current_thread_stats() -> Result<()> {
        for _ in 0..5 {
            thread::sleep(Duration::from_millis(1));
        }

        let stats = Sched::get_current_thread_stats()?;
        let total_switches = stats.nr_voluntary_switches + stats.nr_involuntary_switches;
        assert!(total_switches > 0, "Expected some context switches");

        Ok(())
    }

    #[test]
    fn test_increasing_cpu_time() -> Result<()> {
        let initial_stats = Sched::get_current_thread_stats()?;
        // utime isn't super accurate, spin for 200ms to make sure we accumulate enough CPU time.
        let dur = Duration::from_millis(200);
        let start = std::time::Instant::now();
        let mut sum: u64 = 0;
        while start.elapsed() < dur {
            sum += 1;
        }

        assert!(sum > 0);
        let final_stats = Sched::get_current_thread_stats()?;
        assert_gt!(
            final_stats.user_time.as_nanos(),
            initial_stats.user_time.as_nanos(),
            "User time should increase after CPU-intensive work"
        );
        assert_gt!(
            final_stats.total_time.as_nanos(),
            initial_stats.total_time.as_nanos(),
            "Total CPU time should increase after CPU-intensive work"
        );
        Ok(())
    }
}
