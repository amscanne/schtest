//! Tests for latency scenarios.

use std::time::Duration;

use anyhow::Result;

use crate::test;
use crate::util::system::CPUMask;
use crate::util::system::{CPUSet, System};
use crate::workloads::benchmark::converge;
use crate::workloads::context::Context;
use crate::workloads::spinner::Spinner;

use crate::process;

/// Test that ensures basic fairness for affinitized vs. non-affinitized tasks.
///
/// We create one task per logical CPU, and affinitize each one. Then we create one
/// floating task, and ensure that everyone gets approximate fairness in this case.
fn fairness() -> Result<()> {
    let mut ctx = Context::create()?;
    let mut affinitized_proc_handles = vec![];
    let mut float_proc_handles = vec![];

    // Affinitized tasks.
    for core in System::load()?.cores().iter() {
        for hyperthread in core.hyperthreads().iter() {
            let mask = CPUMask::new(hyperthread);
            affinitized_proc_handles.push(process!(
                &mut ctx,
                None,
                (mask),
                move |mut get_iters| {
                    mask.run(move || {
                        let spinner = Spinner::default();
                        loop {
                            spinner.spin(Duration::from_millis(get_iters() as u64));
                        }
                    })
                }
            ));
        }
    }

    // Floating tasks.
    for _ in 0..System::load()?.logical_cpus() {
        float_proc_handles.push(process!(&mut ctx, None, (), move |mut get_iters| {
            let spinner = Spinner::default();
            loop {
                spinner.spin(Duration::from_millis(get_iters() as u64));
            }
        }));
    }

    let metric = |iters| {
        ctx.start(iters);
        ctx.wait()?;
        let affinitized_times: Vec<f64> = affinitized_proc_handles
            .iter()
            .map(|handle| match handle.stats() {
                Ok(stats) => stats.total_time.as_secs_f64(),
                Err(_) => 0.0,
            })
            .collect();
        let float_times: Vec<f64> = float_proc_handles
            .iter()
            .map(|handle| match handle.stats() {
                Ok(stats) => stats.total_time.as_secs_f64(),
                Err(_) => 0.0,
            })
            .collect();
        let affinitized_mean =
            affinitized_times.iter().sum::<f64>() / affinitized_times.len() as f64;
        let float_mean = float_times.iter().sum::<f64>() / float_times.len() as f64;
        if float_mean > affinitized_mean {
            Ok(affinitized_mean / float_mean)
        } else {
            Ok(float_mean / affinitized_mean)
        }
    };

    // Perfectly fair is 1:1, so a threshold of 1.0, set the threshold to .95 to be as liberal as
    // possible while still catching schedulers that will generally prefer affinitized tasks.
    let target = 0.95;
    let final_value = converge(
        Some(Duration::from_secs_f64(5.0)),
        Some(Duration::from_secs_f64(10.0)),
        Some(target),
        metric,
    )?;
    if final_value < target {
        Err(anyhow::anyhow!(
            "Failed to achieve target: got {:.2}, expected {:.2}",
            final_value,
            target
        ))
    } else {
        Ok(())
    }
}

test!("fairness", fairness);
