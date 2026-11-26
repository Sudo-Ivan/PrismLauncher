#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <algorithm>

#include <FileSystem.h>
#include <Json.h>
#include <meta/Index.h>
#include <meta/JsonFormat.h>
#include <meta/Version.h>
#include <meta/VersionList.h>

class MVATest : public QObject {
    Q_OBJECT

   private slots:
    void test_rootIndexShape()
    {
        const auto doc = QJsonDocument::fromJson(R"({
            "formatVersion": 1,
            "packages": [
                { "name": "Minecraft", "sha256": "abc", "uid": "net.minecraft" },
                { "name": "Java Runtimes", "sha256": "def", "uid": "net.minecraft.java" }
            ]
        })");

        Meta::Index index;
        const auto result = Meta::parseIndex(doc.object(), &index);
        if (!result) {
            QFAIL(qPrintable(result.error()));
        }
        QVERIFY(index.hasUid("net.minecraft"));
        QVERIFY(index.hasUid("net.minecraft.java"));
    }

    void test_minecraftVersionListShape()
    {
        const auto doc = QJsonDocument::fromJson(R"({
            "formatVersion": 1,
            "uid": "net.minecraft",
            "name": "Minecraft",
            "versions": [
                {
                    "version": "1.21.1",
                    "type": "release",
                    "releaseTime": "2024-08-08T12:24:45+00:00",
                    "recommended": false,
                    "sha256": "abc",
                    "requires": [{ "uid": "org.lwjgl3", "suggests": "3.3.3" }]
                }
            ]
        })");

        Meta::VersionList list("net.minecraft");
        const auto result = Meta::parseVersionList(doc.object(), &list);
        if (!result) {
            QFAIL(qPrintable(result.error()));
        }
        QCOMPARE(list.versions().size(), 1);
        QCOMPARE(list.versions().first()->version(), QString("1.21.1"));
        const auto& required = list.versions().first()->requiredSet();
        QVERIFY(std::any_of(required.begin(), required.end(), [](const Meta::Require& req) {
            return req.uid == "org.lwjgl3" && req.suggests == "3.3.3";
        }));
    }

    void test_minecraftVersionDetailShape()
    {
        const auto source = QFINDTESTDATA("testdata/MVA/net.minecraft-1.21.1.json");
        QVERIFY(!source.isEmpty());
        const auto doc = Json::requireDocument(source);
        if (!doc) {
            QFAIL(qPrintable(doc.error()));
        }

        Meta::Version version("net.minecraft", "1.21.1");
        const auto result = Meta::parseVersion(doc->object(), &version);
        if (!result) {
            QFAIL(qPrintable(result.error()));
        }

        const auto data = version.data();
        QVERIFY(data);
        QCOMPARE(data->compatibleJavaMajors, QList<int>{ 21 });
        QCOMPARE(data->compatibleJavaName, QString("java-runtime-delta"));
        QVERIFY(data->mainJar);
        QCOMPARE(data->mainJar->rawName().serialize(), QString("com.mojang:minecraft:1.21.1:client"));
        QVERIFY(!data->libraries.isEmpty());
    }
    void test_javaRuntimeDetailShape()
    {
        const auto source = QFINDTESTDATA("testdata/MVA/net.minecraft.java-java21.json");
        QVERIFY(!source.isEmpty());
        const auto doc = Json::requireDocument(source);
        if (!doc) {
            QFAIL(qPrintable(doc.error()));
        }

        Meta::Version version("net.minecraft.java", "java21");
        const auto result = Meta::parseVersion(doc->object(), &version);
        if (!result) {
            QFAIL(qPrintable(result.error()));
        }

        const auto data = version.data();
        QVERIFY(data);
        QVERIFY(!data->runtimes.isEmpty());
        const auto linuxRuntime = std::find_if(data->runtimes.begin(), data->runtimes.end(), [](const Java::MetadataPtr& runtime) {
            return runtime->runtimeOS == "linux-x64";
        });
        QVERIFY(linuxRuntime != data->runtimes.end());
        QCOMPARE((*linuxRuntime)->m_name, QString("java-runtime-delta"));
        QCOMPARE((*linuxRuntime)->downloadType, Java::DownloadType::Archive);
        QCOMPARE((*linuxRuntime)->packageType, QString("jdk"));
        QCOMPARE((*linuxRuntime)->version.major(), 21);
        QVERIFY((*linuxRuntime)->url.startsWith("https://mva.quad4.io/java/temurin/21/"));
        QCOMPARE((*linuxRuntime)->checksumType, QString("sha256"));
        QVERIFY(!(*linuxRuntime)->checksumHash.isEmpty());
    }

    void test_fabricLoaderDetailShape()
    {
        const auto source = QFINDTESTDATA("testdata/MVA/net.fabricmc.fabric-loader-0.18.4.json");
        QVERIFY(!source.isEmpty());
        const auto doc = Json::requireDocument(source);
        if (!doc) {
            QFAIL(qPrintable(doc.error()));
        }

        Meta::Version version("net.fabricmc.fabric-loader", "0.18.4");
        const auto result = Meta::parseVersion(doc->object(), &version);
        if (!result) {
            QFAIL(qPrintable(result.error()));
        }

        const auto data = version.data();
        QVERIFY(data);
        QCOMPARE(data->mainClass, QString("net.fabricmc.loader.impl.launch.knot.KnotClient"));
        QVERIFY(!data->libraries.isEmpty());
        const auto names = data->libraries;
        QVERIFY(std::any_of(names.begin(), names.end(), [](const LibraryPtr& lib) {
            return lib->rawName().serialize() == "net.fabricmc:fabric-loader:0.18.4";
        }));
        QVERIFY(std::any_of(names.begin(), names.end(), [](const LibraryPtr& lib) {
            return lib->rawName().serialize() == "net.fabricmc:intermediary:1.21.1";
        }));
        QVERIFY(std::any_of(data->m_requires.begin(), data->m_requires.end(), [](const Meta::Require& req) {
            return req.uid == "net.fabricmc.intermediary";
        }));
    }

    void test_intermediaryDetailShape()
    {
        const auto source = QFINDTESTDATA("testdata/MVA/net.fabricmc.intermediary-1.21.1.json");
        QVERIFY(!source.isEmpty());
        const auto doc = Json::requireDocument(source);
        if (!doc) {
            QFAIL(qPrintable(doc.error()));
        }

        Meta::Version version("net.fabricmc.intermediary", "1.21.1");
        const auto result = Meta::parseVersion(doc->object(), &version);
        if (!result) {
            QFAIL(qPrintable(result.error()));
        }

        const auto data = version.data();
        QVERIFY(data);
        QCOMPARE(data->libraries.size(), 1);
        QCOMPARE(data->libraries.first()->rawName().serialize(), QString("net.fabricmc:intermediary:1.21.1"));
        QVERIFY(std::any_of(data->m_requires.begin(), data->m_requires.end(), [](const Meta::Require& req) {
            return req.uid == "net.minecraft" && req.equalsVersion == "1.21.1";
        }));
    }

    void test_lwjglDetailShape()
    {
        const auto source = QFINDTESTDATA("testdata/MVA/org.lwjgl3-3.4.3.json");
        QVERIFY(!source.isEmpty());
        const auto doc = Json::requireDocument(source);
        if (!doc) {
            QFAIL(qPrintable(doc.error()));
        }

        Meta::Version version("org.lwjgl3", "3.4.3");
        const auto result = Meta::parseVersion(doc->object(), &version);
        if (!result) {
            QFAIL(qPrintable(result.error()));
        }

        const auto data = version.data();
        QVERIFY(data);
        QCOMPARE(data->libraries.size(), 3);
        QVERIFY(std::any_of(data->libraries.begin(), data->libraries.end(), [](const LibraryPtr& lib) {
            return lib->rawName().serialize() == "org.lwjgl:lwjgl-freetype:3.4.3-mojang-2.15.0";
        }));
        QVERIFY(std::any_of(data->libraries.begin(), data->libraries.end(), [](const LibraryPtr& lib) {
            return lib->rawName().serialize() == "org.lwjgl:lwjgl-natives-linux:3.4.3";
        }));
    }

};

QTEST_GUILESS_MAIN(MVATest)
#include "MVA_test.moc"
