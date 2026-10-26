#!/usr/bin/env python3
"""
Label captured data, train the risk model, compare it with the rule
baseline, and export everything the firmware needs.

Input : one or more CSVs from capture_data.py (MODE COLLECT or MODE AUTO runs)
Output: Core/Inc/model_weights.h  (weights + scaler + thresholds, plain C)
        <out-dir>/model.keras, model.tflite   (for X-CUBE-AI later, optional)
        <out-dir>/report.md                   (metrics: ML vs rule baseline)
        <out-dir>/*.png                       (plots for the report, if matplotlib exists)

Example:
    python scripts/train_model.py --data data/*.csv
    python scripts/train_model.py --data data/*.csv --horizon 15 --caution-rise 4 --critical-rise 6

Install:
    python -m pip install tensorflow pandas numpy matplotlib
"""

import argparse
import glob
import json
import sys
from pathlib import Path

import numpy as np
import pandas as pd

# Must match feature_extraction.h / app_config.h
FEATURE_WINDOW = 10
FEATURE_NAMES = [
    "temp_rise", "temp_slope10", "temp_slope3",
    "power_now", "power_avg10", "power_slope10", "workload",
]
CLASS_NAMES = ["NORMAL", "CAUTION", "CRITICAL"]
GAP_MS = 2500          # a bigger jump between rows starts a new segment
HIDDEN1 = 16
HIDDEN2 = 16

REPO_ROOT = Path(__file__).resolve().parent.parent


def parse_args():
    p = argparse.ArgumentParser(description="Train the thermal/power risk model.")
    p.add_argument("--data", nargs="+", required=True, help="CSV files (globs allowed)")
    p.add_argument("--horizon", type=int, default=15, help="prediction horizon H in seconds (default 15)")
    p.add_argument("--caution-rise", type=float, default=None, help="degC above baseline (default: from data)")
    p.add_argument("--critical-rise", type=float, default=None, help="degC above baseline (default: from data)")
    p.add_argument("--caution-power", type=float, default=None, help="mW (default: from data)")
    p.add_argument("--critical-power", type=float, default=None, help="mW (default: from data)")
    p.add_argument("--include-run", action="store_true",
                   help="also use rows recorded in MODE RUN (reaction active; normally excluded)")
    p.add_argument("--epochs", type=int, default=120)
    p.add_argument("--seed", type=int, default=42, help="first seed")
    p.add_argument("--seeds", type=int, default=5,
                   help="train this many seeds and keep the one with the lowest validation loss (default 5)")
    p.add_argument("--header", type=Path, default=REPO_ROOT / "Core" / "Inc" / "model_weights.h")
    p.add_argument("--out-dir", type=Path, default=REPO_ROOT / "model_out")
    return p.parse_args()


# ------------------------------------------------------------------ data

def load_segments(patterns, include_run):
    files = []
    for pat in patterns:
        files.extend(sorted(glob.glob(pat)) or [pat])
    segments = []
    for f in files:
        df = pd.read_csv(f)
        if not include_run:
            df = df[df["mode"] != 0]
        if df.empty:
            print(f"  {f}: no COLLECT/AUTO rows, skipped")
            continue
        ts = df["timestamp_ms"].to_numpy()
        breaks = np.where((np.diff(ts) > GAP_MS) | (np.diff(ts) <= 0))[0] + 1
        for part in np.split(np.arange(len(df)), breaks):
            seg = df.iloc[part].reset_index(drop=True)
            if len(seg) > FEATURE_WINDOW + 30:
                segments.append(seg)
        print(f"  {f}: {len(df)} rows")
    if not segments:
        sys.exit("No usable data. Record with MODE AUTO or MODE COLLECT first.")
    return segments


def window_slope(x, i, span):
    avail = min(i + 1, FEATURE_WINDOW)
    span = min(span, avail - 1)
    return 0.0 if span == 0 else (x[i] - x[i - span]) / span


