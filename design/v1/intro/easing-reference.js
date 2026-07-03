/**
 * MyUI Intro — 缓动函数参考实现
 * Agent：将此文件逻辑 1:1 移植到 IntroEasing.java
 * 用法：easePremium(t) 其中 t = 0..1
 */

function cubicBezier(x1, y1, x2, y2) {
  return function (t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    var cx = 3 * x1;
    var bx = 3 * (x2 - x1) - cx;
    var ax = 1 - cx - bx;
    var cy = 3 * y1;
    var by = 3 * (y2 - y1) - cy;
    var ay = 1 - cy - by;
    function sampleX(tt) {
      return ((ax * tt + bx) * tt + cx) * tt;
    }
    function sampleY(tt) {
      return ((ay * tt + by) * tt + cy) * tt;
    }
    var x = t;
    for (var i = 0; i < 8; i++) {
      var curX = sampleX(x) - t;
      if (Math.abs(curX) < 1e-5) break;
      var dX = (3 * ax * x + 2 * bx) * x + cx;
      if (Math.abs(dX) < 1e-6) break;
      x -= curX / dX;
    }
    return sampleY(x);
  };
}

var easePremium = cubicBezier(0.16, 1, 0.3, 1);
var easeCinema = cubicBezier(0.77, 0, 0.175, 1);
var spring = cubicBezier(0.34, 1.56, 0.64, 1);
var springGentle = cubicBezier(0.22, 1.2, 0.36, 1);

function lerp(a, b, t) {
  return a + (b - a) * t;
}

function lerpKeyframes(normalizedT, keyframeTimes, keyframeValues) {
  if (normalizedT <= keyframeTimes[0]) return keyframeValues[0];
  for (var i = 1; i < keyframeTimes.length; i++) {
    if (normalizedT <= keyframeTimes[i]) {
      var localT = (normalizedT - keyframeTimes[i - 1]) / (keyframeTimes[i] - keyframeTimes[i - 1]);
      return lerp(keyframeValues[i - 1], keyframeValues[i], localT);
    }
  }
  return keyframeValues[keyframeValues.length - 1];
}

function trackProgress(elapsedMs, startMs, durationMs) {
  if (elapsedMs <= startMs) return 0;
  if (elapsedMs >= startMs + durationMs) return 1;
  return (elapsedMs - startMs) / durationMs;
}

function easedTrackProgress(elapsedMs, startMs, durationMs, easingFn) {
  return easingFn(trackProgress(elapsedMs, startMs, durationMs));
}

// 示例：iris 半径百分比
function getIrisRadiusPct(elapsedMs, durationMs) {
  var t = elapsedMs / durationMs;
  return lerpKeyframes(
    t,
    [0, 0.42, 0.58, 0.72, 0.88, 1],
    [0, 0, 28, 58, 92, 150]
  );
}

// 示例：void opacity
function getVoidOpacity(elapsedMs, durationMs) {
  var t = elapsedMs / durationMs;
  var raw = lerpKeyframes(t, [0, 0.38, 0.58, 1], [1, 1, 0.65, 0]);
  return easePremium(Math.min(1, t * 1.2)) * raw;
}

if (typeof module !== 'undefined') {
  module.exports = {
    easePremium,
    easeCinema,
    spring,
    springGentle,
    lerp,
    lerpKeyframes,
    trackProgress,
    easedTrackProgress,
    getIrisRadiusPct,
    getVoidOpacity
  };
}
