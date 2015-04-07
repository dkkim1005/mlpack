/**
 * @file hrectbound.hpp
 *
 * Bounds that are useful for binary space partitioning trees.
 *
 * This file describes the interface for the HRectBound class, which implements
 * a hyperrectangle bound.
 */
#ifndef __MLPACK_CORE_TREE_HRECTBOUND_HPP
#define __MLPACK_CORE_TREE_HRECTBOUND_HPP

#include <mlpack/core.hpp>
#include <mlpack/core/math/range.hpp>
#include <mlpack/core/metrics/lmetric.hpp>

namespace mlpack {
namespace bound {

/**
 * Hyper-rectangle bound for an L-metric.  This should be used in conjunction
 * with the LMetric class.  Be sure to use the same template parameters for
 * LMetric as you do for HRectBound -- otherwise odd results may occur.
 *
 * @tparam Power The metric to use; use 2 for Euclidean (L2).
 * @tparam TakeRoot Whether or not the root should be taken (see LMetric
 *     documentation).
 */
template<int Power = 2, bool TakeRoot = true>
class HRectBound
{
 public:
  //! This is the metric type that this bound is using.
  typedef metric::LMetric<Power, TakeRoot> MetricType;

  /**
   * Empty constructor; creates a bound of dimensionality 0.
   */
  HRectBound();

  /**
   * Initializes to specified dimensionality with each dimension the empty
   * set.
   */
  HRectBound(const size_t dimension);

  /**
   * Initialize to the specified dimensionality, and use the given memory.
   * Warning: you are required to know what you are doing.  The destructor
   * ~HRectBound() is going to try to free the memory.  If that's not a thing
   * you want, make sure Dim() is set to 0 before the destructor is called,
   * which will cause the destructor to not try to free the memory.
   *
   * The reason it is like this is that we don't want to hold an extra bool to
   * indicate whether or not we need to free the memory, in order to keep the
   * structure small.
   *
   * @param dimension Dimensionality of the bound.
   * @param memory Pre-allocated memory to use (be careful!).
   */
  HRectBound(const size_t dimension, math::Range* memory);

  //! Copy constructor; necessary to prevent memory leaks.
  HRectBound(const HRectBound& other);
  //! Same as copy constructor; necessary to prevent memory leaks.
  HRectBound& operator=(const HRectBound& other);

  //! Destructor: clean up memory, but only if the dimensionality (Dim()) is
  //! greater than 0.
  ~HRectBound();

  /**
   * Resets all dimensions to the empty set (so that this bound contains
   * nothing).
   */
  void Clear();

  //! Gets the dimensionality.
  size_t Dim() const { return dim; }

  //! Get the range for a particular dimension.  No bounds checking.  Be
  //! careful: this may make MinWidth() invalid.
  math::Range& operator[](const size_t i) { return bounds[i]; }
  //! Modify the range for a particular dimension.  No bounds checking.
  const math::Range& operator[](const size_t i) const { return bounds[i]; }

  //! Get the minimum width of the bound.
  double MinWidth() const { return minWidth; }
  //! Modify the minimum width of the bound.
  double& MinWidth() { return minWidth; }

  /**
   * Calculates the centroid of the range, placing it into the given vector.
   *
   * @param centroid Vector which the centroid will be written to.
   */
  void Centroid(arma::vec& centroid) const;

  /**
   * Calculate the volume of the hyperrectangle.
   *
   * @return Volume of the hyperrectangle.
   */
  double Volume() const;

  /**
   * Calculates minimum bound-to-point distance.
   *
   * @param point Point to which the minimum distance is requested.
   */
  template<typename VecType>
  double MinDistance(const VecType& point,
                     typename boost::enable_if<IsVector<VecType> >* = 0) const;

  /**
   * Calculates minimum bound-to-bound distance.
   *
   * @param other Bound to which the minimum distance is requested.
   */
  double MinDistance(const HRectBound& other) const;

  /**
   * Calculates maximum bound-to-point squared distance.
   *
   * @param point Point to which the maximum distance is requested.
   */
  template<typename VecType>
  double MaxDistance(const VecType& point,
                     typename boost::enable_if<IsVector<VecType> >* = 0) const;

  /**
   * Computes maximum distance.
   *
   * @param other Bound to which the maximum distance is requested.
   */
  double MaxDistance(const HRectBound& other) const;

  /**
   * Calculates minimum and maximum bound-to-bound distance.
   *
   * @param other Bound to which the minimum and maximum distances are
   *     requested.
   */
  math::Range RangeDistance(const HRectBound& other) const;

  /**
   * Calculates minimum and maximum bound-to-point distance.
   *
   * @param point Point to which the minimum and maximum distances are
   *     requested.
   */
  template<typename VecType>
  math::Range RangeDistance(const VecType& point,
                            typename boost::enable_if<IsVector<VecType> >* = 0)
      const;

  /**
   * Expands this region to include new points.
   *
   * @tparam MatType Type of matrix; could be Mat, SpMat, a subview, or just a
   *   vector.
   * @param data Data points to expand this region to include.
   */
  template<typename MatType>
  HRectBound& operator|=(const MatType& data);

  /**
   * Expands this region to encompass another bound.
   */
  HRectBound& operator|=(const HRectBound& other);

  /**
   * Determines if a point is within this bound.
   */
  template<typename VecType>
  bool Contains(const VecType& point) const;

  /**
   * Returns the diameter of the hyperrectangle (that is, the longest diagonal).
   */
  double Diameter() const;

  /**
   * Returns a string representation of this object.
   */
  std::string ToString() const;

  /**
   * Return the metric associated with this bound.  Because it is an LMetric, it
   * cannot store state, so we can make it on the fly.  It is also static
   * because the metric is only dependent on the template arguments.
   */
  static MetricType Metric() { return metric::LMetric<Power, TakeRoot>(); }

 private:
  //! The dimensionality of the bound.
  size_t dim;
  //! The bounds for each dimension.
  math::Range* bounds;
  //! Cached minimum width of bound.
  double minWidth;
};

}; // namespace bound
}; // namespace mlpack

#include "hrectbound_impl.hpp"

#endif // __MLPACK_CORE_TREE_HRECTBOUND_HPP
