//===- Schedule.cpp - Calculate an optimized schedule ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass the isl to calculate a schedule that is optimized for parallelism
// and tileablility. The algorithm used in isl is an optimized version of the
// algorithm described in following paper:
//
// U. Bondhugula, A. Hartono, J. Ramanujam, and P. Sadayappan.
// A Practical Automatic Polyhedral Parallelizer and Locality Optimizer.
// In Proceedings of the 2008 ACM SIGPLAN Conference On Programming Language
// Design and Implementation, PLDI ’08, pages 101–113. ACM, 2008.
//===----------------------------------------------------------------------===//

#include "polly/ScheduleOptimizer.h"

#include "polly/Cloog.h"
#include "polly/LinkAllPasses.h"
#include "polly/CodeGeneration.h"
#include "polly/Support/GICHelper.h"
#include "polly/Dependences.h"
#include "polly/ScopInfo.h"

#include "isl/aff.h"
#include "isl/space.h"
#include "isl/map.h"
#include "isl/constraint.h"
#include "isl/schedule.h"
#include "isl/band.h"
#include "isl/options.h"

#define DEBUG_TYPE "polly-opt-isl"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

static const int CONSTANT_BOUND = 20;

using namespace llvm;
using namespace polly;

namespace polly {
  bool DisablePollyTiling;
}
static cl::opt<bool, true>
DisableTiling("polly-no-tiling",
	      cl::desc("Disable tiling in the scheduler"), cl::Hidden,
              cl::location(polly::DisablePollyTiling), cl::init(false));

static cl::opt<std::string>
SimplifyDeps("polly-opt-simplify-deps",
             cl::desc("Dependences should be simplified (yes/no)"),
             cl::Hidden, cl::init("yes"));

static cl::opt<std::string>
FusionStrategy("polly-opt-fusion",
               cl::desc("The fusion strategy to choose (min/max)"),
               cl::Hidden, cl::init("max"));

static cl::opt<std::string>
MaxizeBandDepth("polly-opt-maximize-bands",
                cl::desc("Maxize the band depth (yes/no)"),
                cl::Hidden, cl::init("yes"));

namespace {

  class IslScheduleOptimizer : public ScopPass {

  public:
    static char ID;
    explicit IslScheduleOptimizer() : ScopPass(ID) {}

    virtual bool runOnScop(Scop &S);
    void printScop(llvm::raw_ostream &OS) const;
    void getAnalysisUsage(AnalysisUsage &AU) const;
  };

}

char IslScheduleOptimizer::ID = 0;

static int getSingleMap(__isl_take isl_map *map, void *user) {
  isl_map **singleMap = (isl_map **) user;
  *singleMap = map;

  return 0;
}

static void extendScattering(Scop &S, unsigned NewDimensions) {
  for (Scop::iterator SI = S.begin(), SE = S.end(); SI != SE; ++SI) {
    ScopStmt *Stmt = *SI;

    if (Stmt->isFinalRead())
      continue;

    unsigned OldDimensions = Stmt->getNumScattering();
    isl_space *Space;
    isl_basic_map *ChangeScattering;

    Space = isl_space_alloc(Stmt->getIslCtx(), 0, OldDimensions, NewDimensions);
    ChangeScattering = isl_basic_map_universe(isl_space_copy(Space));
    isl_local_space *LocalSpace = isl_local_space_from_space(Space);

    for (unsigned i = 0; i < OldDimensions; i++) {
      isl_constraint *c = isl_equality_alloc(isl_local_space_copy(LocalSpace));
      isl_constraint_set_coefficient_si(c, isl_dim_in, i, 1);
      isl_constraint_set_coefficient_si(c, isl_dim_out, i, -1);
      ChangeScattering = isl_basic_map_add_constraint(ChangeScattering, c);
    }

    for (unsigned i = OldDimensions; i < NewDimensions; i++) {
      isl_constraint *c = isl_equality_alloc(isl_local_space_copy(LocalSpace));
      isl_constraint_set_coefficient_si(c, isl_dim_out, i, 1);
      ChangeScattering = isl_basic_map_add_constraint(ChangeScattering, c);
    }

    isl_map *ChangeScatteringMap = isl_map_from_basic_map(ChangeScattering);

    ChangeScatteringMap = isl_map_align_params(ChangeScatteringMap,
                                               S.getParamSpace());
    isl_map *NewScattering = isl_map_apply_range(Stmt->getScattering(),
                                                 ChangeScatteringMap);
    Stmt->setScattering(NewScattering);
    isl_local_space_free(LocalSpace);
  }
}

