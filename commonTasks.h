void printMessage(QString &);
void resetPreferences(int argc, char *argv[]);
void printPreferences(int argc, char *argv[]);
TapisCurl *login(char *name = NULL, char *password = NULL);
void listJobs(int argc, char *argv[]);
void jobDetails(int argc, char *argv[]);
void printHelp(void);