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
    MLP 7-16-16-3        two hidden layers of 16 (the architecture train_model.py exports)

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
    """Same limits, labels and chronological split as train_model.main(). The
    last 15 % of the training part is held out as a validation set: models are
    chosen on validation scores, and the test part is only used to report."""
    thr = tm.thresholds_from_train(segments, h)[0]
    tr, te = [], []
    for seg in segments:
        X = tm.compute_features(seg)
        fut_rise, fut_pow = tm.future_targets(seg, h)
        valid = np.arange(tm.FEATURE_WINDOW - 1, len(seg) - h)
        yt = tm.classify(fut_rise[valid], thr["caution_rise"], thr["critical_rise"])
        yp = tm.classify(fut_pow[valid], thr["caution_power"], thr["critical_power"])
        cut = int(len(valid) * tm.TRAIN_FRACTION)
        tr.append((X[valid[:cut]], yt[:cut], yp[:cut]))
        te.append((X[valid[cut:]], yt[cut:], yp[cut:]))
    cat = lambda parts, k: np.concatenate([p[k] for p in parts])
    return thr, (cat(tr, 0), cat(tr, 1), cat(tr, 2)), (cat(te, 0), cat(te, 1), cat(te, 2))


def tree_params(t):
    # per node: feature index, threshold, two child links, class value -> ~5 numbers
    return int(t.tree_.node_count * 5)


VAL_FRACTION = 0.15   # last 15 % of the training part, same as train_model.py's validation_split
MCU_BUDGET_KB = 64.0  # models larger than this are reported but not eligible for the MCU


