#!/bin/bash
#----------------------------------------------------
# Sample Slurm job script
#   for TACC Stampede2 KNL nodes
#
#   *** Serial Job on Normal Queue ***
# 
# Last revised: 26 MAR 2020
#
# Notes:
#
#   -- Copy/edit this script as desired.  Launch by executing
#      "sbatch knl.serial.slurm" on a Stampede2 login node.
#
#   -- Serial codes run on a single node (upper case N = 1).
#        A serial code ignores the value of lower case n,
#        but slurm needs a plausible value to schedule the job.
#
#   -- For a good way to run multiple serial executables at the
#        same time, execute "module load launcher" followed
#        by "module help launcher".

#----------------------------------------------------

#SBATCH -J TrialJob22           # Job name
#SBATCH -o TrialJob22.o%j       # Name of stdout output file
#SBATCH -e TrialJob22.e%j       # Name of stderr error file
#SBATCH -p development          # Queue (partition) name
#SBATCH -N 1               # Total # of nodes (must be 1 for serial)
#SBATCH -n 1               # Total # of mpi tasks (should be 1 for serial)
#SBATCH -t 2:00:00        # Run time (hh:mm:ss)
#SBATCH --mail-user=ajaybh@berkeley.edu
#SBATCH --mail-type=all    # Send email at begin and end of job
#SBATCH -A DesignSafe-SimCenter

# Other commands must follow all #SBATCH directives...
# Load openfoam module
module load openfoam

# Create blockmesh
# blockMesh > log.blockmesh

# Set buildings
#topoSet -dict system/topoSetDict1
#subsetMesh -overwrite b01 -patch building

#topoSet -dict system/topoSetDict2
#subsetMesh -overwrite b02 -patch building

#topoSet -dict system/topoSetDict3
#subsetMesh -overwrite b03 -patch building

#topoSet -dict system/topoSetDict4
#subsetMesh -overwrite b04 -patch building

# Refine mesh
#topoSet -dict system/topoSetDict5
#refineMesh -dict system/refineMeshDict -overwrite

# Setup the waves
#setWaves > log.setWaves

# Setup the fields
#setFields > log.setfields

# Run serial code
interFoam > log.interfoam

# Run code in parallel 
#decomposePar > log.decomposepar
#ibrun interFoam -parallel > log.interform
#reconstructPar > log.reconstructpar

# ---------------------------------------------------
