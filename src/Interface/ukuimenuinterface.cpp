/*
 * Copyright (C) 2019 Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/&gt;.
 *
 */

#include "ukuimenuinterface.h"
#include <QDir>
#include <QDebug>
#include <QCollator>
#include <QLocale>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>
#include <unistd.h>
#include "ukuichineseletter.h"
#include "src/UtilityFunction/utility.h"

UkuiMenuInterface::UkuiMenuInterface()
{
}

QVector<QStringList> UkuiMenuInterface::appInfoVector=QVector<QStringList>();
QVector<QString> UkuiMenuInterface::desktopfpVector=QVector<QString>();
QVector<QStringList> UkuiMenuInterface::alphabeticVector=QVector<QStringList>();
QVector<QStringList> UkuiMenuInterface::functionalVector=QVector<QStringList>();
QVector<QString> UkuiMenuInterface::allAppVector=QVector<QString>();
QStringList UkuiMenuInterface::androidDesktopfnList=QStringList();

UkuiMenuInterface::~UkuiMenuInterface()
{
}

//文件递归查询
void UkuiMenuInterface::recursiveSearchFile(const QString& _filePath)
{
    QDir dir(_filePath);
    if (!dir.exists()) {
        return;
    }

    dir.setFilter(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsFirst);
    QFileInfoList list = dir.entryInfoList();
    list.removeAll(QFileInfo("/usr/share/applications/screensavers"));
    if(list.size()< 1 ) {
        return;
    }
    int i=0;

    //递归算法的核心部分
    do{
        QFileInfo fileInfo = list.at(i);
        //如果是文件夹，递归
        bool isDir = fileInfo.isDir();
        if(isDir) {
            recursiveSearchFile(fileInfo.filePath());
        }
        else{
            //过滤后缀不是.desktop的文件
            QString filePathStr=fileInfo.filePath();
            if(!filePathStr.endsWith(".desktop"))
            {
                i++;
                continue;
            }

            QByteArray fpbyte=filePathStr.toLocal8Bit();
            char* filepath=fpbyte.data();
            if(0!=access(filepath,R_OK))//判断文件是否可读
            {
                i++;
                continue;
            }

            keyfile=g_key_file_new();
            if(!g_key_file_load_from_file(keyfile,filepath,flags,error))
                return;

            char* ret_0 = g_key_file_get_locale_string(keyfile,"Desktop Entry","Categories", nullptr, nullptr);
            if(ret_0 != nullptr)
            {
                QString str = QString::fromLocal8Bit(ret_0);
                if(str.contains("Android"))
                {
                    g_key_file_free(keyfile);
                    i++;
                    continue;
                }
            }
            char* ret_1=g_key_file_get_locale_string(keyfile,"Desktop Entry","NoDisplay", nullptr, nullptr);
            if(ret_1!=nullptr)
            {
                QString str=QString::fromLocal8Bit(ret_1);
                if(str.contains("true"))
                {
                    g_key_file_free(keyfile);
                    i++;
                    continue;
                }
            }
            char* ret_2=g_key_file_get_locale_string(keyfile,"Desktop Entry","NotShowIn", nullptr, nullptr);
            if(ret_2!=nullptr)
            {
                QString str=QString::fromLocal8Bit(ret_2);
                if(str.contains("UKUI"))
                {
                    g_key_file_free(keyfile);
                    i++;
                    continue;
                }
            }

            //过滤LXQt、KDE
            char* ret=g_key_file_get_locale_string(keyfile,"Desktop Entry","OnlyShowIn", nullptr, nullptr);
            if(ret!=nullptr)
            {
                QString str=QString::fromLocal8Bit(ret);
                if(str.contains("LXQt") || str.contains("KDE"))
                {
                    g_key_file_free(keyfile);
                    i++;
                    continue;
                }
            }

            g_key_file_free(keyfile);
            filePathList.append(filePathStr);
        }
        i++;

    } while(i < list.size());
}

