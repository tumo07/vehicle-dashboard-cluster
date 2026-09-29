import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Shapes
import QtCore
import QtQuick.Controls.Basic
import Qt5Compat.GraphicalEffects

Rectangle {
    id: root
    color: "#161618"
    radius: 30

    // Declare color
    property color brandBlue: "#20b2f0"
    property color pureWhite: "#ffffff"

    // 1. Dùng Canvas để vẽ đồ họa
    Canvas {
        id: dashboardCanvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            var cx = width / 2;
            var cy = height / 2;
            var PI = Math.PI;

            function drawArc(radius, startAngle, endAngle, color, lineWidth) {
                ctx.beginPath();
                ctx.arc(cx, cy, radius, startAngle, endAngle);
                ctx.strokeStyle = color;
                ctx.lineWidth = lineWidth;
                ctx.stroke();
            }

            // Vòng cung RPM
            drawArc(190, PI * 0.75, PI * 1.25, brandBlue, 18);
            // Vòng cung Nhiên liệu
            drawArc(190, -PI * 0.25, PI * 0.05, pureWhite, 18);
            drawArc(190, PI * 0.05, PI * 0.25, brandBlue, 18);
            // Vòng cung giữa
            drawArc(150, PI * 0.70, PI * 2.30, pureWhite, 4);
            drawArc(135, PI * 0.73, PI * 1.75, brandBlue, 10);
        }
    }

    // 2. Chữ và số trên giao diện
    Item {
        anchors.centerIn: parent

        // --- CÁC NHÃN BÊN TRÁI (1 ĐẾN 5) ---
        Text { text: "5"; color: brandBlue; font.pixelSize: 22; font.bold: true; x: -245; y: -130 }
        Text { text: "4"; color: brandBlue; font.pixelSize: 22; font.bold: true; x: -265; y: -65 }
        Text { text: "3"; color: brandBlue; font.pixelSize: 22; font.bold: true; x: -275; y: -12 }
        Text { text: "2"; color: brandBlue; font.pixelSize: 22; font.bold: true; x: -265; y: 41 }
        Text { text: "1"; color: brandBlue; font.pixelSize: 22; font.bold: true; x: -245; y: 106 }

        // --- CÁC NHÃN BÊN PHẢI (MỨC NHIÊN LIỆU) ---
        Text { text: "FULL"; color: brandBlue; font.pixelSize: 18; font.bold: true; x: 210; y: -130 }
        Text { text: "HALF"; color: brandBlue; font.pixelSize: 18; font.bold: true; x: 235; y: -10 }
        Text { text: "EMPTY"; color: brandBlue; font.pixelSize: 18; font.bold: true; x: 210; y: 110 }
    }

    // 3. Cụm Tốc độ ở chính giữa
    Item {
        anchors.centerIn: parent
        width: 180
        height: 150

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 0; width: 160; height: 3
            color: pureWhite
        }

        Text {
            text: "231"
            font.pixelSize: 72; font.bold: true
            color: brandBlue
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: 3
            y: 13
        }
        Text {
            text: "231"
            font.pixelSize: 72; font.bold: true
            color: pureWhite
            anchors.horizontalCenter: parent.horizontalCenter
            y: 10
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 105; width: 160; height: 3
            color: pureWhite
        }

        Text {
            text: "mph"
            font.pixelSize: 32; font.bold: true
            color: brandBlue
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: 2
            y: 118
        }
        Text {
            text: "km/h"
            font.pixelSize: 32; font.bold: true
            color: pureWhite
            anchors.horizontalCenter: parent.horizontalCenter
            y: 115
        }
    }

    // 4. PRND
    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12

        Text { text: "P"; color: brandBlue; font.pixelSize: 18; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
        Text { text: "R"; color: brandBlue; font.pixelSize: 18; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
        Text { text: "N"; color: brandBlue; font.pixelSize: 18; font.bold: true; anchors.verticalCenter: parent.verticalCenter }

        Text { text: "<"; color: pureWhite; font.pixelSize: 24; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
        Text { text: "D"; color: pureWhite; font.pixelSize: 42; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
        Text { text: ">"; color: pureWhite; font.pixelSize: 24; font.bold: true; anchors.verticalCenter: parent.verticalCenter }

        Text { text: "L"; color: brandBlue; font.pixelSize: 18; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
    }
}
