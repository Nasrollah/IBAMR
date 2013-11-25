// Copyright (c) 2002-2013, Boyce Griffith
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

// Config files
#include <IBAMR_config.h>
#include <IBTK_config.h>
#include <SAMRAI_config.h>

// Headers for basic PETSc functions
#include <petscsys.h>

// Headers for basic SAMRAI objects
#include <BergerRigoutsos.h>
#include <CartesianGridGeometry.h>
#include <LoadBalancer.h>
#include <StandardTagAndInitialize.h>

// Headers for basic libMesh objects
#include <libmesh/boundary_info.h>
#include <libmesh/equation_systems.h>
#include <libmesh/exodusII_io.h>
#include <libmesh/mesh.h>
#include <libmesh/mesh_function.h>
#include <libmesh/mesh_generation.h>

// Headers for application-specific algorithm/data structure objects
#include <boost/multi_array.hpp>
#include <ibamr/IBExplicitHierarchyIntegrator.h>
#include <ibamr/IBFEMethod.h>
#include <ibamr/INSCollocatedHierarchyIntegrator.h>
#include <ibamr/INSStaggeredHierarchyIntegrator.h>
#include <ibamr/app_namespaces.h>
#include <ibtk/AppInitializer.h>
#include <ibtk/libmesh_utilities.h>
#include <ibtk/muParserCartGridFunction.h>
#include <ibtk/muParserRobinBcCoefs.h>

