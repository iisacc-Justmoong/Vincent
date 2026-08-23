pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: laserPointer
    clip: true
    enabled: visible

    readonly property int trailLifetimeMs: 2000
    readonly property int repaintIntervalMs: 16
    readonly property int maximumTrailPointCount: 256
    readonly property real minimumTrailSampleDistance: 2
    readonly property real trailLineWidth: 6
    readonly property real trailGlowRadius: 8
    readonly property color laserColor: "#ff2b2b"

    property var trailPoints: []
    property bool laserActive: false
    property real currentPointX: 0
    property real currentPointY: 0

    signal wheelZoomRequested(real angleDeltaY, real pixelDeltaY)

    function appendTrailPoint(x, y, forceSample) {
        const now = Date.now();
        const points = laserPointer.trailPoints.slice();
        if (!forceSample && points.length > 0) {
            const previousPoint = points[points.length - 1];
            const deltaX = x - previousPoint.x;
            const deltaY = y - previousPoint.y;
            const minimumDistanceSquared = laserPointer.minimumTrailSampleDistance * laserPointer.minimumTrailSampleDistance;
            if (deltaX * deltaX + deltaY * deltaY < minimumDistanceSquared) {
                return;
            }
        }

        points.push({
            x: x,
            y: y,
            createdAt: now
        });
        if (points.length > laserPointer.maximumTrailPointCount) {
            points.splice(0, points.length - laserPointer.maximumTrailPointCount);
        }
        laserPointer.trailPoints = points;
        trailCanvas.requestPaint();
    }

    function beginLaserPoint(x, y) {
        laserPointer.currentPointX = x;
        laserPointer.currentPointY = y;
        laserPointer.laserActive = true;
        laserPointer.appendTrailPoint(x, y, true);
    }

    function updateLaserPoint(x, y) {
        if (!laserPointer.laserActive) {
            return;
        }
        laserPointer.currentPointX = x;
        laserPointer.currentPointY = y;
        laserPointer.appendTrailPoint(x, y, false);
    }

    function endLaserPoint(x, y) {
        if (!laserPointer.laserActive) {
            return;
        }
        laserPointer.currentPointX = x;
        laserPointer.currentPointY = y;
        laserPointer.appendTrailPoint(x, y, true);
        laserPointer.laserActive = false;
        trailCanvas.requestPaint();
    }

    function cancelLaserPoint() {
        laserPointer.laserActive = false;
        trailCanvas.requestPaint();
    }

    function pruneExpiredTrailPoints() {
        const cutoff = Date.now() - laserPointer.trailLifetimeMs;
        const remainingPoints = laserPointer.trailPoints.filter(point => point.createdAt > cutoff);
        if (remainingPoints.length !== laserPointer.trailPoints.length) {
            laserPointer.trailPoints = remainingPoints;
        }
        trailCanvas.requestPaint();
    }

    function clearLaserTrail() {
        laserPointer.laserActive = false;
        laserPointer.trailPoints = [];
        trailCanvas.requestPaint();
    }

    onVisibleChanged: if (!visible) {
        laserPointer.clearLaserTrail();
    }

    Timer {
        id: trailRepaintTimer
        interval: laserPointer.repaintIntervalMs
        repeat: true
        running: laserPointer.laserActive || laserPointer.trailPoints.length > 0
        onTriggered: laserPointer.pruneExpiredTrailPoints()
    }

    Canvas {
        id: trailCanvas
        anchors.fill: parent
        antialiasing: true
        renderTarget: Canvas.Image

        onPaint: {
            const context = getContext("2d");
            context.clearRect(0, 0, width, height);

            const now = Date.now();
            const points = laserPointer.trailPoints;
            context.lineCap = "round";
            context.lineJoin = "round";
            context.strokeStyle = laserPointer.laserColor;
            context.fillStyle = laserPointer.laserColor;

            for (let index = 0; index < points.length; ++index) {
                const point = points[index];
                const ageMs = now - point.createdAt;
                const opacity = Math.max(0, Math.min(1, 1 - ageMs / laserPointer.trailLifetimeMs));
                if (opacity <= 0) {
                    continue;
                }

                if (index > 0) {
                    const previousPoint = points[index - 1];
                    const previousAgeMs = now - previousPoint.createdAt;
                    const previousOpacity = Math.max(0, Math.min(1, 1 - previousAgeMs / laserPointer.trailLifetimeMs));
                    context.beginPath();
                    context.moveTo(previousPoint.x, previousPoint.y);
                    context.lineTo(point.x, point.y);
                    context.lineWidth = laserPointer.trailLineWidth;
                    context.globalAlpha = Math.min(opacity, previousOpacity) * 0.55;
                    context.stroke();
                }

                context.beginPath();
                context.arc(point.x, point.y, laserPointer.trailGlowRadius, 0, Math.PI * 2);
                context.globalAlpha = opacity * 0.18;
                context.fill();
                context.beginPath();
                context.arc(point.x, point.y, laserPointer.trailLineWidth / 2, 0, Math.PI * 2);
                context.globalAlpha = opacity * 0.82;
                context.fill();
            }
            context.globalAlpha = 1;
        }
    }

    Item {
        id: activeLaserPoint
        visible: laserPointer.laserActive
        x: laserPointer.currentPointX - width / 2
        y: laserPointer.currentPointY - height / 2
        width: 28
        height: 28

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: Qt.rgba(1, 0.17, 0.17, 0.14)
            antialiasing: true
        }

        Rectangle {
            anchors.centerIn: parent
            width: 16
            height: 16
            radius: width / 2
            color: Qt.rgba(1, 0.17, 0.17, 0.28)
            antialiasing: true
        }

        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: width / 2
            color: "#ff2b2b"
            antialiasing: true
        }
    }

    MouseArea {
        id: laserMouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        preventStealing: true
        cursorShape: pressed ? Qt.BlankCursor : Qt.ArrowCursor

        onPressed: function (mouse) {
            laserPointer.beginLaserPoint(mouse.x, mouse.y);
            mouse.accepted = true;
        }

        onPositionChanged: function (mouse) {
            if (pressed) {
                laserPointer.updateLaserPoint(mouse.x, mouse.y);
                mouse.accepted = true;
            }
        }

        onReleased: function (mouse) {
            laserPointer.endLaserPoint(mouse.x, mouse.y);
            mouse.accepted = true;
        }

        onWheel: function (wheel) {
            if (wheel.angleDelta.y === 0 && wheel.pixelDelta.y === 0) {
                wheel.accepted = false;
                return;
            }
            laserPointer.wheelZoomRequested(wheel.angleDelta.y, wheel.pixelDelta.y);
            wheel.accepted = true;
        }

        onCanceled: laserPointer.cancelLaserPoint()
    }
}
