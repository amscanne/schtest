#include <chrono>
#include <gtest/gtest.h>
#include <sstream>

#include "util/stats.h"

using namespace schtest;

// Test the Histogram constructor with integer type
TEST(Histogram, IntegerConstructor) {
  Histogram<int, 10> h(0, 100);
  // No explicit assertions needed here, just checking that construction works
}

// Test the Histogram constructor with floating-point type
TEST(Histogram, FloatConstructor) {
  Histogram<double, 10> h(0.0, 100.0);
  // No explicit assertions needed here, just checking that construction works
}

// Test the Histogram constructor with chrono duration type
TEST(Histogram, ChronoDurationConstructor) {
  Histogram<std::chrono::nanoseconds, 10> h(std::chrono::nanoseconds(0),
                                            std::chrono::nanoseconds(100));
  // No explicit assertions needed here, just checking that construction works
}

// Test the Histogram constructor with zero width
TEST(Histogram, ZeroWidthConstructor) {
  Histogram<int, 10> h(5, 5); // min == max, so width will be 0
  // No explicit assertions needed here, just checking that construction works
}

// Test the Histogram add method with integer type
TEST(Histogram, IntegerAdd) {
  Histogram<int, 5> h(0, 100);
  h.add(20);
  h.add(50);
  h.add(80);
  // No explicit assertions needed here, just checking that add works
}

// Test the Histogram add method with chrono duration type
TEST(Histogram, ChronoDurationAdd) {
  Histogram<std::chrono::nanoseconds, 5> h(std::chrono::nanoseconds(0),
                                           std::chrono::nanoseconds(100));
  h.add(std::chrono::nanoseconds(20));
  h.add(std::chrono::nanoseconds(50));
  h.add(std::chrono::nanoseconds(80));
  // No explicit assertions needed here, just checking that add works
}

// Test the Histogram add method with zero width
TEST(Histogram, ZeroWidthAdd) {
  Histogram<int, 5> h(5, 5); // min == max, so width will be 0
  h.add(5);
  // No explicit assertions needed here, just checking that add works with zero
  // width
}

// Test the Distribution sample method with integer type
TEST(Distribution, IntegerSample) {
  Distribution<int> d;
  d.sample(10);
  d.sample(20);
  d.sample(30);
  // No explicit assertions needed here, just checking that sample works
}

// Test the Distribution sample method with floating-point type
TEST(Distribution, FloatSample) {
  Distribution<double> d;
  d.sample(10.5);
  d.sample(20.5);
  d.sample(30.5);
  // No explicit assertions needed here, just checking that sample works
}

// Test the Distribution sample method with chrono duration type
TEST(Distribution, ChronoDurationSample) {
  Distribution<std::chrono::nanoseconds> d;
  d.sample(std::chrono::nanoseconds(10));
  d.sample(std::chrono::nanoseconds(20));
  d.sample(std::chrono::nanoseconds(30));
  // No explicit assertions needed here, just checking that sample works
}

// Test the Distribution histogram method with integer type
TEST(Distribution, IntegerHistogram) {
  Distribution<int> d;
  // Add a range of values to get a meaningful distribution
  for (int i = 0; i < 1000; i++) {
    d.sample(i);
  }

  auto h = d.histogram<10>();
  // No explicit assertions needed here, just checking that histogram works
}

// Test the Distribution histogram method with chrono duration type
TEST(Distribution, ChronoDurationHistogram) {
  Distribution<std::chrono::nanoseconds> d;
  // Add a range of values to get a meaningful distribution
  for (int i = 0; i < 1000; i++) {
    d.sample(std::chrono::nanoseconds(i));
  }

  auto h = d.histogram<10>();
  // No explicit assertions needed here, just checking that histogram works
}

// Test the similar function with integer distributions
TEST(Distribution, SimilarIntegerDistributions) {
  Distribution<int> d1;
  Distribution<int> d2;

  // Add similar values to both distributions
  for (int i = 0; i < 1000; i++) {
    d1.sample(i);
    d2.sample(i);
  }

  // These should be similar with high confidence
  EXPECT_TRUE(similar(d1, d2, 0.95));
}

// Test the similar function with different integer distributions
TEST(Distribution, DifferentIntegerDistributions) {
  Distribution<int> d1;
  Distribution<int> d2;

  // Add different ranges of values to the distributions
  for (int i = 0; i < 1000; i++) {
    d1.sample(i);
    d2.sample(i + 500); // Shifted by 500
  }

  // These should not be similar with high confidence
  EXPECT_FALSE(similar(d1, d2, 0.95));
}

// Test the similar function with chrono duration distributions
TEST(Distribution, SimilarChronoDurationDistributions) {
  Distribution<std::chrono::nanoseconds> d1;
  Distribution<std::chrono::nanoseconds> d2;

  // Add similar values to both distributions
  for (int i = 0; i < 1000; i++) {
    d1.sample(std::chrono::nanoseconds(i));
    d2.sample(std::chrono::nanoseconds(i));
  }

  // These should be similar with high confidence
  EXPECT_TRUE(similar(d1, d2, 0.95));
}

// Test the similar function with different chrono duration distributions
TEST(Distribution, DifferentChronoDurationDistributions) {
  Distribution<std::chrono::nanoseconds> d1;
  Distribution<std::chrono::nanoseconds> d2;

  // Add different ranges of values to the distributions
  for (int i = 0; i < 1000; i++) {
    d1.sample(std::chrono::nanoseconds(i));
    d2.sample(std::chrono::nanoseconds(i + 500)); // Shifted by 500
  }

  // These should not be similar with high confidence
  EXPECT_FALSE(similar(d1, d2, 0.95));
}

// Test the operator<< for Histogram
TEST(Histogram, HistogramOutput) {
  Histogram<int, 5> h(0, 100);
  h.add(20);
  h.add(50);
  h.add(80);

  std::stringstream ss;
  ss << h;

  // Check that the output is not empty
  EXPECT_FALSE(ss.str().empty());
}

// Test the operator<< for Distribution
TEST(Distribution, DistributionOutput) {
  Distribution<int> d;
  d.sample(10);
  d.sample(20);
  d.sample(30);

  std::stringstream ss;
  ss << d;

  // Check that the output is not empty
  EXPECT_FALSE(ss.str().empty());
}

// Test the LatencyDistribution typedef
TEST(Distribution, LatencyDistributionTypeDef) {
  LatencyDistribution d;
  d.sample(std::chrono::nanoseconds(10));
  d.sample(std::chrono::nanoseconds(20));
  d.sample(std::chrono::nanoseconds(30));

  std::stringstream ss;
  ss << d;

  // Check that the output is not empty
  EXPECT_FALSE(ss.str().empty());
}

// Test the zscore function indirectly through the similar function
TEST(Distribution, ZScoreViaSimlar) {
  Distribution<int> d1;
  Distribution<int> d2;

  // Add similar values to both distributions
  for (int i = 0; i < 1000; i++) {
    d1.sample(i);
    d2.sample(i);
  }

  // Test with different confidence levels
  EXPECT_TRUE(similar(d1, d2, 0.95));
  EXPECT_TRUE(similar(d1, d2, 0.99));
  EXPECT_TRUE(similar(d1, d2, 0.999));
}
