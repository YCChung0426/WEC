#include "widget.h"
#include "ui_widget.h"
#include <QtEndian>
#include <QDateTime>
#include <QMessageBox>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    SYSTEMINFO sysInfo = readSystemInfo(QCoreApplication::applicationDirPath()+"//config.xml");

    // 根據config.xml改變介面形式
    if (QString(sysInfo.LoadCellComPort).length() > 0){
        ui->lineEdit_LineForceCal->setGeometry(30,400,111,50);
        ui->label_25->setText("推估/實測拉力(kg)");
        bEnableLoadCell = true;
    }

    createChart();
    initLoadCell();


    // 系統時間同步
    auto sysTime = new QTimer(this);
    connect(sysTime,&QTimer::timeout,this,[this](){
        ui->label_sysTime->setText("系統時間："+QDateTime::currentDateTime().toString("yyyy年MM月dd日 hh時mm分ss秒"));
    });
    sysTime->start(10);

    // 離開按鈕
    connect(ui->bCloseProgram,&QPushButton::released,this,[]{
        emit QApplication::exit();
    });




    // 按下連線/離線按鈕的行為
    connect(ui->bConnect,&QPushButton::released,this,[this,sysInfo](){
        if (!connectState){
            // Change beheaivor of button
            emit systemConnectSignal(true);
            ui->bConnect->setText("離線");
            ui->bConnect->setStyleSheet(".QPushButton { background: rgb(255,255,1);}");
            ui->bRecord->setEnabled(true);
            if (QString(sysInfo.LoadCellComPort).length() > 0){
                m_portLoadCell->setPortName(QString(sysInfo.LoadCellComPort));
                m_portLoadCell->open(QIODevice::ReadWrite);
            } else {
                ui->lineEdit_LineForceMea->setText("---");
            }
            connectState = true;
        } else {
            emit systemConnectSignal(false);
            ui->bConnect->setText("連線");
            ui->bConnect->setStyleSheet(".QPushButton {}");
            ui->bRecord->setEnabled(false);
            if (QString(sysInfo.LoadCellComPort).length() > 0){
                m_portLoadCell->clear();
                m_portLoadCell->close();
            }
            // 如果離線的時候還在記錄就強制關閉
            if (recordState){
                emit ui->bRecord->released();
            }
            connectState = false;
        }
    });





    connect(ui->bRecord,&QPushButton::released,this,[this](){
        if (!recordState){
            ui->bRecord->setText("停止記錄");
            ui->bRecord->setStyleSheet(".QPushButton { background: rgb(255,255,1);}");
            // 重新開啟資料/加入header
            // 同步記錄上面所有數據
            dataSyncRecord.clear();
            dataSyncRecord.append(QStringList({"Time","UpTime(ms)","Data Count","Angle(deg)","Speed(rpm)",
                                               "Roll(deg)","Pitch(deg)","Yaw(deg)",
                                               "FreeAcc X(m/s2)","FreeAcc Y(m/s2)","FreeAcc Z(m/s2)",
                                               "Generator Voltage(V)", "Generator Current(A)",
                                               "Load Voltage(V)", "Load Current(A)",
                                               "Battery Voltage(V)","Torque(Nm)","Air Pressure(kg)",
                                               "Load Power(W)","Generator Power(W)","InputPowerRot Power(W)",
                                               "Line Velocity(m/s)","Line Force Cal(kg)","Line Force Measured(kg)",
                                               "Line Input Power(W)",
                                              }));

            // 記錄時間(filename)
            dataStartTime = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
            recordState = true;
        } else {
            ui->bRecord->setText("開始記錄");
            recordState = false;
            //ui->inputStatus->setText("停止記錄數據");
            ui->bRecord->setStyleSheet(".QPushButton {}");
            csvWrite("saveData\\"+dataStartTime+"_Sync.csv",dataSyncRecord);

            //顯示平均值
            float avgLoadPower = 0;
            float avgLoadCurrent = 0;
            float avgLoadVoltage = 0;
            for (int i=0;i<dataSyncRecord.length();i++){
                avgLoadPower += dataSyncRecord.at(i)[18].toFloat();
                avgLoadCurrent += dataSyncRecord.at(i)[12].toFloat();
                avgLoadVoltage += dataSyncRecord.at(i)[11].toFloat();
            }

            avgLoadPower = avgLoadPower/dataSyncRecord.length();
            avgLoadCurrent = avgLoadCurrent/dataSyncRecord.length();
            avgLoadVoltage = avgLoadVoltage/dataSyncRecord.length();

            QMessageBox msgBox;
            msgBox.setText("Average Power:   " + QString::number(avgLoadPower, 'f', 3) + " W"
                           "\nAverage Voltage: " + QString::number(avgLoadVoltage, 'f', 3) + " V"
                           "\nAverage Current: " + QString::number(avgLoadCurrent, 'f', 3) + " A");
            msgBox.exec();

        }
    });

    QString ipWebsocket = QString(sysInfo.ServerIP);
    int portWebsocket = QString(sysInfo.ServerPort).toInt();

    webSocket = new QWebSocket();
    webSocket->setParent(this);


    // 設定連線/離線時的行為
    connect(this,&Widget::systemConnectSignal,this,[this,ipWebsocket,portWebsocket](bool status){
        if (status == true){
            webSocket->open(QUrl("ws://"+ipWebsocket+":"+QString::number(portWebsocket)));
        } else {
            webSocket->close();
        }
    });

    // Update parameters
    connect(webSocket, &QWebSocket::textMessageReceived,this, [this](QString message){
        QJsonDocument jsonDocument = QJsonDocument::fromJson(message.toUtf8());
        QJsonObject jsonObject = jsonDocument.object();
        // Iterate over the key-value pairs
        for (const QString& key : jsonObject.keys()) {
            QJsonValue value = jsonObject.value(key);
            if (key == "Pressure"){
                appendWithSizeLimit(Pressure,value.toDouble());
                ui->lineEdit_Pressure->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Angle"){
                appendWithSizeLimit(Angle,value.toDouble());
                ui->lineEdit_Angle->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Vbat"){
                appendWithSizeLimit(VBattery,value.toDouble());
                ui->lineEdit_Battery->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "AccX"){
                appendWithSizeLimit(FreeAccX,value.toDouble());
                ui->lineEdit_FreeAccX->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "AccY"){
                appendWithSizeLimit(FreeAccY,value.toDouble());
                ui->lineEdit_FreeAccY->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "AccZ"){
                appendWithSizeLimit(FreeAccZ,value.toDouble());
                ui->lineEdit_FreeAccZ->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Iin"){
                appendWithSizeLimit(IGenerator,value.toDouble());
                ui->lineEdit_IGenerator->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Iout"){
                appendWithSizeLimit(ILoad,value.toDouble());
                ui->lineEdit_ILoad->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Pitch"){
                appendWithSizeLimit(Pitch,value.toDouble());
                ui->lineEdit_Pitch->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Roll"){
                appendWithSizeLimit(Roll,value.toDouble());
                ui->lineEdit_Roll->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Yaw"){
                appendWithSizeLimit(Yaw,value.toDouble());
                ui->lineEdit_Yaw->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Omega"){
                appendWithSizeLimit(Omega,value.toDouble());
                ui->lineEdit_Omega->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Torque"){
                appendWithSizeLimit(Torque,value.toDouble());
                ui->lineEdit_Torque->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Vin"){
                appendWithSizeLimit(VGenerator,value.toDouble());
                ui->lineEdit_VGenerator->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "Vout"){
                appendWithSizeLimit(VLoad,value.toDouble());
                ui->lineEdit_VLoad->setText(QString::number(value.toDouble(), 'f', 3));
            } else if (key == "data_count"){
                DataCount = value.toInt();
                ui->lineEdit_DataCount->setText(QString::number(value.toVariant().toUInt()));
            } else if (key == "epoch_time"){
                EpochTime = value.toInt();
                ui->lineEdit_EpochTime->setText(QString::number(value.toVariant().toUInt()));
            } else {
                qDebug() << "Unknown key/value";
            }
        }

        // 接收完成，推估剩餘數據
        appendWithSizeLimit(PLoad,VLoad.last()*ILoad.last());  // V*I
        ui->lineEdit_LoadPower->setText(QString::number(PLoad.last(), 'f', 3));

        appendWithSizeLimit(PGenerator,VGenerator.last()*IGenerator.last());   // V*I
        ui->lineEdit_GeneratorPower->setText(QString::number(PGenerator.last(), 'f', 3));

        float inputRot = Torque.last()*Omega.last()/60*2*3.1415;
        if (inputRot >= 0){
            appendWithSizeLimit(PInputRot,Torque.last()*Omega.last()/60*2*3.1415);  // T*w (Nm*rad/s)
            ui->lineEdit_InputPowerRot->setText(QString::number(PInputRot.last(), 'f', 3));
        } else {
            appendWithSizeLimit(PInputRot,0);  // T*w (Nm*rad/s)
            ui->lineEdit_InputPowerRot->setText(QString::number(0, 'f', 3));
        }


        appendWithSizeLimit(LineVelocity,3.1415*Omega.last()*0.26/60); // 3.1415*w*0.26/60 (m/s)
        ui->lineEdit_LineVelocity->setText(QString::number(LineVelocity.last(), 'f', 3));

        //appendWithSizeLimit(LineForceCal,(Torque.last()/0.26+Pressure.last()*9.81*0.09/0.26)/4.5*1.2/98*101/36*35);
        float forceFromTorque = (Torque.last()/0.26)*0.2672*0.77;
        float forceFromPressure = ((Pressure.last()-3.5)*9.81*0.09/0.26)*0.2672*1.3;
        appendWithSizeLimit(LineForceCal,forceFromTorque+forceFromPressure); // TODO MODIFY
        ui->lineEdit_LineForceCal->setText(QString::number(LineForceCal.last(), 'f', 1));

        float inputPower = LineVelocity.last()*LineForceCal.last()*9.81;
        if (inputPower > 0){
            appendWithSizeLimit(PInputLine,inputPower);
        } else {
            appendWithSizeLimit(PInputLine,0);
        }
        ui->lineEdit_InputPowerLine->setText(QString::number(PInputLine.last(), 'f', 3));

        // 保留 note 動態調整小數位數的方法
        // QString result = QString::number(PInputLine, 'f', qMax(5 - QString::number(PInputLine, 'f', 0).length(), 0));

        // 畫圖
        for (int i=0;i<3;i++){
            float data[4];
            float dataMax = 0;
            float dataMin = 0;
            // 加入資料點
            if (i==0){
                // 負載功率, 發電機輸出功率, 發電機輸入功率, 浮體輸入功率
                data[0] = PLoad.last();
                data[1] = PGenerator.last();
                data[2] = PInputRot.last();
                data[3] = PInputLine.last();
                seriesList1[i]->append(chartTime,data[0]);
                seriesList2[i]->append(chartTime,data[1]);
                seriesList3[i]->append(chartTime,data[2]);
                seriesList4[i]->append(chartTime,data[3]);
                dataMax = qMax<float>(qMax<float>(data[0],qMax<float>(data[1],qMax<float>(data[2],data[3]))),YmaxList[i]);
                dataMin = qMin<float>(qMin<float>(data[0],qMin<float>(data[1],qMin<float>(data[2],data[3]))),YminList[i]);
            } else if (i==1){
                data[0] = FreeAccX.last();
                data[1] = FreeAccY.last();
                data[2] = FreeAccZ.last();
                seriesList1[i]->append(chartTime,data[0]);
                seriesList2[i]->append(chartTime,data[1]);
                seriesList3[i]->append(chartTime,data[2]);
                dataMax = qMax<float>(qMax<float>(data[0],qMax<float>(data[1],data[2])),YmaxList[i]);
                dataMin = qMin<float>(qMin<float>(data[0],qMin<float>(data[1],data[2])),YminList[i]);
            } else { // i==2
                if (bEnableLoadCell == true){
                    data[0] = LineForceMea.last();
                    data[1] = LineForceCal.last();
                } else {
                    data[0] = Pressure.last();
                    data[1] = LineForceCal.last();
                }
                seriesList1[i]->append(chartTime,data[0]);
                seriesList2[i]->append(chartTime,data[1]);
                dataMax = qMax<float>(qMax<float>(data[0],data[1]),YmaxList[i]);
                dataMin = qMin<float>(qMin<float>(data[0],data[1]),YminList[i]);
            }
            YminList[i] = dataMin;
            if (dataMax < 30000){
                YmaxList[i] = dataMax;
            }
            // 移除多餘的點(減少記憶體使用)
            if (seriesList1[i]->count() > AXISLENGTH){
                seriesList1[i]->remove(0);
            }
            if (seriesList2[i]->count() > AXISLENGTH){
                seriesList2[i]->remove(0);
            }
            if (seriesList3[i]->count() > AXISLENGTH){
                seriesList3[i]->remove(0);
            }
            if (seriesList4[i]->count() > AXISLENGTH){
                seriesList4[i]->remove(0);
            }
            // 處理座標軸
            if (i == currentChart){
                axisYList[i]->setMax(dataMax);
                axisYList[i]->setMin(dataMin);
                if (chartTime > AXISLENGTH/10){
                    axisXList[i]->setRange(chartTime-AXISLENGTH/10,chartTime);
                }
                // 清除過多資料
                if (axisXList[i]->max() >= 10000){
                    emit ui->bClean->released();
                }
            }
        }
        chartTime+=0.1;

        // Append 記錄資料
        if (recordState){
            QString tNow = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
            dataSyncRecord.append(QStringList({tNow,
                                               ui->lineEdit_EpochTime->text(), ui->lineEdit_DataCount->text(),
                                               ui->lineEdit_Angle->text(),ui->lineEdit_Omega->text(),
                                               ui->lineEdit_Roll->text(),ui->lineEdit_Pitch->text(),ui->lineEdit_Yaw->text(),
                                               ui->lineEdit_FreeAccX->text(),ui->lineEdit_FreeAccY->text(),ui->lineEdit_FreeAccZ->text(),
                                               ui->lineEdit_VGenerator->text(),ui->lineEdit_IGenerator->text(),
                                               ui->lineEdit_VLoad->text(),ui->lineEdit_ILoad->text(),
                                               ui->lineEdit_Battery->text(),ui->lineEdit_Torque->text(),ui->lineEdit_Pressure->text(),
                                               ui->lineEdit_LoadPower->text(),
                                               ui->lineEdit_GeneratorPower->text(),
                                               ui->lineEdit_InputPowerRot->text(),
                                               ui->lineEdit_LineVelocity->text(),
                                               ui->lineEdit_LineForceCal->text(),
                                               ui->lineEdit_LineForceMea->text(),
                                               ui->lineEdit_InputPowerLine->text(),
                                               }));
        }
    });


    connect(ui->bReset,&QPushButton::released,this,[this]{
        YmaxList = {0,0,0};
        YminList = {0,0,0};
    });
    connect(ui->bClean,&QPushButton::released,this,[this]{
        for (int i=0;i<3;i++){
            seriesList1[i]->clear();
            seriesList2[i]->clear();
            seriesList3[i]->clear();
            seriesList4[i]->clear();
            axisXList[i]->setRange(0,AXISLENGTH/10);
        }
        chartTime = 0;
        YmaxList = {0,0,0};
        YminList = {0,0,0};
    });
    connect(ui->comboBoxVoltageLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),[=](int index){
        if (connectState == false)
            return;
        webSocket->sendTextMessage(".setVoltageLevel"+QString::number(index));
    });
    connect(ui->bReadSetting,&QPushButton::released,[=]{
        if (connectState == false)
            return;
        webSocket->sendTextMessage(".readSetting");
    });
}


