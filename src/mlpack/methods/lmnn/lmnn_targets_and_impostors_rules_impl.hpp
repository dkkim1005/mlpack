/**
 * @file lmnn_targets_and_impostors_rules_impl.hpp
 * @author Ryan Curtin
 *
 * Implementation of LMNNTargetsAndImpostorsRules.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_METHODS_LMNN_LMNN_TARGETS_AND_IMPOSTORS_RULES_IMPL_HPP
#define MLPACK_METHODS_LMNN_LMNN_TARGETS_AND_IMPOSTORS_RULES_IMPL_HPP

// In case it hasn't been included yet.
#include "lmnn_targets_and_impostors_rules.hpp"

namespace mlpack {
namespace lmnn {

template<typename MetricType, typename TreeType>
LMNNTargetsAndImpostorsRules<MetricType, TreeType>::
LMNNTargetsAndImpostorsRules(
    const typename TreeType::Mat& referenceSet,
    const arma::Row<size_t>& referenceLabels,
    const std::vector<size_t>& refOldFromNew,
    const typename TreeType::Mat& querySet,
    const arma::Row<size_t>& queryLabels,
    const std::vector<size_t>& queryOldFromNew,
    const size_t neighborsK,
    const size_t impostorsK,
    const size_t numClasses,
    MetricType& metric) :
    referenceSet(referenceSet),
    referenceLabels(referenceLabels),
    refOldFromNew(refOldFromNew),
    querySet(querySet),
    queryLabels(queryLabels),
    queryOldFromNew(queryOldFromNew),
    neighborsK(neighborsK),
    impostorsK(impostorsK),
    numClasses(numClasses),
    metric(metric),
    lastQueryIndex(querySet.n_cols),
    lastReferenceIndex(referenceSet.n_cols)
{
  // We must set the traversal info last query and reference node pointers to
  // something that is both invalid (i.e. not a tree node) and not NULL.  We'll
  // use the this pointer.
  traversalInfo.LastQueryNode() = (TreeType*) this;
  traversalInfo.LastReferenceNode() = (TreeType*) this;

  // Let's build the list of candidate neighbors and impostors for each query
  // point.  They will be initialized with neighborsK or impostorsK candidates:
  // (DBL_MAX, size_t() - 1) The list of candidates will be updated when
  // visiting new points with the BaseCase() method.
  const Candidate def = std::make_pair(DBL_MAX, size_t() - 1);

  std::vector<Candidate> vect(neighborsK, def);
  CandidateList pqueue(CandidateCmp(), std::move(vect));
  std::vector<Candidate> impVect(impostorsK, def);
  CandidateList impqueue(CandidateCmp(), std::move(impVect));

  candidateNeighbors.reserve(querySet.n_cols);
  candidateImpostors.reserve(querySet.n_cols);
  for (size_t i = 0; i < querySet.n_cols; ++i)
  {
    candidateNeighbors.push_back(pqueue);
    candidateImpostors.push_back(impqueue);
  }
}

template<typename MetricType, typename TreeType>
void LMNNTargetsAndImpostorsRules<MetricType, TreeType>::GetResults(
    arma::Mat<size_t>& neighbors,
    arma::mat& neighborDistances,
    arma::Mat<size_t>& impostors,
    arma::mat& impostorDistances)
{
  neighbors.set_size(neighborsK, querySet.n_cols);
  neighborDistances.set_size(neighborsK, querySet.n_cols);
  impostors.set_size(impostorsK, querySet.n_cols);
  impostorDistances.set_size(impostorsK, querySet.n_cols);

  // We also perform the reverse mapping here.
  for (size_t i = 0; i < querySet.n_cols; i++)
  {
    // First the true neighbors.
    CandidateList& pqueue = candidateNeighbors[i];
    const size_t queryIndex = queryOldFromNew[i];
    for (size_t j = 1; j <= neighborsK; ++j)
    {
      neighbors(neighborsK - j, queryIndex) =
          refOldFromNew[pqueue.top().second];
      neighborDistances(neighborsK - j, queryIndex) = pqueue.top().first;
      pqueue.pop();
    }

    // Now the impostors.
    CandidateList& impqueue = candidateImpostors[i];
    for (size_t j = 1; j <= impostorsK; ++j)
    {
      impostors(impostorsK - j, queryIndex) =
          refOldFromNew[impqueue.top().second];
      impostorDistances(impostorsK - j, queryIndex) = impqueue.top().first;
      impqueue.pop();
    }
  }
};

template<typename MetricType, typename TreeType>
inline force_inline // Absolutely MUST be inline so optimizations can happen.
double LMNNTargetsAndImpostorsRules<MetricType, TreeType>::BaseCase(
    const size_t queryIndex, const size_t referenceIndex)
{
  // If we have already performed this base case, then do not perform it again.
  if ((lastQueryIndex == queryIndex) && (lastReferenceIndex == referenceIndex))
    return lastBaseCase;

  // Don't compare points against themselves.  We always know when we run this
  // that the reference set and query set will be the same, so if that's ever
  // not the case we must change this...
  if (queryIndex == referenceIndex)
    return 0.0;

  double distance = metric.Evaluate(querySet.col(queryIndex),
                                    referenceSet.col(referenceIndex));

  if (queryLabels[queryIndex] == referenceLabels[referenceIndex])
    InsertNeighbor(queryIndex, referenceIndex, distance);
  else
    InsertImpostor(queryIndex, referenceIndex, distance);

  // Cache this information for the next time BaseCase() is called.
  lastQueryIndex = queryIndex;
  lastReferenceIndex = referenceIndex;
  lastBaseCase = distance;

  return distance;
}

template<typename MetricType, typename TreeType>
inline double LMNNTargetsAndImpostorsRules<MetricType, TreeType>::Score(
    const size_t queryIndex,
    TreeType& referenceNode)
{
  double distance;
  if (tree::TreeTraits<TreeType>::FirstPointIsCentroid)
  {
    // The first point in the tree is the centroid.  So we can then calculate
    // the base case between that and the query point.
    double baseCase = -1.0;
    if (tree::TreeTraits<TreeType>::HasSelfChildren)
    {
      // If the parent node is the same, then we have already calculated the
      // base case.
      if ((referenceNode.Parent() != NULL) &&
          (referenceNode.Point(0) == referenceNode.Parent()->Point(0)))
        baseCase = referenceNode.Parent()->Stat().LastDistance();
      else
        baseCase = BaseCase(queryIndex, referenceNode.Point(0));

      // Save this evaluation.
      referenceNode.Stat().LastDistance() = baseCase;
    }

    distance = std::max(baseCase - referenceNode.FurthestDescendantDistance(),
        0.0);
  }
  else
  {
    distance = neighbor::NearestNeighborSort::BestPointToNodeDistance(
        querySet.col(queryIndex), &referenceNode);
  }

  // Compare against the best k'th neighbor and impostor distances for this
  // query point so far.
  double bestDistance = std::max(candidateNeighbors[queryIndex].top().first,
                                 candidateImpostors[queryIndex].top().first);

  return (distance <= bestDistance) ? distance : DBL_MAX;
}

template<typename MetricType, typename TreeType>
inline double LMNNTargetsAndImpostorsRules<MetricType, TreeType>::Rescore(
    const size_t queryIndex,
    TreeType& /* referenceNode */,
    const double oldScore) const
{
  // If we are already pruning, still prune.
  if (oldScore == DBL_MAX)
    return oldScore;

  // Just check the score again against the distances.
  double bestDistance = std::max(candidateNeighbors[queryIndex].top().first,
                                 candidateImpostors[queryIndex].top().first);

  return (oldScore <= bestDistance) ? oldScore : DBL_MAX;
}

