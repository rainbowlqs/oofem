/* $Header: /home/cvs/bp/oofem/oofemlib/src/dofmanager.C,v 1.18.4.1 2004/04/05 15:19:43 bp Exp $ */
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
 *               Copyright (C) 1993 - 2008   Borek Patzak
 *
 *
 *
 *       Czech Technical University, Faculty of Civil Engineering,
 *   Department of Structural Mechanics, 166 29 Prague, Czech Republic
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef rigidarmnode_h
#define rigidarmnode_h

#include "node.h"

#ifndef __MAKEDEPEND
 #include <stdio.h>
#endif

namespace oofem {
class FloatArray;
class IntArray;

/**
 * Class implementing node connected to other node (master) using rigid arm in finite element mesh.
 * Rigid arm node posses no degrees of freedom - all dofs are mapped to master dofs.
 ++ added on August 28, the Rigid arm node supports not only slave dofs mapped to master
 * but also some dofs can be primary doofs. Introduced masterDofMask allowing to
 * distinguish between primary and mapped (slave) dofs. The primary DOFs can have their own BCs, ICs.
 *
 * The introduction of rigid arm connected nodes allows to avoid very stiff elements used
 * for modelling the rigid-arm connection. The rigid arm node maps its dofs to master dofs
 * using simple transformations (small rotations are assumed). Therefore, the contribution
 * to rigid arm node are localized directly to master related equations.
 * The rigid arm node can not have its own boundary or initial conditions,
 * they are determined completely from master dof conditions.
 * The local coordinate system in slave is not supported in current implementation, the global lcs applies.
 * On the other hand, rigid arm node can be loaded independently of master.
 * The transformation for DOFs and load is not ortogonal - the inverse transformation can
 * not be constructed by transposition. Because of time consuming inversion, methods
 * can generally compute both transformations for dofs as well as loads.
 */
class RigidArmNode : public Node
{
protected:
    ///
    IntArray *masterMask;

    /// count of Master Dofs
    IntArray *countOfMasterDofs;
    /// number of master DofManager (Node)
    int masterDofMngr;
    /// pointer to master Node
    Node **masterNode;
    ///
    IntArray **masterDofID;
    /// array of vectors of master contribution coefficients
    FloatArray **masterContribution;

private:
    void allocAuxArrays(void);
    void deallocAuxArrays(void);

public:
    /**
     * Constructor. Creates a rigid-arm node with number n, belonging to aDomain.
     * @param n node number in domain aDomain
     * @param aDomain domain to which node belongs
     */
    RigidArmNode(int n, Domain *aDomain);
    /**
     * Destructor.
     */
    ~RigidArmNode(void) { }

    /**
     * Initializes receiver acording to object description stored in input record.
     */
    IRResultType initializeFrom(InputRecord *ir);
    /**
     * Checks internal data consistency in node.
     * @return nonzero if receiver check is o.k.
     */
    int checkConsistency();
    /**
     * compute vector of master contribution coefficients - SUMA of contributions == 1.0
     */
    int computeMasterContribution();

    /**
     * Returns class name of the receiver.
     */
    const char *giveClassName() const { return "RigidArmNode"; }
    /**
     * Returns classType id of receiver.
     * @see FEMComponent::giveClassID
     */
    classType giveClassID() const { return RigidArmNodeClass; }
    /// Returns true if dof of given type is allowed to be associated to receiver
    bool isDofTypeCompatible(dofType type) const { return ( type == DT_master || type == DT_slave ); }

    /// returns reference to master dof. Public because RigidArmSlaveDof need to access.
    Node *giveMasterDofMngr() const { return * masterNode; }
};
} // end namespace oofem
#endif // rigidarmnode_h
