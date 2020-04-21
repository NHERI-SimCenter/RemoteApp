{
    "Applications": {
        "EDP": {
            "Application": "StandardEarthquakeEDP",
            "ApplicationData": {
            }
        },
        "Events": [
            {
                "Application": "StochasticGroundMotion",
                "ApplicationData": {
                    "modelName": "VlachosSiteSpecificEQ",
                    "seed": 500
                },
                "EventClassification": "Earthquake"
            }
        ],
        "Modeling": {
            "Application": "MDOF_BuildingModel",
            "ApplicationData": {
            }
        },
        "Simulation": {
            "Application": "OpenSees-Simulation",
            "ApplicationData": {
            }
        },
        "UQ": {
            "Application": "Dakota-UQ",
            "ApplicationData": {
            }
        }
    },
    "EDP": {
        "type": "StandardEarthquakeEDP"
    },
    "Events": [
        {
            "EventClassification": "Earthquake",
            "modelName": "Vlachos et al. (2018)",
            "momentMagnitude": 7,
            "ruptureDist": 30,
            "seed": 500,
            "type": "StochasticMotion",
            "vs30": 400
        }
    ],
    "GeneralInformation": {
        "depth": 360,
        "height": 144,
        "location": {
            "latitude": 37.8716,
            "longitude": -127.272
        },
        "name": "",
        "planArea": 129600,
        "stories": 1,
        "units": {
            "force": "kips",
            "length": "in",
            "temperature": "C",
            "time": "sec"
        },
        "width": 360
    },
    "Simulation": {
        "Application": "OpenSees-Simulation",
        "algorithm": "Newton",
        "analysis": "Transient -numSubLevels 2 -numSubSteps 10",
        "convergenceTest": "NormUnbalance 1.0e-2 10",
        "dampingModel": "Rayleigh Damping",
        "dampingRatio": "RV.damp",
        "dampingRatioModal": 0.02,
        "firstMode": 1,
        "integration": "Newmark 0.5 0.25",
        "modalRayleighTangentRatio": 0,
        "numModesModal": 1,
        "rayleighTangent": "Initial",
        "secondMode": 0,
        "solver": "Umfpack"
    },
    "StructuralInformation": {
        "Bx": 0.1,
        "By": 0.1,
        "Fyx": 10,
        "Fyy": 10,
        "Krz": 1000000000000000,
        "Kx": 100,
        "Ky": 100,
        "ModelData": [
            {
                "Fyx": 10,
                "Fyy": 10,
                "Ktheta": 1000000000000000,
                "bx": 0.1,
                "by": 0.1,
                "height": 144,
                "kx": 100,
                "ky": 100,
                "weight": "RV.wF"
            }
        ],
        "height": 144,
        "massX": 0,
        "massY": 0,
        "numStories": 1,
        "randomVar": [
        ],
        "responseX": 0,
        "responseY": 0,
        "type": "MDOF_BuildingModel",
        "weight": "RV.wF"
    },
    "UQ_Method": {
        "samplingMethodData": {
            "method": "LHS",
            "samples": 100,
            "seed": 989
        },
        "uqEngine": "Dakota",
        "uqType": "Sensitivity Analysis"
    },
    "WorkflowType": "Building Simulation",
    "localAppDir": "/Users/fmckenna/release/SimCenterBackendApplications/modules",
    "randomVariables": [
        {
            "distribution": "Normal",
            "mean": 150,
            "name": "wF",
            "refCount": 3,
            "stdDev": 15,
            "value": "RV.wF",
            "variableClass": "Uncertain"
        },
        {
            "distribution": "Normal",
            "mean": 0.03,
            "name": "damp",
            "refCount": 1,
            "stdDev": 0.01,
            "value": "RV.damp",
            "variableClass": "Uncertain"
        }
    ],
    "remoteAppDir": "/Users/fmckenna/release/SimCenterBackendApplications/modules",
    "runDir": "/Users/fmckenna/Documents/EE-UQ/LocalWorkDir/tmp.SimCenter",
    "runType": "local",
    "workingDir": "/Users/fmckenna/Documents/EE-UQ/LocalWorkDir"
}