void Widget::csvWrite(QString filename,QList<QStringList> stringList)
{
    QFile outFile(filename);
    QStringList lines;
    for (int i=0;i<stringList.length();i++){
        for (int j=0;j<stringList[i].length();j++){
            //qDebug() << stringList[i][j] << ",";
            lines << stringList[i][j] << ",";
        }
        lines << "\n";
    }
    if (outFile.open(QIODevice::WriteOnly))
    {
        for (int i = 0; i < lines.size(); i++)
        {
            outFile.write(lines[i].toStdString().c_str());/*寫入每一行數據到文件*/
        }
        outFile.close();
    }
}

void Widget::createChart()
{

    // 建立圖表
    for (int i=0;i<3;i++){
        // 創造曲線資訊存放空間
        QLineSeries* series = new QLineSeries();
        seriesList1.append(series);
        QLineSeries* series2 = new QLineSeries();
        seriesList2.append(series2);
        QLineSeries* series3 = new QLineSeries();
        seriesList3.append(series3);
        QLineSeries* series4 = new QLineSeries();
        seriesList4.append(series4);

        QChart* chart = new QChart();
        // 根據曲線圖選擇要增加那些曲線進圖內
        if (i == 0){
            series->setName("負載功率(W)");
            series2->setName("電機輸出功率(W)");
            series3->setName("電機輸入功率(W)");
            series4->setName("浮體輸入功率(W)");
            chart->addSeries(series);
            chart->addSeries(series2);
            chart->addSeries(series3);
            chart->addSeries(series4);
        } else if (i== 1){
            chart->addSeries(series);
            chart->addSeries(series2);
            chart->addSeries(series3);
            series->setName("X加速度(m/s2)");
            series2->setName("Y加速度(m/s2)");
            series3->setName("Z加速度(m/s2)");
        } else { // i==2
            chart->addSeries(series);
            chart->addSeries(series2);
            if (bEnableLoadCell == true){
                series->setName("纜繩拉力實測值(kg)");
                series2->setName("纜繩拉力推估值(kg)");
            } else {
                series->setName("蓄壓器壓力(kg)");
                series2->setName("纜繩拉力(kg)");
            }

        }
        chart->legend()->setVisible(false);
        chart->setTheme(QChart::ChartThemeDark);
        chart->setFont(QFont("微軟正黑體"));
        //chart->setTitle(" ");
        chartList.append(chart);

        // 設定坐標軸 (固定流程)
        QValueAxis *chartAxisX = new QValueAxis();
        QValueAxis *chartAxisY = new QValueAxis();
        chartAxisX->setRange(0, AXISLENGTH/10);
        chartAxisX->setTickCount(21);
        chartAxisX->setLabelFormat("%.1f");
        chartAxisY->setRange(0, 1);
        chartAxisY->setLabelFormat("%.2f");
        QFont labelsFont = QFont("微軟正黑體");
        labelsFont.setPixelSize(18);
        chartAxisX->setLabelsFont(labelsFont);
        chartAxisY->setLabelsFont(labelsFont);
        chartAxisX->setTitleText("時間(s)");
        axisXList.append(chartAxisX);
        axisYList.append(chartAxisY);
        chart->addAxis(chartAxisX, Qt::AlignBottom);
        chart->addAxis(chartAxisY, Qt::AlignLeft);
        chart->legend()->setVisible(true);
        chart->legend()->setFont(labelsFont);



        if (i == 0){
            series->attachAxis(chartAxisX);
            series->attachAxis(chartAxisY);
            series2->attachAxis(chartAxisX);
            series2->attachAxis(chartAxisY);
            series3->attachAxis(chartAxisX);
            series3->attachAxis(chartAxisY);
            series4->attachAxis(chartAxisX);
            series4->attachAxis(chartAxisY);
            //chartAxisY->setTitleText("功率(W)");
        } else if (i== 1){
            series->attachAxis(chartAxisX);
            series->attachAxis(chartAxisY);
            series2->attachAxis(chartAxisX);
            series2->attachAxis(chartAxisY);
            series3->attachAxis(chartAxisX);
            series3->attachAxis(chartAxisY);
            //chartAxisY->setTitleText("電壓(V)");
        } else { // i==2
            series->attachAxis(chartAxisX);
            series->attachAxis(chartAxisY);
            series2->attachAxis(chartAxisX);
            series2->attachAxis(chartAxisY);
        }
    }

    // 建立Chartview並關聯到介面
    chartview = new QChartView(chartList[0]);
    chartview->setRenderHint(QPainter::Antialiasing);
    chartview->setParent(ui->frameChart);
    chartview->resize(ui->frameChart->width(),ui->frameChart->height());

    ui->radioChart1->setStyleSheet("QRadioButton{color:rgb(255, 255, 1); background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");

    connect(ui->radioChart1,&QRadioButton::released,this,[this]{
        QTimer::singleShot(50,this,[this]{chartview->setChart(chartList[0]);});
        currentChart = 0;
        ui->radioChart1->setStyleSheet("QRadioButton{color:rgb(255, 255, 1); background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");
        ui->radioChart2->setStyleSheet("QRadioButton{background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");
        ui->radioChart3->setStyleSheet("QRadioButton{background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");
    });
    connect(ui->radioChart2,&QRadioButton::released,this,[this]{
        QTimer::singleShot(50,this,[this]{chartview->setChart(chartList[1]);});
        currentChart = 1;
        ui->radioChart1->setStyleSheet("QRadioButton{background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");
        ui->radioChart2->setStyleSheet("QRadioButton{color:rgb(255, 255, 1); background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");
        ui->radioChart3->setStyleSheet("QRadioButton{background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");
    });
    connect(ui->radioChart3,&QRadioButton::released,this,[this]{
        QTimer::singleShot(50,this,[this]{chartview->setChart(chartList[2]);});
        currentChart = 2;
        ui->radioChart1->setStyleSheet("QRadioButton{background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");
        ui->radioChart2->setStyleSheet("QRadioButton{background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");
        ui->radioChart3->setStyleSheet("QRadioButton{color:rgb(255, 255, 1); background-color: rgba(255, 255, 1, 0);font: 22px \"微軟正黑體\";}");
    });
}

