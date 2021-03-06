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

#include "fluidstructureproblem.h"
#include "engngm.h"
#include "timestep.h"
#include "function.h"
#include "metastep.h"
#include "exportmodulemanager.h"
#include "mathfem.h"
#include "oofemtxtdatareader.h"
#include "util.h"
#include "verbose.h"
#include "classfactory.h"

#include "pfem.h"
#include "interactionpfemparticle.h"
#include "../../sm/EngineeringModels/diidynamic.h"
#include "../../sm/EngineeringModels/nlineardynamic.h"

#include <stdlib.h>

#ifdef __OOFEG
 #include "oofeggraphiccontext.h"
#endif

namespace oofem {

REGISTER_EngngModel( FluidStructureProblem );

FluidStructureProblem :: FluidStructureProblem(int i, EngngModel *_master) : StaggeredProblem(i, _master)
{
///    ndomains = 1; // domain is needed to store the time step ltf
///    nModels  = 2;
///    emodelList = new AList< EngngModel >(nModels);
///    inputStreamNames = new std :: string [ nModels ];

///    dtTimeFunction = 0;
///    stepMultiplier = 1.;
///    timeDefinedByProb = 0;
    tol = 1.e-3;
    iterationNumber = 0;
}

FluidStructureProblem :: ~FluidStructureProblem()
{
///    delete emodelList;
///    delete [] inputStreamNames;
}

///////////
int
FluidStructureProblem :: instanciateYourself(DataReader *dr, InputRecord *ir, const char *dataOutputFileName, const char *desc)
{
    int result;
    result = EngngModel :: instanciateYourself(dr, ir, dataOutputFileName, desc);
    ir->finish();
    // instanciate slave problems
    result &= this->instanciateSlaveProblems();
    return result;
}

int
FluidStructureProblem :: instanciateDefaultMetaStep(InputRecord *ir)
{
    if ( !timeDefinedByProb ) {
        EngngModel :: instanciateDefaultMetaStep(ir);
    }

    //there are no slave problems initiated so far, the overall metaStep will defined in a slave problem instantiation

    return 1;
}

int
FluidStructureProblem :: instanciateSlaveProblems()
{//first instantiate master problem if defined
    EngngModel *timeDefProb = NULL;
    emodelList.resize(inputStreamNames.size());
    if ( timeDefinedByProb ) {
        OOFEMTXTDataReader dr( inputStreamNames [ timeDefinedByProb - 1 ] );
        std :: unique_ptr< EngngModel >prob( InstanciateProblem(& dr, this->pMode, this->contextOutputMode, NULL) );
        timeDefProb = prob.get();
        emodelList[timeDefinedByProb-1] = std::move(prob);
    }

    for ( int i = 1; i <= (int)inputStreamNames.size(); i++ ) {
        if ( i == timeDefinedByProb ) {
            continue;
        }

        OOFEMTXTDataReader dr( inputStreamNames [ i - 1 ] );
        //the slave problem dictating time needs to have attribute master=NULL, other problems point to the dictating slave
        std :: unique_ptr< EngngModel >prob( InstanciateProblem(& dr, this->pMode, this->contextOutputMode, timeDefinedByProb ? timeDefProb : this) );
        emodelList[i-1] = std::move(prob);
    }

    return 1;
}


IRResultType
FluidStructureProblem :: initializeFrom(InputRecord *ir)
{
    IRResultType result;                // Required by IR_GIVE_FIELD macro


    if ( ir->hasField(_IFT_StaggeredProblem_deltat) ) {
        EngngModel :: initializeFrom(ir);
        IR_GIVE_FIELD(ir, deltaT, _IFT_StaggeredProblem_deltat);
        dtFunction = 0;
    } else if ( ir->hasField(_IFT_StaggeredProblem_prescribedtimes) ) {
        EngngModel :: initializeFrom(ir);
        IR_GIVE_FIELD(ir, discreteTimes, _IFT_StaggeredProblem_prescribedtimes);
        dtFunction = 0;
    } else {
        IR_GIVE_FIELD(ir, timeDefinedByProb, _IFT_StaggeredProblem_timeDefinedByProb);
    }

    if ( dtFunction < 1 ) {
        ndomains = 0;
    }

    IR_GIVE_OPTIONAL_FIELD(ir, dtFunction, _IFT_StaggeredProblem_dtf);
    IR_GIVE_OPTIONAL_FIELD(ir, stepMultiplier, _IFT_StaggeredProblem_stepmultiplier);
    if ( stepMultiplier < 0 ) {
        OOFEM_ERROR("stepMultiplier must be > 0")
    }

    IR_GIVE_FIELD(ir, inputStreamNames [ 0 ], _IFT_StaggeredProblem_prob1);
    IR_GIVE_FIELD(ir, inputStreamNames [ 1 ], _IFT_StaggeredProblem_prob2);

    renumberFlag = true; // The staggered problem itself should always try to check if the sub-problems needs renumbering.

    coupledModels.resize(3);
    IR_GIVE_OPTIONAL_FIELD(ir, this->coupledModels, _IFT_StaggeredProblem_coupling);

    return IRRT_OK;
}


void
FluidStructureProblem :: updateAttributes(MetaStep *mStep)
{
    IRResultType result;                  // Required by IR_GIVE_FIELD macro

    InputRecord *ir = mStep->giveAttributesRecord();

    EngngModel :: updateAttributes(mStep);

    // update attributes of slaves
    for ( auto &emodel: emodelList ) {
        emodel->updateAttributes(mStep);
    }

    if ( !timeDefinedByProb ) {
        if ( ir->hasField(_IFT_StaggeredProblem_deltat) ) {
            IR_GIVE_FIELD(ir, deltaT, _IFT_StaggeredProblem_deltat);
            IR_GIVE_OPTIONAL_FIELD(ir, dtFunction, _IFT_StaggeredProblem_dtf);
            IR_GIVE_OPTIONAL_FIELD(ir, stepMultiplier, _IFT_StaggeredProblem_stepmultiplier);
            if ( stepMultiplier < 0 ) {
                OOFEM_ERROR("stepMultiplier must be > 0")
            }
        } else if ( ir->hasField(_IFT_StaggeredProblem_prescribedtimes) ) {
            IR_GIVE_FIELD(ir, discreteTimes, _IFT_StaggeredProblem_prescribedtimes);
        }
    }
}

Function *FluidStructureProblem :: giveDtFunction()
// Returns the load-time function of the receiver.
{
    if ( !dtFunction ) {
        return NULL;
    }

    return giveDomain(1)->giveFunction(dtFunction);
}

double
FluidStructureProblem :: giveDeltaT(int n)
{
    if ( giveDtFunction() ) {
        return deltaT * giveDtFunction()->evaluateAtTime(n);
    }

    //in the first step the time increment is taken as the initial, user-specified value
    if ( stepMultiplier != 1 && currentStep != NULL ) {
        if ( currentStep->giveNumber() >= 2 ) {
            return ( currentStep->giveTargetTime() * ( stepMultiplier ) );
        }
    }

    if ( discreteTimes.giveSize() > 0 ) {
        return this->giveDiscreteTime(n) - this->giveDiscreteTime(n - 1);
    }

    return deltaT;
}

double
FluidStructureProblem :: giveDiscreteTime(int iStep)
{
    if ( ( iStep > 0 ) && ( iStep <= discreteTimes.giveSize() ) ) {
        return ( discreteTimes.at(iStep) );
    }

    if ( ( iStep == 0 ) && ( iStep <= discreteTimes.giveSize() ) ) {
        return ( 0.0 );
    }

    OOFEM_ERROR("giveDiscreteTime: invalid iStep");
    return 0.0;
}

TimeStep *
FluidStructureProblem :: giveSolutionStepWhenIcApply()
{
    if ( !stepWhenIcApply ) {
        int inin = giveNumberOfTimeStepWhenIcApply();
        int nFirst = giveNumberOfFirstStep();
        stepWhenIcApply.reset(new TimeStep(inin, this, 0, -giveDeltaT(nFirst), giveDeltaT(nFirst), 0));
    }

    return stepWhenIcApply.get();
}

TimeStep *
FluidStructureProblem :: giveNextStep()
{
    int istep = this->giveNumberOfFirstStep();
    double totalTime = 0;
    StateCounterType counter = 1;

    if ( currentStep != NULL ) {
        istep = currentStep->giveNumber() + 1;
        totalTime = currentStep->giveTargetTime() + this->giveDeltaT(istep);
        counter = currentStep->giveSolutionStateCounter() + 1;
    } else {
        TimeStep *newStep = giveSolutionStepWhenIcApply();
        // first step -> generate initial step
        currentStep.reset(new TimeStep(*newStep));
    }

    previousStep = std :: move(currentStep);
    currentStep.reset(new TimeStep(istep, this, 1, totalTime, this->giveDeltaT(istep), counter));

    // time and dt variables are set eq to 0 for statics - has no meaning
    return currentStep.get();
}

void
FluidStructureProblem :: solveYourself()
{
    if ( !timeDefinedByProb ) {
        EngngModel :: solveYourself();
    } else { //time dictated by slave problem
        int imstep, jstep, nTimeSteps;
        int smstep = 1, sjstep = 1;
        MetaStep *activeMStep;
        FILE *out = this->giveOutputStream();
        this->timer.startTimer(EngngModelTimer :: EMTT_AnalysisTimer);

        EngngModel *sp = this->giveSlaveProblem(timeDefinedByProb);

        if ( sp->giveCurrentStep() ) {
            smstep = sp->giveCurrentStep()->giveMetaStepNumber();
            sjstep = sp->giveMetaStep(smstep)->giveStepRelativeNumber( sp->giveCurrentStep()->giveNumber() ) + 1;
        } else {
            nMetaSteps = sp->giveNumberOfMetaSteps();
        }

        for ( imstep = smstep; imstep <= nMetaSteps; imstep++, sjstep = 1 ) { //loop over meta steps
            activeMStep = sp->giveMetaStep(imstep);
            // update state according to new meta step in all slaves
            this->initMetaStepAttributes(activeMStep);

            nTimeSteps = activeMStep->giveNumberOfSteps();
            for ( jstep = sjstep; jstep <= nTimeSteps; jstep++ ) { //loop over time steps
                this->timer.startTimer(EngngModelTimer :: EMTT_SolutionStepTimer);
                this->timer.initTimer(EngngModelTimer :: EMTT_NetComputationalStepTimer);
                sp->giveNextStep();

                // renumber equations if necessary. Ensure to call forceEquationNumbering() for staggered problems
                if ( this->requiresEquationRenumbering( sp->giveCurrentStep() ) ) {
                    this->forceEquationNumbering();
                }

                this->initializeYourself( sp->giveCurrentStep() );
                this->solveYourselfAt( sp->giveCurrentStep() );
                this->updateYourself( sp->giveCurrentStep() );
                this->terminate( sp->giveCurrentStep() );

                this->timer.stopTimer(EngngModelTimer :: EMTT_SolutionStepTimer);
                double _steptime = this->timer.getUtime(EngngModelTimer :: EMTT_SolutionStepTimer);
                OOFEM_LOG_INFO("EngngModel info: user time consumed by solution step %d: %.2fs\n",
                               sp->giveCurrentStep()->giveNumber(), _steptime);

                fprintf(out, "\nUser time consumed by solution step %d: %.3f [s]\n\n",
                        sp->giveCurrentStep()->giveNumber(), _steptime);

#ifdef __PARALLEL_MODE
                if ( loadBalancingFlag ) {
                    this->balanceLoad( sp->giveCurrentStep() );
                }

#endif
            }
        }
    }
}

void
FluidStructureProblem :: initializeYourself(TimeStep *tStep)
{
    for ( auto &emodel: emodelList ) {
        //for ( int i = 1; i <= nModels; i++ ) {
        emodel->initializeYourself(tStep);

        DIIDynamic* dynamicProblem = dynamic_cast<DIIDynamic*>(emodel.get());
        if (dynamicProblem) {
            this->giveCurrentStep()->setTimeDiscretization(dynamicProblem->giveInitialTimeDiscretization());
        }
        NonLinearDynamic* nonlinearDynamicProblem = dynamic_cast<NonLinearDynamic*>(emodel.get());
        if (nonlinearDynamicProblem) {
            this->giveCurrentStep()->setTimeDiscretization(nonlinearDynamicProblem->giveInitialTimeDiscretization());
        }
    }
}

void
FluidStructureProblem :: solveYourselfAt(TimeStep *stepN)
{
    PFEM* pfemProblem = dynamic_cast<PFEM*>(this->giveSlaveProblem(1));
    Domain* pfemDomain = pfemProblem->giveDomain(1);

#ifdef VERBOSE
    OOFEM_LOG_RELEVANT( "Solving [step number %5d, time %e]\n", stepN->giveNumber(), stepN->giveTargetTime() );
#endif

    FloatArray currentInteractionParticlesVelocities;
    FloatArray currentInteractionParticlesPressures;
    FloatArray previousInteractionParticlesVelocities;
    FloatArray previousInteractionParticlesPressures;

    if (stepN->isTheFirstStep()) {
        int ndman = pfemDomain->giveNumberOfDofManagers();
        for (int i = 1; i <= ndman; i++) {
            if(dynamic_cast<InteractionPFEMParticle*>(pfemDomain->giveDofManager(i))) {
                interactionParticles.followedBy(i, 10);
            }
        }
    }

    currentInteractionParticlesVelocities.resize(2 * interactionParticles.giveSize());
    previousInteractionParticlesVelocities.resize(2 * interactionParticles.giveSize());
    currentInteractionParticlesPressures.resize(interactionParticles.giveSize());
    previousInteractionParticlesPressures.resize(interactionParticles.giveSize());

    double velocityDifference = 1.0;
    double pressureDifference = 1.0;

    iterationNumber = 0;
    while ( ( (velocityDifference > tol) || (pressureDifference > tol) ) && iterationNumber < 50 ) {
        previousInteractionParticlesPressures = currentInteractionParticlesPressures;
        previousInteractionParticlesVelocities = currentInteractionParticlesVelocities;

                for ( auto &emodel: emodelList ) {
        //		for ( int i = 1; i <= nModels; i++ ) {
        //			EngngModel *emodel = this->giveSlaveProblem(i);
            emodel->solveYourselfAt(stepN);
        }

        for (int i = 1; i <= interactionParticles.giveSize(); i++) {
            currentInteractionParticlesPressures.at(i) = pfemProblem->giveUnknownComponent(VM_Total, stepN, pfemDomain, pfemDomain->giveDofManager(interactionParticles.at(i))->giveDofWithID(P_f));
            InteractionPFEMParticle *interactionParticle = dynamic_cast<InteractionPFEMParticle*>(pfemDomain->giveDofManager(interactionParticles.at(i)));
            FloatArray velocities;
            interactionParticle->giveCoupledVelocities(velocities, stepN);
            currentInteractionParticlesVelocities.at(2*(i - 1) + 1) = velocities.at(1);
            currentInteractionParticlesVelocities.at(2*(i - 1) + 2) = velocities.at(2);
        }

        pressureDifference = 0.0;
        velocityDifference = 0.0;
        for (int i = 1; i <= currentInteractionParticlesPressures.giveSize(); i++) {
            pressureDifference += (currentInteractionParticlesPressures.at(i) - previousInteractionParticlesPressures.at(i))*(currentInteractionParticlesPressures.at(i) - previousInteractionParticlesPressures.at(i));
            velocityDifference += (currentInteractionParticlesVelocities.at(2*(i - 1) + 1) - previousInteractionParticlesVelocities.at(2*(i -1) + 1))*(currentInteractionParticlesVelocities.at(2*(i - 1) + 1) - previousInteractionParticlesVelocities.at(2*(i -1) + 1));
            velocityDifference += (currentInteractionParticlesVelocities.at(2*(i - 1) + 2) - previousInteractionParticlesVelocities.at(2*(i -1) + 2))*(currentInteractionParticlesVelocities.at(2*(i - 1) + 2) - previousInteractionParticlesVelocities.at(2*(i -1) + 2));
        }
        pressureDifference = sqrt(pressureDifference);
        velocityDifference = sqrt(velocityDifference);

        if (iterationNumber > 0) {
            pressureDifference /= previousInteractionParticlesPressures.computeNorm();
            velocityDifference /= previousInteractionParticlesVelocities.computeNorm();
        }

        OOFEM_LOG_RELEVANT("%3d %le %le\n", iterationNumber++, pressureDifference, velocityDifference );
    }
    if ( iterationNumber > 49 ) {
        OOFEM_ERROR("Maximal fluid-structure interface iteration count exceded");
    }
    stepN->incrementStateCounter();
}

int
FluidStructureProblem :: forceEquationNumbering()
{
    int neqs = 0;
    //    for ( int i = 1; i <= nModels; i++ ) {
    //        EngngModel *emodel = this->giveSlaveProblem(i);
        // renumber equations if necessary
    for ( auto &emodel: emodelList ) {
        if ( emodel->requiresEquationRenumbering( emodel->giveCurrentStep() ) ) {
            neqs += emodel->forceEquationNumbering();
        }
    }

    return neqs;
}

void
FluidStructureProblem :: updateYourself(TimeStep *stepN)
{
    for ( auto &emodel: emodelList ) {
        emodel->updateYourself(stepN);
    }

    EngngModel :: updateYourself(stepN);
}

void
FluidStructureProblem :: terminate(TimeStep *tStep)
{
    for ( auto &emodel: emodelList ) {
        emodel->terminate(tStep);
    }

    fflush( this->giveOutputStream() );
}

void
FluidStructureProblem :: doStepOutput(TimeStep *stepN)
{
    FILE *File = this->giveOutputStream();

    // print output
    this->printOutputAt(File, stepN);
    // export using export manager
    for ( auto &emodel: emodelList ) {
        emodel->giveExportModuleManager()->doOutput(stepN);
    }
}


void
FluidStructureProblem :: printOutputAt(FILE *File, TimeStep *stepN)
{
    for ( auto &emodel: emodelList ) {
        FILE *slaveFile = emodel->giveOutputStream();
        emodel->printOutputAt(slaveFile, stepN);
    }
}


contextIOResultType
FluidStructureProblem :: saveContext(DataStream *stream, ContextMode mode, void *obj)
{
    EngngModel :: saveContext(stream, mode, obj);
    
    for ( auto &emodel: emodelList ) {
        emodel->saveContext(stream, mode, obj);
    }

    return CIO_OK;
}


contextIOResultType
FluidStructureProblem :: restoreContext(DataStream *stream, ContextMode mode, void *obj)
{
    EngngModel :: restoreContext(stream, mode, obj);
    for ( auto &emodel: emodelList ) {
        emodel->restoreContext(stream, mode, obj);
    }

    return CIO_OK;
}


EngngModel *
FluidStructureProblem :: giveSlaveProblem(int i)
{
    if ( ( i > 0 ) && ( i <= this->giveNumberOfSlaveProblems() ) ) {
        return this->emodelList[i-1].get();
    } else {
        OOFEM_ERROR("giveSlaveProblem: Undefined problem");
    }

    return NULL;
}


int
FluidStructureProblem :: checkProblemConsistency()
{
    // check internal consistency
    // if success returns nonzero
    int result = 1;
    for ( auto &emodel: emodelList ) {
        result &= emodel->checkProblemConsistency();
    }

#  ifdef VERBOSE
    if ( result ) {
        OOFEM_LOG_DEBUG("Consistency check:  OK\n");
    } else {
        VERBOSE_PRINTS("Consistency check", "failed")
        exit(1);
    }

#  endif

    return result;
}

void
FluidStructureProblem :: updateDomainLinks()
{
    for ( auto &emodel: emodelList ) {
        emodel->updateDomainLinks();
    }
}

void
FluidStructureProblem :: setRenumberFlag()
{
    for ( auto &emodel: emodelList ) {
        emodel->setRenumberFlag();
    }
}

void
FluidStructureProblem :: preInitializeNextStep()
{
    for ( auto &emodel: emodelList ) {
        emodel->preInitializeNextStep();
    }
}

#if 0
void
FluidStructureProblem :: postInitializeCurrentStep()
{
    for ( int i = 1; i <= nModels; i++ ) {
        DIIDynamic* dynamicProblem = dynamic_cast<DIIDynamic*>(this->giveSlaveProblem(i));
        if (dynamicProblem) {
            this->giveCurrentStep()->setTimeDiscretization(dynamicProblem->giveInitialTimeDiscretization());
        }
    }
}
#endif


#ifdef __OOFEG
void FluidStructureProblem :: drawYourself(oofegGraphicContext &context)
{
    int ap = context.getActiveProblemIndx();
    if ( ( ap > 0 ) && ( ap <= giveNumberOfSlaveProblems() ) ) {
        this->giveSlaveProblem(ap)->drawYourself(context);
    }
}

void FluidStructureProblem :: drawElements(oofegGraphicContext &context)
{
    int ap = context.getActiveProblemIndx();
    if ( ( ap > 0 ) && ( ap <= giveNumberOfSlaveProblems() ) ) {
        this->giveSlaveProblem(ap)->drawElements(context);
    }
}

void FluidStructureProblem :: drawNodes(oofegGraphicContext &context)
{
    int ap = context.getActiveProblemIndx();
    if ( ( ap > 0 ) && ( ap <= giveNumberOfSlaveProblems() ) ) {
        this->giveSlaveProblem(ap)->drawNodes(context);
    }
}
#endif
} // end namespace oofem