// getTileMap - Create a map that describes a n-dimensonal tiling.
//
// getTileMap creates a map from a n-dimensional scattering space into an
// 2*n-dimensional scattering space. The map describes a rectangular tiling.
//
// Example:
//   scheduleDimensions = 2, parameterDimensions = 1, tileSize = 32
//
//   tileMap := [p0] -> {[s0, s1] -> [t0, t1, s0, s1]:
//                        t0 % 32 = 0 and t0 <= s0 < t0 + 32 and
//                        t1 % 32 = 0 and t1 <= s1 < t1 + 32}
//
//  Before tiling:
//
//  for (i = 0; i < N; i++)
//    for (j = 0; j < M; j++)
//	S(i,j)
//
//  After tiling:
//
//  for (t_i = 0; t_i < N; i+=32)
//    for (t_j = 0; t_j < M; j+=32)
//	for (i = t_i; i < min(t_i + 32, N); i++)  | Unknown that N % 32 = 0
//	  for (j = t_j; j < t_j + 32; j++)        |   Known that M % 32 = 0
//	    S(i,j)
//
static isl_basic_map *getTileMap(isl_ctx *ctx, int scheduleDimensions,
				 isl_space *SpaceModel, int tileSize = 32) {
  // We construct
  //
  // tileMap := [p0] -> {[s0, s1] -> [t0, t1, p0, p1, a0, a1]:
  //	                  s0 = a0 * 32 and s0 = p0 and t0 <= p0 < t0 + 32 and
  //	                  s1 = a1 * 32 and s1 = p1 and t1 <= p1 < t1 + 32}
  //
  // and project out the auxilary dimensions a0 and a1.
  isl_space *Space = isl_space_alloc(ctx, 0, scheduleDimensions,
                                     scheduleDimensions * 3);
  isl_basic_map *tileMap = isl_basic_map_universe(isl_space_copy(Space));

  isl_local_space *LocalSpace = isl_local_space_from_space(Space);

  for (int x = 0; x < scheduleDimensions; x++) {
    int sX = x;
    int tX = x;
    int pX = scheduleDimensions + x;
    int aX = 2 * scheduleDimensions + x;

    isl_constraint *c;

    // sX = aX * tileSize;
    c = isl_equality_alloc(isl_local_space_copy(LocalSpace));
    isl_constraint_set_coefficient_si(c, isl_dim_out, sX, 1);
    isl_constraint_set_coefficient_si(c, isl_dim_out, aX, -tileSize);
    tileMap = isl_basic_map_add_constraint(tileMap, c);

    // pX = sX;
    c = isl_equality_alloc(isl_local_space_copy(LocalSpace));
    isl_constraint_set_coefficient_si(c, isl_dim_out, pX, 1);
    isl_constraint_set_coefficient_si(c, isl_dim_in, sX, -1);
    tileMap = isl_basic_map_add_constraint(tileMap, c);

    // tX <= pX
    c = isl_inequality_alloc(isl_local_space_copy(LocalSpace));
    isl_constraint_set_coefficient_si(c, isl_dim_out, pX, 1);
    isl_constraint_set_coefficient_si(c, isl_dim_out, tX, -1);
    tileMap = isl_basic_map_add_constraint(tileMap, c);

    // pX <= tX + (tileSize - 1)
    c = isl_inequality_alloc(isl_local_space_copy(LocalSpace));
    isl_constraint_set_coefficient_si(c, isl_dim_out, tX, 1);
    isl_constraint_set_coefficient_si(c, isl_dim_out, pX, -1);
    isl_constraint_set_constant_si(c, tileSize - 1);
    tileMap = isl_basic_map_add_constraint(tileMap, c);
  }

  // Project out auxilary dimensions.
  //
  // The auxilary dimensions are transformed into existentially quantified ones.
  // This reduces the number of visible scattering dimensions and allows Cloog
  // to produces better code.
  tileMap = isl_basic_map_project_out(tileMap, isl_dim_out,
				      2 * scheduleDimensions,
				      scheduleDimensions);
  isl_local_space_free(LocalSpace);
  return tileMap;
}

