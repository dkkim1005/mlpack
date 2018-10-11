/**
 * @file hmm_viterbi_main.cpp
 * @author Ryan Curtin
 *
 * Compute the most probably hidden state sequence of a given observation
 * sequence for a given HMM.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#include <mlpack/prereqs.hpp>
#include <mlpack/core/util/cli.hpp>
#include <mlpack/core/util/mlpack_main.hpp>

#include "hmm.hpp"
#include "hmm_model.hpp"

#include <mlpack/methods/gmm/gmm.hpp>

using namespace mlpack;
using namespace mlpack::hmm;
using namespace mlpack::distribution;
using namespace mlpack::util;
using namespace mlpack::gmm;
using namespace arma;
using namespace std;

PROGRAM_INFO("Hidden Markov Model (HMM) Viterbi State Prediction",
    "A utility to uses a pre-trained Hidden Markov Model to predict hidden "
    "states given an observation sequence.",
    "This utility takes an already-trained HMM, specified as " +
    PRINT_PARAM_STRING("input_model") + ", and evaluates the most probable "
    "hidden state sequence of a given sequence of observations (specified as "
    "'" + PRINT_PARAM_STRING("input") + ", using the Viterbi algorithm.  The "
    "computed state sequence may be saved using the " +
    PRINT_PARAM_STRING("output") + " output parameter."
    "\n\n"
    "For example, to predict the state sequence of the observations " +
    PRINT_DATASET("obs") + " using the HMM " + PRINT_MODEL("hmm") + ", "
    "storing the predicted state sequence to " + PRINT_DATASET("states") +
    ", the following command could be used:"
    "\n\n" +
    PRINT_CALL("hmm_viterbi", "input", "obs", "input_model", "hmm", "output",
        "states"));

PARAM_MATRIX_IN_REQ("input", "Matrix containing observations,", "i");
PARAM_MODEL_IN_REQ(HMMModel, "input_model", "Trained HMM to use.", "m");
PARAM_UMATRIX_OUT("output", "File to save predicted state sequence to.", "o");

// Because we don't know what the type of our HMM is, we need to write a
// function that can take arbitrary HMM types.
struct Viterbi
{
  template<typename HMMType>
  static void Apply(HMMType& hmm, void* /* extraInfo */)
  {
    // Load observations.
    mat dataSeq = std::move(CLI::GetParam<arma::mat>("input"));

    // See if transposing the data could make it the right dimensionality.
    if ((dataSeq.n_cols == 1) && (hmm.Emission()[0].Dimensionality() == 1))
    {
      Log::Info << "Data sequence appears to be transposed; correcting."
          << endl;
      dataSeq = dataSeq.t();
    }

    // Verify correct dimensionality.
    if (dataSeq.n_rows != hmm.Emission()[0].Dimensionality())
    {
      Log::Fatal << "Observation dimensionality (" << dataSeq.n_rows << ") "
          << "does not match HMM Gaussian dimensionality ("
          << hmm.Emission()[0].Dimensionality() << ")!" << endl;
    }

    arma::Row<size_t> sequence;
    hmm.Predict(dataSeq, sequence);

    // Save output.
    CLI::GetParam<arma::Mat<size_t>>("output") = std::move(sequence);
  }
};

static void mlpackMain()
{
  RequireAtLeastOnePassed({ "output" }, false, "no results will be saved");

  CLI::GetParam<HMMModel*>("input_model")->PerformAction<Viterbi>((void*) NULL);
}
