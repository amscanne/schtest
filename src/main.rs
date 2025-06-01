use anyhow::{anyhow, Context, Result};
use clap::{ArgAction, Parser};
use nix::sys::signal::Signal;

use libtest_with;
use schtest::tests;
use schtest::util::child::Child;
use schtest::util::sched::SchedExt;
use schtest::util::user::User;
use schtest::workloads::context::Context as WorkloadContext;
use std::{thread, time::Duration};

/// Command line arguments for the schtest binary
#[derive(Parser, Debug)]
#[command(
    name = "schtest",
    about = "Scheduler testing framework",
    long_about = "This program drives a series of scheduler tests. It is used to drive simulated \
                 workloads, and assert functional properties of the scheduler, which map to test \
                 results that are emitted. There are also standardized benchmarks.\n\n\
                 If an additional binary is given, it will be run safely and the program will \
                 ensure that a custom scheduler is installed before any tests are run. This requires \
                 root privileges."
)]
struct Args {
    /// List all available tests and benchmarks.
    #[arg(long, action = ArgAction::SetTrue)]
    list: bool,

    /// Filter to apply to the list of tests and benchmarks.
    #[arg(long, default_value = None)]
    filter: Option<String>,

    /// Benchmarks should be run.
    #[arg(long, action = ArgAction::SetTrue)]
    benchmarks: bool,

    /// Minimum time to run benchmarks.
    #[arg(long, default_value_t = 1.0)]
    min_time: f64,

    /// Binary to run (with optional arguments).
    #[arg(trailing_var_arg = true)]
    binary: Vec<String>,
}

fn run(args: Vec<String>) -> Result<Child> {
    // Check if a scheduler is already installed.
    let scheduler = SchedExt::installed().with_context(|| "unable to query scheduler")?;
    if scheduler.is_some() {
        return Err(anyhow!(
            "scheduler already installed: {}",
            scheduler.unwrap()
        ));
    }

    // Run the given binary safely.
    eprintln!("spawning:");
    for arg in &args {
        eprintln!(" - {}", arg);
    }
    let mut child = Child::spawn(&args).with_context(|| "unable to spawn child process")?;

    // Wait for a custom scheduler to be installed.
    loop {
        if !child.alive() {
            let result = child.wait(true, false);
            return if let Some(Err(e)) = result {
                Err(e)
            } else {
                Err(anyhow!("child exited without installing scheduler"))
            };
        }

        // If it is installed, we're all set.
        let scheduler = SchedExt::installed().with_context(|| "unable to query scheduler")?;
        if let Some(scheduler_name) = scheduler {
            eprintln!("scheduler: {}", scheduler_name);
            return Ok(child);
        }

        // Wait for a while to see if it starts up.
        thread::sleep(Duration::from_millis(10));
    }
}

/// Run all registered tests.
fn run_tests(args: &Args) -> libtest_with::Conclusion {
    let libtest_args = libtest_with::Arguments {
        include_ignored: false,
        ignored: false,
        test: !args.benchmarks,
        bench: args.benchmarks,
        list: args.list,
        nocapture: false,
        show_output: true,
        unstable_flags: None,
        exact: false,
        quiet: false,
        test_threads: Some(1),
        logfile: None,
        skip: vec![],
        color: None,
        format: None,
        filter: args.filter.clone(),
    };
    let mut libtest_tests = Vec::new();
    for t in inventory::iter::<tests::Test> {
        libtest_tests.push(libtest_with::Trial::test(t.name, move || {
            let mut ctx = WorkloadContext::create()?;
            (t.test_fn)(&mut ctx)?;
            Ok(())
        }));
    } /*
      for t in inventory::iters::<tests::Benchmark> {
          libtest_tests.push(libtest_with::Trial::bench(t.name, move || {
                  (t.bench_fn)(args.min_time)?;
                  Ok(())
                }));
      }*/
    libtest_with::run(&libtest_args, libtest_tests)
}

fn main() -> Result<()> {
    // Parse command line arguments.
    let args = Args::parse();

    // We require root privileges to create cgroups, install the custom scheduler.
    if !User::is_root() {
        return Err(anyhow!("must run as root"));
    }

    // Handle running an external binary if provided.
    let maybe_child = if args.binary.is_empty() {
        None
    } else {
        Some(run(args.binary.clone())?)
    };

    // Set up test framework and run tests.
    let test_result = run_tests(&args);

    // Kill the scheduler, if there was one running.
    if let Some(mut child) = maybe_child {
        child.kill(Signal::SIGKILL)?;
        if let Some(result) = child.wait(true, true) {
            result?;
        }
    }

    // Return the test result.
    test_result.exit()
}