// getScheduleForBand - Get the schedule for this band.
//
// Polly applies transformations like tiling on top of the isl calculated value.
// This can influence the number of scheduling dimension. The number of
// schedule dimensions is returned in the parameter 'Dimension'.
isl_union_map *getScheduleForBand(isl_band *Band, int *Dimensions) {
  isl_union_map *PartialSchedule;
  isl_ctx *ctx;
  isl_space *Space;
  isl_basic_map *TileMap;
  isl_union_map *TileUMap;

  PartialSchedule = isl_band_get_partial_schedule(Band);
  *Dimensions = isl_band_n_member(Band);

  if (DisableTiling)
    return PartialSchedule;

  // It does not make any sense to tile a band with just one dimension.
  if (*Dimensions == 1)
    return PartialSchedule;

  ctx = isl_union_map_get_ctx(PartialSchedule);
  Space = isl_union_map_get_space(PartialSchedule);

  TileMap = getTileMap(ctx, *Dimensions, Space);
  TileUMap = isl_union_map_from_map(isl_map_from_basic_map(TileMap));
  TileUMap = isl_union_map_align_params(TileUMap, Space);
  *Dimensions = 2 * *Dimensions;

  return isl_union_map_apply_range(PartialSchedule, TileUMap);
}

// Create a map that pre-vectorizes one scheduling dimension.
//
// getPrevectorMap creates a map that maps each input dimension to the same
// output dimension, except for the dimension DimToVectorize. DimToVectorize is
// strip mined by 'VectorWidth' and the newly created point loop of
// DimToVectorize is moved to the innermost level.
//
// Example (DimToVectorize=0, ScheduleDimensions=2, VectorWidth=4):
//
// | Before transformation
// |
// | A[i,j] -> [i,j]
// |
// | for (i = 0; i < 128; i++)
// |    for (j = 0; j < 128; j++)
// |      A(i,j);
//
//   Prevector map:
//   [i,j] -> [it,j,ip] : it % 4 = 0 and it <= ip <= it + 3 and i = ip
//
// | After transformation:
// |
// | A[i,j] -> [it,j,ip] : it % 4 = 0 and it <= ip <= it + 3 and i = ip
// |
// | for (it = 0; it < 128; it+=4)
// |    for (j = 0; j < 128; j++)
// |      for (ip = max(0,it); ip < min(128, it + 3); ip++)
// |        A(ip,j);
//
// The goal of this transformation is to create a trivially vectorizable loop.
// This means a parallel loop at the innermost level that has a constant number
// of iterations corresponding to the target vector width.
//
// This transformation creates a loop at the innermost level. The loop has a
// constant number of iterations, if the number of loop iterations at
// DimToVectorize can be devided by VectorWidth. The default VectorWidth is
// currently constant and not yet target specific. This function does not reason
// about parallelism.
static isl_map *getPrevectorMap(isl_ctx *ctx, int DimToVectorize,
				int ScheduleDimensions,
				int VectorWidth = 4) {
  isl_space *Space;
  isl_local_space *LocalSpace, *LocalSpaceRange;
  isl_set *Modulo;
  isl_map *TilingMap;
  isl_constraint *c;
  isl_aff *Aff;
  int PointDimension; /* ip */
  int TileDimension;  /* it */
  isl_int VectorWidthMP;

  assert (0 <= DimToVectorize && DimToVectorize < ScheduleDimensions);

  Space = isl_space_alloc(ctx, 0, ScheduleDimensions, ScheduleDimensions + 1);
  TilingMap = isl_map_universe(isl_space_copy(Space));
  LocalSpace = isl_local_space_from_space(Space);
  PointDimension = ScheduleDimensions;
  TileDimension = DimToVectorize;

  // Create an identity map for everything except DimToVectorize and map
  // DimToVectorize to the point loop at the innermost dimension.
  for (int i = 0; i < ScheduleDimensions; i++) {
    c = isl_equality_alloc(isl_local_space_copy(LocalSpace));
    isl_constraint_set_coefficient_si(c, isl_dim_in, i, -1);

    if (i == DimToVectorize)
      isl_constraint_set_coefficient_si(c, isl_dim_out, PointDimension, 1);
    else
      isl_constraint_set_coefficient_si(c, isl_dim_out, i, 1);

    TilingMap = isl_map_add_constraint(TilingMap, c);
  }

  // it % 'VectorWidth' = 0
  LocalSpaceRange = isl_local_space_range(isl_local_space_copy(LocalSpace));
  Aff = isl_aff_zero_on_domain(LocalSpaceRange);
  Aff = isl_aff_set_constant_si(Aff, VectorWidth);
  Aff = isl_aff_set_coefficient_si(Aff, isl_dim_in, TileDimension, 1);
  isl_int_init(VectorWidthMP);
  isl_int_set_si(VectorWidthMP, VectorWidth);
  Aff = isl_aff_mod(Aff, VectorWidthMP);
  isl_int_clear(VectorWidthMP);
  Modulo = isl_pw_aff_zero_set(isl_pw_aff_from_aff(Aff));
  TilingMap = isl_map_intersect_range(TilingMap, Modulo);

  // it <= ip
  c = isl_inequality_alloc(isl_local_space_copy(LocalSpace));
  isl_constraint_set_coefficient_si(c, isl_dim_out, TileDimension, -1);
  isl_constraint_set_coefficient_si(c, isl_dim_out, PointDimension, 1);
  TilingMap = isl_map_add_constraint(TilingMap, c);

  // ip <= it + ('VectorWidth' - 1)
  c = isl_inequality_alloc(LocalSpace);
  isl_constraint_set_coefficient_si(c, isl_dim_out, TileDimension, 1);
  isl_constraint_set_coefficient_si(c, isl_dim_out, PointDimension, -1);
  isl_constraint_set_constant_si(c, VectorWidth - 1);
  TilingMap = isl_map_add_constraint(TilingMap, c);

  isl_map_dump(TilingMap);

  return TilingMap;
}

