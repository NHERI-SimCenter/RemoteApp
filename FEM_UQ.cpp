#include "FEM_UQ.h"

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
#include <QDebug>

bool
FEM_UQ::runRemote(int argc, char **argv) {

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
    exit(-1);
  }

  QString inputFileDir = inputFile.absolutePath();

  // FMK provide code to check input file not in 1 of statndard dir, HOME< DOwnloads, Documents, ...
  
  //
  // get pointer to QSettings .. we are ging to need some entries
  //

  QString appName = QCoreApplication::applicationName();
  QSettings settingsCommon("SimCenter", "Common");
  QSettings settingsApplication("SimCenter", appName);

  //
  // parse args
  //
  
  int count = 1;
  QString jobName("EE_UQ-test");
  QString maxRunTime ("00:01:00");
  int numNode = 1;
  int numProcessors = 16;
  char *username = NULL;
  char *password = NULL;
  
  while (count < argc-1) { 
    if ((strcmp(argv[count],"-j") == 0) || (strcmp(argv[count],"--jobName") == 0)) {
      jobName = argv[count+1];
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

  // try logging in .. app quits if this fails
  login(username, password);
  
  //
  // Lets start .. get remote workdir and copy all input files there
  //
  
  QVariant  remoteWorkdirVariant = settingsApplication.value("remoteWorkDir");
  QString workingDir;
  if (remoteWorkdirVariant.isValid()) {
    workingDir = remoteWorkdirVariant.toString();
  }
  
  QDir dirWork(workingDir);
  if (!dirWork.exists()) 
    if (!dirWork.mkpath(workingDir)) {
      std::cerr << "Could not create Working Dir: " << workingDir.toStdString() << "\n";
      exit(-1);
    }
   
  //
  // create dir with unique name .. using UUIDs
  //

   QUuid uniqueName = QUuid::createUuid();
   QString strUnique = uniqueName.toString();
   strUnique = strUnique.mid(1,36);
   QString newName = QString("tmp.SimCenter") + strUnique;
   
   QString copyLocation = workingDir + QDir::separator() + newName;
   QString originalLocation = inputFile.absoluteFilePath();

   //
   // copy input file and all directories and subdir to new dir
   //
   
   recursiveCopy(inputFileDir, copyLocation);

   QDir templateDir(copyLocation);
   templateDir.cd("templatedir");
   QString templateDIR = templateDir.absolutePath();

#ifdef Q_OS_WIN
   templateDir.rename("workflow_driver.bat","workflow_driver");
#endif

   QFileInfo check_workflow(templateDir.absoluteFilePath("workflow_driver"));
   if (!check_workflow.exists() || !check_workflow.isFile()) {
     std::cerr << "no workflow_driver ";
     exit(-1);
   }
   templateDir.cdUp();

   QFileInfo checkDakotaIn(templateDir.absoluteFilePath("dakota.in"));
   if (!checkDakotaIn.exists() || !checkDakotaIn.isFile()) {
     std::cerr << "no dakota.in ";
     exit(-1);
   }

   //
   // zip up templatedir into templatedir.zip and remove template dir
   //

   QString zipFile(templateDir.absoluteFilePath("templatedir.zip"));
   //std::cerr << "ZIP FILE: " << zipFile.toStdString() << "\n";
   //std::cerr << "DIR TO ZIP: " << templateDIR.toStdString() << "\n";

   QDir tmpDir(templateDIR);
   ZipUtils::ZipFolder(tmpDir, zipFile);

   templateDir.cd("templatedir");
   templateDir.removeRecursively();

   QString remoteDir =  theRemoteService->getHomeDirPath() + QString("/") + appName;

   std::cerr << "Uploading files .. \n";
   if (theRemoteService->uploadDirectory(copyLocation, remoteDir) != true)
     return false;
   std::cerr << "Uploaded files .. \n";

   QJsonObject job;
   job["name"]=jobName;
   job["nodeCount"]=numNode; //QString::number(numNode);
   job["processorsPerNode"]=numProcessors; // QString::number(numNode*numProcessors); // tapis people messed up!
   // job["processorsOnEachNode"]=QString::number(numProcessors);
   job["maxRunTime"]=maxRunTime;

   QVariant remoteAppNameVariant = settingsApplication.value("remoteTapisApp");
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
      
   qDebug() << "JOBS_SUBMIT: " << job;

   //
   // start the remote job
   //

   std::cerr << "Starting Job ... \n";

   QString result =  theRemoteService->startJob(job);

   std::cerr << result.toStdString() << "\n";


   //
   // remove tmp directory
   //

   QDir copyDir(copyLocation);      
   copyDir.removeRecursively();

   return true;
}