//获取系统deskyop文件路径
QStringList UkuiMenuInterface::getDesktopFilePath()
{
    filePathList.clear();

    QString jsonPath=QDir::homePath()+"/.config/ukui-menu-security-config.json";
    QFile file(jsonPath);
    if(file.exists())
    {
        file.open(QIODevice::ReadOnly);
        QByteArray readBy=file.readAll();
        QJsonParseError error;
        QJsonDocument readDoc=QJsonDocument::fromJson(readBy,&error);
        if(!readDoc.isNull() && error.error==QJsonParseError::NoError)
        {
            QJsonObject obj=readDoc.object().value("ukui-menu").toObject();
            if(obj.value("mode").toString()=="whitelist")
            {
                QJsonArray blArray=obj.value("whitelist").toArray();
                QJsonArray enArray=blArray.at(0).toObject().value("entries").toArray();
                for(int index=0;index<enArray.size();index++)
                {
                    QJsonObject obj=enArray.at(index).toObject();
                    filePathList.append(obj.value("path").toString());
//                    qDebug()<<obj.value("path").toString();
                }
                return filePathList;
            }
            else if(obj.value("mode").toString()=="blacklist")
            {
                getAndroidApp();
                recursiveSearchFile("/usr/share/applications/");
                recursiveSearchFile(QDir::homePath()+"/.local/share/applications/");
                recursiveSearchFile("/var/lib/snapd/desktop/applications/");
                recursiveSearchFile("/var/lib/flatpak/exports/share/applications/");
                QJsonArray blArray=obj.value("blacklist").toArray();
                QJsonArray enArray=blArray.at(0).toObject().value("entries").toArray();
                for(int index=0;index<enArray.size();index++)
                {
                    QJsonObject obj=enArray.at(index).toObject();
                    filePathList.removeAll(obj.value("path").toString());
//                    qDebug()<<obj.value("path").toString();
                }
            }
            else
            {
                getAndroidApp();
                recursiveSearchFile("/usr/share/applications/");
                recursiveSearchFile(QDir::homePath()+"/.local/share/applications/");
                recursiveSearchFile("/var/lib/snapd/desktop/applications/");
                recursiveSearchFile("/var/lib/flatpak/exports/share/applications/");
            }

        }

        file.close();
    }
    else
    {
        getAndroidApp();
        recursiveSearchFile("/usr/share/applications/");
        recursiveSearchFile(QDir::homePath()+"/.local/share/applications/");
        recursiveSearchFile("/var/lib/snapd/desktop/applications/");
        recursiveSearchFile("/var/lib/flatpak/exports/share/applications/");
    }

    filePathList.removeAll("/usr/share/applications/software-properties-livepatch.desktop");
    filePathList.removeAll("/usr/share/applications/mate-color-select.desktop");
    filePathList.removeAll("/usr/share/applications/blueman-adapters.desktop");
    filePathList.removeAll("/usr/share/applications/mate-user-guide.desktop");
    filePathList.removeAll("/usr/share/applications/nm-connection-editor.desktop");
    filePathList.removeAll("/usr/share/applications/debian-uxterm.desktop");
    filePathList.removeAll("/usr/share/applications/debian-xterm.desktop");
    filePathList.removeAll("/usr/share/applications/im-config.desktop");
    filePathList.removeAll("/usr/share/applications/fcitx.desktop");
    filePathList.removeAll("/usr/share/applications/fcitx-configtool.desktop");
    filePathList.removeAll("/usr/share/applications/onboard-settings.desktop");
    filePathList.removeAll("/usr/share/applications/info.desktop");
    filePathList.removeAll("/usr/share/applications/ukui-power-preferences.desktop");
    filePathList.removeAll("/usr/share/applications/ukui-power-statistics.desktop");
    filePathList.removeAll("/usr/share/applications/software-properties-drivers.desktop");
    filePathList.removeAll("/usr/share/applications/software-properties-gtk.desktop");
    filePathList.removeAll("/usr/share/applications/gnome-session-properties.desktop");
    filePathList.removeAll("/usr/share/applications/org.gnome.font-viewer.desktop");
    filePathList.removeAll("/usr/share/applications/xdiagnose.desktop");
    filePathList.removeAll("/usr/share/applications/gnome-language-selector.desktop");
    filePathList.removeAll("/usr/share/applications/mate-notification-properties.desktop");
    filePathList.removeAll("/usr/share/applications/transmission-gtk.desktop");
    filePathList.removeAll("/usr/share/applications/mpv.desktop");
    filePathList.removeAll("/usr/share/applications/system-config-printer.desktop");
    filePathList.removeAll("/usr/share/applications/org.gnome.DejaDup.desktop");
    filePathList.removeAll("/usr/share/applications/yelp.desktop");

    //v10
    filePathList.removeAll("/usr/share/applications/mate-about.desktop");
    filePathList.removeAll("/usr/share/applications/time.desktop");
    filePathList.removeAll("/usr/share/applications/network.desktop");
    filePathList.removeAll("/usr/share/applications/shares.desktop");
    filePathList.removeAll("/usr/share/applications/mate-power-statistics.desktop");
    filePathList.removeAll("/usr/share/applications/display-im6.desktop");
    filePathList.removeAll("/usr/share/applications/display-im6.q16.desktop");
    filePathList.removeAll("/usr/share/applications/openjdk-8-policytool.desktop");
    filePathList.removeAll("/usr/share/applications/kylin-io-monitor.desktop");
    filePathList.removeAll("/usr/share/applications/wps-office-uninstall.desktop");
    filePathList.removeAll("/usr/share/applications/wps-office-misc.desktop");

    return filePathList;
}

