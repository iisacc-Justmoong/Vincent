#pragma once

#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class CanvasBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)

public:
    explicit CanvasBackend(QObject *parent = nullptr);

    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

    Q_INVOKABLE QVariantMap captureSnapshot(int canvasWidth,
                                            int canvasHeight,
                                            const QVariantList &strokes,
                                            const QVariantList &images,
                                            int selectedImageId) const;
    Q_INVOKABLE void pushUndoState(const QVariantMap &snapshot, int maxUndoSteps);
    Q_INVOKABLE QVariantMap undo(const QVariantMap &currentSnapshot, int maxUndoSteps);
    Q_INVOKABLE QVariantMap redo(const QVariantMap &currentSnapshot, int maxUndoSteps);
    Q_INVOKABLE void clearHistory();

    Q_INVOKABLE QVariantMap documentFitTransform(int canvasWidth,
                                                 int canvasHeight,
                                                 int documentWidth,
                                                 int documentHeight) const;
    Q_INVOKABLE QVariantMap resetImagePlacement(int canvasWidth,
                                                int canvasHeight,
                                                int originalWidth,
                                                int originalHeight) const;
    Q_INVOKABLE QVariantMap resolveTransform(const QString &role,
                                             qreal dx,
                                             qreal dy,
                                             const QVariantMap &startRect,
                                             qreal minSize,
                                             int canvasWidth,
                                             int canvasHeight,
                                             bool constrainAspect) const;

signals:
    void canUndoChanged();
    void canRedoChanged();

private:
    [[nodiscard]] QVariant deepClone(const QVariant &value) const;
    [[nodiscard]] QVariantMap cloneMap(const QVariantMap &value) const;
    [[nodiscard]] QVariantList cloneList(const QVariantList &value) const;
    static void trimStack(QVector<QVariantMap> &stack, int maxUndoSteps);
    void emitHistorySignals(bool previousCanUndo, bool previousCanRedo);

    QVector<QVariantMap> m_undoStack;
    QVector<QVariantMap> m_redoStack;
};
