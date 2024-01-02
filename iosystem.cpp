#include "iosystem.h"
#include <QDebug>
#include "pugixml.hpp"
#include <qfile.h>
#include <qtextstream.h>
using namespace std;

void readNode(const char *string, char *address, pugi::xml_node devNode){
    pugi::xml_node node = devNode.child(string);
    if (!node.empty()){
        //qDebug() << string << "-" << node.text().as_string();
        memcpy(address, node.text().as_string(), strlen(node.text().as_string()));
    }
}

SYSTEMINFO readSystemInfo(const QString& filename)
{
    // 清除info的內容
    SYSTEMINFO info;
    memset(&info, 0x00, sizeof(SYSTEMINFO));

    pugi::xml_document doc;
    if (!doc.load_file(filename.toStdString().c_str()))
        qWarning() << "Load Device failed:";

    pugi::xml_node root = doc.child("conf");//定义'conf'为根节点
    if (root.empty())
        qWarning() << "Load Device failed(xml format error):" << filename;

    pugi::xml_node devNode = root.first_child();

    readNode("ServerIP",info.ServerIP,devNode);
    readNode("ServerPort",info.ServerPort,devNode);
    readNode("LoadCellComPort",info.LoadCellComPort,devNode);

    //qDebug() << "End of system xml";
    return info;
}
