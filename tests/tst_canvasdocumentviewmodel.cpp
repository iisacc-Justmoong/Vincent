#include <QtTest>
#include <QSignalSpy>

#include "canvasdocumentviewmodel.h"
#include "layerlistmodel.h"
#include "paletteutils.h"

class tst_CanvasDocumentViewModel : public QObject
{
    Q_OBJECT

private slots:
    void exposesDefaultDocumentState();
    void appendsAndSelectsLayers();
    void exportsAndImportsLayersWithMetadata();
    void keepsLayerCountSignalStableOnSameSizedImport();
    void exposesDerivedSelectionStateAndSignals();
    void movesLayersThroughViewModel();
    void movesAdjacentLayerForward();
    void removesSelectedLayerAndFallsBackToLast();
};

void tst_CanvasDocumentViewModel::exposesDefaultDocumentState()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QVERIFY(!viewModel.palette().isEmpty());
    QCOMPARE(viewModel.brushColor(), QColor(QStringLiteral("#1a1a1a")));
    QCOMPARE(viewModel.brushSize(), 2.0);
    QCOMPARE(viewModel.toolMode(), QStringLiteral("brush"));
    QCOMPARE(viewModel.selectedLayerId(), -1);
    QCOMPARE(viewModel.canvasWidth(), 1);
    QCOMPARE(viewModel.canvasHeight(), 1);
    QVERIFY(!viewModel.hasImportedLayer());
    QVERIFY(!viewModel.hasLayerSelection());
    QCOMPARE(viewModel.layerCount(), 0);
    QVERIFY(viewModel.selectedLayerData().isEmpty());

    auto *layerModel = qobject_cast<LayerListModel *>(viewModel.layerListModel());
    QVERIFY(layerModel);
    QCOMPARE(layerModel->count(), 0);
}

void tst_CanvasDocumentViewModel::appendsAndSelectsLayers()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    const int firstId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/one.png")},
        {QStringLiteral("layerName"), QStringLiteral("Layer One")},
        {QStringLiteral("ready"), true}
    });

    const int secondId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/two.png")},
        {QStringLiteral("layerName"), QStringLiteral("Layer Two")},
        {QStringLiteral("ready"), true}
    });

    QVERIFY(firstId > 0);
    QVERIFY(secondId > firstId);
    QVERIFY(viewModel.hasImportedLayer());

    viewModel.setSelectedLayerId(secondId);
    QVERIFY(viewModel.hasLayerSelection());
    QCOMPARE(viewModel.selectedLayerId(), secondId);
    QCOMPARE(viewModel.selectedLayerName(), QStringLiteral("Layer Two"));
    QCOMPARE(viewModel.layerCount(), 2);
    QCOMPARE(viewModel.selectedLayerData().value(QStringLiteral("imageId")).toInt(), secondId);

    QVERIFY(viewModel.updateLayerPropertyById(secondId, QStringLiteral("layerVisible"), false));
    QVERIFY(!viewModel.layerById(secondId).value(QStringLiteral("layerVisible")).toBool());
}

void tst_CanvasDocumentViewModel::exportsAndImportsLayersWithMetadata()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel sourceViewModel(&paletteUtils);
    const int layerId = sourceViewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/layer.png")},
        {QStringLiteral("layerName"), QStringLiteral("PSD Layer")},
        {QStringLiteral("ready"), true},
        {QStringLiteral("layerOpacity"), 0.5},
        {QStringLiteral("importMetadata"),
         QVariantMap{
             {QStringLiteral("kind"), QStringLiteral("psd-layer")},
             {QStringLiteral("psdLayer"),
              QVariantMap{
                  {QStringLiteral("name"), QStringLiteral("PSD Layer")},
                  {QStringLiteral("visible"), true}
              }}
         }}
    });
    sourceViewModel.setSelectedLayerId(layerId);

    const QVariantList snapshot = sourceViewModel.exportLayers();
    QCOMPARE(snapshot.size(), 1);

    CanvasDocumentViewModel restoredViewModel(&paletteUtils);
    restoredViewModel.importLayers(snapshot);
    restoredViewModel.setSelectedLayerId(layerId);

    QCOMPARE(restoredViewModel.findLayerIndexById(layerId), 0);
    const QVariantMap restored = restoredViewModel.selectedLayer();
    QCOMPARE(restored.value(QStringLiteral("layerName")).toString(), QStringLiteral("PSD Layer"));
    QCOMPARE(restored.value(QStringLiteral("layerOpacity")).toDouble(), 0.5);
    QCOMPARE(restored.value(QStringLiteral("importMetadata")).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("psd-layer"));
}

