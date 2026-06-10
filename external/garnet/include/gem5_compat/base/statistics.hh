/**
 * @file statistics.hh
 * @brief Statistics framework stub for Garnet
 */

#ifndef __GARNET_COMPAT_BASE_STATISTICS_HH__
#define __GARNET_COMPAT_BASE_STATISTICS_HH__

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <functional>

namespace gem5 {
namespace statistics {

// Forward declarations
class Formula;

/**
 * Scalar - single counter statistic
 */
class Scalar {
public:
    Scalar() : value_(0), name_("") {}

    Scalar& name(const std::string& n) { name_ = n; return *this; }
    Scalar& desc(const std::string& d) { desc_ = d; return *this; }
    Scalar& flags(int f) { return *this; }
    Scalar& prereq(const Scalar& s) { return *this; }

    void operator++() { ++value_; }
    void operator++(int) { ++value_; }
    void operator--() { --value_; }
    void operator+=(uint64_t v) { value_ += v; }
    void operator=(uint64_t v) { value_ = v; }

    operator uint64_t() const { return value_; }
    uint64_t value() const { return value_; }

    void reset() { value_ = 0; }

private:
    uint64_t value_;
    std::string name_;
    std::string desc_;
};

/**
 * Vector - vector of counters
 */
class Vector {
public:
    Vector() : name_("") {}

    Vector& name(const std::string& n) { name_ = n; return *this; }
    Vector& desc(const std::string& d) { desc_ = d; return *this; }
    Vector& flags(int f) { return *this; }
    Vector& init(size_t size) { values_.resize(size, 0); subnames_.resize(size); return *this; }
    Vector& prereq(const Scalar& s) { return *this; }
    Vector& subname(size_t i, const std::string& name) {
        if (i < subnames_.size()) subnames_[i] = name;
        return *this;
    }

    uint64_t& operator[](size_t i) {
        if (i >= values_.size()) {
            values_.resize(i + 1, 0);
            subnames_.resize(i + 1);
        }
        return values_[i];
    }

    const uint64_t& operator[](size_t i) const {
        static uint64_t zero = 0;
        if (i >= values_.size()) return zero;
        return values_[i];
    }

    size_t size() const { return values_.size(); }

    uint64_t total() const {
        uint64_t sum = 0;
        for (auto v : values_) sum += v;
        return sum;
    }

    void reset() {
        for (auto& v : values_) v = 0;
    }

    // Division operator for formula support
    friend Formula operator/(const Vector& a, const Vector& b);

private:
    std::vector<uint64_t> values_;
    std::vector<std::string> subnames_;
    std::string name_;
    std::string desc_;
};

/**
 * Average - running average statistic
 */
class Average {
public:
    Average() : sum_(0), count_(0), name_("") {}

    Average& name(const std::string& n) { name_ = n; return *this; }
    Average& desc(const std::string& d) { desc_ = d; return *this; }
    Average& flags(int f) { return *this; }
    Average& prereq(const Scalar& s) { return *this; }

    void sample(double v) { sum_ += v; ++count_; }

    double value() const {
        return count_ > 0 ? sum_ / count_ : 0.0;
    }

    void reset() { sum_ = 0; count_ = 0; }

private:
    double sum_;
    uint64_t count_;
    std::string name_;
    std::string desc_;
};

/**
 * Formula - computed statistic
 */
class Formula {
public:
    Formula() : name_("") {}

    Formula& name(const std::string& n) { name_ = n; return *this; }
    Formula& desc(const std::string& d) { desc_ = d; return *this; }
    Formula& flags(int f) { return *this; }
    Formula& prereq(const Scalar& s) { return *this; }

    // Assignment operators for formula computation
    Formula& operator=(const Scalar& s) { return *this; }
    Formula& operator=(const Vector& v) { return *this; }
    Formula& operator=(const Formula& f) { return *this; }
    Formula& operator=(double v) { value_ = v; return *this; }

    // Expression building (simplified - just stores for debugging)
    template<typename T>
    Formula& operator/(const T& other) { return *this; }

    template<typename T>
    Formula& operator*(const T& other) { return *this; }

    template<typename T>
    Formula& operator+(const T& other) { return *this; }

    template<typename T>
    Formula& operator-(const T& other) { return *this; }

    double value() const { return value_; }

private:
    std::string name_;
    std::string desc_;
    double value_ = 0.0;
};

/**
 * Histogram - distribution statistic
 */
class Histogram {
public:
    Histogram() : name_("") {}

    Histogram& name(const std::string& n) { name_ = n; return *this; }
    Histogram& desc(const std::string& d) { desc_ = d; return *this; }
    Histogram& flags(int f) { return *this; }
    Histogram& init(size_t buckets) { counts_.resize(buckets, 0); return *this; }

    void sample(double v) {
        size_t bucket = static_cast<size_t>(v);
        if (bucket < counts_.size())
            counts_[bucket]++;
    }

    void reset() {
        for (auto& c : counts_) c = 0;
    }

private:
    std::vector<uint64_t> counts_;
    std::string name_;
    std::string desc_;
};

/**
 * Distribution - more detailed distribution
 */
class Distribution : public Histogram {
public:
    Distribution& init(double min, double max, double bucket_size) {
        min_ = min;
        max_ = max;
        bucketSize_ = bucket_size;
        Histogram::init(static_cast<size_t>((max - min) / bucket_size) + 1);
        return *this;
    }

private:
    double min_ = 0;
    double max_ = 0;
    double bucketSize_ = 1;
};

// Statistics flags (bit flags)
enum StatFlag {
    none = 0,
    nozero = 1,
    nonan = 2,
    total = 4,
    pdf = 8,
    cdf = 16,
    dist = 32,
    oneline = 64
};

// Sum function for vectors — returns double to avoid integer division-by-zero
// (SIGFPE) when used in formulas like sum(latency) / sum(packets_received)
// before any packets have been processed.  In real gem5, sum() returns a
// lazy stat node; here we evaluate immediately.
inline double sum(const Vector& v) {
    return static_cast<double>(v.total());
}

// Constant wrapper
template<typename T>
class Constant {
public:
    Constant(T v) : value_(v) {}
    operator T() const { return value_; }
private:
    T value_;
};

template<typename T>
Constant<T> constant(T v) { return Constant<T>(v); }

// Vector division returns a Formula
inline Formula operator/(const Vector& a, const Vector& b) {
    Formula f;
    // In real gem5, this computes element-wise division
    // For standalone, we just return an empty formula
    return f;
}

} // namespace statistics

// csprintf - formatted string (gem5 utility)
template<typename... Args>
inline std::string csprintf(const char* fmt, Args... args) {
    char buf[1024];
    snprintf(buf, sizeof(buf), fmt, args...);
    return std::string(buf);
}

// Alias for gem5 compatibility
namespace Stats = statistics;

} // namespace gem5

#endif // __GARNET_COMPAT_BASE_STATISTICS_HH__