def compute_features(seg):
    """Exact mirror of Features_Update() in feature_extraction.c."""
    rise = seg["temp_rise_c"].to_numpy(dtype=np.float64)
    power = seg["power_mw"].to_numpy(dtype=np.float64)
    wl = seg["workload_eff"].to_numpy(dtype=np.float64)
    n = len(seg)
    X = np.zeros((n, len(FEATURE_NAMES)), dtype=np.float32)
    for i in range(n):
        avail = min(i + 1, FEATURE_WINDOW)
        X[i] = [
            rise[i],
            window_slope(rise, i, FEATURE_WINDOW - 1),
            window_slope(rise, i, 3),
            power[i],
            power[i - avail + 1:i + 1].mean(),
            window_slope(power, i, FEATURE_WINDOW - 1),
            wl[i],
        ]
    return X


def classify(values, caution, critical):
    return np.where(values >= critical, 2, np.where(values >= caution, 1, 0)).astype(np.int64)


def future_targets(seg, h):
    """Max rise and mean power over the next H samples (NaN where the future is unknown)."""
    rise = seg["temp_rise_c"].to_numpy(dtype=np.float64)
    power = seg["power_mw"].to_numpy(dtype=np.float64)
    n = len(seg)
    fut_rise = np.full(n, np.nan)
    fut_power = np.full(n, np.nan)
    for i in range(n - h):
        fut_rise[i] = rise[i + 1:i + 1 + h].max()
        fut_power[i] = power[i + 1:i + 1 + h].mean()
    return fut_rise, fut_power


def rule_predict(X, h, c_rise, k_rise, c_pow, k_pow):
    """Mirror of rule_predict() in predictor.c."""
    t_slope = np.maximum(X[:, 1], 0.0)
    rise_future = X[:, 0] + t_slope * h
    p_future = X[:, 3] + X[:, 5] * h
    return classify(rise_future, c_rise, k_rise), classify(p_future, c_pow, k_pow)


# ------------------------------------------------------------------ metrics

def metrics(y_true, y_pred):
    cm = np.zeros((3, 3), dtype=int)
    for t, p in zip(y_true, y_pred):
        cm[t, p] += 1
    acc = np.trace(cm) / max(cm.sum(), 1)
    f1s = []
    for k in range(3):
        tp = cm[k, k]
        prec = tp / max(cm[:, k].sum(), 1)
        rec = tp / max(cm[k, :].sum(), 1)
        f1s.append(0.0 if prec + rec == 0 else 2 * prec * rec / (prec + rec))
    normal = cm[0].sum()
    crit = cm[2].sum()
    return {
        "accuracy": acc,
        "macro_f1": float(np.mean(f1s)),
        "false_alarm_rate": (cm[0, 1] + cm[0, 2]) / normal if normal else float("nan"),
        "critical_recall": cm[2, 2] / crit if crit else float("nan"),
        "critical_missed_as_normal": cm[2, 0] / crit if crit else float("nan"),
        "confusion": cm.tolist(),
    }


def lead_times(rise, pred, critical, lookback=60):
    """Seconds of CRITICAL warning (prediction == CRITICAL held continuously)
    before each real crossing of the critical limit. 0 = not predicted in time.
    The best possible value is about the horizon H."""
    leads = []
    for i in range(1, len(rise)):
        if rise[i] >= critical > rise[i - 1]:
            j = i
            while j - 1 >= max(0, i - lookback) and pred[j - 1] == 2:
                j -= 1
            leads.append(i - j)
    return leads


# ------------------------------------------------------------------ model

def build_model(n_feat, seed):
    import tensorflow as tf
    tf.keras.utils.set_random_seed(seed)
    inp = tf.keras.Input(shape=(n_feat,), name="features")
    x = tf.keras.layers.Dense(HIDDEN1, activation="relu", name="dense1")(inp)
    x = tf.keras.layers.Dense(HIDDEN2, activation="relu", name="dense2")(x)
    out_t = tf.keras.layers.Dense(3, activation="softmax", name="temp_risk")(x)
    out_p = tf.keras.layers.Dense(3, activation="softmax", name="power_risk")(x)
    model = tf.keras.Model(inp, [out_t, out_p])
    model.compile(optimizer=tf.keras.optimizers.Adam(3e-3),
                  loss=["sparse_categorical_crossentropy", "sparse_categorical_crossentropy"])
    return model


def class_weights(y):
    counts = np.bincount(y, minlength=3).astype(np.float64)
    w = np.where(counts > 0, len(y) / (3 * np.maximum(counts, 1)), 0.0)
    return w[y]


