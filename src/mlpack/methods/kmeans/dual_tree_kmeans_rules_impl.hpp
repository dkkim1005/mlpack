/**
 * @file dual_tree_kmeans_rules_impl.hpp
 * @author Ryan Curtin
 *
 * A set of tree traversal rules for dual-tree k-means clustering.
 */
#ifndef __MLPACK_METHODS_KMEANS_DUAL_TREE_KMEANS_RULES_IMPL_HPP
#define __MLPACK_METHODS_KMEANS_DUAL_TREE_KMEANS_RULES_IMPL_HPP

// In case it hasn't been included yet.
#include "dual_tree_kmeans_rules.hpp"

namespace mlpack {
namespace kmeans {

template<typename MetricType, typename TreeType>
DualTreeKMeansRules<MetricType, TreeType>::DualTreeKMeansRules(
    const typename TreeType::Mat& dataset,
    const arma::mat& centroids,
    arma::mat& newCentroids,
    arma::Col<size_t>& counts,
    const std::vector<size_t>& mappings,
    const size_t iteration,
    const arma::vec& clusterDistances,
    arma::vec& distances,
    arma::Col<size_t>& assignments,
    arma::Col<size_t>& visited,
    arma::Col<size_t>& distanceIteration,
    const arma::mat& interclusterDistances,
    MetricType& metric) :
    dataset(dataset),
    centroids(centroids),
    newCentroids(newCentroids),
    counts(counts),
    mappings(mappings),
    iteration(iteration),
    clusterDistances(clusterDistances),
    distances(distances),
    assignments(assignments),
    visited(visited),
    distanceIteration(distanceIteration),
    interclusterDistances(interclusterDistances),
    metric(metric),
    distanceCalculations(0)
{ }

template<typename MetricType, typename TreeType>
inline force_inline double DualTreeKMeansRules<MetricType, TreeType>::BaseCase(
    const size_t queryIndex,
    const size_t referenceIndex)
{
  // Collect the number of clusters that have been pruned during the traversal.
  // The ternary operator may not be necessary.
  const size_t traversalPruned = (traversalInfo.LastReferenceNode() != NULL) ?
      traversalInfo.LastReferenceNode()->Stat().ClustersPruned() : 0;

  // It's possible that the reference node has been pruned before we got to the
  // base case.  In that case, don't do the base case, and just return.
//  if (referenceIndex == 37447)
//    Log::Warn << "Visit " << referenceIndex << ", q" << queryIndex << ".  " <<
//traversalInfo.LastReferenceNode()->Stat().ClustersPruned() +
//visited[referenceIndex] << ".\n";
  if (traversalInfo.LastReferenceNode()->Stat().ClustersPruned() +
      visited[referenceIndex] == centroids.n_cols)
    return 0.0;

  ++distanceCalculations;

  const double distance = metric.Evaluate(centroids.col(queryIndex),
                                          dataset.col(referenceIndex));

  // Iteration change check.
  if (distanceIteration[referenceIndex] < iteration)
  {
    distanceIteration[referenceIndex] = iteration;
    distances[referenceIndex] = distance;
    assignments[referenceIndex] = mappings[queryIndex];
  }
  else if (distance < distances[referenceIndex])
  {
    distances[referenceIndex] = distance;
    assignments[referenceIndex] = mappings[queryIndex];
  }

  ++visited[referenceIndex];

  if (visited[referenceIndex] + traversalPruned == centroids.n_cols)
  {
    newCentroids.col(assignments[referenceIndex]) +=
        dataset.col(referenceIndex);
    ++counts(assignments[referenceIndex]);
//    Log::Warn << "Commit base case " << referenceIndex << ".\n";
  }

  return distance;
}

template<typename MetricType, typename TreeType>
double DualTreeKMeansRules<MetricType, TreeType>::Score(
    const size_t /* queryIndex */,
    TreeType& referenceNode)
{
  // No pruning here, for now.
  return 0.0;
}

template<typename MetricType, typename TreeType>
double DualTreeKMeansRules<MetricType, TreeType>::Score(
    TreeType& queryNode,
    TreeType& referenceNode)
{
//  if (referenceNode.Begin() == 33313 || referenceNode.Begin() == 37121 ||
//  if (referenceNode.Begin() == 37447)
//    Log::Warn << "Visit r" << referenceNode.Begin() << "c" <<
//referenceNode.Count() << ", q" << queryNode.Begin() << "c" << queryNode.Count()
//<< ":\n" << referenceNode.Stat();

  // This won't happen with the root since it is explicitly set to 0.
  if (referenceNode.Stat().ClustersPruned() == size_t(-1))
    referenceNode.Stat().ClustersPruned() =
        referenceNode.Parent()->Stat().ClustersPruned();

  if (referenceNode.Stat().HamerlyPruned())
  {
    // Add to centroids if necessary.
    if (referenceNode.Stat().MinQueryNodeDistance() == DBL_MAX /* hack */)
    {
      newCentroids.col(referenceNode.Stat().Owner()) +=
          referenceNode.NumDescendants() * referenceNode.Stat().Centroid();
      counts(referenceNode.Stat().Owner()) += referenceNode.NumDescendants();
      referenceNode.Stat().MinQueryNodeDistance() = 0.0;
    }
    return DBL_MAX; // No need to go further.
  }

  traversalInfo.LastReferenceNode() = &referenceNode;

  // Calculate distance to node.
  // This costs about the same (in terms of runtime) as a single MinDistance()
  // call, so there only need to add one distance computation.
  const math::Range distances = referenceNode.RangeDistance(&queryNode);
  ++distanceCalculations;

  // Is this closer than the current best query node?
  if (distances.Lo() < referenceNode.Stat().MinQueryNodeDistance())
  {
    // This is the new closest node.
    referenceNode.Stat().SecondMinQueryNodeDistance() =
        referenceNode.Stat().MinQueryNodeDistance();
    referenceNode.Stat().SecondMaxQueryNodeDistance() =
        referenceNode.Stat().MaxQueryNodeDistance();
    referenceNode.Stat().MinQueryNodeDistance() = distances.Lo();
    referenceNode.Stat().MaxQueryNodeDistance() = distances.Hi();
  }
  else if (distances.Lo() < referenceNode.Stat().SecondMinQueryNodeDistance())
  {
    // This is the new second closest node.
    referenceNode.Stat().SecondMinQueryNodeDistance() = distances.Lo();
    referenceNode.Stat().SecondMaxQueryNodeDistance() = distances.Hi();
  }
  else if (distances.Lo() > referenceNode.Stat().SecondMaxQueryNodeDistance())
  {
//    if (referenceNode.Begin() == 37447)
//      Log::Warn << "Pelleg-Moore pruned.\n";
    referenceNode.Stat().ClustersPruned() += queryNode.NumDescendants();

    // Is everything pruned?  Then commit the points.
    if (referenceNode.Stat().ClustersPruned() +
        visited[referenceNode.Descendant(0)] == centroids.n_cols)
    {
//      Log::Warn << "Commit points in r" << referenceNode.Begin() << "c" <<
//referenceNode.Count() << ".\n";
      for (size_t i = 0; i < referenceNode.NumDescendants(); ++i)
      {
        const size_t index = referenceNode.Descendant(i);
        const size_t cluster = assignments[index];
        referenceNode.Stat().Owner() = cluster;
        newCentroids.col(cluster) += dataset.col(index);
        ++counts(cluster);
      }
    }
    return DBL_MAX;
  }

  return distances.Lo(); // No pruning allowed at this time.
}

template<typename MetricType, typename TreeType>
double DualTreeKMeansRules<MetricType, TreeType>::Rescore(
    const size_t /* queryIndex */,
    TreeType& /* referenceNode */,
    const double oldScore) const
{
  return oldScore;
}

template<typename MetricType, typename TreeType>
double DualTreeKMeansRules<MetricType, TreeType>::Rescore(
    TreeType& /* queryNode */,
    TreeType& /* referenceNode */,
    const double oldScore) const
{
  return oldScore;
}

template<typename MetricType, typename TreeType>
bool DualTreeKMeansRules<MetricType, TreeType>::IsDescendantOf(
    const TreeType& potentialParent,
    const TreeType& potentialChild) const
{
  if (potentialChild.Parent() == &potentialParent)
    return true;
  else if (potentialChild.Parent() == NULL)
    return false;
  else
    return IsDescendantOf(potentialParent, *potentialChild.Parent());
}

template<typename MetricType, typename TreeType>
double DualTreeKMeansRules<MetricType, TreeType>::HamerlyTypeScore(
    TreeType& referenceNode)
{
  if (referenceNode.Stat().HamerlyPruned())
  {
//    if (referenceNode.Begin() == 26038)
//      Log::Warn << "Hamerly prune! r" << referenceNode.Begin() << "c" <<
//referenceNode.Count() << ".\n";
    return DBL_MAX;
  }

  return 0.0;
}

template<typename MetricType, typename TreeType>
double DualTreeKMeansRules<MetricType, TreeType>::ElkanTypeScore(
    TreeType& queryNode,
    TreeType& referenceNode)
{
  // We have to calculate the minimum distance between the query node and the
  // reference node's best query node.  First, try to use the cached distance.
  if (queryNode.NumDescendants() > 1)
  {
    const double minQueryDistance = queryNode.Stat().FirstBound();
    const double score = ElkanTypeScore(queryNode, referenceNode,
        minQueryDistance);
    return score;
  }
  else
  {
    const double score = ElkanTypeScore(queryNode, referenceNode,
        interclusterDistances[queryNode.Descendant(0)]);
    return score;
  }
}

template<typename MetricType, typename TreeType>
double DualTreeKMeansRules<MetricType, TreeType>::ElkanTypeScore(
    TreeType& queryNode,
    TreeType& referenceNode,
    const double minQueryDistance) const
{
  // See if we can do an Elkan-type prune on between-centroid distances.

  const double maxDistance = referenceNode.Stat().MaxQueryNodeDistance();
  if (maxDistance == DBL_MAX)
    return minQueryDistance;

  if ((minQueryDistance > 2.0 * maxDistance) &&
      !(IsDescendantOf(*(TreeType*) referenceNode.Stat().ClosestQueryNode(),
          queryNode)) &&
      (&queryNode != (TreeType*) referenceNode.Stat().ClosestQueryNode()))
  {
    if (referenceNode.Begin() == 26038)
      Log::Warn << "Elkan prune r26038c" << referenceNode.Count() << ", q" <<
queryNode.Begin() << "c" << queryNode.Count() << "!\n";
    // Then we can conclude d_max(best(N_r), N_r) <= d_min(N_q, N_r) which
    // means that N_q cannot possibly hold any clusters that own any points in
    // N_r.
    return DBL_MAX;
  }

  return minQueryDistance;
}

template<typename MetricType, typename TreeType>
double DualTreeKMeansRules<MetricType, TreeType>::PellegMooreScore(
    TreeType& /* queryNode */,
    TreeType& referenceNode,
    const double minDistance) const
{
  // If the minimum distance to the node is greater than the bound, then every
  // cluster in the query node cannot possibly be the nearest neighbor of any of
  // the points in the reference node.
//  if (referenceNode.Begin() == 26038)
//      Log::Warn << "Pelleg-Moore prune attempt r26038c" << referenceNode.Count() << ", "
//          << "q" << queryNode.Begin() << "c" << queryNode.Count() << "; "
//          << "minDistance " << minDistance << ", MQND " <<
//referenceNode.Stat().MaxQueryNodeDistance() << ".\n";
  if (minDistance > referenceNode.Stat().MaxQueryNodeDistance())
  {
//    if (referenceNode.Begin() == 26038)
//      Log::Warn << "Attempt successful!\n";
    return DBL_MAX;
  }

  return minDistance;
}

} // namespace kmeans
} // namespace mlpack

#endif
