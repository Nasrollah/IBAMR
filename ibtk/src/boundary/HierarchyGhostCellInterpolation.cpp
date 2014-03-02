// Filename: HierarchyGhostCellInterpolation.cpp
// Created on 05 Nov 2007 by Boyce Griffith
//
// Copyright (c) 2002-2014, Boyce Griffith
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of New York University nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/////////////////////////////// INCLUDES /////////////////////////////////////

#include <algorithm>
#include <ostream>

#include "SAMRAI/pdat/CellVariable.h"
#include "SAMRAI/hier/CoarsenOperator.h"
#include "SAMRAI/xfer/CoarsenPatchStrategy.h"
#include "SAMRAI/xfer/CoarsenSchedule.h"
#include "HierarchyGhostCellInterpolation.h"
#include "SAMRAI/pdat/NodeVariable.h"
#include "SAMRAI/hier/Patch.h"
#include "SAMRAI/hier/PatchData.h"
#include "SAMRAI/hier/PatchGeometry.h"
#include "SAMRAI/hier/PatchLevel.h"
#include "SAMRAI/hier/RefineOperator.h"
#include "SAMRAI/xfer/RefinePatchStrategy.h"
#include "SAMRAI/xfer/RefineSchedule.h"
#include "SAMRAI/SAMRAI_config.h"
#include "SAMRAI/pdat/SideVariable.h"
#include "SAMRAI/hier/Variable.h"
#include "SAMRAI/hier/VariableDatabase.h"
#include "ibtk/CartCellDoubleCubicCoarsen.h"
#include "ibtk/CartCellDoubleQuadraticCFInterpolation.h"
#include "ibtk/CartCellRobinPhysBdryOp.h"
#include "ibtk/CartExtrapPhysBdryOp.h"
#include "ibtk/CartSideDoubleCubicCoarsen.h"
#include "ibtk/CartSideDoubleQuadraticCFInterpolation.h"
#include "ibtk/CartSideRobinPhysBdryOp.h"
#include "ibtk/CoarseFineBoundaryRefinePatchStrategy.h"
#include "ibtk/RefinePatchStrategySet.h"
#include "ibtk/ibtk_utilities.h"
#include "ibtk/namespaces.h" // IWYU pragma: keep
#include "SAMRAI/tbox/Timer.h"
#include "SAMRAI/tbox/TimerManager.h"
#include "SAMRAI/tbox/Utilities.h"

namespace SAMRAI {
namespace hier {
class IntVector;
}  // namespace hier
namespace solv {
class RobinBcCoefStrategy;
}  // namespace solv
}  // namespace SAMRAI

/////////////////////////////// NAMESPACE ////////////////////////////////////

