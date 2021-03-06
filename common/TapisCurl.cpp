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

// Written: fmckenna
#include "TapisCurl.h"

#include <QProcess>
#include <QDebug>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QJsonObject>


#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <iterator>
#include <string>

#include <QUuid>
#include <QFuture>
#include <QStandardPaths>
#include <QDir>

using namespace std;

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

TapisCurl::TapisCurl(QString &_tenant, QString &_storage, QString *appDir, QObject *parent)
    :RemoteService(parent), tenant(_tenant), storage(_storage), loggedInFlag(false), slotNeededLocally(false)
{
    //
    // for operation this class needs two temporary files to function
    //  - hence thing1 and thing2

    //Get application data folder
    QString writableLocation = QStandardPaths::writableLocation(QStandardPaths::StandardLocation::AppLocalDataLocation);

    //Create the folder if it does not exist
    QDir writableDir(writableLocation);
    if(!writableDir.exists())
        writableDir.mkpath(".");

    uniqueFileName1 = writableDir.filePath("SimCenter.thing1");
    uniqueFileName2 = writableDir.filePath("SimCenter.thing2");

    //
    // init curl variables
    //

    hnd = curl_easy_init();
    slist1 = NULL;
    slist2 = NULL;

    tenantURL="https://agave.designsafe-ci.org/";
    appClient = QString("appClient");

    if (appDir != nullptr)
        appDirName = QString(*appDir);
}

TapisCurl::~TapisCurl()
{
    //
    // clean up, remove temp files, delete QProcess and delete login
    //  - deleted login as never set the widgets parent window

    // if we have logged in .. delete the client app


    if (loggedInFlag == true) {

        QString url = tenantURL + QString("clients/v2/") + appClient;
        QString user_passwd = username + QString(":") + password;

        // note should not use emit as object which was sending deleted before this one
        curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
        curl_easy_setopt(hnd, CURLOPT_USERPWD, user_passwd.toStdString().c_str());
        curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "DELETE");
        this->invokeCurl();
    }

    curl_slist_free_all(slist1);
    slist1 = NULL;

    //
    // remove temporary files
    //

    QFile file1 (uniqueFileName1);
    file1.remove();
    
    QFile file2 (uniqueFileName2);
    file2.remove();

    //
    // finally invoke cleanup
    //

    curl_easy_cleanup(hnd);
}

bool 
TapisCurl::isLoggedIn()
{
    return loggedInFlag;
}


void
TapisCurl::loginCall(QString uname, QString upassword)
{
    bool result = this->login(uname, upassword);
    emit loginReturn(result);
    if (result == true) {
        QString result = storage + username;
        if (!appDirName.isEmpty())
            result = result + "/" + appDirName;

        emit getHomeDirPathReturn(result);
    }
}

