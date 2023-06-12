#include "qsocprojectmanager.h"

#include <fstream>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QVersionNumber>

#include "yaml-cpp/yaml.h"

QSocProjectManager::QSocProjectManager(QObject *parent)
    : QObject{parent}
{
    /* Get system environments */
    const QStringList envList = QProcess::systemEnvironment();
    /* Save system environments into QMap */
    foreach (const QString str, envList) {
        QStringList keyAndValue = str.split('=');
        if (keyAndValue.size() == 2) {
            env[keyAndValue[0]] = keyAndValue[1];
        }
    }
    /* Set project default name */
    projectName = "";
    /* Set project default paths */
    projectPath   = QDir::currentPath();
    busPath       = QDir::currentPath() + "/bus";
    symbolPath    = QDir::currentPath() + "/symbol";
    schematicPath = QDir::currentPath() + "/schematic";
    outputPath    = QDir::currentPath() + "/output";
    /* Add socstudio system environments */
    env["SOCSTUDIO_PROJECT_DIR"] = projectPath;
}

void QSocProjectManager::setEnv(const QString &key, const QString &value)
{
    env[key] = value;
}

void QSocProjectManager::setEnv(const QMap<QString, QString> &env)
{
    this->env = env;
}

QString QSocProjectManager::getSimplifyPath(const QString &path)
{
    QString result = path;
    /* Substitute path to environment variables */
    QMapIterator<QString, QString> iterator(env);
    while (iterator.hasNext()) {
        iterator.next();
        if (iterator.key().contains("SOCSTUDIO_")) {
            const QString pattern = QString("${%1}").arg(iterator.key());
            result                = result.replace(iterator.value(), pattern);
        }
    }
    return result;
}

QString QSocProjectManager::getExpandPath(const QString &path)
{
    QString result = path;
    /* Substitute environment variables */
    QMapIterator<QString, QString> iterator(env);
    while (iterator.hasNext()) {
        iterator.next();
        const QString pattern = QString("${%1}").arg(iterator.key());
        result                = result.replace(pattern, iterator.value());
    }
    return result;
}

bool QSocProjectManager::create(const QString &projectName)
{
    /* Create project directory */
    if (!QDir().mkpath(projectPath)) {
        qCritical() << "Error: failed to create bus directory.";
        return false;
    }
    if (!QDir().mkpath(busPath)) {
        qCritical() << "Error: failed to create bus directory.";
        return false;
    }
    if (!QDir().mkpath(symbolPath)) {
        qCritical() << "Error: failed to create symbol directory.";
        return false;
    }
    if (!QDir().mkpath(schematicPath)) {
        qCritical() << "Error: failed to create schematic directory.";
        return false;
    }
    if (!QDir().mkpath(outputPath)) {
        qCritical() << "Error: failed to create output directory.";
        return false;
    }

    YAML::Node projectNode;
    projectNode["version"]   = QCoreApplication::applicationVersion().toStdString();
    projectNode["bus"]       = getSimplifyPath(busPath).toStdString();
    projectNode["symbol"]    = getSimplifyPath(symbolPath).toStdString();
    projectNode["schematic"] = getSimplifyPath(schematicPath).toStdString();
    projectNode["output"]    = getSimplifyPath(outputPath).toStdString();

    const QString &projectFilePath = QString(projectPath + "/" + projectName + ".soc_pro");
    std::ofstream  outputFileStream(projectFilePath.toStdString());
    outputFileStream << projectNode;

    this->projectName = projectName;

    return true;
}

bool QSocProjectManager::load(const QString &projectFilePath)
{
    QString filePath = projectFilePath;
    /* If path is a directory, search and pick a *.soc_pro file */
    if (QFileInfo(projectFilePath).isDir()) {
        QDir dir(projectFilePath);
        dir.setNameFilters(QStringList() << "*.soc_pro");
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        if (dir.count() == 0) {
            qCritical() << "Error: project file not found.";
            return false;
        }
        filePath = dir.absoluteFilePath(dir[0]);
    }
    /* Check the existence of project files */
    if (!QFile::exists(filePath)) {
        qCritical() << "Error: project file not found.";
        return false;
    }
    /* Load project file */
    YAML::Node projectNode = YAML::LoadFile(filePath.toStdString());
    /* Check project file version */
    const QVersionNumber projectVersion = QVersionNumber::fromString(
        QString::fromStdString(projectNode["version"].as<std::string>()));
    const QVersionNumber appVersion = QVersionNumber::fromString(
        QCoreApplication::applicationVersion());
    if (projectVersion > appVersion) {
        qCritical() << "Error: project file version is newer than application version.";
        return false;
    }
    /* Set project name */
    projectName = filePath.split('/').last().split('.').first();
    /* Set project paths */
    projectPath   = QFileInfo(filePath).absoluteDir().absolutePath();
    busPath       = getExpandPath(QString::fromStdString(projectNode["bus"].as<std::string>()));
    symbolPath    = getExpandPath(QString::fromStdString(projectNode["symbol"].as<std::string>()));
    schematicPath = getExpandPath(
        QString::fromStdString(projectNode["schematic"].as<std::string>()));
    outputPath = getExpandPath(QString::fromStdString(projectNode["output"].as<std::string>()));
    /* Add socstudio system environments */
    env["SOCSTUDIO_PROJECT_DIR"] = projectPath;

    return true;
}

const QString &QSocProjectManager::getProjectPath()
{
    return projectPath;
}

const QString &QSocProjectManager::getBusPath()
{
    return busPath;
}

const QString &QSocProjectManager::getSymbolPath()
{
    return symbolPath;
}

const QString &QSocProjectManager::getSchematicPath()
{
    return schematicPath;
}

const QString &QSocProjectManager::getOutputPath()
{
    return outputPath;
}

void QSocProjectManager::setProjectPath(const QString &projectPath)
{
    this->projectPath            = projectPath;
    env["SOCSTUDIO_PROJECT_DIR"] = projectPath;
}

void QSocProjectManager::setBusPath(const QString &busPath)
{
    this->busPath = busPath;
}

void QSocProjectManager::setSymbolPath(const QString &symbolPath)
{
    this->symbolPath = symbolPath;
}

void QSocProjectManager::setSchematicPath(const QString &schematicPath)
{
    this->schematicPath = schematicPath;
}

void QSocProjectManager::setOutputPath(const QString &outputPath)
{
    this->outputPath = outputPath;
}