void tst_CanvasDocumentViewModel::keepsLayerCountSignalStableOnSameSizedImport()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QSignalSpy layerCountSpy(&viewModel, &CanvasDocumentViewModel::layerCountChanged);

    viewModel.appendLayer({
        {QStringLiteral("imageId"), 1},
        {QStringLiteral("source"), QStringLiteral("file:///tmp/one.png")},
        {QStringLiteral("layerName"), QStringLiteral("One")}
    });
    QCOMPARE(layerCountSpy.count(), 1);

    const QVariantList replacement = {
        QVariantMap{
            {QStringLiteral("imageId"), 9},
            {QStringLiteral("source"), QStringLiteral("file:///tmp/replaced.png")},
            {QStringLiteral("layerName"), QStringLiteral("Replaced")}
        }
    };

    layerCountSpy.clear();
    viewModel.importLayers(replacement);
    QCOMPARE(viewModel.layerCount(), 1);
    QCOMPARE(layerCountSpy.count(), 0);
}

void tst_CanvasDocumentViewModel::exposesDerivedSelectionStateAndSignals()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    QSignalSpy layerCountSpy(&viewModel, &CanvasDocumentViewModel::layerCountChanged);
    QSignalSpy selectionSpy(&viewModel, &CanvasDocumentViewModel::selectedLayerDataChanged);

    const int layerId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/accent.png")},
        {QStringLiteral("layerName"), QStringLiteral("Accent")},
        {QStringLiteral("layerOpacity"), 0.75},
        {QStringLiteral("ready"), true}
    });

    QCOMPARE(layerCountSpy.count(), 1);
    QVERIFY(selectionSpy.count() >= 1);

    viewModel.setSelectedLayerId(layerId);
    QCOMPARE(viewModel.selectedLayerName(), QStringLiteral("Accent"));
    QCOMPARE(viewModel.selectedLayerData().value(QStringLiteral("layerOpacity")).toDouble(), 0.75);

    QVERIFY(viewModel.updateLayerById(layerId, {
        {QStringLiteral("layerName"), QStringLiteral("Accent Updated")},
        {QStringLiteral("layerOpacity"), 0.5}
    }));
    QCOMPARE(viewModel.selectedLayerName(), QStringLiteral("Accent Updated"));
    QCOMPARE(viewModel.selectedLayerData().value(QStringLiteral("layerOpacity")).toDouble(), 0.5);
    QVERIFY(selectionSpy.count() >= 3);
}

void tst_CanvasDocumentViewModel::movesLayersThroughViewModel()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    const int firstId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/first.png")},
        {QStringLiteral("layerName"), QStringLiteral("First")}
    });
    const int secondId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/second.png")},
        {QStringLiteral("layerName"), QStringLiteral("Second")}
    });
    const int thirdId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/third.png")},
        {QStringLiteral("layerName"), QStringLiteral("Third")}
    });

    viewModel.setSelectedLayerId(secondId);
    QVERIFY(viewModel.moveLayer(0, 3));

    QCOMPARE(viewModel.layerAt(0).value(QStringLiteral("imageId")).toInt(), secondId);
    QCOMPARE(viewModel.layerAt(1).value(QStringLiteral("imageId")).toInt(), thirdId);
    QCOMPARE(viewModel.layerAt(2).value(QStringLiteral("imageId")).toInt(), firstId);
    QCOMPARE(viewModel.selectedLayerId(), secondId);
    QCOMPARE(viewModel.selectedLayerName(), QStringLiteral("Second"));
}

void tst_CanvasDocumentViewModel::movesAdjacentLayerForward()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    const int firstId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/first.png")},
        {QStringLiteral("layerName"), QStringLiteral("First")}
    });
    const int secondId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/second.png")},
        {QStringLiteral("layerName"), QStringLiteral("Second")}
    });
    const int thirdId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/third.png")},
        {QStringLiteral("layerName"), QStringLiteral("Third")}
    });

    QVERIFY(viewModel.moveLayer(0, 1));
    QCOMPARE(viewModel.layerAt(0).value(QStringLiteral("imageId")).toInt(), secondId);
    QCOMPARE(viewModel.layerAt(1).value(QStringLiteral("imageId")).toInt(), firstId);
    QCOMPARE(viewModel.layerAt(2).value(QStringLiteral("imageId")).toInt(), thirdId);
}

void tst_CanvasDocumentViewModel::removesSelectedLayerAndFallsBackToLast()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);

    const int firstId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/base.png")},
        {QStringLiteral("layerName"), QStringLiteral("Base")}
    });
    const int secondId = viewModel.appendLayer({
        {QStringLiteral("source"), QStringLiteral("file:///tmp/paint.png")},
        {QStringLiteral("layerName"), QStringLiteral("Paint")}
    });

    viewModel.setSelectedLayerId(firstId);
    QVERIFY(viewModel.removeLayerById(firstId));
    QCOMPARE(viewModel.selectedLayerId(), secondId);
    QCOMPARE(viewModel.selectedLayerName(), QStringLiteral("Paint"));

    QVERIFY(viewModel.removeLayerById(secondId));
    QCOMPARE(viewModel.selectedLayerId(), -1);
    QVERIFY(!viewModel.hasImportedLayer());
    QVERIFY(!viewModel.hasLayerSelection());
}

QTEST_APPLESS_MAIN(tst_CanvasDocumentViewModel)

#include "tst_canvasdocumentviewmodel.moc"