bool
TapisCurl::login(QString uname, QString upassword)
{
    username = uname;
    password = upassword;

    qDebug() << "username: " << username;

    QString consumerSecret;
    QString consumerKey;

    curl_slist_free_all(slist1);
    slist1 = NULL;

    //
    // first try deleting old app, needed if program crashed or old not deleted
    // before thread was shutdown
    //

    QString message = QString("Contacting ") + tenant + QString(" to delete any old clients ");
    emit statusMessage(message);

    QString url = tenantURL + QString("clients/v2/") + appClient;
    QString user_passwd = username + QString(":") + password;

    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_USERPWD, user_passwd.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "DELETE");

    if (this->invokeCurl() == false) {
        return false;
    }

    //
    // first try creating a client with username & password
    // with purpose of getting the two keys: consumerSecret and consumerKey
    //

    message = QString("Contacting ") + tenant + QString(" to create new client ");
    emit statusMessage(message);

    url = tenantURL + QString("clients/v2/?pretty=true");

    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERPWD, user_passwd.toStdString().c_str());

    QString postField = QString("clientName=") + appClient + QString("&tier=Unlimited&description=client for app development&callbackUrl=");
    int postFieldLength = postField.length() ; // strlen(postFieldChar);
    char *pField = new char[postFieldLength+1]; // have to do new char as seems to miss ending string char when pass directcly
    strncpy(pField, postField.toStdString().c_str(),postFieldLength+1);
    
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, pField);
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)postFieldLength);
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
    
    if (this->invokeCurl() == false) {
        return false;
    }

    // process curl results .. open results file
    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: Logging in COULD NOT OPEN RESULT FILE!");
        return false;
    }

    // read results file & check for errors
    QString val;
    val=file.readAll();
    file.close();
    
    // process results into json object and get the results object whhich contains the two keys
    if (val.isEmpty()) {
        emit errorMessage(QString("ERROR - libCurl ERROR!"));
        return false;
    }

    bool retry = true;
    if (val.contains("error")) {
        QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
        QJsonObject jsonObj = doc.object();
        if (jsonObj.contains("message")) {
            QString error =  jsonObj["message"].toString();
            emit errorMessage(QString("ERROR: ") + QString(error));
            return false;
        } else {
            QString error("ERROR - Logging in .. type unknown .. please report");
            emit errorMessage(error);
            return false;
        }
    } else if (val.contains("success")) {
        QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
        QJsonObject jsonObj = doc.object();
        if (jsonObj.contains("result")) {
            QJsonObject resultObj =  jsonObj["result"].toObject();
            if (resultObj.contains("consumerKey"))
                consumerKey = resultObj["consumerKey"].toString();
            if (resultObj.contains("consumerSecret"))
                consumerSecret = resultObj["consumerSecret"].toString();
        }
        // some processing to remove some crap from frnt and end of strings
        consumerKey.remove("\"");
        consumerSecret.remove("\"");
        retry = false;
    }
    Q_UNUSED(retry);


    //
    // given keys we now want to create the n Oauth bearer token .. good for 4 hours
    //

    // invoke curl

    message = QString("Contacting ") + tenant + QString(" to get authorization token ");
    emit statusMessage(message);

    QString combined = consumerKey + QString(":") + consumerSecret;
    url = tenantURL + QString("token");
    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_USERPWD, combined.toStdString().c_str());
    QString postField2 = QString("username=") + username + QString("&password=") + password +
            QString("&grant_type=password&scope=PRODUCTION");
    int postFieldLength2 = postField2.length() ; // strlen(postFieldChar);
    char *pField2 = new char[postFieldLength2+1]; // have to do new char as seems to miss ending string char when pass directcly
    strncpy(pField2, postField2.toStdString().c_str(),postFieldLength2+1);
    
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, pField2);
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)postFieldLength2);
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
    if (this->invokeCurl() == false) {
        return false;
    }
    

    // process result .. we want that access token
    // when we have it we add to slist1 variable which is used in all the subsequent curl calls
    // we also mark loggedInFlag as true for subsequent calls to logn

    QFile file2(uniqueFileName1);
    if (!file2.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: COULD NOT OPEN RESULT of OAuth");
        return false;
    }
    
    // read results file & check for errors
    val=file2.readAll();
    file2.close();

    QString accessToken;
    
    if (val.contains("Invalid Credentals") || val.isEmpty()) {
        emit errorMessage("ERROR: Invalid Credentials in OAuth!");
        return false;
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
        QJsonObject jsonObj = doc.object();
        if (jsonObj.contains("access_token")) {
            accessToken = jsonObj["access_token"].toString();
            bearer = QString("Authorization: Bearer ") + accessToken;
            slist1 = curl_slist_append(slist1, bearer.toStdString().c_str());
            loggedInFlag = true;

            // now if appDir is specified make sure dir exists,
            // creating it does delete existing one and one call as opposed to 2 if not there
            if (!appDirName.isEmpty()) {
                QString home = storage + username;
                bool ok = this->mkdir(appDirName, username);
                if (ok != true) {
                    QString message = QString("WARNING - could not create " ) + appDirName
                            + QString("on login, using home dir instead");
                    appDirName = QString(""); // no erase function!
                    emit statusMessage(message);
                } else {
                    emit statusMessage("Login SUCCESS");
                }
            } else {
                emit statusMessage("Login SUCCESS");
            }

            return true;
        }
        emit statusMessage("ERROR - no access token returned!");
        return false;
    }
    return false;

    /* *************************************************** to test aloe
    consumerKey = QString("");
    consumerSecret = QString("");
    QString accessToken = QString("");
    bearer = QString("Authorization: Bearer ") + accessToken;
    slist1 = curl_slist_append(slist1, bearer.toStdString().c_str());
    loggedInFlag = true;
    emit statusMessage("SUCCESS fmk");
    return true;
    **************************************************************** */
}

