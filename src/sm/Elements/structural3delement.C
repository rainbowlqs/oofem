/*
 *
 *                 #####    #####   ######  ######  ###   ###
 *               ##   ##  ##   ##  ##      ##      ## ### ##
 *              ##   ##  ##   ##  ####    ####    ##  #  ##
 *             ##   ##  ##   ##  ##      ##      ##     ##
 *            ##   ##  ##   ##  ##      ##      ##     ##
 *            #####    #####   ##      ######  ##     ##
 *
 *
 *             OOFEM : Object Oriented Finite Element Code
 *
 *               Copyright (C) 1993 - 2013   Borek Patzak
 *
 *
 *
 *       Czech Technical University, Faculty of Civil Engineering,
 *   Department of Structural Mechanics, 166 29 Prague, Czech Republic
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Elements/structural3delement.h"
#include "feinterpol3d.h"
#include "gausspoint.h"
#include "CrossSections/structuralcrosssection.h"
#include "gaussintegrationrule.h"

namespace oofem {
Structural3DElement :: Structural3DElement(int n, Domain *aDomain) :
    NLStructuralElement(n, aDomain)
    // Constructor. Creates an element with number n, belonging to aDomain.
{
    //nlGeometry = 0; // Geometrical nonlinearities disabled as default
}



void
Structural3DElement :: computeBmatrixAt(GaussPoint *gp, FloatMatrix &answer, int li, int ui)
// Returns the [ 6 x (nno*3) ] strain-displacement matrix {B} of the receiver, eva-
// luated at gp.
// B matrix  -  6 rows : epsilon-X, epsilon-Y, epsilon-Z, gamma-YZ, gamma-ZX, gamma-XY  :
{
    FEInterpolation *interp = this->giveInterpolation();
    FloatMatrix dNdx; 
    interp->evaldNdx( dNdx, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );
    
    answer.resize(6, dNdx.giveNumberOfRows() * 3);
    answer.zero();

    for ( int i = 1; i <= dNdx.giveNumberOfRows(); i++ ) {
        answer.at(1, 3 * i - 2) = dNdx.at(i, 1);
        answer.at(2, 3 * i - 1) = dNdx.at(i, 2);
        answer.at(3, 3 * i - 0) = dNdx.at(i, 3);

        answer.at(5, 3 * i - 2) = answer.at(4, 3 * i - 1) = dNdx.at(i, 3);
        answer.at(6, 3 * i - 2) = answer.at(4, 3 * i - 0) = dNdx.at(i, 2);
        answer.at(6, 3 * i - 1) = answer.at(5, 3 * i - 0) = dNdx.at(i, 1);
    }
}



void
Structural3DElement :: computeBHmatrixAt(GaussPoint *gp, FloatMatrix &answer)
// Returns the [ 9 x (nno * 3) ] displacement gradient matrix {BH} of the receiver,
// evaluated at gp.
// BH matrix  -  9 rows : du/dx, dv/dy, dw/dz, dv/dz, du/dz, du/dy, dw/dy, dw/dx, dv/dx
{
    FEInterpolation *interp = this->giveInterpolation();
    FloatMatrix dNdx; 
    interp->evaldNdx( dNdx, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );
    
    answer.resize(9, dNdx.giveNumberOfRows() * 3);
    answer.zero();

    for ( int i = 1; i <= dNdx.giveNumberOfRows(); i++ ) {
        answer.at(1, 3 * i - 2) = dNdx.at(i, 1);     // du/dx
        answer.at(2, 3 * i - 1) = dNdx.at(i, 2);     // dv/dy
        answer.at(3, 3 * i - 0) = dNdx.at(i, 3);     // dw/dz
        answer.at(4, 3 * i - 1) = dNdx.at(i, 3);     // dv/dz
        answer.at(7, 3 * i - 0) = dNdx.at(i, 2);     // dw/dy
        answer.at(5, 3 * i - 2) = dNdx.at(i, 3);     // du/dz
        answer.at(8, 3 * i - 0) = dNdx.at(i, 1);     // dw/dx
        answer.at(6, 3 * i - 2) = dNdx.at(i, 2);     // du/dy
        answer.at(9, 3 * i - 1) = dNdx.at(i, 1);     // dv/dx
    }

}


MaterialMode
Structural3DElement :: giveMaterialMode()
{
    return _3dMat;
}


void
Structural3DElement :: computeStressVector(FloatArray &answer, const FloatArray &strain, GaussPoint *gp, TimeStep *tStep)
{
    this->giveStructuralCrossSection()->giveRealStress_3d(answer, gp, strain, tStep);
}

void
Structural3DElement :: computeConstitutiveMatrixAt(FloatMatrix &answer, MatResponseMode rMode, GaussPoint *gp, TimeStep *tStep)
{
    this->giveStructuralCrossSection()->giveStiffnessMatrix_3d(answer, rMode, gp, tStep);
}


void
Structural3DElement :: giveDofManDofIDMask(int inode, IntArray &answer) const
{
    answer = {D_u, D_v, D_w};
}


int 
Structural3DElement :: computeNumberOfDofs() 
{ 
    ///@todo move one hiearchy up and generalize
    IntArray dofIdMask; 
    this->giveDofManDofIDMask(-1, dofIdMask); // ok for standard elements
    return this->giveInterpolation()->giveNumberOfNodes() * dofIdMask.giveSize(); 
  
}


void Structural3DElement :: computeGaussPoints()
// Sets up the array containing the four Gauss points of the receiver.
{
    if ( integrationRulesArray.size() == 0 ) {
        integrationRulesArray.resize( 1 );
        integrationRulesArray [ 0 ].reset( new GaussIntegrationRule(1, this, 1, 6) );
        this->giveCrossSection()->setupIntegrationPoints(* integrationRulesArray [ 0 ], numberOfGaussPoints, this);
    }
}



double 
Structural3DElement :: computeVolumeAround(GaussPoint *gp)
// Returns the portion of the receiver which is attached to gp.
{
    double determinant, weight, volume;
    determinant = fabs( this->giveInterpolation()->giveTransformationJacobian( gp->giveNaturalCoordinates(),
                                                                       FEIElementGeometryWrapper(this) ) );

    weight = gp->giveWeight();
    volume = determinant * weight;

    return volume;
}



double
Structural3DElement :: giveCharacteristicLength(const FloatArray &normalToCrackPlane)
{
    return this->giveLengthInDir(normalToCrackPlane);
}





// Surface support
void
Structural3DElement :: computeSurfaceNMatrixAt(FloatMatrix &answer, int iSurf, GaussPoint *sgp)
{
    /* Returns the [ 3 x (nno*3) ] shape function matrix {N} of the receiver, 
     * evaluated at the given gp.
     * {u} = {N}*{a} gives the displacements at the integration point.
     */ 
          
    // Evaluate the shape functions at the position of the gp. 
    FloatArray N;
    static_cast< FEInterpolation3d* > ( this->giveInterpolation() )->
        surfaceEvalN( N, iSurf, sgp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );  
    answer.beNMatrixOf(N, 3);
}