//创建应用信息容器
QVector<QStringList> UkuiMenuInterface::createAppInfoVector()
{
    desktopfpVector.clear();
    QVector<QStringList> appInfoVector;
    QVector<QStringList> vector;
    vector.append(QStringList()<<"Android");//0安卓
    vector.append(QStringList()<<"Network");//1网络
    vector.append(QStringList()<<"Messaging");//2社交
    vector.append(QStringList()<<"Audio"<<"Video");//3影音
    vector.append(QStringList()<<"Development");//4开发
    vector.append(QStringList()<<"Graphics");//5图像
    vector.append(QStringList()<<"Game");//6游戏
    vector.append(QStringList()<<"Office"<<"Calculator"<<"Spreadsheet"<<"Presentation"<<"WordProcessor"<<"TextEditor");//7办公
    vector.append(QStringList()<<"Education");//8教育
    vector.append(QStringList()<<"System"<<"Settings"<<"Security");//9系统

    QStringList desktopfpList=getDesktopFilePath();

    for(int i=0;i<desktopfpList.count();i++)
    {
        QStringList appInfoList;
        QString desktopfp=desktopfpList.at(i);
        QString name=getAppName(desktopfpList.at(i));
        if(!name.isEmpty())
        {
            QString englishName=getAppEnglishName(desktopfpList.at(i));
            QString letter=getAppNameInitial(desktopfpList.at(i));
            QString letters=getAppNameInitials(desktopfpList.at(i));

            desktopfpVector.append(desktopfp);

            appInfoList<<desktopfp<<name<<englishName<<letter<<letters;
            bool is_owned=false;
            for(int j=0;j<vector.size();j++)
            {
                if(matchingAppCategories(desktopfpList.at(i),vector.at(j)))//有对应分类
                {
                    is_owned=true;
                    appInfoList.append(QString::number(j));
                }
            }
            if(!is_owned)//该应用无对应分类
                appInfoList.append(QString::number(10));

            appInfoVector.append(appInfoList);
        }
    }

    return appInfoVector;
}

//获取应用名称
QString UkuiMenuInterface::getAppName(QString desktopfp)
{
    GError** error=nullptr;
    GKeyFileFlags flags=G_KEY_FILE_NONE;
    GKeyFile* keyfile=g_key_file_new ();

    QByteArray fpbyte=desktopfp.toLocal8Bit();
    char* filepath=fpbyte.data();
    g_key_file_load_from_file(keyfile,filepath,flags,error);

    char* name=g_key_file_get_locale_string(keyfile,"Desktop Entry","Name", nullptr, nullptr);
    QString namestr=QString::fromLocal8Bit(name);

    g_key_file_free(keyfile);
    return namestr;
}