void
TapisCurl::logoutCall()
{
    bool result = this->logout();
    emit logoutReturn(result);
}

bool
TapisCurl::logout()
{
    if (loggedInFlag == false)
        return true;

    QString url = tenantURL + QString("clients/v2/") + appClient;
    QString user_passwd = username + QString(":") + password;

    emit statusMessage("STATUS: contacting Tapis to delete remote client app");

    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_USERPWD, user_passwd.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "DELETE");
    bool ok = this->invokeCurl();

    if (ok == false) {
        emit statusMessage("ERROR: Failed to invokeCurl when deleting remote client app");
    } else {
        emit statusMessage("STATUS: contacting Tapis to revoke auth tokens");
    }

    curl_slist_free_all(slist1);
    slist1 = NULL;

    /* DOES Not appear to be needed .. need to set correct URL
    //QString command = QString("auth-tokens-revoke > ")  + uniqueFileName1;
    //this->invokeCurl();
    */

    emit statusMessage("Logout SUCCESS");

    loggedInFlag = false;

    return true;
}


void
TapisCurl::getHomeDirPathCall(void) {
    QString result = storage + username;
    emit getHomeDirPathReturn(result);
}

QString
TapisCurl::getHomeDirPath(void) {
    QString result = storage + username;
    return result;
}


void
TapisCurl::uploadDirectoryCall(const QString &local, const QString &remote)
{
    bool result = this->uploadDirectory(local, remote);
    emit uploadDirectoryReturn(result);
}

bool
TapisCurl::uploadDirectory(const QString &local, const QString &remote)
{
    //
    // check local exists
    //

    QDir originDirectory(local);
    if (! originDirectory.exists()) {
        emit errorMessage(QString("ERROR - local directory does not exist : ") + local);
        std::cerr << "ERROR - local directory does not exist : "  + local.toStdString();
        return false;
    }
    QString dirName = originDirectory.dirName();

    //
    // create remote dir
    //

    QString remoteCleaned = remote;
    remoteCleaned.remove(storage);
    if (this->mkdir(dirName, remoteCleaned) != true) {
        std::cerr << "Could not create dir: " << dirName.toStdString() << " in " << remoteCleaned.toStdString() << "\n";
        return false;
    }
    remoteCleaned = remoteCleaned + QString("/") + dirName;

    //
    // now we go through each file in local dir & upload to new remote directory
    //   - if dir we recursivily call the method
    //

    // originDirectory.mkpath(destinationDir);

    foreach (QString directoryName, originDirectory.entryList(QDir::Dirs | \
                                                              QDir::NoDotAndDotDot))
    {
        QString nextRemote = remoteCleaned + "/" + directoryName;
        QString nextLocal = local + QDir::separator() + directoryName;

        if (this->uploadDirectory(nextLocal, remoteCleaned) != true) {
            this->removeDirectory(remoteCleaned); // remove any directory we created if failure
            return false;
        }
    }

    foreach (QString fileName, originDirectory.entryList(QDir::Files))
    {
        QString localFile = local + QDir::separator() + fileName;
        QString remoteFile = remoteCleaned + "/" + fileName;
        std::cerr << "\tuploading file: " << localFile.toStdString() << "\n";
        if (this->uploadFile(localFile, remoteFile) != true) {
            this->removeDirectory(remoteCleaned);
            return false;
        }
    }

    return true;
}


