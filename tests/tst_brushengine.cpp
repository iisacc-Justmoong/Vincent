#include <QtTest>

#include "brushengine.h"

class tst_BrushEngine : public QObject
{
    Q_OBJECT

private slots:
    void mouseInputKeepsFullSizeAndOpacity();
    void stylusPressureScalesSizeAndOpacity();
    void smoothingBlendsTowardCurrentValues();
    void appendDecisionAccountsForPressureOrOpacityChange();
    void stampCountTracksSegmentLength();
};

void tst_BrushEngine::mouseInputKeepsFullSizeAndOpacity()
{
    BrushEngine engine;

    QCOMPARE(engine.resolvedPressure(0.05, false), 1.0);
    QCOMPARE(engine.resolvedOpacity(0.05, false), 1.0);
    QCOMPARE(engine.sampleSize(12.0, 0.05, false), 12.0);
}

void tst_BrushEngine::stylusPressureScalesSizeAndOpacity()
{
    BrushEngine engine;

    const qreal softSize = engine.sampleSize(18.0, 0.08, true);
    const qreal hardSize = engine.sampleSize(18.0, 1.0, true);
    const qreal softOpacity = engine.resolvedOpacity(0.08, true);
    const qreal hardOpacity = engine.resolvedOpacity(1.0, true);

    QVERIFY(softSize >= engine.minimumSampleSize());
    QVERIFY(softSize < hardSize);
    QVERIFY(softOpacity > 0.0);
    QVERIFY(softOpacity < hardOpacity);
    QCOMPARE(hardSize, 18.0);
    QCOMPARE(hardOpacity, 1.0);
}

void tst_BrushEngine::smoothingBlendsTowardCurrentValues()
{
    BrushEngine engine;

    const qreal smoothedSize = engine.smoothedSampleSize(4.0, 10.0);
    const qreal smoothedOpacity = engine.smoothedSampleOpacity(0.25, 0.9);

    QVERIFY(smoothedSize > 4.0);
    QVERIFY(smoothedSize < 10.0);
    QVERIFY(qAbs(smoothedSize - 10.0) < qAbs(smoothedSize - 4.0));
    QVERIFY(smoothedOpacity > 0.25);
    QVERIFY(smoothedOpacity < 0.9);
}

void tst_BrushEngine::appendDecisionAccountsForPressureOrOpacityChange()
{
    BrushEngine engine;

    QVERIFY(!engine.shouldAppendPoint(10.0, 10.0, 4.0, 0.6, 10.04, 10.04, 4.02, 0.62, 12.0));
    QVERIFY(engine.shouldAppendPoint(10.0, 10.0, 4.0, 0.6, 10.04, 10.04, 6.0, 0.62, 12.0));
    QVERIFY(engine.shouldAppendPoint(10.0, 10.0, 4.0, 0.4, 10.04, 10.04, 4.02, 0.8, 12.0));
}

void tst_BrushEngine::stampCountTracksSegmentLength()
{
    BrushEngine engine;

    QCOMPARE(engine.stampCount(10.0, 10.0, 6.0, 10.0, 10.0, 6.0), 1);
    QVERIFY(engine.stampCount(10.0, 10.0, 6.0, 42.0, 18.0, 6.0) > 1);
}

QTEST_APPLESS_MAIN(tst_BrushEngine)

#include "tst_brushengine.moc"
