import QtQuick 2.12

Item {
    id: root
    width: 96
    height: 96
    opacity: 0

    property bool shown: false
    property string stateName: "idle"
    property real audioLevel: 0
    property real orbSize: 0
    property real phase: 0
    property real gentlePulse: 0
    signal hideFinished()

    Timer {
        interval: 16
        repeat: true
        running: root.shown || root.opacity > 0
        onTriggered: root.phase += 0.022 + root.audioLevel * 0.04
    }

    onShownChanged: {
        if (shown) {
            hideAnimation.stop()
            showAnimation.restart()
        } else {
            showAnimation.stop()
            hideAnimation.restart()
        }
    }

    SequentialAnimation {
        id: showAnimation
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; duration: 150; easing.type: Easing.OutCubic }
            SequentialAnimation {
                NumberAnimation { target: root; property: "orbSize"; from: 0; to: 62; duration: 190; easing.type: Easing.OutBack }
                NumberAnimation { target: root; property: "orbSize"; from: 62; to: 56; duration: 120; easing.type: Easing.OutCubic }
            }
        }
    }

    SequentialAnimation {
        id: hideAnimation
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; from: root.opacity; to: 0; duration: 170; easing.type: Easing.InCubic }
            NumberAnimation { target: root; property: "orbSize"; from: Math.max(root.orbSize, 56); to: 0; duration: 170; easing.type: Easing.InCubic }
        }
        ScriptAction { script: root.hideFinished() }
    }

    NumberAnimation on gentlePulse {
        from: 0
        to: 1
        duration: 1200
        loops: Animation.Infinite
        running: root.shown
        easing.type: Easing.InOutSine
    }

    property real liveSize: {
        if (!shown) {
            return orbSize
        }

        var idleMotion = 1.8 * Math.sin(gentlePulse * Math.PI * 2.0)
        var voiceMotion = audioLevel * 11.0
        if (stateName === "listening") {
            return Math.max(54, Math.min(68, 56 + voiceMotion + idleMotion))
        }
        if (stateName === "recognizing" || stateName === "done") {
            return Math.max(52, Math.min(62, 56 + idleMotion * 0.6))
        }
        return Math.max(52, Math.min(60, 56 + idleMotion))
    }

    ShaderEffect {
        id: ambientGlow
        anchors.centerIn: parent
        width: root.liveSize + 40
        height: root.liveSize + 40
        opacity: 0.55 + root.audioLevel * 0.18
        property real time: root.phase
        property real level: root.audioLevel
        fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            uniform lowp float qt_Opacity;
            uniform highp float time;
            uniform highp float level;
            void main() {
                highp vec2 p = qt_TexCoord0 * 2.0 - 1.0;
                highp float d = length(p);
                highp float pulse = 0.035 * sin(time * 2.0);
                highp float alpha = smoothstep(1.06 + pulse + level * 0.06, 0.52, d);
                alpha *= smoothstep(1.14, 0.78, d);
                highp float sweep = 0.5 + 0.5 * sin(atan(p.y, p.x) * 2.0 - time * 1.25);
                highp vec3 color = mix(vec3(0.26, 0.48, 1.0), vec3(1.0, 0.48, 0.18), sweep);
                color = mix(color, vec3(0.75, 0.96, 1.0), 0.18 + level * 0.22);
                highp float a = alpha * 0.30 * qt_Opacity;
                gl_FragColor = vec4(color * a, a);
            }"
    }

    ShaderEffect {
        id: glassOrb
        anchors.centerIn: parent
        width: root.liveSize
        height: root.liveSize
        opacity: 0.98
        property real time: root.phase
        property real level: root.audioLevel
        property real listening: root.stateName === "listening" ? 1.0 : 0.0
        property real recognizing: root.stateName === "recognizing" ? 1.0 : 0.0
        fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            uniform lowp float qt_Opacity;
            uniform highp float time;
            uniform highp float level;
            uniform highp float listening;
            uniform highp float recognizing;

            void main() {
                highp vec2 uv = qt_TexCoord0;
                highp vec2 p = uv * 2.0 - 1.0;
                highp float d = length(p);
                if (d > 1.0) {
                    gl_FragColor = vec4(0.0);
                    return;
                }

                highp float z = sqrt(max(0.0, 1.0 - d * d));
                highp vec3 normal = normalize(vec3(p.x, p.y, z));
                highp vec3 viewDir = vec3(0.0, 0.0, 1.0);
                highp vec3 lightDir = normalize(vec3(-0.42, -0.62, 0.82));
                highp vec3 warmLight = normalize(vec3(0.65, 0.42, 0.62));

                highp float diffuse = max(dot(normal, lightDir), 0.0);
                highp float warm = max(dot(normal, warmLight), 0.0);
                highp float fresnel = pow(1.0 - z, 2.2);
                highp float rim = smoothstep(0.68, 1.0, d);
                highp float angle = atan(p.y, p.x);

                highp float rotatedX = p.x * cos(time * 0.34) - p.y * sin(time * 0.34);
                highp float rotatedY = p.x * sin(time * 0.34) + p.y * cos(time * 0.34);
                highp float flowA = sin(rotatedX * 5.4 + rotatedY * 2.1 + z * 3.0 - time * 1.9);
                highp float flowB = sin(angle * 2.15 - rotatedY * 4.2 + time * 1.35);
                highp float flowC = sin((rotatedX - rotatedY) * 6.0 + z * 4.8 + time * 2.25);
                highp float blueBand = smoothstep(0.12, 0.95, flowA) * (1.0 - rim * 0.22);
                highp float violetBand = smoothstep(0.25, 1.0, flowB) * smoothstep(0.95, 0.12, d);
                highp float fireBand = smoothstep(0.42, 1.0, flowC) * smoothstep(0.2, 0.95, uv.x) * smoothstep(0.08, 0.9, uv.y);

                highp vec3 glassBase = mix(vec3(0.18, 0.38, 0.94), vec3(0.69, 0.90, 1.0), diffuse * 0.65 + 0.22);
                highp vec3 inner = glassBase;
                inner = mix(inner, vec3(0.35, 0.30, 1.0), blueBand * 0.50);
                inner = mix(inner, vec3(0.62, 0.28, 0.96), violetBand * 0.62);
                inner = mix(inner, vec3(1.0, 0.48, 0.08), fireBand * (0.36 + listening * 0.16));
                inner = mix(inner, vec3(1.0, 0.16, 0.07), fireBand * warm * 0.25);

                highp float upperSpark = smoothstep(0.23, 0.0, distance(uv, vec2(0.36, 0.23)));
                highp float lowerSpark = smoothstep(0.28, 0.0, distance(uv, vec2(0.39, 0.72)));
                highp float sideSpark = smoothstep(0.20, 0.0, distance(uv, vec2(0.73, 0.38)));
                highp float curvedCut = abs((uv.y - 0.58) + 0.30 * sin((uv.x + time * 0.16) * 5.7));
                highp float glassRibbon = smoothstep(0.055, 0.0, curvedCut) * smoothstep(0.92, 0.18, d);

                highp vec3 reflected = reflect(-lightDir, normal);
                highp float spec = pow(max(dot(reflected, viewDir), 0.0), 54.0);
                highp float hotSpec = pow(max(dot(reflect(-warmLight, normal), viewDir), 0.0), 42.0);

                highp vec3 color = inner;
                color += vec3(0.92, 0.98, 1.0) * (upperSpark * 0.42 + spec * 0.38);
                color += vec3(0.78, 0.95, 1.0) * lowerSpark * 0.18;
                color += vec3(1.0, 0.74, 0.38) * (sideSpark * 0.18 + hotSpec * 0.22);
                color += vec3(0.86, 0.95, 1.0) * glassRibbon * (0.16 + level * 0.14);
                color += mix(vec3(0.42, 0.64, 1.0), vec3(1.0, 0.74, 0.32), 0.45 + 0.55 * sin(angle * 2.0 - time)) * fresnel * 0.55;
                color += recognizing * vec3(0.08, 0.08, 0.18) * (0.35 + 0.35 * sin(time * 4.0));

                highp float edgeMask = smoothstep(1.0, 0.965, d);
                highp float alpha = edgeMask * (0.67 + fresnel * 0.30 + diffuse * 0.08 + level * 0.04);
                highp float a = clamp(alpha, 0.0, 0.96) * qt_Opacity;
                gl_FragColor = vec4(color * a, a);
            }"
    }

    ShaderEffect {
        id: refractedHighlights
        anchors.centerIn: parent
        width: root.liveSize + 2
        height: root.liveSize + 2
        opacity: 0.82
        property real time: root.phase
        property real level: root.audioLevel
        fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            uniform lowp float qt_Opacity;
            uniform highp float time;
            uniform highp float level;
            void main() {
                highp vec2 uv = qt_TexCoord0;
                highp vec2 p = uv * 2.0 - 1.0;
                highp float d = length(p);
                if (d > 1.0) {
                    gl_FragColor = vec4(0.0);
                    return;
                }

                highp float mask = smoothstep(1.0, 0.92, d);
                highp float sweep = abs((uv.x - 0.50) * 0.55 + (uv.y - 0.50) + 0.18 * sin(uv.x * 6.0 + time * 1.15));
                highp float ribbon = smoothstep(0.045, 0.0, sweep) * smoothstep(0.92, 0.22, d);
                highp float dotA = smoothstep(0.13, 0.0, distance(uv, vec2(0.31 + 0.035 * sin(time * 0.9), 0.28)));
                highp float dotB = smoothstep(0.12, 0.0, distance(uv, vec2(0.54, 0.78 + 0.025 * cos(time))));
                highp float rimLine = smoothstep(0.995, 0.94, d) * smoothstep(0.72, 0.94, d);
                highp vec3 color = mix(vec3(0.95, 0.99, 1.0), vec3(1.0, 0.72, 0.35), 0.5 + 0.5 * sin(time + uv.x * 5.0));
                highp float alpha = (ribbon * (0.20 + level * 0.20) + dotA * 0.28 + dotB * 0.12 + rimLine * 0.22) * mask;
                highp float a = alpha * qt_Opacity;
                gl_FragColor = vec4(color * a, a);
            }"
    }
}
