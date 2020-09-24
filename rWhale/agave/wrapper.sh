#!/bin/sh
date

# load modules
module load intel
module load dakota
module load launcher/3.1

echo "Inputs:"
echo "------"
echo "dataFiles: ${dataFiles}"
echo "configFile: ${configFile}"
echo "buildingsCount: ${buildingsCount}"

if [[ -z "${buildingsPerTask}" ]]
then
      echo "Number of buildings per task is not provided, defaulting to 10"
	  buildingsPerTask=10
else
      buildingsPerTask=${buildingsPerTask}
fi

echo "buildingsPerTask: $buildingsPerTask"

if [[ -z "${firstBuilding}" ]]
then
      echo "First building is not provided, defaulting to 1"
	  firstBuilding=1
else
      firstBuilding=${firstBuilding}
fi
echo "firstBuilding: $firstBuilding"

echo ""


echo "Current Job Directory"
pwd

date

# Copying Applications the job directory .. placing in new rWhale folder
echo ""
echo "Copying Workflow Applications"
#mkdir ./rWhale
#cp -rf /home1/00477/tg457427/SimCenterBackendApplications/v2.2.0/* ./rWhale
#cp -rf /home1/00477/tg457427/bin/OpenSees ./rWhale/applications/Workflow
#mkdir ./rWhale/applications/Workflow/python
#cp -rf /home1/00477/tg457427/python/python3.7.9/* ./rWhale/applications/Workflow/python
#cp -rf /home1/00477/tg457427/SimCenterBackendApplications/python ./rWhale/applications/Workflow

cp /home1/00477/tg457427/SimCenterBackendApplications/v2.2.0.tar.gz ./
tar zxBf v2.2.0.tar.gz
mv v2.2.0 rWhale
date

# Extracting regional simulation data
echo ""
echo "Current Job Directory"
ls -lh
for dataFile in ${dataFiles}; do
    /home1/00477/tg457427/p7zip/p7zip_16.02/bin/7za x $dataFile -orWhale/applications/Workflow/data
done

ls ./rWhale/applications/Workflow/data

date

# Copying config file
cp ${configFile} ./rWhale/applications/Workflow

#chmod -R 'a+rwx' ./rWhale

echo ""
echo "Workflow directory"
ls -lh ./rWhale/applications/Workflow

echo ""
echo "rWhale"
ls -lh ./rWhale

date

#Creating launcher tasks
echo $PWD
export PATH=$PWD/rWhale/applications/Workflow/python/bin:$PATH

python3 ./rWhale/applications/Workflow/CreateLauncherTasks.py ${buildingsCount} ${configFile} $PWD $buildingsPerTask $firstBuilding

# Copying data to local node /tmp
echo "Started copying data to compute nodes"
echo "node list"
echo $SLURM_NODELIST

date
#for node in $(scontrol show hostnames $SLURM_NODELIST) ; do
#  srun -N 1 -n 1 -w $node cp -rf $PWD/rWhale /tmp
#done
#wait


for node in $(scontrol show hostnames $SLURM_NODELIST) ; do
  srun -N 1 -n 1 -w $node tar zxBf $PWD/v2.2.0.tar.gz -C /tmp &
done
wait

for node in $(scontrol show hostnames $SLURM_NODELIST) ; do
  srun -N 1 -n 1 -w $node mv /tmp/v2.2.0 /tmp/rWhale  &
done
wait

for node in $(scontrol show hostnames $SLURM_NODELIST) ; do
  srun -N 1 -n 1 -w $node cp -fr $PWD/rWhale/applications/Workflow/data /tmp/rWhale/applications/Workflow &
done

for node in $(scontrol show hostnames $SLURM_NODELIST) ; do
  srun -N 1 -n 1 -w $node cp ${configFile} /tmp/rWhale/applications/Workflow
done
wait

echo "" 
echo "Finished copying data to compute nodes"
date

export PATH=/tmp/rWhale/applications:/tmp/rWhale/applications/Workflow::/tmp/rWhale/applications/Workflow/python/bin:$PATH

export LAUNCHER_JOB_FILE=$PWD/WorkflowTasks.txt
export LAUNCHER_SCHED=interleaved
export LAUNCHER_BIND=1
export LAUNCHER_WORKDIR=/tmp/rWhale/applications/Workflow/

echo ""
echo "WorkflowTasks"
cat $PWD/WorkflowTasks.txt

$TACC_LAUNCHER_DIR/paramrun > launcher.out 2> launcher.err

date

echo ""
echo "Aggregating Results"
python3 ./rWhale/applications/Workflow/AggregateResults.py
date

echo ""
echo "Archiving Log Files"
date
/home1/00477/tg457427/p7zip/p7zip_16.02/bin/7za a -tzip logs.zip ./logs/* 
date

echo ""
echo "Cleaning up before archiving"
for node in $(scontrol show hostnames $SLURM_NODELIST) ; do
  srun -N 1 -n 1 -w $node rm -fr /tmp/rWhale &
done
wait

rm -rf rWhale
rm -rf results
rm -rf logs
rm v2.2.0.tar.gz

date

for dataFile in ${dataFiles}; do
    rm $dataFile
done

rm ${configFile}

date
