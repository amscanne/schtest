#include <chrono>
#include <gtest/gtest.h>
#include <sstream>

#include "util/stats.h"

using namespace schtest;

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
