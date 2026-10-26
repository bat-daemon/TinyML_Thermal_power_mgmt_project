#!/usr/bin/env python3
"""
Model selection: train several candidate models on the same data, features,
labels and train/test split as train_model.py, and compare them on the
held-out test set. Run this before train_model.py to justify which model
goes on the MCU.

Candidates (each predicts temperature risk and power risk, 3 classes):
    slope rule           no training, the baseline
    logistic regression  linear model
    decision tree        depth-limited tree
    random forest        50 trees
    MLP 7-8-3            one hidden layer of 8
    MLP 7-16-16-3        two hidden layers of 16 (the model train_model.py exports)

Models whose training is random (random forest, MLPs) are trained with
several seeds (--seeds, default 5) and reported as mean +- standard
deviation, so a lucky or unlucky single run cannot decide the choice.

Output: <out-dir>/comparison.md and comparison.csv

Example:
    python scripts/compare_models.py --data data/auto1.csv

Install:
    python -m pip install tensorflow scikit-learn pandas numpy
"""

import argparse
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import train_model as tm  # same data pipeline, labels and metrics


def parse_args():
    p = argparse.ArgumentParser(description="Compare candidate models on the same split.")
    p.add_argument("--data", nargs="+", required=True, help="CSV files (globs allowed)")
    p.add_argument("--horizon", type=int, default=15)
    p.add_argument("--include-run", action="store_true")
    p.add_argument("--epochs", type=int, default=120)
    p.add_argument("--seed", type=int, default=42, help="first seed")
    p.add_argument("--seeds", type=int, default=5, help="runs per randomised model (default 5)")
    p.add_argument("--out-dir", type=Path, default=tm.REPO_ROOT / "model_out")
    return p.parse_args()


def build_split(segments, h):
    """Same thresholds, labels and chronological 75/25 split as train_model.main()."""
    all_rise = np.concatenate([s["temp_rise_c"].to_numpy() for s in segments])
    all_pow = np.concatenate([s["power_mw"].to_numpy() for s in segments])
    rise_hi = float(np.percentile(all_rise, 98))
    p_lo, p_hi = float(np.percentile(all_pow, 5)), float(np.percentile(all_pow, 98))
    thr = {
        "caution_rise": round(0.5 * rise_hi, 1),
        "critical_rise": round(0.8 * rise_hi, 1),
        "caution_power": round(p_lo + 0.55 * (p_hi - p_lo), 1),
        "critical_power": round(p_lo + 0.85 * (p_hi - p_lo), 1),
    }
    tr, te = [], []
    for seg in segments:
        X = tm.compute_features(seg)
        fut_rise, fut_pow = tm.future_targets(seg, h)
        valid = np.arange(tm.FEATURE_WINDOW - 1, len(seg) - h)
        yt = tm.classify(fut_rise[valid], thr["caution_rise"], thr["critical_rise"])
        yp = tm.classify(fut_pow[valid], thr["caution_power"], thr["critical_power"])
        cut = int(len(valid) * 0.75)
        tr.append((X[valid[:cut]], yt[:cut], yp[:cut]))
        te.append((X[valid[cut:]], yt[cut:], yp[cut:]))
    cat = lambda parts, k: np.concatenate([p[k] for p in parts])
    return thr, (cat(tr, 0), cat(tr, 1), cat(tr, 2)), (cat(te, 0), cat(te, 1), cat(te, 2))


def tree_params(t):
    # per node: feature index, threshold, two child links, class value -> ~5 numbers
    return int(t.tree_.node_count * 5)


