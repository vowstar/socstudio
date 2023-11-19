#include "common/qsocmodulemanager.h"

#include "common/qslangdriver.h"

#include <QDebug>
#include <QDir>
#include <QFile>

#include <fstream>

QSocModuleManager::QSocModuleManager(QObject *parent, QSocProjectManager *projectManager)
    : QObject{parent}
{
    /* Set projectManager */
    setProjectManager(projectManager);
}

void QSocModuleManager::setProjectManager(QSocProjectManager *projectManager)
{
    /* Set projectManager */
    if (projectManager) {
        this->projectManager = projectManager;
    }
}

QSocProjectManager *QSocModuleManager::getProjectManager()
{
    return projectManager;
}

bool QSocModuleManager::isModulePathValid()
{
    /* Validate projectManager */
    if (!projectManager) {
        qCritical() << "Error: projectManager is nullptr.";
        return false;
    }
    /* Validate module path. */
    if (!projectManager->isValidModulePath()) {
        qCritical() << "Error: Invalid module path:" << projectManager->getModulePath();
        return false;
    }
    return true;
}

YAML::Node QSocModuleManager::mergeNodes(const YAML::Node &toYaml, const YAML::Node &fromYaml)
{
    if (!fromYaml.IsMap()) {
        /* If fromYaml is not a map, merge result is fromYaml, unless fromYaml is null */
        return fromYaml.IsNull() ? toYaml : fromYaml;
    }
    if (!toYaml.IsMap()) {
        /* If toYaml is not a map, merge result is fromYaml */
        return fromYaml;
    }
    if (!fromYaml.size()) {
        /* If toYaml is a map, and fromYaml is an empty map, return toYaml */
        return toYaml;
    }
    /* Create a new map 'resultYaml' with the same mappings as toYaml, merged with fromYaml */
    YAML::Node resultYaml = YAML::Node(YAML::NodeType::Map);
    for (auto iter : toYaml) {
        if (iter.first.IsScalar()) {
            const std::string &key      = iter.first.Scalar();
            auto               tempYaml = YAML::Node(fromYaml[key]);
            if (tempYaml) {
                resultYaml[iter.first] = mergeNodes(iter.second, tempYaml);
                continue;
            }
        }
        resultYaml[iter.first] = iter.second;
    }
    /* Add the mappings from 'fromYaml' not already in 'resultYaml' */
    for (auto iter : fromYaml) {
        if (!iter.first.IsScalar() || !resultYaml[iter.first.Scalar()]) {
            resultYaml[iter.first] = iter.second;
        }
    }
    return resultYaml;
}

bool QSocModuleManager::importFromFileList(
    const QString            &libraryName,
    const QRegularExpression &moduleNameRegex,
    const QString            &fileListPath,
    const QStringList        &filePathList)
{
    /* Validate projectManager and its path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return false;
    }

    QSlangDriver driver(this, projectManager);
    if (driver.parseFileList(fileListPath, filePathList)) {
        /* Parse success */
        QStringList moduleList = driver.getModuleList();
        if (moduleList.isEmpty()) {
            /* No module found */
            qCritical() << "Error: no module found.";
            return false;
        }
        YAML::Node libraryYaml;
        QString    locallibraryName = libraryName;

        if (moduleNameRegex.pattern().isEmpty()) {
            /* Pick first module if pattern is empty */
            const QString &moduleName = moduleList.first();
            qDebug() << "Pick first module:" << moduleName;
            if (locallibraryName.isEmpty()) {
                /* Use first module name as library filename */
                locallibraryName = moduleName.toLower();
                qDebug() << "Pick library filename:" << locallibraryName;
            }
            const json       &moduleAst  = driver.getModuleAst(moduleName);
            const YAML::Node &moduleYaml = getModuleYaml(moduleAst);
            /* Add module to library yaml */
            libraryYaml[moduleName.toStdString()] = moduleYaml;
            saveLibraryYaml(locallibraryName, libraryYaml);
            return true;
        }
        /* Find module by pattern */
        bool hasMatch = false;
        for (const QString &moduleName : moduleList) {
            const QRegularExpressionMatch &match = moduleNameRegex.match(moduleName);
            if (match.hasMatch()) {
                qDebug() << "Found module:" << moduleName;
                if (locallibraryName.isEmpty()) {
                    /* Use first module name as library filename */
                    locallibraryName = moduleName.toLower();
                    qDebug() << "Pick library filename:" << locallibraryName;
                }
                const json       &moduleAst           = driver.getModuleAst(moduleName);
                const YAML::Node &moduleYaml          = getModuleYaml(moduleAst);
                libraryYaml[moduleName.toStdString()] = moduleYaml;
                hasMatch                              = true;
            }
        }
        if (hasMatch) {
            saveLibraryYaml(locallibraryName, libraryYaml);
            return true;
        }
    }
    qCritical() << "Error: no module found.";
    return false;
}

