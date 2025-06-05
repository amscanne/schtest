use anyhow::{anyhow, Context, Result};
use clap::{ArgAction, Parser};
use once_cell::sync::Lazy;
use schtest::cases;
use schtest::util::child::Child;
use schtest::util::sched::SchedExt;
use schtest::util::user::User;
use std::collections::HashSet;
use std::sync::Mutex;
use std::{thread, time::Duration};

/// Command line arguments for the schtest binary.
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

    /// Binary to run (with optional arguments).
    #[arg(trailing_var_arg = true)]
    binary: Vec<String>,

    /// Sample size for benchmarks.
    #[arg(long, default_value_t = 10)]
    sample_size: usize,

    /// Significance level for benchmarks.
    #[arg(long, default_value_t = 0.05)]
    significance_level: f64,

    /// Percentile for benchmarks.
    #[arg(long, default_value_t = 0.50)]
    percentile: f64,
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

fn make_trial<F, C>(name: &'static str, test_fn: F, mut constraints: C) -> libtest_with::Trial
where
    F: FnOnce() -> Result<(), libtest_with::Failed> + Send + 'static,
    C: FnMut() -> Result<()> + 'static,
{
    let err = constraints();
    let ignore = err.is_err();
    let ignore_reason: Option<String> = if ignore {
        Some(err.unwrap_err().to_string())
    } else {
        None
    };
    libtest_with::Trial::test(name, test_fn).with_ignored_flag(ignore, ignore_reason)
}

static INTERNED_NAMES: Lazy<Mutex<HashSet<&'static str>>> =
    Lazy::new(|| Mutex::new(HashSet::new()));

fn intern_string(s: String) -> &'static str {
    // Leak the string to get a &'static str
    let static_str: &'static str = Box::leak(s.into_boxed_str());
    // Optionally track it to avoid leaking duplicates (not strictly necessary, but good hygiene)
    let mut set = INTERNED_NAMES.lock().unwrap();
    set.insert(static_str);
    static_str
}

/// Run all registered tests.
fn run_tests(args: &Args) -> libtest_with::Conclusion {
    let libtest_args = libtest_with::Arguments {
        include_ignored: false,
        ignored: false,
        test: true,
        bench: false,
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
    if args.benchmarks {
        use schtest::workloads::benchmark::BenchArgs;
        for t in inventory::iter::<cases::Benchmark> {
            #[allow(clippy::redundant_field_names)]
            let name = intern_string((t.name)());
            let bench_args = BenchArgs {
                name,
                sample_size: args.sample_size,
                significance_level: args.significance_level,
                percentile: args.percentile,
            };
            libtest_tests.push(make_trial(
                name,
                move || {
                    (t.test_fn)(&bench_args).map_err(|e| libtest_with::Failed::from(e.to_string()))
                },
                t.constraints,
            ));
        }
    } else {
        for t in inventory::iter::<cases::Test> {
            let name = intern_string((t.name)());
            libtest_tests.push(make_trial(
                name,
                || (t.test_fn)().map_err(|e| libtest_with::Failed::from(e.to_string())),
                t.constraints,
            ));
        }
    }
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
    drop(maybe_child);

    // Return the test result.
    test_result.exit()
}
