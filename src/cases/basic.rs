//! Basic tests for the scheduler testing framework
//!
//! This module contains basic tests for the scheduler testing framework,
//! including ping-pong tests and worker tests.

/*
use std::thread;
use std::time::Duration;

use schtest::util::stats::Distribution;
use schtest::util::system::System;
use schtest::workloads::{Context, Semaphore, Spinner, benchmark};
*/

use std::time::Duration;

use crate::util::stats::Distribution;
use crate::workloads::benchmark::BenchArgs;
use crate::workloads::benchmark::BenchResult::{Count, Latency};
use crate::workloads::spinner::Spinner;
use crate::workloads::{context::Context, semaphore::Semaphore};

use crate::benchmark;
use crate::measure;
use crate::process;
use crate::test;

use anyhow::Result;

fn self_test() -> Result<()> {
    Ok(())
}

test!("self_test", self_test, None);

fn self_bench(args: &BenchArgs) -> Result<()> {
    let mut ctx = Context::create()?;
    measure!(&mut ctx, &args, "1ms", (), |iters| {
        let spinner = Spinner::new();
        spinner.spin(Duration::from_millis(iters as u64));
        Ok(Count(iters as u64))
    })
}

benchmark!("self_bench", self_bench, (), None);

fn ping_pong(args: &BenchArgs) -> Result<()> {
    let mut ctx = Context::create()?;
    let sem1 = ctx.allocate(Semaphore::<256, 1024>::new(1))?;
    let sem2 = ctx.allocate(Semaphore::<256, 1024>::new(1))?;

    process!(&mut ctx, None, (sem1, sem2), move |mut get_iters| {
        loop {
            let iters = get_iters();
            for _ in 0..iters {
                sem1.produce(1, 1, None);
                sem2.consume(1, 1, None);
            }
        }
    });
    process!(&mut ctx, None, (sem1, sem2), move |mut get_iters| {
        loop {
            let iters = get_iters();
            for _ in 0..iters {
                sem2.produce(1, 1, None);
                sem1.consume(1, 1, None);
            }
        }
    });
    measure!(&mut ctx, &args, "wake_latency", (sem1, sem2), |_| {
        let mut d = Distribution::<Duration>::default();
        sem1.collect_wake_stats(&mut d);
        sem2.collect_wake_stats(&mut d);
        Ok(Latency(d))
    })
}

benchmark!("ping_pong", ping_pong, (), None);
