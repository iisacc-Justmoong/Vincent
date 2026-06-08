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
    void updatesEngineBrushSettingsAndClampsValues();
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
    QCOMPARE(viewModel.brushFlow(), 1.0);
    QCOMPARE(viewModel.brushOpacity(), 1.0);
    QCOMPARE(viewModel.brushHardness(), 1.0);
    QCOMPARE(viewModel.brushSpacing(), 0.0);
    QCOMPARE(viewModel.brushSpacingRatio(), 0.0);
    QCOMPARE(viewModel.pressureCurveMinimum(), 0.0);
    QCOMPARE(viewModel.pressureCurveCenter(), 0.5);
    QCOMPARE(viewModel.pressureCurveMaximum(), 1.0);
    QCOMPARE(viewModel.stabilizerStrength(), 0.0);
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

void tst_CanvasDocumentViewModel::updatesEngineBrushSettingsAndClampsValues()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QSignalSpy flowSpy(&viewModel, &CanvasDocumentViewModel::brushFlowChanged);
    QSignalSpy hardnessSpy(&viewModel, &CanvasDocumentViewModel::brushHardnessChanged);
    QSignalSpy pressureCenterSpy(&viewModel, &CanvasDocumentViewModel::pressureCurveCenterChanged);

    viewModel.setBrushFlow(0.35);
    QCOMPARE(viewModel.brushFlow(), 0.35);
    QCOMPARE(flowSpy.count(), 1);

    viewModel.setBrushFlow(-1.0);
    QCOMPARE(viewModel.brushFlow(), 0.0);
    QCOMPARE(flowSpy.count(), 2);

    viewModel.setBrushOpacity(2.0);
    QCOMPARE(viewModel.brushOpacity(), 1.0);

    viewModel.setBrushHardness(0.0);
    QCOMPARE(viewModel.brushHardness(), 0.01);
    QCOMPARE(hardnessSpy.count(), 1);

    viewModel.setBrushSpacing(-4.0);
    QCOMPARE(viewModel.brushSpacing(), 0.0);
    viewModel.setBrushSpacing(18.5);
    QCOMPARE(viewModel.brushSpacing(), 18.5);

    viewModel.setBrushSpacingRatio(1.4);
    QCOMPARE(viewModel.brushSpacingRatio(), 1.0);

    viewModel.setPressureCurveMinimum(0.7);
    QCOMPARE(viewModel.pressureCurveMinimum(), 0.7);
    QCOMPARE(viewModel.pressureCurveCenter(), 0.7);
    QCOMPARE(viewModel.pressureCurveMaximum(), 1.0);
    QCOMPARE(pressureCenterSpy.count(), 1);

    viewModel.setPressureCurveMaximum(0.4);
    QCOMPARE(viewModel.pressureCurveMinimum(), 0.4);
    QCOMPARE(viewModel.pressureCurveCenter(), 0.4);
    QCOMPARE(viewModel.pressureCurveMaximum(), 0.4);
    QCOMPARE(pressureCenterSpy.count(), 2);

    viewModel.setStabilizerStrength(1.5);
    QCOMPARE(viewModel.stabilizerStrength(), 1.0);
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
