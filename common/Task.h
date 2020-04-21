#ifndef SIMCENTER_TASK_H
#define SIMCENTER_TASK_H

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

class TapisCurl;
class QString;

/** 
 *  @author  fmckenna
 *  @date    2/2017
 *  @version 1.0 
 *  
 *  @section DESCRIPTION
 *  
 *  This is the Task Interface. It is a base class that provides the functionality
 *  for interfaceing with remote web services, currently limited to sites supporting
 *  the Tapis API
 */

class Task {
 public:

  /**
   * @constructor Construct new Task.
   */
  Task();

  /**
   * @destructor Virtual desctructor for Task
   */
  virtual ~Task();

  /**
   * runRemote method all suerclass should implement. it will run job remotely
   * @param[in] argc integer value indiacating  number of strngs in argv
   * @param[in] argv array of strings passed by user when running application
   * @return Returns 0 if successfull, negative number if not
   */
  virtual int runRemote(int argc, char *argv[]);

  /**
   * listJobs method to list current jobs at remote service
   * @param[in] argc integer value indiacating  number of strngs in argv
   * @param[in] argv array of strings passed by user when running application
   * @return Returns 0 if successfull, negative number if not
   */
  int listJobs(int argc, char *argv[]);

  /**
   * jobDetails method to list details of a job
   * @param[in] argc integer value indiacating  number of strngs in argv
   * @param[in] argv array of strings passed by user when running application
   * @return Returns 0 if successfull, negative number if not
   */
  virtual int jobDetails(int argc, char *argv[]);

  /**
   * resetPreferences method to reset the preferences stored
   * @param[in] argc integer value indiacating  number of strngs in argv
   * @param[in] argv array of strings passed by user when running application
   * @return Returns 0 if successfull, negative number if not
   */
  virtual int resetPreferences(int argc, char *argv[]);

  /**
   * printPreferences method to reset the preferences stored
   * @param[in] argc integer value indiacating  number of strngs in argv
   * @param[in] argv array of strings passed by user when running application
   * @return Returns 0 if successfull, negative number if not
   */
  virtual int printPreferences(int argc, char *argv[]);

  /**
   * printHelp method to print list of application options
   * @param[in] argc integer value indiacating  number of strngs in argv
   * @param[in] argv array of strings passed by user when running application
   * @return Returns 0 if successfull, negative number if not
   */
  virtual int printHelp(int argc, char *argv[]);

  /**
   * parseCommand method to parse command line args and invoke appropriate method above
   * @param[in] argc integer value indiacating  number of strngs in argv
   * @param[in] argv array of strings passed by user when running application
   * @return Returns 0 if successfull, negative number if not
   */
  virtual int parseCommand(int argc, char *argv[]);

  /**
   * printMethod method used as ouput
   * @param[in] message Qstring containing message to be output
   * @return Returns 0 if successfull, negative number if not
   */
  virtual int printMessage(QString &message);


  /**
   * Login to service
   * @param[in] name username, default is nullptr in which case will use that stored in preferences
   * @param[in] password again default is nullptr in which case will use that stored in preferences
   * @return Returns 0 if successfull, negative number if not
   */
  int login(char *name = nullptr, char *password = nullptr);


  /**
   * recursiveCopy
   * @param[in] source the source directory
   * @param[in] target the target directort
   * @return Returns 0 if successfull, negative number if not
   */
  int recursiveCopy(const QString &source, const QString &target);


 protected:
  TapisCurl *theRemoteService; 

};

#endif // SIMCENTER_TASK