template<typename MetricType, typename TreeType>
inline double LMNNTargetsAndImpostorsRules<MetricType, TreeType>::Score(
    TreeType& queryNode,
    TreeType& referenceNode)
{
  // Update our bound.
  const double bestDistance = CalculateBound(queryNode);

  // Use the traversal info to see if a parent-child or parent-parent prune is
  // possible.  This is a looser bound than we could make, but it might be
  // sufficient.
  const double queryParentDist = queryNode.ParentDistance();
  const double queryDescDist = queryNode.FurthestDescendantDistance();
  const double refParentDist = referenceNode.ParentDistance();
  const double refDescDist = referenceNode.FurthestDescendantDistance();
  const double score = traversalInfo.LastScore();
  double adjustedScore;

  // We want to set adjustedScore to be the distance between the centroid of the
  // last query node and last reference node.  We will do this by adjusting the
  // last score.  In some cases, we can just use the last base case.
  if (tree::TreeTraits<TreeType>::FirstPointIsCentroid)
  {
    adjustedScore = traversalInfo.LastBaseCase();
  }
  else if (score == 0.0) // Nothing we can do here.
  {
    adjustedScore = 0.0;
  }
  else
  {
    // The last score is equal to the distance between the centroids minus the
    // radii of the query and reference bounds along the axis of the line
    // between the two centroids.  In the best case, these radii are the
    // furthest descendant distances, but that is not always true.  It would
    // take too long to calculate the exact radii, so we are forced to use
    // MinimumBoundDistance() as a lower-bound approximation.
    const double lastQueryDescDist =
        traversalInfo.LastQueryNode()->MinimumBoundDistance();
    const double lastRefDescDist =
        traversalInfo.LastReferenceNode()->MinimumBoundDistance();
    adjustedScore = neighbor::NearestNeighborSort::CombineWorst(score,
        lastQueryDescDist);
    adjustedScore = neighbor::NearestNeighborSort::CombineWorst(adjustedScore,
        lastRefDescDist);
  }

  // Assemble an adjusted score.  For nearest neighbor search, this adjusted
  // score is a lower bound on MinDistance(queryNode, referenceNode) that is
  // assembled without actually calculating MinDistance().  For furthest
  // neighbor search, it is an upper bound on
  // MaxDistance(queryNode, referenceNode).  If the traversalInfo isn't usable
  // then the node should not be pruned by this.
  if (traversalInfo.LastQueryNode() == queryNode.Parent())
  {
    const double queryAdjust = queryParentDist + queryDescDist;
    adjustedScore = std::max(adjustedScore - queryAdjust, 0.0);
  }
  else if (traversalInfo.LastQueryNode() == &queryNode)
  {
    adjustedScore = std::max(adjustedScore - queryDescDist, 0.0);
  }
  else
  {
    // The last query node wasn't this query node or its parent.  So we force
    // the adjustedScore to be such that this combination can't be pruned here,
    // because we don't really know anything about it.

    // It would be possible to modify this section to try and make a prune based
    // on the query descendant distance and the distance between the query node
    // and last traversal query node, but this case doesn't actually happen for
    // kd-trees or cover trees.
    adjustedScore = 0.0;
  }

  if (traversalInfo.LastReferenceNode() == referenceNode.Parent())
  {
    const double refAdjust = refParentDist + refDescDist;
    adjustedScore = std::max(adjustedScore - refAdjust, 0.0);
  }
  else if (traversalInfo.LastReferenceNode() == &referenceNode)
  {
    adjustedScore = std::max(adjustedScore - refDescDist, 0.0);
  }
  else
  {
    // The last reference node wasn't this reference node or its parent.  So we
    // force the adjustedScore to be such that this combination can't be pruned
    // here, because we don't really know anything about it.

    // It would be possible to modify this section to try and make a prune based
    // on the reference descendant distance and the distance between the
    // reference node and last traversal reference node, but this case doesn't
    // actually happen for kd-trees or cover trees.
    adjustedScore = 0.0;
  }

  // Can we prune?
  if (!(adjustedScore <= bestDistance))
  {
    if (!(tree::TreeTraits<TreeType>::FirstPointIsCentroid && score == 0.0))
    {
      // There isn't any need to set the traversal information because no
      // descendant combinations will be visited, and those are the only
      // combinations that would depend on the traversal information.
      return DBL_MAX;
    }
  }

  double distance;
  if (tree::TreeTraits<TreeType>::FirstPointIsCentroid)
  {
    // The first point in the node is the centroid, so we can calculate the
    // distance between the two points using BaseCase() and then find the
    // bounds.  This is potentially loose for non-ball bounds.
    double baseCase = -1.0;
    if (tree::TreeTraits<TreeType>::HasSelfChildren &&
       (traversalInfo.LastQueryNode()->Point(0) == queryNode.Point(0)) &&
       (traversalInfo.LastReferenceNode()->Point(0) == referenceNode.Point(0)))
    {
      // We already calculated it.
      baseCase = traversalInfo.LastBaseCase();
    }
    else
    {
      baseCase = BaseCase(queryNode.Point(0), referenceNode.Point(0));
    }

    distance = std::max(baseCase - (queryNode.FurthestDescendantDistance() +
        referenceNode.FurthestDescendantDistance()), 0.0);

    lastQueryIndex = queryNode.Point(0);
    lastReferenceIndex = referenceNode.Point(0);
    lastBaseCase = baseCase;

    traversalInfo.LastBaseCase() = baseCase;
  }
  else
  {
    distance = referenceNode.MinDistance(queryNode);
  }

  if (distance <= bestDistance)
  {
    // Set traversal information.
    traversalInfo.LastQueryNode() = &queryNode;
    traversalInfo.LastReferenceNode() = &referenceNode;
    traversalInfo.LastScore() = distance;

    return distance;
  }
  else
  {
    // There isn't any need to set the traversal information because no
    // descendant combinations will be visited, and those are the only
    // combinations that would depend on the traversal information.
    return DBL_MAX;
  }
}