#if 1
IntegrationRule *
Structural3DElement :: giveSurfaceIntegrationRule(int order, int isurf)
{
    return static_cast< FEInterpolation3d* > ( this->giveInterpolation() )->giveBoundaryIntegrationRule(order, isurf);   
}
#endif

void
Structural3DElement :: giveSurfaceDofMapping(IntArray &answer, int iSurf) const
{
    IntArray nodes;
    const int ndofsn = 3;

     static_cast< FEInterpolation3d* > ( this->giveInterpolation() )->
        computeLocalSurfaceMapping(nodes, iSurf);

    answer.resize(nodes.giveSize() *3 );

    for ( int i = 1; i <= nodes.giveSize(); i++ ) {
        answer.at(i * ndofsn - 2) = nodes.at(i) * ndofsn - 2;
        answer.at(i * ndofsn - 1) = nodes.at(i) * ndofsn - 1;
        answer.at(i * ndofsn) = nodes.at(i) * ndofsn;
    }
}

double
Structural3DElement :: computeSurfaceVolumeAround(GaussPoint *gp, int iSurf)
{
    double determinant, weight, volume;
    determinant = fabs( static_cast< FEInterpolation3d* > ( this->giveInterpolation() )-> 
        surfaceGiveTransformationJacobian( iSurf, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) ) );

    weight = gp->giveWeight();
    volume = determinant * weight;

    return volume;
}

