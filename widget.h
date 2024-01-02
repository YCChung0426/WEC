#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDebug>
#include <QTimer>

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QFile>

#include <QLineSeries>
#include <QtCharts>

#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>

#include <iosystem.h>

#define AXISLENGTH 600
#define LOADDATASIZE 7

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE


class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    void csvWrite(QString filename,QList<QStringList> stringList);
    void createChart();
    void initLoadCell();

    float ConvHexToFloatIEEE754(QByteArray input);
    int ConvHexToInt_BigEndian(QByteArray input);
    quint16 crc16ForModbus(const QByteArray &data);
    void appendWithSizeLimit(QList<double>& list, const double item);

signals:
    void systemConnectSignal(bool state);
    void handleMarkerClicked();

private:
    Ui::Widget *ui;
    bool connectState = false;
    bool recordState = false;
    QList<QStringList> dataSyncRecord;
    QString dataStartTime;

    QTimer* recordTimer;

    // 作圖
    QList<QLineSeries*> seriesList1;
    QList<QLineSeries*> seriesList2;
    QList<QLineSeries*> seriesList3;
    QList<QLineSeries*> seriesList4;
    QList<QChart*> chartList;
    QChartView *chartview;
    QList<QValueAxis*> axisXList;
    QList<QValueAxis*> axisYList;
    float chartTime = 0;
    QList<float> YmaxList = {0,0,0};
    QList<float> YminList = {0,0,0};
    short currentChart = 0;

    // Data variable
    QList<double> Pressure;     // 蓄壓器
    QList<double> Angle,Omega;  // Encoder
    QList<double> VBattery;     // Battery
    QList<double> FreeAccX,FreeAccY,FreeAccZ;  // 自由加速度
    QList<double> IGenerator,ILoad;  // 電流
    QList<double> Pitch,Roll,Yaw;    // 浮體姿態
    QList<double> Torque;            // 扭力計
    QList<double> VGenerator,VLoad;  // 電壓
    int32_t DataCount;    // 目前資料筆數
    int32_t EpochTime;    // Epoch time

    // 推估數據
    QList<double> PLoad,PGenerator, PInputRot, PInputLine;
    QList<double> LineVelocity, LineForceCal, LineForceMea;

    /* Websocket Server */
    QWebSocket  *webSocket;
    QByteArray buffer;

    /* Load cell */
    QSerialPort* m_portLoadCell; //串口类
    QByteArray bufferLoad;
    bool bEnableLoadCell = false;
};
#endif // WIDGET_H
