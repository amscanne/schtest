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

use crate::workloads::context::Context;

use inventory;

use super::Test;

fn self_test(_ctx: &mut Context) -> Result<(), String> {
  println!("hello world!");
  Ok(())
}

inventory::submit!(Test{
    name: "self_test",
    test_fn: self_test,
});

/*
fn ping_pong() {
    let ctx = Context::create();

    // Create semaphores for synchronization
    let sem1: &Semaphore = ctx.allocate();
    let sem2: &Semaphore = ctx.allocate();

    // Add the first thread (producer)
    ctx.add(move || {
        while ctx.running() {
            sem1.produce(1, None);
            sem2.consume(1).unwrap();
        }
        sem1.produce(1, None);
        Ok(())
    }).unwrap();

    // Add the second thread (consumer)
    ctx.add(move || {
        let mut iters = 0;
        while ctx.running() {
            sem2.produce(1, None);
            sem1.consume(1).unwrap();
            iters += 1;
        }
        sem2.produce(1, None);
        Ok(())
    }).unwrap();

    // Run the benchmark
    let stats = benchmark(&ctx, || {
        sem1.reset();
        sem2.reset();
        let mut wake_latency = Distribution::new();
        sem1.flush(&mut wake_latency);
        sem2.flush(&mut wake_latency);
        wake_latency.estimates()
    }, Some(Duration::from_millis(100)), Some(1));

    println!("Ping-pong test results: {}", stats);
}

inventory::submit!(Test{
    name: "ping_pong",
    test_fn: ping_pong,
})

/// Test multiple worker threads
#[test]
fn test_workers() {
    let ctx = Context::create();

    // Get the number of logical CPUs
    let system = System::load().expect("Failed to load system information");
    let worker_count = system.logical_cpus();

    // Create semaphores for synchronization
    let outbound: &Semaphore = ctx.allocate();
    let inbound: &Semaphore = ctx.allocate();

    // Add the main thread
    ctx.add(move || {
        let mut workers = Vec::new();

        // Create worker threads
        for _ in 0..worker_count {
            let ctx_clone = ctx.clone();
            let outbound_ref = outbound;
            let inbound_ref = inbound;

            let handle = thread::spawn(move || {
                while ctx_clone.running() {
                    let work = Spinner::new(ctx_clone.clone(), Some(Duration::from_micros(10)));
                    outbound_ref.consume(1).unwrap();
                    work.spin().unwrap();
                    inbound_ref.produce(1, None);
                }
            });

            workers.push(handle);
        }

        // Wake up all threads
        outbound.produce(worker_count, Some(worker_count));

        // Process work
        while ctx.running() {
            inbound.consume(1).unwrap();
            outbound.produce(1, None);
        }

        // Wake up anything still blocked
        outbound.produce(worker_count, Some(worker_count));

        // Wait for all workers to finish
        for handle in workers {
            handle.join().unwrap();
        }

        Ok(())
    }).unwrap();

    // Run the benchmark
    let stats = benchmark(&ctx, || {
        outbound.reset();
        inbound.reset();
        let mut wake_latency = Distribution::new();
        outbound.flush(&mut wake_latency);
        inbound.flush(&mut wake_latency);
        wake_latency.estimates()
    }, Some(Duration::from_millis(100)), Some(1));

    println!("Workers test results: {}", stats);
}

/// Test for waking up multiple threads (herd test)
#[test]
fn test_wake_ups() {
    for count in [1, 2, 4, 8, 16] {
        println!("Testing with {} threads", count);

        let ctx = Context::create();
        let outbound: &Semaphore = ctx.allocate();
        let inbound: &Semaphore = ctx.allocate();

        // Add the main thread
        ctx.add(move || {
            // Create threads
            let mut threads = Vec::new();
            let running = std::sync::Arc::new(std::sync::atomic::AtomicBool::new(true));

            for _ in 0..count {
                let running_clone = running.clone();
                let outbound_ref = outbound;
                let inbound_ref = inbound;

                let handle = thread::spawn(move || {
                    while running_clone.load(std::sync::atomic::Ordering::Relaxed) {
                        outbound_ref.consume(1).unwrap();
                        inbound_ref.produce(1, None);
                    }
                });

                threads.push(handle);
            }

            // Broadcast and collect
            while ctx.running() {
                for _ in 0..count {
                    outbound.produce(count, None);
                    inbound.consume(count).unwrap();
                }
            }

            // Unblock all threads
            running.store(false, std::sync::atomic::Ordering::Relaxed);
            outbound.produce(2 * count, Some(count));

            // Wait for all threads to finish
            for handle in threads {
                handle.join().unwrap();
            }

            Ok(())
        }).unwrap();

        // Run the benchmark
        let stats = benchmark(&ctx, || {
            outbound.reset();
            inbound.reset();
            let mut wake_latency = Distribution::new();
            outbound.flush(&mut wake_latency);
            wake_latency.estimates()
        }, Some(Duration::from_millis(100)), Some(1));

        println!("Wake-ups test results (count={}): {}", count, stats);
    }
}
*/
