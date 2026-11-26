// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "OfflineBundle.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSysInfo>
#include <QtConcurrent>

#include <algorithm>

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"
#include "Json.h"
#include "java/JavaUtils.h"
#include "SysInfo.h"
#include "minecraft/AssetsUtils.h"
#include "minecraft/Library.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"

namespace OfflineBundle {

QString directoryPath(const QString& basePath)
{
    return FS::PathCombine(basePath, DirectoryName);
}

bool existsIn(const QString& basePath)
{
    return QFileInfo::exists(FS::PathCombine(directoryPath(basePath), ManifestFileName));
}

bool isSafeRelativePath(const QString& relativePath)
{
    if (relativePath.isEmpty() || QDir::isAbsolutePath(relativePath) || relativePath.contains('\\')) {
        return false;
    }

    const auto parts = relativePath.split('/');
    if (parts.size() < 2) {
        return false;
    }

    static const QSet<QString> allowedRoots = { "libraries", "assets", "meta", "java" };
    if (!allowedRoots.contains(parts.first())) {
        return false;
    }

    return std::ranges::none_of(parts, [](const QString& part) { return part.isEmpty() || part == "." || part == ".."; });
}

namespace {

bool pathInside(const QString& path, const QString& root)
{
    const auto cleanPath = QDir::cleanPath(path);
    const auto cleanRoot = QDir::cleanPath(root);
#ifdef Q_OS_WIN
    return cleanPath.startsWith(cleanRoot + '/', Qt::CaseInsensitive);
#else
    return cleanPath.startsWith(cleanRoot + '/');
#endif
}

QString sourcePathFor(const QFileInfo& info)
{
    if (info.isSymLink()) {
        const auto canonical = info.canonicalFilePath();
        if (!canonical.isEmpty()) {
            return canonical;
        }
    }
    return info.absoluteFilePath();
}

void addDirectory(Collection& collection, QSet<QString>& seen, const QString& sourceRoot, const QString& relativeRoot)
{
    QDir root(sourceRoot);
    if (!root.exists()) {
        collection.missing.append(relativeRoot);
        return;
    }

    QDirIterator it(sourceRoot, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const auto info = it.fileInfo();
        const auto relativePath = relativeRoot + '/' + root.relativeFilePath(info.absoluteFilePath());
        if (seen.contains(relativePath)) {
            continue;
        }

        const auto source = sourcePathFor(info);
        if (!QFileInfo::exists(source)) {
            collection.missing.append(relativePath);
            continue;
        }
        seen.insert(relativePath);
        collection.files.append({ source, relativePath });
    }
}

void addFile(Collection& collection, QSet<QString>& seen, const QString& source, const QString& relativePath, bool required)
{
    if (seen.contains(relativePath)) {
        return;
    }

    QFileInfo info(source);
    const auto realSource = sourcePathFor(info);
    if (!QFileInfo(realSource).isFile()) {
        if (required) {
            collection.missing.append(relativePath);
        }
        return;
    }

    seen.insert(relativePath);
    collection.files.append({ realSource, relativePath });
}

void addLibraryFile(Collection& collection,
                    QSet<QString>& seen,
                    const QString& path,
                    const QDir& sharedLibrariesDir,
                    const QString& sharedLibrariesRoot)
{
    QFileInfo info(path);
    const auto absolutePath = info.absoluteFilePath();
    if (pathInside(absolutePath, sharedLibrariesRoot)) {
        const auto relativePath = "libraries/" + sharedLibrariesDir.relativeFilePath(absolutePath);
        addFile(collection, seen, absolutePath, relativePath, true);
    }
}

}  // namespace

Result<Collection> collectFiles(MinecraftInstance* instance)
{
    if (!instance) {
        return std::unexpected(QStringLiteral("No instance was provided."));
    }

    auto* packProfile = instance->getPackProfile();
    if (!packProfile) {
        return std::unexpected(QStringLiteral("The instance does not have a component profile."));
    }

    Collection collection;
    QSet<QString> seen;

    const auto profile = packProfile->getProfile();
    QStringList jars;
    QStringList nativeJars;
    profile->getLibraryFiles(instance->runtimeContext(), jars, nativeJars, instance->getLocalLibraryPath(), instance->binRoot(), false);

    for (const auto& mavenFile : profile->getMavenFiles()) {
        QStringList jar;
        QStringList natives;
        QStringList natives32;
        QStringList natives64;
        mavenFile->getApplicableFiles(instance->runtimeContext(), jar, natives, natives32, natives64, instance->getLocalLibraryPath());
        jars.append(jar);
        nativeJars.append(natives);
        nativeJars.append(instance->runtimeContext().javaArchitecture == "32" ? natives32 : natives64);
    }

    for (const auto& agent : profile->getAgents()) {
        QStringList jar;
        QStringList natives;
        QStringList natives32;
        QStringList natives64;
        agent.library->getApplicableFiles(instance->runtimeContext(), jar, natives, natives32, natives64, instance->getLocalLibraryPath());
        jars.append(jar);
        nativeJars.append(natives);
        nativeJars.append(instance->runtimeContext().javaArchitecture == "32" ? natives32 : natives64);
    }

    QDir sharedLibrariesDir(instance->librariesPath());
    const auto sharedLibrariesRoot = sharedLibrariesDir.absolutePath();
    for (const auto& path : jars + nativeJars) {
        addLibraryFile(collection, seen, path, sharedLibrariesDir, sharedLibrariesRoot);
    }

    const auto assets = profile->getMinecraftAssets();
    const auto indexRelativePath = "assets/indexes/" + assets->id + ".json";
    const auto indexAbsolutePath = QDir::current().absoluteFilePath(indexRelativePath);
    addFile(collection, seen, indexAbsolutePath, indexRelativePath, true);

    AssetsIndex index;
    if (AssetsUtils::loadAssetsIndexJson(assets->id, indexRelativePath, index)) {
        for (const auto& object : index.objects) {
            const auto relativePath = object.getLocalPath();
            addFile(collection, seen, QDir::current().absoluteFilePath(relativePath), relativePath, true);
        }
    } else {
        collection.missing.append(indexRelativePath);
    }

    addDirectory(collection, seen, QDir::current().absoluteFilePath("meta"), "meta");

    QString javaRelativeRoot;
    const auto compatibleJavaName = profile->getCompatibleJavaName();
    if (!compatibleJavaName.isEmpty()) {
        javaRelativeRoot = "java/" + compatibleJavaName;
    } else {
        const auto configuredJava = QFileInfo(instance->settings()->get("JavaPath").toString()).canonicalFilePath();
        const auto javaRoot = QDir(APPLICATION->javaPath()).absolutePath();
        if (!configuredJava.isEmpty() && pathInside(configuredJava, javaRoot)) {
            const auto relative = QDir(javaRoot).relativeFilePath(configuredJava);
            javaRelativeRoot = "java/" + relative.section('/', 0, 0);
        }
    }

    if (!javaRelativeRoot.isEmpty()) {
        const auto javaAbsoluteRoot = QDir::current().absoluteFilePath(javaRelativeRoot);
        const auto executable = FS::PathCombine(javaAbsoluteRoot, "bin", JavaUtils::javaExecutable);
        if (QFileInfo::exists(executable)) {
            addDirectory(collection, seen, javaAbsoluteRoot, javaRelativeRoot);
        } else {
            collection.missing.append(javaRelativeRoot);
        }
    }

    return collection;
}

QByteArray createManifest(const Collection& collection)
{
    QStringList files;
    for (const auto& file : collection.files) {
        files.append(file.relativePath);
    }
    files.sort();
    files.removeDuplicates();

    QJsonObject root;
    root.insert("formatVersion", 1);
    root.insert("launcher", BuildConfig.LAUNCHER_DISPLAYNAME);
    root.insert("platform", SysInfo::currentSystem());
    root.insert("architecture", QSysInfo::currentCpuArchitecture());
    root.insert("files", QJsonArray::fromStringList(files));

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

}  // namespace OfflineBundle

void OfflineBundleInstallTask::executeTask()
{
    m_future = QtConcurrent::run(QThreadPool::globalInstance(), [this] { return install(); });
    connect(&m_watcher, &QFutureWatcher<Result<>>::finished, this, &OfflineBundleInstallTask::finish);
    m_watcher.setFuture(m_future);
}

Result<> OfflineBundleInstallTask::install()
{
    const auto bundleRoot = OfflineBundle::directoryPath(m_stagingPath);
    const auto manifestPath = FS::PathCombine(bundleRoot, OfflineBundle::ManifestFileName);

    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return std::unexpected(tr("Could not open the offline runtime manifest."));
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::unexpected(tr("The offline runtime manifest is not valid JSON."));
    }

