/* *****************************************************************************
Copyright (c) 2016-2017, The Regents of the University of California (Regents).
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.

REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED HEREUNDER IS 
PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, 
UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

*************************************************************************** */

#include "SimCenterDakota.h"

#include <iostream>
#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <ZipUtils.h>
#include <QUuid>
#include <TapisCurl.h>

/*  Implementation of SimCenterDakota: 
 *
 *  This is the task that will run the SimCenterDakota application. Its purpose is to run the
 *  dakota application at designsafe using files on local machine. The user provides the full 
 *  path to a local dakota.in file. It assumes that the directory containing the file also contains 
 *  a templatedir dir (which is what will be there if using a SimCenter UI.
 * 
 *  This class contains 1 method run which runs a remote job. To do this it:
 *   1) creates a new dir, copies dakota.in and templatedir dir there, zips up template
 *   2) copies new dir to Designsafe placing under usr/appName dir
 *   3) starts a job at designsafe
 *   4) deletes the new dir
 */

int
SimCenterDakota::runRemote(int argc, char **argv) {

  //
  // check inputs
  //

  if (argc < 2) {
    std::cerr << "runRemote: inputFile not provided\n";
    printHelp(argc, argv);
  }

  QString inputFilename = argv[1];
  QFileInfo inputFile(inputFilename);

  if (!inputFile.exists()) {
    std::cerr << "input file does not exist: " << argv[1] << "\n";
    return(-1);
  }

  QDir inputFileDir = inputFile.dir();
  QString inputFileDirPath = inputFileDir.absolutePath();

  std::cerr << "Input file location: " << inputFileDirPath.toStdString() << "\n";
    
  int count = 1;
  QString jobName("SimCenterDakota:");
  QString maxRunTime ("00:01:00");
  int numNode = 1;
  int numProcessors = 16;
  char *username = NULL;
  char *password = NULL;
  
  while (count < argc-1) { 
    if ((strcmp(argv[count],"-j") == 0) || (strcmp(argv[count],"--jobName") == 0)) {
      jobName = jobName + QString(argv[count+1]);
      count += 2;
    }
    else if ((strcmp(argv[count],"-n") == 0) || (strcmp(argv[count],"--numNode") == 0)) {
      QString input = argv[count+1];
      numNode = input.toInt();
      count += 2;
    }
    else if ((strcmp(argv[count],"-np") == 0) || (strcmp(argv[count],"--numProcessors") == 0)) {
      QString input = argv[count+1];
      numNode = input.toInt();
      count += 2;
    }
    else if ((strcmp(argv[count],"-t") == 0) || (strcmp(argv[count],"--runTime") == 0)) {
      maxRunTime = argv[count+1];
      count += 2;
    }
    else if ((strcmp(argv[count],"-u") == 0) || (strcmp(argv[count],"--user") == 0)) {
      username = argv[count+1];
      count += 2;
    }
    else if ((strcmp(argv[count],"-p") == 0) || (strcmp(argv[count],"--password") == 0)) {
      password = argv[count+1];
      count += 2;
    }
    else
      count++;
  }
  
  //
  // get pointer to QSettings .. we are ging to need some entries
  //

  QString appName = QCoreApplication::applicationName();
  QSettings settingsCommon("SimCenter", "Common");
  QSettings settingsApplication("SimCenter", appName);

  // 
  // before do any actual work .. lets login
  //

  std::cerr << "Logging in ..\n";
  login(username, password);
  QString remoteDir =  theRemoteService->getHomeDirPath() + QString("/") + appName;




  //
  // get remote workdir and copy all input files there
  //
  
  QVariant  remoteWorkdirVariant = settingsApplication.value("remoteWorkDir");
  QString workingDir;
  if (remoteWorkdirVariant.isValid()) {
    workingDir = remoteWorkdirVariant.toString();
  } else {
    this->resetPreferences(0, NULL);
    remoteWorkdirVariant = settingsApplication.value("remoteWorkDir");
    if (remoteWorkdirVariant.isValid()) {
      workingDir = remoteWorkdirVariant.toString();
    } else {
      std::cerr << "Could not create Working dir .. run --resetPreferences \n";
      return(-2);      
    }
  }
  
  QDir dirWork(workingDir);
   if (!dirWork.exists())
     if (!dirWork.mkpath(workingDir)) {
       std::cerr << "Could not create Working Dir: " << workingDir.toStdString() << "\n";
       exit(-1);
     }
   
   // create dir with unique name .. using UIUID
   QUuid uniqueName = QUuid::createUuid();
   QString strUnique = uniqueName.toString();
   strUnique = strUnique.mid(1,36);
   QString newName = QString("tmp.SimCenter") + strUnique;
   
   QString copyLocation = workingDir + QDir::separator() + newName;
   QString originalLocation = inputFile.absoluteFilePath();

   //
   // copy input file and all directories and subdir to new dir
   //

   std::cerr << "Obtaining Files From: " << inputFileDirPath.toStdString() << "\n";
   std::cerr << "& Copying files to " << copyLocation.toStdString() << "\n";

   recursiveCopy(inputFileDirPath, copyLocation);

   QDir templateDir(copyLocation);
   templateDir.cd("templatedir");
   QString templateDIR = templateDir.absolutePath();

#ifdef Q_OS_WIN
   templateDir.rename("workflow_driver.bat","workflow_driver");
#endif

   QFileInfo check_workflow(templateDir.absoluteFilePath("workflow_driver"));
   if (!check_workflow.exists() || !check_workflow.isFile()) {
     std::cerr << "no workflow_driver ";
     return -3;
   }
   templateDir.cdUp();

   QFileInfo checkDakotaIn(templateDir.absoluteFilePath("dakota.in"));
   if (!checkDakotaIn.exists() || !checkDakotaIn.isFile()) {
     std::cerr << "no dakota.in ";
     exit(-1);
   }

   QString zipFile(templateDir.absoluteFilePath("templatedir.zip"));
   std::cerr << "ZIP FILE: " << zipFile.toStdString() << "\n";
   std::cerr << "DIR TO ZIP: " << templateDIR.toStdString() << "\n";

   QDir tmpDir(templateDIR);
   ZipUtils::ZipFolder(tmpDir, zipFile);

   templateDir.cd("templatedir");
   templateDir.removeRecursively();


   std::cerr << "Uploading files .. \n";
   if (theRemoteService->uploadDirectory(copyLocation, remoteDir) != true)
     return -4;
   std::cerr << "Uploaded files .. \n";

   QJsonObject job;
   job["name"]=jobName;
   job["nodeCount"]=numNode; //QString::number(numNode);
   job["processorsPerNode"]=numProcessors; // QString::number(numNode*numProcessors); // tapis people messed up!
   // job["processorsOnEachNode"]=QString::number(numProcessors);
   job["maxRunTime"]=maxRunTime;

   QVariant  remoteAppNameVariant = settingsApplication.value("remoteTapisApp");
   QString remoteAppName;
   if (remoteAppNameVariant.isValid()) {
     remoteAppName = remoteAppNameVariant.toString();
   }
      
   job["appId"]=remoteAppName;
   job["memoryPerNode"]= "1GB";
   job["archive"]=true;
   job["archivePath"]="";
   job["archiveSystem"]="designsafe.storage.default";
      
   QJsonObject parameters;
   parameters["inputFile"]="dakota.in";
   parameters["outputFile"]="dakota.out";
   parameters["errorFile"]="dakota.err";
   parameters["driverFile"]="workflow_driver";
   parameters["modules"]="petsc,python3";

   /*
   for (auto parameterName : extraParameters.keys())
     {
       parameters[parameterName] = extraParameters[parameterName];
     }
   */

   job["parameters"]=parameters;
   QJsonObject inputs;
   remoteDir = remoteDir + QString("/") + newName;
   inputs["inputDirectory"]=remoteDir;

   /*
   for (auto inputName : extraInputs.keys())
     {
       inputs[inputName] = extraInputs[inputName];
     }
   */

   job["inputs"]=inputs;

   
   //
   // start the remote job
   //

   std::cerr << "Starting Job ... \n";

   QString result =  theRemoteService->startJob(job);

   std::cerr << result.toStdString() << "\n";
      
   // now remove the tmp directory
   // theDirectory.removeRecursively();

  // 
  // get list of jobs
  //

   return 0;
}
