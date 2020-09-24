To run the workflow:

1. Create an rWHALE folder. I'll assume you put in in C:/rWHALE

2. Get the SimCenter Backend Applications, build them, and put the resulting applications folder under rWHALE.

3. Extract the input_data.zip file under rWHALE. Now you should have a C:/rWHALE/input_data folder.

4. Put the rWHALE_Atlantic_City.json file - the configuration file - under rWHALE

4.1 (If you have a Mac or you want to put the rWHALE folder somewhere else)
Open the configuration file and update the location of the rWHALE foder under "localAppDir"

5 Run the simulation workflow by executing the following command in the terminal

python "C:/rWHALE/applications/Workflow/RDT_workflow.py" "C:/rWHALE/rWHALE_Atlantic_City.json" --registry "C:/rWHALE/applications/Workflow/WorkflowApplications.json" --referenceDir "C:/rWHALE/input_data/" -w "C:/rWHALE/results"

6 After running the simulation, you shoud have a C:/rWHALE/results folder with 3 CSV files in there that contain the main results.