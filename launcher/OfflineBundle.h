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

#pragma once

#include <QFuture>
#include <QFutureWatcher>
#include <QStringList>

#include <utility>

#include "Result.h"
#include "tasks/Task.h"

class MinecraftInstance;

namespace OfflineBundle {

inline constexpr const char* DirectoryName = "prismlauncher-runtime";
inline constexpr const char* FilesDirectoryName = "files";
inline constexpr const char* ManifestFileName = "manifest.json";

struct FileEntry {
    QString source;
    QString relativePath;
};

struct Collection {
    QList<FileEntry> files;
    QStringList missing;
};

QString directoryPath(const QString& basePath);
bool existsIn(const QString& basePath);
bool isSafeRelativePath(const QString& relativePath);
Result<Collection> collectFiles(MinecraftInstance* instance);
QByteArray createManifest(const Collection& collection);

}  // namespace OfflineBundle

class OfflineBundleInstallTask : public Task {
    Q_OBJECT

   public:
    explicit OfflineBundleInstallTask(QString stagingPath) : m_stagingPath(std::move(stagingPath)) { setAbortable(true); }
    ~OfflineBundleInstallTask() override = default;

   protected:
    void executeTask() override;
    bool abort() override;

   private:
    Result<> install();
    void finish();

   private:
    QString m_stagingPath;
    QFuture<Result<>> m_future;
    QFutureWatcher<Result<>> m_watcher;
};