def c_array(name, arr):
    arr = np.asarray(arr, dtype=np.float32)
    fmt = lambda v: f"{v:.8e}f"
    if arr.ndim == 1:
        return f"static const float {name}[{arr.shape[0]}] = {{ {', '.join(fmt(v) for v in arr)} }};\n"
    rows = ",\n".join("    { " + ", ".join(fmt(v) for v in row) + " }" for row in arr)
    return f"static const float {name}[{arr.shape[0]}][{arr.shape[1]}] = {{\n{rows}\n}};\n"


def export_header(path, model, mean, std, h, thr):
    w1, b1 = model.get_layer("dense1").get_weights()
    w2, b2 = model.get_layer("dense2").get_weights()
    wt, bt = model.get_layer("temp_risk").get_weights()
    wp, bp = model.get_layer("power_risk").get_weights()
    text = (
        "/*\n * model_weights.h\n *\n * GENERATED by scripts/train_model.py — do not edit by hand.\n"
        " * Dense(16, relu) -> Dense(16, relu) -> two Dense(3) heads (temp risk, power risk).\n"
        " * Weights are stored [out][in].\n */\n\n"
        "#ifndef MODEL_WEIGHTS_H\n#define MODEL_WEIGHTS_H\n\n"
        "#define MODEL_TRAINED            1\n"
        f"#define MODEL_N_FEAT             {len(mean)}\n"
        f"#define MODEL_HIDDEN1            {HIDDEN1}\n"
        f"#define MODEL_HIDDEN2            {HIDDEN2}\n"
        "#define MODEL_N_CLASSES          3\n"
        f"#define MODEL_HORIZON_S          {h}\n"
        f"#define MODEL_CAUTION_RISE_C     {thr['caution_rise']:.2f}f\n"
        f"#define MODEL_CRITICAL_RISE_C    {thr['critical_rise']:.2f}f\n"
        f"#define MODEL_CAUTION_POWER_MW   {thr['caution_power']:.1f}f\n"
        f"#define MODEL_CRITICAL_POWER_MW  {thr['critical_power']:.1f}f\n\n"
        + c_array("MODEL_FEAT_MEAN", mean) + c_array("MODEL_FEAT_STD", std) + "\n"
        + c_array("MODEL_W1", w1.T) + c_array("MODEL_B1", b1)
        + c_array("MODEL_W2", w2.T) + c_array("MODEL_B2", b2)
        + c_array("MODEL_WT", wt.T) + c_array("MODEL_BT", bt)
        + c_array("MODEL_WP", wp.T) + c_array("MODEL_BP", bp)
        + "\n#endif /* MODEL_WEIGHTS_H */\n"
    )
    path.write_text(text)


# ------------------------------------------------------------------ main

