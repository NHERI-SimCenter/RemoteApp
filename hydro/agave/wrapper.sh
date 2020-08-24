# OpenFoam 7.0 wrapper for DesignSafe

set -x
WRAPPERDIR=$( cd "$( dirname "$0" )" && pwd )

${AGAVE_JOB_CALLBACK_RUNNING}

# Load the OpenFoam module
module load intel/18.0.0
module load openfoam/7.0
source $TACC_OPENFOAM_DIR/etc/bashrc 

# cd to the input directory and run the application with the runtime
#values passed in from the job request
cd ${inputDirectory}


ls

zipFile="${openFoamDir}.zip"
unzip $zipFile
rm  $zipFile

ls

cd ${openFoamDir}

ls

v1="On"
v2="Off"

#Decomposition on/off
d1=${decomp}
echo "$d1"

#Meshing within solver on/off
m1=${mesh}
echo "$m1"

if [ "$d1" == "$v1" ] && [ "$m1" == "$v1" ]
then blockMesh > blockMesh.log; decomposePar > decomposePar.log; ibrun ${solver} -parallel > ${solver}.log; reconstructPar > reconstructPar.log
elif [ "$d1" == "$v2" ] && [ "$m1" == "$v1" ]
then blockMesh > blockMesh.log; ibrun ${solver} > ${solver}.log
elif [ "$d1" == "$v2" ] && [ "$m1" == "$v2" ] 
then ibrun ${solver} > ${solver}.log
elif [ "$d1" == "$v1" ] && [ "$m1" == "$v2" ]   
then decomposePar > decomposePar.log; ibrun ${solver} -parallel > ${solver}.log; reconstructPar > reconstructPar.log
else echo "Invalid Selection"
fi 


cd ..

if [ ! $? ]; then
        echo "OpenFoam ${solver} exited with an error status. $?" >&2
        ${AGAVE_JOB_CALLBACK_FAILURE}
        exit
fi
