// Filename: IBEulerianForceFunction.h
// Created on 28 Sep 2004 by Boyce Griffith
//
// Copyright (c) 2002-2010 Boyce Griffith
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef included_IBEulerianForceFunction
#define included_IBEulerianForceFunction

/////////////////////////////// INCLUDES /////////////////////////////////////

// IBTK INCLUDES
#include <ibtk/CartGridFunction.h>

// SAMRAI INCLUDES
#include <Patch.h>
#include <Variable.h>
#include <tbox/Pointer.h>

/////////////////////////////// CLASS DEFINITION /////////////////////////////

namespace IBAMR
{
/*!
 * \brief Class IBEulerianForceFunction is used to communicate the Eulerian body
 * force computed by class IBHierarchyIntegrator to the incompressible
 * Navier-Stokes solver.
 */
class IBEulerianForceFunction
    : public IBTK::CartGridFunction
{
public:
    /*!
     * \brief Constructor.
     */
    IBEulerianForceFunction(
        const std::string& object_name,
        const int F_current_idx,
        const int F_new_idx,
        const int F_half_idx);

    /*!
     * \brief Virtual destructor.
     */
    virtual
    ~IBEulerianForceFunction();

    /*!
     * \brief Set the current and new times for the present timestep.
     */
    void
    setTimeInterval(
        const double current_time,
        const double new_time);

    /*!
     * \brief Register an optional additional body force specification which
     * will be added to the IB force.
     */
    void
    registerBodyForceSpecification(
        SAMRAI::tbox::Pointer<IBTK::CartGridFunction> F_fcn);

    /*!
     * \name Methods to set the data.
     */
    //\{

    /*!
     * \note This concrete IBTK::CartGridFunction is time-dependent.
     */
    virtual bool
    isTimeDependent() const;

    /*!
     * Set the data on the patch interior.
     */
    virtual void
    setDataOnPatch(
        const int data_idx,
        SAMRAI::tbox::Pointer<SAMRAI::hier::Variable<NDIM> > var,
        SAMRAI::tbox::Pointer<SAMRAI::hier::Patch<NDIM> > patch,
        const double data_time,
        const bool initial_time=false,
        SAMRAI::tbox::Pointer<SAMRAI::hier::PatchLevel<NDIM> > level=SAMRAI::tbox::Pointer<SAMRAI::hier::PatchLevel<NDIM> >(NULL));

    //\}

private:
    /*!
     * \brief Default constructor.
     *
     * \note This constructor is not implemented and should not be used.
     */
    IBEulerianForceFunction();

    /*!
     * \brief Copy constructor.
     *
     * \note This constructor is not implemented and should not be used.
     *
     * \param from The value to copy to this object.
     */
    IBEulerianForceFunction(
        const IBEulerianForceFunction& from);

    /*!
     * \brief Assignment operator.
     *
     * \note This operator is not implemented and should not be used.
     *
     * \param that The value to assign to this object.
     *
     * \return A reference to this object.
     */
    IBEulerianForceFunction&
    operator=(
        const IBEulerianForceFunction& that);

    /*!
     * The current and new time for the present timestep.
     */
    double d_current_time, d_new_time;

    /*!
     * Patch data descriptor indices for the current, new, and half-time force
     * data.
     */
    const int d_F_current_idx, d_F_new_idx, d_F_half_idx;

    /*!
     * Optional body force generator.
     */
    SAMRAI::tbox::Pointer<IBTK::CartGridFunction> d_body_force_fcn;
};
}// namespace IBAMR

/////////////////////////////// INLINE ///////////////////////////////////////

//#include <ibamr/IBEulerianForceFunction.I>

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_IBEulerianForceFunction