void Widget::initLoadCell()
{
    /************* USB串列埠 - Load cell *************/
    m_portLoadCell = new QSerialPort();
    m_portLoadCell->setBaudRate(QSerialPort::Baud38400);
    m_portLoadCell->setParity(QSerialPort::NoParity);
    m_portLoadCell->setDataBits(QSerialPort::Data8);
    m_portLoadCell->setStopBits(QSerialPort::OneStop);
    m_portLoadCell->setFlowControl(QSerialPort::NoFlowControl);
    m_portLoadCell->setReadBufferSize(1000000);

    /************* Load Cell *************/
    connect(m_portLoadCell, &QSerialPort::readyRead, this, [this](){
        QByteArray data = m_portLoadCell->readAll();
        bufferLoad+=data;
        //qDebug() << bufferLoad.toHex();
        while (bufferLoad.length() >= LOADDATASIZE){
            int index = bufferLoad.indexOf(QByteArray::fromHex("05 03 02"));
            if (index != -1 && bufferLoad.length()>=LOADDATASIZE+index){
                QByteArray chopped = bufferLoad.mid(index,LOADDATASIZE);
                bufferLoad = bufferLoad.mid(index+LOADDATASIZE,bufferLoad.length()-index-LOADDATASIZE);
                // CRC
                int crcGet = qFromLittleEndian<quint16>(chopped.mid(LOADDATASIZE-2,2));
                int crcCal = crc16ForModbus(chopped.mid(0,5));
                if (crcGet != crcCal){
                    continue;
                    qDebug() << crcGet << crcCal;
                }
                int loadInt =  ConvHexToInt_BigEndian(chopped.mid(3,2).toHex());
                if (loadInt > 32768){
                    loadInt = -(65535-loadInt);
                }
                // Load cell的格式為顯示螢幕上的所有字元，小數點要自己標 (目前設定是小數點1位)
                appendWithSizeLimit(LineForceMea,(float)loadInt/10*2);
                ui->lineEdit_LineForceMea->setText(QString::number(LineForceMea.last(),'f',1));
            } else {
                qDebug() << "break";
                break;
            }
        }
    });

    // 使用另外一組Timer定時送訊號給RS485 Modbus，來抓取回傳值
    auto loadTimer = new QTimer(this);
    loadTimer->setTimerType(Qt::PreciseTimer);
    connect(loadTimer,&QTimer::timeout,this,[this](){
        if (connectState && m_portLoadCell->isOpen() == true){
            // Load cell型號： JS-300 Force indicator Modbus版本
            // 抓取的指令固定：目前站別01 讀取命令03 資料位指00 48 資料長度00 01 CRC 04 1C
            m_portLoadCell->write(QByteArray::fromHex("05 03 00 48 00 01 05 98"));
            //m_portLoadCell->write(QByteArray::fromHex("01 03 00 48 00 01 04 1c"));
        }
    });
    // 取樣頻率10Hz
    loadTimer->start(100);

}


