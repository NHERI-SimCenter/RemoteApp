#include <iostream>

#include <QCoreApplication>
#include <QTimer>
#include <QSettings>
#include <QString>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>

#include <GoogleAnalytics.h>
#include <TapisCurl.h>

void printMessage(QString &message) {
    std::cerr << message.toStdString() << "\n";
}

bool recursiveCopy(const QString &source,
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

void resetPreferences(int argc, char *argv[]) {

   QString appName = QCoreApplication::applicationName();

   QSettings settingsCommon("SimCenter", "Common");
   QSettings settingsApplication("SimCenter", appName);

   QString remoteAppName = QString("simcenter-dakota-1.0.0u1");
   settingsApplication.setValue("remoteTapisApp", remoteAppName);

#ifdef Q_OS_WIN
   QStringList paths{QCoreApplication::applicationDirPath().append("/applications/python")};
   QString pythonPath = QStandardPaths::findExecutable("python.exe", paths);
   if(pythonPath.isEmpty())
     pythonPath = QStandardPaths::findExecutable("python.exe");
#else
   QString pythonPath = QStandardPaths::findExecutable("python3");
   if (pythonPath.isEmpty()) {
     QFileInfo localPython3("/usr/local/bin/python3");
     if (localPython3.exists())
       pythonPath = localPython3.filePath();
     else
       pythonPath = QStandardPaths::findExecutable("python");
   }
#endif
   settingsCommon.setValue("pythonExePath", pythonPath);

   QDir workingDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
   QString remoteWorkDirLocation = workingDir.filePath(QCoreApplication::applicationName() + "/RemoteWorkDir");
   settingsApplication.setValue("remoteWorkDir", remoteWorkDirLocation);

   QString remoteAppDirLocation = QString("/home1/00477/tg457427/SimCenterBackendApplications/Mar-2020");
   settingsApplication.setValue("remoteAppDir", remoteAppDirLocation);
}

void printPreferences(int argc, char *argv[]) {

    std::cout << "Preferences for Application: " << QCoreApplication::applicationName().toStdString() << "\n";
    QString appName = QCoreApplication::applicationName();
    QSettings settingsCommon("SimCenter", "Common");
    QSettings settingsApplication("SimCenter", QCoreApplication::applicationName());

    QVariant  pythonVariant = settingsCommon.value("pythonExePath");
    QString python;
    if (pythonVariant.isValid()) {
        python = pythonVariant.toString();
    }
    std::cout << "\tpythonExePath: " << python.toStdString() << "\n";

    QVariant  remoteWorkdirVariant = settingsApplication.value("remoteWorkDir");
    QString remoteWorkdir;
    if (remoteWorkdirVariant.isValid()) {
        remoteWorkdir = remoteWorkdirVariant.toString();
    }
    std::cout << "\tremoteWorkDir: " << remoteWorkdir.toStdString() << "\n";

    QVariant  remoteAppNameVariant = settingsApplication.value("remoteTapisApp");
    QString remoteAppName;
    if (remoteAppNameVariant.isValid()) {
        remoteAppName = remoteAppNameVariant.toString();
    }

    std::cout << "\tremoteAppName: " << remoteAppName.toStdString() << "\n";

    QVariant  loginName = settingsCommon.value("login");
    QVariant  loginPassword = settingsCommon.value("password");
    if (loginName.isValid()) {
      std::cout << "\tusername: " << loginName.toString().toStdString() << "\n";
    }
    if (loginPassword.isValid()) {
      std::cout << "\tpassword: " << loginPassword.toString().toStdString() << "\n";
    }
}

TapisCurl *login(char *name = NULL, char *password = NULL) {
  //
  // from settings get exe name, username and password
  //

  QString appName = QCoreApplication::applicationName();
  QSettings settingsCommon("SimCenter", "Common");
  QSettings settingsApplication("SimCenter", appName);

  QString username;
  QString userpassword;

  if (name == NULL || password == NULL) {
    QVariant  loginName = settingsCommon.value("login");
    QVariant  loginPassword = settingsCommon.value("password");
    if (loginName.isValid()) {
      username = loginName.toString();
    }
    if (loginPassword.isValid()) {
      userpassword = loginPassword.toString();
    }
  } else {
    username = name;
    userpassword = password;
  }
  
  //
  // create a Service & log in
  //

  QString tenant("designsafe");
  QString storage("agave://designsafe.storage.default/");
  QString dirName("remoteApp");

  TapisCurl *theRemoteService = new TapisCurl(tenant, storage, &dirName);

  bool loggedIn = theRemoteService->login(username, userpassword);
  if (loggedIn == false) {
    std::cerr << "Could not login using username and password\n";
    exit(-1);
  } 
    
  std::cerr << "Logged in .. \n";


  return theRemoteService;
}

void listJobs(int argc, char *argv[]) {

  TapisCurl *theRemoteService = login();

  // 
  // get list of jobs
  //

   QString appName = QCoreApplication::applicationName();  
   QJsonObject jobs = theRemoteService->getJobList(appName);

  if (jobs.contains("jobs")) {
    QJsonArray jobData=jobs["jobs"].toArray();
    int numJobs = jobData.count();

    for (int i=0; i<numJobs; i++) {
      QJsonObject job=jobData.at(i).toObject();
      QString jobID = job["id"].toString();
      QString jobName = job["name"].toString();
      QString jobStatus = job["status"].toString();
      QString jobDate = job["created"].toString();
      std::cout << jobID.toStdString() << " " << jobName.toStdString() << " " << jobStatus.toStdString() << " " << jobDate.toStdString() << "\n";
    }
  }  

  delete theRemoteService;
}

void jobDetails(int argc, char *argv[]) {

  if (argc < 2) {
    std::cerr << "incorrect usage, no jobID provided\n";
    exit(-1);
  }

  QString jobID(argv[1]);

  TapisCurl *theRemoteService = login();

  // 
  // get job details
  //
  
  QJsonObject jobs = theRemoteService->getJobDetails(jobID);
  QJsonDocument doc(jobs);
  QString strJson(doc.toJson(QJsonDocument::Compact));
  std::cout << strJson.toStdString();

  delete theRemoteService;
}

void printHelp(void) {
   std::cout << "--help               print this message\n";
   std::cout << "--runRemote          run job remotely\n";
   std::cout << "--listJobs           print list stored jobs\n";
   std::cout << "--jobDetails jobID?  print deatils of job\n";
   std::cout << "--printPreferences   print preferences\n";
   std::cout << "--resetPreferences   reset default preferences\n";
   std::cout << "--printPreferences   print preferences\n";
}