YAML::Node QSocModuleManager::getModuleYaml(const json &moduleAst)
{
    YAML::Node moduleYaml;
    if (moduleAst.contains("kind") && moduleAst.contains("name") && moduleAst.contains("body")
        && moduleAst["kind"] == "Instance" && moduleAst["body"].contains("members")) {
        const QString &kind = QString::fromStdString(moduleAst["kind"]);
        const QString &name = QString::fromStdString(moduleAst["name"]);
        const json    &body = moduleAst["body"];
        for (const json &member : moduleAst["body"]["members"]) {
            if (member.contains("kind") && member.contains("name") && member.contains("type")) {
                const QString &memberKind = QString::fromStdString(member["kind"]);
                const QString &memberName = QString::fromStdString(member["name"]);
                const QString &memberType = QString::fromStdString(member["type"]);
                if (memberKind == "Port") {
                    moduleYaml["port"][memberName.toStdString()]["type"] = memberType.toStdString();
                } else if (memberKind == "Parameter") {
                    moduleYaml["parameter"][memberName.toStdString()]["type"]
                        = memberType.toStdString();
                    if (member.contains("value")) {
                        const QString &memberValue = QString::fromStdString(member["value"]);
                        moduleYaml["parameter"][memberName.toStdString()]["value"]
                            = memberValue.toStdString();
                    }
                }
            }
        }
    }
    return moduleYaml;
}

bool QSocModuleManager::saveLibraryYaml(const QString &libraryName, const YAML::Node &libraryYaml)
{
    YAML::Node localLibraryYaml;
    /* Validate projectManager and its path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return false;
    }
    /* Check file path */
    const QString &modulePath = projectManager->getModulePath();
    const QString &filePath   = modulePath + "/" + libraryName + ".soc_sym";
    if (QFile::exists(filePath)) {
        /* Load library YAML file */
        std::ifstream inputFileStream(filePath.toStdString());
        localLibraryYaml = mergeNodes(YAML::Load(inputFileStream), libraryYaml);
        qDebug() << "Load and merge";
    } else {
        localLibraryYaml = libraryYaml;
    }

    /* Save YAML file */
    std::ofstream outputFileStream(filePath.toStdString());
    outputFileStream << localLibraryYaml;
    return true;
}

QStringList QSocModuleManager::listLibrary(const QRegularExpression &libraryNameRegex)
{
    QStringList result;
    /* Validate projectManager and its path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return result;
    }
    /* QDir for '.soc_sym' files in module path, sorted by name. */
    const QDir modulePathDir(
        projectManager->getModulePath(),
        "*.soc_sym",
        QDir::SortFlag::Name | QDir::SortFlag::IgnoreCase,
        QDir::Files | QDir::NoDotAndDotDot);
    /* Add matching file basenames from projectDir to result list. */
    foreach (const QString &filename, modulePathDir.entryList()) {
        if (libraryNameRegex.match(filename).hasMatch()) {
            result.append(filename.split('.').first());
        }
    }

    return result;
}

bool QSocModuleManager::isExist(const QString &libraryName)
{
    /* Validate projectManager and its module path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return false;
    }

    /* Check library basename */
    if (libraryName.isEmpty()) {
        qCritical() << "Error: library basename is empty.";
        return false;
    }

    /* Get the full file path by joining module path and basename with extension */
    const QString filePath
        = QDir(projectManager->getModulePath()).filePath(libraryName + ".soc_sym");

    /* Check if library file exists */
    return QFile::exists(filePath);
}