template<typename MetricType, typename TreeType>
inline double LMNNTargetsAndImpostorsRules<MetricType, TreeType>::Rescore(
    TreeType& queryNode,
    TreeType& /* referenceNode */,
    const double oldScore) const
{
  if (oldScore == DBL_MAX || oldScore == 0.0)
    return oldScore;

  // Update our bound.
  const double bestDistance = CalculateBound(queryNode);

  return (oldScore <= bestDistance) ? oldScore : DBL_MAX;
}

// Calculate the bound for a given query node in its current state and update
// it.
template<typename MetricType, typename TreeType>
inline double LMNNTargetsAndImpostorsRules<MetricType, TreeType>::
CalculateBound(TreeType& queryNode) const
{
  // This is an adapted form of the B_1(N_q) function in the paper
  // ``Tree-Independent Dual-Tree Algorithms'' by Curtin et. al.; the goal is to
  // place a bound on the worst possible distance a point combination could have
  // to improve any of the current neighbor estimates.  If the best possible
  // distance between two nodes is greater than this bound, then the node
  // combination can be pruned (see Score()).

  // This particular form will work best only for depth-first recursions, as it
  // does not consider the B_2(N_q) term from that paper.  This is because it
  // turns out to be complex to adapt that term: we would need to hold one
  // B_2(N_q) for each possible class to properly apply the bound.  Given that
  // extra bookkeeping and complexity, we have chosen here to just use B_1(N_q).
  // Therefore, the cover tree will not be a good choice for these rules.

  // In this particular case, we have to consider both the k'th best impostor
  // and neighbor candidate distances, not just the impostor distances as in
  // LMNNImpostorsRules.

  double worstDistance = 0.0;

  // Loop over points held in the node.
  for (size_t i = 0; i < queryNode.NumPoints(); ++i)
  {
    const double distance = std::max(
        candidateNeighbors[queryNode.Point(i)].top().first,
        candidateImpostors[queryNode.Point(i)].top().first);
    if (worstDistance < distance)
      worstDistance = distance;
  }

  // Loop over children of the node, and use their cached information to
  // assemble bounds.
  for (size_t i = 0; i < queryNode.NumChildren(); ++i)
  {
    const double bound = queryNode.Child(i).Stat().Bound();
    if (worstDistance < bound)
      worstDistance = bound;
  }

  // Now consider the parent bounds.
  if (queryNode.Parent() != NULL)
  {
    // The parent's bound implies that the bound for this node must be at least
    // as good.  Thus, if the parent bound is better, then take it.
    if (queryNode.Parent()->Stat().Bound() <= worstDistance)
      worstDistance = queryNode.Parent()->Stat().Bound();
  }

  // Could the existing bounds be better?
  if (queryNode.Stat().Bound() <= worstDistance)
    worstDistance = queryNode.Stat().Bound();

  // Cache bounds for later.
  queryNode.Stat().Bound() = worstDistance;

  return worstDistance;
}