//获取英应用英文名
QString UkuiMenuInterface::getAppEnglishName(QString desktopfp)
{
    GError** error=nullptr;
    GKeyFileFlags flags=G_KEY_FILE_NONE;
    GKeyFile* keyfile=g_key_file_new ();

    QByteArray fpbyte=desktopfp.toLocal8Bit();
    char* filepath=fpbyte.data();
    g_key_file_load_from_file(keyfile,filepath,flags,error);
    char* name=g_key_file_get_string(keyfile,"Desktop Entry","Name", nullptr);
    QString namestr=QString::fromLocal8Bit(name);
    return namestr;
}

//获取应用分类
QString UkuiMenuInterface::getAppCategories(QString desktopfp)
{
    GError** error=nullptr;
    GKeyFileFlags flags=G_KEY_FILE_NONE;
    GKeyFile* keyfile=g_key_file_new ();

    QByteArray fpbyte=desktopfp.toLocal8Bit();
    char* filepath=fpbyte.data();
    g_key_file_load_from_file(keyfile,filepath,flags,error);
    char* category=g_key_file_get_locale_string(keyfile,"Desktop Entry","Categories", nullptr, nullptr);
    g_key_file_free(keyfile);
    return QString::fromLocal8Bit(category);
}

//获取应用图标
QString UkuiMenuInterface::getAppIcon(QString desktopfp)
{   
    GError** error=nullptr;
    GKeyFileFlags flags=G_KEY_FILE_NONE;
    GKeyFile* keyfile=g_key_file_new ();

    QByteArray fpbyte=desktopfp.toLocal8Bit();
    char* filepath=fpbyte.data();
    g_key_file_load_from_file(keyfile,filepath,flags,error);
    char* icon=g_key_file_get_locale_string(keyfile,"Desktop Entry","Icon", nullptr, nullptr);
    g_key_file_free(keyfile);
    return QString::fromLocal8Bit(icon);
}

//获取应用命令
QString UkuiMenuInterface::getAppExec(QString desktopfp)
{
    GError** error=nullptr;
    GKeyFileFlags flags=G_KEY_FILE_NONE;
    GKeyFile* keyfile=g_key_file_new ();

    QByteArray fpbyte=desktopfp.toLocal8Bit();
    char* filepath=fpbyte.data();
    g_key_file_load_from_file(keyfile,filepath,flags,error);
    char* exec=g_key_file_get_locale_string(keyfile,"Desktop Entry","Exec", nullptr, nullptr);
    g_key_file_free(keyfile);
    return QString::fromLocal8Bit(exec);
}

//获取应用注释
QString UkuiMenuInterface::getAppComment(QString desktopfp)
{
    GError** error=nullptr;
    GKeyFileFlags flags=G_KEY_FILE_NONE;
    GKeyFile* keyfile=g_key_file_new ();

    QByteArray fpbyte=desktopfp.toLocal8Bit();
    char* filepath=fpbyte.data();
    g_key_file_load_from_file(keyfile,filepath,flags,error);
    char* comment=g_key_file_get_locale_string(keyfile,"Desktop Entry","Comment", nullptr, nullptr);
    g_key_file_free(keyfile);
    return QString::fromLocal8Bit(comment);

}

//获取应用类型
QString UkuiMenuInterface::getAppType(QString desktopfp)
{
    GError** error=nullptr;
    GKeyFileFlags flags=G_KEY_FILE_NONE;
    GKeyFile* keyfile=g_key_file_new ();

    QByteArray fpbyte=desktopfp.toLocal8Bit();
    char* filepath=fpbyte.data();
    g_key_file_load_from_file(keyfile,filepath,flags,error);
    char* type=g_key_file_get_locale_string(keyfile,"Desktop Entry","Type", nullptr, nullptr);
    g_key_file_free(keyfile);
    return QString::fromLocal8Bit(type);
}