// Elasticity model data.
namespace ModelData
{
static double block_kappa_s = 1.0e6;
static double beam_kappa_s  = 1.0e6;

// Tether (penalty) force function for the solid blocks.
void
block_tether_force_function(
    VectorValue<double>& F,
    const TensorValue<double>& /*FF*/,
    const libMesh::Point& X,
    const libMesh::Point& s,
    Elem* const /*elem*/,
    NumericVector<double>& /*X_vec*/,
    const vector<NumericVector<double>*>& /*system_data*/,
    double /*time*/,
    void* /*ctx*/)
{
    F = block_kappa_s*(s-X);
    return;
}// block_tether_force_function

// Tether (penalty) force function for the thin beam.
void
beam_tether_force_function(
    VectorValue<double>& F,
    const TensorValue<double>& /*FF*/,
    const libMesh::Point& X,
    const libMesh::Point& s,
    Elem* const /*elem*/,
    NumericVector<double>& /*X_vec*/,
    const vector<NumericVector<double>*>& /*system_data*/,
    double /*time*/,
    void* /*ctx*/)
{
    F = beam_kappa_s*(s-X);
    return;
}// beam_tether_force_function

// Stress tensor function for the thin beam.
static double mu_s, beta_s;
void
beam_PK1_stress_function(
    TensorValue<double>& PP,
    const TensorValue<double>& FF,
    const libMesh::Point& /*X*/,
    const libMesh::Point& /*s*/,
    Elem* const /*elem*/,
    NumericVector<double>& /*X_vec*/,
    const vector<NumericVector<double>*>& /*system_data*/,
    double /*time*/,
    void* /*ctx*/)
{
    const TensorValue<double> FF_inv_trans = tensor_inverse_transpose(FF, NDIM);
    PP = mu_s*(FF-FF_inv_trans);
    if (!MathUtilities<double>::equalEps(beta_s, 0.0))
    {
        const TensorValue<double> CC = FF.transpose()*FF;
        PP += beta_s*log(CC.det())*FF_inv_trans;
    }
    return;
}// beam_PK1_stress_function

struct node_x_comp
    : std::binary_function<const libMesh::Node*,const libMesh::Node*,bool>
{
    inline bool
    operator()(
        const libMesh::Node* const a,
        const libMesh::Node* const b)
        {
            return (*a)(0) < (*b)(0);
        }
};

template<class node_set>
double
compute_displaced_area(
    node_set& nodes,
    EquationSystems* equation_systems)
{
    System& X_system = equation_systems->get_system<System>(IBFEMethod::COORDS_SYSTEM_NAME);
    NumericVector<double>* X_vec = X_system.solution.get();
    AutoPtr<NumericVector<Number> > X_serial_vec = NumericVector<Number>::build(X_vec->comm());
    X_serial_vec->init(X_vec->size(), true, SERIAL);
    X_vec->localize(*X_serial_vec);
    DofMap& X_dof_map = X_system.get_dof_map();
    vector<unsigned int> vars(2);
    vars[0] = 0; vars[1] = 1;
    MeshFunction X_fcn(*equation_systems, *X_serial_vec, X_dof_map, vars);
    X_fcn.init();
    DenseVector<double> n0(2), n1(2);
    X_fcn(**nodes.rbegin(), 0.0, n0);
    double A2 = 0.0;
    for (typename node_set::iterator it = nodes.begin(); it != nodes.end(); ++it)
    {
        X_fcn(**it, 0.0, n1);
        A2 += n0(0)*n1(1) - n0(1)*n1(0);
        n0 = n1;
    }
    return 0.5*abs(A2);
}

double
compute_inflow_flux(
    const Pointer<PatchHierarchy<NDIM> > hierarchy,
    const int U_idx,
    const int wgt_sc_idx)
{
    double Q_in = 0.0;
    for (int ln = 0; ln <= hierarchy->getFinestLevelNumber(); ++ln)
    {
        Pointer<PatchLevel<NDIM> > level = hierarchy->getPatchLevel(ln);
        for (PatchLevel<NDIM>::Iterator p(level); p; p++)
        {
            Pointer<Patch<NDIM> > patch = level->getPatch(p());
            Pointer<CartesianPatchGeometry<NDIM> > pgeom = patch->getPatchGeometry();
            if (pgeom->getTouchesRegularBoundary())
            {
                Pointer<SideData<NDIM,double> >      U_data = patch->getPatchData(     U_idx);
                Pointer<SideData<NDIM,double> > wgt_sc_data = patch->getPatchData(wgt_sc_idx);
                const Box<NDIM>& patch_box = patch->getBox();
                const double* const x_lower = pgeom->getXLower();
                const double* const dx = pgeom->getDx();
                double dV = 1.0;
                for (int d = 0; d < NDIM; ++d)
                {
                    dV *= dx[d];
                }
                double X[NDIM];
                static const int axis = NDIM-1;
                static const int side = 0;
                if (pgeom->getTouchesRegularBoundary(axis,side))
                {
                    Vector n;
                    for (int d = 0; d < NDIM; ++d)
                    {
                        n[d] = axis == d ? +1.0 : 0.0;
                    }
                    Box<NDIM> side_box = patch_box;
                    side_box.lower(axis) = patch_box.lower(axis);
                    side_box.upper(axis) = patch_box.lower(axis);
                    for (Box<NDIM>::Iterator b(side_box); b; b++)
                    {
                        const Index<NDIM>& i = b();
                        for (int d = 0; d < NDIM; ++d)
                        {
                            X[d] = x_lower[d] + dx[d]*(double(i(d)-patch_box.lower(d))+(d == axis ? 0.0 : 0.5));
                        }
                        if (X[0] >= 0.5 && X[0] <= 1.5)
                        {
                            const SideIndex<NDIM> i_s(i, axis, SideIndex<NDIM>::Lower);
                            if ((*wgt_sc_data)(i_s) > std::numeric_limits<double>::epsilon())
                            {
                                double dA = n[axis]*dV/dx[axis];
                                Q_in += (*U_data)(i_s)*dA;
                            }
                        }
                    }
                }
            }
        }
    }
    SAMRAI_MPI::sumReduction(&Q_in,1);
    return Q_in;
}
}
using namespace ModelData;

/*******************************************************************************
 * For each run, the input filename and restart information (if needed) must   *
 * be given on the command line.  For non-restarted case, command line is:     *
 *                                                                             *
 *    executable <input file name>                                             *
 *                                                                             *
 * For restarted run, command line is:                                         *
 *                                                                             *
 *    executable <input file name> <restart directory> <restart number>        *
 *                                                                             *
 *******************************************************************************/
