'use strict';

const hist = {
    headYaw: [], headPitch: [],   
    gazeYaw: [], gazePitch: [],   
    au4: [], au7: [],
    maxLen: 75,  
};

let attnWindow = [];

function windowStd(arr) {
    if (arr.length < 5) return Infinity;
    const n = arr.length, mean = arr.reduce((a, b) => a + b, 0) / n;
    return Math.sqrt(arr.reduce((s, v) => s + (v - mean) * (v - mean), 0) / n);
}

function windowMean(arr) {
    return arr.length ? arr.reduce((a, b) => a + b, 0) / arr.length : 0;
}

function pushHist(arr, val) {
    arr.push(val);
    while (arr.length > hist.maxLen) arr.shift();
}

export function resetAttentionState() {
    for (const k of Object.keys(hist)) if (Array.isArray(hist[k])) hist[k] = [];
    attnWindow = [];
}

export function computeAttentionScore(m) {
    if (!m.found) {
        return { score: 0, level: 'none', gazeDesc: '--', headDesc: '--', rawScore: 0 };
    }

    
    const headYawDeg = m.pose_valid ? m.pose[1] : null;
    const headPitchDeg = m.pose_valid ? m.pose[0] : null;
    const gazeYawDeg = m.gaze_valid ? m.gaze[0] * 180 / Math.PI : null;
    const gazePitchDeg = m.gaze_valid ? m.gaze[1] * 180 / Math.PI : null;

    if (headYawDeg !== null) pushHist(hist.headYaw, headYawDeg);
    if (headPitchDeg !== null) pushHist(hist.headPitch, headPitchDeg);
    if (gazeYawDeg !== null) pushHist(hist.gazeYaw, gazeYawDeg);
    if (gazePitchDeg !== null) pushHist(hist.gazePitch, gazePitchDeg);

    let au4v = 0, au7v = 0, au43v = 0;
    if (m.aus && m.aus.length) {
        const au = Object.fromEntries(m.aus);
        au4v = au['AU4'] || 0; au7v = au['AU7'] || 0; au43v = au['AU43'] || 0;
    }
    pushHist(hist.au4, au4v); pushHist(hist.au7, au7v);

    const BASELINE = 0.80;

    
    let penalty = 0;

    
    const gazeYawStd = windowStd(hist.gazeYaw);
    const gazePitchStd = windowStd(hist.gazePitch);
    const gazeStd = (gazeYawStd === Infinity) ? 0 :
        Math.sqrt(gazeYawStd * gazeYawStd + gazePitchStd * gazePitchStd);
    if (gazeStd > 8) penalty += 0.30 * Math.min(1, (gazeStd - 8) / 12);

    
    let headJerk = 0;
    if (hist.headYaw.length >= 5) {
        const r = hist.headYaw.slice(-5);
        for (let i = 1; i < r.length; i++) headJerk = Math.max(headJerk, Math.abs(r[i] - r[i - 1]));
    }
    if (headJerk > 3) penalty += 0.15 * Math.min(1, (headJerk - 3) / 5);

    
    if (au43v >= 0.5) penalty += 0.20;

    
    if (hist.headYaw.length >= 5 && hist.gazeYaw.length >= 5) {
        const n = Math.min(hist.headYaw.length, hist.gazeYaw.length);
        const h = hist.headYaw.slice(-n), g = hist.gazeYaw.slice(-n);
        if ((h[n - 1] - h[0]) * (g[n - 1] - g[0]) < 0) penalty += 0.10;
    }

    
    const au4Avg = windowMean(hist.au4), au7Avg = windowMean(hist.au7);
    if (au4Avg < 0.15 && au7Avg < 0.1 && hist.au4.length >= 20) penalty += 0.05;

    penalty = Math.min(penalty, 0.65);

    
    let bonus = 0;

    
    if (gazeStd > 0 && gazeStd < 3) bonus += 0.06;

    
    const headYawStd = windowStd(hist.headYaw);
    if (headYawStd !== Infinity && headYawStd < 5) bonus += 0.05;

    
    if (au4Avg >= 0.4 || au7Avg >= 0.4) bonus += 0.04;

    
    if (hist.headYaw.length >= 10 && hist.gazeYaw.length >= 10) {
        const h = hist.headYaw.slice(-10), g = hist.gazeYaw.slice(-10);
        if ((h[9] - h[0]) * (g[9] - g[0]) > 0 && Math.abs(h[9] - h[0]) > 2) bonus += 0.03;
    }

    bonus = Math.min(bonus, 0.12);

    
    let rawScore = BASELINE - penalty + bonus;
    rawScore = Math.max(0, Math.min(1.0, rawScore));

    
    const alpha = 0.3;
    if (attnWindow.length === 0) attnWindow.push(rawScore);
    attnWindow.push(alpha * rawScore + (1 - alpha) * attnWindow[attnWindow.length - 1]);
    if (attnWindow.length > 30) attnWindow.shift();

    const finalScore = attnWindow[attnWindow.length - 1] || rawScore;
    const scorePct = Math.round(finalScore * 100);

    
    let level, gazeDesc, headDesc;
    if (scorePct >= 65) {
        level = 'focused';
        gazeDesc = gazeStd < 3 ? 'gaze locked' : gazeStd < 8 ? 'tracking' : 'stable';
        headDesc = headJerk < 1 ? 'head still' : headJerk < 3 ? 'head tracking' : 'active';
    } else if (scorePct >= 40) {
        level = 'partial';
        gazeDesc = gazeStd > 12 ? 'drifting' : 'shifting';
        headDesc = headJerk > 5 ? 'jerky' : 'active';
    } else {
        level = 'distracted';
        gazeDesc = 'wandering';
        headDesc = 'distracted';
    }

    return { score: scorePct, level, gazeDesc, headDesc, rawScore: finalScore };
}

export const AU_MEANINGS = {
    AU1: '眉毛内角抬起', AU2: '眉毛外角抬起', AU4: '眉毛整体压低',
    AU5: '上眼睑抬起', AU6: '脸颊抬起', AU7: '眼睑收紧',
    AU9: '鼻根皱起', AU10: '上唇抬起', AU11: '鼻唇沟加深', AU12: '嘴角向外上扬',
    AU13: '嘴角急剧上扬', AU14: '嘴角收紧(酒窝)', AU15: '嘴角下压',
    AU16: '下唇下压', AU17: '下巴抬起', AU18: '嘴唇缩拢',
    AU19: '舌头露出', AU20: '嘴角横向拉伸', AU22: '嘴唇外翻(漏斗状)',
    AU23: '嘴唇收紧', AU24: '嘴唇压紧', AU25: '双唇分开', AU26: '下颌下坠',
    AU27: '嘴巴张大', AU32: '咬唇', AU38: '鼻孔扩张', AU39: '鼻孔收紧',
};

export function classifyEmotion(aus) {
    return { name: '-', ekman: '-' };
}
