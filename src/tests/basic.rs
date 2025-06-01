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

use crate::workloads::{context::Context, semaphore::Semaphore};

use inventory;

use super::Test;
use crate::process;
use anyhow::Result;

fn self_test(_ctx: &mut Context) -> Result<()> {
    println!("hello world!");
    Ok(())
}

inventory::submit!(Test {
    name: "self_test",
    test_fn: self_test,
});

fn ping_pong(ctx: &mut Context) -> Result<()> {
    let sem1 = ctx.allocate(Semaphore::<256, 1024>::new(1))?;
    let sem2 = ctx.allocate(Semaphore::<256, 1024>::new(1))?;

    let mut proc1 = process!(&ctx, None, (sem1, sem2), move |mut get_iters| {
        loop {
            let iters = get_iters();
            for _ in 0..iters {
                sem1.produce(1, 1, None);
                sem2.consume(1, 1, None);
            }
        }
    })?;
    let mut proc2 = process!(&ctx, None, (sem1, sem2), move |mut get_iters| {
        loop {
            let iters = get_iters();
            for _ in 0..iters {
                sem2.produce(1, 1, None);
                sem1.consume(1, 1, None);
            }
        }
    })?;
    proc1.start(100000);
    proc2.start(100000);
    proc1.wait()?;
    proc2.wait()?;
    Ok(())
}

inventory::submit!(Test {
    name: "ping_pong",
    test_fn: ping_pong,
});