void
TapisCurl::removeDirectoryCall(const QString &remote) {
    bool result = this->removeDirectory(remote);
    emit removeDirectoryReturn(result);
}

bool
TapisCurl::removeDirectory(const QString &remote)
{
    QString message = QString("Contacting ") + tenant + QString(" to remove dir ") + remote;
    emit statusMessage(message);

    // invoke curl to remove the file or directory
    QString url = tenantURL + QString("files/v2/media/") + remote;
    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "DELETE");
    if (this->invokeCurl() == false) {
        return false;
    }

    //
    // process the results
    //

    // open results file
    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: removeDirectory .. COULD NOT OPEN RESULT .. libCurl ERROR");
        return false;
    }

    // read results file & check for errors
    QString val;
    val=file.readAll();
    file.close();

    // read into json object
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();

    // parse json object for status
    //   if error emit error message & return NOT FOUND, if no status it's an error
    //   if success then get result & return the job sttaus

    QString status;
    if (theObj.contains("status")) {
        status = theObj["status"].toString();
        if (status == "error") {

            // if error get errormessage and return error
            QString message("Job Not Found");
            if (theObj.contains("message"))
                message = theObj["message"].toString();

            emit errorMessage(message);
            return false;

        } else if (status == "success") {
            emit errorMessage("Succesfully removed directory");
            return true;
        }

    } else if (theObj.contains("fault")) {
        QJsonObject theFault = theObj["fault"].toObject();
        if (theFault.contains("message")) {
            QString message = theFault["message"].toString();
            emit errorMessage(message);
            return false;
        }
    }
    return false;
}

void
TapisCurl::mkdirCall(const QString &remoteName, const QString &remotePath) {
    bool result = mkdir(remoteName, remotePath);
    emit mkdirReturn(result);
}

bool
TapisCurl::mkdir(const QString &remoteName, const QString &remotePath) {

    QString message = QString("Contacting ") + tenant + QString(" to create dir ") + remotePath + QString("/") + remoteName;
    emit statusMessage(message);

    bool result = false;

    QString url = tenantURL + QString("files/v2/media/") + remotePath;

    QString postField = QString("action=mkdir&path=") + remoteName;
    int postFieldLength = postField.length() ; // strlen(postFieldChar);
    char *pField = new char[postFieldLength+1]; // have to do new char as seems to miss ending string char when pass directcly
    strncpy(pField, postField.toStdString().c_str(),postFieldLength+1);

    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, pField);
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)postFieldLength);
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "PUT");

    if (this->invokeCurl() == false) {
        return false;
    }

    //
    // process the results
    //

    // open results file
    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: removeDirectory .. COULD NOT OPEN RESULT");
        return false;
    }

    // read results file & check for errors
    QString val;
    val=file.readAll();
    file.close();

    // read into json object
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();

    // parse json object for status
    //   if error emit error message & return NOT FOUND, if no status it's an error
    //   if success then get result & return the job sttaus

    QString status;
    if (theObj.contains("status")) {
        status = theObj["status"].toString();
        if (status == "error") {
            QString message("Job Not Found");
            if (theObj.contains("message"))
                message = theObj["message"].toString();
            emit errorMessage(message);
            return false;
        } else if (status == "success") {
            return true;
        }
    } else {
        QJsonDocument doc(theObj);
        QString strJson(doc.toJson(QJsonDocument::Compact));
        emit errorMessage(strJson);
        return false;
    }

    return result;
}

void
TapisCurl::uploadFileCall(const QString &local, const QString &remote) {
    bool result = this->uploadFile(local, remote);
    emit uploadFileReturn(result);
}

