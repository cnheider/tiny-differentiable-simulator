#pragma once

#include <cstdio>
#include <ostream>

#include "../base.hpp"

namespace tds {
template <typename Algebra>
struct SpatialVector {
  using Scalar = typename Algebra::Scalar;
  using Vector3 = typename Algebra::Vector3;
  // using Vector6 = typename Algebra::Vector6;
  using Matrix6 = typename Algebra::Matrix6;

  Vector3 top{Algebra::zero3()};
  Vector3 bottom{Algebra::zero3()};

  SpatialVector() = default;
  SpatialVector(const Vector3 &top, const Vector3 &bottom)
      : top(top), bottom(bottom) {}
  SpatialVector(const SpatialVector &v) = default;
  // SpatialVector(const Vector6 &vec)
  //     : top(vec[0], vec[1], vec[2]), bottom(vec[3], vec[4], vec[5]) {}

  // operator Vector6() const {
  //   return Vector6(top[0], top[1], top[2], bottom[0], bottom[1], bottom[2]);
  // }

  TINY_INLINE Scalar &operator[](int i) {
    if (i < 3)
      return top[i];
    else
      return bottom[i - 3];
  }
  const TINY_INLINE Scalar &operator[](int i) const {
    if (i < 3)
      return top[i];
    else
      return bottom[i - 3];
  }

  TINY_INLINE void set_zero() {
    Algebra::set_zero(top);
    Algebra::set_zero(bottom);
  }

  friend std::ostream &operator<<(std::ostream &os, const SpatialVector &v) {
    os << "[ " << v.top << "  " << v.bottom << " ]";
    return os;
  }

  void print(const char *name) const {
    printf("%s\n", name);
    double x = Algebra::to_double(top[0]);
    double y = Algebra::to_double(top[1]);
    double z = Algebra::to_double(top[2]);
    printf("%.16f,%.16f,%.16f,    ", x, y, z);
    x = Algebra::to_double(bottom[0]);
    y = Algebra::to_double(bottom[1]);
    z = Algebra::to_double(bottom[2]);
    printf("%.16f,%.16f,%.16f\n", x, y, z);
  }

  friend SpatialVector operator*(const Matrix6 &m, const SpatialVector &v) {
    SpatialVector result;
    for (typename Algebra::Index i = 0; i < 6; ++i) {
      for (typename Algebra::Index j = 0; j < 6; ++j) {
        result[i] += m(i, j) * v[j];
      }
    }
    return result;
  }

  //   ENOKI_STRUCT(SpatialVector, top, bottom)
};
// ENOKI_STRUCT_SUPPORT(SpatialVector, top, bottom)

template <typename Algebra>
struct MotionVector : public SpatialVector<Algebra> {
  using SpatialVector = tds::SpatialVector<Algebra>;
  using Scalar = typename Algebra::Scalar;
  using SpatialVector::bottom;
  using SpatialVector::SpatialVector;
  using SpatialVector::top;

  explicit MotionVector(const SpatialVector &v) {
    top = v.top;
    bottom = v.bottom;
  }

  TINY_INLINE MotionVector operator-(const MotionVector &vec) const {
    return MotionVector(top - vec.top, bottom - vec.bottom);
  }
  TINY_INLINE MotionVector operator+(const MotionVector &vec) const {
    return MotionVector(top + vec.top, bottom + vec.bottom);
  }

  TINY_INLINE MotionVector &operator+=(const MotionVector &vec) {
    top += vec.top;
    bottom += vec.bottom;
    return *this;
  }
  TINY_INLINE MotionVector &operator-=(const MotionVector &vec) {
    top -= vec.top;
    bottom -= vec.bottom;
    return *this;
  }

  TINY_INLINE MotionVector &operator*=(const Scalar &s) {
    top *= s;
    bottom *= s;
    return *this;
  }

  TINY_INLINE MotionVector operator-() const {
    return MotionVector(-top, -bottom);
  }
  TINY_INLINE MotionVector operator*(const Scalar &s) const {
    return MotionVector(s * top, s * bottom);
  }
};

template <typename Algebra>
struct ForceVector : public SpatialVector<Algebra> {
  using SpatialVector = tds::SpatialVector<Algebra>;
  using Scalar = typename Algebra::Scalar;
  // using Vector6 = typename Algebra::Vector6;
  using Matrix6 = typename Algebra::Matrix6;
  using SpatialVector::bottom;
  using SpatialVector::SpatialVector;
  using SpatialVector::top;

  TINY_INLINE ForceVector operator-(const ForceVector &vec) const {
    return ForceVector(top - vec.top, bottom - vec.bottom);
  }
  TINY_INLINE ForceVector operator+(const ForceVector &vec) const {
    return ForceVector(top + vec.top, bottom + vec.bottom);
  }

  TINY_INLINE ForceVector &operator+=(const ForceVector &vec) {
    top += vec.top;
    bottom += vec.bottom;
    return *this;
  }
  TINY_INLINE ForceVector &operator-=(const ForceVector &vec) {
    top -= vec.top;
    bottom -= vec.bottom;
    return *this;
  }

  TINY_INLINE ForceVector &operator*=(const Scalar &s) {
    top *= s;
    bottom *= s;
    return *this;
  }

  TINY_INLINE ForceVector operator-() const {
    return ForceVector(-top, -bottom);
  }
  TINY_INLINE ForceVector operator*(const Scalar &s) const {
    return ForceVector(s * top, s * bottom);
  }

  /**
   * This function only exists to multiply the inverse of the 6x6 inertia matrix
   * (ABI) with the bias force vector of the MultiBody base.
   */
  // TINY_INLINE friend MotionVector<Algebra> operator*(const Matrix6 &m,
  //                                                    const ForceVector &v) {
  //   Vector6 v6 = v;
  //   return m * v6;
  // }
};
}  // namespace tds