bool QSocModuleManager::load(const QString &libraryName)
{
    /* Validate projectManager and its path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return false;
    }

    /* Check if library file exists */
    if (!isExist(libraryName)) {
        qCritical() << "Error: Library file does not exist for basename:" << libraryName;
        return false;
    }

    /* Get the full file path by joining module path and basename with extension */
    const QString filePath
        = QDir(projectManager->getModulePath()).filePath(libraryName + ".soc_sym");

    /* Open the YAML file */
    std::ifstream fileStream(filePath.toStdString());
    if (!fileStream.is_open()) {
        qCritical() << "Error: Unable to open file:" << filePath;
        return false;
    }

    try {
        /* Load YAML content into a temporary node */
        YAML::Node tempNode = YAML::Load(fileStream);

        /* Iterate through the temporary node and add to moduleData */
        for (YAML::const_iterator it = tempNode.begin(); it != tempNode.end(); ++it) {
            const auto key = it->first.as<std::string>();

            /* Add to moduleData */
            moduleData[key] = it->second;

            /* Update libraryMap with libraryName to key mapping */
            libraryMap[libraryName] = QString::fromStdString(key);
        }
    } catch (const YAML::Exception &e) {
        qCritical() << "Error parsing YAML file:" << filePath << ":" << e.what();
        return false;
    }

    return true;
}

bool QSocModuleManager::load(const QRegularExpression &libraryNameRegex)
{
    /* Validate projectManager and its path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return false;
    }

    /* Get the list of library basenames matching the regex */
    const QStringList matchingBasenames = listLibrary(libraryNameRegex);

    /* Iterate through the list and load each library */
    for (const QString &basename : matchingBasenames) {
        if (!load(basename)) {
            qCritical() << "Error: Failed to load library:" << basename;
            return false;
        }
    }

    return true;
}

bool QSocModuleManager::load(const QStringList &libraryNameList)
{
    if (!projectManager || !projectManager->isValid()) {
        qCritical() << "Error: Invalid or null projectManager.";
        return false;
    }

    const QSet<QString> uniqueBasenames(libraryNameList.begin(), libraryNameList.end());

    for (const QString &basename : uniqueBasenames) {
        if (!load(basename)) {
            qCritical() << "Error: Failed to load library:" << basename;
            return false;
        }
    }

    return true;
}

bool QSocModuleManager::remove(const QString &libraryName)
{
    /* Validate projectManager and its module path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return false;
    }

    /* Check library basename */
    if (libraryName.isEmpty()) {
        qCritical() << "Error: library basename is empty.";
        return false;
    }

    /* Get the full file path */
    const QString filePath
        = QDir(projectManager->getModulePath()).filePath(libraryName + ".soc_sym");

    /* Check if library file exists */
    if (!QFile::exists(filePath)) {
        qCritical() << "Error: library file does not exist for basename:" << libraryName;
        return false;
    }

    /* Remove the file */
    if (!QFile::remove(filePath)) {
        qCritical() << "Error: Failed to remove module file:" << filePath;
        return false;
    }

    /* Remove from moduleData and libraryMap */
    moduleData.remove(libraryName.toStdString());
    libraryMap.remove(libraryName);

    return true;
}

bool QSocModuleManager::remove(const QRegularExpression &libraryNameRegex)
{
    /* Validate projectManager and its path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return false;
    }

    /* Get the list of library basenames matching the regex */
    const QStringList matchingBasenames = listLibrary(libraryNameRegex);

    /* Iterate through the list and remove each library */
    for (const QString &basename : matchingBasenames) {
        if (!remove(basename)) {
            qCritical() << "Error: Failed to remove library:" << basename;
            return false;
        }
    }

    return true;
}

bool QSocModuleManager::remove(const QStringList &libraryNameList)
{
    if (!projectManager || !projectManager->isValid()) {
        qCritical() << "Error: Invalid or null projectManager.";
        return false;
    }

    const QSet<QString> uniqueBasenames(libraryNameList.begin(), libraryNameList.end());

    for (const QString &basename : uniqueBasenames) {
        if (!remove(basename)) {
            qCritical() << "Error: Failed to remove library:" << basename;
            return false;
        }
    }

    return true;
}

