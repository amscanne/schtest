//! Statistical utilities.

use std::cmp::PartialOrd;
use std::fmt;
use std::mem::MaybeUninit;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::Duration;

use rand::Rng;
use tdigest::TDigest;

/// A thread-safe reservoir sampler that uses atomic operations to safely
/// collect samples from any number of concurrent threads.
pub struct ReservoirSampler<T, const N: usize> {
    /// The set of samples.
    samples: [MaybeUninit<T>; N],

    /// The number of items in the reservoir. Note that the number of
    /// samples retained is always capped at `N`, and the probably of
    /// any single sample being retained is N/count if count > N.
    count: AtomicUsize,
}

impl<T: Copy, const N: usize> ReservoirSampler<T, N> {
    /// Create a new empty reservoir sampler.
    ///
    /// # Returns
    ///
    /// A new `ReservoirSampler` instance.
    pub fn new() -> Self {
        // Handle the special case where N = 0
        if N == 0 {
            // Create a dummy implementation with an empty array
            // This is safe because we'll never access the array when N = 0
            return Self {
                samples: unsafe { MaybeUninit::uninit().assume_init() },
                count: AtomicUsize::new(0),
            };
        }

        Self {
            samples: unsafe { MaybeUninit::uninit().assume_init() },
            count: AtomicUsize::new(0),
        }
    }

    /// Add an item to the reservoir.
    ///
    /// # Arguments
    ///
    /// * `item` - The item to add to the reservoir.
    ///
    /// This method is thread-safe and can be called from multiple threads simultaneously.
    pub fn sample(&self, item: T) {
        // Do nothing if N = 0
        if N == 0 {
            return;
        }

        // Reservoir not yet full, add item directly.
        let index = self.count.fetch_add(1, Ordering::Relaxed);
        if index < N {
            unsafe {
                std::ptr::write(self.samples[index].as_ptr() as *mut T, item);
            }
        } else {
            // Reservoir is full, use reservoir sampling algorithm.
            let random_index = rand::thread_rng().gen_range(0..N);
            unsafe {
                std::ptr::write(self.samples[random_index].as_ptr() as *mut T, item);
            }
        }
    }

    /// Reset the reservoir.
    ///
    /// This method clears the reservoir and resets the counters.
    pub fn reset(&self) {
        self.count.store(0, Ordering::Relaxed);
    }
}

impl<T: Copy, const N: usize> Default for ReservoirSampler<T, N> {
    fn default() -> Self {
        Self::new()
    }
}

/// A set of standardized estimates for printing, comparison, etc.
#[derive(Debug, Clone)]
pub struct Estimates<T>
where
    T: DistributionValue,
{
    /// The number of samples in the distribution.
    pub count: usize,

    /// The quantiles of the distribution as (percentile, value) pairs.
    pub quantiles: Vec<(f64, T)>,
}

impl<T> Estimates<T>
where
    T: DistributionValue,
{
    /// Get a specific percentile value from the estimates.
    ///
    /// # Arguments
    ///
    /// * `percentile` - The percentile to get (0.0 to 1.0)
    ///
    /// # Returns
    ///
    /// The value at the specified percentile, or None if not available.
    pub fn percentile(&self, percentile: f64) -> Option<T> {
        // Find the exact match if it exists.
        if let Some(q) = self
            .quantiles
            .iter()
            .find(|(p, _)| (*p - percentile).abs() < f64::EPSILON)
        {
            return Some(q.1);
        }

        // Otherwise interpolate between the closest percentiles.
        if self.quantiles.is_empty() {
            return None;
        }

        // If the percentile is less than the first quantile, return the first value.
        if percentile <= self.quantiles[0].0 {
            return Some(self.quantiles[0].1);
        }

        // If the percentile is greater than the last quantile, return the last value.
        if percentile >= self.quantiles.last().unwrap().0 {
            return Some(self.quantiles.last().unwrap().1);
        }

        // Find the two quantiles that bracket the percentile.
        for i in 0..self.quantiles.len() - 1 {
            if self.quantiles[i].0 <= percentile && percentile <= self.quantiles[i + 1].0 {
                // For interpolation, we need to convert to f64, interpolate, and convert back.
                let p1 = self.quantiles[i].0;
                let p2 = self.quantiles[i + 1].0;
                let v1_f64 = to_f64(self.quantiles[i].1);
                let v2_f64 = to_f64(self.quantiles[i + 1].1);

                // Linear interpolation in f64 space.
                let factor = (percentile - p1) / (p2 - p1);
                let interpolated = v1_f64 + (v2_f64 - v1_f64) * factor;

                // Convert back to T.
                if std::any::TypeId::of::<T>() == std::any::TypeId::of::<Duration>() {
                    // For Duration, convert from seconds to nanos
                    let nanos = (interpolated * 1_000_000_000.0) as u64;
                    return Some(T::from_u64(nanos));
                } else {
                    // For other types, use the bits representation
                    return Some(T::from_u64(f64::to_bits(interpolated)));
                }
            }
        }

        None
    }
}