/**
 * Helper function to insert a point into the list of candidate neighbors.
 *
 * @param queryIndex Index of point whose neighbors we are inserting into.
 * @param neighbor Index of reference point which is being inserted.
 * @param distance Distance from query point to reference point.
 */
template<typename MetricType, typename TreeType>
inline void LMNNTargetsAndImpostorsRules<MetricType, TreeType>::InsertNeighbor(
    const size_t queryIndex,
    const size_t neighbor,
    const double distance)
{
  CandidateList& pqueue = candidateNeighbors[queryIndex];
  Candidate c = std::make_pair(distance, neighbor);

  if (CandidateCmp()(c, pqueue.top()))
  {
    pqueue.pop();
    pqueue.push(c);
  }
}

/**
 * Helper function to insert a point into the list of candidate impostors.
 *
 * @param queryIndex Index of point whose impostors we are inserting into.
 * @param neighbor Index of reference point which is being inserted.
 * @param distance Distance from query point to reference point.
 */
template<typename MetricType, typename TreeType>
inline void LMNNTargetsAndImpostorsRules<MetricType, TreeType>::InsertImpostor(
    const size_t queryIndex,
    const size_t neighbor,
    const double distance)
{
  CandidateList& pqueue = candidateImpostors[queryIndex];
  Candidate c = std::make_pair(distance, neighbor);

  if (CandidateCmp()(c, pqueue.top()))
  {
    pqueue.pop();
    pqueue.push(c);
  }
}

} // namespace lmnn
} // namespace mlpack

#endif // MLPACK_METHODS_LMNN_LMNN_TARGETS_AND_IMPOSTORS_RULES_IMPL_HPP