bool UkuiMenuInterface::cmpApp(QStringList &arg_1, QStringList &arg_2)
{
    QLocale local;
    QString language=local.languageToString(local.language());
    if(QString::compare(language,"Chinese")==0)
        local=QLocale(QLocale::Chinese);
    else
        local=QLocale(QLocale::English);
    QCollator collator(local);
    if(collator.compare(arg_1.at(1),arg_2.at(1))<0)
        return true;
    else
        return false;
}

QVector<QString> UkuiMenuInterface::getAllClassification()
{
    QVector<QString> allAppVector;
    QVector<QString> commonVector;
    QVector<QStringList> appVector;
    allAppVector.clear();
    commonVector.clear();
    appVector.clear();
    commonVector=getCommonUseApp();
    int index=0;
    Q_FOREACH(QString desktopfp, desktopfpVector)
    {
        if(!commonVector.contains(desktopfp))
            appVector.append(appInfoVector.at(index));
        index++;
    }
    qSort(appVector.begin(),appVector.end(),cmpApp);
    Q_FOREACH(QString desktopfp, commonVector)
        allAppVector.append(desktopfp);

    for(int index=0;index<appVector.size();index++)
        allAppVector.append(appVector.at(index).at(0));

    return allAppVector;
}

QVector<QString> UkuiMenuInterface::getCommonUseApp()
{
//    QDateTime dt=QDateTime::currentDateTime();
//    int currentDateTime=dt.toTime_t();
//    int nDaySec=24*60*60;
//    setting->beginGroup("datetime");
//    QStringList dateTimeKeys=setting->allKeys();
//    QStringList timeOutKeys;
//    timeOutKeys.clear();
//    for(int i=0;i<dateTimeKeys.count();i++)
//    {
//        if((currentDateTime-setting->value(dateTimeKeys.at(i)).toInt())/nDaySec >= 4)
//        {
//            timeOutKeys.append(dateTimeKeys.at(i));
//        }

//    }
//    setting->endGroup();
//    for(int i=0;i<timeOutKeys.count();i++)
//    {
//        setting->beginGroup("application");
//        setting->remove(timeOutKeys.at(i));
//        setting->sync();
//        setting->endGroup();
//        setting->beginGroup("datetime");
//        setting->remove(timeOutKeys.at(i));
//        setting->sync();
//        setting->endGroup();
//    }

//    setting->beginGroup("lockapplication");
//    QStringList lockdesktopfnList=setting->allKeys();
//    for(int i=0;i<lockdesktopfnList.count()-1;i++)
//        for(int j=0;j<lockdesktopfnList.count()-1-i;j++)
//        {
//            int value_1=setting->value(lockdesktopfnList.at(j)).toInt();
//            int value_2=setting->value(lockdesktopfnList.at(j+1)).toInt();
//            if(value_1 > value_2)
//            {
//                QString tmp=lockdesktopfnList.at(j);
//                lockdesktopfnList.replace(j,lockdesktopfnList.at(j+1));
//                lockdesktopfnList.replace(j+1,tmp);

//            }
//        }
//    setting->endGroup();
//    setting->beginGroup("application");
//    QStringList desktopfnList=setting->allKeys();
//    for(int i=0;i<desktopfnList.count()-1;i++)
//        for(int j=0;j<desktopfnList.count()-1-i;j++)
//        {
//            int value_1=setting->value(desktopfnList.at(j)).toInt();
//            int value_2=setting->value(desktopfnList.at(j+1)).toInt();
//            if(value_1 < value_2)
//            {
//                QString tmp=desktopfnList.at(j);
//                desktopfnList.replace(j,desktopfnList.at(j+1));
//                desktopfnList.replace(j+1,tmp);

//            }
//        }
//    setting->endGroup();

//    QVector<QString> data;
//    Q_FOREACH(QString desktopfn,lockdesktopfnList)
//    {
//        QString desktopfp;
//        if(androidDesktopfnList.contains(desktopfn))
//            desktopfp=QString(QDir::homePath()+"/.local/share/applications/"+desktopfn);
//        else
//            desktopfp=QString("/usr/share/applications/"+desktopfn);
//        QFileInfo fileInfo(desktopfp);
//        if(!fileInfo.isFile()|| !desktopfpVector.contains(desktopfp))
//            continue;
//        data.append(desktopfp);
//    }
//    Q_FOREACH(QString desktopfn,desktopfnList)
//    {
//        QString desktopfp;
//        if(androidDesktopfnList.contains(desktopfn))
//            desktopfp=QString(QDir::homePath()+"/.local/share/applications/"+desktopfn);
//        else
//            desktopfp=QString("/usr/share/applications/"+desktopfn);
//        QFileInfo fileInfo(desktopfp);
//        if(!fileInfo.isFile() || !desktopfpVector.contains(desktopfp))
//            continue;
//        data.append(desktopfp);
//    }

    QVector<QString> data;
    Q_FOREACH(QString desktopfn, getLockAppList())
    {
        QString desktopfp;
        if(androidDesktopfnList.contains(desktopfn))
            desktopfp=QString(QDir::homePath()+"/.local/share/applications/"+desktopfn);
        else
            desktopfp=QString("/usr/share/applications/"+desktopfn);
        QFileInfo fileInfo(desktopfp);
        if(!fileInfo.isFile() || !desktopfpVector.contains(desktopfp))
            continue;
        data.append(desktopfp);
    }

    Q_FOREACH(QString desktopfn, getUnlockAllList())
    {
        QString desktopfp;
        if(androidDesktopfnList.contains(desktopfn))
            desktopfp=QString(QDir::homePath()+"/.local/share/applications/"+desktopfn);
        else
            desktopfp=QString("/usr/share/applications/"+desktopfn);
        QFileInfo fileInfo(desktopfp);
        if(!fileInfo.isFile() || !desktopfpVector.contains(desktopfp))
            continue;
        data.append(desktopfp);
    }

    return data;
}

