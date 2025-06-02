//! Tests for system topology behavior.

use std::collections::HashMap;
use std::time::Duration;
use std::thread;

use anyhow::Result;

use crate::test;
use crate::util::system::{CPUSet, System};
use crate::workloads::context::Context;
use crate::workloads::spinner::Spinner;

use crate::process;
use crate::converge;

/// Test that verifies the scheduler spreads threads across physical cores.
///
/// This test creates a spinner for each physical core in the system, all initially
/// pinned to the same core. It then verifies that the scheduler eventually spreads
/// these threads across different physical cores, rather than just different logical
/// cores (hyperthreads).
fn spreading_out() -> Result<()> {
    let mut ctx = Context::create()?;
    let system = System::load()?;

    let mut logical_to_physical = HashMap::new();
    for core in system.cores() {
        for hyperthread in core.hyperthreads() {
            logical_to_physical.insert(hyperthread.id(), core.id());
        }
    }

    let cores = system.cores();
    let mut spinners = Vec::new();
    let first_core = &cores[0];

    // Create a spinner for each process. They will all start on core[0].
    for (_, _) in cores.iter().enumerate() {
        let spinner = ctx.allocate(Spinner::new())?;
        spinners.push(spinner.clone());
        process!(&mut ctx, None, (first_core), move |mut get_iters| {
            first_core.migrate()?; // Migrate to the core.
            loop {
                spinner.spin(Duration::from_millis(get_iters() as u64));
            }
        });
    }

    // Define our metric: the percentage of physical cores that are covered by
    // our spinners after each execution (each one spun for N milliseconds).
    let metric = move |iters| {
        ctx.start(iters);
        let mut counts = HashMap::new();
        // The iters will be milliseconds, so every 10ms wake up and check the
        // last core that each sleep used. This will be cumulative.
        for _ in 0..iters / 10 {
            thread::sleep(Duration::from_millis(10));
            for spinner in &spinners {
                let cpu_id = spinner.last_cpu();
                if let Some(&physical_id) = logical_to_physical.get(&(cpu_id as i32)) {
                    *counts.entry(physical_id).or_insert(0) += 1;
                }
            }
        }
        ctx.wait()?;
        // Calculate the ratio of physical cores covered to total physical cores.
        Ok(counts.len() as f64 / cores.len() as f64)
    };

    let target = 0.95; // 95% of cores used.
    let final_value = converge!((1.0, 30.0), target, metric);
    if final_value >= target {
        Ok(())
    } else {
        Err(anyhow::anyhow!(
            "Failed to achieve target: got {:.2}, expected {:.2}",
            final_value,
            target
        ))
    }
}

test!("spreading_out", spreading_out, None);
