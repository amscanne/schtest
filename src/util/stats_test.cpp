#include <chrono>
#include <gtest/gtest.h>
#include <sstream>

#include "util/stats.h"

using namespace schtest;

TEST(HistogramTest, Constructor) {
  Histogram<int, 10> h(0, 100);
  EXPECT_EQ(h.bucket_count(), 11u);
  EXPECT_EQ(h.min(), 0);
  EXPECT_EQ(h.max(), 100);
}

TEST(HistogramTest, ZeroWidthConstructor) {
  Histogram<int, 10> h(5, 5);
  EXPECT_EQ(h.bucket_count(), 1u);
}

TEST(HistogramTest, Add) {
  Histogram<int, 10> h(0, 100);
  h.add(20);
  h.add(50);
  h.add(80);
  EXPECT_EQ(h.samples(), 3u);
}

TEST(HistogramTest, AddAtUpperBound) {
  Histogram<int, 5> h(0, 100);
  h.add(100);
  EXPECT_EQ(h.samples(), 1u);
}

TEST(HistogramTest, ZeroWidthAdd) {
  Histogram<int, 5> h(5, 5);
  h.add(5);
  EXPECT_EQ(h.samples(), 1u);
}

TEST(HistogramTest, Output) {
  Histogram<int, 10> h(0, 100);
  h.add(20);
  h.add(50);
  h.add(80);
  std::stringstream ss;
  ss << h;
  EXPECT_FALSE(ss.str().empty());
}

TEST(HistogramTest, DoubleConstructor) {
  Histogram<double, 10> h(0.0, 100.0);
  EXPECT_EQ(h.bucket_count(), 11u);
  EXPECT_DOUBLE_EQ(h.min(), 0.0);
  EXPECT_DOUBLE_EQ(h.max(), 100.0);
}

TEST(HistogramTest, DoubleZeroWidthConstructor) {
  Histogram<double, 10> h(5.0, 5.0);
  EXPECT_EQ(h.bucket_count(), 1u);
}

TEST(HistogramTest, DoubleAdd) {
  Histogram<double, 10> h(0.0, 100.0);
  h.add(20.0);
  h.add(50.0);
  h.add(80.0);
  EXPECT_EQ(h.samples(), 3u);
}

TEST(HistogramTest, DoubleAddAtUpperBound) {
  Histogram<double, 5> h(0.0, 100.0);
  h.add(100.0);
  EXPECT_EQ(h.samples(), 1u);
}

TEST(HistogramTest, DoubleZeroWidthAdd) {
  Histogram<double, 5> h(5.0, 5.0);
  h.add(5.0);
  EXPECT_EQ(h.samples(), 1u);
}

TEST(HistogramTest, DoubleOutput) {
  Histogram<double, 10> h(0.0, 100.0);
  h.add(20.0);
  h.add(50.0);
  h.add(80.0);
  std::stringstream ss;
  ss << h;
  EXPECT_FALSE(ss.str().empty());
}

TEST(HistogramTest, DurationConstructor) {
  Histogram<std::chrono::nanoseconds, 10> h(std::chrono::nanoseconds(0),
                                            std::chrono::nanoseconds(100));
  EXPECT_EQ(h.bucket_count(), 11u);
  EXPECT_EQ(h.min(), std::chrono::nanoseconds(0));
  EXPECT_EQ(h.max(), std::chrono::nanoseconds(100));
}

TEST(HistogramTest, DurationZeroWidthConstructor) {
  Histogram<std::chrono::nanoseconds, 10> h(std::chrono::nanoseconds(5),
                                            std::chrono::nanoseconds(5));
  EXPECT_EQ(h.bucket_count(), 1u);
}

TEST(HistogramTest, DurationAdd) {
  Histogram<std::chrono::nanoseconds, 10> h(std::chrono::nanoseconds(0),
                                            std::chrono::nanoseconds(100));
  h.add(std::chrono::nanoseconds(20));
  h.add(std::chrono::nanoseconds(50));
  h.add(std::chrono::nanoseconds(80));
  EXPECT_EQ(h.samples(), 3u);
}

TEST(HistogramTest, DurationAddAtUpperBound) {
  Histogram<std::chrono::nanoseconds, 5> h(std::chrono::nanoseconds(0),
                                           std::chrono::nanoseconds(100));
  h.add(std::chrono::nanoseconds(100));
  EXPECT_EQ(h.samples(), 1u);
}