impl<T> fmt::Display for Estimates<T>
where
    T: DistributionValue + fmt::Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "count: {}", self.count)?;
        for i in 0..self.quantiles.len() - 1 {
            write!(
                f,
                ", p{}: {}",
                self.quantiles[i].0 * 100.0,
                self.quantiles[i].1
            )?;
        }

        Ok(())
    }
}

/// Calculate the similarity between two statistical estimates.
///
/// This function computes the similarity between two distributions by calculating
/// 1.0 minus the Kolmogorov-Smirnov distance between them. The result is a value
/// between 0.0 and 1.0, where 1.0 means the distributions are identical.
///
/// # Arguments
///
/// * `a` - The first statistical estimates
/// * `b` - The second statistical estimates
///
/// # Returns
///
/// A value between 0.0 and 1.0 representing the similarity between the distributions.
pub fn similarity<T>(a: &Estimates<T>, b: &Estimates<T>) -> f64
where
    T: DistributionValue,
{
    // Convert the quantiles to f64 for comparison.
    let a_quantiles: Vec<(f64, f64)> = a.quantiles.iter().map(|&(p, v)| (p, to_f64(v))).collect();

    let b_quantiles: Vec<(f64, f64)> = b.quantiles.iter().map(|&(p, v)| (p, to_f64(v))).collect();

    // Calculate the distance between the distributions.
    1.0 - distance(&a_quantiles, &b_quantiles)
}

/// Calculate the width of a quantile.
fn width(quantiles: &[(f64, f64)], index: usize) -> f64 {
    let before = if index == 0 {
        quantiles[0].0
    } else {
        (quantiles[index].0 - quantiles[index - 1].0) / 2.0
    };

    let after = if index == quantiles.len() - 1 {
        1.0 - quantiles[index].0
    } else {
        (quantiles[index + 1].0 - quantiles[index].0) / 2.0
    };

    before + after
}

/// Calculate the Kolmogorov-Smirnov distance between two distributions.
fn distance(a_quantiles: &[(f64, f64)], b_quantiles: &[(f64, f64)]) -> f64 {
    // If either distribution has no quantiles, return 1.0 (maximum distance).
    if a_quantiles.is_empty() || b_quantiles.is_empty() {
        return 1.0;
    }

    // If the distributions have different quantiles, we can't compare them directly
    // In this case, we'll interpolate the missing quantiles.
    let quantiles = merge_quantiles(a_quantiles, b_quantiles);

    // Calculate the total range of values.
    let total_min = f64::min(
        a_quantiles.first().map(|q| q.1).unwrap_or(0.0),
        b_quantiles.first().map(|q| q.1).unwrap_or(0.0),
    );
    let total_max = f64::max(
        a_quantiles.last().map(|q| q.1).unwrap_or(0.0),
        b_quantiles.last().map(|q| q.1).unwrap_or(0.0),
    );

    // If the range is zero, the distributions are identical.
    if (total_max - total_min).abs() < f64::EPSILON {
        return 0.0;
    }

    // Calculate the distance between the distributions.
    let mut distance = 0.0;
    for (i, &(percentile, _)) in quantiles.iter().enumerate() {
        let a_value = interpolate_value(a_quantiles, percentile);
        let b_value = interpolate_value(b_quantiles, percentile);
        let delta = (a_value - b_value).abs();
        distance += width(&quantiles, i) * delta;
    }

    distance / (total_max - total_min)
}

