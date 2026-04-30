#include <QSignalSpy>
#include <QtTest>

#include "canvasdocumentviewmodel.h"
#include "paletteutils.h"

class tst_CanvasDocumentViewModel : public QObject
{
    Q_OBJECT

private slots:
    void exposesDefaultDocumentState();
    void updatesBrushStateAndClampsValues();
    void acceptsOnlySupportedTools();
    void keepsCanvasDimensionsPositive();
};

void tst_CanvasDocumentViewModel::exposesDefaultDocumentState()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVERIFY(!viewModel.palette().isEmpty());
    QCOMPARE(viewModel.brushColor(), QColor(QStringLiteral("#1a1a1a")));
    QCOMPARE(viewModel.brushSize(), 2.0);
    QCOMPARE(viewModel.toolMode(), QStringLiteral("brush"));
    QCOMPARE(viewModel.canvasWidth(), 1);
    QCOMPARE(viewModel.canvasHeight(), 1);
}

void tst_CanvasDocumentViewModel::updatesBrushStateAndClampsValues()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QSignalSpy colorSpy(&viewModel, &CanvasDocumentViewModel::brushColorChanged);
    QSignalSpy sizeSpy(&viewModel, &CanvasDocumentViewModel::brushSizeChanged);

    viewModel.setBrushColor(QColor(QStringLiteral("#ff7043")));
    QCOMPARE(viewModel.brushColor(), QColor(QStringLiteral("#ff7043")));
    QCOMPARE(colorSpy.count(), 1);

    viewModel.setBrushSize(0.2);
    QCOMPARE(viewModel.brushSize(), 1.0);

    viewModel.setBrushSize(80.0);
    QCOMPARE(viewModel.brushSize(), 48.0);
    QCOMPARE(sizeSpy.count(), 2);

    viewModel.setBrushSize(48.0);
    QCOMPARE(sizeSpy.count(), 2);
}

void tst_CanvasDocumentViewModel::acceptsOnlySupportedTools()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QSignalSpy toolSpy(&viewModel, &CanvasDocumentViewModel::toolModeChanged);

    viewModel.setToolMode(QStringLiteral("eraser"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("eraser"));

    viewModel.setToolMode(QStringLiteral("text"));
    QCOMPARE(viewModel.toolMode(), QStringLiteral("brush"));
    QCOMPARE(toolSpy.count(), 2);
}

void tst_CanvasDocumentViewModel::keepsCanvasDimensionsPositive()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    viewModel.setCanvasWidth(640);
    viewModel.setCanvasHeight(480);
    QCOMPARE(viewModel.canvasWidth(), 640);
    QCOMPARE(viewModel.canvasHeight(), 480);

    viewModel.setCanvasWidth(0);
    viewModel.setCanvasHeight(-4);
    QCOMPARE(viewModel.canvasWidth(), 1);
    QCOMPARE(viewModel.canvasHeight(), 1);
}

QTEST_APPLESS_MAIN(tst_CanvasDocumentViewModel)

#include "tst_canvasdocumentviewmodel.moc"