def main():
    args = parse_args()
    segments = tm.load_segments(args.data, args.include_run)
    thr, (Xtr, ytr_t, ytr_p), (Xte, yte_t, yte_p) = build_split(segments, args.horizon)
    mean, std = Xtr.mean(axis=0), Xtr.std(axis=0)
    std[std < 1e-6] = 1.0
    Ztr, Zte = (Xtr - mean) / std, (Xte - mean) / std
    print(f"Train rows {len(Xtr)}, test rows {len(Xte)}, thresholds {thr}")

    seeds = [args.seed + k for k in range(args.seeds)]
    results = []  # (name, params, [temp metrics per run], [power metrics per run], MCU note)

    rt, rp = tm.rule_predict(Xte, args.horizon, thr["caution_rise"], thr["critical_rise"],
                             thr["caution_power"], thr["critical_power"])
    results.append(("Slope rule (baseline)", 4, [tm.metrics(yte_t, rt)], [tm.metrics(yte_p, rp)],
                    "a few lines of C"))

    from sklearn.linear_model import LogisticRegression
    from sklearn.tree import DecisionTreeClassifier
    from sklearn.ensemble import RandomForestClassifier

    def fit_pair(make):
        mt = make().fit(Ztr, ytr_t, sample_weight=tm.class_weights(ytr_t))
        mp = make().fit(Ztr, ytr_p, sample_weight=tm.class_weights(ytr_p))
        return mt, mp, tm.metrics(yte_t, mt.predict(Zte)), tm.metrics(yte_p, mp.predict(Zte))

    mt, mp, a, b = fit_pair(lambda: LogisticRegression(max_iter=2000))
    results.append(("Logistic regression", mt.coef_.size + mt.intercept_.size + mp.coef_.size + mp.intercept_.size,
                    [a], [b], "tiny, X-CUBE-AI or hand C"))

    mt, mp, a, b = fit_pair(lambda: DecisionTreeClassifier(max_depth=8, random_state=args.seed))
    results.append(("Decision tree (depth 8)", tree_params(mt) + tree_params(mp), [a], [b],
                    "if/else code; overfits easily"))

    runs_t, runs_p, size = [], [], 0
    for sd in seeds:
        print(f"Random forest, seed {sd}")
        mt, mp, a, b = fit_pair(lambda: RandomForestClassifier(n_estimators=50, max_depth=8, random_state=sd))
        runs_t.append(a); runs_p.append(b)
        size = sum(tree_params(e) for e in mt.estimators_) + sum(tree_params(e) for e in mp.estimators_)
    results.append(("Random forest (50 trees)", size, runs_t, runs_p, "large for an MCU"))

    import tensorflow as tf

    def mlp(hidden, seed):
        tf.keras.utils.set_random_seed(seed)
        inp = tf.keras.Input(shape=(Xtr.shape[1],))
        x = inp
        for n in hidden:
            x = tf.keras.layers.Dense(n, activation="relu")(x)
        out_t = tf.keras.layers.Dense(3, activation="softmax")(x)
        out_p = tf.keras.layers.Dense(3, activation="softmax")(x)
        m = tf.keras.Model(inp, [out_t, out_p])
        m.compile(optimizer=tf.keras.optimizers.Adam(3e-3),
                  loss=["sparse_categorical_crossentropy", "sparse_categorical_crossentropy"])
        m.fit(Ztr, [ytr_t, ytr_p], sample_weight=[tm.class_weights(ytr_t), tm.class_weights(ytr_p)],
              validation_split=0.15, epochs=args.epochs, batch_size=64, verbose=0,
              callbacks=[tf.keras.callbacks.EarlyStopping(patience=15, restore_best_weights=True)])
        pt, pp = m.predict(Zte, verbose=0)
        return m.count_params(), tm.metrics(yte_t, pt.argmax(1)), tm.metrics(yte_p, pp.argmax(1))

    for name, hidden in [("MLP 7-8-3 (1 hidden layer)", [8]), ("MLP 7-16-16-3 (chosen)", [16, 16])]:
        runs_t, runs_p = [], []
        for sd in seeds:
            print(f"{name}, seed {sd}")
            n, a, b = mlp(hidden, sd)
            runs_t.append(a); runs_p.append(b)
        results.append((name, n, runs_t, runs_p, "X-CUBE-AI / plain C"))

    # ---------- report
    def stat(runs, key):
        v = np.array([r[key] for r in runs], dtype=float)
        return v.mean(), v.std()

    def fmt(runs, key):
        m, sd = stat(runs, key)
        return f"{m:.3f}" if len(runs) == 1 else f"{m:.3f} ± {sd:.3f}"

    lines = [
        "# Model comparison",
        "",
        f"Same data, features, labels and split as train_model.py: train rows {len(Xtr)}, "
        f"test rows {len(Xte)} (last 25 % of each recording). Horizon {args.horizon} s.",
        f"Limits: caution +{thr['caution_rise']} C / critical +{thr['critical_rise']} C; "
        f"power {thr['caution_power']} / {thr['critical_power']} mW.",
        f"Random forest and MLPs: mean ± standard deviation over {len(seeds)} training seeds "
        f"({seeds[0]}-{seeds[-1]}); the other rows are deterministic.",
        "",
        "| Model | Parameters | Size (float32) | Temp accuracy | Temp macro F1 | Temp critical recall | "
        "Temp false alarms | Power accuracy | Power macro F1 | MCU deployment |",
        "|---|---|---|---|---|---|---|---|---|---|",
    ]
    csv = ["model,params,runs,temp_acc,temp_acc_sd,temp_f1,temp_f1_sd,temp_crit_recall,temp_false_alarm,"
           "power_acc,power_f1,power_f1_sd"]
    for name, params, rt_, rp_, note in results:
        lines.append(f"| {name} | {params} | {params * 4 / 1024:.1f} KB | {fmt(rt_, 'accuracy')} | "
                     f"{fmt(rt_, 'macro_f1')} | {fmt(rt_, 'critical_recall')} | {fmt(rt_, 'false_alarm_rate')} | "
                     f"{fmt(rp_, 'accuracy')} | {fmt(rp_, 'macro_f1')} | {note} |")
        ta, tas = stat(rt_, "accuracy"); tf1, tf1s = stat(rt_, "macro_f1")
        pf1, pf1s = stat(rp_, "macro_f1")
        csv.append(f"{name},{params},{len(rt_)},{ta:.4f},{tas:.4f},{tf1:.4f},{tf1s:.4f},"
                   f"{stat(rt_, 'critical_recall')[0]:.4f},{stat(rt_, 'false_alarm_rate')[0]:.4f},"
                   f"{stat(rp_, 'accuracy')[0]:.4f},{pf1:.4f},{pf1s:.4f}")
    text = "\n".join(lines) + "\n"
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "comparison.md").write_text(text)
    (args.out_dir / "comparison.csv").write_text("\n".join(csv) + "\n")
    print(text)
    print(f"Written to {args.out_dir / 'comparison.md'}")


if __name__ == "__main__":
    main()