def main():
    args = parse_args()
    segments = tm.load_segments(args.data, args.include_run)
    thr, (Xtr, ytr_t, ytr_p), (Xte, yte_t, yte_p) = build_split(segments, args.horizon)
    mean, std = Xtr.mean(axis=0), Xtr.std(axis=0)
    std[std < 1e-6] = 1.0
    Ztr, Zte = (Xtr - mean) / std, (Xte - mean) / std
    # chronological validation set: the end of the training part
    nv = int(len(Ztr) * VAL_FRACTION)
    Zfit, Zva = Ztr[:-nv], Ztr[-nv:]
    yfit_t, yva_t, yfit_p, yva_p = ytr_t[:-nv], ytr_t[-nv:], ytr_p[:-nv], ytr_p[-nv:]
    print(f"Fit rows {len(Zfit)}, validation rows {len(Zva)}, test rows {len(Zte)}, limits {thr}")

    seeds = [args.seed + k for k in range(args.seeds)]
    # each entry: name, params, note, runs; a run = dict(val_t, val_p, test_t, test_p) of metric dicts
    results = []

    def run_of(pred_va_t, pred_va_p, pred_te_t, pred_te_p):
        return {"val_t": tm.metrics(yva_t, pred_va_t), "val_p": tm.metrics(yva_p, pred_va_p),
                "test_t": tm.metrics(yte_t, pred_te_t), "test_p": tm.metrics(yte_p, pred_te_p)}

    rule = lambda X: tm.rule_predict(X, args.horizon, thr["caution_rise"], thr["critical_rise"],
                                     thr["caution_power"], thr["critical_power"])
    (rvt, rvp), (rtt, rtp) = rule(Xtr[-nv:]), rule(Xte)
    results.append(("Slope rule (baseline)", 4, "a few lines of C", [run_of(rvt, rvp, rtt, rtp)]))

    from sklearn.linear_model import LogisticRegression
    from sklearn.tree import DecisionTreeClassifier
    from sklearn.ensemble import RandomForestClassifier

    def fit_pair(make):
        mt = make().fit(Zfit, yfit_t, sample_weight=tm.class_weights(yfit_t))
        mp = make().fit(Zfit, yfit_p, sample_weight=tm.class_weights(yfit_p))
        return mt, mp, run_of(mt.predict(Zva), mp.predict(Zva), mt.predict(Zte), mp.predict(Zte))

    mt, mp, r = fit_pair(lambda: LogisticRegression(max_iter=2000))
    results.append(("Logistic regression", mt.coef_.size + mt.intercept_.size + mp.coef_.size + mp.intercept_.size,
                    "tiny, X-CUBE-AI or hand C", [r]))

    mt, mp, r = fit_pair(lambda: DecisionTreeClassifier(max_depth=8, random_state=args.seed))
    results.append(("Decision tree (depth 8)", tree_params(mt) + tree_params(mp), "if/else code", [r]))

    runs, size = [], 0
    for sd in seeds:
        print(f"Random forest, seed {sd}")
        mt, mp, r = fit_pair(lambda: RandomForestClassifier(n_estimators=50, max_depth=8, random_state=sd))
        runs.append(r)
        size = max(size, sum(tree_params(e) for e in mt.estimators_) + sum(tree_params(e) for e in mp.estimators_))
    results.append(("Random forest (50 trees)", size, "large for an MCU", runs))

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
        m.fit(Zfit, [yfit_t, yfit_p], sample_weight=[tm.class_weights(yfit_t), tm.class_weights(yfit_p)],
              validation_data=(Zva, [yva_t, yva_p]), epochs=args.epochs, batch_size=64, verbose=0,
              callbacks=[tf.keras.callbacks.EarlyStopping(patience=15, restore_best_weights=True)])
        vt, vp = m.predict(Zva, verbose=0)
        pt, pp = m.predict(Zte, verbose=0)
        return m.count_params(), run_of(vt.argmax(1), vp.argmax(1), pt.argmax(1), pp.argmax(1))

    for name, hidden in [("MLP 7-8-3 (1 hidden layer)", [8]), ("MLP 7-16-16-3", [16, 16])]:
        runs = []
        for sd in seeds:
            print(f"{name}, seed {sd}")
            n, r = mlp(hidden, sd)
            runs.append(r)
        results.append((name, n, "X-CUBE-AI / plain C", runs))

    # ---------- selection: on VALIDATION only, among models that fit the MCU budget
    def stat(runs, part, key):
        v = np.array([r[part][key] for r in runs], dtype=float)
        return v.mean(), v.std()

    def score(entry):   # mean of temperature and power macro F1 on validation
        return 0.5 * (stat(entry[3], "val_t", "macro_f1")[0] + stat(entry[3], "val_p", "macro_f1")[0])

    eligible = [e for e in results if e[1] * 4 / 1024 <= MCU_BUDGET_KB and not e[0].startswith("Slope")]
    chosen = max(eligible, key=score)

    def fmt(runs, part, key):
        m, sd = stat(runs, part, key)
        return f"{m:.3f}" if len(runs) == 1 else f"{m:.3f} ± {sd:.3f}"

    lines = [
        "# Model comparison",
        "",
        f"Same data, features, labels and chronological split as train_model.py: fit rows {len(Zfit)}, "
        f"validation rows {len(Zva)} (end of the training part), test rows {len(Zte)} (last 25 % of each recording). "
        f"Horizon {args.horizon} s. Limits (from the training part only): caution +{thr['caution_rise']} C / "
        f"critical +{thr['critical_rise']} C; power {thr['caution_power']} / {thr['critical_power']} mW.",
        f"Random forest and MLPs: mean ± standard deviation over {len(seeds)} seeds ({seeds[0]}-{seeds[-1]}).",
        "",
        f"**Selection rule (fixed before looking at the test set):** highest mean of temperature and power macro F1 "
        f"on the validation rows, among models of at most {MCU_BUDGET_KB:.0f} KB. **Selected: {chosen[0]}** "
        f"(validation score {score(chosen):.3f}).",
        "",
        "| Model | Parameters | Size (float32) | Val temp F1 | Val power F1 | Test temp accuracy | Test temp macro F1 | "
        "Test temp critical recall | Test temp false alarms | Test power accuracy | Test power macro F1 | MCU deployment |",
        "|---|---|---|---|---|---|---|---|---|---|---|---|",
    ]
    csv = ["model,params,runs,val_score,val_temp_f1,val_power_f1,temp_acc,temp_acc_sd,temp_f1,temp_f1_sd,"
           "temp_crit_recall,temp_false_alarm,power_acc,power_f1,power_f1_sd,selected"]
    for e in results:
        name, params, note, runs = e
        lines.append(f"| {name}{' **(selected)**' if e is chosen else ''} | {params} | {params * 4 / 1024:.1f} KB | "
                     f"{fmt(runs, 'val_t', 'macro_f1')} | {fmt(runs, 'val_p', 'macro_f1')} | "
                     f"{fmt(runs, 'test_t', 'accuracy')} | {fmt(runs, 'test_t', 'macro_f1')} | "
                     f"{fmt(runs, 'test_t', 'critical_recall')} | {fmt(runs, 'test_t', 'false_alarm_rate')} | "
                     f"{fmt(runs, 'test_p', 'accuracy')} | {fmt(runs, 'test_p', 'macro_f1')} | {note} |")
        g = lambda part, key, i=0: stat(runs, part, key)[i]
        csv.append(f"{name},{params},{len(runs)},{score(e):.4f},{g('val_t','macro_f1'):.4f},{g('val_p','macro_f1'):.4f},"
                   f"{g('test_t','accuracy'):.4f},{g('test_t','accuracy',1):.4f},{g('test_t','macro_f1'):.4f},"
                   f"{g('test_t','macro_f1',1):.4f},{g('test_t','critical_recall'):.4f},{g('test_t','false_alarm_rate'):.4f},"
                   f"{g('test_p','accuracy'):.4f},{g('test_p','macro_f1'):.4f},{g('test_p','macro_f1',1):.4f},"
                   f"{int(e is chosen)}")
    text = "\n".join(lines) + "\n"
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "comparison.md").write_text(text)
    (args.out_dir / "comparison.csv").write_text("\n".join(csv) + "\n")
    print(text)
    print(f"Written to {args.out_dir / 'comparison.md'}")


if __name__ == "__main__":
    main()