bool
TapisCurl::uploadFile(const QString &local, const QString &remote) {

    QFileInfo localNameInfo(local);
    QString localName = localNameInfo.fileName();

    QString message = QString("Contacting ") + tenant + QString(" to upload file ") + localName;
    emit statusMessage(message);

    //
    // obtain filename and remote path from the remote string
    // note: for upload we need to remove the agave storage URL if there
    //

    QString remoteCleaned = remote;
    remoteCleaned.remove(storage);
    QFileInfo   fileInfo(remoteCleaned);
    QString remoteName = fileInfo.fileName();
    QString remotePath = fileInfo.path();

    // invoke curl to upload the file or directory
    struct curl_httppost *post1;
    struct curl_httppost *postend;

    post1 = NULL;
    postend = NULL;
    curl_formadd(&post1, &postend,
                 CURLFORM_COPYNAME, "fileToUpload",
                 CURLFORM_FILE, local.toStdString().c_str(),
                 CURLFORM_CONTENTTYPE, "text/plain",
                 CURLFORM_END);
    curl_formadd(&post1, &postend,
                 CURLFORM_COPYNAME, "fileName",
                 CURLFORM_COPYCONTENTS, remoteName.toStdString().c_str(),
                 CURLFORM_END);

    QString url = tenantURL + QString("files/v2/media/") + remotePath;
    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_HTTPPOST, post1);
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");

    if (this->invokeCurl() == false) {
        return false;
    }

    //
    // process the results
    //

    // open results file
    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: COULD NOT OPEN RESULT");
        return false;
    }

    // read results file & check for errors
    QString val;
    val=file.readAll();
    file.close();

    // read into json object
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();

    // parse json object for status
    //   if error emit error message & return NOT FOUND, if no status it's an error
    //   if success then get result & return the job sttaus

    QString status;
    if (theObj.contains("status")) {
        status = theObj["status"].toString();
        if (status == "error") {
            QString message("Job Not Found");
            if (theObj.contains("message"))
                message = theObj["message"].toString();
            emit errorMessage(message);
            return false;
        } else if (status == "success") {
            emit statusMessage("Successfully uploaded diretory");
            return true;
        }

    } else if (theObj.contains("fault")) {
        QJsonObject theFault = theObj["fault"].toObject();
        if (theFault.contains("message")) {
            QString message = theFault["message"].toString();
            emit errorMessage(message);
            return false;
        }
    }

    return false;
}


void
TapisCurl::downloadFilesCall(const QStringList &remoteFiles, const QStringList &localFiles, QObject* sender) {
    bool result = true;
    for (int i=0; i<remoteFiles.size(); i++) {
        QString remote = remoteFiles.at(i);
        QString local = localFiles.at(i);

        result = this->downloadFile(remote, local);
        if (result == false) {
            emit downloadFilesReturn(result, sender);
        }
    }

    emit downloadFilesReturn(result, sender);
}

bool
TapisCurl::downloadFile(const QString &remoteFile, const QString &localFile)
{
    // this method does not invoke the invokeCurl() as want to write to local file directtly

    CURLcode ret;

    QFileInfo remoteFileInfo(remoteFile);
    QString remoteName = remoteFileInfo.fileName();
    //   QString localName = localFile.fileName();

    QString message = QString("Contacting ") + tenant + QString(" to download remote file ") + remoteName; // + QString( " to ") + localFile;
    emit statusMessage(message);

    // set up the call
    QString url = tenantURL + QString("files/v2/media/") + remoteFile;

    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.54.0");
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(hnd, CURLOPT_FAILONERROR, true);

    // openup localfile and set the writedata pointer to it
    FILE *pagefile = fopen(localFile.toStdString().c_str(), "wb");
    if(pagefile) {
        curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(hnd, CURLOPT_WRITEDATA, pagefile);
    }

    // make the call
    ret = curl_easy_perform(hnd);
    fclose(pagefile);

    // reset the handle for next method as per normal
    curl_easy_reset(hnd);

    // if no error, return
    if (ret == CURLE_OK) {
        emit statusMessage("Successfully downloaded file");
        return true;
    }

    // if failure, go get message, emit signal and return false;

    message = QString("Failed to Download File: ") + remoteFile; // more descriptive message

    emit errorMessage(message);
    return false;
}


