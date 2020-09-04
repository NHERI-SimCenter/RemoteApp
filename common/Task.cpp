/* *****************************************************************************
Copyright (c) 2016-2020, The Regents of the University of California (Regents).
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

// Written:fmk

#include <iostream>
#include <QCoreApplication>
#include <QTimer>
#include <QSettings>
#include <QString>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QUuid>
#include <GoogleAnalytics.h>

#include <Task.h>
#include <TapisCurl.h>


Task::Task(QString &id) {
    theRemoteService = NULL;
    appID = id;
}


Task::~Task() {
    if (theRemoteService != NULL)
        delete theRemoteService;
}


int 
Task::printMessage(QString &message) {
    std::cerr << message.toStdString() << "\n";
    return 0;
}

int 
Task::recursiveCopy(const QString &source,
                    const QString &target)
{

  QFileInfo sourceInfo(source);

  if (!sourceInfo.isDir()) {

   // std::cerr << "FILE:\n";
   // std::cerr << "source: " << source.toStdString() << " target: " << target.toStdString() << "\n";

    // if not directory, then a file just copy
    if (!QFile::copy(source, target))
      return -1;
    else
      return 0;
    
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
    //std::cerr << "DIR: \n";
    //std::cerr << "source: " << source.toStdString() << " target: " << target.toStdString() << "\n";

    QDir targetDir(target);
    targetDir.cdUp();
    if (!targetDir.mkdir(QFileInfo(target).fileName())) {
      return -1;
    }
    
    // get list of all files in source and copy them, by calling this function
    
    QDir sourceDir(source);
    std::cerr << "sourceDir path: " << sourceDir.absolutePath().toStdString() << "\n";

    QStringList fileNames = sourceDir.entryList(QDir::Files
						| QDir::Dirs
						| QDir::NoDotAndDotDot);
    
    //qDebug() << fileNames;

    foreach (const QString &fileName, fileNames) {
      std::cerr << "\tcopying: " << fileName.toStdString() << "\n";
      const QString newSrcFilePath = source + QDir::separator() + fileName;
      const QString newTgtFilePath = target + QDir::separator() + fileName;
      if (recursiveCopy(newSrcFilePath, newTgtFilePath) != 0)
	return -1;
    }
  }
  
  return 0;
}

int
Task::runRemote(int argc, char *argv[]) {
  std::cerr << "Basic Task does not run anything - method need to be overwritten";
}

int
Task::resetPreferences(int argc, char *argv[]) {

    QString appName = QCoreApplication::applicationName();
    qDebug() << "APP NAME: " << appName;

    QSettings settingsCommon("SimCenter", "Common");
    QSettings settingsApplication("SimCenter", appName);

    settingsApplication.setValue("remoteTapisApp", appID);

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

    int count = 0;
    char *username = NULL;
    char *password = NULL;

    while (count < argc-1) {
        if ((strcmp(argv[count],"-u") == 0) || (strcmp(argv[count],"--user") == 0)) {
            QString username(argv[count+1]);
            settingsCommon.setValue("login", username);
            count += 2;
        }
        else if ((strcmp(argv[count],"-p") == 0) || (strcmp(argv[count],"--password") == 0)) {
            QString password(argv[count+1]);
            settingsCommon.setValue("password", password);
            count += 2;
        }
        else if ((strcmp(argv[count],"-d") == 0) || (strcmp(argv[count],"--appDir") == 0)) {
            QString dir(argv[count+1]);
            QDir theDir(dir);
            if (!theDir.exists()) {
                std::cerr << "directory : " << dir.toStdString() << " does not exist! No appdir set\n";
            } else {
                settingsApplication.setValue("appDir", theDir.absolutePath());
            }
            count += 2;
        }
        else if ((strcmp(argv[count],"-i") == 0) || (strcmp(argv[count],"--appID") == 0)) {
            QString id(argv[count+1]);
            appID = id;
            settingsApplication.setValue("remoteTapisApp", appID);
            count += 2;
        }
        else
            count += 1;
    }

    return 0;
}

int
Task::printPreferences(int argc, char *argv[]) {

    QString appName = QCoreApplication::applicationName();

    std::cout << "Preferences for Application: " << appName.toStdString() << "\n";

    QSettings settingsCommon("SimCenter", "Common");
    QSettings settingsApplication("SimCenter", appName);

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
    return 0;
}

int
Task::login(char *name, char *password) {

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
    QString dirName = QCoreApplication::applicationName();

    theRemoteService = new TapisCurl(tenant, storage, &dirName);

    bool loggedIn = theRemoteService->login(username, userpassword);
    if (loggedIn == false) {
        std::cerr << "Could not login using username and password\n";
        return -1;
    }
    
    std::cerr << "Logged in .. \n";

    return 0;
}

int
Task::listJobs(int argc, char *argv[]) {

    login();

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

    return 0;
}

int
Task::jobDetails(int argc, char *argv[]) {

    if (argc < 2) {
        std::cerr << "incorrect usage, no jobID provided\n";
        exit(-1);
    }

    QString jobID(argv[1]);

    login();

    //
    // get job details
    //

    QJsonObject jobs = theRemoteService->getJobDetails(jobID);
    QJsonDocument doc(jobs);
    QString strJson(doc.toJson(QJsonDocument::Compact));
    std::cout << strJson.toStdString();

    return 0;
}

int
Task::printHelp(int argc, char *argv[]) {
    std::cout << "--help               print this message\n";
    std::cout << "--runRemote          run job remotely\n";
    std::cout << "--listJobs           print list stored jobs\n";
    std::cout << "--jobDetails jobID?  print deatils of job\n";
    std::cout << "--printPreferences   print preferences\n";
    std::cout << "--resetPreferences   reset default preferences\n";
    std::cout << "--printPreferences   print preferences\n";
    return 0;
}

int
Task::parseCommand(int argc, char *argv[]) {

    if (argc == 1) {
        printHelp(0, NULL);
        return 0;
    }

    std::cout << "CURRENT ARG [" << 1 << "]: " << argv[1] << "\n";
    if (strcmp(argv[1], "--help") == 0) {
        printHelp(argc-1, &argv[1]);
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
        printHelp(0, NULL);
    }

    return 0;
}