// getScheduleForBandList - Get the scheduling map for a list of bands.
//
// We walk recursively the forest of bands to combine the schedules of the
// individual bands to the overall schedule. In case tiling is requested,
// the individual bands are tiled.
static isl_union_map *getScheduleForBandList(isl_band_list *BandList) {
  int NumBands;
  isl_union_map *Schedule;
  isl_ctx *ctx;

  ctx = isl_band_list_get_ctx(BandList);
  NumBands = isl_band_list_n_band(BandList);
  Schedule = isl_union_map_empty(isl_space_params_alloc(ctx, 0));

  for (int i = 0; i < NumBands; i++) {
    isl_band *Band;
    isl_union_map *PartialSchedule;
    int ScheduleDimensions;
    isl_space *Space;

    Band = isl_band_list_get_band(BandList, i);
    PartialSchedule = getScheduleForBand(Band, &ScheduleDimensions);
    Space = isl_union_map_get_space(PartialSchedule);

    if (isl_band_has_children(Band)) {
      isl_band_list *Children;
      isl_union_map *SuffixSchedule;

      Children = isl_band_get_children(Band);
      SuffixSchedule = getScheduleForBandList(Children);
      PartialSchedule = isl_union_map_flat_range_product(PartialSchedule,
							 SuffixSchedule);
      isl_band_list_free(Children);
    } else if (EnablePollyVector) {
      for (int i = ScheduleDimensions - 1 ;  i >= 0 ; i--) {
	if (isl_band_member_is_zero_distance(Band, i)) {
          isl_map *TileMap;
          isl_union_map *TileUMap;

	  TileMap = getPrevectorMap(ctx, i, ScheduleDimensions);
	  TileUMap = isl_union_map_from_map(TileMap);
          TileUMap = isl_union_map_align_params(TileUMap,
                                                isl_space_copy(Space));
	  PartialSchedule = isl_union_map_apply_range(PartialSchedule,
						      TileUMap);
	  break;
	}
      }
    }

    Schedule = isl_union_map_union(Schedule, PartialSchedule);

    isl_band_free(Band);
    isl_space_free(Space);
  }

  return Schedule;
}

static isl_union_map *getScheduleMap(isl_schedule *Schedule) {
  isl_band_list *BandList = isl_schedule_get_band_forest(Schedule);
  isl_union_map *ScheduleMap = getScheduleForBandList(BandList);
  isl_band_list_free(BandList);
  return ScheduleMap;
}