QJsonArray
TapisCurl::remoteLS(const QString &remotePath)
{
    QJsonArray result;
    // this method does not invoke the invokeCurl() as want to write to local file directtly

    QFileInfo remoteFileInfo(remotePath);
    if(remotePath.isEmpty())
        remoteFileInfo.setFile(username);


    QString remoteName = remoteFileInfo.fileName();
    QString message = QString("Contacting ") + tenant + QString(" to get dir listing ") + remoteName;
    emit statusMessage(message);

    // set up the call
    QString url = tenantURL + QString("files/v2/listings/") + remotePath;
    if(remotePath.isEmpty())
        url.append(username);

    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    if (this->invokeCurl() == false) {
        return result;
    }

    //
    // process the results
    //

    // open results file
    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: COULD NOT OPEN RESULT .. remoteLS!");
        return result;
    }

    // read results file & check for errors
    QString val;
    val=file.readAll();
    file.close();

    // read into json object
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();

    QString status;
    if (theObj.contains("status")) {
        status = theObj["status"].toString();
        if (status == "error") {
            QString message("Directory Not Found");
            if (theObj.contains("message"))
                message = QString("ERROR: " ) + theObj["message"].toString();
            emit errorMessage(message);
            return result;
        } else if (status == "success") {
            if (theObj.contains("result")) {
                result = theObj["result"].toArray();
                QString message = QString("Succesfully obtained lising: ") + remoteName;
                emit statusMessage(message);
                return result;
            }
        }

    } else if (theObj.contains("fault")) {
        QJsonObject theFault = theObj["fault"].toObject();
        if (theFault.contains("message")) {
            QString message = theFault["message"].toString();
            emit errorMessage(message);
            return result;
        }
    }

    return result;

}



QString
TapisCurl::startJob(const QString &jobDescriptionFile)
{
    QString result = "FAILURE";

    //
    // openfile, put in QJsonObj and call other method
    //

    // open results file
    QFile file(jobDescriptionFile);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: COULD NOT OPEN RESULT");
        return result;
    }

    // read results file & check for errors
    QString val;
    val=file.readAll();
    file.close();

    // read into json object
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();

    return startJob(theObj);
}

void
TapisCurl::startJobCall(const QJsonObject &theJob) {

    QString result = startJob(theJob);
    emit startJobReturn(result);
}

QString
TapisCurl::startJob(const QJsonObject &theJob)
{
    QString result = "FAILURE";

    slist2 = NULL;
    slist2 = curl_slist_append(slist2, "Content-Type: application/json");
    slist2 = curl_slist_append(slist2, bearer.toStdString().c_str());

    QJsonDocument docJob(theJob);
    QString strJson(docJob.toJson(QJsonDocument::Compact));
    QByteArray ba = strJson.toLocal8Bit();
    const char *c_str2 = ba.data();
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, c_str2);
    //  curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)strlen(c_str2));

    QString url = tenantURL + QString("jobs/v2/?pretty=true");
    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");

    if (this->invokeCurl() == false) {
        return result;
    }

    //
    // process the results
    //

    // open results file
    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: COULD NOT OPEN RESULT");
        return result;
    }

    // read results file & check for errors
    QString val;
    val=file.readAll();
    file.close();

    // read into json object
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();

    // parse json object for status
    //   if error emit error message & return NOT FOUND, if no status it's an error
    //   if success then get result & return the job sttaus

    QString status;
    if (theObj.contains("status")) {
        status = theObj["status"].toString();
        if (status == "error") {
            QString message("Job Not Found");
            if (theObj.contains("message"))
                message = QString("ERROR: " ) + theObj["message"].toString();
            emit errorMessage(message);
            std::cerr << message.toStdString();
            return result;
        } else if (status == "success") {
            if (theObj.contains("result")) {
                QJsonObject resObj = theObj["result"].toObject();
                if (resObj.contains("id")) {
                    QString jobID =  resObj["id"].toString();
                    QString message = QString("Succesfully started job: ") + jobID;
                    emit statusMessage(message);
                    std::cerr << message.toStdString();
                    return jobID;
                }
            }
        }

    } else if (theObj.contains("fault")) {
        QJsonObject theFault = theObj["fault"].toObject();
        if (theFault.contains("message")) {
            QString message = theFault["message"].toString();
            emit errorMessage(message);
            std::cerr << message.toStdString();
            return result;
        }
    } else {
        QString message("Job failed for unknown reason");;
        emit errorMessage(message);
        std::cerr << message.toStdString();
        return result;
    }

    return result;
}


