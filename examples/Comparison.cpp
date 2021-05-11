/*
Example file to perform robust optimization on g2o files
author: Yun Chang
*/

#include <stdlib.h>
#include <memory>
#include <string>
#include <limits>

#include <gtsam/geometry/Pose2.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/slam/dataset.h>

#include "KimeraRPGO/RobustSolver.h"
#include "KimeraRPGO/SolverParams.h"
#include "KimeraRPGO/logger.h"
#include "KimeraRPGO/utils/geometry_utils.h"
#include "KimeraRPGO/utils/type_utils.h"

using namespace KimeraRPGO;

/* Usage: 
  ./RpgoReadG2o {NoPCM,PCM2dOrig,PCM2dSimp} {NoGNC,GNC} <some-2d-g2o-file> {pcm_t_simple_thresh,pcm_odo_thresh} {pcm_R_simple_thresh,pcm_lc_thresh} <gnc_barc_sq> <max_clique_method> <output-g2o-file> <verbosity> 
  [or]   
  ./RpgoReadG2o 3d <some-3d-g2o-file> <pcm_t_simple_thresh> <pcm_R_simple_thresh> <gnc_barc_sq> <max_clique_method> <output-g2o-file> <verbosity>
*/
template <class T>
void Simulate(gtsam::GraphAndValues gv,
              RobustSolverParams params,
              std::string output_folder) {
  gtsam::NonlinearFactorGraph nfg = *gv.first;
  gtsam::Values values = *gv.second;

  std::unique_ptr<RobustSolver> pgo =
      KimeraRPGO::make_unique<RobustSolver>(params);

  size_t dim = getDim<T>();

  Eigen::VectorXd noise = Eigen::VectorXd::Zero(dim);
  static const gtsam::SharedNoiseModel& init_noise =
      gtsam::noiseModel::Diagonal::Sigmas(noise);

  gtsam::Key current_key = nfg[0]->front();

  gtsam::Values init_values;  // add first value with prior factor
  gtsam::NonlinearFactorGraph init_factors;
  init_values.insert(current_key, values.at<T>(current_key));
  gtsam::PriorFactor<T> prior_factor(
      current_key, values.at<T>(current_key), init_noise);
  nfg.add(prior_factor);
  pgo->update(nfg, values);

  pgo->saveData(output_folder);  // tell pgo to save g2o result
}

int main(int argc, char* argv[]) {
  gtsam::GraphAndValues graphNValues;
  std::string pcm_str = argv[1];
  std::string gnc_str = argv[2];
  std::string g2ofile = argv[3];
  double pcm_t = atof(argv[4]);
  double pcm_R = atof(argv[5]);
  double gnc_barcsq = atof(argv[6]);
  std::string max_clique_method_str = argv[7];

  MaxCliqueMethod max_clique_method;
  
  if (max_clique_method_str == "pmc_exact") {
    max_clique_method = MaxCliqueMethod::PMC_EXACT;
    log<INFO>("Max Clique Solver: PMC (Exact)");
  } else if (max_clique_method_str == "pmc_heu") {
    max_clique_method = MaxCliqueMethod::PMC_HEU;
    log<INFO>("Max Clique Solver: PMC (Heuristic)"); 
  } else if (max_clique_method_str == "clipper") {
    max_clique_method = MaxCliqueMethod::CLIPPER;
    log<INFO>("Max Clique Solver: Clipper");
  } else {
    log<WARNING>("Unsupported Max Clique Method (options are: pmc_exct, pmc_heu, clipper) ");
    log<INFO>("Max Clique Solver: PMC (Heuristic)");
    max_clique_method = MaxCliqueMethod::PMC_HEU;
  }

  std::string output_folder;
  if (argc > 8) output_folder = argv[8];

  bool verbose = false;
  if (argc > 9) {
    std::string flag = argv[9];
    if (flag == "v") verbose = true;
  }
  RobustSolverParams params;

  params.logOutput(output_folder);

  Verbosity verbosity = Verbosity::VERBOSE;
  if (!verbose) verbosity = Verbosity::QUIET;


  if (pcm_str == "NoPCM") {
    // FIXME - soft deactivation of PCM
    pcm_t = std::numeric_limits<double>::max();
    pcm_R = std::numeric_limits<double>::max();
    params.setPcm2DParams(pcm_t, pcm_R, verbosity);
    params.setMaxCliqueMethod(max_clique_method);
    if (gnc_str == "NoGNC") {
      // none
      log<INFO>("Options: NONE");
      // FIXME - soft deactivation of GNC
      gnc_barcsq = std::numeric_limits<double>::max();
      params.setGncInlierCostThresholds(gnc_barcsq);
    } else if (gnc_str == "GNC") {
      // GNC only
      log<INFO>("Options: GNC");
      params.setGncInlierCostThresholds(gnc_barcsq);
    } else {
      // error
      log<WARNING>("Unsupported GNC option");
    }
  } else if (pcm_str == "PCM2dSimp") {
    params.setPcmSimple2DParams(pcm_t, pcm_R, verbosity);
    params.setMaxCliqueMethod(max_clique_method);
    if (gnc_str == "NoGNC") {
      // PCM Simp only
      log<INFO>("Options: PCM Simple");
      // FIXME - soft deactivation of GNC
      gnc_barcsq = std::numeric_limits<double>::max();
      params.setGncInlierCostThresholds(gnc_barcsq);
    } else if (gnc_str == "GNC") {
      // PCM Simp + GNC only
      log<INFO>("Options: PCM Simple + GNC");
      params.setGncInlierCostThresholds(gnc_barcsq);
    } else {
      // error
      log<WARNING>("Unsupported GNC option");
    }
  } else if (pcm_str == "PCM2dOrig") {
    // FIXME(Kasra): note that in pcm original, pcm_t and pcm_R are
    // used as params for odo and loop closure edges
    params.setPcm2DParams(pcm_t, pcm_R, verbosity);
    params.setMaxCliqueMethod(max_clique_method);
    if (gnc_str == "NoGNC") {
      // PCM Orig only
      log<INFO>("Options: PCM Orig");
      // FIXME - soft deactivation of GNC
      gnc_barcsq = std::numeric_limits<double>::max();
      params.setGncInlierCostThresholds(gnc_barcsq);
    } else if (gnc_str == "GNC") {
      // PCM Orig + GNC only
      log<INFO>("Options: PCM Orig + GNC");
      params.setGncInlierCostThresholds(gnc_barcsq);
    } else {
      // error
      log<WARNING>("Unsupported GNC option");
    }
  } else {
    // error
    log<WARNING>("Unsupported PCM option");
  }

  // currently only 2D
  graphNValues = gtsam::load2D(g2ofile,
                               gtsam::SharedNoiseModel(),
                               0,
                               false,
                               true,
                               gtsam::NoiseFormatG2O);
  Simulate<gtsam::Pose2>(graphNValues, params, output_folder);
}