TEST(HistogramTest, DurationZeroWidthAdd) {
  Histogram<std::chrono::nanoseconds, 5> h(std::chrono::nanoseconds(5),
                                           std::chrono::nanoseconds(5));
  h.add(std::chrono::nanoseconds(5));
  EXPECT_EQ(h.samples(), 1u);
}

TEST(HistogramTest, DurationOutput) {
  Histogram<std::chrono::nanoseconds, 10> h(std::chrono::nanoseconds(0),
                                            std::chrono::nanoseconds(100));
  h.add(std::chrono::nanoseconds(20));
  h.add(std::chrono::nanoseconds(50));
  h.add(std::chrono::nanoseconds(80));
  std::stringstream ss;
  ss << h;
  EXPECT_FALSE(ss.str().empty());
}

TEST(DistributionTest, IntSample) {
  Distribution<int> d;
  d.sample(10);
  d.sample(20);
  d.sample(30);
}

TEST(DistributionTest, DoubleSample) {
  Distribution<double> d;
  d.sample(10.5);
  d.sample(20.5);
  d.sample(30.5);
}

TEST(DistributionTest, DurationSample) {
  Distribution<std::chrono::nanoseconds> d;
  d.sample(std::chrono::nanoseconds(10));
  d.sample(std::chrono::nanoseconds(20));
  d.sample(std::chrono::nanoseconds(30));
}

TEST(DistributionTest, IntHistogram) {
  Distribution<int> d;
  for (int i = 0; i < 1000; i++)
    d.sample(i);
  auto h = d.histogram<10>();
  EXPECT_EQ(h.bucket_count(), 11u);
}

TEST(DistributionTest, DurationHistogram) {
  Distribution<std::chrono::nanoseconds> d;
  for (int i = 0; i < 1000; i++)
    d.sample(std::chrono::nanoseconds(i));
  auto h = d.histogram<10>();
  EXPECT_EQ(h.bucket_count(), 11u);
}

TEST(DistributionTest, IntSimilarDistributions) {
  Distribution<int> d1, d2;
  for (int i = 0; i < 1000; i++) {
    d1.sample(i);
    d2.sample(i);
  }
  EXPECT_GT(similarity(d1, d2), 0.95);
}

TEST(DistributionTest, IntDifferentDistributions) {
  Distribution<int> d1, d2;
  for (int i = 0; i < 1000; i++) {
    d1.sample(i);
    d2.sample(i + 500);
  }
  EXPECT_LT(similarity(d1, d2), 0.95);
}

TEST(DistributionTest, DurationSimilarDistributions) {
  Distribution<std::chrono::nanoseconds> d1, d2;
  for (int i = 0; i < 1000; i++) {
    d1.sample(std::chrono::nanoseconds(i));
    d2.sample(std::chrono::nanoseconds(i));
  }
  EXPECT_GT(similarity(d1, d2), 0.95);
}

TEST(DistributionTest, DurationDifferentDistributions) {
  Distribution<std::chrono::nanoseconds> d1, d2;
  for (int i = 0; i < 1000; i++) {
    d1.sample(std::chrono::nanoseconds(i));
    d2.sample(std::chrono::nanoseconds(i + 500));
  }
  EXPECT_LT(similarity(d1, d2), 0.95);
}

TEST(DistributionTest, IntOutput) {
  Distribution<int> d;
  d.sample(10);
  d.sample(20);
  d.sample(30);
  std::stringstream ss;
  ss << d;
  EXPECT_FALSE(ss.str().empty());
}

TEST(DistributionTest, DurationLatencyDistributionTypeDef) {
  LatencyDistribution d;
  d.sample(std::chrono::nanoseconds(10));
  d.sample(std::chrono::nanoseconds(20));
  d.sample(std::chrono::nanoseconds(30));
  std::stringstream ss;
  ss << d;
  EXPECT_FALSE(ss.str().empty());
}

TEST(DistributionTest, IntZScoreViaSimilar) {
  Distribution<int> d1, d2;
  for (int i = 0; i < 1000; i++) {
    d1.sample(i);
    d2.sample(i);
  }
  EXPECT_GT(similarity(d1, d2), 0.95);
  EXPECT_GT(similarity(d1, d2), 0.99);
  EXPECT_GT(similarity(d1, d2), 0.999);
}
