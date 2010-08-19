// Filename: TGACoefs.h
// Created on 26 Aug 2007 by Boyce Griffith
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

#ifndef included_TGACoefs
#define included_TGACoefs

/////////////////////////////// INCLUDES /////////////////////////////////////

/////////////////////////////// CLASS DEFINITION /////////////////////////////

namespace IBAMR
{
/*!
 * \brief Class TGACoefs is a lightweight utility class which is used to specify
 * the coefficients employed by the TGA implicit time integration algorithm.
 *
 * The TGA discretization is:
 *
 *     (I-nu2*dt*mu*L(t_int)) (I-nu1*dt*mu*L(t_new)) Q(n+1) = [(I+nu3*dt*mu*L(t_old)) Q(n) + (I+nu4*dt*mu*L) F(t_avg) dt]
 *
 * where
 *
 *    t_old = n dt
 *    t_new = (n+1) dt
 *    t_int = t_new - nu1*dt = t_old + (nu2+nu3)*dt
 *    t_avg = (t_new+t_old)/2 = t_old + (nu1+nu2+nu4)*dt
 *
 * Following McCorquodale et al., the coefficients for the TGA discretization
 * are:
 *
 *     nu1 = (a - sqrt(a^2-4*a+2))/2
 *     nu2 = (a + sqrt(a^2-4*a+2))/2
 *     nu3 = (1-a)
 *     nu4 = (0.5-a)
 *
 * Note that by choosing a = 2 - sqrt(2), nu1 == nu2.
 *
 * Ref: McCorquodale, Colella, Johansen.  "A Cartesian grid embedded boundary
 * method for the heat equation on irregular domains." JCP 173, pp. 620-635
 * (2001)
 */
class TGACoefs
{
public:
    static const double a;
    static const double nu1;
    static const double nu2;
    static const double nu3;
    static const double nu4;

protected:

private:
    /*!
     * \brief Default constructor.
     *
     * \note This constructor is not implemented and should not be
     * used.
     */
    TGACoefs();

    /*!
     * \brief Copy constructor.
     *
     * \note This constructor is not implemented and should not be
     * used.
     *
     * \param from The value to copy to this object.
     */
    TGACoefs(
        const TGACoefs& from);

    /*!
     * \brief Destructor.
     */
    ~TGACoefs();

    /*!
     * \brief Assignment operator.
     *
     * \note This operator is not implemented and should not be used.
     *
     * \param that The value to assign to this object.
     *
     * \return A reference to this object.
     */
    TGACoefs& operator=(
        const TGACoefs& that);
};
}// namespace IBAMR

/////////////////////////////// INLINE ///////////////////////////////////////

//#include "TGACoefs.I"

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_TGACoefs
