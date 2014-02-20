// Filename: PoissonSolver.cpp
// Created on 07 Apr 2012 by Boyce Griffith
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

#include <stddef.h>
#include <memory>
#include <string>

#include "SAMRAI/solv/LocationIndexRobinBcCoefs.h"
#include "PoissonSolver.h"
#include "SAMRAI/solv/RobinBcCoefStrategy.h"
#include "ibtk/namespaces.h" // IWYU pragma: keep
#include "SAMRAI/tbox/Database.h"


/////////////////////////////// NAMESPACE ////////////////////////////////////

namespace IBTK
{
/////////////////////////////// STATIC ///////////////////////////////////////

/////////////////////////////// PUBLIC ///////////////////////////////////////

PoissonSolver::PoissonSolver()
    : d_poisson_spec(d_object_name+"::poisson_spec"),
      d_default_bc_coef(new LocationIndexRobinBcCoefs(d_object_name+"::default_bc_coef", boost::shared_ptr<Database>(NULL))),
      d_bc_coefs(1, d_default_bc_coef)
{
    // Initialize the Poisson specifications.
    d_poisson_spec.setCZero();
    d_poisson_spec.setDConstant(-1.0);

    // Setup a default boundary condition object that specifies homogeneous
    // Dirichlet boundary conditions.
    for (unsigned int d = 0; d < NDIM; ++d)
    {
        LocationIndexRobinBcCoefs* p_default_bc_coef = dynamic_cast<LocationIndexRobinBcCoefs*>(d_default_bc_coef);
        p_default_bc_coef->setBoundaryValue(2*d  ,0.0);
        p_default_bc_coef->setBoundaryValue(2*d+1,0.0);
    }
    return;
}// PoissonSolver()

PoissonSolver::~PoissonSolver()
{
    delete d_default_bc_coef;
    d_default_bc_coef = NULL;
    return;
}// ~PoissonSolver()

void
PoissonSolver::setPoissonSpecifications(
    const PoissonSpecifications& poisson_spec)
{
    d_poisson_spec = poisson_spec;
    return;
}// setPoissonSpecifications

void
PoissonSolver::setPhysicalBcCoef(
    boost::shared_ptr<RobinBcCoefStrategy> const bc_coef)
{
    setPhysicalBcCoefs(std::vector<boost::shared_ptr<RobinBcCoefStrategy> >(1,bc_coef));
    return;
}// setPhysicalBcCoef

void
PoissonSolver::setPhysicalBcCoefs(
    const std::vector<boost::shared_ptr<RobinBcCoefStrategy> >& bc_coefs)
{
    d_bc_coefs.resize(bc_coefs.size());
    for (unsigned int l = 0; l < bc_coefs.size(); ++l)
    {
        if (bc_coefs[l])
        {
            d_bc_coefs[l] = bc_coefs[l];
        }
        else
        {
            d_bc_coefs[l] = d_default_bc_coef;
        }
    }
    return;
}// setPhysicalBcCoefs

/////////////////////////////// PRIVATE //////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

}// namespace IBTK

//////////////////////////////////////////////////////////////////////////////
