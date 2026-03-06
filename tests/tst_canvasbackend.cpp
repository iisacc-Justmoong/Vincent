#include <QtTest>
#include <QSignalSpy>

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
    void calculatesDocumentFitAndResetPlacement();
    void resolvesTransformGeometry();
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
        {QStringLiteral("images"),
         QVariantList{
             QVariantMap{
                 {QStringLiteral("imageId"), id},
                 {QStringLiteral("source"), QStringLiteral("file:///tmp/%1.png").arg(id)},
                 {QStringLiteral("importMetadata"),
                  QVariantMap{
                      {QStringLiteral("kind"), QStringLiteral("fixture")},
                      {QStringLiteral("sequence"), id}
                  }}
             }
         }},
        {QStringLiteral("selectedImageId"), id}
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
    QVariantList images{
        QVariantMap{
            {QStringLiteral("imageId"), 7},
            {QStringLiteral("importMetadata"),
             QVariantMap{
                 {QStringLiteral("kind"), QStringLiteral("psd")},
                 {QStringLiteral("visible"), true}
             }}
        }
    };

    const QVariantMap snapshot = backend.captureSnapshot(320, 240, strokes, images, 7);

    QVariantMap stroke = strokes[0].toMap();
    QVariantList points = stroke.value(QStringLiteral("points")).toList();
    QVariantMap point = points[0].toMap();
    point.insert(QStringLiteral("x"), 99);
    points[0] = point;
    stroke.insert(QStringLiteral("points"), points);
    strokes[0] = stroke;

    QVariantMap image = images[0].toMap();
    QVariantMap metadata = image.value(QStringLiteral("importMetadata")).toMap();
    metadata.insert(QStringLiteral("kind"), QStringLiteral("changed"));
    image.insert(QStringLiteral("importMetadata"), metadata);
    images[0] = image;

    const QVariantMap snapshotStroke = snapshot.value(QStringLiteral("strokes")).toList()[0].toMap();
    const QVariantMap snapshotPoint = snapshotStroke.value(QStringLiteral("points")).toList()[0].toMap();
    const QVariantMap snapshotImage = snapshot.value(QStringLiteral("images")).toList()[0].toMap();
    const QVariantMap snapshotMetadata = snapshotImage.value(QStringLiteral("importMetadata")).toMap();

    QCOMPARE(snapshotPoint.value(QStringLiteral("x")).toInt(), 12);
    QCOMPARE(snapshotMetadata.value(QStringLiteral("kind")).toString(), QStringLiteral("psd"));
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
    QCOMPARE(undone.value(QStringLiteral("selectedImageId")).toInt(), 1);
    QVERIFY(!backend.canUndo());
    QVERIFY(backend.canRedo());
    QCOMPARE(canUndoSpy.count(), 2);
    QCOMPARE(canRedoSpy.count(), 1);

    const QVariantMap redone = backend.redo(firstSnapshot, 8);
    QCOMPARE(redone.value(QStringLiteral("selectedImageId")).toInt(), 2);
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
    QCOMPARE(restored.value(QStringLiteral("selectedImageId")).toInt(), 3);

    restored = backend.undo(restored, 2);
    QCOMPARE(restored.value(QStringLiteral("selectedImageId")).toInt(), 2);

    QVERIFY(backend.undo(restored, 2).isEmpty());
    QVERIFY(!backend.canUndo());
}

void tst_CanvasBackend::calculatesDocumentFitAndResetPlacement()
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

    const QVariantMap placement = backend.resetImagePlacement(100, 100, 200, 50);
    compareReal(placement.value(QStringLiteral("scaleX")).toDouble(), 0.5);
    compareReal(placement.value(QStringLiteral("scaleY")).toDouble(), 0.5);
    compareReal(placement.value(QStringLiteral("x")).toDouble(), 0.0);
    compareReal(placement.value(QStringLiteral("y")).toDouble(), 37.5);
    QVERIFY(placement.value(QStringLiteral("ready")).toBool());

    QVERIFY(backend.resetImagePlacement(100, 100, 0, 50).isEmpty());
}

void tst_CanvasBackend::resolvesTransformGeometry()
{
    CanvasBackend backend;

    const QVariantMap baseRect{
        {QStringLiteral("x"), 10.0},
        {QStringLiteral("y"), 20.0},
        {QStringLiteral("w"), 100.0},
        {QStringLiteral("h"), 80.0}
    };
    const QVariantMap widened = backend.resolveTransform(
        QStringLiteral("right"), 40, 0, baseRect, 24, 300, 200, false);
    compareReal(widened.value(QStringLiteral("x")).toDouble(), 10.0);
    compareReal(widened.value(QStringLiteral("y")).toDouble(), 20.0);
    compareReal(widened.value(QStringLiteral("width")).toDouble(), 140.0);
    compareReal(widened.value(QStringLiteral("height")).toDouble(), 80.0);

    const QVariantMap clamped = backend.resolveTransform(
        QStringLiteral("topLeft"), 90, 80, baseRect, 24, 300, 200, false);
    compareReal(clamped.value(QStringLiteral("x")).toDouble(), 86.0);
    compareReal(clamped.value(QStringLiteral("y")).toDouble(), 76.0);
    compareReal(clamped.value(QStringLiteral("width")).toDouble(), 24.0);
    compareReal(clamped.value(QStringLiteral("height")).toDouble(), 24.0);

    const QVariantMap constrainedRect{
        {QStringLiteral("x"), 10.0},
        {QStringLiteral("y"), 20.0},
        {QStringLiteral("w"), 100.0},
        {QStringLiteral("h"), 50.0}
    };
    const QVariantMap constrained = backend.resolveTransform(
        QStringLiteral("right"), 50, 0, constrainedRect, 24, 300, 200, true);
    compareReal(constrained.value(QStringLiteral("x")).toDouble(), 10.0);
    compareReal(constrained.value(QStringLiteral("y")).toDouble(), 7.5);
    compareReal(constrained.value(QStringLiteral("width")).toDouble(), 150.0);
    compareReal(constrained.value(QStringLiteral("height")).toDouble(), 75.0);
}

QTEST_APPLESS_MAIN(tst_CanvasBackend)

#include "tst_canvasbackend.moc"
