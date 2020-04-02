#include <iostream>

#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>

#include <GoogleAnalytics.h>
#include <TapisCurl.h>
#include <ZipUtils.h>

#include <commonTasks.h>
static bool recursiveCopy(const QString &source,
			  const QString &target)
{
  QFileInfo sourceInfo(source);
  if (!sourceInfo.isDir()) { 
    
    // if not directory, then a file just copy
    if (!QFile::copy(source, target))
      return false;
    
  } else {
 
    // don't of course copy from HOME,DESKTOP, DOWNLOADS,
    /*
    QStandardPaths::DesktopLocation
      QStandardPaths::DocumentsLocation
      QStandardPaths::ApplicationsLocation
      QStandardPaths::HomeLocation
      QStandardPaths::DownloadLocation
    */

    //
    // source is a directory:
    //   1. make directory for target 
    //   2. now copy all files and directory in source to target
    //
    
    // mkdir target
    QDir targetDir(target);
    targetDir.cdUp();
    if (!targetDir.mkdir(QFileInfo(target).fileName())) {
      return false;
    }
    
    // get list of all files in source and copy them, by calling this function
    
    QDir sourceDir(source);
    QStringList fileNames = sourceDir.entryList(QDir::Files 
						| QDir::Dirs 
						| QDir::NoDotAndDotDot 
						| QDir::Hidden 
						| QDir::System);
    
    foreach (const QString &fileName, fileNames) {
      const QString newSrcFilePath = source + QDir::separator() + fileName;
      const QString newTgtFilePath = target + QDir::separator() + fileName;
      if (!recursiveCopy(newSrcFilePath, newTgtFilePath))
	return false;
    }
  }

  return true;
}


void runRemote(int argc, char *argv[]) {

  if (argc < 2) {
    std::cerr << "runRemote: inputFile not provided\n";
    printHelp();
  }

  QString inputFilename = argv[1];
  QFileInfo inputFile(inputFilename);

  if (!inputFile.exists()) {
    std::cerr << "input file does not exist: " << argv[1] << "\n";
    exit(-1);
  }

  QString inputFileDir = inputFile.absolutePath();
  
  //
  // get pointer to QSettings .. we are ging to need some entries
  //

  QString appName = QCoreApplication::applicationName();
  QSettings settingsCommon("SimCenter", "Common");
  QSettings settingsApplication("SimCenter", appName);

  //
  // get remote workdir and copy all input files there
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

   QString zipFile(templateDir.absoluteFilePath("templatedir.zip"));
   std::cerr << "ZIP FILE: " << zipFile.toStdString() << "\n";
   std::cerr << "DIR TO ZIP: " << templateDIR.toStdString() << "\n";

   QDir tmpDir(templateDIR);
   ZipUtils::ZipFolder(tmpDir, zipFile);

   templateDir.cd("templatedir");
   templateDir.removeRecursively();

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

   TapisCurl *theRemoteService = login(username, password);
   QString remoteDir =  theRemoteService->getHomeDirPath() + QString("/") + appName;

   std::cerr << "Uploading files .. \n";
   if (theRemoteService->uploadDirectory(copyLocation, remoteDir) != true)
     return;
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
      
   qDebug() << "JOBS_SUBMIT: " << job;

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

   return;
}


int parseArgs(int argc, char *argv[]) {

    if (argc == 1) {
        printHelp();
        return 0;
    }

    std::cout << "CURRENT ARG [" << 1 << "]: " << argv[1] << "\n";
    if (strcmp(argv[1], "--help") == 0) {
        printHelp();
    } else if (strcmp(argv[1], "--resetPreferences") == 0) {
        resetPreferences(argc-1, &argv[1]);
    } else if (strcmp(argv[1], "--printPreferences") == 0) {
        printPreferences(argc-1, &argv[1]);
    } else if (strcmp(argv[1], "--runRemote") == 0) {
        runRemote(argc-1, &argv[1]);
    } else if (strcmp(argv[1], "--listJobs") == 0) {
        listJobs(argc-1, &argv[1]);
    } else if (strcmp(argv[1], "--jobDetails") == 0) {
        jobDetails(argc-1, &argv[1]);
    } else {
      std::cout << "UNKNOWN OPTION : " << argv[1] << "\n";
      printHelp();
    }
}



int main(int argc, char *argv[])
{
  QCoreApplication a(argc, argv);
   
  QCoreApplication::setApplicationName("remoteApp");
  QCoreApplication::setOrganizationName("SimCenter");
  QCoreApplication::setApplicationVersion("2.0.1");
  //    GoogleAnalytics::SetTrackingId("UA-121636495-1");
  GoogleAnalytics::StartSession();
  GoogleAnalytics::ReportStart();
  
  parseArgs(argc, argv);

  GoogleAnalytics::EndSession();

  std::cout << "Bye!\n";
  exit(0);
}
