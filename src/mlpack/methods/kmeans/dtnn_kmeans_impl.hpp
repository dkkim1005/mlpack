/**
 * @file dtnn_kmeans_impl.hpp
 * @author Ryan Curtin
 *
 * An implementation of a Lloyd iteration which uses dual-tree nearest neighbor
 * search as a black box.  The conditions under which this will perform best are
 * probably limited to the case where k is close to the number of points in the
 * dataset, and the number of iterations of the k-means algorithm will be few.
 */
#ifndef __MLPACK_METHODS_KMEANS_DTNN_KMEANS_IMPL_HPP
#define __MLPACK_METHODS_KMEANS_DTNN_KMEANS_IMPL_HPP

// In case it hasn't been included yet.
#include "dtnn_kmeans.hpp"

#include "dtnn_rules.hpp"

namespace mlpack {
namespace kmeans {

//! Call the tree constructor that does mapping.
template<typename TreeType>
TreeType* BuildTree(
    typename TreeType::Mat& dataset,
    std::vector<size_t>& oldFromNew,
    typename boost::enable_if_c<
        tree::TreeTraits<TreeType>::RearrangesDataset == true, TreeType*
    >::type = 0)
{
  // This is a hack.  I know this will be BinarySpaceTree, so force a leaf size
  // of two.
  return new TreeType(dataset, oldFromNew, 1);
}

//! Call the tree constructor that does not do mapping.
template<typename TreeType>
TreeType* BuildTree(
    const typename TreeType::Mat& dataset,
    const std::vector<size_t>& /* oldFromNew */,
    const typename boost::enable_if_c<
        tree::TreeTraits<TreeType>::RearrangesDataset == false, TreeType*
    >::type = 0)
{
  return new TreeType(dataset);
}

template<typename MetricType, typename MatType, typename TreeType>
DTNNKMeans<MetricType, MatType, TreeType>::DTNNKMeans(const MatType& dataset,
                                                      MetricType& metric) :
    datasetOrig(dataset),
    dataset(tree::TreeTraits<TreeType>::RearrangesDataset ? datasetCopy :
        datasetOrig),
    metric(metric),
    distanceCalculations(0),
    iteration(0),
    upperBounds(dataset.n_cols),
    lowerBounds(dataset.n_cols),
    prunedPoints(dataset.n_cols, false), // Fill with false.
    assignments(dataset.n_cols),
    visited(dataset.n_cols, false) // Fill with false.
{
  Timer::Start("tree_building");

  // Copy the dataset, if necessary.
  if (tree::TreeTraits<TreeType>::RearrangesDataset)
    datasetCopy = datasetOrig;

  // Now build the tree.  We don't need any mappings.
  tree = new TreeType(const_cast<typename TreeType::Mat&>(this->dataset));

  Timer::Stop("tree_building");

  for (size_t i = 0; i < dataset.n_cols; ++i)
  {
    prunedPoints[i] = false;
    visited[i] = false;
  }
  assignments.fill(size_t(-1));
  upperBounds.fill(DBL_MAX);
  lowerBounds.fill(DBL_MAX);
}

template<typename MetricType, typename MatType, typename TreeType>
DTNNKMeans<MetricType, MatType, TreeType>::~DTNNKMeans()
{
  if (tree)
    delete tree;
}

// Run a single iteration.
template<typename MetricType, typename MatType, typename TreeType>
double DTNNKMeans<MetricType, MatType, TreeType>::Iterate(
    const arma::mat& centroids,
    arma::mat& newCentroids,
    arma::Col<size_t>& counts)
{
  // Build a tree on the centroids.
  arma::mat oldCentroids(centroids); // Slow. :(
  std::vector<size_t> oldFromNewCentroids;
  TreeType* centroidTree = BuildTree<TreeType>(
      const_cast<typename TreeType::Mat&>(centroids), oldFromNewCentroids);

  // Reset information in the tree, if we need to.
  if (iteration > 0)
  {
    Timer::Start("knn");
    // Find the nearest neighbors of each of the clusters.
    neighbor::NeighborSearch<neighbor::NearestNeighborSort, MetricType,
        TreeType> nns(centroidTree, centroids);
    arma::mat interclusterDistancesTemp;
    arma::Mat<size_t> closestClusters; // We don't actually care about these.
    nns.Search(1, closestClusters, interclusterDistancesTemp);
    distanceCalculations += nns.BaseCases() + nns.Scores();

    // We need to do the unmapping ourselves.
    for (size_t i = 0; i < interclusterDistances.n_elem; ++i)
      interclusterDistances[oldFromNewCentroids[i]] =
          interclusterDistancesTemp[i];

    Timer::Stop("knn");

    UpdateTree(*tree, oldCentroids, interclusterDistances);

    for (size_t i = 0; i < dataset.n_cols; ++i)
      visited[i] = false;
  }
  else
  {
    // Not initialized yet.
    clusterDistances.set_size(centroids.n_cols + 1);
    interclusterDistances.set_size(centroids.n_cols);
  }

  // We won't use the AllkNN class here because we have our own set of rules.
  //lastIterationCentroids = oldCentroids;
  typedef DTNNKMeansRules<MetricType, TreeType> RuleType;
  RuleType rules(centroids, dataset, assignments, upperBounds, lowerBounds,
      metric, prunedPoints, oldFromNewCentroids, visited);

  typename TreeType::template BreadthFirstDualTreeTraverser<RuleType>
      traverser(rules);

  Timer::Start("tree_mod");
  CoalesceTree(*tree);
  Timer::Stop("tree_mod");

  // Set the number of pruned centroids in the root to 0.
  tree->Stat().Pruned() = 0;
  traverser.Traverse(*tree, *centroidTree);
  distanceCalculations += rules.BaseCases() + rules.Scores();

  Timer::Start("tree_mod");
  DecoalesceTree(*tree);
  Timer::Stop("tree_mod");

  // Now we need to extract the clusters.
  newCentroids.zeros(centroids.n_rows, centroids.n_cols);
  counts.zeros(centroids.n_cols);
  ExtractCentroids(*tree, newCentroids, counts, oldCentroids);

  // Now, calculate how far the clusters moved, after normalizing them.
  double residual = 0.0;
  clusterDistances[centroids.n_cols] = 0.0;
  for (size_t c = 0; c < centroids.n_cols; ++c)
  {
    // Get the mapping to the old cluster, if necessary.
    const size_t old = (tree::TreeTraits<TreeType>::RearrangesDataset) ?
        oldFromNewCentroids[c] : c;
    if (counts[old] == 0)
    {
      newCentroids.col(old).fill(DBL_MAX);
      clusterDistances[old] = 0;
    }
    else
    {
      newCentroids.col(old) /= counts(old);
      const double movement = metric.Evaluate(centroids.col(c),
          newCentroids.col(old));
      clusterDistances[old] = movement;
      residual += std::pow(movement, 2.0);

      if (movement > clusterDistances[centroids.n_cols])
        clusterDistances[centroids.n_cols] = movement;
    }
  }
  distanceCalculations += centroids.n_cols;

  delete centroidTree;

  ++iteration;

  return std::sqrt(residual);
}

template<typename MetricType, typename MatType, typename TreeType>
void DTNNKMeans<MetricType, MatType, TreeType>::UpdateTree(
    TreeType& node,
    const arma::mat& centroids,
    const arma::vec& interclusterDistances)
{
  const bool prunedLastIteration = node.Stat().StaticPruned();
  node.Stat().StaticPruned() = false;

  // Grab information from the parent, if we can.
  if (node.Parent() != NULL &&
      node.Parent()->Stat().Pruned() == centroids.n_cols)
  {
    node.Stat().UpperBound() = node.Parent()->Stat().UpperBound();
    node.Stat().LowerBound() = node.Parent()->Stat().LowerBound() +
        clusterDistances[centroids.n_cols];
    node.Stat().Pruned() = node.Parent()->Stat().Pruned();
    node.Stat().Owner() = node.Parent()->Stat().Owner();
  }


  // Exhaustive lower bound check. Sigh.
/*  if (!prunedLastIteration)
  {
    for (size_t i = 0; i < node.NumDescendants(); ++i)
    {
      double closest = DBL_MAX;
      double secondClosest = DBL_MAX;
      arma::vec distances(centroids.n_cols);
      for (size_t j = 0; j < centroids.n_cols; ++j)
      {
        const double dist = metric.Evaluate(dataset.col(node.Descendant(i)),
            lastIterationCentroids.col(j));
        distances(j) = dist;

        if (dist < closest)
        {
          secondClosest = closest;
          closest = dist;
        }
        else if (dist < secondClosest)
          secondClosest = dist;
      }

      if (closest - 1e-10 > node.Stat().UpperBound())
      {
        Log::Warn << distances.t();
      Log::Fatal << "Point " << node.Descendant(i) << " in " << node.Point(0) <<
"c" << node.NumDescendants() << " invalidates upper bound " <<
node.Stat().UpperBound() << " with closest cluster distance " << closest <<
".\n";
      }

    if (node.NumChildren() == 0)
    {
      if (secondClosest + 1e-10 < std::min(lowerBounds[node.Descendant(i)],
  node.Stat().LowerBound()))
      {
      Log::Warn << distances.t();
      Log::Fatal << "Point " << node.Descendant(i) << " in " << node.Point(0) <<
"c" << node.NumDescendants() << " invalidates lower bound " <<
std::min(lowerBounds[node.Descendant(i)], node.Stat().LowerBound()) << " (" <<
lowerBounds[node.Descendant(i)] << ", " << node.Stat().LowerBound() << ") with "
      << "second closest cluster distance " << secondClosest << ". cd " <<
closest << "; pruned " << prunedPoints[node.Descendant(i)] << " visited " <<
visited[node.Descendant(i)] << ".\n";
      }
    }
  }
  }*/


  if ((node.Stat().Pruned() == centroids.n_cols) &&
      (node.Stat().Owner() < centroids.n_cols))
  {
    // Adjust bounds.
    node.Stat().UpperBound() += clusterDistances[node.Stat().Owner()];
    node.Stat().LowerBound() -= clusterDistances[centroids.n_cols];
    const double interclusterBound = interclusterDistances[node.Stat().Owner()]
        / 2.0;
    if (interclusterBound > node.Stat().LowerBound())
      node.Stat().LowerBound() = interclusterBound;
    if (node.Stat().UpperBound() < node.Stat().LowerBound())
    {
      node.Stat().StaticPruned() = true;
    }
    else
    {
      // Tighten bound.
      node.Stat().UpperBound() =
          node.MaxDistance(centroids.col(node.Stat().Owner()));
      ++distanceCalculations;
      if (node.Stat().UpperBound() < node.Stat().LowerBound())
      {
        node.Stat().StaticPruned() = true;
      }
    }
  }
  else
  {
    node.Stat().LowerBound() -= clusterDistances[centroids.n_cols];
  }

  bool allPointsPruned = true;
  if (!node.Stat().StaticPruned())
  {
    // Try to prune individual points.
    for (size_t i = 0; i < node.NumPoints(); ++i)
    {
      const size_t index = node.Point(i);
      if (!visited[index] && !prunedPoints[index])
      {
        upperBounds[index] = DBL_MAX; // Reset the bounds.
        lowerBounds[index] = DBL_MAX;
        allPointsPruned = false;
        continue; // We didn't visit it and we don't have valid bounds -- so we
                  // can't prune it.
      }

      if (prunedLastIteration)
      {
        // It was pruned last iteration but not this iteration.
        // Set the bounds correctly.
        upperBounds[index] += node.Stat().StaticUpperBoundMovement();
        lowerBounds[index] -= node.Stat().StaticLowerBoundMovement();
      }

      prunedPoints[index] = false;
      const size_t owner = assignments[index];
      const double lowerBound = std::min(lowerBounds[index] -
          clusterDistances[centroids.n_cols], node.Stat().LowerBound());
      const double pruningLowerBound = std::max(lowerBound,
          interclusterDistances[owner] / 2.0);
      if (upperBounds[index] + clusterDistances[owner] < pruningLowerBound)
      {
        prunedPoints[index] = true;
        upperBounds[index] += clusterDistances[owner];
        lowerBounds[index] = pruningLowerBound;
      }
      else
      {
        // Attempt to tighten the bound.
        upperBounds[index] = metric.Evaluate(dataset.col(index),
                                             centroids.col(owner));
        ++distanceCalculations;
        if (upperBounds[index] < pruningLowerBound)
        {
          prunedPoints[index] = true;
          lowerBounds[index] = pruningLowerBound;
        }
        else
        {
          // Point cannot be pruned.
          upperBounds[index] = DBL_MAX;
          lowerBounds[index] = DBL_MAX;
          allPointsPruned = false;
        }
      }
    }
  }

  // Recurse into children, and if all the children (and all the points) are
  // pruned, then we can mark this as statically pruned.
  bool allChildrenPruned = true;
  for (size_t i = 0; i < node.NumChildren(); ++i)
  {
    UpdateTree(node.Child(i), centroids, interclusterDistances);
    if (!node.Child(i).Stat().StaticPruned())
      allChildrenPruned = false;
  }

  if (node.Stat().StaticPruned() && !allChildrenPruned)
  {
    Log::Warn << node;
    Log::Fatal << "Node is statically pruned but not all its children are!\n";
  }

  // If all of the children and points are pruned, we may mark this node as
  // pruned.
  if (allChildrenPruned && allPointsPruned && !node.Stat().StaticPruned())
  {
    node.Stat().StaticPruned() = true;
    node.Stat().Owner() = centroids.n_cols; // Invalid owner.
    node.Stat().Pruned() = size_t(-1);
  }

  if (!node.Stat().StaticPruned())
  {
    node.Stat().UpperBound() = DBL_MAX;
    node.Stat().LowerBound() = DBL_MAX;
    node.Stat().Pruned() = size_t(-1);
    node.Stat().Owner() = centroids.n_cols;
    node.Stat().StaticPruned() = false;
  }
  else // The node is now pruned.
  {
    if (prunedLastIteration)
    {
      // Track total movement while pruned.
      node.Stat().StaticUpperBoundMovement() +=
          clusterDistances[node.Stat().Owner()];
      node.Stat().StaticLowerBoundMovement() +=
          clusterDistances[centroids.n_cols];
    }
    else
    {
      node.Stat().StaticUpperBoundMovement() =
          clusterDistances[node.Stat().Owner()];
      node.Stat().StaticLowerBoundMovement() =
          clusterDistances[centroids.n_cols];
    }
  }
}

template<typename MetricType, typename MatType, typename TreeType>
void DTNNKMeans<MetricType, MatType, TreeType>::ExtractCentroids(
    TreeType& node,
    arma::mat& newCentroids,
    arma::Col<size_t>& newCounts,
    arma::mat& centroids)
{
  // Does this node own points?
  if ((node.Stat().Pruned() == newCentroids.n_cols) ||
      (node.Stat().StaticPruned() && node.Stat().Owner() < newCentroids.n_cols))
  {
    const size_t owner = node.Stat().Owner();
    newCentroids.col(owner) += node.Stat().Centroid() * node.NumDescendants();
    newCounts[owner] += node.NumDescendants();

    // Perform the sanity check here.
/*
    for (size_t i = 0; i < node.NumDescendants(); ++i)
    {
      const size_t index = node.Descendant(i);
      arma::vec trueDistances(centroids.n_cols);
      for (size_t j = 0; j < centroids.n_cols; ++j)
      {
        const double dist = metric.Evaluate(dataset.col(index),
                                            centroids.col(j));
        trueDistances[j] = dist;
      }

      arma::uword minIndex;
      const double minDist = trueDistances.min(minIndex);
      if (size_t(minIndex) != owner)
      {
        Log::Warn << node;
        Log::Warn << trueDistances.t();
        Log::Fatal << "Point " << index << " of node " << node.Point(0) << "c"
<< node.NumDescendants() << " has true minimum cluster " << minIndex << " with "
      << "distance " << minDist << " but node is pruned with upper bound " <<
node.Stat().UpperBound() << " and owner " << node.Stat().Owner() << ".\n";
      }
    }
*/
  }
  else
  {
    // Check each point held in the node.
    // Only check at leaves.
    if (node.NumChildren() == 0)
    {
      for (size_t i = 0; i < node.NumPoints(); ++i)
      {
        const size_t owner = assignments[node.Point(i)];
        newCentroids.col(owner) += dataset.col(node.Point(i));
        ++newCounts[owner];

/*
        const size_t index = node.Point(i);
        arma::vec trueDistances(centroids.n_cols);
        for (size_t j = 0; j < centroids.n_cols; ++j)
        {
          const double dist = metric.Evaluate(dataset.col(index),
                                              centroids.col(j));
          trueDistances[j] = dist;
        }

        arma::uword minIndex;
        const double minDist = trueDistances.min(minIndex);
        if (size_t(minIndex) != owner)
        {
          Log::Warn << node;
          Log::Warn << trueDistances.t();
          Log::Fatal << "Point " << index << " of node " << node.Point(0) << "c"
  << node.NumDescendants() << " has true minimum cluster " << minIndex << " with "
        << "distance " << minDist << " but was assigned to cluster " <<
assignments[node.Point(i)] << " with ub " << upperBounds[node.Point(i)] <<
" and lb " << lowerBounds[node.Point(i)] << "; pp " <<
(prunedPoints[node.Point(i)] ? "true" : "false") << ", visited " <<
(visited[node.Point(i)] ? "true"
: "false") << ".\n";
        }
*/
      }
    }

    // The node is not entirely owned by a cluster.  Recurse.
    for (size_t i = 0; i < node.NumChildren(); ++i)
      ExtractCentroids(node.Child(i), newCentroids, newCounts, centroids);
  }
}

template<typename MetricType, typename MatType, typename TreeType>
void DTNNKMeans<MetricType, MatType, TreeType>::CoalesceTree(
    TreeType& node,
    const size_t child /* Which child are we? */)
{
  // If one of the two children is pruned, we hide this node.
  // This assumes the BinarySpaceTree.  (bad Ryan! bad!)
  if (node.NumChildren() == 0)
    return; // We can't do anything.

  // If this is the root node, we can't coalesce.
  if (node.Parent() != NULL)
  {
    if (node.Child(0).Stat().StaticPruned() &&
        !node.Child(1).Stat().StaticPruned())
    {
      CoalesceTree(node.Child(1), 1);

      // Link the right child to the parent.
      node.Child(1).Parent() = node.Parent();
      node.Parent()->ChildPtr(child) = node.ChildPtr(1);
    }
    else if (!node.Child(0).Stat().StaticPruned() &&
             node.Child(1).Stat().StaticPruned())
    {
      CoalesceTree(node.Child(0), 0);

      // Link the left child to the parent.
      node.Child(0).Parent() = node.Parent();
      node.Parent()->ChildPtr(child) = node.ChildPtr(0);

    }
    else if (!node.Child(0).Stat().StaticPruned() &&
             !node.Child(1).Stat().StaticPruned())
    {
      // The conditional is probably not necessary.
      CoalesceTree(node.Child(0), 0);
      CoalesceTree(node.Child(1), 1);
    }
  }
  else
  {
    CoalesceTree(node.Child(0), 0);
    CoalesceTree(node.Child(1), 1);
  }
}

template<typename MetricType, typename MatType, typename TreeType>
void DTNNKMeans<MetricType, MatType, TreeType>::DecoalesceTree(TreeType& node)
{
  node.Parent() = (TreeType*) node.Stat().TrueParent();
  node.ChildPtr(0) = (TreeType*) node.Stat().TrueLeft();
  node.ChildPtr(1) = (TreeType*) node.Stat().TrueRight();

  if (node.NumChildren() > 0)
  {
    DecoalesceTree(node.Child(0));
    DecoalesceTree(node.Child(1));
  }
}

} // namespace kmeans
} // namespace mlpack

#endif
