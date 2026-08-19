#include <QEvent>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QtTest>

#include "temporarycamerainput.h"

class tst_TemporaryCameraInput : public QObject
{
    Q_OBJECT

private slots:
    void spaceUsesPanUntilRelease();
    void controlOrMetaWithSpaceUsesZoom_data();
    void controlOrMetaWithSpaceUsesZoom();
    void modifierChangesModeWhileSpaceRemainsHeld();
    void disabledInputDoesNotCaptureCameraKeys();
    void deactivationAndDisableClearHeldKeys();
};

void tst_TemporaryCameraInput::spaceUsesPanUntilRelease()
{
    TemporaryCameraInput input;
    QObject target;
    QSignalSpy modeChanged(&input, &TemporaryCameraInput::modeChanged);
    input.setEnabled(true);

    QKeyEvent press(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
    QVERIFY(input.eventFilter(&target, &press));
    QCOMPARE(input.mode(), QStringLiteral("pan"));
    QCOMPARE(modeChanged.count(), 1);

    QKeyEvent repeatedPress(
        QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "), true, 2);
    QVERIFY(input.eventFilter(&target, &repeatedPress));
    QCOMPARE(input.mode(), QStringLiteral("pan"));
    QCOMPARE(modeChanged.count(), 1);

    QKeyEvent repeatedRelease(
        QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "), true, 2);
    QVERIFY(input.eventFilter(&target, &repeatedRelease));
    QCOMPARE(input.mode(), QStringLiteral("pan"));
    QCOMPARE(modeChanged.count(), 1);

    QKeyEvent release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
    QVERIFY(input.eventFilter(&target, &release));
    QVERIFY(input.mode().isEmpty());
    QCOMPARE(modeChanged.count(), 2);
}

void tst_TemporaryCameraInput::controlOrMetaWithSpaceUsesZoom_data()
{
    QTest::addColumn<int>("modifierKey");
    QTest::addColumn<Qt::KeyboardModifiers>("modifier");

    QTest::newRow("control") << static_cast<int>(Qt::Key_Control)
                              << Qt::KeyboardModifiers(Qt::ControlModifier);
    QTest::newRow("command-meta") << static_cast<int>(Qt::Key_Meta)
                                  << Qt::KeyboardModifiers(Qt::MetaModifier);
}

void tst_TemporaryCameraInput::controlOrMetaWithSpaceUsesZoom()
{
    QFETCH(int, modifierKey);
    QFETCH(Qt::KeyboardModifiers, modifier);

    TemporaryCameraInput input;
    QObject target;
    input.setEnabled(true);

    QKeyEvent modifierPress(QEvent::KeyPress, modifierKey, modifier);
    QVERIFY(!input.eventFilter(&target, &modifierPress));
    QVERIFY(input.mode().isEmpty());

    QKeyEvent spacePress(QEvent::KeyPress, Qt::Key_Space, modifier, QStringLiteral(" "));
    QVERIFY(input.eventFilter(&target, &spacePress));
    QCOMPARE(input.mode(), QStringLiteral("zoom"));

    QKeyEvent spaceRelease(QEvent::KeyRelease, Qt::Key_Space, modifier, QStringLiteral(" "));
    QVERIFY(input.eventFilter(&target, &spaceRelease));
    QVERIFY(input.mode().isEmpty());

    QKeyEvent modifierRelease(QEvent::KeyRelease, modifierKey, Qt::NoModifier);
    QVERIFY(!input.eventFilter(&target, &modifierRelease));
}

void tst_TemporaryCameraInput::modifierChangesModeWhileSpaceRemainsHeld()
{
    TemporaryCameraInput input;
    QObject target;
    QSignalSpy modeChanged(&input, &TemporaryCameraInput::modeChanged);
    input.setEnabled(true);

    QKeyEvent spacePress(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
    QVERIFY(input.eventFilter(&target, &spacePress));
    QCOMPARE(input.mode(), QStringLiteral("pan"));

    QKeyEvent controlPress(QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier);
    QVERIFY(input.eventFilter(&target, &controlPress));
    QCOMPARE(input.mode(), QStringLiteral("zoom"));

    QKeyEvent metaPress(QEvent::KeyPress,
                        Qt::Key_Meta,
                        Qt::ControlModifier | Qt::MetaModifier);
    QVERIFY(input.eventFilter(&target, &metaPress));
    QCOMPARE(input.mode(), QStringLiteral("zoom"));

    QKeyEvent controlRelease(QEvent::KeyRelease, Qt::Key_Control, Qt::MetaModifier);
    QVERIFY(input.eventFilter(&target, &controlRelease));
    QCOMPARE(input.mode(), QStringLiteral("zoom"));

    QKeyEvent metaRelease(QEvent::KeyRelease, Qt::Key_Meta, Qt::NoModifier);
    QVERIFY(input.eventFilter(&target, &metaRelease));
    QCOMPARE(input.mode(), QStringLiteral("pan"));

    QKeyEvent spaceRelease(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
    QVERIFY(input.eventFilter(&target, &spaceRelease));
    QVERIFY(input.mode().isEmpty());
    QCOMPARE(modeChanged.count(), 4);
}

void tst_TemporaryCameraInput::disabledInputDoesNotCaptureCameraKeys()
{
    TemporaryCameraInput input;
    QObject target;

    QKeyEvent disabledSpace(
        QEvent::KeyPress, Qt::Key_Space, Qt::ControlModifier, QStringLiteral(" "));
    QVERIFY(!input.eventFilter(&target, &disabledSpace));
    QVERIFY(input.mode().isEmpty());

    QKeyEvent disabledControl(QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier);
    QVERIFY(!input.eventFilter(&target, &disabledControl));
    QVERIFY(input.mode().isEmpty());

    input.setEnabled(true);
    QKeyEvent unrelatedPress(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    QVERIFY(!input.eventFilter(&target, &unrelatedPress));
    QVERIFY(input.mode().isEmpty());
}

void tst_TemporaryCameraInput::deactivationAndDisableClearHeldKeys()
{
    TemporaryCameraInput input;
    QObject target;
    input.setEnabled(true);

    QKeyEvent firstPress(
        QEvent::KeyPress, Qt::Key_Space, Qt::ControlModifier, QStringLiteral(" "));
    QVERIFY(input.eventFilter(&target, &firstPress));
    QCOMPARE(input.mode(), QStringLiteral("zoom"));

    QEvent deactivate(QEvent::ApplicationDeactivate);
    QVERIFY(!input.eventFilter(&target, &deactivate));
    QVERIFY(input.mode().isEmpty());

    QKeyEvent secondPress(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
    QVERIFY(input.eventFilter(&target, &secondPress));
    QCOMPARE(input.mode(), QStringLiteral("pan"));

    input.setEnabled(false);
    QVERIFY(input.mode().isEmpty());
}

QTEST_APPLESS_MAIN(tst_TemporaryCameraInput)

#include "tst_temporarycamerainput.moc"