    const auto root = document.object();
    if (root.value("formatVersion").toInt() != 1) {
        return std::unexpected(tr("The offline runtime format is not supported."));
    }

    const auto files = root.value("files").toArray();
    const auto filesRoot = QDir(FS::PathCombine(bundleRoot, OfflineBundle::FilesDirectoryName));
    const auto dataRoot = QDir::current();

    setProgress(0, files.size());
    int copied = 0;
    for (const auto& value : files) {
        if (m_future.isCanceled()) {
            return std::unexpected(QStringLiteral("Cancelled"));
        }

        const auto relativePath = value.toString();
        if (!OfflineBundle::isSafeRelativePath(relativePath)) {
            return std::unexpected(tr("The offline runtime contains an unsafe path: %1").arg(relativePath));
        }

        const auto sourceInfo = QFileInfo(filesRoot.absoluteFilePath(relativePath));
        if (!sourceInfo.isFile()) {
            return std::unexpected(tr("The offline runtime is missing %1.").arg(relativePath));
        }

        const auto targetPath = dataRoot.absoluteFilePath(relativePath);
        if (!QFileInfo::exists(targetPath)) {
            if (!FS::ensureFilePathExists(targetPath)) {
                return std::unexpected(tr("Could not create the folder for %1.").arg(relativePath));
            }

            const auto sourcePath = sourceInfo.isSymLink() ? sourceInfo.canonicalFilePath() : sourceInfo.absoluteFilePath();
            if (sourcePath.isEmpty() || !QFile::copy(sourcePath, targetPath)) {
                return std::unexpected(tr("Could not copy %1 into the launcher data folder.").arg(relativePath));
            }
            QFile::setPermissions(targetPath, QFile::permissions(sourcePath));
        }

        copied++;
        setProgress(copied, files.size());
    }

    if (!FS::deletePath(bundleRoot)) {
        return std::unexpected(tr("Could not remove the offline runtime files from the imported instance."));
    }

    return {};
}

void OfflineBundleInstallTask::finish()
{
    if (m_future.isCanceled()) {
        emitAborted();
    } else if (auto result = m_future.result(); !result) {
        emitFailed(result.error());
    } else {
        emitSucceeded();
    }
}

bool OfflineBundleInstallTask::abort()
{
    if (m_future.isRunning()) {
        m_future.cancel();
        return true;
    }
    return false;
}