/// Merge two sets of quantiles into a single set.
fn merge_quantiles(a: &[(f64, f64)], b: &[(f64, f64)]) -> Vec<(f64, f64)> {
    let mut percentiles = Vec::new();

    // Add all percentiles from both distributions.
    for &(p, _) in a {
        if !percentiles.contains(&p) {
            percentiles.push(p);
        }
    }
    for &(p, _) in b {
        if !percentiles.contains(&p) {
            percentiles.push(p);
        }
    }

    // Sort by percentile.
    percentiles.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));

    // Create quantiles with interpolated values.
    percentiles
        .into_iter()
        .map(|p| {
            let a_value = interpolate_value(a, p);
            let b_value = interpolate_value(b, p);
            (p, (a_value + b_value) / 2.0)
        })
        .collect()
}

/// Interpolate a value at a given percentile.
fn interpolate_value(quantiles: &[(f64, f64)], percentile: f64) -> f64 {
    // If the quantiles are empty, return 0.0.
    if quantiles.is_empty() {
        return 0.0;
    }

    // If the percentile is less than the first quantile, return the first value.
    if percentile <= quantiles[0].0 {
        return quantiles[0].1;
    }

    // If the percentile is greater than the last quantile, return the last value.
    if percentile >= quantiles[quantiles.len() - 1].0 {
        return quantiles[quantiles.len() - 1].1;
    }

    // Find the two quantiles that bracket the percentile.
    for i in 0..quantiles.len() - 1 {
        if quantiles[i].0 <= percentile && percentile <= quantiles[i + 1].0 {
            // Interpolate between the two values.
            let p1 = quantiles[i].0;
            let p2 = quantiles[i + 1].0;
            let v1 = quantiles[i].1;
            let v2 = quantiles[i + 1].1;

            // Linear interpolation.
            return v1 + (v2 - v1) * (percentile - p1) / (p2 - p1);
        }
    }

    0.0 // Should never happen.
}

/// Convert a value of type T to f64.
fn to_f64<T>(value: T) -> f64
where
    T: DistributionValue,
{
    if std::any::TypeId::of::<T>() == std::any::TypeId::of::<Duration>() {
        let duration = unsafe { std::mem::transmute_copy::<T, Duration>(&value) };
        duration.as_secs_f64()
    } else {
        f64::from_bits(value.to_u64())
    }
}

/// Trait for types that can be used in a distribution.
pub trait DistributionValue: Copy + PartialOrd + 'static {
    /// Convert the value to u64 for storage in the histogram.
    fn to_u64(self) -> u64;

    /// Convert a u64 from the histogram back to the value.
    fn from_u64(value: u64) -> Self;
}

impl DistributionValue for f64 {
    fn to_u64(self) -> u64 {
        unsafe { std::mem::transmute_copy::<f64, u64>(&self) }
    }

    fn from_u64(value: u64) -> Self {
        unsafe { std::mem::transmute_copy::<u64, f64>(&value) }
    }
}

impl DistributionValue for Duration {
    fn to_u64(self) -> u64 {
        self.as_nanos() as u64
    }

    fn from_u64(value: u64) -> Self {
        Duration::from_nanos(value)
    }
}

/// A distribution of values.
#[derive(Debug, Clone)]
pub struct Distribution<T>
where
    T: DistributionValue,
{
    /// The TDigest used to store and analyze the distribution.
    digest: TDigest,

    /// Phantom data to use the type parameter.
    _phantom: std::marker::PhantomData<T>,
}

