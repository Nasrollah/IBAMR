// Filename: DebuggingUtilities.cpp
// Created on 12 Dec 2008 by Boyce Griffith
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

#include <cmath>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <memory>
#include <ostream>

#include "SAMRAI/hier/Box.h"
#include "SAMRAI/pdat/CellData.h"
#include "SAMRAI/pdat/CellGeometry.h"
#include "SAMRAI/pdat/CellIndex.h"
#include "DebuggingUtilities.h"
#include "SAMRAI/pdat/FaceData.h"
#include "SAMRAI/pdat/FaceGeometry.h"
#include "SAMRAI/pdat/FaceIndex.h"
#include "SAMRAI/hier/Index.h"
#include "SAMRAI/hier/IntVector.h"
#include "SAMRAI/pdat/NodeData.h"
#include "SAMRAI/pdat/NodeGeometry.h"
#include "SAMRAI/pdat/NodeIndex.h"
#include "SAMRAI/hier/Patch.h"
#include "SAMRAI/hier/PatchHierarchy.h"
#include "SAMRAI/hier/PatchLevel.h"
#include "SAMRAI/pdat/SideData.h"
#include "SAMRAI/pdat/SideGeometry.h"
#include "SAMRAI/pdat/SideIndex.h"
#include "ibtk/LData.h"
#include "ibtk/LData-inl.h"
#include "ibtk/namespaces.h" // IWYU pragma: keep
#include "SAMRAI/tbox/PIO.h"
#include "SAMRAI/tbox/SAMRAI_MPI.h"
#include "SAMRAI/tbox/Utilities.h"

/////////////////////////////// NAMESPACE ////////////////////////////////////

