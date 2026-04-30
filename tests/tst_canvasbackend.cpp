#include <QSignalSpy>
#include <QtTest>

#include "canvasbackend.h"

class tst_CanvasBackend : public QObject
{
    Q_OBJECT

private:
    static QVariantMap makeSnapshot(int id);
    static void compareReal(qreal actual, qreal expected);

private slots:
    void capturesIndependentSnapshots();
    void managesUndoRedoHistory();
    void trimsUndoHistoryToConfiguredLimit();
    void calculatesDocumentFit();
};

QVariantMap tst_CanvasBackend::makeSnapshot(int id)
{
    return {
        {QStringLiteral("canvasWidth"), 200 + id},
        {QStringLiteral("canvasHeight"), 100 + id},
        {QStringLiteral("strokes"),
         QVariantList{
             QVariantMap{
                 {QStringLiteral("size"), 2 + id},
                 {QStringLiteral("points"),
                  QVariantList{
                      QVariantMap{
                          {QStringLiteral("x"), id},
                          {QStringLiteral("y"), id * 2},
                          {QStringLiteral("opacity"), 0.5}
                      }
                  }}
             }
         }},
        {QStringLiteral("background"),
         QVariantMap{
             {QStringLiteral("source"), QStringLiteral("file:///tmp/background-%1.png").arg(id)},
             {QStringLiteral("x"), 10 + id},
             {QStringLiteral("y"), 20 + id},
             {QStringLiteral("width"), 30 + id},
             {QStringLiteral("height"), 40 + id}
         }}
    };
}

void tst_CanvasBackend::compareReal(qreal actual, qreal expected)
{
    QVERIFY(qAbs(actual - expected) < 0.0001);
}

void tst_CanvasBackend::capturesIndependentSnapshots()
{
    CanvasBackend backend;

    QVariantList strokes{
        QVariantMap{
            {QStringLiteral("size"), 8},
            {QStringLiteral("points"),
             QVariantList{
                 QVariantMap{
                     {QStringLiteral("x"), 12},
                     {QStringLiteral("y"), 24},
                     {QStringLiteral("opacity"), 0.8}
                 }
             }}
        }
    };
    QVariantMap background{
        {QStringLiteral("source"), QStringLiteral("file:///tmp/opened.png")},
        {QStringLiteral("x"), 14},
        {QStringLiteral("y"), 18},
        {QStringLiteral("width"), 120},
        {QStringLiteral("height"), 90}
    };

    const QVariantMap snapshot = backend.captureSnapshot(320, 240, strokes, background);

    QVariantMap stroke = strokes[0].toMap();
    QVariantList points = stroke.value(QStringLiteral("points")).toList();
    QVariantMap point = points[0].toMap();
    point.insert(QStringLiteral("x"), 99);
    points[0] = point;
    stroke.insert(QStringLiteral("points"), points);
    strokes[0] = stroke;

    background.insert(QStringLiteral("source"), QStringLiteral("file:///tmp/changed.png"));
    background.insert(QStringLiteral("width"), 10);

    const QVariantMap snapshotStroke = snapshot.value(QStringLiteral("strokes")).toList()[0].toMap();
    const QVariantMap snapshotPoint = snapshotStroke.value(QStringLiteral("points")).toList()[0].toMap();
    const QVariantMap snapshotBackground = snapshot.value(QStringLiteral("background")).toMap();

    QCOMPARE(snapshotPoint.value(QStringLiteral("x")).toInt(), 12);
    QCOMPARE(snapshotBackground.value(QStringLiteral("source")).toString(),
             QStringLiteral("file:///tmp/opened.png"));
    QCOMPARE(snapshotBackground.value(QStringLiteral("width")).toInt(), 120);
}

void tst_CanvasBackend::managesUndoRedoHistory()
{
    CanvasBackend backend;
    QSignalSpy canUndoSpy(&backend, &CanvasBackend::canUndoChanged);
    QSignalSpy canRedoSpy(&backend, &CanvasBackend::canRedoChanged);

    const QVariantMap firstSnapshot = makeSnapshot(1);
    const QVariantMap secondSnapshot = makeSnapshot(2);

    QVERIFY(!backend.canUndo());
    QVERIFY(!backend.canRedo());

    backend.pushUndoState(firstSnapshot, 8);
    QVERIFY(backend.canUndo());
    QVERIFY(!backend.canRedo());
    QCOMPARE(canUndoSpy.count(), 1);
    QCOMPARE(canRedoSpy.count(), 0);

    const QVariantMap undone = backend.undo(secondSnapshot, 8);
    QCOMPARE(undone.value(QStringLiteral("background")).toMap().value(QStringLiteral("source")).toString(),
             QStringLiteral("file:///tmp/background-1.png"));
    QVERIFY(!backend.canUndo());
    QVERIFY(backend.canRedo());
    QCOMPARE(canUndoSpy.count(), 2);
    QCOMPARE(canRedoSpy.count(), 1);

    const QVariantMap redone = backend.redo(secondSnapshot, 8);
    QCOMPARE(redone.value(QStringLiteral("background")).toMap().value(QStringLiteral("source")).toString(),
             QStringLiteral("file:///tmp/background-2.png"));
    QVERIFY(backend.canUndo());
    QVERIFY(!backend.canRedo());
    QCOMPARE(canUndoSpy.count(), 3);
    QCOMPARE(canRedoSpy.count(), 2);

    backend.clearHistory();
    QVERIFY(!backend.canUndo());
    QVERIFY(!backend.canRedo());
    QCOMPARE(canUndoSpy.count(), 4);
    QCOMPARE(canRedoSpy.count(), 2);
}

void tst_CanvasBackend::trimsUndoHistoryToConfiguredLimit()
{
    CanvasBackend backend;

    const QVariantMap firstSnapshot = makeSnapshot(1);
    const QVariantMap secondSnapshot = makeSnapshot(2);
    const QVariantMap thirdSnapshot = makeSnapshot(3);
    const QVariantMap fourthSnapshot = makeSnapshot(4);

    backend.pushUndoState(firstSnapshot, 2);
    backend.pushUndoState(secondSnapshot, 2);
    backend.pushUndoState(thirdSnapshot, 2);

    QVariantMap restored = backend.undo(fourthSnapshot, 2);
    QCOMPARE(restored.value(QStringLiteral("canvasWidth")).toInt(), 203);

    restored = backend.undo(restored, 2);
    QCOMPARE(restored.value(QStringLiteral("canvasWidth")).toInt(), 202);

    QVERIFY(backend.undo(restored, 2).isEmpty());
    QVERIFY(!backend.canUndo());
}

void tst_CanvasBackend::calculatesDocumentFit()
{
    CanvasBackend backend;

    const QVariantMap fitTransform = backend.documentFitTransform(100, 100, 200, 50);
    compareReal(fitTransform.value(QStringLiteral("scale")).toDouble(), 0.5);
    compareReal(fitTransform.value(QStringLiteral("offsetX")).toDouble(), 0.0);
    compareReal(fitTransform.value(QStringLiteral("offsetY")).toDouble(), 37.5);

    const QVariantMap noUpscaleTransform = backend.documentFitTransform(100, 100, 40, 20);
    compareReal(noUpscaleTransform.value(QStringLiteral("scale")).toDouble(), 1.0);
    compareReal(noUpscaleTransform.value(QStringLiteral("offsetX")).toDouble(), 30.0);
    compareReal(noUpscaleTransform.value(QStringLiteral("offsetY")).toDouble(), 40.0);
}

QTEST_APPLESS_MAIN(tst_CanvasBackend)

#include "tst_canvasbackend.moc"