QVector<QStringList> UkuiMenuInterface::getAlphabeticClassification()
{
    QVector<QStringList> data;
    QStringList appnameList;
    appnameList.clear();
    QVector<QStringList> appVector[27];
    int index=0;
    while(index<appInfoVector.size())
    {
        QString appname=appInfoVector.at(index).at(1);
        QString appnamepy=UkuiChineseLetter::getPinyins(appname);
        if(!appnamepy.isEmpty())
        {
            char c=appnamepy.at(0).toLatin1();
            switch (c) {
            case 'A':
                appVector[0].append(appInfoVector.at(index));
                break;
            case 'B':
                appVector[1].append(appInfoVector.at(index));
                break;
            case 'C':
                appVector[2].append(appInfoVector.at(index));
                break;
            case 'D':
                appVector[3].append(appInfoVector.at(index));
                break;
            case 'E':
                appVector[4].append(appInfoVector.at(index));
                break;
            case 'F':
                appVector[5].append(appInfoVector.at(index));
                break;
            case 'G':
                appVector[6].append(appInfoVector.at(index));
                break;
            case 'H':
                appVector[7].append(appInfoVector.at(index));
                break;
            case 'I':
                appVector[8].append(appInfoVector.at(index));
                break;
            case 'J':
                appVector[9].append(appInfoVector.at(index));
                break;
            case 'K':
                appVector[10].append(appInfoVector.at(index));
                break;
            case 'L':
                appVector[11].append(appInfoVector.at(index));
                break;
            case 'M':
                appVector[12].append(appInfoVector.at(index));
                break;
            case 'N':
                appVector[13].append(appInfoVector.at(index));
                break;
            case 'O':
                appVector[14].append(appInfoVector.at(index));
                break;
            case 'P':
                appVector[15].append(appInfoVector.at(index));
                break;
            case 'Q':
                appVector[16].append(appInfoVector.at(index));
                break;
            case 'R':
                appVector[17].append(appInfoVector.at(index));
                break;
            case 'S':
                appVector[18].append(appInfoVector.at(index));
                break;
            case 'T':
                appVector[19].append(appInfoVector.at(index));
                break;
            case 'U':
                appVector[20].append(appInfoVector.at(index));
                break;
            case 'V':
                appVector[21].append(appInfoVector.at(index));
                break;
            case 'W':
                appVector[22].append(appInfoVector.at(index));
                break;
            case 'X':
                appVector[23].append(appInfoVector.at(index));
                break;
            case 'Y':
                appVector[24].append(appInfoVector.at(index));
                break;
            case 'Z':
                appVector[25].append(appInfoVector.at(index));
                break;
            default:
                appVector[26].append(appInfoVector.at(index));
                break;
            }
        }
        index++;
    }

    for(int i=0;i<26;i++)
    {
        QStringList desktopfpList;
        desktopfpList.clear();
        qSort(appVector[i].begin(),appVector[i].end(),cmpApp);
        for(int j=0;j<appVector[i].size();j++)
            desktopfpList.append(appVector[i].at(j).at(0));
        data.append(desktopfpList);
    }

     QVector<QStringList> otherVector;
     QVector<QStringList> numberVector;
    for(int i=0;i<appVector[26].count();i++)
    {
        QString appname=appVector[26].at(i).at(2);
        QChar c=appname.at(0);
        if(c<48 || (c>57 && c<65) || c>90)
            otherVector.append(appVector[26].at(i));
        else
            numberVector.append(appVector[26].at(i));
    }
    qSort(otherVector.begin(),otherVector.end(),cmpApp);
    qSort(numberVector.begin(),numberVector.end(),cmpApp);
    QStringList otherfpList;
    otherfpList.clear();
    for(int i=0;i<otherVector.size();i++)
        otherfpList.append(otherVector.at(i).at(0));
    QStringList numberfpList;
    numberfpList.clear();
    for(int i=0;i<numberVector.size();i++)
        numberfpList.append(numberVector.at(i).at(0));
    data.append(otherfpList);
    data.append(numberfpList);

    return data;
}

