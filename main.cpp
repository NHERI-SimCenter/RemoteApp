#include <iostream>

#include <QCoreApplication>
#include <GoogleAnalytics.h>
#include <FEM_UQ.h>

int main(int argc, char *argv[])
{
  QCoreApplication a(argc, argv);
   
  QCoreApplication::setApplicationName("remoteApp");
  QCoreApplication::setOrganizationName("SimCenter");
  QCoreApplication::setApplicationVersion("2.0.1");
  //    GoogleAnalytics::SetTrackingId("UA-121636495-1");
  GoogleAnalytics::StartSession();
  GoogleAnalytics::ReportStart();

  FEM_UQ theTask;
  theTask.parseCommand(argc, argv);

  GoogleAnalytics::EndSession();

  std::cout << "Bye!\n";
  exit(0);
}