def main():
    args = parse_args()
    h = args.horizon
    args.out_dir.mkdir(parents=True, exist_ok=True)

    print("Loading data")
    segments = load_segments(args.data, args.include_run)

    all_rise = np.concatenate([s["temp_rise_c"].to_numpy() for s in segments])
    all_pow = np.concatenate([s["power_mw"].to_numpy() for s in segments])
    rise_hi = float(np.percentile(all_rise, 98))
    p_lo, p_hi = float(np.percentile(all_pow, 5)), float(np.percentile(all_pow, 98))
    thr = {
        "caution_rise": args.caution_rise if args.caution_rise is not None else round(0.5 * rise_hi, 1),
        "critical_rise": args.critical_rise if args.critical_rise is not None else round(0.8 * rise_hi, 1),
        "caution_power": args.caution_power if args.caution_power is not None else round(p_lo + 0.55 * (p_hi - p_lo), 1),
        "critical_power": args.critical_power if args.critical_power is not None else round(p_lo + 0.85 * (p_hi - p_lo), 1),
    }
    print(f"Observed: max rise ~{rise_hi:.1f} C, power {p_lo:.0f}-{p_hi:.0f} mW")
    print(f"Thresholds: {thr}")
    if rise_hi < 3.0:
        print("WARNING: temperature rise under 3 C. The temperature classes will be noisy; "
              "consider a box, longer HIGH segments, or lower limits.")

    # Features + labels per segment; first window and last H rows dropped
    tr_X, tr_yt, tr_yp, te = [], [], [], []
    for seg in segments:
        X = compute_features(seg)
        fut_rise, fut_pow = future_targets(seg, h)
        valid = np.arange(FEATURE_WINDOW - 1, len(seg) - h)
        yt = classify(fut_rise[valid], thr["caution_rise"], thr["critical_rise"])
        yp = classify(fut_pow[valid], thr["caution_power"], thr["critical_power"])
        cut = int(len(valid) * 0.75)   # chronological split: last 25 % of each segment is test
        tr_X.append(X[valid[:cut]]); tr_yt.append(yt[:cut]); tr_yp.append(yp[:cut])
        te.append({"X": X[valid[cut:]], "yt": yt[cut:], "yp": yp[cut:],
                   "rise": seg["temp_rise_c"].to_numpy()[valid[cut:]],
                   "t": (seg["timestamp_ms"].to_numpy()[valid[cut:]] - seg["timestamp_ms"].iloc[0]) / 1000.0})
    Xtr = np.concatenate(tr_X); ytr_t = np.concatenate(tr_yt); ytr_p = np.concatenate(tr_yp)
    Xte = np.concatenate([d["X"] for d in te]); yte_t = np.concatenate([d["yt"] for d in te])
    yte_p = np.concatenate([d["yp"] for d in te])
    print(f"Train rows {len(Xtr)}, test rows {len(Xte)}")
    print("Train temp classes:", np.bincount(ytr_t, minlength=3), " power classes:", np.bincount(ytr_p, minlength=3))

    mean = Xtr.mean(axis=0)
    std = Xtr.std(axis=0)
    std[std < 1e-6] = 1.0

    import tensorflow as tf
    # Several seeds; the one with the lowest validation loss is kept. The
    # choice uses only the validation part of the training data, never the test set.
    model, hist, best_val, best_seed = None, None, float("inf"), None
    for sd in range(args.seed, args.seed + args.seeds):
        m = build_model(Xtr.shape[1], sd)
        hst = m.fit((Xtr - mean) / std, [ytr_t, ytr_p],
                    sample_weight=[class_weights(ytr_t), class_weights(ytr_p)],
                    validation_split=0.15, epochs=args.epochs, batch_size=64, verbose=0,
                    callbacks=[tf.keras.callbacks.EarlyStopping(patience=15, restore_best_weights=True)])
        val = min(hst.history["val_loss"])
        print(f"seed {sd}: best validation loss {val:.4f} after {len(hst.history['val_loss'])} epochs")
        if val < best_val:
            model, hist, best_val, best_seed = m, hst, val, sd
    print(f"Keeping seed {best_seed} (lowest validation loss {best_val:.4f})")

    pt, pp = model.predict((Xte - mean) / std, verbose=0)
    ml_t, ml_p = pt.argmax(1), pp.argmax(1)
    rule_t, rule_p = rule_predict(Xte, h, thr["caution_rise"], thr["critical_rise"],
                                  thr["caution_power"], thr["critical_power"])

    res = {
        "temp": {"ml": metrics(yte_t, ml_t), "rule": metrics(yte_t, rule_t)},
        "power": {"ml": metrics(yte_p, ml_p), "rule": metrics(yte_p, rule_p)},
    }
    lead_ml, lead_rule, off = [], [], 0
    for d in te:
        n = len(d["X"])
        lead_ml += lead_times(d["rise"], ml_t[off:off + n], thr["critical_rise"])
        lead_rule += lead_times(d["rise"], rule_t[off:off + n], thr["critical_rise"])
        off += n

    # ---------- export
    export_header(args.header, model, mean, std, h, thr)
    model.save(args.out_dir / "model.keras")
    try:
        conv = tf.lite.TFLiteConverter.from_keras_model(model)
        (args.out_dir / "model.tflite").write_bytes(conv.convert())
    except Exception as exc:  # TFLite is only for the optional X-CUBE-AI route
        print(f"(tflite export skipped: {exc})")
    np.savez(args.out_dir / "test_vectors.npz", X=Xte[:200], ml_t=ml_t[:200], ml_p=ml_p[:200])
    # training curves and metrics, for the notebook / report
    (args.out_dir / "history.json").write_text(json.dumps(
        {k: [float(v) for v in vals] for k, vals in hist.history.items()}))
    (args.out_dir / "metrics.json").write_text(json.dumps(
        {"thresholds": thr, "results": res, "lead_ml": lead_ml, "lead_rule": lead_rule,
         "params": int(model.count_params()), "seed": best_seed, "train_rows": int(len(Xtr)), "test_rows": int(len(Xte))},
        default=float, indent=1))

    # ---------- report
    def row(name, m):
        return (f"| {name} | {m['accuracy']:.3f} | {m['macro_f1']:.3f} | {m['false_alarm_rate']:.3f} | "
                f"{m['critical_recall']:.3f} | {m['critical_missed_as_normal']:.3f} |")

    def lead_str(v):
        return "no critical crossings in test data" if not v else \
            f"mean {np.mean(v):.1f} s, median {np.median(v):.1f} s over {len(v)} crossings"

    n_params = model.count_params()
    report = f"""# Model report

Horizon H = {h} s, window = {FEATURE_WINDOW} s, features = {", ".join(FEATURE_NAMES)}.
Thresholds: caution +{thr['caution_rise']} C / critical +{thr['critical_rise']} C above idle baseline;
power caution {thr['caution_power']} mW / critical {thr['critical_power']} mW.
Train rows {len(Xtr)}, test rows {len(Xte)} (last 25 % of every recording segment).
Model: {n_params} parameters (~{n_params * 4 / 1024:.1f} KB of float weights); seed {best_seed} kept out of {args.seeds} by lowest validation loss.

## Temperature risk (test set)

| Predictor | Accuracy | Macro F1 | False alarm rate | Critical recall | Critical missed as Normal |
|---|---|---|---|---|---|
{row("ML model", res["temp"]["ml"])}
{row("Slope rule", res["temp"]["rule"])}

CRITICAL warning lead time before the limit was actually crossed (ideal is about H):
- ML model: {lead_str(lead_ml)}
- Slope rule: {lead_str(lead_rule)}

## Power risk (test set)

| Predictor | Accuracy | Macro F1 | False alarm rate | Critical recall | Critical missed as Normal |
|---|---|---|---|---|---|
{row("ML model", res["power"]["ml"])}
{row("Slope rule", res["power"]["rule"])}

Confusion matrices (rows = true NORMAL/CAUTION/CRITICAL, columns = predicted):
```
{chr(10).join(f"{k:5s} {p:4s}: {res[k][p]['confusion']}" for k in res for p in res[k])}
```
"""
    (args.out_dir / "report.md").write_text(report)
    print(report)
    print(f"Header written to {args.header}")
    print(f"Report, model and plots in {args.out_dir}")

    # ---------- plot (optional)
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        di = max(range(len(te)), key=lambda k: te[k]["rise"].max())
        d = te[di]
        start = sum(len(s["X"]) for s in te[:di])
        n = len(d["X"])
        fig, ax = plt.subplots(2, 1, figsize=(11, 6), sharex=True)
        ax[0].plot(d["t"], d["rise"], color="black", label="measured rise")
        ax[0].axhline(thr["caution_rise"], color="orange", ls="--", label="caution")
        ax[0].axhline(thr["critical_rise"], color="red", ls="--", label="critical")
        ax[0].set_ylabel("rise above baseline (C)"); ax[0].legend(loc="upper left")
        ax[1].step(d["t"], d["yt"], where="post", color="black", label=f"true class at t+{h}s")
        ax[1].step(d["t"], ml_t[start:start + n] + 0.06, where="post", label="ML")
        ax[1].step(d["t"], rule_t[start:start + n] - 0.06, where="post", label="slope rule")
        ax[1].set_yticks([0, 1, 2]); ax[1].set_yticklabels(CLASS_NAMES)
        ax[1].set_xlabel("time in segment (s)"); ax[1].legend(loc="upper left")
        fig.tight_layout()
        fig.savefig(args.out_dir / "prediction_example.png", dpi=130)
    except Exception as exc:
        print(f"(plot skipped: {exc})")


if __name__ == "__main__":
    main()