quint16 Widget::crc16ForModbus(const QByteArray &data)
{
        static const quint16 crc16Table[] =
        {
            0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
            0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
            0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
            0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
            0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
            0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
            0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
            0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
            0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
            0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
            0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
            0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
            0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
            0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
            0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
            0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
            0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
            0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
            0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
            0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
            0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
            0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
            0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
            0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
            0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
            0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
            0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
            0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
            0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
            0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
            0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
            0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
        };

        quint8 buf;
        quint16 crc16 = 0xFFFF;

        for ( auto i = 0; i < data.size(); ++i )
        {
            buf = data.at( i ) ^ crc16;
            crc16 >>= 8;
            crc16 ^= crc16Table[ buf ];
        }

        return crc16;
}
float Widget::ConvHexToFloatIEEE754(QByteArray input){
    QByteArray hex=QByteArray::fromHex(input);
    unsigned char s[4];
    s[3] = hex[0];
    s[2] = hex[1];
    s[1] = hex[2];
    s[0] = hex[3];
    float fnum;
    memcpy_s(&fnum ,sizeof(float),s,4);
    return fnum;
}


int Widget::ConvHexToInt_BigEndian(QByteArray input){
    // Updated: 2022/06/13 (確認完畢)
    // 0258/00000258/0000000000000258->600, 58->88

    QByteArray hex;
    if (input.length() >= 4){
        hex=QByteArray::fromHex(input);
    } else {
        QByteArray temp;
        for (int i=input.length();i<4;i=i+2)
            temp += "00";
        hex=QByteArray::fromHex(temp+input);
    }

    if (input.length() == 2){
        return qFromBigEndian<quint16>(hex);
    } else if (input.length() == 4){
        return qFromBigEndian<quint16>(hex);
    } else if (input.length() == 8){
        return qFromBigEndian<quint32>(hex);
    } else if (input.length() == 16){
        return qFromBigEndian<quint64>(hex);
    } else {
        return 0;
    }
}


void Widget::appendWithSizeLimit(QList<double>& list, const double item) {
    list.append(item);
    if (list.size() > AXISLENGTH) {
        list.removeFirst();
    }
}


Widget::~Widget()
{
    delete ui;
}

