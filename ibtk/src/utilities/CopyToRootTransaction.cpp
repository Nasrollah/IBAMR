// Filename: CopyToRootTransaction.cpp
// Created on 04 May 2011 by Boyce Griffith
//
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

/////////////////////////////// INCLUDES /////////////////////////////////////

#include <ostream>

#include "BoxArray.h"
#include "SAMRAI/hier/BoxGeometry.h"
#include "SAMRAI/hier/BoxOverlap.h"
#include "CopyToRootTransaction.h"
#include "SAMRAI/geom/GridGeometry.h"
#include "SAMRAI/hier/IntVector.h"
#include "SAMRAI/hier/Patch.h"
#include "SAMRAI/hier/PatchDataFactory.h"
#include "SAMRAI/hier/PatchDescriptor.h"
#include "SAMRAI/SAMRAI_config.h"
#include "ibtk/namespaces.h" // IWYU pragma: keep
#include "SAMRAI/tbox/AbstractStream.h"
#include "SAMRAI/tbox/Utilities.h"

namespace SAMRAI {
namespace hier {
class Box;
}  // namespace hier
}  // namespace SAMRAI

/////////////////////////////// NAMESPACE ////////////////////////////////////

namespace IBTK
{
/////////////////////////////// STATIC ///////////////////////////////////////

/////////////////////////////// PUBLIC ///////////////////////////////////////

CopyToRootTransaction::CopyToRootTransaction(
    const int src_proc,
    const int dst_proc,
    boost::shared_ptr<PatchLevel > patch_level,
    const int src_patch_data_idx,
    boost::shared_ptr<PatchData > dst_patch_data)
    : d_src_proc(src_proc),
      d_dst_proc(dst_proc),
      d_patch_level(patch_level),
      d_src_patch_data_idx(src_patch_data_idx),
      d_dst_patch_data(dst_patch_data)
{
    // intentionally blank
    return;
}// CopyToRootTransaction

CopyToRootTransaction::~CopyToRootTransaction()
{
    // intentionally blank
    return;
}// CopyToRootTransaction

boost::shared_ptr<PatchData >
CopyToRootTransaction::getRootPatchData() const
{
    return d_dst_patch_data;
}// getRootPatchData

bool
CopyToRootTransaction::canEstimateIncomingMessageSize()
{
    return false;
}// canEstimateIncomingMessageSize

int
CopyToRootTransaction::computeIncomingMessageSize()
{
    return 0;
}// computeIncomingMessageSize

int
CopyToRootTransaction::computeOutgoingMessageSize()
{
    boost::shared_ptr<PatchDataFactory > pdat_factory = d_patch_level->getPatchDescriptor()->getPatchDataFactory(d_src_patch_data_idx);

    boost::shared_ptr<GridGeometry > grid_geom = d_patch_level->getGridGeometry();
#if !defined(NDEBUG)
    TBOX_ASSERT(grid_geom->getDomainIsSingleBox());
#endif
    const Box& dst_box = grid_geom->getPhysicalDomain()[0];
    boost::shared_ptr<BoxGeometry > dst_box_geometry = pdat_factory->getBoxGeometry(dst_box);

    int size = AbstractStream::sizeofInt();
    for (PatchLevel::Iterator p(d_patch_level); p; p++)
    {
        const int src_patch_num = p();
        size += AbstractStream::sizeofInt();
        boost::shared_ptr<Patch > patch = d_patch_level->getPatch(src_patch_num);
        const Box& src_box = patch->getBox();
        boost::shared_ptr<BoxGeometry > src_box_geometry = pdat_factory->getBoxGeometry(src_box);
        const Box& src_mask = dst_box;
        const bool overwrite_interior = true;
        const IntVector src_shift = 0;
        boost::shared_ptr<BoxOverlap > box_overlap = dst_box_geometry->calculateOverlap(*src_box_geometry, src_mask, overwrite_interior, src_shift);
        size += patch->getPatchData(d_src_patch_data_idx)->getDataStreamSize(*box_overlap);
    }
    return size;
}// computeOutgoingMessageSize

int
CopyToRootTransaction::getSourceProcessor()
{
    return d_src_proc;
}// getSourceProcessor

int
CopyToRootTransaction::getDestinationProcessor()
{
    return d_dst_proc;
}// getDestinationProcessor

void
CopyToRootTransaction::packStream(
    AbstractStream& stream)
{
    boost::shared_ptr<PatchDataFactory > pdat_factory = d_patch_level->getPatchDescriptor()->getPatchDataFactory(d_src_patch_data_idx);

    boost::shared_ptr<GridGeometry > grid_geom = d_patch_level->getGridGeometry();
#if !defined(NDEBUG)
    TBOX_ASSERT(grid_geom->getDomainIsSingleBox());
#endif
    const Box& dst_box = grid_geom->getPhysicalDomain()[0];
    boost::shared_ptr<BoxGeometry > dst_box_geometry = pdat_factory->getBoxGeometry(dst_box);

    int src_patch_count = 0;
    for (PatchLevel::Iterator p(d_patch_level); p; p++)
    {
        ++src_patch_count;
    }
    stream << src_patch_count;

    for (PatchLevel::Iterator p(d_patch_level); p; p++)
    {
        const int src_patch_num = p();
        stream << src_patch_num;
        boost::shared_ptr<Patch > patch = d_patch_level->getPatch(src_patch_num);
        const Box& src_box = patch->getBox();
        boost::shared_ptr<BoxGeometry > src_box_geometry = pdat_factory->getBoxGeometry(src_box);
        const Box& src_mask = dst_box;
        const bool overwrite_interior = true;
        const IntVector src_shift = 0;
        boost::shared_ptr<BoxOverlap > box_overlap = dst_box_geometry->calculateOverlap(*src_box_geometry, src_mask, overwrite_interior, src_shift);
        patch->getPatchData(d_src_patch_data_idx)->packStream(stream, *box_overlap);
    }
    return;
}// packStream

void
CopyToRootTransaction::unpackStream(
    AbstractStream& stream)
{
    boost::shared_ptr<PatchDataFactory > pdat_factory = d_patch_level->getPatchDescriptor()->getPatchDataFactory(d_src_patch_data_idx);

    boost::shared_ptr<GridGeometry > grid_geom = d_patch_level->getGridGeometry();
#if !defined(NDEBUG)
    TBOX_ASSERT(grid_geom->getDomainIsSingleBox());
#endif
    const Box& dst_box = grid_geom->getPhysicalDomain()[0];
    boost::shared_ptr<BoxGeometry > dst_box_geometry = pdat_factory->getBoxGeometry(dst_box);

    int src_patch_count;
    stream >> src_patch_count;
    for (int p = 0; p < src_patch_count; ++p)
    {
        int src_patch_num;
        stream >> src_patch_num;
        const Box& src_box = d_patch_level->getBoxes()[src_patch_num];
        boost::shared_ptr<BoxGeometry > src_box_geometry = pdat_factory->getBoxGeometry(src_box);
        const Box& src_mask = dst_box;
        const bool overwrite_interior = true;
        const IntVector src_shift = 0;
        boost::shared_ptr<BoxOverlap > box_overlap = dst_box_geometry->calculateOverlap(*src_box_geometry, src_mask, overwrite_interior, src_shift);
        d_dst_patch_data->unpackStream(stream, *box_overlap);
    }
    return;
}// unpackStream

void
CopyToRootTransaction::copyLocalData()
{
    boost::shared_ptr<PatchDataFactory > pdat_factory = d_patch_level->getPatchDescriptor()->getPatchDataFactory(d_src_patch_data_idx);

    boost::shared_ptr<GridGeometry > grid_geom = d_patch_level->getGridGeometry();
#if !defined(NDEBUG)
    TBOX_ASSERT(grid_geom->getDomainIsSingleBox());
#endif
    const Box& dst_box = grid_geom->getPhysicalDomain()[0];
    boost::shared_ptr<BoxGeometry > dst_box_geometry = pdat_factory->getBoxGeometry(dst_box);

    for (PatchLevel::Iterator p(d_patch_level); p; p++)
    {
        int src_patch_num = p();
        boost::shared_ptr<Patch > patch = d_patch_level->getPatch(src_patch_num);
        const Box& src_box = patch->getBox();
        boost::shared_ptr<BoxGeometry > src_box_geometry = pdat_factory->getBoxGeometry(src_box);
        const Box& src_mask = dst_box;
        const bool overwrite_interior = true;
        const IntVector src_shift = 0;
        boost::shared_ptr<BoxOverlap > box_overlap = dst_box_geometry->calculateOverlap(*src_box_geometry, src_mask, overwrite_interior, src_shift);
        d_dst_patch_data->copy(*patch->getPatchData(d_src_patch_data_idx), *box_overlap);
    }
    return;
}// copyLocalData

void
CopyToRootTransaction::printClassData(
    std::ostream& stream) const
{
    stream << "CopyToRootTransaction::printClassData() is not implemented\n";
    return;
}// printClassData

/////////////////////////////// PROTECTED ////////////////////////////////////

/////////////////////////////// PRIVATE //////////////////////////////////////

/////////////////////////////// NAMESPACE ////////////////////////////////////

}// namespace IBTK

//////////////////////////////////////////////////////////////////////////////
