// Filename: LMarkerRefine.cpp
// Created on 04 Oct 2007 by Boyce Griffith
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

#include <vector>

#include "SAMRAI/geom/CartesianPatchGeometry.h"
#include "SAMRAI/hier/Index.h"
#include "LMarkerRefine.h"
#include "SAMRAI/hier/Patch.h"
#include "boost/array.hpp"
#include "ibtk/IndexUtilities.h"
#include "ibtk/IndexUtilities-inl.h"
#include "ibtk/LMarker.h"
#include "ibtk/LMarkerSet.h"
#include "ibtk/LMarkerSetData.h"
#include "ibtk/LMarkerSetVariable.h"
#include "ibtk/LMarker-inl.h"
#include "ibtk/LSet.h"
#include "ibtk/LSetData.h"
#include "ibtk/LSet-inl.h"
#include "boost/array.hpp"
#include "ibtk/namespaces.h" // IWYU pragma: keep

namespace SAMRAI {
namespace hier {
class Variable;
}  // namespace hier
}  // namespace SAMRAI

/////////////////////////////// NAMESPACE ////////////////////////////////////

namespace IBTK
{
/////////////////////////////// STATIC ///////////////////////////////////////

const std::string LMarkerRefine::s_op_name = "LMARKER_REFINE";

namespace
{
static const int REFINE_OP_PRIORITY = 0;
static const int REFINE_OP_STENCIL_WIDTH = 0;
}

/////////////////////////////// PUBLIC ///////////////////////////////////////

LMarkerRefine::LMarkerRefine()
{
    // intentionally blank
    return;
}// LMarkerRefine

LMarkerRefine::~LMarkerRefine()
{
    // intentionally blank
    return;
}// ~LMarkerRefine

bool
LMarkerRefine::findRefineOperator(
    const boost::shared_ptr<Variable >& var,
    const std::string& op_name) const
{
    boost::shared_ptr<LMarkerSetVariable> mark_var = var;
    return (mark_var && op_name == s_op_name);
}// findRefineOperator

const std::string&
LMarkerRefine::getOperatorName() const
{
    return s_op_name;
}// getOperatorName

int
LMarkerRefine::getOperatorPriority() const
{
    return REFINE_OP_PRIORITY;
}// getOperatorPriority

IntVector
LMarkerRefine::getStencilWidth() const
{
    return REFINE_OP_STENCIL_WIDTH;
}// getStencilWidth

void
LMarkerRefine::refine(
    Patch& fine,
    const Patch& coarse,
    const int dst_component,
    const int src_component,
    const Box& fine_box,
    const IntVector& ratio) const
{
    boost::shared_ptr<LMarkerSetData> dst_mark_data = fine  .getPatchData(dst_component);
    boost::shared_ptr<LMarkerSetData> src_mark_data = coarse.getPatchData(src_component);

    const Box& fine_patch_box = fine.getBox();
    const boost::shared_ptr<CartesianPatchGeometry > fine_patch_geom = fine.getPatchGeometry();
    const Index& fine_patch_lower = fine_patch_box.lower();
    const Index& fine_patch_upper = fine_patch_box.upper();
    const double* const fine_patchXLower = fine_patch_geom->getXLower();
    const double* const fine_patchXUpper = fine_patch_geom->getXUpper();
    const double* const fine_patchDx = fine_patch_geom->getDx();

    const boost::shared_ptr<CartesianPatchGeometry > coarse_patch_geom = coarse.getPatchGeometry();
    const double* const coarse_patchDx = coarse_patch_geom->getDx();

    const Box coarse_box = Box::coarsen(fine_box,ratio);
    for (LMarkerSetData::SetIterator it(*src_mark_data); it; it++)
    {
        const Index& coarse_i = it.getIndex();
        if (coarse_box.contains(coarse_i))
        {
            const LMarkerSet& coarse_mark_set = it();
            for (LMarkerSet::const_iterator cit = coarse_mark_set.begin(); cit != coarse_mark_set.end(); ++cit)
            {
                const LMarkerSet::value_type& coarse_mark = *cit;
                const Point& X = coarse_mark->getPosition();
                const IntVector& offset = coarse_mark->getPeriodicOffset();
                boost::array<double,NDIM> X_shifted;
                for (unsigned int d = 0; d < NDIM; ++d)
                {
                    X_shifted[d] = X[d] + static_cast<double>(offset(d))*coarse_patchDx[d];
                }

                const Index fine_i = IndexUtilities::getCellIndex(X_shifted, fine_patchXLower, fine_patchXUpper, fine_patchDx, fine_patch_lower, fine_patch_upper);
                if (fine_box.contains(fine_i))
                {
                    if (!dst_mark_data->isElement(fine_i))
                    {
                        dst_mark_data->appendItemboost::shared_ptr(fine_i, new LMarkerSet());
                    }
                    LMarkerSet& fine_mark_set = *(dst_mark_data->getItem(fine_i));
                    fine_mark_set.push_back(coarse_mark);
                }
            }
        }
    }
    return;
}// refine

/////////////////////////////// PROTECTED ////////////////////////////////////

/////////////////////////////// PRIVATE //////////////////////////////////////

/////////////////////////////// NAMESPACE ////////////////////////////////////

}// namespace IBTK

//////////////////////////////////////////////////////////////////////////////
