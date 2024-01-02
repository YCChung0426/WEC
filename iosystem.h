#ifndef IOSYSTEM_H_
#define IOSYSTEM_H_

#define MAX_STR_LEN 50

#include <QString>
struct SYSTEMINFO
{

    char ServerIP[MAX_STR_LEN];
    char ServerPort[MAX_STR_LEN];
    char LoadCellComPort[MAX_STR_LEN];

};

SYSTEMINFO readSystemInfo(const QString& filename);
QList<QStringList> csvRead(QString);

#endif
