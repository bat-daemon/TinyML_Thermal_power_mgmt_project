# Model comparison

Same data, features, labels and chronological split as train_model.py: fit rows 6870, validation rows 1212 (end of the training part), test rows 2694 (last 25 % of each recording). Horizon 15 s. Limits (from the training part only): caution +5.5 C / critical +8.9 C; power 160.1 / 210.3 mW.
Random forest and MLPs: mean ± standard deviation over 5 seeds (42-46).

**Selection rule (fixed before looking at the test set):** highest mean of temperature and power macro F1 on the validation rows, among models of at most 64 KB. **Selected: MLP 7-16-16-3** (validation score 0.920).

| Model | Parameters | Size (float32) | Val temp F1 | Val power F1 | Test temp accuracy | Test temp macro F1 | Test temp critical recall | Test temp false alarms | Test power accuracy | Test power macro F1 | MCU deployment |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Slope rule (baseline) | 4 | 0.0 KB | 0.912 | 0.770 | 0.924 | 0.895 | 0.975 | 0.006 | 0.877 | 0.759 | a few lines of C |
| Logistic regression | 48 | 0.2 KB | 0.903 | 0.895 | 0.952 | 0.938 | 0.968 | 0.010 | 0.931 | 0.891 | tiny, X-CUBE-AI or hand C |
| Decision tree (depth 8) | 1750 | 6.8 KB | 0.923 | 0.864 | 0.956 | 0.940 | 0.979 | 0.002 | 0.919 | 0.873 | if/else code |
| Random forest (50 trees) | 91580 | 357.7 KB | 0.940 ± 0.001 | 0.907 ± 0.000 | 0.958 ± 0.000 | 0.943 ± 0.001 | 0.979 ± 0.001 | 0.002 ± 0.001 | 0.935 ± 0.000 | 0.898 ± 0.000 | large for an MCU |
| MLP 7-8-3 (1 hidden layer) | 118 | 0.5 KB | 0.927 ± 0.011 | 0.889 ± 0.004 | 0.954 ± 0.004 | 0.938 ± 0.005 | 0.974 ± 0.003 | 0.006 ± 0.003 | 0.923 ± 0.001 | 0.880 ± 0.002 | X-CUBE-AI / plain C |
| MLP 7-16-16-3 **(selected)** | 502 | 2.0 KB | 0.934 ± 0.006 | 0.907 ± 0.001 | 0.957 ± 0.001 | 0.942 ± 0.002 | 0.983 ± 0.001 | 0.003 ± 0.001 | 0.934 ± 0.001 | 0.895 ± 0.002 | X-CUBE-AI / plain C |