namespace IBTK
{
/////////////////////////////// STATIC ///////////////////////////////////////

namespace
{
// Timers.
static boost::shared_ptr<Timer> t_initialize_operator_state;
static boost::shared_ptr<Timer> t_reset_transaction_component;
static boost::shared_ptr<Timer> t_reset_transaction_components;
static boost::shared_ptr<Timer> t_reinitialize_operator_state;
static boost::shared_ptr<Timer> t_deallocate_operator_state;
static boost::shared_ptr<Timer> t_fill_data;
static boost::shared_ptr<Timer> t_fill_data_coarsen;
static boost::shared_ptr<Timer> t_fill_data_refine;
static boost::shared_ptr<Timer> t_fill_data_set_physical_bcs;
}

/////////////////////////////// PUBLIC ///////////////////////////////////////

HierarchyGhostCellInterpolation::HierarchyGhostCellInterpolation()
    : d_is_initialized(false),
      d_homogeneous_bc(false),
      d_transaction_comps(),
      d_hierarchy(),
      d_grid_geom(),
      d_coarsest_ln(-1),
      d_finest_ln(-1),
      d_coarsen_alg(),
      d_coarsen_strategy(),
      d_coarsen_scheds(),
      d_refine_alg(),
      d_refine_strategy(),
      d_refine_scheds(),
      d_cf_bdry_ops(),
      d_extrap_bc_ops(),
      d_cc_robin_bc_ops(),
      d_sc_robin_bc_ops()
{
    // Setup Timers.
    IBTK_DO_ONCE(
        t_initialize_operator_state = TimerManager::getManager()->getTimer("IBTK::HierarchyGhostCellInterpolation::initializeOperatorState()");
        t_reset_transaction_component = TimerManager::getManager()->getTimer("IBTK::HierarchyGhostCellInterpolation::resetTransactionComponent()");
        t_reset_transaction_components = TimerManager::getManager()->getTimer("IBTK::HierarchyGhostCellInterpolation::resetTransactionComponents()");
        t_reinitialize_operator_state = TimerManager::getManager()->getTimer("IBTK::HierarchyGhostCellInterpolation::reinitializeOperatorState()");
        t_deallocate_operator_state = TimerManager::getManager()->getTimer("IBTK::HierarchyGhostCellInterpolation::deallocateOperatorState()");
        t_fill_data = TimerManager::getManager()->getTimer("IBTK::HierarchyGhostCellInterpolation::fillData()");
        t_fill_data_coarsen = TimerManager::getManager()->getTimer("IBTK::HierarchyGhostCellInterpolation::fillData()[coarsen]");
        t_fill_data_refine = TimerManager::getManager()->getTimer("IBTK::HierarchyGhostCellInterpolation::fillData()[refine]");
        t_fill_data_set_physical_bcs = TimerManager::getManager()->getTimer("IBTK::HierarchyGhostCellInterpolation::fillData()[set_physical_bcs]");
                 );
    return;
}// HierarchyGhostCellInterpolation

HierarchyGhostCellInterpolation::~HierarchyGhostCellInterpolation()
{
    if (d_is_initialized) deallocateOperatorState();
    return;
}// ~HierarchyGhostCellInterpolation

void
HierarchyGhostCellInterpolation::setHomogeneousBc(
    const bool homogeneous_bc)
{
    d_homogeneous_bc = homogeneous_bc;
    return;
}// setHomogeneousBc

void
HierarchyGhostCellInterpolation::initializeOperatorState(
    const InterpolationTransactionComponent transaction_comp,
    const boost::shared_ptr<PatchHierarchy>& hierarchy,
    const int coarsest_ln,
    const int finest_ln)
{
    IBTK_TIMER_START(t_initialize_operator_state);

    initializeOperatorState(std::vector<InterpolationTransactionComponent>(1,transaction_comp), hierarchy, coarsest_ln, finest_ln);

    IBTK_TIMER_STOP(t_initialize_operator_state);
    return;
}// initializeOperatorState

void
HierarchyGhostCellInterpolation::initializeOperatorState(
    const std::vector<InterpolationTransactionComponent>& transaction_comps,
    const boost::shared_ptr<PatchHierarchy>& hierarchy,
    const int coarsest_ln,
    const int finest_ln)
{
    IBTK_TIMER_START(t_initialize_operator_state);

    // Deallocate the operator state if the operator is already initialized.
    if (d_is_initialized) deallocateOperatorState();

    // Reset the transaction components.
    d_transaction_comps = transaction_comps;

    // Cache hierarchy data.
    d_hierarchy = hierarchy;
    d_grid_geom = BOOST_CAST<CartesianGridGeometry>(d_hierarchy->getGridGeometry());
    TBOX_ASSERT(d_grid_geom);
    d_coarsest_ln = coarsest_ln == -1 ? 0                                   : coarsest_ln;
    d_finest_ln   =   finest_ln == -1 ? d_hierarchy->getFinestLevelNumber() :   finest_ln;
    const Dimension& dim = d_hierarchy->getDim();
    
    // Register the cubic coarsen operators with the grid geometry object.
    IBTK_DO_ONCE(
        d_grid_geom->addCoarsenOperator(typeid(CellVariable<double>).name(), boost::make_shared<CartCellDoubleCubicCoarsen>());
        d_grid_geom->addCoarsenOperator(typeid(SideVariable<double>).name(), boost::make_shared<CartSideDoubleCubicCoarsen>());
                 );

    // Setup cached coarsen algorithms and schedules.
    VariableDatabase* var_db = VariableDatabase::getDatabase();
    bool registered_coarsen_op = false;
    d_coarsen_alg = boost::make_shared<CoarsenAlgorithm>(dim);
    for (unsigned int comp_idx = 0; comp_idx < d_transaction_comps.size(); ++comp_idx)
    {
        const std::string& coarsen_op_name = d_transaction_comps[comp_idx].d_coarsen_op_name;
        if (coarsen_op_name != "NONE")
        {
            const int src_data_idx = d_transaction_comps[comp_idx].d_src_data_idx;
            boost::shared_ptr<Variable> var;
            var_db->mapIndexToVariable(src_data_idx, var);
            TBOX_ASSERT(var);
            boost::shared_ptr<CoarsenOperator> coarsen_op = d_grid_geom->lookupCoarsenOperator(var, coarsen_op_name);
            TBOX_ASSERT(coarsen_op);
            d_coarsen_alg->registerCoarsen(src_data_idx, src_data_idx, coarsen_op);
            registered_coarsen_op = true;
        }
    }

    d_coarsen_strategy.reset();

    d_coarsen_scheds.resize(d_finest_ln+1);
    if (registered_coarsen_op)
    {
        for (int src_ln = std::max(1,d_coarsest_ln); src_ln <= d_finest_ln; ++src_ln)
        {
            boost::shared_ptr<PatchLevel> level = d_hierarchy->getPatchLevel(src_ln);
            boost::shared_ptr<PatchLevel> coarser_level = d_hierarchy->getPatchLevel(src_ln-1);
            d_coarsen_scheds[src_ln] = d_coarsen_alg->createSchedule(coarser_level, level, d_coarsen_strategy.get());
        }
    }

    // Setup cached refine algorithms and schedules.
    d_cf_bdry_ops.resize(d_transaction_comps.size());
    d_extrap_bc_ops.resize(d_transaction_comps.size());
    d_cc_robin_bc_ops.resize(d_transaction_comps.size());
    d_sc_robin_bc_ops.resize(d_transaction_comps.size());
    d_refine_alg = boost::make_shared<RefineAlgorithm>();
    std::vector<boost::shared_ptr<RefinePatchStrategy> > refine_patch_strategies;
    for (unsigned int comp_idx = 0; comp_idx < d_transaction_comps.size(); ++comp_idx)
    {
        const int dst_data_idx = d_transaction_comps[comp_idx].d_dst_data_idx;
        const int src_data_idx = d_transaction_comps[comp_idx].d_src_data_idx;
        boost::shared_ptr<Variable > var;
        var_db->mapIndexToVariable(src_data_idx, var);
        auto cc_var = boost::dynamic_pointer_cast<CellVariable<double> >(var);
        auto nc_var = boost::dynamic_pointer_cast<NodeVariable<double> >(var);
        auto sc_var = boost::dynamic_pointer_cast<SideVariable<double> >(var);
        boost::shared_ptr<RefineOperator> refine_op;
        d_cf_bdry_ops[comp_idx].reset();
        boost::shared_ptr<VariableFillPattern> fill_pattern = d_transaction_comps[comp_idx].d_fill_pattern;
        if (cc_var)
        {
            if (d_transaction_comps[comp_idx].d_refine_op_name != "NONE")
            {
                refine_op = d_grid_geom->lookupRefineOperator(var, d_transaction_comps[comp_idx].d_refine_op_name);
            }
            if (d_transaction_comps[comp_idx].d_use_cf_bdry_interpolation)
            {
                d_cf_bdry_ops[comp_idx] = boost::make_shared<CartCellDoubleQuadraticCFInterpolation>();
                d_cf_bdry_ops[comp_idx]->setPatchDataIndex(dst_data_idx);
                d_cf_bdry_ops[comp_idx]->setPatchHierarchy(d_hierarchy);
                refine_patch_strategies.push_back(d_cf_bdry_ops[comp_idx]);
            }
        }
        else if (nc_var)
        {
            if (d_transaction_comps[comp_idx].d_refine_op_name != "NONE")
            {
                refine_op = d_grid_geom->lookupRefineOperator(var, d_transaction_comps[comp_idx].d_refine_op_name);
            }
            if (d_transaction_comps[comp_idx].d_use_cf_bdry_interpolation)
            {
                TBOX_ERROR("not supported yet.\n");
            }
        }
        else if (sc_var)
        {
            if (d_transaction_comps[comp_idx].d_refine_op_name != "NONE")
            {
                refine_op = d_grid_geom->lookupRefineOperator(var, d_transaction_comps[comp_idx].d_refine_op_name);
            }
            if (d_transaction_comps[comp_idx].d_use_cf_bdry_interpolation)
            {
                d_cf_bdry_ops[comp_idx] = boost::make_shared<CartSideDoubleQuadraticCFInterpolation>();
                d_cf_bdry_ops[comp_idx]->setPatchDataIndex(dst_data_idx);
                d_cf_bdry_ops[comp_idx]->setPatchHierarchy(d_hierarchy);
                refine_patch_strategies.push_back(d_cf_bdry_ops[comp_idx]);
            }
        }
        else
        {
            TBOX_ERROR("HierarchyGhostCellInterpolation::initializeOperatorState():\n"
                       << "  only double-precision cell-, node-, or side-centered data is presently supported." << std::endl);
        }

        d_refine_alg->registerRefine(dst_data_idx, src_data_idx, dst_data_idx, refine_op, fill_pattern);

        const std::string& phys_bdry_extrap_type = d_transaction_comps[comp_idx].d_phys_bdry_extrap_type;
        if (phys_bdry_extrap_type != "NONE")
        {
            d_extrap_bc_ops[comp_idx] = boost::make_shared<CartExtrapPhysBdryOp>(dst_data_idx, phys_bdry_extrap_type);
            refine_patch_strategies.push_back(d_extrap_bc_ops[comp_idx]);
        }

        const std::vector<boost::shared_ptr<RobinBcCoefStrategy> >& robin_bc_coefs = d_transaction_comps[comp_idx].d_robin_bc_coefs;
        bool null_bc_coefs = true;
        for (const auto& bc : robin_bc_coefs)
        {
            if (bc) null_bc_coefs = false;
        }
        if (!null_bc_coefs && cc_var)
        {
            d_cc_robin_bc_ops[comp_idx] = boost::make_shared<CartCellRobinPhysBdryOp>(dst_data_idx, robin_bc_coefs, d_homogeneous_bc);
        }
        if (!null_bc_coefs && sc_var)
        {
            TBOX_ASSERT(robin_bc_coefs.size() == d_hierarchy->getDim().getValue());
            d_sc_robin_bc_ops[comp_idx] = boost::make_shared<CartSideRobinPhysBdryOp>(dst_data_idx, robin_bc_coefs, d_homogeneous_bc);
        }
    }

    d_refine_strategy = boost::make_shared<RefinePatchStrategySet>(begin(refine_patch_strategies), end(refine_patch_strategies), false);

    d_refine_scheds.resize(d_finest_ln+1);
    for (int dst_ln = d_coarsest_ln; dst_ln <= d_finest_ln; ++dst_ln)
    {
        boost::shared_ptr<PatchLevel> level = d_hierarchy->getPatchLevel(dst_ln);
        d_refine_scheds[dst_ln] = d_refine_alg->createSchedule(level, dst_ln-1, d_hierarchy, d_refine_strategy.get());
    }

    // Setup physical BC type.
    setHomogeneousBc(d_homogeneous_bc);

    // Indicate the operator is initialized.
    d_is_initialized = true;

    IBTK_TIMER_STOP(t_initialize_operator_state);
    return;
}// initializeOperatorState

void
HierarchyGhostCellInterpolation::resetTransactionComponent(
    const InterpolationTransactionComponent& transaction_comp)
{
    IBTK_TIMER_START(t_reset_transaction_component);

    TBOX_ASSERT(d_is_initialized);
    if (d_transaction_comps.size() != 1)
    {
        TBOX_ERROR("HierarchyGhostCellInterpolation::resetTransactionComponent():"
                   << "  invalid reset operation.  attempting to change the number of registered interpolation transaction components.\n");
    }
    resetTransactionComponents(std::vector<InterpolationTransactionComponent>(1,transaction_comp));

    IBTK_TIMER_STOP(t_reset_transaction_component);
    return;
}// resetTransactionComponent

void
HierarchyGhostCellInterpolation::resetTransactionComponents(
    const std::vector<InterpolationTransactionComponent>& transaction_comps)
{
    IBTK_TIMER_START(t_reset_transaction_components);

    TBOX_ASSERT(d_is_initialized);
    if (d_transaction_comps.size() != transaction_comps.size())
    {
        TBOX_ERROR("HierarchyGhostCellInterpolation::resetTransactionComponents():"
                   << "  invalid reset operation.  attempting to change the number of registered interpolation transaction components.\n");
    }

    // Reset the transaction components.
    d_transaction_comps = transaction_comps;

    // Reset cached coarsen algorithms and schedules.
    VariableDatabase* var_db = VariableDatabase::getDatabase();
    bool registered_coarsen_op = false;
    const Dimension& dim = d_hierarchy->getDim();
    d_coarsen_alg = boost::make_shared<CoarsenAlgorithm>(dim);
    for (unsigned int comp_idx = 0; comp_idx < d_transaction_comps.size(); ++comp_idx)
    {
        const std::string& coarsen_op_name = d_transaction_comps[comp_idx].d_coarsen_op_name;
        if (coarsen_op_name != "NONE")
        {
            const int src_data_idx = d_transaction_comps[comp_idx].d_src_data_idx;
            boost::shared_ptr<Variable > var;
            var_db->mapIndexToVariable(src_data_idx, var);
            TBOX_ASSERT(var);
            boost::shared_ptr<CoarsenOperator> coarsen_op = d_grid_geom->lookupCoarsenOperator(var, coarsen_op_name);
            TBOX_ASSERT(coarsen_op);
            d_coarsen_alg->registerCoarsen(src_data_idx, src_data_idx, coarsen_op);
            registered_coarsen_op = true;
        }
    }

    if (registered_coarsen_op)
    {
        for (int src_ln = std::max(1,d_coarsest_ln); src_ln <= d_finest_ln; ++src_ln)
        {
            d_coarsen_alg->resetSchedule(d_coarsen_scheds[src_ln]);
        }
    }

    // Reset cached refine algorithms and schedules.
    d_refine_alg = boost::make_shared<RefineAlgorithm>();
    for (unsigned int comp_idx = 0; comp_idx < d_transaction_comps.size(); ++comp_idx)
    {
        const int dst_data_idx = d_transaction_comps[comp_idx].d_dst_data_idx;
        const int src_data_idx = d_transaction_comps[comp_idx].d_src_data_idx;
        boost::shared_ptr<Variable > var;
        var_db->mapIndexToVariable(src_data_idx, var);
        auto cc_var = boost::dynamic_pointer_cast<CellVariable<double> >(var);
        auto nc_var = boost::dynamic_pointer_cast<NodeVariable<double> >(var);
        auto sc_var = boost::dynamic_pointer_cast<SideVariable<double> >(var);
        boost::shared_ptr<RefineOperator> refine_op;
        boost::shared_ptr<VariableFillPattern > fill_pattern = d_transaction_comps[comp_idx].d_fill_pattern;
        if (d_cf_bdry_ops[comp_idx]) d_cf_bdry_ops[comp_idx]->setPatchDataIndex(dst_data_idx);
        if (cc_var)
        {
            if (d_transaction_comps[comp_idx].d_refine_op_name != "NONE")
            {
                refine_op = d_grid_geom->lookupRefineOperator(var, d_transaction_comps[comp_idx].d_refine_op_name);
            }
        }
        else if (nc_var)
        {
            if (d_transaction_comps[comp_idx].d_refine_op_name != "NONE")
            {
                refine_op = d_grid_geom->lookupRefineOperator(var, d_transaction_comps[comp_idx].d_refine_op_name);
            }
        }
        else if (sc_var)
        {
            if (d_transaction_comps[comp_idx].d_refine_op_name != "NONE")
            {
                refine_op = d_grid_geom->lookupRefineOperator(var, d_transaction_comps[comp_idx].d_refine_op_name);
            }
        }
        else
        {
            TBOX_ERROR("HierarchyGhostCellInterpolation::resetTransactionComponents():\n"
                       << "  only double-precision cell-, node-, or side-centered data is presently supported." << std::endl);
        }

        d_refine_alg->registerRefine(dst_data_idx, src_data_idx, dst_data_idx, refine_op, fill_pattern);

        const std::string& phys_bdry_extrap_type = d_transaction_comps[comp_idx].d_phys_bdry_extrap_type;
        if (d_extrap_bc_ops[comp_idx])
        {
            TBOX_ASSERT(phys_bdry_extrap_type != "NONE");
            d_extrap_bc_ops[comp_idx]->setPatchDataIndex(dst_data_idx);
            d_extrap_bc_ops[comp_idx]->setExtrapolationType(phys_bdry_extrap_type);
        }
        else
        {
            TBOX_ASSERT(phys_bdry_extrap_type == "NONE");
        }

        const std::vector<boost::shared_ptr<RobinBcCoefStrategy> >& robin_bc_coefs = d_transaction_comps[comp_idx].d_robin_bc_coefs;
        bool null_bc_coefs = true;
        for (const auto& bc : robin_bc_coefs)
        {
            if (bc) null_bc_coefs = false;
        }
        if (d_cc_robin_bc_ops[comp_idx])
        {
            TBOX_ASSERT(!null_bc_coefs);
            TBOX_ASSERT(cc_var);
            d_cc_robin_bc_ops[comp_idx]->setPhysicalBcCoefs(robin_bc_coefs);
            d_cc_robin_bc_ops[comp_idx]->setPatchDataIndex(dst_data_idx);
        }
        if (d_sc_robin_bc_ops[comp_idx])
        {
            TBOX_ASSERT(!null_bc_coefs);
            TBOX_ASSERT(sc_var);
            TBOX_ASSERT(robin_bc_coefs.size() == d_hierarchy->getDim().getValue());
            d_sc_robin_bc_ops[comp_idx]->setPhysicalBcCoefs(robin_bc_coefs);
            d_sc_robin_bc_ops[comp_idx]->setPatchDataIndex(dst_data_idx);
        }
    }

    for (int dst_ln = d_coarsest_ln; dst_ln <= d_finest_ln; ++dst_ln)
    {
        d_refine_alg->resetSchedule(d_refine_scheds[dst_ln]);
    }

    IBTK_TIMER_STOP(t_reset_transaction_components);
    return;
}// resetTransactionComponents

void
HierarchyGhostCellInterpolation::reinitializeOperatorState(
    const boost::shared_ptr<PatchHierarchy>& hierarchy)
{
    if (!d_is_initialized) return;

    IBTK_TIMER_START(t_reinitialize_operator_state);

    initializeOperatorState(d_transaction_comps, hierarchy);

    IBTK_TIMER_STOP(t_reinitialize_operator_state);
    return;
}// reinitializeOperatorState

void
HierarchyGhostCellInterpolation::deallocateOperatorState()
{
    if (!d_is_initialized) return;

    IBTK_TIMER_START(t_deallocate_operator_state);

    // Clear cached refinement operators.
    d_cf_bdry_ops.clear();
    d_extrap_bc_ops.clear();
    d_cc_robin_bc_ops.clear();
    d_sc_robin_bc_ops.clear();

    // Clear cached communication schedules.
    d_coarsen_alg.reset();
    d_coarsen_strategy.reset();
    d_coarsen_strategy.reset();
    d_coarsen_scheds.clear();

    d_refine_alg.reset();
    d_refine_strategy.reset();
    d_refine_strategy.reset();
    d_refine_scheds.clear();

    // Indicate that the operator is NOT initialized.
    d_is_initialized = false;

    IBTK_TIMER_STOP(t_deallocate_operator_state);
    return;
}// deallocateOperatorState

void
HierarchyGhostCellInterpolation::fillData(
    double fill_time)
{
    IBTK_TIMER_START(t_fill_data);

    TBOX_ASSERT(d_is_initialized);
    
    // Ensure the boundary condition objects are in the correct state.
    for (unsigned int comp_idx = 0; comp_idx < d_transaction_comps.size(); ++comp_idx)
    {
        if (d_cc_robin_bc_ops[comp_idx]) d_cc_robin_bc_ops[comp_idx]->setHomogeneousBc(d_homogeneous_bc);
        if (d_sc_robin_bc_ops[comp_idx]) d_sc_robin_bc_ops[comp_idx]->setHomogeneousBc(d_homogeneous_bc);
    }

    // Synchronize data on the patch hierarchy prior to filling ghost cell
    // values.
    IBTK_TIMER_START(t_fill_data_coarsen);
    for (int src_ln = d_finest_ln; src_ln >= std::max(1,d_coarsest_ln); --src_ln)
    {
        if (d_coarsen_scheds[src_ln]) d_coarsen_scheds[src_ln]->coarsenData();
    }
    IBTK_TIMER_STOP(t_fill_data_coarsen);

    // Perform the initial data fill, using extrapolation to determine ghost
    // cell values at physical boundaries.
    IBTK_TIMER_START(t_fill_data_refine);
    for (int dst_ln = d_coarsest_ln; dst_ln <= d_finest_ln; ++dst_ln)
    {
        if (d_refine_scheds[dst_ln]) d_refine_scheds[dst_ln]->fillData(fill_time);
        boost::shared_ptr<PatchLevel> level = d_hierarchy->getPatchLevel(dst_ln);
        const IntVector& ratio = level->getRatioToCoarserLevel();
        for (const auto& patch : *level)
        {
            for (unsigned int comp_idx = 0; comp_idx < d_transaction_comps.size(); ++comp_idx)
            {
                if (d_cf_bdry_ops[comp_idx])
                {
                    const int dst_data_idx = d_transaction_comps[comp_idx].d_dst_data_idx;
                    const IntVector& ghost_width_to_fill = patch->getPatchData(dst_data_idx)->getGhostCellWidth();
                    d_cf_bdry_ops[comp_idx]->computeNormalExtension(*patch, ratio, ghost_width_to_fill);
                }
            }
        }
    }
    IBTK_TIMER_STOP(t_fill_data_refine);

    // Set Robin boundary conditions at physical boundaries.
    IBTK_TIMER_START(t_fill_data_set_physical_bcs);
    for (int ln = d_coarsest_ln; ln <= d_finest_ln; ++ln)
    {
        boost::shared_ptr<PatchLevel> level = d_hierarchy->getPatchLevel(ln);
        for (const auto& patch : *level)
        {
            if (patch->getPatchGeometry()->getTouchesRegularBoundary())
            {
                for (unsigned int comp_idx = 0; comp_idx < d_transaction_comps.size(); ++comp_idx)
                {
                    if (d_cc_robin_bc_ops[comp_idx])
                    {
                        const int dst_data_idx = d_transaction_comps[comp_idx].d_dst_data_idx;
                        const IntVector& ghost_width_to_fill = patch->getPatchData(dst_data_idx)->getGhostCellWidth();
                        d_cc_robin_bc_ops[comp_idx]->setPhysicalBoundaryConditions(*patch, fill_time, ghost_width_to_fill);
                    }
                    if (d_sc_robin_bc_ops[comp_idx])
                    {
                        const int dst_data_idx = d_transaction_comps[comp_idx].d_dst_data_idx;
                        const IntVector& ghost_width_to_fill = patch->getPatchData(dst_data_idx)->getGhostCellWidth();
                        d_sc_robin_bc_ops[comp_idx]->setPhysicalBoundaryConditions(*patch, fill_time, ghost_width_to_fill);
                    }
                }
            }
        }
    }
    IBTK_TIMER_STOP(t_fill_data_set_physical_bcs);

    IBTK_TIMER_STOP(t_fill_data);
    return;
}// fillData

/////////////////////////////// PROTECTED ////////////////////////////////////

/////////////////////////////// PRIVATE //////////////////////////////////////

/////////////////////////////// NAMESPACE ////////////////////////////////////

}// namespace IBTK

//////////////////////////////////////////////////////////////////////////////