int
main(
    int argc,
    char* argv[])
{
    // Initialize libMesh, PETSc, MPI, and SAMRAI.
    LibMeshInit init(argc, argv);
    SAMRAI_MPI::setCommunicator(PETSC_COMM_WORLD);
    SAMRAI_MPI::setCallAbortInSerialInsteadOfExit();
    SAMRAIManager::startup();

    {// cleanup dynamically allocated objects prior to shutdown

        // Parse command line options, set some standard options from the input
        // file, initialize the restart database (if this is a restarted run),
        // and enable file logging.
        Pointer<AppInitializer> app_initializer = new AppInitializer(argc, argv, "IB.log");
        Pointer<Database> input_db = app_initializer->getInputDatabase();

        // Get various standard options set in the input file.
        const bool dump_viz_data = app_initializer->dumpVizData();
        const int viz_dump_interval = app_initializer->getVizDumpInterval();
        const bool uses_visit = dump_viz_data && app_initializer->getVisItDataWriter();
        const bool uses_exodus = dump_viz_data && !app_initializer->getExodusIIFilename().empty();
        const string block1_exodus_filename = app_initializer->getExodusIIFilename("block1");
        const string block2_exodus_filename = app_initializer->getExodusIIFilename("block2");
        const string   beam_exodus_filename = app_initializer->getExodusIIFilename("beam"  );

        const bool dump_restart_data = app_initializer->dumpRestartData();
        const int restart_dump_interval = app_initializer->getRestartDumpInterval();
        const string restart_dump_dirname = app_initializer->getRestartDumpDirectory();

        const bool dump_timer_data = app_initializer->dumpTimerData();
        const int timer_dump_interval = app_initializer->getTimerDumpInterval();

        // Create a simple FE mesh.
        const double dx = input_db->getDouble("DX");
        const double ds_block = input_db->getDouble("BLOCK_MFAC")*dx;
        const double ds_beam  = input_db->getDouble("BEAM_MFAC" )*dx;

        string block_elem_type = input_db->getString("BLOCK_ELEM_TYPE");
        string  beam_elem_type = input_db->getString("BEAM_ELEM_TYPE" );

        Mesh block1_mesh(NDIM);
        MeshTools::Generation::build_square(block1_mesh,
                                            ceil(0.5/ds_block), ceil(0.5/ds_block),
                                            0.0, 0.5,
                                            0.0, 0.5,
                                            Utility::string_to_enum<ElemType>(block_elem_type));
        Mesh block2_mesh(NDIM);
        MeshTools::Generation::build_square(block2_mesh,
                                            ceil(0.5/ds_block), ceil(0.5/ds_block),
                                            1.5, 2.0,
                                            0.0, 0.5,
                                            Utility::string_to_enum<ElemType>(block_elem_type));

        Mesh beam_mesh(NDIM);
        MeshTools::Generation::build_square(beam_mesh,
                                            ceil(1.0/ds_beam), max(2*static_cast<int>(ceil(0.016/ds_beam)/2.0),4),
                                            0.5, 1.5,
                                            0.5-0.008, 0.5+0.008,
                                            Utility::string_to_enum<ElemType>(beam_elem_type));
        block1_mesh.prepare_for_use();
        block2_mesh.prepare_for_use();
        beam_mesh  .prepare_for_use();

        // Make an ordered list of the nodes along the bottom edge of the beam.
        typedef std::set<libMesh::Node*,node_x_comp> node_set;
        node_set centerline_node_set;
        std::set<libMesh::dof_id_type> tethered_node_ids;
        for (MeshBase::node_iterator n_it = beam_mesh.nodes_begin(); n_it != beam_mesh.nodes_end(); ++n_it)
        {
            const libMesh::Node& n = **n_it;
            if (abs(n(1) - 0.5) < 1.0e-8)
            {
                centerline_node_set.insert(*n_it);
                if (abs(n(0) - 0.5) < 1.0e-8 || abs(n(0) - 1.5) < 1.0e-8)
                {
                    tethered_node_ids.insert(n.id());
                }
            }
        }

        vector<Mesh*> meshes(3);
        meshes[0] = &block1_mesh;
        meshes[1] = &block2_mesh;
        meshes[2] = &  beam_mesh;

        mu_s = input_db->getDouble("MU_S");
        beta_s = input_db->getDouble("BETA_S");
        block_kappa_s = input_db->getDouble("BLOCK_KAPPA_S");
        beam_kappa_s  = input_db->getDouble("BEAM_KAPPA_S" );
        double beam_kappa_t = input_db->getDouble("BEAM_KAPPA_T");

        // Create major algorithm and data objects that comprise the
        // application.  These objects are configured from the input database
        // and, if this is a restarted run, from the restart database.
        Pointer<INSHierarchyIntegrator> navier_stokes_integrator = new INSStaggeredHierarchyIntegrator(
            "INSStaggeredHierarchyIntegrator", app_initializer->getComponentDatabase("INSStaggeredHierarchyIntegrator"));
        Pointer<IBFEMethod> ib_method_ops = new IBFEMethod(
            "IBFEMethod", app_initializer->getComponentDatabase("IBFEMethod"), meshes, app_initializer->getComponentDatabase("GriddingAlgorithm")->getInteger("max_levels"));
        Pointer<IBHierarchyIntegrator> time_integrator = new IBExplicitHierarchyIntegrator(
            "IBHierarchyIntegrator", app_initializer->getComponentDatabase("IBHierarchyIntegrator"), ib_method_ops, navier_stokes_integrator);
        Pointer<CartesianGridGeometry<NDIM> > grid_geometry = new CartesianGridGeometry<NDIM>(
            "CartesianGeometry", app_initializer->getComponentDatabase("CartesianGeometry"));
        Pointer<PatchHierarchy<NDIM> > patch_hierarchy = new PatchHierarchy<NDIM>(
            "PatchHierarchy", grid_geometry);
        Pointer<StandardTagAndInitialize<NDIM> > error_detector = new StandardTagAndInitialize<NDIM>(
            "StandardTagAndInitialize", time_integrator, app_initializer->getComponentDatabase("StandardTagAndInitialize"));
        Pointer<BergerRigoutsos<NDIM> > box_generator = new BergerRigoutsos<NDIM>();
        Pointer<LoadBalancer<NDIM> > load_balancer = new LoadBalancer<NDIM>(
            "LoadBalancer", app_initializer->getComponentDatabase("LoadBalancer"));
        Pointer<GriddingAlgorithm<NDIM> > gridding_algorithm = new GriddingAlgorithm<NDIM>(
            "GriddingAlgorithm", app_initializer->getComponentDatabase("GriddingAlgorithm"), error_detector, box_generator, load_balancer);

        // Configure the IBFE solver.
        ib_method_ops->registerLagBodyForceFunction(&block_tether_force_function, std::vector<unsigned int>(), NULL, 0);
        ib_method_ops->registerLagBodyForceFunction(&block_tether_force_function, std::vector<unsigned int>(), NULL, 1);
        ib_method_ops->registerLagBodyForceFunction(& beam_tether_force_function, std::vector<unsigned int>(), NULL, 2);
        ib_method_ops->registerPK1StressTensorFunction(&beam_PK1_stress_function, std::vector<unsigned int>(), NULL, 2);
        ib_method_ops->registerTetheredNodes(tethered_node_ids, beam_kappa_t, 2);
        EquationSystems* block1_equation_systems = ib_method_ops->getFEDataManager(0)->getEquationSystems();
        EquationSystems* block2_equation_systems = ib_method_ops->getFEDataManager(1)->getEquationSystems();
        EquationSystems*   beam_equation_systems = ib_method_ops->getFEDataManager(2)->getEquationSystems();

        // Create Eulerian initial condition specification objects.
        if (input_db->keyExists("VelocityInitialConditions"))
        {
            Pointer<CartGridFunction> u_init = new muParserCartGridFunction(
                "u_init", app_initializer->getComponentDatabase("VelocityInitialConditions"), grid_geometry);
            navier_stokes_integrator->registerVelocityInitialConditions(u_init);
        }

        if (input_db->keyExists("PressureInitialConditions"))
        {
            Pointer<CartGridFunction> p_init = new muParserCartGridFunction(
                "p_init", app_initializer->getComponentDatabase("PressureInitialConditions"), grid_geometry);
            navier_stokes_integrator->registerPressureInitialConditions(p_init);
        }

        // Create Eulerian boundary condition specification objects (when necessary).
        const IntVector<NDIM>& periodic_shift = grid_geometry->getPeriodicShift();
        vector<RobinBcCoefStrategy<NDIM>*> u_bc_coefs(NDIM);
        if (periodic_shift.min() > 0)
        {
            for (unsigned int d = 0; d < NDIM; ++d)
            {
                u_bc_coefs[d] = NULL;
            }
        }
        else
        {
            for (unsigned int d = 0; d < NDIM; ++d)
            {
                ostringstream bc_coefs_name_stream;
                bc_coefs_name_stream << "u_bc_coefs_" << d;
                const string bc_coefs_name = bc_coefs_name_stream.str();

                ostringstream bc_coefs_db_name_stream;
                bc_coefs_db_name_stream << "VelocityBcCoefs_" << d;
                const string bc_coefs_db_name = bc_coefs_db_name_stream.str();

                u_bc_coefs[d] = new muParserRobinBcCoefs(
                    bc_coefs_name, app_initializer->getComponentDatabase(bc_coefs_db_name), grid_geometry);
            }
            navier_stokes_integrator->registerPhysicalBoundaryConditions(u_bc_coefs);
        }

        // Create Eulerian body force function specification objects.
        if (input_db->keyExists("ForcingFunction"))
        {
            Pointer<CartGridFunction> f_fcn = new muParserCartGridFunction(
                "f_fcn", app_initializer->getComponentDatabase("ForcingFunction"), grid_geometry);
            time_integrator->registerBodyForceFunction(f_fcn);
        }

        // Set up visualization plot file writers.
        Pointer<VisItDataWriter<NDIM> > visit_data_writer = app_initializer->getVisItDataWriter();
        if (uses_visit)
        {
            time_integrator->registerVisItDataWriter(visit_data_writer);
        }
        AutoPtr<ExodusII_IO> block1_exodus_io(uses_exodus ? new ExodusII_IO(block1_mesh) : NULL);
        AutoPtr<ExodusII_IO> block2_exodus_io(uses_exodus ? new ExodusII_IO(block2_mesh) : NULL);
        AutoPtr<ExodusII_IO>   beam_exodus_io(uses_exodus ? new ExodusII_IO(  beam_mesh) : NULL);

        // Initialize hierarchy configuration and data on all patches.
        ib_method_ops->initializeFEData();
        time_integrator->initializePatchHierarchy(patch_hierarchy, gridding_algorithm);

        // Deallocate initialization objects.
        app_initializer.setNull();

        // Print the input database contents to the log file.
        plog << "Input database:\n";
        input_db->printClassData(plog);

        // Write out initial visualization data.
        int iteration_num = time_integrator->getIntegratorStep();
        double loop_time = time_integrator->getIntegratorTime();
        if (dump_viz_data)
        {
            pout << "\n\nWriting visualization files...\n\n";
            if (uses_visit)
            {
                time_integrator->setupPlotData();
                visit_data_writer->writePlotData(patch_hierarchy, iteration_num, loop_time);
            }
            if (uses_exodus)
            {
                block1_exodus_io->write_timestep(block1_exodus_filename, *block1_equation_systems, iteration_num/viz_dump_interval+1, loop_time);
                block2_exodus_io->write_timestep(block2_exodus_filename, *block2_equation_systems, iteration_num/viz_dump_interval+1, loop_time);
                beam_exodus_io  ->write_timestep(  beam_exodus_filename, *  beam_equation_systems, iteration_num/viz_dump_interval+1, loop_time);
            }
        }

        // Inflow volumes (areas).
        double A_in_current = 0.0;

        // Main time step loop.
        double loop_time_end = time_integrator->getEndTime();
        double dt = 0.0;
        while (!MathUtilities<double>::equalEps(loop_time,loop_time_end) &&
               time_integrator->stepsRemaining())
        {
            iteration_num = time_integrator->getIntegratorStep();
            loop_time = time_integrator->getIntegratorTime();

            pout <<                                                    "\n";
            pout << "+++++++++++++++++++++++++++++++++++++++++++++++++++\n";
            pout << "At beginning of timestep # " <<  iteration_num << "\n";
            pout << "Simulation time is " << loop_time              << "\n";

            dt = time_integrator->getMaximumTimeStepSize();

            VariableDatabase<NDIM>* var_db = VariableDatabase<NDIM>::getDatabase();
            const int U_current_idx = var_db->mapVariableAndContextToIndex(navier_stokes_integrator->getVelocityVariable(), navier_stokes_integrator->getCurrentContext());
            const int wgt_sc_idx = navier_stokes_integrator->getHierarchyMathOps()->getSideWeightPatchDescriptorIndex();
            const double Q_in_current   = compute_inflow_flux(patch_hierarchy, U_current_idx, wgt_sc_idx);
            const double A_disp_current = compute_displaced_area(centerline_node_set, beam_equation_systems);

            time_integrator->advanceHierarchy(dt);
            loop_time += dt;

            const double Q_in_new    = compute_inflow_flux(patch_hierarchy, U_current_idx, wgt_sc_idx);
            const double Q_in_half   = (Q_in_new+Q_in_current)/2.0;
            const double A_in_new    = A_in_current + dt*Q_in_half;
            const double A_in_half   = (A_in_new+A_in_current)/2.0;
            const double A_disp_new  = compute_displaced_area(centerline_node_set, beam_equation_systems);
            const double A_disp_half = (A_disp_new+A_disp_current)/2.0;
            const double Q_disp_half = (A_disp_new-A_disp_current)/dt;

            pout << "t      = " << loop_time - 0.5*dt << "\n"
                 << "A_in   = " << A_in_half   << "\n"
                 << "A_disp = " << A_disp_half << "\n"
                 << "A_diff = " << A_in_half - A_disp_half << "\n"
                 << "Q_in   = " << Q_in_half   << "\n"
                 << "Q_disp = " << Q_disp_half << "\n"
                 << "Q_diff = " << Q_in_half - Q_disp_half << "\n";

            A_in_current = A_in_new;

            pout <<                                                    "\n";
            pout << "At end       of timestep # " <<  iteration_num << "\n";
            pout << "Simulation time is " << loop_time              << "\n";
            pout << "+++++++++++++++++++++++++++++++++++++++++++++++++++\n";
            pout <<                                                    "\n";

            // At specified intervals, write visualization and restart files,
            // print out timer data, and store hierarchy data for post
            // processing.
            iteration_num += 1;
            const bool last_step = !time_integrator->stepsRemaining();
            if (dump_viz_data && (iteration_num%viz_dump_interval == 0 || last_step))
            {
                pout << "\nWriting visualization files...\n\n";
                if (uses_visit)
                {
                    time_integrator->setupPlotData();
                    visit_data_writer->writePlotData(patch_hierarchy, iteration_num, loop_time);
                }
                if (uses_exodus)
                {
                    block1_exodus_io->write_timestep(block1_exodus_filename, *block1_equation_systems, iteration_num/viz_dump_interval+1, loop_time);
                    block2_exodus_io->write_timestep(block2_exodus_filename, *block2_equation_systems, iteration_num/viz_dump_interval+1, loop_time);
                    beam_exodus_io  ->write_timestep(  beam_exodus_filename, *  beam_equation_systems, iteration_num/viz_dump_interval+1, loop_time);
                }
            }
            if (dump_restart_data && (iteration_num%restart_dump_interval == 0 || last_step))
            {
                pout << "\nWriting restart files...\n\n";
                RestartManager::getManager()->writeRestartFile(restart_dump_dirname, iteration_num);
            }
            if (dump_timer_data && (iteration_num%timer_dump_interval == 0 || last_step))
            {
                pout << "\nWriting timer data...\n\n";
                TimerManager::getManager()->print(plog);
            }
        }

        // Cleanup Eulerian boundary condition specification objects (when
        // necessary).
        for (unsigned int d = 0; d < NDIM; ++d) delete u_bc_coefs[d];

    }// cleanup dynamically allocated objects prior to shutdown

    SAMRAIManager::shutdown();
    return 0;
}// main