void
Structural3DElement :: computeSurfIpGlobalCoords(FloatArray &answer, GaussPoint *gp, int iSurf)
{
    static_cast< FEInterpolation3d* > ( this->giveInterpolation() )-> 
        surfaceLocal2global( answer, iSurf, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );
}




int
Structural3DElement :: computeLoadLSToLRotationMatrix(FloatMatrix &answer, int, GaussPoint *)
{
    OOFEM_ERROR("surface local coordinate system not supported");
    return 1;
}






// Edge support

void
Structural3DElement :: computeEgdeNMatrixAt(FloatMatrix &answer, int iedge, GaussPoint *gp)
{
    /* Returns the [ 3 x (nno*3)] shape function matrix {N} of the receiver, 
     * evaluated at the given gp.
     * {u} = {N}*{a} gives the displacements at the integration point.
     */ 
    ///@todo move up in hiearchy
          
    // Evaluate the shape functions at the position of the gp. 
    FloatArray N;
    static_cast< FEInterpolation3d* > ( this->giveInterpolation() )->
        edgeEvalN( N, iedge, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );  
    answer.beNMatrixOf(N, 3);
}



void
Structural3DElement :: giveEdgeDofMapping(IntArray &answer, int iEdge) const
{
    /*
     * provides dof mapping of local edge dofs (only nonzero are taken into account)
     * to global element dofs
     */
    IntArray eNodes;
    static_cast< FEInterpolation3d* > ( this->giveInterpolation() )->computeLocalEdgeMapping(eNodes,  iEdge);

    answer.resize( eNodes.giveSize() * 3 );
    for ( int i = 1; i <= eNodes.giveSize(); i++ ) {
        answer.at(i * 3 - 2) = eNodes.at(i) * 3 - 2;
        answer.at(i * 3 - 1) = eNodes.at(i) * 3 - 1;
        answer.at(i * 3)     = eNodes.at(i) * 3;
    }
}



void
Structural3DElement :: computeEdgeIpGlobalCoords(FloatArray &answer, GaussPoint *gp, int iEdge)
{
    static_cast< FEInterpolation3d* > ( this->giveInterpolation() )->
        edgeLocal2global( answer, iEdge, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );
}



double
Structural3DElement :: computeEdgeVolumeAround(GaussPoint *gp, int iEdge)
{
    /* Returns the line element ds associated with the given gp on the specific edge.
     * Note: The name is misleading since there is no volume to speak of in this case. 
     * The returned value is used for integration of a line integral (external forces).
     */
    double detJ = static_cast< FEInterpolation3d* > ( this->giveInterpolation() )->
        edgeGiveTransformationJacobian( iEdge, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );
    return detJ * gp->giveWeight();
}


int
Structural3DElement :: computeLoadLEToLRotationMatrix(FloatMatrix &answer, int iEdge, GaussPoint *gp)
{
    // returns transformation matrix from
    // edge local coordinate system
    // to element local c.s
    // (same as global c.s in this case)
    //
    // i.e. f(element local) = T * f(edge local)
    //
    ///@todo how should this be supported 
    OOFEM_ERROR("egde local coordinate system not supported");
    return 1;
}



} // end namespace oofem