namespace IBTK
{
/////////////////////////////// STATIC ///////////////////////////////////////

/////////////////////////////// PUBLIC ///////////////////////////////////////

bool
DebuggingUtilities::checkCellDataForNaNs(
    const int patch_data_idx,
    const boost::shared_ptr<PatchHierarchy > hierarchy,
    const bool interior_only,
    const int coarsest_ln_in,
    const int finest_ln_in)
{
    int num_nans = 0;
    const int coarsest_ln = coarsest_ln_in < 0 ? 0 : coarsest_ln_in;
    const int finest_ln = finest_ln_in < 0 ? hierarchy->getFinestLevelNumber() : finest_ln_in;
    for (int ln = coarsest_ln; ln <= finest_ln; ++ln)
    {
        boost::shared_ptr<PatchLevel > level = hierarchy->getPatchLevel(ln);
        for (PatchLevel::Iterator p(level); p; p++)
        {
            const int patch_num = p();
            boost::shared_ptr<Patch > patch = level->getPatch(patch_num);
            boost::shared_ptr<CellData<double> > patch_data = patch->getPatchData(patch_data_idx);
            const Box& data_box = interior_only ? patch_data->getBox() : patch_data->getGhostBox();
            for (Box::Iterator it(data_box); it; it++)
            {
                const Index& i = it();
                for (int d = 0; d < patch_data->getDepth(); ++d)
                {
                    if ((*patch_data)(i,d) != (*patch_data)(i,d) || std::isnan((*patch_data)(i,d)))
                    {
                        ++num_nans;
                        plog << "found NaN!\n"
                             << "level number = " << ln << "\n"
                             << "index = " << i << "\n"
                             << "depth = " << d << "\n"
                             << "data value = " << (*patch_data)(i,d) << std::endl;
                    }
                    if (std::abs((*patch_data)(i,d)) > 1.0e12)
                    {
                        plog << "found large value!\n"
                             << "level number = " << ln << "\n"
                             << "index = " << i << "\n"
                             << "depth = " << d << "\n"
                             << "data value = " << (*patch_data)(i,d) << std::endl;
                    }
                }
            }
        }
    }
    return SAMRAI_MPI::minReduction(num_nans) > 0;
}// checkCellDataForNaNs

bool
DebuggingUtilities::checkFaceDataForNaNs(
    const int patch_data_idx,
    const boost::shared_ptr<PatchHierarchy > hierarchy,
    const bool interior_only,
    const int coarsest_ln_in,
    const int finest_ln_in)
{
    int num_nans = 0;
    const int coarsest_ln = coarsest_ln_in < 0 ? 0 : coarsest_ln_in;
    const int finest_ln = finest_ln_in < 0 ? hierarchy->getFinestLevelNumber() : finest_ln_in;
    for (int ln = coarsest_ln; ln <= finest_ln; ++ln)
    {
        boost::shared_ptr<PatchLevel > level = hierarchy->getPatchLevel(ln);
        for (PatchLevel::Iterator p(level); p; p++)
        {
            const int patch_num = p();
            boost::shared_ptr<Patch > patch = level->getPatch(patch_num);
            boost::shared_ptr<FaceData<double> > patch_data = patch->getPatchData(patch_data_idx);
            const Box& data_box = interior_only ? patch_data->getBox() : patch_data->getGhostBox();
            for (unsigned int axis = 0; axis < NDIM; ++axis)
            {
                for (Box::Iterator it(FaceGeometry::toFaceBox(data_box,axis)); it; it++)
                {
                    const Index& i = it();
                    const FaceIndex i_f(i, axis, FaceIndex::Lower);
                    for (int d = 0; d < patch_data->getDepth(); ++d)
                    {
                        if ((*patch_data)(i_f,d) != (*patch_data)(i_f,d) || std::isnan((*patch_data)(i_f,d)))
                        {
                            ++num_nans;
                            plog << "found NaN!\n"
                                 << "level number = " << ln << "\n"
                                 << "index = " << i << "\n"
                                 << "depth = " << d << "\n"
                                 << "data value = " << (*patch_data)(i_f,d) << std::endl;
                        }
                        if (std::abs((*patch_data)(i_f,d)) > 1.0e12)
                        {
                            plog << "found large value!\n"
                                 << "level number = " << ln << "\n"
                                 << "index = " << i << "\n"
                                 << "depth = " << d << "\n"
                                 << "data value = " << (*patch_data)(i_f,d) << std::endl;
                        }
                    }
                }
            }
        }
    }
    return SAMRAI_MPI::minReduction(num_nans) > 0;
}// checkFaceDataForNaNs

bool
DebuggingUtilities::checkNodeDataForNaNs(
    const int patch_data_idx,
    const boost::shared_ptr<PatchHierarchy > hierarchy,
    const bool interior_only,
    const int coarsest_ln_in,
    const int finest_ln_in)
{
    int num_nans = 0;
    const int coarsest_ln = coarsest_ln_in < 0 ? 0 : coarsest_ln_in;
    const int finest_ln = finest_ln_in < 0 ? hierarchy->getFinestLevelNumber() : finest_ln_in;
    for (int ln = coarsest_ln; ln <= finest_ln; ++ln)
    {
        boost::shared_ptr<PatchLevel > level = hierarchy->getPatchLevel(ln);
        for (PatchLevel::Iterator p(level); p; p++)
        {
            const int patch_num = p();
            boost::shared_ptr<Patch > patch = level->getPatch(patch_num);
            boost::shared_ptr<NodeData<double> > patch_data = patch->getPatchData(patch_data_idx);
            const Box& data_box = interior_only ? patch_data->getBox() : patch_data->getGhostBox();
            for (Box::Iterator it(NodeGeometry::toNodeBox(data_box)); it; it++)
            {
                const Index& i = it();
                const NodeIndex i_n(i,0);
                for (int d = 0; d < patch_data->getDepth(); ++d)
                {
                    if ((*patch_data)(i_n,d) != (*patch_data)(i_n,d) || std::isnan((*patch_data)(i_n,d)))
                    {
                        ++num_nans;
                        plog << "found NaN!\n"
                             << "level number = " << ln << "\n"
                             << "index = " << i << "\n"
                             << "depth = " << d << "\n"
                             << "data value = " << (*patch_data)(i_n,d) << std::endl;
                    }
                    if (std::abs((*patch_data)(i_n,d)) > 1.0e12)
                    {
                        plog << "found large value!\n"
                             << "level number = " << ln << "\n"
                             << "index = " << i << "\n"
                             << "depth = " << d << "\n"
                             << "data value = " << (*patch_data)(i_n,d) << std::endl;
                    }
                }
            }
        }
    }
    return SAMRAI_MPI::minReduction(num_nans) > 0;
}// checkNodeDataForNaNs

bool
DebuggingUtilities::checkSideDataForNaNs(
    const int patch_data_idx,
    const boost::shared_ptr<PatchHierarchy > hierarchy,
    const bool interior_only,
    const int coarsest_ln_in,
    const int finest_ln_in)
{
    int num_nans = 0;
    const int coarsest_ln = coarsest_ln_in < 0 ? 0 : coarsest_ln_in;
    const int finest_ln = finest_ln_in < 0 ? hierarchy->getFinestLevelNumber() : finest_ln_in;
    for (int ln = coarsest_ln; ln <= finest_ln; ++ln)
    {
        boost::shared_ptr<PatchLevel > level = hierarchy->getPatchLevel(ln);
        for (PatchLevel::Iterator p(level); p; p++)
        {
            const int patch_num = p();
            boost::shared_ptr<Patch > patch = level->getPatch(patch_num);
            boost::shared_ptr<SideData<double> > patch_data = patch->getPatchData(patch_data_idx);
            const Box& data_box = interior_only ? patch_data->getBox() : patch_data->getGhostBox();
            for (unsigned int axis = 0; axis < NDIM; ++axis)
            {
                for (Box::Iterator it(SideGeometry::toSideBox(data_box,axis)); it; it++)
                {
                    const Index& i = it();
                    const SideIndex i_s(i, axis, SideIndex::Lower);
                    for (int d = 0; d < patch_data->getDepth(); ++d)
                    {
                        if ((*patch_data)(i_s,d) != (*patch_data)(i_s,d) || std::isnan((*patch_data)(i_s,d)))
                        {
                            ++num_nans;
                            plog << "found NaN!\n"
                                 << "level number = " << ln << "\n"
                                 << "index = " << i << "\n"
                                 << "depth = " << d << "\n"
                                 << "data value = " << (*patch_data)(i_s,d) << std::endl;
                        }
                        if (std::abs((*patch_data)(i_s,d)) > 1.0e12)
                        {
                            plog << "found large value!\n"
                                 << "level number = " << ln << "\n"
                                 << "index = " << i << "\n"
                                 << "depth = " << d << "\n"
                                 << "data value = " << (*patch_data)(i_s,d) << std::endl;
                        }
                    }
                }
            }
        }
    }
    return SAMRAI_MPI::minReduction(num_nans) > 0;
}// checkSideDataForNaNs

void
DebuggingUtilities::saveCellData(
    const int patch_data_idx,
    const boost::shared_ptr<PatchHierarchy > hierarchy,
    const std::string& filename,
    const std::string& dirname)
{
    std::string truncated_dirname = dirname;
    while (truncated_dirname[truncated_dirname.size()-1] == '/')
    {
        truncated_dirname = std::string(truncated_dirname, truncated_dirname.size()-1);
    }
    Utilities::recursiveMkdir(truncated_dirname);

    const int rank = SAMRAI_MPI::getRank();
    const int nodes = SAMRAI_MPI::getNodes();
    for (int n = 0; n < nodes; ++n)
    {
        if (n == rank)
        {
            for (int ln = 0; ln <= hierarchy->getFinestLevelNumber(); ++ln)
            {
                boost::shared_ptr<PatchLevel > level = hierarchy->getPatchLevel(ln);
                for (PatchLevel::Iterator p(level); p; p++)
                {
                    const int patch_num = p();
                    boost::shared_ptr<Patch > patch = level->getPatch(patch_num);
                    const Box& patch_box = patch->getBox();
                    boost::shared_ptr<CellData<double> > data = patch->getPatchData(patch_data_idx);

                    const std::string patch_filename = truncated_dirname + '/' + filename + '_' + Utilities::levelToString(ln) + '_' + Utilities::patchToString(patch_num);
                    std::ofstream of(patch_filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
                    for (unsigned int d = 0; d < NDIM; ++d)
                    {
                        of.write(reinterpret_cast<const char*>(&patch_box.lower()(d)),sizeof(int));
                        of.write(reinterpret_cast<const char*>(&patch_box.upper()(d)),sizeof(int));
                    }
                    const int depth = data->getDepth();
                    of.write(reinterpret_cast<const char*>(&depth),sizeof(int));
                    for (int d = 0; d < depth; ++d)
                    {
                        for (Box::Iterator it(CellGeometry::toCellBox(patch_box)); it; it++)
                        {
                            const CellIndex i(it());
                            of.write(reinterpret_cast<const char*>(&(*data)(i,d)),sizeof(double));
                        }
                    }
                    of.close();
                }
            }
        }
        SAMRAI_MPI::barrier();
    }
    return;
}// saveCellData

void
DebuggingUtilities::saveFaceData(
    const int patch_data_idx,
    const boost::shared_ptr<PatchHierarchy > hierarchy,
    const std::string& filename,
    const std::string& dirname)
{
    std::string truncated_dirname = dirname;
    while (truncated_dirname[truncated_dirname.size()-1] == '/')
    {
        truncated_dirname = std::string(truncated_dirname, truncated_dirname.size()-1);
    }
    Utilities::recursiveMkdir(truncated_dirname);

    const int rank = SAMRAI_MPI::getRank();
    const int nodes = SAMRAI_MPI::getNodes();
    for (int n = 0; n < nodes; ++n)
    {
        if (n == rank)
        {
            for (int ln = 0; ln <= hierarchy->getFinestLevelNumber(); ++ln)
            {
                boost::shared_ptr<PatchLevel > level = hierarchy->getPatchLevel(ln);
                for (PatchLevel::Iterator p(level); p; p++)
                {
                    const int patch_num = p();
                    boost::shared_ptr<Patch > patch = level->getPatch(patch_num);
                    const Box& patch_box = patch->getBox();
                    boost::shared_ptr<FaceData<double> > data = patch->getPatchData(patch_data_idx);

                    const std::string patch_filename = truncated_dirname + '/' + filename + '_' + Utilities::levelToString(ln) + '_' + Utilities::patchToString(patch_num);
                    std::ofstream of(patch_filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
                    for (unsigned int d = 0; d < NDIM; ++d)
                    {
                        of.write(reinterpret_cast<const char*>(&patch_box.lower()(d)),sizeof(int));
                        of.write(reinterpret_cast<const char*>(&patch_box.upper()(d)),sizeof(int));
                    }
                    const int depth = data->getDepth();
                    of.write(reinterpret_cast<const char*>(&depth),sizeof(int));
                    for (unsigned int face = 0; face < NDIM; ++face)
                    {
                        for (int d = 0; d < depth; ++d)
                        {
                            for (Box::Iterator it(FaceGeometry::toFaceBox(patch_box,face)); it; it++)
                            {
                                const FaceIndex i(it(),face,FaceIndex::Lower);
                                of.write(reinterpret_cast<const char*>(&(*data)(i,d)),sizeof(double));
                            }
                        }
                    }
                    of.close();
                }
            }
        }
        SAMRAI_MPI::barrier();
    }
    return;
}// saveFaceData

void
DebuggingUtilities::saveNodeData(
    const int patch_data_idx,
    const boost::shared_ptr<PatchHierarchy > hierarchy,
    const std::string& filename,
    const std::string& dirname)
{
    std::string truncated_dirname = dirname;
    while (truncated_dirname[truncated_dirname.size()-1] == '/')
    {
        truncated_dirname = std::string(truncated_dirname, truncated_dirname.size()-1);
    }
    Utilities::recursiveMkdir(truncated_dirname);

    const int rank = SAMRAI_MPI::getRank();
    const int nodes = SAMRAI_MPI::getNodes();
    for (int n = 0; n < nodes; ++n)
    {
        if (n == rank)
        {
            for (int ln = 0; ln <= hierarchy->getFinestLevelNumber(); ++ln)
            {
                boost::shared_ptr<PatchLevel > level = hierarchy->getPatchLevel(ln);
                for (PatchLevel::Iterator p(level); p; p++)
                {
                    const int patch_num = p();
                    boost::shared_ptr<Patch > patch = level->getPatch(patch_num);
                    const Box& patch_box = patch->getBox();
                    boost::shared_ptr<NodeData<double> > data = patch->getPatchData(patch_data_idx);

                    const std::string patch_filename = truncated_dirname + '/' + filename + '_' + Utilities::levelToString(ln) + '_' + Utilities::patchToString(patch_num);
                    std::ofstream of(patch_filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
                    for (unsigned int d = 0; d < NDIM; ++d)
                    {
                        of.write(reinterpret_cast<const char*>(&patch_box.lower()(d)),sizeof(int));
                        of.write(reinterpret_cast<const char*>(&patch_box.upper()(d)),sizeof(int));
                    }
                    const int depth = data->getDepth();
                    of.write(reinterpret_cast<const char*>(&depth),sizeof(int));
                    for (int d = 0; d < depth; ++d)
                    {
                        for (Box::Iterator it(NodeGeometry::toNodeBox(patch_box)); it; it++)
                        {
                            const NodeIndex i(it(),IntVector(0));
                            of.write(reinterpret_cast<const char*>(&(*data)(i,d)),sizeof(double));
                        }
                    }
                    of.close();
                }
            }
        }
        SAMRAI_MPI::barrier();
    }
    return;
}// saveNodeData

void
DebuggingUtilities::saveSideData(
    const int patch_data_idx,
    const boost::shared_ptr<PatchHierarchy > hierarchy,
    const std::string& filename,
    const std::string& dirname)
{
    std::string truncated_dirname = dirname;
    while (truncated_dirname[truncated_dirname.size()-1] == '/')
    {
        truncated_dirname = std::string(truncated_dirname, truncated_dirname.size()-1);
    }
    Utilities::recursiveMkdir(truncated_dirname);

    const int rank = SAMRAI_MPI::getRank();
    const int nodes = SAMRAI_MPI::getNodes();
    for (int n = 0; n < nodes; ++n)
    {
        if (n == rank)
        {
            for (int ln = 0; ln <= hierarchy->getFinestLevelNumber(); ++ln)
            {
                boost::shared_ptr<PatchLevel > level = hierarchy->getPatchLevel(ln);
                for (PatchLevel::Iterator p(level); p; p++)
                {
                    const int patch_num = p();
                    boost::shared_ptr<Patch > patch = level->getPatch(patch_num);
                    const Box& patch_box = patch->getBox();
                    boost::shared_ptr<SideData<double> > data = patch->getPatchData(patch_data_idx);

                    const std::string patch_filename = truncated_dirname + '/' + filename + '_' + Utilities::levelToString(ln) + '_' + Utilities::patchToString(patch_num);
                    std::ofstream of(patch_filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
                    for (unsigned int d = 0; d < NDIM; ++d)
                    {
                        of.write(reinterpret_cast<const char*>(&patch_box.lower()(d)),sizeof(int));
                        of.write(reinterpret_cast<const char*>(&patch_box.upper()(d)),sizeof(int));
                    }
                    const int depth = data->getDepth();
                    of.write(reinterpret_cast<const char*>(&depth),sizeof(int));
                    for (unsigned int side = 0; side < NDIM; ++side)
                    {
                        for (int d = 0; d < depth; ++d)
                        {
                            for (Box::Iterator it(SideGeometry::toSideBox(patch_box,side)); it; it++)
                            {
                                const SideIndex i(it(),side,SideIndex::Lower);
                                of.write(reinterpret_cast<const char*>(&(*data)(i,d)),sizeof(double));
                            }
                        }
                    }
                    of.close();
                }
            }
        }
        SAMRAI_MPI::barrier();
    }
    return;
}// saveSideData

void
DebuggingUtilities::saveLagrangianData(
    const boost::shared_ptr<LData> lag_data,
    const bool save_ghost_nodes,
    const std::string& filename,
    const std::string& dirname)
{
    std::string truncated_dirname = dirname;
    while (truncated_dirname[truncated_dirname.size()-1] == '/')
    {
        truncated_dirname = std::string(truncated_dirname, truncated_dirname.size()-1);
    }
    Utilities::recursiveMkdir(truncated_dirname);

    const boost::multi_array_ref<double,2>& array_data = *lag_data->getGhostedLocalFormVecArray();
    const int rank = SAMRAI_MPI::getRank();
    const int nodes = SAMRAI_MPI::getNodes();
    for (int n = 0; n < nodes; ++n)
    {
        if (n == rank)
        {
            const std::string data_filename = truncated_dirname + '/' + filename + '_' + Utilities::processorToString(n);
            std::ofstream of(data_filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
            const int depth = lag_data->getDepth();
            of.write(reinterpret_cast<const char*>(&depth),sizeof(int));
            const int num_local_nodes = lag_data->getLocalNodeCount();
            of.write(reinterpret_cast<const char*>(&num_local_nodes),sizeof(int));
            for (int i = 0; i < num_local_nodes; ++i)
            {
                for (int d = 0; d < depth; ++d)
                {
                    of.write(reinterpret_cast<const char*>(&(array_data[i][d])),sizeof(double));
                }
            }
            if (save_ghost_nodes)
            {
                const int num_ghost_nodes = lag_data->getGhostNodeCount();
                of.write(reinterpret_cast<const char*>(&num_ghost_nodes),sizeof(int));
                for (int i = 0; i < num_ghost_nodes; ++i)
                {
                    for (int d = 0; d < depth; ++d)
                    {
                        of.write(reinterpret_cast<const char*>(&(array_data[i+num_local_nodes][d])),sizeof(double));
                    }
                }
            }
            of.close();
        }
        SAMRAI_MPI::barrier();
    }
    lag_data->restoreArrays();
    return;
}// saveLagrangianData

/////////////////////////////// PROTECTED ////////////////////////////////////

/////////////////////////////// PRIVATE //////////////////////////////////////

/////////////////////////////// NAMESPACE ////////////////////////////////////

}// namespace IBTK

//////////////////////////////////////////////////////////////////////////////