void
TapisCurl::getJobListCall(const QString &matchingName) {
    QJsonObject result = getJobList(matchingName);
    emit getJobListReturn(result);
}

QJsonObject
TapisCurl::getJobList(const QString &matchingName)
{
    //TODO: implement matching
    Q_UNUSED(matchingName);
    QString message = QString("Contacting ") + tenant + QString(" to Get Job list");
    emit statusMessage(message);

    QJsonObject result;

    QString url = tenantURL + QString("jobs/v2");
    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    this->invokeCurl();

    //
    // process the results
    //

    // open results file
    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: COULD NOT OPEN RESULT");
        return result;
    }

    // read results file & check for errors
    QString val;
    val=file.readAll();
    file.close();

    if ((val.contains("Missing Credentals")) || (val.contains("Invalid Credentals"))){
        emit errorMessage("ERROR: Trouble LOGGING IN .. try Logout and Login Again");
        return result;
    } else if ((val.contains("Service Unavailable"))){
        QString message = QString("ERROR ") + tenant + QString(" Jobs Service Unavailable .. contact DesignSafe-ci");
        emit errorMessage(message);
        return result;
    }

    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();
    if (theObj.contains("result")){
        QJsonArray jobs = theObj["result"].toArray();
        result["jobs"] = jobs;
        emit statusMessage("Successfully obtained list of submitted jobs");
    }

    return result;
}

void
TapisCurl::getJobDetailsCall(const QString &jobID)
{
    QJsonObject result = getJobDetails(jobID);
    emit getJobDetailsReturn(result);
}


QJsonObject
TapisCurl::getJobDetails(const QString &jobID)
{
    QJsonObject result;

    QString message = QString("Contacting ") + tenant + QString(" to Get Job Details of ") + jobID;
    emit statusMessage(message);

    QString url = tenantURL + QString("jobs/v2/") + jobID;
    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    if (this->invokeCurl() == false) {
        return result;
    }

    //
    // process the results
    //

    // open results file
    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: COULD NOT OPEN RESULT .. getJobDetails!");
        return result;
    }

    // read results file & check for errors
    QString val;
    val=file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();
    if (theObj.contains("result")){
        result = theObj["result"].toObject();
    }

    return result;
}

void
TapisCurl::getJobStatusCall(const QString &jobID){
    QString result = this->getJobStatus(jobID);
    emit getJobStatusReturn(result);
}