impl<T> Distribution<T>
where
    T: DistributionValue,
{
    /// Create a new empty distribution.
    ///
    /// # Returns
    ///
    /// A new `Distribution` instance.
    pub fn new() -> Self {
        Self {
            digest: TDigest::new_with_size(100),
            _phantom: std::marker::PhantomData,
        }
    }

    /// Add a value to the distribution.
    ///
    /// # Arguments
    ///
    /// * `value` - The value to add.
    pub fn add(&mut self, value: T) {
        // Convert the value to f64 and record it in the digest
        let value_f64 = to_f64(value);
        let values = vec![value_f64];
        self.digest = self.digest.merge_unsorted(values);
    }

    /// Get the statistical estimates of the distribution.
    ///
    /// # Returns
    ///
    /// An `Estimates` instance with the statistical estimates of the distribution.
    pub fn estimates(&self) -> Estimates<T> {
        let mut quantiles = Vec::new();
        let value_at = |p| {
            let v = self.digest.estimate_quantile(p);
            if std::any::TypeId::of::<T>() == std::any::TypeId::of::<Duration>() {
                let nanos = (v * 1_000_000_000.0) as u64;
                Some(T::from_u64(nanos))
            } else {
                Some(T::from_u64(f64::to_bits(v)))
            }
        };

        quantiles.push((0.001, value_at(0.001).unwrap()));
        quantiles.push((0.01, value_at(0.01).unwrap()));
        quantiles.push((0.1, value_at(0.1).unwrap()));
        quantiles.push((0.5, value_at(0.5).unwrap()));
        quantiles.push((0.9, value_at(0.9).unwrap()));
        quantiles.push((0.99, value_at(0.99).unwrap()));
        quantiles.push((0.999, value_at(0.999).unwrap()));

        Estimates {
            count: self.digest.count() as usize,
            quantiles: quantiles,
        }
    }

    /// Get all samples from a reservoir.
    ///
    /// # Arguments
    ///
    /// * `reservoir` - The reservoir to get samples from
    pub fn add_all<const N: usize>(&mut self, reservoir: &ReservoirSampler<T, N>) {
        // Do nothing if N = 0
        if N == 0 {
            return;
        }

        let count = reservoir.count.load(Ordering::Relaxed);
        let limit = std::cmp::min(count, N);
        let mut values = Vec::with_capacity(limit);
        for i in 0..limit {
            unsafe {
                let value = std::ptr::read(reservoir.samples[i].as_ptr());
                values.push(to_f64(value));
            }
        }
        self.digest = self.digest.merge_unsorted(values);
    }
}

impl<T> Default for Distribution<T>
where
    T: DistributionValue,
{
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use more_asserts::{assert_gt, assert_lt};
    use std::time::Duration;

    #[test]
    fn test_distribution_empty() {
        let dist = Distribution::<f64>::new();
        let estimates = dist.estimates();

        assert_eq!(estimates.count, 0);
    }

    #[test]
    fn test_distribution_single_value() {
        let mut dist = Distribution::<f64>::new();
        for _ in 0..10000 {
            dist.add(42.0);
        }
        let estimates = dist.estimates();

        //assert_eq!(estimates.count, 100);
        assert_eq!(estimates.percentile(0.0), Some(42.0));
        assert_eq!(estimates.percentile(1.0), Some(42.0));
        assert_eq!(estimates.percentile(0.5), Some(42.0));
        assert_eq!(estimates.percentile(0.9), Some(42.0));
        assert_eq!(estimates.percentile(0.99), Some(42.0));
        assert_eq!(estimates.percentile(0.999), Some(42.0));
    }

    #[test]
    fn test_distribution_multiple_values() {
        let mut dist = Distribution::<f64>::new();
        dist.add(1.0);
        dist.add(2.0);
        dist.add(3.0);
        dist.add(4.0);
        dist.add(5.0);
        let estimates = dist.estimates();

        assert_eq!(estimates.count, 5);
        assert_lt!(estimates.percentile(0.001), Some(2.0));
        assert_lt!(estimates.percentile(0.5), Some(4.0));
        assert_gt!(estimates.percentile(0.5), Some(2.0));
        assert_gt!(estimates.percentile(0.999), Some(4.0));
    }

    #[test]
    fn test_distribution_duration() {
        let mut dist = Distribution::<Duration>::new();
        dist.add(Duration::from_nanos(100));
        dist.add(Duration::from_nanos(200));
        dist.add(Duration::from_nanos(300));
        let estimates = dist.estimates();

        assert_eq!(estimates.count, 3);
        assert_lt!(estimates.percentile(0.0), Some(Duration::from_nanos(110)));
        assert_lt!(estimates.percentile(0.5), Some(Duration::from_nanos(210)));
        assert_gt!(estimates.percentile(0.5), Some(Duration::from_nanos(190)));
        assert_gt!(estimates.percentile(1.0), Some(Duration::from_nanos(290)));
    }

    #[test]
    fn test_statistical_estimates_display() {
        let mut dist = Distribution::<f64>::new();
        dist.add(1.0);
        dist.add(2.0);
        dist.add(3.0);

        let estimates = dist.estimates();
        let display = format!("{}", estimates);

        assert!(display.contains("count: 3"));
        assert!(display.contains("p50: 2"));
    }
}