QVector<QStringList> UkuiMenuInterface::getFunctionalClassification()
{
    QVector<QStringList> appVector[11];
    int index=0;
    while(index<appInfoVector.size())
    {
        int count=appInfoVector.at(index).size()-5;
        for(int i=0;i<count;i++)
        {
            int category=appInfoVector.at(index).at(5+i).toInt();
            switch (category) {
            case 0:
                appVector[0].append(appInfoVector.at(index));
                break;
            case 1:
                appVector[1].append(appInfoVector.at(index));
                break;
            case 2:
                appVector[2].append(appInfoVector.at(index));
                break;
            case 3:
                appVector[3].append(appInfoVector.at(index));
                break;
            case 4:
                appVector[4].append(appInfoVector.at(index));
                break;
            case 5:
                appVector[5].append(appInfoVector.at(index));
                break;
            case 6:
                appVector[6].append(appInfoVector.at(index));
                break;
            case 7:
                appVector[7].append(appInfoVector.at(index));
                break;
            case 8:
                appVector[8].append(appInfoVector.at(index));
                break;
            case 9:
                appVector[9].append(appInfoVector.at(index));
                break;
            case 10:
                appVector[10].append(appInfoVector.at(index));
                break;
            default:
                break;
            }
        }
        index++;
    }

    QVector<QStringList> data;
    data.clear();

    for(int i=0;i<11;i++)
    {
        QStringList desktopfpList;
        desktopfpList.clear();
        qSort(appVector[i].begin(),appVector[i].end(),cmpApp);
        for(int j=0;j<appVector[i].size();j++)
            desktopfpList.append(appVector[i].at(j).at(0));
        data.append(desktopfpList);
    }
    return data;
}

bool UkuiMenuInterface::matchingAppCategories(QString desktopfp, QStringList categorylist)
{
    QString category=getAppCategories(desktopfp);
    int index;
    for(index=0;index<categorylist.count();index++)
    {
        if(category.contains(categorylist.at(index),Qt::CaseInsensitive))
            return true;
    }
    if(index==categorylist.count())
        return false;

    return false;
}