QString
TapisCurl::getJobStatus(const QString &jobID){

    QString result("NO JOB FOUND");

    //
    // invoke curl
    //

    QString message = QString("Contacting ") + tenant + QString(" to Get Job Status of ") + jobID;
    emit statusMessage(message);

    QString url = tenantURL + QString("jobs/v2/") + jobID + QString("/status");
    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    if (this->invokeCurl() == false) {
        return result;
    }

    //
    // process the results
    //

    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: COULD NOT OPEN RESULT from getJobStatus .. permission issue!");
        return result;
    }

    // read results file
    QString val;
    val=file.readAll();
    file.close();

    // read into json object
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();

    // parse json object for status
    //   if error emit error message & return NOT FOUND
    //   if sucess then get result & return the job sttaus

    QString status;
    if (theObj.contains("status")) {
        status = theObj["status"].toString();
        if (status == "error") {
            QString message("Job Not Found");
            if (theObj.contains("message"))
                message = theObj["message"].toString();
            qDebug() << "ERROR: " << message;
            emit errorMessage(message);
            return result;

        } else if (status == "success")
            if (theObj.contains("result")) {
                QJsonObject resultObj = theObj["result"].toObject();
                if (resultObj.contains("status")) {
                    result = resultObj["status"].toString();
                    QString message = QString("Successfully obtained job status: " + result);
                    emit statusMessage(message);
                    return result;
                }

            }
    }

    return result;
}

void
TapisCurl::deleteJobCall(const QString &jobID, const QStringList &dirToRemove) {
    bool result = this->deleteJob(jobID, dirToRemove);
    emit deleteJobReturn(result);
}

void TapisCurl::remoteLSCall(const QString &remotePath)
{
    QJsonArray dirList = this->remoteLS(remotePath);
    emit remoteLSReturn(dirList);
}

bool
TapisCurl::deleteJob(const QString &jobID, const QStringList &dirToRemove)
{
    bool result = false;

    //
    // first remove the directories
    //

    foreach(QString item, dirToRemove) {
        result = this->removeDirectory(item);
    }
    Q_UNUSED(result);

    //
    // invoke curl to delete the job
    //

    QString message = QString("Contacting ") + tenant + QString(" to delete job");
    emit statusMessage(message);

    QString url = tenantURL + QString("jobs/v2/") + jobID;

    curl_easy_setopt(hnd, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "DELETE");

    if (this->invokeCurl() == false) {
        return false;
    }

    //
    // process the results
    //

    // open results file
    QFile file(uniqueFileName1);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage("ERROR: COULD NOT OPEN RESULT");
        return false;
    }

    // read into json object
    QString val;
    val=file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject theObj = doc.object();

    // parse json object for status
    //   if error emit error message & return NOT FOUND
    //   if sucess then get result & return the job sttaus

    QString status;
    if (theObj.contains("status")) {
        status = theObj["status"].toString();
        if (status == "error") {
            QString message("Unknown ERROR with Curl Call - deleteJob");
            if (theObj.contains("message"))
                message = theObj["message"].toString();
            qDebug() << "ERROR MESSAGE: " << message;
            emit errorMessage(message);
            return false;
        } else if (status == "success") {
            emit statusMessage("Successfullt deleted job");
            return true;
        }
    }
    return false;
}


bool 
TapisCurl::invokeCurl(void) {

    CURLcode ret;

    //
    // the methods set many of the options, this private method just sets the
    // default ones, invokes curl and then reset the handler for the next call
    // if an error occurs it gets the error messag and emits a signal before returning false
    //

    // set default options
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.54.0");

    if (slist2 != NULL) {
        curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist2);
    }
    else if (slist1 != NULL) {
        curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
    }

    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    //  curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1L);

    // we send the result of curl request to a file > uniqueFileName1
    FILE *pagefile = fopen(uniqueFileName1.toStdString().c_str(), "wb");
    if(pagefile) {
        curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(hnd, CURLOPT_WRITEDATA, pagefile);
    }

    // perform the curl operation that has been set up
    ret = curl_easy_perform(hnd);
    fclose(pagefile);

    // reset the handle so methods can fill in the different options before next call
    curl_easy_reset(hnd);
    if (slist2 != NULL) {
        curl_slist_free_all(slist2);
        slist2 = NULL;
    }

    // check for success
    if (ret == CURLE_OK)
        return true;

    // if failure, go get message, emit signal and return false;
    const char *str = curl_easy_strerror(ret);
    QString errorString(str);
    emit errorMessage(QString("ERROR: " ) + QString(errorString));

    return false;
}