bool QSocModuleManager::save(const QString &libraryName)
{
    /* Validate projectManager and its path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid library path.";
        return false;
    }

    /* Check if the libraryName exists in libraryMap */
    if (!libraryMap.contains(libraryName)) {
        qCritical() << "Error: Library basename not found in libraryMap.";
        return false;
    }

    /* Extract modules from moduleData */
    YAML::Node dataToSave;
    auto       range = libraryMap.equal_range(libraryName);
    for (auto it = range.first; it != range.second; ++it) {
        dataToSave[it.value().toStdString()] = moduleData[it.value().toStdString()];
    }

    /* Serialize and save to file */
    const QString filePath
        = QDir(projectManager->getModulePath()).filePath(libraryName + ".soc_sym");
    std::ofstream outFile(filePath.toStdString());
    if (!outFile.is_open()) {
        qCritical() << "Error: Unable to open file for writing:" << filePath;
        return false;
    }

    outFile << dataToSave;
    return true;
}

bool QSocModuleManager::save(const QRegularExpression &libraryNameRegex)
{
    bool allSaved = true;

    /* Validate projectManager and its path */
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return false;
    }

    /* Iterate over libraryMap and save matching libraries */
    for (const QString &libraryName : libraryMap.keys()) {
        if (libraryNameRegex.match(libraryName).hasMatch()) {
            if (!save(libraryName)) {
                qCritical() << "Error: Failed to save library:" << libraryName;
                allSaved = false;
            }
        }
    }

    return allSaved;
}

bool QSocModuleManager::save(const QStringList &libraryNameList)
{
    if (!projectManager || !projectManager->isValid()) {
        qCritical() << "Error: Invalid or null projectManager.";
        return false;
    }

    const QSet<QString> uniqueBasenames(libraryNameList.begin(), libraryNameList.end());

    for (const QString &basename : uniqueBasenames) {
        if (!save(basename)) {
            qCritical() << "Error: Failed to save library:" << basename;
            return false;
        }
    }

    return true;
}

QStringList QSocModuleManager::listModule(const QRegularExpression &moduleNameRegex)
{
    QStringList result;

    /* Iterate through each node in moduleData */
    for (YAML::const_iterator it = moduleData.begin(); it != moduleData.end(); ++it) {
        const QString moduleName = QString::fromStdString(it->first.as<std::string>());

        /* Check if the module name matches the regex */
        if (moduleNameRegex.match(moduleName).hasMatch()) {
            result.append(moduleName);
        }
    }

    return result;
}

bool QSocModuleManager::removeModule(const QRegularExpression &moduleNameRegex)
{
    if (!isModulePathValid()) {
        qCritical() << "Error: projectManager is null or invalid module path.";
        return false;
    }

    QSet<QString> libraryToSave;
    QSet<QString> libraryToRemove;
    QSet<QString> moduleToRemove;

    for (YAML::const_iterator it = moduleData.begin(); it != moduleData.end(); ++it) {
        const QString moduleName = QString::fromStdString(it->first.as<std::string>());
        if (moduleNameRegex.match(moduleName).hasMatch()) {
            moduleToRemove.insert(moduleName);
            for (auto moduleIt = libraryMap.begin(); moduleIt != libraryMap.end();) {
                if (moduleIt.value() == moduleName) {
                    const QString libraryName = moduleIt.key();
                    libraryToRemove.insert(libraryName);
                    moduleIt = libraryMap.erase(moduleIt);
                    libraryToSave.insert(libraryName);
                } else {
                    ++moduleIt;
                }
            }
        }
    }

    /* Remove modules from moduleData */
    for (const QString &moduleName : moduleToRemove) {
        moduleData.remove(moduleName.toStdString());
    }

    /* Ensure libraryToSave does not include modules marked for removal */
    for (const QString &moduleToRemove : libraryToRemove) {
        libraryToSave.remove(moduleToRemove);
    }

    const QStringList libraryToSaveList = QList<QString>(libraryToSave.begin(), libraryToSave.end());
    const QStringList libraryToRemoveList
        = QList<QString>(libraryToRemove.begin(), libraryToRemove.end());

    /* Save libraries that still have associations in libraryMap */
    if (!save(libraryToSaveList)) {
        qCritical() << "Error: Failed to save libraries.";
        return false;
    }

    /* Remove modules with no remaining associations in libraryMap */
    if (!remove(libraryToRemoveList)) {
        qCritical() << "Error: Failed to remove modules.";
        return false;
    }

    return true;
}