bool IslScheduleOptimizer::runOnScop(Scop &S) {
  Dependences *D = &getAnalysis<Dependences>();

  // Build input data.
  int dependencyKinds = Dependences::TYPE_RAW
                          | Dependences::TYPE_WAR
                          | Dependences::TYPE_WAW;

  isl_union_map *dependences = D->getDependences(dependencyKinds);
  isl_union_set *domain = NULL;

  for (Scop::iterator SI = S.begin(), SE = S.end(); SI != SE; ++SI)
    if ((*SI)->isFinalRead())
      continue;
    else if (!domain)
      domain = isl_union_set_from_set((*SI)->getDomain());
    else
      domain = isl_union_set_union(domain,
        isl_union_set_from_set((*SI)->getDomain()));

  if (!domain)
    return false;

  // Simplify the dependences by removing the constraints introduced by the
  // domains. This can speed up the scheduling time significantly, as large
  // constant coefficients will be removed from the dependences. The
  // introduction of some additional dependences reduces the possible
  // transformations, but in most cases, such transformation do not seem to be
  // interesting anyway. In some cases this option may stop the scheduler to
  // find any schedule.
  if (SimplifyDeps == "yes") {
    dependences = isl_union_map_gist_domain(dependences,
                                            isl_union_set_copy(domain));
    dependences = isl_union_map_gist_range(dependences,
                                           isl_union_set_copy(domain));
  } else if (SimplifyDeps != "no") {
    errs() << "warning: Option -polly-opt-simplify-deps should either be 'yes' "
              "or 'no'. Falling back to default: 'yes'\n";
  }

  isl_schedule *schedule;
  isl_union_map *proximity = isl_union_map_copy(dependences);
  isl_union_map *validity = dependences;

  DEBUG(dbgs() << "\n\nCompute schedule from: ");
  DEBUG(dbgs() << "Domain := "; isl_union_set_dump(domain); dbgs() << ";\n");
  DEBUG(dbgs() << "Proximity := "; isl_union_map_dump(proximity);
        dbgs() << ";\n");
  DEBUG(dbgs() << "Validity := "; isl_union_map_dump(validity);
        dbgs() << ";\n");

  int IslFusionStrategy;

  if (FusionStrategy == "max") {
    IslFusionStrategy = ISL_SCHEDULE_FUSE_MAX;
  } else if (FusionStrategy == "min") {
    IslFusionStrategy = ISL_SCHEDULE_FUSE_MIN;
  } else {
    errs() << "warning: Unknown fusion strategy. Falling back to maximal "
              "fusion.\n";
    IslFusionStrategy = ISL_SCHEDULE_FUSE_MAX;
  }

  int IslMaximizeBands;

  if (MaxizeBandDepth == "yes") {
    IslMaximizeBands = 1;
  } else if (MaxizeBandDepth == "no") {
    IslMaximizeBands = 0;
  } else {
    errs() << "warning: Option -polly-opt-maximize-bands should either be 'yes'"
              " or 'no'. Falling back to default: 'yes'\n";
    IslMaximizeBands = 1;
  }

  isl_options_set_schedule_fuse(S.getIslCtx(), IslFusionStrategy);
  isl_options_set_schedule_max_constant_term(S.getIslCtx(), CONSTANT_BOUND);
  isl_options_set_schedule_maximize_band_depth(S.getIslCtx(), IslMaximizeBands);

  isl_options_set_on_error(S.getIslCtx(), ISL_ON_ERROR_CONTINUE);
  schedule  = isl_union_set_compute_schedule(domain, validity, proximity);
  isl_options_set_on_error(S.getIslCtx(), ISL_ON_ERROR_ABORT);

  // In cases the scheduler is not able to optimize the code, we just do not
  // touch the schedule.
  if (!schedule)
    return false;

  DEBUG(dbgs() << "Computed schedule: ");
  DEBUG(dbgs() << stringFromIslObj(schedule));
  DEBUG(dbgs() << "Individual bands: ");

  isl_union_map *ScheduleMap = getScheduleMap(schedule);

  for (Scop::iterator SI = S.begin(), SE = S.end(); SI != SE; ++SI) {
    ScopStmt *stmt = *SI;

    if (stmt->isFinalRead())
      continue;

    isl_set *domain = stmt->getDomain();
    isl_union_map *stmtBand;
    stmtBand = isl_union_map_intersect_domain(isl_union_map_copy(ScheduleMap),
					      isl_union_set_from_set(domain));
    isl_map *stmtSchedule;
    isl_union_map_foreach_map(stmtBand, getSingleMap, &stmtSchedule);
    stmt->setScattering(stmtSchedule);
    isl_union_map_free(stmtBand);
  }

  isl_union_map_free(ScheduleMap);
  isl_schedule_free(schedule);

  unsigned maxScatDims = 0;

  for (Scop::iterator SI = S.begin(), SE = S.end(); SI != SE; ++SI)
    maxScatDims = std::max((*SI)->getNumScattering(), maxScatDims);

  extendScattering(S, maxScatDims);
  return false;
}

void IslScheduleOptimizer::printScop(raw_ostream &OS) const {
}

void IslScheduleOptimizer::getAnalysisUsage(AnalysisUsage &AU) const {
  ScopPass::getAnalysisUsage(AU);
  AU.addRequired<Dependences>();
}

INITIALIZE_PASS_BEGIN(IslScheduleOptimizer, "polly-opt-isl",
                      "Polly - Optimize schedule of SCoP", false, false)
INITIALIZE_PASS_DEPENDENCY(Dependences)
INITIALIZE_PASS_DEPENDENCY(ScopInfo)
INITIALIZE_PASS_END(IslScheduleOptimizer, "polly-opt-isl",
                      "Polly - Optimize schedule of SCoP", false, false)

Pass* polly::createIslScheduleOptimizerPass() {
  return new IslScheduleOptimizer();
}
