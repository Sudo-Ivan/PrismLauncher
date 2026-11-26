#include <QTest>

#include <OfflineBundle.h>

class OfflineBundleTest : public QObject {
    Q_OBJECT

   private slots:
    void test_safeRelativePath_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("valid");

        QTest::newRow("library") << "libraries/com/example/test/1.0/test-1.0.jar" << true;
        QTest::newRow("asset") << "assets/objects/ab/abcdef" << true;
        QTest::newRow("empty") << "" << false;
        QTest::newRow("root file") << "libraries" << false;
        QTest::newRow("unknown root") << "instances/test/file" << false;
        QTest::newRow("dot dot") << "libraries/../meta/index.json" << false;
        QTest::newRow("empty segment") << "libraries//test.jar" << false;
        QTest::newRow("backslash") << "libraries\\..\\meta" << false;
        QTest::newRow("absolute") << "/libraries/test.jar" << false;
    }

    void test_safeRelativePath()
    {
        QFETCH(QString, path);
        QFETCH(bool, valid);
        QCOMPARE(OfflineBundle::isSafeRelativePath(path), valid);
    }
};

QTEST_GUILESS_MAIN(OfflineBundleTest)
#include "OfflineBundle_test.moc"
