/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2012, Alex Rattner
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Application
    interThermalPhaseChangeFoam

Description
    Solver for 2 incompressible, flow using a VOF (volume of fluid) phase-
    fraction based interface capturing approach.

    The momentum and other fluid properties are of the "mixture" and a single
    momentum equation is solved.

    Thermal transport and thermally driven phase change effects are implemented
    in this code.

    Turbulence modelling is generic, i.e. laminar, RAS or LES may be selected.

    For a two-fluid approach see twoPhaseEulerFoam.

    Support for the Kistler (1993) dynamic contact angle model based on earlier
    work by Edin Berberovic (2008)

\*---------------------------------------------------------------------------*/

#include "fvCFD.H"
#include "MULES.H"
#include "subCycle.H"
#include "interfaceProperties.H"
#include "twoPhaseThermalMixture.H"
#include "turbulenceModel.H"
#include "interpolationTable.H"
#include "pimpleControl.H"
#include "RiddersRoot.H"
#include "wallFvPatch.H"
#include "MeshGraph.H"
#include "thermalPhaseChangeModel.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

int main(int argc, char *argv[])
{
    #include "setRootCase.H"
    #include "createTime.H"
    #include "createMesh.H"

    pimpleControl pimple(mesh);

    #include "initContinuityErrs.H"
    #include "createFields.H"
    #include "readTimeControls.H"
    #include "correctPhi.H"
    #include "CourantNo.H"
    #include "setInitialDeltaT.H"
    #include "getCellDims.H"

    // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

    Info<< "\nStarting time loop\n" << endl;

    while (runTime.run())
    {
        #include "readTimeControls.H"
        #include "CourantNo.H"
        #include "alphaCourantNo.H"
        #include "FourierNo.H"
        #include "setDeltaT.H"
        runTime++;

        Info<< "Time = " << runTime.timeName() << nl << endl;

		//Update turbulence and two phase properties
        twoPhaseProperties.correct();

        //Update fields for Kistler model
        muEffKistler = twoPhaseProperties.mu() + rho*turbulence->nut();

        //Update phase change rates:
		phaseChangeModel->correct();

//Check alpha1 content before + after
//Info<< "****alpha1 before: " << gSum( alpha1.internalField() * mesh.V() ) << " m^3" << endl;

		//Solve for alpha1
        #include "alphaEqnSubCycle.H"

//Info<< "****alpha1 after: " << gSum( alpha1.internalField() * mesh.V() ) << " m^3" << endl;

        // --- Pressure-velocity PIMPLE corrector loop
        while (pimple.loop())
        {
            #include "UEqn.H"

            // --- PISO loop
            while (pimple.correct())
            {
                #include "pEqn.H"
            }

            if (pimple.turbCorr())
            {
                turbulence->correct();
            }
        }

Info<< "****" << endl;
Info<< "****Pressure range: " << gMax(p) - gMin(p) << " Pa" << endl;
Info<< "****Max velocity: " << gMax( mag(U.internalField()) ) << " m/s" << endl;
Info<< "****Phase change energy: " << gSum( phaseChangeModel->Q_pc()*mesh.V() ) << " W" << endl;
Info<< "****Volume change: " << gSum( phaseChangeModel->PCV() * mesh.V() ) << " m^3/s" << endl;
//Info<< "****Volume*alpha1 gen: " << gSum( phaseChangeModel->alpha1Gen() * mesh.V() ) << " m^3/s" << endl;

        //For now, the energy equation is only 1-way coupled with the momentum/pressure equations,
        //so it can be solved explicitly, and separately here
        #include "EEqn.H"

        runTime.write();

        Info<< "ExecutionTime = " << runTime.elapsedCpuTime() << " s"
            << "  ClockTime = " << runTime.elapsedClockTime() << " s"
            << nl << endl;
    }

    Info<< "End\n" << endl;

    return 0;
}


// ************************************************************************* //