void UkuiMenuInterface::getAndroidApp()
{
    androidDesktopfnList.clear();
    QVector<QStringList> androidVector;
    androidVector.clear();
    QString path=QDir::homePath()+"/.local/share/applications/";
    QDir dir(path);
    if (!dir.exists()) {
        return;
    }

    dir.setFilter(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsFirst);
    QFileInfoList list = dir.entryInfoList();
    if(list.size()< 1 ) {
        return;
    }
    int i=0;

    GError** error=nullptr;
    GKeyFileFlags flags=G_KEY_FILE_NONE;
    GKeyFile* keyfile=g_key_file_new ();

    do{
        QFileInfo fileInfo = list.at(i);
        if(!fileInfo.isFile())
        {
            i++;
            continue;
        }
        //过滤后缀不是.desktop的文件
        QString filePathStr=fileInfo.filePath();
        if(!filePathStr.endsWith(".desktop"))
        {
            i++;
            continue;
        }

        QByteArray fpbyte=filePathStr.toLocal8Bit();
        char* filepath=fpbyte.data();
        g_key_file_load_from_file(keyfile,filepath,flags,error);
        char* ret_1=g_key_file_get_locale_string(keyfile,"Desktop Entry","Categories", nullptr, nullptr);
        if(ret_1!=nullptr)
        {
            QString str=QString::fromLocal8Bit(ret_1);
            if(!str.contains("Android"))
            {
                i++;
                continue;
            }
            else
            {
                filePathList.append(filePathStr);
                androidDesktopfnList.append(fileInfo.fileName());
            }
        }

        i++;
    }while(i < list.size());

    g_key_file_free(keyfile);
}

QString UkuiMenuInterface::getAppNameInitials(QString desktopfp)
{
    QString firstLetters;
    QString appname=getAppName(desktopfp);
    QStringList appnamestr=appname.split(" ");
    QString letters;
    Q_FOREACH(QString name,appnamestr)
    {
        letters.clear();
        letters=UkuiChineseLetter::getFirstLettersAll(name);
        if(letters.isEmpty())
            letters=UkuiChineseLetter::getFirstLetter(name);
        firstLetters.append(letters);
    }

    return firstLetters;
}

QString UkuiMenuInterface::getAppNameInitial(QString desktopfp)
{
    return UkuiChineseLetter::getFirstLetter(getAppName(desktopfp));
}

//获取应用拼音
QString UkuiMenuInterface::getAppNamePinyin(QString appname)
{
    return UkuiChineseLetter::getPinyins(appname);
}

bool UkuiMenuInterface::checkKreApp(QString desktopfp)
{
    GError** error=nullptr;
    GKeyFileFlags flags=G_KEY_FILE_NONE;
    GKeyFile* keyfile=g_key_file_new ();

    QByteArray fpbyte=desktopfp.toLocal8Bit();
    char* filepath=fpbyte.data();
    g_key_file_load_from_file(keyfile,filepath,flags,error);

    char* value=g_key_file_get_locale_string(keyfile,"Desktop Entry","environment", nullptr, nullptr);
    g_key_file_free(keyfile);
    if(!value)
        return false;
    else if(QString::compare(QString::fromLocal8Bit(value),"kre-environment")==0)
        return true;
    return false;
}

//获取指定类型应用列表
QStringList UkuiMenuInterface::getSpecifiedCategoryAppList(QString categorystr)
{
    QByteArray categorybyte=categorystr.toLocal8Bit();
    char* category=categorybyte.data();

    QStringList desktopfpList=getDesktopFilePath();
    QStringList appnameList;
    appnameList.clear();
    for(int index=0;index<desktopfpList.count();index++)
    {
        QString appcategorystr=getAppCategories(desktopfpList.at(index));
        if(!appcategorystr.isEmpty())
        {
            QByteArray appcategorybyte=appcategorystr.toLocal8Bit();
            char* appcategory=appcategorybyte.data();
            int len=static_cast<int>(strlen(appcategory)-6);
            for(int i=0;i<len;i++)
            {
                if(strncmp(appcategory+i,category,strlen(category))==0)
                {
                    QString appname=getAppName(desktopfpList.at(index));
                    if(QString::compare(appname,"访问提示")==0)
                    appnameList.append(appname);
                    break;
                }
            }

        }
    }
    return appnameList;

}
