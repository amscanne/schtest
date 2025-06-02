use anyhow::{anyhow, Result};
use crate::util::system::System;
use crate::util::sched::SchedExt;

#[derive(Debug)]
pub struct Constraints {
    pub min_cpus: Option<usize>,
    pub max_cpus: Option<usize>,
    pub min_complexes: Option<usize>,
    pub max_complexes: Option<usize>,
    pub min_nodes: Option<usize>,
    pub max_nodes: Option<usize>,
    pub required_schedulers: Option<Vec<String>>,
    pub excluded_schedulers: Option<Vec<String>>,
}

impl Constraints {
    pub fn new() -> Self {
        Constraints {
            min_cpus: None,
            max_cpus: None,
            min_complexes: None,
            max_complexes: None,
            min_nodes: None,
            max_nodes: None,
            required_schedulers: None,
            excluded_schedulers: None,
        }
    }

    pub fn with_min_cpus(mut self, min_cpus: usize) -> Self {
        self.min_cpus = Some(min_cpus);
        self
    }

    pub fn with_max_cpus(mut self, max_cpus: usize) -> Self {
        self.max_cpus = Some(max_cpus);
        self
    }

    pub fn with_min_complexes(mut self, min_complexes: usize) -> Self {
        self.min_complexes = Some(min_complexes);
        self
    }

    pub fn with_max_complexes(mut self, max_complexes: usize) -> Self {
        self.max_complexes = Some(max_complexes);
        self
    }

    pub fn with_min_nodes(mut self, min_nodes: usize) -> Self {
        self.min_nodes = Some(min_nodes);
        self
    }

    pub fn with_max_nodes(mut self, max_nodes: usize) -> Self {
        self.max_nodes = Some(max_nodes);
        self
    }

    pub fn with_required_schedulers(mut self, schedulers: Vec<String>) -> Self {
        self.required_schedulers = Some(schedulers);
        self
    }

    pub fn with_excluded_schedulers(mut self, schedulers: Vec<String>) -> Self {
        self.excluded_schedulers = Some(schedulers);
        self
    }

    pub fn check(&self) -> Result<()> {
        let system = System::load()?;
        if let Some(min_cpus) = self.min_cpus {
            if system.logical_cpus() < min_cpus {
                return Err(anyhow!("insufficient logical cpus"));
            }
        }
        if let Some(max_cpus) = self.max_cpus {
            if system.logical_cpus() > max_cpus {
                return Err(anyhow!("too many logical cpus"));
            }
        }
        if let Some(min_complexes) = self.min_complexes {
            if system.complexes() < min_complexes {
                return Err(anyhow!("insufficient complexes"));
            }
        }
        if let Some(max_complexes) = self.max_complexes {
            if system.complexes() > max_complexes {
                return Err(anyhow!("too many complexes"));
            }
        }
        if let Some(min_nodes) = self.min_nodes {
            if system.nodes().len() < min_nodes {
                return Err(anyhow!("insufficient nodes"));
            }
        }
        if let Some(max_nodes) = self.max_nodes {
            if system.nodes().len() > max_nodes {
                return Err(anyhow!("too many nodes"));
            }
        }
        if self.required_schedulers.is_some() || self.excluded_schedulers.is_some() {
            let installed = SchedExt::installed()?.unwrap_or_else(|| "".to_string());
            if let Some(excluded_schedulers) = &self.excluded_schedulers {
                for scheduler in excluded_schedulers {
                    if installed == *scheduler {
                        return Err(anyhow!("scheduler is excluded"));
                    }
                }
            }
            if let Some(required_schedulers) = &self.required_schedulers {
                let mut found = false;
                for scheduler in required_schedulers {
                    if installed == *scheduler {
                        found = true;
                    }
                }
                if !found {
                    return Err(anyhow!("required scheduler not found"));
                }
            }
        }

        // All checks passed.
        Ok(())
    }
}
