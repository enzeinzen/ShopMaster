#include "emagmdichild.h"
#include "ui_emagmdichild.h"

EMAGMdiChild::EMAGMdiChild(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EMAGMdiChild)
{
    //WE SET THE PATH OF THE ORDERS DIRECTORY ASS A CLASS VARIABLE AS IT IS USED BY MANY THINGS
    EMAGOrdersDirectory.setPath(QCoreApplication::applicationDirPath() + "/Orders/eMAG");

    //WE MAKE SURE TO CREATE THE DIRECTORY OR ELSE THOSE MANY THINGS WOULD FAIL
    EMAGOrdersDirectory.mkpath(EMAGOrdersDirectory.path());

    //WE LINK THE END OF THE PROCESSING OF THE NETWORK REQUEST INTO FILES TO THE POPULATION OF THE TABLE
    //THIS WILL BE CHANGED AND THE ACTUAL ORDER GETTING AND THE TABLE VIEW WILL BE SEPARATED
    connect(this, &EMAGMdiChild::OrderGetComplete, this, &EMAGMdiChild::PopulateOrderViewTable);

    //WE SET THE CURRENT DATE AS A CLASS VARIABLE AS IT IS INTEGRAL TO THE DIRECTORY STRUCTURE
    EMAGMdiChild::CurrentDate = QDate::currentDate();

    //WE CALL THE GER ORDERS FUNCTION HERE FOR NOW
    //IT SHOULD BE MOVED OUT OF THE CONSTRUCTOR AND ONTO A BUTTON ONCE IT IS SEPARATED FROM THE TABLE VIEW
    EMAGMdiChild::GetEMAGOrders();

    //DO NOT CALL ANYTHING USING ui-> BEFORE THIS; IT WILL CRASH THE PROGRAM
    //I DID THIS AND SPENT AN HOUR TRYING TO FIGURE IT OUT
    ui->setupUi(this);

    //WE INITIALIZE THE WINDOW BY SETTING THE DATE OF THE DATE WIDGET TO THE CURRENT ONE
    ui->OrderDateView->setDate(EMAGMdiChild::CurrentDate);

    //WE SET THE RESIZE POLICY FOR THE TABLES
    ui->OrdersView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->OrderDetailsView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

EMAGMdiChild::~EMAGMdiChild()
{
    delete ui;
}

void EMAGMdiChild::GetEMAGOrders()
{
    //The NetworkManager class is initialised and the signal is connected to our slot function.
    AuthManager = new QNetworkAccessManager();
    connect(AuthManager, &QNetworkAccessManager::finished, this, &EMAGMdiChild::on_AuthRequestComplete);

    //The post request is constructed and then executed.
    QUrl ApiUrl;
    ApiUrl.setUrl("https://marketplace.emag.ro/api-3/order/read");


    //The auth data that will be sent in the post request headers is stored in AuthData which we initialise here.
    //The hash that was stored in the AuthDialog class is retrieved from the file.
    QByteArray AuthData;
    QFile CredentialStore(QCoreApplication::applicationDirPath() + "/CredentialStore.bin");
    QString EncryptedString;

    if(CredentialStore.open(QIODevice::ReadOnly | QIODevice::Text))
        {
        EncryptedString = CredentialStore.readAll();
        }

    AuthData.append(EncryptedString);
    AuthData.prepend("Basic ");

    //The request is constructed and the HTTP headers are added:
    QNetworkRequest ApiRequest;
    ApiRequest.setUrl(ApiUrl);
    ApiRequest.setRawHeader("Authorization", AuthData);
    ApiRequest.setRawHeader("Content-Type", "application/json");

    //The data body of the post request is constructed here (doesn´t appear to work yet).
    QByteArray PostData;
    PostData.append("currentPage=1&itemsPerPage=10");

    //The request is made and the data is printed to the debug console.
    AuthManager->post(ApiRequest, PostData);
    qDebug() << PostData;
}

void EMAGMdiChild::on_AuthRequestComplete(QNetworkReply * AuthReply)
{
    //This function is called asynchronously when the QNetworkAccessManager has finished the request.
    //The connection between the QNetworkAccessManager and this function is handled by the Signal/Slot System.
    //We also check if the reply has the "X-User-Id" header which is only present if the credentials are right.
    //If they are right, we launch a mdi subwindow using the EMAGMdiChild class.
    if(!AuthReply->hasRawHeader("X-User-Id"))
    {
        QMessageBox WrongCredentials;
        WrongCredentials.setText("Eroare de autentificare eMAG.");
        WrongCredentials.exec();
    }
    else
    {
        QJsonArray OrdersArray = QJsonDocument::fromJson(AuthReply->readAll()).object()["results"].toArray();
        for (int i = 0; i < OrdersArray.size(); i++)
        {
            //WE DECLARE A NEW FILE FOR EACH ORDER, USING THE QSETTINGS CLASS
            //WE USE THE DIRECTORY STRUCTURE AS A MEANS OF CATEGORIZATION
            //WE BUILD THE PATH FOR EACH FILE FROM THE DATE CONTAINED IN THE JSON
            //WE USE THE ID CONTAINED IN THE JSON AS THE FILENAME
            QSettings* OrderFile = new QSettings(EMAGOrdersDirectory.path() + "/"
                    + OrdersArray.at(i).toObject()["date"].toString().left(4) + "/"
                    + OrdersArray.at(i).toObject()["date"].toString().left(7).right(2) + "/"
                    + OrdersArray.at(i).toObject()["date"].toString().left(10).right(2) + "/"
                    + QString::number(OrdersArray.at(i).toObject()["id"].toInt()) + ".order", QSettings::IniFormat);

            //HERE WE READ THE JSON REPLY AND PARSE THE DATA INTO OUR UNIVERSAL ORDER FORMAT
            OrderFile->beginGroup("ORDER_DATA");
            OrderFile->setValue("ID", QString::number(OrdersArray.at(i).toObject()["id"].toInt()));
            OrderFile->setValue("DATE", OrdersArray.at(i).toObject()["date"].toString());
            OrderFile->endGroup();

            OrderFile->beginGroup("CONTACT_DATA");
            OrderFile->setValue("CONTACT_NAME", OrdersArray.at(i).toObject()["customer"].toObject()["shipping_contact"].toString());
            OrderFile->setValue("CONTACT_PHONE", OrdersArray.at(i).toObject()["customer"].toObject()["shipping_phone"].toString());
            OrderFile->setValue("CONTACT_COUNTRY", OrdersArray.at(i).toObject()["customer"].toObject()["shipping_country"].toString());
            OrderFile->setValue("CONTACT_COUNTY", OrdersArray.at(i).toObject()["customer"].toObject()["shipping_suburb"].toString());
            OrderFile->setValue("CONTACT_CITY", OrdersArray.at(i).toObject()["customer"].toObject()["shipping_city"].toString());
            OrderFile->setValue("CONTACT_ADRESS", OrdersArray.at(i).toObject()["customer"].toObject()["shipping_street"].toString());
            OrderFile->endGroup();

            OrderFile->beginGroup("BILLING_DATA");
            //AICI TREBUIE IMPLEMENTATA SI O OPTIUNE PENTRU PERSOANE JURIDICE, CU COD CIF SI NR. REG. COM.
            OrderFile->setValue("BILLING_NAME", OrdersArray.at(i).toObject()["customer"].toObject()["billing_name"].toString());
            OrderFile->setValue("BILLING_COUNTRY", OrdersArray.at(i).toObject()["customer"].toObject()["billing_country"].toString());
            OrderFile->setValue("BILLING_COUNTY", OrdersArray.at(i).toObject()["customer"].toObject()["billing_suburb"].toString());
            OrderFile->setValue("BILLING_CITY", OrdersArray.at(i).toObject()["customer"].toObject()["billing_city"].toString());
            OrderFile->setValue("BILLING_ADRESS", OrdersArray.at(i).toObject()["customer"].toObject()["billing_street"].toString());
            OrderFile->endGroup();

            //HERE WE RECURSIVELY PARSE THE PRODUCTS IN THE ORDER INTO OUR FORMAT
            for (int j=0; j < OrdersArray.at(i).toObject()["products"].toArray().size(); j++)
            {
                OrderFile->beginGroup("PRODUCT_" + QString::number(j + 1));
                OrderFile->setValue("PRODUCT_ID", OrdersArray.at(i).toObject()["products"].toArray().at(j).toObject()["part_number"].toString());
                OrderFile->setValue("SALE_PRICE", OrdersArray.at(i).toObject()["products"].toArray().at(j).toObject()["sale_price"].toString());
                OrderFile->setValue("QUANTITY", QString::number(OrdersArray.at(i).toObject()["products"].toArray().at(j).toObject()["quantity"].toInt()));
                OrderFile->endGroup();
            }
            OrderFile->sync();
            //REMOVE THIS DATA FROM MEMORY SOMEHOW SO AS TO NOT USE TOO MUCH MEMORY
        }
        //ONCE ALL FILES ARE SAVED, WE EMIT THIS SIGNAL
        emit OrderGetComplete();
    }
}

void EMAGMdiChild::PopulateOrderViewTable()
{
    //WE CLEAR THE TABLES BEFORE REPOPULATING THEM TO PREVENT DATA INTERFERENCE AND CRASHES
    //AND ALSO BECAUSE IT MAKES SENSE IDK
    ui->OrdersView->clearSelection();
    ui->OrderDetailsView->clearSelection();
    ui->OrdersView->setRowCount(0);

    //WE USE THE CURRENTDATE OR THE DATE SET BY THE USER TO DETERMINE WHAT DIRECTORY TO LOOK INTO FOR ORDERS SAVED PREVIOUSLY
    QDir CurrentDirectory;

    //THE DATE IS DERIVED FROM THE CURRENTDATE VARIABLE AND ZEROES ARE ADDED IN ORDER TO BE COMPATIBLE WITH THE FOLDER STRUCTURE
    QString MonthWithZero = QString::number(EMAGMdiChild::CurrentDate.month());
    if (MonthWithZero.length() == 1)
    {
        MonthWithZero.prepend(QString::number(0));
    }

    QString DayWithZero = QString::number(EMAGMdiChild::CurrentDate.day());
    if (DayWithZero.length() == 1)
    {
        DayWithZero.prepend(QString::number(0));
    }

    CurrentDirectory.setPath(EMAGOrdersDirectory.path() + "/" + QString::number(EMAGMdiChild::CurrentDate.year()) + "/" + MonthWithZero + "/" + DayWithZero);

    //WE SET THE ROW COUNT FOR THE TABLE AND ADD ALL THE ITEMS RECURSIVELY
    //WE NEED TO SUBTRACT 2 BECAUSE THE FUNCTION LISTS "." and ".." AS SUBDIRECTORIES
    ui->OrdersView->setRowCount(CurrentDirectory.count() - 2);
    foreach(QString filename, CurrentDirectory.entryList())
    {
        if(filename != "." && filename != ".." && CurrentDirectory.entryList().count() > 2)
        {
            //THIS LOOP ADDS ALL THE ITEMS TO THE TABLE. THE COORDINATES ARE DERIVED BY USING THE FILENAME AS AN ITERATOR BY FINDING IT IN THE DIRECTORY LIST
            QSettings * OrderFile = new QSettings(CurrentDirectory.path() + "/" + filename, QSettings::IniFormat);
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 0, new QTableWidgetItem(QString::number(OrderFile->value("ORDER_DATA/ID").toInt())));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 1, new QTableWidgetItem(OrderFile->value("ORDER_DATA/DATE").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 2, new QTableWidgetItem(OrderFile->value("CONTACT_DATA/CONTACT_NAME").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 3, new QTableWidgetItem(OrderFile->value("CONTACT_DATA/CONTACT_PHONE").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 4, new QTableWidgetItem(OrderFile->value("CONTACT_DATA/CONTACT_COUNTRY").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 5, new QTableWidgetItem(OrderFile->value("CONTACT_DATA/CONTACT_COUNTY").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 6, new QTableWidgetItem(OrderFile->value("CONTACT_DATA/CONTACT_CITY").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 7, new QTableWidgetItem(OrderFile->value("CONTACT_DATA/CONTACT_ADRESS").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 8, new QTableWidgetItem(OrderFile->value("BILLING_DATA/BILLING_NAME").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 9, new QTableWidgetItem(OrderFile->value("BILLING_DATA/BILLING_COUNTRY").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 10, new QTableWidgetItem(OrderFile->value("BILLING_DATA/BILLING_COUNTY").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 11, new QTableWidgetItem(OrderFile->value("BILLING_DATA/BILLING_CITY").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 12, new QTableWidgetItem(OrderFile->value("BILLING_DATA/BILLING_ADRESS").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 13, new QTableWidgetItem(OrderFile->value("BILLING_DATA/BILLING_COMPANY_NAME").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 14, new QTableWidgetItem(OrderFile->value("BILLING_DATA/BILLING_CIF").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 15, new QTableWidgetItem(OrderFile->value("BILLING_DATA/BILLING_NRRC").toString()));
            ui->OrdersView->setItem(CurrentDirectory.entryList().indexOf(filename) - 2, 16, new QTableWidgetItem(OrderFile->value("BILLING_DATA/BILLING_ACCOUNT").toString()));
        }
    }
}

void EMAGMdiChild::PopulateOrderDetailsViewTable()
{
    ui->OrderDetailsView->setRowCount(0);
    QDir CurrentDirectory;
    QString MonthWithZero = QString::number(EMAGMdiChild::CurrentDate.month());
    if (MonthWithZero.length() == 1)
    {
        MonthWithZero.prepend(QString::number(0));
    }

    QString DayWithZero = QString::number(EMAGMdiChild::CurrentDate.day());
    if (DayWithZero.length() == 1)
    {
        DayWithZero.prepend(QString::number(0));
    }

    CurrentDirectory.setPath(EMAGOrdersDirectory.path() + "/" + QString::number(EMAGMdiChild::CurrentDate.year()) + "/" + MonthWithZero + "/" + DayWithZero);
    //WE NEED TO CHECK IF SOMETHING IS ACTUALLY SELECTED IN THE MAIN TABLE
    //OTHERWISE THE WHOLE PROGRAM WILL CRASH

    //ORDERS WITH MULTIPLE PRODUCTS DO NOT SHOW UP PROPERLY IN THE SECONDARY TABLE
    if (ui->OrdersView->selectedItems().count() != 0)
        {
        QSettings * OrderFile = new QSettings(CurrentDirectory.path() + "/" + ui->OrdersView->item(ui->OrdersView->selectedRanges().first().topRow(), 0)->text() + ".order", QSettings::IniFormat);
        foreach(QString group, OrderFile->childGroups())
            {
            if(group != "ORDER_DATA" && group != "BILLING_DATA" && group != "CONTACT_DATA" && group.left(8) == "PRODUCT_")
                {
                ui->OrderDetailsView->setRowCount(ui->OrderDetailsView->rowCount() + 1);
                ui->OrderDetailsView->setItem(group.mid(group.indexOf("_"), group.length()).toInt(), 0, new QTableWidgetItem(OrderFile->value(group + "/PRODUCT_ID").toString()));
                ui->OrderDetailsView->setItem(group.mid(group.indexOf("_"), group.length()).toInt(), 1, new QTableWidgetItem(OrderFile->value(group + "/QUANTITY").toString()));
                ui->OrderDetailsView->setItem(group.mid(group.indexOf("_"), group.length()).toInt(), 2, new QTableWidgetItem(OrderFile->value(group + "/SALE_PRICE").toString()));
            }
        }
    }
}

void EMAGMdiChild::on_PreviousDayButton_clicked()
{
    ui->OrdersView->clearSelection();
    ui->OrderDetailsView->clearSelection();
    ui->OrderDetailsView->setRowCount(0);
    EMAGMdiChild::CurrentDate.setDate(EMAGMdiChild::CurrentDate.addDays(-1).year(), EMAGMdiChild::CurrentDate.addDays(-1).month(), EMAGMdiChild::CurrentDate.addDays(-1).day() );
    ui->OrderDateView->setDate(EMAGMdiChild::CurrentDate);
    PopulateOrderViewTable();
}

void EMAGMdiChild::on_NextDayButton_clicked()
{
    ui->OrdersView->clearSelection();
    ui->OrderDetailsView->clearSelection();
    ui->OrderDetailsView->setRowCount(0);
    EMAGMdiChild::CurrentDate.setDate(EMAGMdiChild::CurrentDate.addDays(1).year(), EMAGMdiChild::CurrentDate.addDays(1).month(), EMAGMdiChild::CurrentDate.addDays(1).day() );
    ui->OrderDateView->setDate(EMAGMdiChild::CurrentDate);
    PopulateOrderViewTable();
}

void EMAGMdiChild::on_OrderDateView_userDateChanged(const QDate &date)
{
    ui->OrdersView->clearSelection();
    ui->OrderDetailsView->clearSelection();
    ui->OrderDetailsView->setRowCount(0);
    EMAGMdiChild::CurrentDate.setDate(date.year(), date.month(), date.day());
    PopulateOrderViewTable();
}

void EMAGMdiChild::on_OrdersView_itemSelectionChanged()
{
    PopulateOrderDetailsViewTable();
}
